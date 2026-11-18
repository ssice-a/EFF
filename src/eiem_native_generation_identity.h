#pragma once
#include "eiem_native_preload_closure.h"
#include "eiem_native_serialized_splice.h"

// Generation identity belongs to native loading inputs, not live Unity objects.
namespace EiemNativeGenerationIdentity {
using namespace EiemNativeInput;
using View=std::shared_ptr<const EiemInputSpanView::View>;
using Names=std::map<std::string,std::string>;
using ScriptPreloads=std::map<std::string,std::vector<NativeReference>>;
inline std::string Lower(std::string value){for(auto& c:value)c=char(std::tolower(static_cast<unsigned char>(c)));return value;}
inline std::string Scoped(uint64_t scope,const std::string& name){
  auto dot=name.find('.');auto base=name.substr(0,dot),suffix=dot==name.npos?std::string{}:name.substr(dot);
  Require(base.size()>=8,"native generation member identity is too short");
  auto key=std::to_string(scope)+"/"+Lower(base);auto hash=Sha(reinterpret_cast<const unsigned char*>(key.data()),key.size());
  auto hex=Hex(hash.data(),hash.size());std::string result="CAB-";
  while(result.size()<base.size())result+=hex.substr(0,std::min(hex.size(),base.size()-result.size()));
  return result+suffix;
}
inline Names Collect(uint64_t scope,const std::vector<View>& inputs) {
  Require(scope!=0,"native generation identity scope absent");
  Names names;
  for(const auto& input:inputs){Require(bool(input),"native generation input absent");
    EiemNativeRandomAccess::Reader reader(input->Size(),[input](size_t at,unsigned char* out,size_t n){
      Require(input->ReadAt(at,out,n)==n,"native generation input truncated");});
    for(const auto& member:reader.directory.members){
      names.emplace(member.name,Scoped(scope,member.name));
    }
  }return names;
}
inline std::string Path(std::string value,const Names& names){
  for(size_t begin=0;begin<value.size();){auto end=value.find_first_of("/\\",begin);if(end==value.npos)end=value.size();
    auto token=value.substr(begin,end-begin);auto found=names.find(token);
    if(found==names.end()){const auto lower=Lower(token);found=std::find_if(names.begin(),names.end(),[&](const auto& row){return Lower(row.first)==lower;});}
    if(found!=names.end()){Require(found->second.size()==token.size(),"native generation path width differs");value.replace(begin,token.size(),found->second);}
    begin=end+1;
  }return value;
}
inline bool StreamSchema(const Type& type,size_t node){
  const auto& current=type.nodes.at(node);
  if(current.type=="StreamingInfo"||current.type=="StreamedResource")return true;
  for(auto child:current.children)if(StreamSchema(type,child))return true;return false;
}
inline bool RemapStream(const Type& type,Value& value,const Names& names,bool stream=false){
  const auto& node=type.nodes.at(value.node);stream|=node.type=="StreamingInfo"||node.type=="StreamedResource";bool changed=false;
  if(stream&&node.type=="string"){auto before=Text(value.raw.data(),value.raw.size());auto after=Path(before,names);
    if(before!=after){SetString(type,value,after);changed=true;}}
  for(auto& child:value.children)changed=RemapStream(type,child,names,stream)||changed;return changed;
}
inline ScriptPreloads CollectScriptPreloads(const std::vector<View>& inputs){
  ScriptPreloads result;
  for(const auto& input:inputs){
    EiemNativeRandomAccess::Reader reader(input->Size(),[input](size_t at,unsigned char* out,size_t n){
      Require(input->ReadAt(at,out,n)==n,"native script preload input truncated");});
    for(const auto& member:reader.directory.members){
      if(!(member.flags&4))continue;
      const auto layout=reader.Layout(member);
      if(!layout.scriptCount)continue;
      const auto size=MultiplySize(layout.scriptCount,12);
      const auto bytes=reader.ReadAt(member.offset+layout.tableEnd+4,size,reader.limits.metadataBytes);
      Reader scripts(bytes,0,false,bytes.size());
      std::vector<NativeReference> references;
      std::set<NativeReference> seen;
      for(size_t i=0;i<layout.scriptCount;++i){
        const auto file=scripts.Get(4);
        const auto id=static_cast<int64_t>(scripts.Get(8));
        if(!id)continue;
        Require(file<=layout.externals.size(),"native script preload external index differs");
        NativeReference reference{file?layout.externals[file-1]:member.name,id};
        if(seen.insert(reference).second)references.push_back(std::move(reference));
      }
      auto entry=result.try_emplace(Lower(member.name),std::move(references));
      Require(entry.second||entry.first->second==references,"native generation script preload table changed");
    }
  }
  return result;
}
inline bool IncludeScriptPreloads(const Type& type,Value& bundle,SerializedLayout& layout,
    const std::string& cab,const ScriptPreloads& scripts){
  const auto& preloads=ConstArray(type,Child(type,bundle,"m_PreloadTable"));
  NativePreloadExpansion expansion;
  for(const auto& pointer:preloads.children){
    auto reference=NullableReference(type,pointer,layout,cab);
    if(!reference.second||reference.first==cab||expansion.count(reference))continue;
    auto provider=scripts.find(Lower(reference.first));
    if(provider!=scripts.end())expansion.emplace(std::move(reference),provider->second);
  }
  if(expansion.empty())return false;
  std::set<NativeReference> referenced;
  return ExpandNativePreloads(type,bundle,layout,cab,expansion,referenced,NativePreloadPlacement::BeforeAnchor);
}
inline View Isolate(const View& input,const Names& names,const std::string& bundleName={},const ScriptPreloads& scriptPreloads={}){
  namespace R=EiemNativeRandomAccess;namespace P=EiemNativeSerializedSplice;namespace S=EiemNativeSpliceView;
  auto read=[input](size_t at,unsigned char* out,size_t n){Require(input->ReadAt(at,out,n)==n,"native generation source truncated");};
  R::Reader reader(input->Size(),read);std::vector<S::Rewrite> rewrites;
  for(size_t mi=0;mi<reader.directory.members.size();++mi){const auto& member=reader.directory.members[mi];if(!(member.flags&4))continue;
    auto layout=reader.Layout(member);std::map<int64_t,P::RangePayload> changed;
    // A dependent AssetBundle can preload components of a versioned Prefab
    // before the Prefab's own asset request. Bring its MonoScripts forward.
    const bool crossBundleScripts=std::any_of(layout.externals.begin(),layout.externals.end(),
      [&](const auto& cab){return scriptPreloads.count(Lower(cab))!=0;});
    for(auto id:layout.order){const auto& object=layout.objects.at(id);const auto& type=layout.types.at(object.type);std::set<std::string> wanted;
      if(type.classId==142&&crossBundleScripts){
        auto bundle=reader.Object(member,layout,id);
        if(IncludeScriptPreloads(type,bundle,layout,member.name,scriptPreloads)){
          if(!bundleName.empty())for(const char* field:{"m_Name","m_AssetBundleName"})
            SetString(type,Child(type,bundle,field),bundleName);
          auto bytes=std::make_shared<const Bytes>(EncodeValue(type,bundle));
          changed[id].pieces.push_back({0,bytes->size(),bytes,{}});
          continue;
        }
      }
      for(auto node:type.nodes.at(0).children)if(StreamSchema(type,node)||(type.classId==142&&!bundleName.empty()&&(type.nodes.at(node).name=="m_Name"||type.nodes.at(node).name=="m_AssetBundleName")))wanted.insert(type.nodes.at(node).name);
      if(wanted.empty())continue;auto fields=reader.Fields(member,layout,id,wanted);std::map<std::string,Value> edits;
      for(auto& field:fields){bool edited=RemapStream(type,field.second,names);
        if(type.classId==142&&(field.first=="m_Name"||field.first=="m_AssetBundleName")){SetString(type,field.second,bundleName);edited=true;}
        if(edited)edits.emplace(field.first,std::move(field.second));}
      if(edits.empty())continue;auto ranges=reader.Ranges(member,layout,id);P::RangePayload payload;size_t output=0;
      for(auto node:type.nodes.at(0).children){auto range=ranges.at(type.nodes.at(node).name);auto edit=edits.find(type.nodes.at(node).name);
        if(edit==edits.end()){Require(((range.offset-member.offset-object.offset)&3)==(output&3),"native generation retained field alignment differs");payload.pieces.push_back({range.offset-member.offset,range.size,{},{}});output+=range.size;}
        else{Bytes bytes(output&3);WriteValue(type,edit->second,bytes);bytes.erase(bytes.begin(),bytes.begin()+(output&3));auto literal=std::make_shared<const Bytes>(std::move(bytes));payload.pieces.push_back({0,literal->size(),literal,{}});output+=literal->size();}}
      if(type.nodes.at(0).align&&(output&3)){auto padding=std::make_shared<const Bytes>(4-(output&3),static_cast<unsigned char>(0));payload.pieces.push_back({0,padding->size(),padding,{}});}
      changed.emplace(id,std::move(payload));
    }
    auto plan=P::BuildRanges(reader,mi,layout,changed);
    // External PPtrs keep their FileID/PathID and original GUID/type records.
    // Only their native archive path changes; opaque payloads are never searched.
    auto prefix=changed.empty()?reader.ReadAt(member.offset,layout.dataOffset,reader.limits.metadataBytes):*plan.rewrite.pieces.at(0).bytes;
    auto current=ParseSerialized(prefix,plan.size);EiemNativeInput::Reader external(prefix,current.tableEnd,false,48+current.metadataSize);
    auto scripts=external.Count();external.Skip(MultiplySize(scripts,12));auto count=external.Count();
    for(size_t i=0;i<count;++i){external.Z();external.Skip(20);auto at=external.p;auto before=external.Z();auto after=Path(before,names);
      Require(before.size()==after.size(),"native generation external path width differs");std::copy(after.begin(),after.end(),prefix.begin()+at);}
    auto literal=std::make_shared<const Bytes>(std::move(prefix));
    if(changed.empty())plan.rewrite.pieces={{0,literal->size(),literal,{}},{layout.dataOffset,member.size-layout.dataOffset,{},{}}};
    else plan.rewrite.pieces.at(0)={0,literal->size(),literal,{}};
    rewrites.push_back(std::move(plan.rewrite));
  }
  Require(!rewrites.empty(),"native generation has no SerializedFile member");
  auto prepared=S::BuildRanges(reader,rewrites);
  for(auto& member:prepared.members){auto found=names.find(member.name);Require(found!=names.end(),"native generation member mapping absent");member.name=found->second;}
  auto prefix=std::make_shared<const Bytes>(EiemNativeViewWriter::Prefix(reader.headerBytes,prepared.blocks,prepared.members));
  prepared.size=prepared.size-prepared.spans.at(0).size+prefix->size();prepared.spans.at(0)={EiemInputSpanView::literalSource,0,prefix->size(),prefix};
  return prepared.Bind(read);
}
} // namespace EiemNativeGenerationIdentity

#pragma once
#include "eiem_demand_author_resources.h"
#include "eiem_native_material_operations.h"
#include "eiem_native_standalone_input.h"

// Build an immutable Material variant from the current game object at the
// moment a matched Mesh consumer is prepared.
namespace EiemDemandMaterialBuild {
using namespace EiemNativeInput;
namespace D=EiemRuntimeAuthorDirectory;
namespace R=EiemNativeRandomAccess;
namespace P=EiemNativeSerializedSplice;
namespace S=EiemNativeStandaloneInput;

inline bool HasReferences(const Type& type,size_t node){
  if(type.nodes.at(node).type.rfind("PPtr<",0)==0)return true;
  for(auto child:type.nodes.at(node).children)if(HasReferences(type,child))return true;return false;
}
inline void ResourceReferences(const Type& type,Value& value,const SerializedLayout& origin,
    SerializedLayout& output,const std::string& cab,std::set<int64_t>& local,std::set<NativeReference>& external,size_t depth=0){
  Require(depth<type.nodes.size(),"native Material resource reference schema cycle");
  if(type.nodes.at(value.node).type.rfind("PPtr<",0)==0){auto file=Integer(Child(type,value,"m_FileID"));auto id=int64_t(Integer(Child(type,value,"m_PathID")));
    Require(file<=origin.externals.size(),"native Material resource external index differs");
    NativeReference reference{file?origin.externals[size_t(file)-1]:cab,id};
    if(id){if(reference.first==cab)local.insert(id);else external.insert(reference);}
    SetReference(type,value,id?reference:NativeReference{cab,0},output,cab);return;
  }
  for(auto& child:value.children)ResourceReferences(type,child,origin,output,cab,local,external,depth+1);
}
inline S::Input Build(R::Reader& reader,size_t memberIndex,const SerializedLayout& layout,
    int64_t sourceId,const Identity& logical,const EiemAuthorMaterialEdits::Author& author,
    const std::map<std::string,NativeReference>& textures,R::ReadRange lease) {
  const auto& member=reader.directory.members.at(memberIndex);
  auto sourceObject=layout.objects.find(sourceId);
  Require(sourceObject!=layout.objects.end()&&layout.types.at(sourceObject->second.type).classId==21,
    "current logical Material is absent from its native Bundle");
  const auto& type=layout.types.at(sourceObject->second.type);
  auto value=reader.Object(member,layout,sourceId);auto references=layout;
  Require(author.source==logical.path,"EFFMAT source identity differs from Material declaration");
  Require(author.textures.size()==textures.size(),"EFFMAT texture binding set differs from resolved resources");
  for(const auto& binding:author.textures)Require(textures.count(binding.first),
    "EFFMAT texture binding has no resolved author Texture: "+binding.first);
  value=EiemNativeMaterialOperations::Apply(type,std::move(value),author.edits,references,member.name,textures);
  // Keep only the source-local resource closure actually used by this variant.
  // A local Texture must not depend on the donor PFB which consumes this new
  // Material. Selected pixel data remains lazy source spans, never literals.
  std::set<NativeReference> dependencies;std::set<int64_t> local;
  auto editedReferences=references;ResourceReferences(type,value,editedReferences,references,member.name,local,dependencies);
  struct Local {std::map<std::string,Field> ranges;std::map<std::string,Value> fields;};
  std::map<int64_t,Local> objects;std::vector<S::StreamRange> streams;size_t streamBytes=0;
  while(true){auto next=std::find_if(local.begin(),local.end(),[&](int64_t id){return id!=sourceId&&!objects.count(id);});if(next==local.end())break;
    auto id=*next;auto object=layout.objects.find(id);Require(object!=layout.objects.end(),"native Material local dependency object absent");
    const auto& nativeType=layout.types.at(object->second.type);Require(nativeType.classId!=142,"native Material references its donor Bundle directory");
    Local current;current.ranges=reader.Ranges(member,layout,id);std::set<std::string> projected;
    for(auto node:nativeType.nodes.at(0).children)if(HasReferences(nativeType,node)||nativeType.nodes.at(node).name=="m_StreamData")projected.insert(nativeType.nodes.at(node).name);
    current.fields=reader.Fields(member,layout,id,projected);
    for(auto& field:current.fields)ResourceReferences(nativeType,field.second,layout,references,member.name,local,dependencies);
    auto stream=current.fields.find("m_StreamData");
    if(stream!=current.fields.end()){auto& descriptor=stream->second;auto size=Integer(Child(nativeType,descriptor,"size"));
      if(size){const auto& path=Child(nativeType,descriptor,"path");auto sourcePath=Text(path.raw.data(),path.raw.size());auto slash=sourcePath.find_last_of("/\\");
        auto name=sourcePath.substr(slash==std::string::npos?0:slash+1);auto backing=std::find_if(reader.directory.members.begin(),reader.directory.members.end(),[&](const auto& candidate){return candidate.name==name;});
        Require(backing!=reader.directory.members.end(),"native Material local resource stream is outside its declared source Bundle");
        auto offset=Integer(Child(nativeType,descriptor,"offset"));Require(offset<=backing->size&&size<=backing->size-offset,"native Material local resource stream range differs");
        streams.push_back({AddSize(backing->offset,size_t(offset)),size_t(size)});
        SetInteger(nativeType,Child(nativeType,descriptor,"offset"),streamBytes);streamBytes=AddSize(streamBytes,size_t(size));
      }
    }
    objects.emplace(id,std::move(current));
  }
  auto payload=std::make_shared<const Bytes>(EncodeValue(type,value));

  D::Writer identity;identity.S("eff-demand-material-variant/2");identity.S(logical.path);identity.S(logical.asset);
  identity.S(member.name);identity.N(uint64_t(sourceId));identity.N(type.nativeRecord.size());
  identity.b.insert(identity.b.end(),type.nativeRecord.begin(),type.nativeRecord.end());
  identity.N(payload->size());identity.b.insert(identity.b.end(),payload->begin(),payload->end());
  for(const auto& dependency:dependencies){identity.S(dependency.first);identity.N(uint64_t(dependency.second));}
  for(const auto& row:objects){const auto& object=layout.objects.at(row.first);const auto& localType=layout.types.at(object.type);
    identity.N(uint64_t(row.first));identity.N(object.offset);identity.N(object.size);identity.N(localType.nativeRecord.size());
    identity.b.insert(identity.b.end(),localType.nativeRecord.begin(),localType.nativeRecord.end());
    for(const auto& field:row.second.fields){identity.S(field.first);auto bytes=EncodeValue(localType,field.second);identity.N(bytes.size());identity.b.insert(identity.b.end(),bytes.begin(),bytes.end());}}
  auto digest=Sha(identity.b.data(),identity.b.size());auto token=Hex(digest.data(),digest.size());
  std::transform(token.begin(),token.end(),token.begin(),[](unsigned char c){return char(std::tolower(c));});

  std::map<int64_t,P::RangePayload> retained;
  for(auto& row:objects){const auto& object=layout.objects.at(row.first);const auto& localType=layout.types.at(object.type);auto stream=row.second.fields.find("m_StreamData");
    if(stream!=row.second.fields.end()&&Integer(Child(localType,stream->second,"size")))SetString(localType,Child(localType,stream->second,"path"),"archive:/CAB-"+token.substr(0,32)+"/CAB-"+token.substr(0,32)+".resS");
    P::RangePayload edit;size_t output=0;for(auto node:localType.nodes.at(0).children){const auto& name=localType.nodes.at(node).name;const auto& range=row.second.ranges.at(name);
      if(EiemNativeObjectGraft::HasAlignment(localType,node))Require(((range.offset-member.offset-object.offset)&3)==(output&3),"native Material local resource field alignment differs");
      auto field=row.second.fields.find(name);if(field==row.second.fields.end()){edit.pieces.push_back({range.offset-member.offset,range.size,{},{}});output=AddSize(output,range.size);}
      else{Bytes bytes(output&3);WriteValue(localType,field->second,bytes);bytes.erase(bytes.begin(),bytes.begin()+(output&3));auto literal=std::make_shared<const Bytes>(std::move(bytes));edit.pieces.push_back({0,literal->size(),literal,{}});output=AddSize(output,literal->size());}}
    if(localType.nodes.at(0).align&&(output&3)){auto pad=std::make_shared<const Bytes>(4-(output&3),0);edit.pieces.push_back({0,pad->size(),pad,{}});}
    retained.emplace(row.first,std::move(edit));
  }
  P::RangePayload edit;edit.pieces.push_back({0,payload->size(),payload,{}});
  return S::Build(reader,memberIndex,layout,sourceId,references,std::move(edit),token,dependencies,std::move(lease),retained,streams);
}
} // namespace EiemDemandMaterialBuild

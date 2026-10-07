#pragma once
#include "eiem_prefab_resource_rules.h"

// Portable, author-compiled recipes. The game reads these records; it never
// runs Mesh/Material/Texture builders or discovers prefab consumers.
namespace EiemCompiledMod {
using namespace EiemOfflinePackage;
constexpr const char* structure="compiled-mod/source-author-spans+object-layouts+type-dictionary+relocations/le64/sha256";
struct StreamSite {int64_t object=0;size_t offset=0,width=0;std::string member;uint64_t value=0;};
struct MemberProof {SerializedLayout before,after;std::vector<StreamSite> streams;std::vector<EiemLoadingGeneration::Patch> pristineNamespace;};
struct Artifact {Manifest manifest;std::map<std::string,MemberProof> members;std::vector<Root> pristine;std::string digest;};

inline Bytes Pack(const Bytes& plain){
 COMPRESSOR_HANDLE h=nullptr;Require(CreateCompressor(COMPRESS_ALGORITHM_XPRESS_HUFF,nullptr,&h)!=0,"compiled Mod compressor unavailable");
 struct Close{COMPRESSOR_HANDLE h;~Close(){CloseCompressor(h);}}close{h};SIZE_T n=0;
 Compress(h,plain.data(),plain.size(),nullptr,0,&n);Require(n&&GetLastError()==ERROR_INSUFFICIENT_BUFFER,"compiled Mod compression extent differs");
 Bytes packed(n);Require(Compress(h,plain.data(),plain.size(),packed.data(),packed.size(),&n)!=0,"compiled Mod compression failed");packed.resize(n);
 Writer w;w.S(structure);w.N(plain.size());w.Blob(packed);auto hash=Sha(w.b.data(),w.b.size());w.Raw(hash.data(),hash.size());return w.b;
}
inline Bytes Unpack(const Bytes& bytes){
 Require(bytes.size()>=32,"compiled Mod truncated");auto hash=Sha(bytes.data(),bytes.size()-32);Require(std::equal(hash.begin(),hash.end(),bytes.end()-32),"compiled Mod digest differs");
 Bytes body(bytes.begin(),bytes.end()-32);EiemOfflinePackage::Reader r{body};Require(r.S()==structure,"compiled Mod field structure unsupported");auto size=r.Size();auto packed=r.Blob();Require(size&&size<=Bytes{}.max_size()&&r.p==body.size(),"compiled Mod envelope extent differs");
 DECOMPRESSOR_HANDLE h=nullptr;Require(CreateDecompressor(COMPRESS_ALGORITHM_XPRESS_HUFF,nullptr,&h)!=0,"compiled Mod decompressor unavailable");struct Close{DECOMPRESSOR_HANDLE h;~Close(){CloseDecompressor(h);}}close{h};SIZE_T n=0;
 Require(!Decompress(h,packed.data(),packed.size(),nullptr,0,&n)&&GetLastError()==ERROR_INSUFFICIENT_BUFFER&&n==size,"compiled Mod declared extent differs");Bytes plain(size);Require(Decompress(h,packed.data(),packed.size(),plain.data(),plain.size(),&n)&&n==size,"compiled Mod decompression failed");return plain;
}
struct SchemaWriter {std::map<Bytes,size_t> ids;};
struct SchemaReader {std::vector<Type> types;};
inline void WriteLayout(Writer& w,const SerializedLayout& l,SchemaWriter& schemas){
 for(auto n:{l.metadataSize,l.dataOffset,l.countOffset,l.tableStart,l.tableEnd})w.N(n);
 w.N(l.types.size());for(const auto& t:l.types){Writer encoded;encoded.N(uint64_t(t.classId));encoded.Raw(t.hash.data(),t.hash.size());encoded.N(t.nodes.size());for(const auto& n:t.nodes){encoded.S(n.type);encoded.S(n.name);encoded.B(n.align);encoded.N(n.children.size());for(auto c:n.children)encoded.N(c);}auto id=schemas.ids.emplace(encoded.b,schemas.ids.size());w.N(id.first->second);if(id.second)w.Raw(encoded.b.data(),encoded.b.size());}
 w.N(l.order.size());for(auto id:l.order){const auto& o=l.objects.at(id);w.N(uint64_t(id));w.N(o.offset);w.N(o.size);w.N(o.type);}w.Strings(l.externals);
}
inline SerializedLayout ReadLayout(EiemOfflinePackage::Reader& r,SchemaReader& schemas){
 SerializedLayout l;l.metadataSize=r.Size();l.dataOffset=r.Size();l.countOffset=r.Size();l.tableStart=r.Size();l.tableEnd=r.Size();
 Require(l.countOffset>=48&&l.tableStart>=l.countOffset+4&&l.tableEnd<=AddSize(48,l.metadataSize)&&l.dataOffset>=AddSize(48,l.metadataSize),"compiled object metadata extent differs");
 auto count=r.Count();for(size_t i=0;i<count;++i){auto id=r.Size();Require(id<=schemas.types.size(),"compiled schema dictionary reference differs");if(id==schemas.types.size()){Type t;t.classId=int(r.N());r.Raw(t.hash.data(),t.hash.size());auto nodes=r.Count();for(size_t j=0;j<nodes;++j){Node n;n.type=r.S();n.name=r.S();n.align=r.B();auto children=r.Count();for(size_t k=0;k<children;++k){auto c=r.Size();Require(c>j&&c<nodes,"compiled schema child extent/cycle differs");n.children.push_back(c);}t.nodes.push_back(std::move(n));}schemas.types.push_back(std::move(t));}l.types.push_back(schemas.types.at(id));}
 count=r.Count();for(size_t i=0;i<count;++i){Object o;o.id=int64_t(r.N());o.offset=r.Size();o.size=r.Size();o.type=r.Size();Require(o.id&&o.type<l.types.size()&&o.offset>=l.dataOffset&&l.objects.emplace(o.id,o).second,"compiled object identity/type differs");l.order.push_back(o.id);}Require(MultiplySize(count,24)==l.tableEnd-l.tableStart,"compiled object table extent differs");l.externals=r.Strings();return l;
}
inline Bytes Encode(const Artifact& a,const std::filesystem::path& game){
 auto portable=a.manifest;for(auto& source:portable.plan.originals)source.physical=source.physical.lexically_relative(game);
 Writer w;w.Blob(EiemOfflinePackage::Encode(portable));w.N(a.manifest.plan.originals.size());
 for(const auto& s:a.manifest.plan.originals){auto path=s.physical.lexically_relative(game);Require(!path.empty()&&!path.is_absolute()&&path.u8string().find("..") ==std::string::npos,"compiled source must be relative to game");w.S(path.u8string());}
 SchemaWriter schemas;w.N(a.members.size());for(const auto& row:a.members){w.S(row.first);WriteLayout(w,row.second.before,schemas);WriteLayout(w,row.second.after,schemas);w.N(row.second.streams.size());for(const auto& site:row.second.streams){w.N(uint64_t(site.object));w.N(site.offset);w.N(site.width);w.S(site.member);w.N(site.value);}}
 for(const auto& row:a.members){w.N(row.second.pristineNamespace.size());for(const auto& patch:row.second.pristineNamespace){w.N(patch.offset);w.S(patch.before);}}
 w.N(a.pristine.size());for(const auto& root:a.pristine){w.S(root.path);w.N(root.input);w.S(root.cab);w.N(uint64_t(root.source));w.B(!root.assembly.sources.empty());if(!root.assembly.sources.empty())WriteAssembly(w,root.assembly);}return Pack(w.b);
}
inline Artifact Decode(const Bytes& bytes,const std::filesystem::path& mod,const std::filesystem::path& game){
 auto plain=Unpack(bytes);EiemOfflinePackage::Reader r{plain};Artifact a;auto manifest=r.Blob();a.manifest=EiemOfflinePackage::Decode(manifest,mod,mod.parent_path(),mod);
 auto& plan=a.manifest.plan;Require(!a.manifest.composed&&plan.rulesOnly&&plan.spanRecipes,"compiled Mod requires independent recipes");
 auto n=r.Count();Require(n==plan.originals.size(),"compiled source range coverage differs");auto base=std::filesystem::weakly_canonical(std::filesystem::absolute(game));
 for(auto& source:plan.originals){auto relative=std::filesystem::u8path(r.S());Require(!relative.empty()&&!relative.is_absolute()&&relative.u8string().find("..") ==std::string::npos,"compiled source path escapes game");source.physical=std::filesystem::weakly_canonical(base/relative);Require(source.physical.lexically_relative(base).u8string().rfind("..",0)!=0,"compiled source link escapes game");}
 SchemaReader schemas;n=r.Count();for(size_t i=0;i<n;++i){auto key=r.S();MemberProof p;p.before=ReadLayout(r,schemas);p.after=ReadLayout(r,schemas);Require(p.before.externals==p.after.externals,"compiled external reference order differs");auto count=r.Count();for(size_t j=0;j<count;++j){StreamSite site;site.object=int64_t(r.N());site.offset=r.Size();site.width=r.Size();site.member=r.S();site.value=r.N();Require(p.after.objects.count(site.object)&&site.width&&site.width<=8&&site.offset<=p.after.objects.at(site.object).size&&site.width<=p.after.objects.at(site.object).size-site.offset,"compiled stream relocation extent differs");p.streams.push_back(std::move(site));}Require(a.members.emplace(key,std::move(p)).second,"compiled duplicate member proof");}
 for(auto& row:a.members){auto count=r.Count();for(size_t i=0;i<count;++i){EiemLoadingGeneration::Patch patch;patch.offset=r.Size();patch.before=r.S();row.second.pristineNamespace.push_back(std::move(patch));}}
 n=r.Count();for(size_t i=0;i<n;++i){Root root;root.path=r.S();root.input=r.Size();root.cab=r.S();root.source=int64_t(r.N());if(r.B())root.assembly=ReadAssembly(r);Require(root.input<plan.inputs.size(),"compiled pristine root input differs");a.pristine.push_back(std::move(root));}
 Require(r.p==plain.size()&&a.pristine.size()==plan.roots.size(),"compiled Mod trailing data/pristine root coverage differs");a.digest=Hash(bytes);return a;
}
inline Artifact Read(const std::filesystem::path& mod,const std::filesystem::path& game){return Decode(EiemNativeInput::Read(mod/"compiled.bin"),mod,game);}

// Ordered sequence edits are based on original slots, so two Mods can expand
// different renderers in one LOD/helper/preload list without losing either.
template<class T,class Equal> struct SequenceResult {std::vector<T> values;std::vector<size_t> offsets;};
template<class T,class Equal> inline SequenceResult<T,Equal> MergeSequence(const std::vector<T>& original,const std::vector<std::vector<T>>& variants,Equal equal){
 struct Change{size_t first=0,last=0;std::vector<T> values;};std::map<std::pair<size_t,size_t>,std::vector<T>> changes;
 for(const auto& variant:variants){size_t p=0,q=0;while(p<original.size()||q<variant.size()){
  if(p<original.size()&&q<variant.size()&&equal(original[p],variant[q])){++p;++q;continue;}
  size_t nextP=original.size(),nextQ=variant.size();bool found=false;
  for(size_t j=q;j<variant.size()&&!found;++j)for(size_t i=p;i<original.size();++i)if(equal(original[i],variant[j])){nextP=i;nextQ=j;found=true;break;}
  std::vector<T> values(variant.begin()+q,variant.begin()+nextQ);auto key=std::make_pair(p,nextP);auto old=changes.find(key);
  if(old==changes.end())changes.emplace(key,std::move(values));
  else if(p==nextP){for(const auto& value:values)if(std::none_of(old->second.begin(),old->second.end(),[&](const auto& x){return equal(x,value);}))old->second.push_back(value);}
  else Require(old->second.size()==values.size()&&std::equal(values.begin(),values.end(),old->second.begin(),equal),"compiled rules conflict on an original list interval");
  p=nextP;q=nextQ;
 }}
 SequenceResult<T,Equal> out;out.offsets.resize(original.size()+1);size_t p=0;
 for(const auto& row:changes){auto first=row.first.first,last=row.first.second;Require(first>=p,"compiled list edits overlap");while(p<first){out.offsets[p]=out.values.size();out.values.push_back(original[p++]);}out.offsets[p]=out.values.size();out.values.insert(out.values.end(),row.second.begin(),row.second.end());while(p<last)out.offsets[++p]=out.values.size();}
 while(p<original.size()){out.offsets[p]=out.values.size();out.values.push_back(original[p++]);}out.offsets[p]=out.values.size();return out;
}
inline Value MergeValue(const Type& t,const Value& base,const std::vector<Value>& edits,bool preload=false){
 auto bytes=EncodeValue(t,base);std::vector<Value> changed;for(const auto& v:edits)if(EncodeValue(t,v)!=bytes)changed.push_back(v);if(changed.empty())return base;
 auto first=EncodeValue(t,changed.front());if(std::all_of(changed.begin(),changed.end(),[&](const auto& v){return EncodeValue(t,v)==first;}))return changed.front();
 const auto& node=t.nodes.at(base.node);if(preload&&(node.name=="preloadIndex"||node.name=="preloadSize"))return base;
 Value out=base;
 if(node.type=="Array"&&!(Primitive(t.nodes.at(node.children.at(1)).type)&&!t.nodes.at(node.children.at(1)).align)){
  const auto& element=t.nodes.at(node.children.at(1));bool pointers=element.type.rfind("PPtr<",0)==0;
  if(!pointers&&std::all_of(changed.begin(),changed.end(),[&](const auto& v){return v.children.size()==base.children.size();})){
   for(size_t i=0;i<out.children.size();++i){std::vector<Value> values;for(const auto& v:changed)values.push_back(v.children[i]);out.children[i]=MergeValue(t,base.children[i],values,preload);}return out;
  }
  std::vector<std::vector<Value>> variants;for(const auto& v:changed)variants.push_back(v.children);auto result=MergeSequence(base.children,variants,[&](const Value& a,const Value& b){return EncodeValue(t,a)==EncodeValue(t,b);});out.children=std::move(result.values);out.count=out.children.size();return out;
 }
 if(!Primitive(node.type)&&node.type!="Array"&&node.type!="string"&&node.type!="TypelessData"){
  for(size_t i=0;i<out.children.size();++i){std::vector<Value> values;for(const auto& v:changed){Require(v.children.size()==out.children.size(),"compiled field structure differs");values.push_back(v.children[i]);}out.children[i]=MergeValue(t,base.children[i],values,preload);}return out;
 }
 throw std::runtime_error("compiled rules conflict on field "+node.name);
}
inline Bytes MergeObject(const Type& t,const Bytes& base,const std::vector<Bytes>& variants){
 if(variants.size()==1)return variants.front();auto before=DecodeValue(t,0,base);std::vector<Value> values;for(const auto& bytes:variants)values.push_back(DecodeValue(t,0,bytes));auto merged=MergeValue(t,before,values,t.classId==142);
 if(t.classId==142){const auto& original=ArrayValue(t,Child(t,before,"m_PreloadTable"));std::vector<std::vector<Value>> lists;for(const auto& v:values)lists.push_back(ArrayValue(t,const_cast<Value&>(Child(t,v,"m_PreloadTable"))).children);
  auto result=MergeSequence(original.children,lists,[&](const Value& a,const Value& b){return EncodeValue(t,a)==EncodeValue(t,b);});auto update=[&](Value& info,const Value& old){auto first=Integer(Child(t,old,"preloadIndex")),count=Integer(Child(t,old,"preloadSize"));Require(first<result.offsets.size()&&count<result.offsets.size()-first,"compiled preload interval differs");SetInteger(t,Child(t,info,"preloadIndex"),result.offsets[size_t(first)]);SetInteger(t,Child(t,info,"preloadSize"),result.offsets[size_t(first+count)]-result.offsets[size_t(first)]);};
  for(const char* name:{"m_Container","m_HashContainer"}){auto& list=ArrayValue(t,Child(t,merged,name));const auto& old=ArrayValue(t,const_cast<Value&>(Child(t,before,name)));Require(list.children.size()==old.children.size(),"compiled container address count differs");for(size_t i=0;i<list.children.size();++i)update(Child(t,list.children[i],"second"),Child(t,old.children[i],"second"));}update(Child(t,merged,"m_MainAsset"),Child(t,before,"m_MainAsset"));auto& list=ArrayValue(t,Child(t,merged,"m_PreloadTable"));list.children=std::move(result.values);list.count=list.children.size();
 }return EncodeValue(t,merged);
}
} // namespace EiemCompiledMod

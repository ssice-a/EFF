#pragma once
#include "eiem_prefab_resource_rules.h"
#include "eiem_compiled_input_view.h"

// Portable, author-compiled recipes. The game reads these records; it never
// runs Mesh/Material/Texture builders or discovers prefab consumers.
namespace EiemCompiledMod {
using namespace EiemOfflinePackage;
constexpr const char* structure=EiemPackageProtocol::compiledRuntimeIndex;
constexpr const char* compressionStructure="packed-xpress-huffman/le64/sha256";
struct StreamSite {int64_t object=0;size_t offset=0,width=0;std::string member;uint64_t value=0;};
struct MemberProof {SerializedLayout before,after;std::vector<StreamSite> streams;std::vector<EiemLoadingGeneration::Patch> pristineNamespace;};
struct Artifact {mutable Manifest manifest;mutable std::map<std::string,MemberProof> members;std::vector<Root> pristine;std::string digest;bool genericResources=false;
 std::vector<EiemCompiledInputView::Recipe> views;std::map<std::string,Bytes> preparedFrames;
 std::vector<std::set<std::string>> inputMembers,addedBundles;
 std::vector<InputFileStamp> verifiedSources;
 std::shared_ptr<const std::vector<Bytes>> proofBlocks;
 mutable std::set<size_t> hydrated;std::shared_ptr<std::mutex> proofMutex=std::make_shared<std::mutex>();
};

inline Bytes Pack(const Bytes& plain){
 COMPRESSOR_HANDLE h=nullptr;Require(CreateCompressor(COMPRESS_ALGORITHM_XPRESS_HUFF,nullptr,&h)!=0,"compiled Mod compressor unavailable");
 struct Close{COMPRESSOR_HANDLE h;~Close(){CloseCompressor(h);}}close{h};SIZE_T n=0;
 Compress(h,plain.data(),plain.size(),nullptr,0,&n);Require(n&&GetLastError()==ERROR_INSUFFICIENT_BUFFER,"compiled Mod compression extent differs");
 Bytes packed(n);Require(Compress(h,plain.data(),plain.size(),packed.data(),packed.size(),&n)!=0,"compiled Mod compression failed");packed.resize(n);
 Writer w;w.S(compressionStructure);w.N(plain.size());w.Blob(packed);auto hash=Sha(w.b.data(),w.b.size());w.Raw(hash.data(),hash.size());return w.b;
}
inline Bytes Unpack(const Bytes& bytes){
 Require(bytes.size()>=32,"compiled Mod truncated");auto hash=Sha(bytes.data(),bytes.size()-32);Require(std::equal(hash.begin(),hash.end(),bytes.end()-32),"compiled Mod digest differs");
 Bytes body(bytes.begin(),bytes.end()-32);EiemOfflinePackage::Reader r{body};Require(r.S()==compressionStructure,"compiled section field structure unsupported");auto size=r.Size();auto packed=r.Blob();Require(size&&size<=Bytes{}.max_size()&&r.p==body.size(),"compiled Mod envelope extent differs");
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
 auto count=r.Count();for(size_t i=0;i<count;++i){auto id=r.Size();Require(id<=schemas.types.size(),"compiled schema dictionary reference differs");if(id==schemas.types.size()){Type t;t.classId=int(r.N());r.Raw(t.hash.data(),t.hash.size());auto nodes=r.Count();for(size_t j=0;j<nodes;++j){Node n;n.type=r.S();n.name=r.S();n.align=r.B();auto children=r.Count();for(size_t k=0;k<children;++k){auto c=r.Size();Require(c>j&&c<nodes,"compiled schema child extent/cycle differs");n.children.push_back(c);}t.nodes.push_back(std::move(n));}PrepareFixedWidths(t);schemas.types.push_back(std::move(t));}l.types.push_back(schemas.types.at(id));}
 count=r.Count();for(size_t i=0;i<count;++i){Object o;o.id=int64_t(r.N());o.offset=r.Size();o.size=r.Size();o.type=r.Size();Require(o.id&&o.type<l.types.size()&&o.offset>=l.dataOffset&&l.objects.emplace(o.id,o).second,"compiled object identity/type differs");l.order.push_back(o.id);}Require(MultiplySize(count,24)==l.tableEnd-l.tableStart,"compiled object table extent differs");l.externals=r.Strings();return l;
}
} // namespace EiemCompiledMod
#include "eiem_compiled_runtime_index.h"
namespace EiemCompiledMod {
inline Artifact Read(const std::filesystem::path& mod,const std::filesystem::path& game,const std::function<void(const char*,double)>& cost={},const Package* verifiedAuthor=nullptr){
 auto start=cost?std::chrono::steady_clock::now():std::chrono::steady_clock::time_point{};auto bytes=EiemNativeInput::Read(mod/"compiled.bin");
 if(cost)cost("artifact-file-read",std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count());return Decode(bytes,mod,game,cost,verifiedAuthor);
}
inline Artifact ReadRuntime(const std::filesystem::path& mod,const std::filesystem::path& game,const std::function<void(const char*,double)>& cost={}){
 auto start=std::chrono::steady_clock::now();auto bytes=EiemNativeInput::Read(mod/"compiled.bin");
 if(cost)cost("artifact-file-read",std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count());return DecodeRuntime(bytes,mod,game,cost);
}

// Ordered sequence edits are based on original slots, so two Mods can expand
// different renderers in one LOD/helper/preload list without losing either.
template<class T,class Equal> struct SequenceResult {std::vector<T> values;std::vector<size_t> offsets;};
template<class T,class Equal> inline SequenceResult<T,Equal> MergeSequence(const std::vector<T>& original,const std::vector<std::vector<T>>& variants,Equal equal){
 struct Change{size_t first=0,last=0;std::vector<T> values;};std::map<std::pair<size_t,size_t>,std::vector<T>> changes;
 for(const auto& variant:variants){std::vector<size_t> anchors;size_t cursor=0;for(const auto& value:original){while(cursor<variant.size()&&!equal(value,variant[cursor]))++cursor;if(cursor==variant.size())break;anchors.push_back(cursor++);}bool appendOnly=anchors.size()==original.size();size_t p=0,q=0;while(p<original.size()||q<variant.size()){
  if(p<original.size()&&q<variant.size()&&equal(original[p],variant[q])){++p;++q;continue;}
  size_t nextP=original.size(),nextQ=variant.size();bool found=false;
  if(appendOnly&&p<original.size()){nextP=p;nextQ=anchors[p];}
  else for(size_t j=q;j<variant.size()&&!found;++j)for(size_t i=p;i<original.size();++i)if(equal(original[i],variant[j])){nextP=i;nextQ=j;found=true;break;}
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
inline bool ValueContentEqual(const Value& a,const Value& b,bool& paddingDiffers){
 if(a.node!=b.node||a.count!=b.count||a.raw!=b.raw||a.children.size()!=b.children.size())return false;
 paddingDiffers=paddingDiffers||a.padding!=b.padding;
 for(size_t i=0;i<a.children.size();++i)if(!ValueContentEqual(a.children[i],b.children[i],paddingDiffers))return false;return true;
}
inline bool EqualCompiledValue(const Type& type,const Value& a,const Value& b){
 if(a.node!=b.node)return EncodeValue(type,a)==EncodeValue(type,b);
 bool paddingDiffers=false;if(!ValueContentEqual(a,b,paddingDiffers))return false;
 // Identical typed payloads cannot differ on serialization. Padding whose
 // emission depends on the enclosing offset retains the exact codec check.
 return !paddingDiffers||EncodeValue(type,a)==EncodeValue(type,b);
}
inline Value MergeValue(const Type& t,const Value& base,const std::vector<Value>& edits,bool preload=false){
 std::vector<Value> changed;for(const auto& v:edits)if(!EqualCompiledValue(t,v,base))changed.push_back(v);if(changed.empty())return base;
 if(std::all_of(changed.begin(),changed.end(),[&](const auto& v){return EqualCompiledValue(t,v,changed.front());}))return changed.front();
 const auto& node=t.nodes.at(base.node);if(preload&&(node.name=="preloadIndex"||node.name=="preloadSize"||node.name=="m_PreloadTable"))return base;
 Value out=base;
 if(node.type=="Array"&&!(Primitive(t.nodes.at(node.children.at(1)).type)&&!t.nodes.at(node.children.at(1)).align)){
  const auto& element=t.nodes.at(node.children.at(1));bool pointers=element.type.rfind("PPtr<",0)==0;
  if(!pointers&&std::all_of(changed.begin(),changed.end(),[&](const auto& v){return v.children.size()==base.children.size();})){
   for(size_t i=0;i<out.children.size();++i){std::vector<Value> values;for(const auto& v:changed)values.push_back(v.children[i]);out.children[i]=MergeValue(t,base.children[i],values,preload);}return out;
  }
  std::vector<std::vector<Value>> variants;for(const auto& v:changed)variants.push_back(v.children);auto result=MergeSequence(base.children,variants,[&](const Value& a,const Value& b){return EqualCompiledValue(t,a,b);});out.children=std::move(result.values);out.count=out.children.size();return out;
 }
 if(!Primitive(node.type)&&node.type!="Array"&&node.type!="string"&&node.type!="TypelessData"){
  for(size_t i=0;i<out.children.size();++i){std::vector<Value> values;for(const auto& v:changed){Require(v.children.size()==out.children.size(),"compiled field structure differs");values.push_back(v.children[i]);}try{out.children[i]=MergeValue(t,base.children[i],values,preload);}catch(const std::exception& error){throw std::runtime_error(t.nodes.at(base.children[i].node).name+": "+error.what());}}return out;
 }
 throw std::runtime_error("compiled rules conflict on field "+node.name);
}
struct PreloadResult {std::vector<Value> values;std::vector<size_t> offsets;};
inline PreloadResult MergePreloads(const Type& t,const Value& before,const std::vector<Value>& variants){
 const auto& original=ArrayValue(t,Child(t,before,"m_PreloadTable"));std::vector<std::map<size_t,size_t>> boundaries;std::vector<const Value*> lists;
 auto equal=[&](const Value& a,const Value& b){return EqualCompiledValue(t,a,b);};
 for(const auto& variant:variants){const auto& list=ArrayValue(t,Child(t,variant,"m_PreloadTable"));lists.push_back(&list);std::map<size_t,size_t> mapping{{0,0},{original.children.size(),list.children.size()}};
  auto anchor=[&](const Value& old,const Value& next){auto first=Integer(Child(t,old,"preloadIndex")),count=Integer(Child(t,old,"preloadSize")),start=Integer(Child(t,next,"preloadIndex")),size=Integer(Child(t,next,"preloadSize"));Require(first<=original.count&&count<=original.count-first&&start<=list.count&&size<=list.count-start,"compiled preload interval differs");
   for(auto row:{std::make_pair(size_t(first),size_t(start)),std::make_pair(size_t(first+count),size_t(start+size))}){auto inserted=mapping.emplace(row);Require(inserted.second||inserted.first->second==row.second,"compiled preload boundaries disagree");}};
  for(const char* name:{"m_Container","m_HashContainer"}){const auto& old=ArrayValue(t,Child(t,before,name));const auto& next=ArrayValue(t,Child(t,variant,name));Require(old.children.size()==next.children.size(),"compiled container address count differs");for(size_t i=0;i<old.children.size();++i)anchor(Child(t,old.children[i],"second"),Child(t,next.children[i],"second"));}anchor(Child(t,before,"m_MainAsset"),Child(t,variant,"m_MainAsset"));boundaries.push_back(std::move(mapping));
 }
 // Native interval boundaries disambiguate repeated dependency pointers. An
 // inserted reference may equal a later original entry in another asset.
 PreloadResult out;out.offsets.resize(original.children.size()+1);const auto& cuts=boundaries.front();
 for(auto first=cuts.begin(),last=std::next(first);last!=cuts.end();++first,++last){std::vector<Value> base(original.children.begin()+first->first,original.children.begin()+last->first);std::vector<std::vector<Value>> edits;
  for(size_t i=0;i<lists.size();++i){auto start=boundaries[i].at(first->first),end=boundaries[i].at(last->first);Require(start<=end,"compiled preload boundaries reordered");const auto& values=lists[i]->children;std::vector<Value> edit(values.begin()+start,values.begin()+end);size_t p=0;for(const auto& value:edit)if(p<base.size()&&equal(base[p],value))++p;Require(p==base.size(),"compiled preload removed an original dependency");edits.push_back(std::move(edit));}
  auto result=MergeSequence(base,edits,equal);for(size_t i=0;i<result.offsets.size();++i)out.offsets[first->first+i]=out.values.size()+result.offsets[i];out.values.insert(out.values.end(),result.values.begin(),result.values.end());
 }return out;
}
inline Bytes MergeObject(const Type& t,const Bytes& base,const std::vector<Bytes>& variants){
 if(variants.size()==1)return variants.front();auto before=DecodeValue(t,0,base);std::vector<Value> values;for(const auto& bytes:variants)values.push_back(DecodeValue(t,0,bytes));auto merged=MergeValue(t,before,values,t.classId==142);
 if(t.classId==142){auto result=MergePreloads(t,before,values);auto update=[&](Value& info,const Value& old){auto first=Integer(Child(t,old,"preloadIndex")),count=Integer(Child(t,old,"preloadSize"));Require(first<result.offsets.size()&&count<result.offsets.size()-first,"compiled preload interval differs");SetInteger(t,Child(t,info,"preloadIndex"),result.offsets[size_t(first)]);SetInteger(t,Child(t,info,"preloadSize"),result.offsets[size_t(first+count)]-result.offsets[size_t(first)]);};
  for(const char* name:{"m_Container","m_HashContainer"}){auto& list=ArrayValue(t,Child(t,merged,name));const auto& old=ArrayValue(t,const_cast<Value&>(Child(t,before,name)));Require(list.children.size()==old.children.size(),"compiled container address count differs");for(size_t i=0;i<list.children.size();++i)update(Child(t,list.children[i],"second"),Child(t,old.children[i],"second"));}update(Child(t,merged,"m_MainAsset"),Child(t,before,"m_MainAsset"));auto& list=ArrayValue(t,Child(t,merged,"m_PreloadTable"));list.children=std::move(result.values);list.count=list.children.size();
 }return EncodeValue(t,merged);
}
} // namespace EiemCompiledMod

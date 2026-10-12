#pragma once
#include "../src/eiem_static_resource_sources.h"
#include <string_view>
#include <unordered_map>

// Author-side only: index discovery and native compilation share one process.
// No AnimeStudio runtime, child process, or new whole-game index is required.
namespace EiemAuthorSources {
using namespace EiemNativeInput;
using EiemStaticSources::Logical;
using EiemStaticSources::Selection;
using Requests = std::map<Identity,std::string>;
inline Requests Collect(const Package& p){
 Requests sources;for(const auto& t:p.targetIndex)sources.emplace(t.first,"Mesh");
 for(const auto& m:p.materials)if(!m.second.source.path.empty())sources.emplace(m.second.source,"Material");
 for(const auto& t:p.textures)if(!t.second.source.path.empty())sources.emplace(t.second.source,"Texture2D");
 for(const auto& r:p.resources)if(r.second->blocks.count("@source-prefabs")){auto rows=Strings(*r.second,"@source-prefabs");Require(rows.size()%2==0,"Physics source Prefab identity truncated");for(size_t i=0;i<rows.size();i+=2)sources.emplace(Identity{rows[i],rows[i+1]},"Prefab");}return sources;
}
inline std::string Json(const std::string& value){std::string s="\"";for(unsigned char c:value){if(c=='\"'||c=='\\'){s+='\\';s+=char(c);}else if(c<32){static const char* hex="0123456789ABCDEF";s+="\\u00";s+=hex[c>>4];s+=hex[c&15];}else s+=char(c);}return s+'\"';}
inline std::string RequestJson(const Requests& sources){std::string s="{\"sources\":[";bool first=true;for(const auto& row:sources){if(!first)s+=',';first=false;s+="{\"kind\":"+Json(row.second)+",\"path\":"+Json(row.first.path)+",\"asset\":"+Json(row.first.asset)+'}';}return s+"]}";}
struct Reader {
 const Bytes& b;size_t p=0,limit;
 explicit Reader(const Bytes& data,size_t end=SIZE_MAX):b(data),limit(std::min(end,data.size())){}
 uint64_t N(size_t n){Require(n<=8&&p<=limit&&n<=limit-p,"source index/VFS truncated");auto v=Le(b,p,n);p+=n;return v;}
 std::string_view View(size_t n){Require(n<=limit-p,"source string exceeds encoded extent");auto s=std::string_view((const char*)b.data()+p,n);p+=n;return s;}
 std::string_view S(){uint32_t n=0;for(unsigned i=0;i<5;++i){auto v=unsigned(N(1));Require(i!=4||v<=7,"source index string length invalid");n|=(v&127)<<(i*7);if(!(v&128))return View(n);}throw std::runtime_error("source index string length invalid");}
 size_t Count(size_t minimum=1){auto n=N(4);Require(n<=INT32_MAX&&n<=(limit-p)/minimum,"source count exceeds encoded extent");return size_t(n);}
};
inline char Fold(unsigned char c){return c=='\\'?'/':(c>='A'&&c<='Z'?char(c+32):char(c));}
inline uint64_t PathHash(std::string_view path){uint64_t h=1469598103934665603ull;for(unsigned char c:path)h=(h^static_cast<unsigned char>(Fold(c)))*1099511628211ull;return h;}
struct PathEqual {bool operator()(std::string_view a,std::string_view b)const{if(a.size()!=b.size())return false;for(size_t i=0;i<a.size();++i)if(Fold(a[i])!=Fold(b[i]))return false;return true;}};
struct ViewHash {size_t operator()(std::string_view value)const{return size_t(PathHash(value));}};
inline std::string_view CabView(std::string_view value){auto slash=value.find_last_of("/\\");return slash==value.npos?value:value.substr(slash+1);}
inline std::string_view SourceView(std::string_view value){for(size_t i=0;i+8<=value.size();++i)if(PathEqual{}(value.substr(i,8),"bundles/"))return value.substr(i);return value;}
inline std::string Source(std::string value){value=Logical(std::move(value));auto marker=value.find("bundles/");return marker==value.npos?value:value.substr(marker);}
inline std::string Cab(std::string value){value=Logical(std::move(value));auto slash=value.find_last_of('/');return slash==value.npos?value:value.substr(slash+1);}
struct AssetRow {std::string name,path,kind,source;int64_t id=0;};
struct CabRow {size_t source=0,start=0,count=0;};
struct Index {std::string fingerprint;std::vector<AssetRow> assets;std::vector<std::string> sources;std::vector<std::vector<size_t>> graph;};
inline Index ReadIndex(const std::filesystem::path& path,const Requests& requests){
 auto data=Read(path);Reader r(data);auto magic=r.S();Require(magic=="EFF_END_FIELD_INDEX"||magic=="EIEM_END_FIELD_INDEX","not an EFF source index");r.N(4);Index result;result.fingerprint=std::string(r.S());
 std::unordered_map<uint64_t,std::set<std::string>> wanted;for(const auto& request:requests){auto logical=Logical(request.first.path);wanted[PathHash(logical)].insert(std::move(logical));}
 // Views point into this invocation's index bytes. Intern each bundle once;
 // retain flat CAB references rather than one heap string/set node per edge.
 std::unordered_map<std::string_view,CabRow,ViewHash,PathEqual> cabs;
 std::unordered_map<std::string_view,size_t,ViewHash,PathEqual> sourceIds;
 std::vector<std::string_view> sourceNames,dependencies;uint64_t assets=0;bool ended=false;
 while(r.p<r.limit){auto tag=r.N(1);if(tag==0){Require(r.N(8)==assets&&r.N(8)==cabs.size()&&r.p==r.limit,"source index counts/trailing data differ");ended=true;break;}
  if(tag==1){++assets;auto name=r.S(),container=r.S(),kind=r.S();auto id=int64_t(r.N(8));auto source=r.S();auto found=wanted.find(PathHash(container));
   if(found!=wanted.end()){auto logical=Logical(std::string(container));if(found->second.count(logical))result.assets.push_back({std::string(name),logical,std::string(kind),Source(std::string(source)),id});}continue;
  }
  Require(tag==2,"unknown source index record");auto name=CabView(r.S()),source=SourceView(r.S());auto n=r.Count();auto start=dependencies.size();for(size_t i=0;i<n;++i){auto dep=CabView(r.S());if(!dep.empty())dependencies.push_back(dep);}
  if(name.empty()||source.empty())continue;auto interned=sourceIds.emplace(source,sourceNames.size());if(interned.second)sourceNames.push_back(source);CabRow row{interned.first->second,start,dependencies.size()-start};auto inserted=cabs.emplace(name,row);
  if(!inserted.second){const auto& old=inserted.first->second;Require(old.source==row.source,"conflicting source CAB owner");auto values=[&](const CabRow& item){std::set<std::string> values;for(size_t i=0;i<item.count;++i)values.insert(Logical(std::string(dependencies[item.start+i])));return values;};Require(values(old)==values(row),"conflicting source CAB dependency record");}
 }
 Require(ended,"source index end record absent");result.graph.resize(sourceNames.size());for(auto name:sourceNames)result.sources.push_back(Logical(std::string(name)));
 for(const auto& cab:cabs){auto& graph=result.graph[cab.second.source];for(size_t i=0;i<cab.second.count;++i){auto dep=dependencies[cab.second.start+i];if(PathEqual{}(dep,"unity default resources"))continue;auto owner=cabs.find(dep);if(owner!=cabs.end()&&owner->second.source!=cab.second.source)graph.push_back(owner->second.source);}}
 for(auto& graph:result.graph){std::sort(graph.begin(),graph.end());graph.erase(std::unique(graph.begin(),graph.end()),graph.end());}return result;
}
inline uint32_t Rotate(uint32_t v,unsigned n){return (v<<n)|(v>>(32-n));}
inline void ChaCha(Bytes& data,const unsigned char* nonce){
 static const unsigned char key[]={0xE9,0x5B,0x31,0x7A,0xC4,0xF8,0x28,0x56,0x9D,0x23,0xA8,0x6B,0xF2,0x71,0xDC,0xB5,0x3E,0x84,0x6F,0xA7,0x5C,0x92,0x4D,0x67,0x1D,0xBA,0x8E,0x38,0xF4,0xCA,0x52,0xE1};
 auto word=[](const unsigned char* p){uint32_t v;std::memcpy(&v,p,4);return v;};uint32_t state[16]={0x61707865,0x3320646e,0x79622d32,0x6b206574};for(size_t i=0;i<8;++i)state[i+4]=word(key+i*4);state[12]=1;for(size_t i=0;i<3;++i)state[i+13]=word(nonce+i*4);
 for(size_t offset=0;offset<data.size();offset+=64){uint32_t w[16];std::copy_n(state,16,w);auto q=[&](int a,int b,int c,int d){w[a]+=w[b];w[d]=Rotate(w[d]^w[a],16);w[c]+=w[d];w[b]=Rotate(w[b]^w[c],12);w[a]+=w[b];w[d]=Rotate(w[d]^w[a],8);w[c]+=w[d];w[b]=Rotate(w[b]^w[c],7);};
  for(int i=0;i<10;++i){q(0,4,8,12);q(1,5,9,13);q(2,6,10,14);q(3,7,11,15);q(0,5,10,15);q(1,6,11,12);q(2,7,8,13);q(3,4,9,14);}unsigned char block[64];for(size_t i=0;i<16;++i){auto v=w[i]+state[i];std::memcpy(block+i*4,&v,4);}for(size_t i=0;i<std::min<size_t>(64,data.size()-offset);++i)data[offset+i]^=block[i];Require(++state[12]!=0||offset+64>=data.size(),"VFS cipher counter overflow");
 }
}
inline std::string VfsLogical(std::string_view raw){
 std::string path;path.reserve(raw.size());for(unsigned char c:raw)if(c>=0x20&&c<=0x7e)path+=Fold(c);
 auto allowed=[](unsigned char c){return (c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='_'||c=='.'||c=='/'||c=='-';};
 size_t start=std::string::npos;
 for(size_t i=0;i<path.size();++i){auto prefix=path.compare(i,5,"data/")==0?5:(path.compare(i,7,"assets/")==0?7:0);if(prefix&&i+prefix<path.size()&&allowed(path[i+prefix])){start=i;break;}}
 if(start!=std::string::npos){size_t end=start;while(end<path.size()&&allowed(path[end]))++end;path.resize(end);path.erase(0,start);}
 for(const char* prefix:{"assets/streamingassets/","assets/","data/"})if(path.rfind(prefix,0)==0){path.erase(0,strlen(prefix));break;}
 auto first=path.find_first_not_of('/');if(first==path.npos)return {};path.resize(path.find_last_not_of('/')+1);path.erase(0,first);return path;
}
struct VfsEntry {std::filesystem::path physical;uint64_t offset=0,size=0;std::string md5;bool encrypted=false;};
inline std::vector<std::filesystem::path> Files(const std::filesystem::path& root,const wchar_t* extension){std::vector<std::filesystem::path> result;if(!std::filesystem::is_directory(root))return result;for(const auto& row:std::filesystem::recursive_directory_iterator(root))if(row.is_regular_file()&&_wcsicmp(row.path().extension().c_str(),extension)==0)result.push_back(row.path());std::sort(result.begin(),result.end(),[](const auto& a,const auto& b){return _wcsicmp(a.c_str(),b.c_str())<0;});return result;}
inline std::string Fingerprint(const std::filesystem::path& root,const std::vector<std::filesystem::path>& blcs){
 std::string text;auto paths=blcs;auto chunks=Files(root,L".chk");paths.insert(paths.end(),chunks.begin(),chunks.end());for(const auto& path:paths){WIN32_FILE_ATTRIBUTE_DATA info{};Require(GetFileAttributesExW(path.c_str(),GetFileExInfoStandard,&info)!=0,"VFS fingerprint file unavailable");auto size=(uint64_t(info.nFileSizeHigh)<<32)|info.nFileSizeLow;auto stamp=(uint64_t(info.ftLastWriteTime.dwHighDateTime)<<32)|info.ftLastWriteTime.dwLowDateTime;
  text+=path.lexically_relative(root).generic_u8string()+'|'+std::to_string(size)+'|'+std::to_string(stamp+504911232000000000ull)+'\n';}return EiemResourceReloadPlan::Hash(Bytes(text.begin(),text.end()));
}
inline std::map<std::string,VfsEntry> ReadVfs(const std::filesystem::path& root,const std::set<std::string>& selected,const std::string& fingerprint={}){
 Require(std::filesystem::is_directory(root),"game VFS directory absent");auto files=Files(root,L".blc");if(!fingerprint.empty())Require(Fingerprint(root,files)==fingerprint,"source index no longer describes installed VFS; regenerate it with AnimeStudio");std::map<std::string,VfsEntry> entries;
 struct Parsed {std::map<std::string,VfsEntry> entries;bool any=false;};std::vector<Parsed> parsed(files.size());
 EiemOfflineCompile::ParallelInputs(files.size(),[&](size_t index){const auto& file=files[index];auto& output=parsed[index];auto raw=Read(file);Require(raw.size()>=16,"VFS BLC truncated");Bytes data(raw.begin()+12,raw.end());ChaCha(data,raw.data());Reader r(data,data.size()-4);auto version=int32_t(r.N(4));auto code=version<11?version:3;if(version<11)r.N(4);r.View(size_t(r.N(2)));r.N(8);r.N(4);r.N(8);r.N(1);auto chunks=r.Count(45);
  for(size_t i=0;i<chunks;++i){auto chunk=r.View(16);auto physical=file.parent_path()/std::filesystem::u8path(Hex((const unsigned char*)chunk.data(),chunk.size())+".chk");r.View(16);r.N(8);r.N(1);if(code>3)r.N(4);auto count=r.Count(60);
   for(size_t j=0;j<count;++j){auto logical=VfsLogical(r.View(size_t(r.N(2))));r.N(8);r.View(16);auto digest=r.View(16);auto offset=r.N(8),size=r.N(8);r.N(1);bool encrypted=r.N(1)!=0;if(encrypted)r.N(8);if(code>3)r.N(4);if(logical.empty())continue;output.any=true;
    if(selected.count(logical)){Require(offset<=INT64_MAX&&size&&size<=INT64_MAX,"VFS source range invalid");output.entries[logical]={physical,offset,size,Hex((const unsigned char*)digest.data(),digest.size()),encrypted};}
   }
  }
 });bool any=false;for(auto& output:parsed){any=any||output.any;for(auto& row:output.entries)entries[row.first]=std::move(row.second);}Require(any,"game VFS has no logical files");return entries;
}
inline std::string Md5(const Bytes& bytes){BCRYPT_ALG_HANDLE alg=nullptr;BCRYPT_HASH_HANDLE handle=nullptr;unsigned char digest[16];Require(BCryptOpenAlgorithmProvider(&alg,BCRYPT_MD5_ALGORITHM,nullptr,0)>=0,"MD5 provider failed");auto status=BCryptCreateHash(alg,&handle,nullptr,0,nullptr,0,0);for(size_t p=0;p<bytes.size()&&status>=0;){auto n=ULONG(std::min<size_t>(bytes.size()-p,0x10000000));status=BCryptHashData(handle,(PUCHAR)bytes.data()+p,n,0);p+=n;}if(status>=0)status=BCryptFinishHash(handle,digest,16,0);if(handle)BCryptDestroyHash(handle);BCryptCloseAlgorithmProvider(alg,0);Require(status>=0,"MD5 source validation failed");return Hex(digest,16);}
inline Selection Prepare(const Package& package,const std::filesystem::path& game,const std::filesystem::path& index,std::function<void(const char*,double)> cost={},bool refresh=false){
 using EiemResourceReloadPlan::Measure;auto requests=Collect(package);auto request=RequestJson(requests)+"\nselection=reverse-mesh-consumers+declared-assets+material-dependency-closure";auto identity=EiemResourceReloadPlan::Hash(Bytes(request.begin(),request.end()));auto destination=package.root/"source-inputs.bin";
 if(!refresh&&std::filesystem::is_regular_file(destination)){try{auto previous=EiemStaticSources::LoadMod(package.root,game);if(previous.manifest==identity&&EiemStaticSources::Covers(previous,package)){if(cost)cost("source-index-reused",0);return previous;}}catch(const std::exception&){} }
 auto catalog=Measure(cost,"source-index",[&]{return ReadIndex(index,requests);});Selection selection;selection.manifest=identity;std::set<size_t> meshes,materials,declared;
 std::vector<size_t> order(catalog.sources.size());for(size_t i=0;i<order.size();++i)order[i]=i;std::sort(order.begin(),order.end(),[&](size_t a,size_t b){return catalog.sources[a]<catalog.sources[b];});
 auto sourceId=[&](const std::string& source){auto found=std::lower_bound(order.begin(),order.end(),source,[&](size_t id,const std::string& name){return catalog.sources[id]<name;});Require(found!=order.end()&&catalog.sources[*found]==source,"declared source has no CAB owner: "+source);return *found;};
 for(const auto& item:requests){std::set<std::pair<std::string,int64_t>> matches;for(const auto& row:catalog.assets)if(row.path==Logical(item.first.path)&&(item.second=="Prefab"||(row.name==item.first.asset&&row.kind==item.second)))matches.emplace(row.source,item.second=="Prefab"?0:row.id);
  Require(matches.size()==1,"expected one exact "+item.second+" source: "+item.first.path+"/"+item.first.asset+" found "+std::to_string(matches.size()));auto source=matches.begin()->first;auto inserted=selection.assets.emplace(Logical(item.first.path),source);Require(inserted.second||inserted.first->second==source,"conflicting author asset sources");auto id=sourceId(source);declared.insert(id);if(item.second=="Mesh")meshes.insert(id);if(item.second=="Material")materials.insert(id);
 }
 std::vector<size_t> offsets(catalog.graph.size()+1);for(const auto& graph:catalog.graph)for(auto dep:graph)++offsets[dep+1];for(size_t i=1;i<offsets.size();++i)offsets[i]+=offsets[i-1];auto cursor=offsets;std::vector<size_t> reverse(offsets.back());for(size_t owner=0;owner<catalog.graph.size();++owner)for(auto dep:catalog.graph[owner])reverse[cursor[dep]++]=owner;
 auto candidates=meshes;std::vector<size_t> queue(meshes.begin(),meshes.end());while(!queue.empty()){auto value=queue.back();queue.pop_back();for(size_t i=offsets[value];i<offsets[value+1];++i)if(candidates.insert(reverse[i]).second)queue.push_back(reverse[i]);}
 auto selectedIds=candidates;for(auto id:declared){selectedIds.insert(id);selectedIds.insert(catalog.graph[id].begin(),catalog.graph[id].end());}queue.assign(materials.begin(),materials.end());auto closure=materials;while(!queue.empty()){auto value=queue.back();queue.pop_back();for(auto dep:catalog.graph[value])if(closure.insert(dep).second)queue.push_back(dep);}selectedIds.insert(closure.begin(),closure.end());
 std::set<std::string> selected;for(auto id:selectedIds)selected.insert(catalog.sources[id]);for(auto id:candidates)selection.candidates.insert(catalog.sources[id]);
 for(const auto& row:catalog.assets)if(row.kind=="GameObject"&&EiemStaticSources::Ends(row.path,".prefab")&&selection.candidates.count(row.source)){auto added=selection.assets.emplace(row.path,row.source);Require(added.second||added.first->second==row.source,"ambiguous source Prefab");}
 auto shipped=Measure(cost,"source-vfs",[&]{return ReadVfs(game/L"Endfield_Data/StreamingAssets/VFS",selected,catalog.fingerprint);});auto persistent=game/L"Endfield_Data/Persistent/VFS";std::map<std::string,VfsEntry> cached;if(std::filesystem::is_directory(persistent))cached=Measure(cost,"source-vfs-persistent",[&]{return ReadVfs(persistent,selected);});
 std::vector<std::string> paths(selected.begin(),selected.end());std::vector<EiemStaticSources::Entry> inputs(paths.size());Measure(cost,"source-ranges",[&]{EiemOfflineCompile::ParallelInputs(paths.size(),[&](size_t i){const auto& path=paths[i];auto a=cached.find(path),b=shipped.find(path);const VfsEntry* entry=a!=cached.end()&&std::filesystem::is_regular_file(a->second.physical)?&a->second:(b!=shipped.end()?&b->second:nullptr);Require(entry,"selected physical source unavailable: "+path);Require(!entry->encrypted,"outer-encrypted static source unsupported: "+path);auto relative=entry->physical.lexically_relative(game);Require(!relative.empty()&&!relative.is_absolute()&&relative.generic_u8string().rfind("..",0)!=0,"source file escapes game directory");auto stamp=FileStamp(entry->physical);Require(entry->offset<=stamp.bytes&&entry->size<=stamp.bytes-entry->offset&&entry->size<=SIZE_MAX,"source range exceeds CHK extent");
  Bytes bytes(size_t(entry->size));std::ifstream stream(entry->physical,std::ios::binary);Require(bool(stream),"selected source file unavailable");stream.seekg(std::streamoff(entry->offset));stream.read((char*)bytes.data(),bytes.size());Require(bool(stream)&&SameStamp(stamp,FileStamp(entry->physical)),"game source changed during discovery");Require(Md5(bytes)==entry->md5,"source range MD5 differs: "+path);inputs[i]={path,EiemResourceReloadPlan::Hash(bytes),relative,entry->offset,entry->size};
 });return 0;});
 for(size_t i=0;i<paths.size();++i){selection.inputs.emplace(paths[i],std::move(inputs[i]));auto& deps=selection.dependencies[paths[i]];for(auto dep:catalog.graph[sourceId(paths[i])])if(selectedIds.count(dep))deps.insert(catalog.sources[dep]);}auto bytes=EiemStaticSources::Encode(selection);EiemOfflineCompile::AtomicSave(destination,bytes);return EiemStaticSources::LoadMod(package.root,game);
}
} // namespace EiemAuthorSources

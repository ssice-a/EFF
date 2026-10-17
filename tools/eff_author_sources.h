#pragma once
#include "../src/eiem_static_resource_sources.h"
#include "../src/eiem_vfs_catalog.h"
#include <string_view>
#include <unordered_map>

// Author-side only: index discovery and native compilation share one process.
// No AnimeStudio runtime, child process, or new whole-game index is required.
namespace EiemAuthorSources {
using namespace EiemNativeInput;
using EiemStaticSources::Logical;
using EiemStaticSources::Selection;
using Requests = std::map<Identity,std::string>;
struct IndexNeedsRefresh : std::runtime_error {
 explicit IndexNeedsRefresh():std::runtime_error("source index must describe both installed VFS layers; reopen the game folder with current AnimeStudio"){}
};
inline Requests Collect(const Package& p){
 Requests sources;for(const auto& t:p.targetIndex)sources.emplace(t.first,"Mesh");
 for(const auto& m:p.materials)if(!m.second.source.path.empty())sources.emplace(m.second.source,"Material");
 for(const auto& t:p.textures)if(!t.second.source.path.empty())sources.emplace(t.second.source,"Texture2D");
 for(const auto& r:p.resources)if(r.second->blocks.count("@source-prefabs")){auto rows=Strings(*r.second,"@source-prefabs");Require(rows.size()%2==0,"Physics source Prefab identity truncated");for(size_t i=0;i<rows.size();i+=2)sources.emplace(Identity{rows[i],rows[i+1]},"Prefab");}return sources;
}
inline std::string Json(const std::string& value){std::string s="\"";for(unsigned char c:value){if(c=='\"'||c=='\\'){s+='\\';s+=char(c);}else if(c<32){static const char* hex="0123456789ABCDEF";s+="\\u00";s+=hex[c>>4];s+=hex[c&15];}else s+=char(c);}return s+'\"';}
inline std::string RequestJson(const Requests& sources){std::string s="{\"sources\":[";bool first=true;for(const auto& row:sources){if(!first)s+=',';first=false;s+="{\"kind\":"+Json(row.second)+",\"path\":"+Json(row.first.path)+",\"asset\":"+Json(row.first.asset)+'}';}return s+"]}";}
using EiemVfsCatalog::Reader;
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
using EiemVfsCatalog::Rotate;
using EiemVfsCatalog::ChaCha;
using EiemVfsCatalog::VfsLogical;
using EiemVfsCatalog::VfsEntry;
using EiemVfsCatalog::Files;
using EiemVfsCatalog::Fingerprint;
using EiemVfsCatalog::ReadVfs;
using EiemVfsCatalog::GameFingerprint;
using EiemVfsCatalog::ReadEffectiveVfs;
inline std::string ReadIndexFingerprint(const std::filesystem::path& path){
 std::ifstream in(path,std::ios::binary);Require(bool(in),"source index unavailable");auto string=[&](){uint32_t size=0;for(unsigned n=0;n<5;++n){int byte=in.get();Require(byte>=0,"source index header truncated");Require(n!=4||byte<=7,"source index header string length invalid");size|=uint32_t(byte&127)<<(7*n);if(!(byte&128)){Require(size<=4096,"source index header string too long");std::string value(size,'\0');in.read(value.data(),size);Require(size_t(in.gcount())==size,"source index header truncated");return value;}}throw std::runtime_error("source index header string invalid");};
 auto magic=string();Require(magic=="EFF_END_FIELD_INDEX"||magic=="EIEM_END_FIELD_INDEX","not an EFF source index");in.ignore(4);return string();
}
using EiemVfsCatalog::Md5;
inline Selection Prepare(const Package& package,const std::filesystem::path& game,const std::filesystem::path& index,std::function<void(const char*,double)> cost={},bool refresh=false){
 using EiemResourceReloadPlan::Measure;auto indexFingerprint=ReadIndexFingerprint(index);auto gameFingerprint=Measure(cost,"source-game-identity",[&]{return GameFingerprint(game);});if(indexFingerprint!=gameFingerprint)throw IndexNeedsRefresh();
 auto requests=Collect(package);auto request=RequestJson(requests)+"\nselection=reverse-mesh-consumers+declared-assets+material-dependency-closure\nresourceCatalog="+indexFingerprint;auto identity=EiemResourceReloadPlan::Hash(Bytes(request.begin(),request.end()));auto destination=package.root/"source-inputs.bin";
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
 Require(catalog.fingerprint==indexFingerprint,"source index changed during preparation");auto effective=Measure(cost,"source-vfs",[&]{return ReadEffectiveVfs(game,selected);});
 std::vector<std::string> paths(selected.begin(),selected.end());std::vector<EiemStaticSources::Entry> inputs(paths.size());Measure(cost,"source-ranges",[&]{EiemOfflineCompile::ParallelInputs(paths.size(),[&](size_t i){const auto& path=paths[i];const auto* entry=&effective.at(path);Require(!entry->encrypted,"outer-encrypted static source unsupported: "+path);auto relative=entry->physical.lexically_relative(game);Require(!relative.empty()&&!relative.is_absolute()&&relative.generic_u8string().rfind("..",0)!=0,"source file escapes game directory");auto stamp=FileStamp(entry->physical);Require(entry->offset<=stamp.bytes&&entry->size<=stamp.bytes-entry->offset&&entry->size<=SIZE_MAX,"source range exceeds CHK extent");
  Bytes bytes(size_t(entry->size));std::ifstream stream(entry->physical,std::ios::binary);Require(bool(stream),"selected source file unavailable");stream.seekg(std::streamoff(entry->offset));stream.read((char*)bytes.data(),bytes.size());Require(bool(stream)&&SameStamp(stamp,FileStamp(entry->physical)),"game source changed during discovery");Require(Md5(bytes)==entry->md5,"source range MD5 differs: "+path);inputs[i]={path,EiemResourceReloadPlan::Hash(bytes),relative,entry->offset,entry->size};
 });return 0;});
 Require(GameFingerprint(game)==gameFingerprint,"installed VFS changed during source discovery; retry after the update completes");
 for(size_t i=0;i<paths.size();++i){selection.inputs.emplace(paths[i],std::move(inputs[i]));auto& deps=selection.dependencies[paths[i]];for(auto dep:catalog.graph[sourceId(paths[i])])if(selectedIds.count(dep))deps.insert(catalog.sources[dep]);}auto bytes=EiemStaticSources::Encode(selection);EiemOfflineCompile::AtomicSave(destination,bytes);return EiemStaticSources::LoadMod(package.root,game);
}
} // namespace EiemAuthorSources

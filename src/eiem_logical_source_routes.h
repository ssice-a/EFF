#pragma once
#include "eiem_static_resource_sources.h"
#include "eiem_vfs_catalog.h"

namespace EiemLogicalSourceRoutes {
using namespace EiemNativeInput;
using Source=EiemResourceReloadPlan::Source;
using Routes=std::map<std::string,std::string>;
inline Bytes ReadEntry(const EiemVfsCatalog::VfsEntry& entry){
 const auto before=FileStamp(entry.physical);Require(entry.offset<=before.bytes&&entry.size<=before.bytes-entry.offset&&entry.size<=SIZE_MAX,"current logical source range invalid");
 std::ifstream in(entry.physical,std::ios::binary);Require(bool(in),"current logical source open failed");in.seekg(std::streamoff(entry.offset));Bytes bytes(size_t(entry.size));
 for(size_t p=0;p<bytes.size();){auto n=std::min<size_t>(1024*1024,bytes.size()-p);in.read((char*)bytes.data()+p,n);Require(size_t(in.gcount())==n,"current logical source truncated");p+=n;}
 Require(SameStamp(before,FileStamp(entry.physical))&&EiemVfsCatalog::Md5(bytes)==entry.md5,"current logical source content differs from VFS metadata");return bytes;
}
inline Bytes ReadLogicalBundle(const std::filesystem::path& game,const std::string& logical){
 auto entry=EiemVfsCatalog::ReadEffectiveVfs(game,{logical}).at(logical);Require(!entry.encrypted,"outer-encrypted runtime source unsupported: "+logical);return ReadEntry(entry);
}
inline void Include(Routes& routes,const EiemStaticSources::Selection& selection){
 for(const auto& row:selection.inputs){auto file=EiemStaticSources::SourceFile(row.first,row.second.revision);auto found=routes.emplace(file,row.first);
  Require(found.second||found.first->second==row.first,"compiled source has conflicting logical paths");}
}
inline void Rebind(std::vector<Source>& sources,const Routes& routes,const std::filesystem::path& game){
 std::set<std::string> requested;
 for(const auto& source:sources){auto found=routes.find(source.file);Require(found!=routes.end(),"compiled source has no logical route; repair the Mod: "+source.file);
  const auto& revision=source.baselineHash.empty()?source.hash:source.baselineHash;
  Require(EiemStaticSources::SourceFile(found->second,revision)==source.file,"compiled source logical identity differs");requested.insert(found->second);}
 if(requested.empty())return;
 // Read the current two-layer catalog once for the complete set. No whole-game
 // asset index, Unity query, payload decoder or compilation runs here.
 auto current=EiemVfsCatalog::ReadEffectiveVfs(game,requested);
 for(auto& source:sources){const auto& logical=routes.at(source.file);const auto& entry=current.at(logical);
  Require(!entry.encrypted,"outer-encrypted runtime source unsupported: "+logical);
  auto nextStamp=FileStamp(entry.physical);if(source.baselineHash.empty())source.baselineHash=source.hash;
  const bool sameLocation=source.physical==entry.physical&&source.offset==entry.offset&&source.size==entry.size;
  const bool sameContainerStamp=source.runtimeStampBytes==nextStamp.bytes&&source.runtimeStampWritten==nextStamp.written;
  if(!sameLocation||!sameContainerStamp){auto bytes=ReadEntry(entry);source.hash=EiemResourceReloadPlan::Hash(bytes);}
  source.logical=logical;source.physical=entry.physical;source.offset=entry.offset;source.size=entry.size;
  source.runtimeStampBytes=nextStamp.bytes;source.runtimeStampWritten=nextStamp.written;source.runtimeChanged=source.hash!=source.baselineHash;
 }
}
} // namespace EiemLogicalSourceRoutes

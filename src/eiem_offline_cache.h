#pragma once
#include "eiem_offline_resource_package.h"

namespace EiemOfflineCache {
using namespace EiemOfflinePackage;

// Builds that write the same current.tsv must share a lock even if callers
// supply different Mod directories. Resolve aliases before choosing the lock.
inline std::filesystem::path CanonicalCache(const std::filesystem::path& path) {
 return std::filesystem::weakly_canonical(std::filesystem::absolute(path));
}
inline std::string PathIdentity(const std::filesystem::path& path) {
 auto text=InputPath(CanonicalCache(path).u8string());
 return Hash(Bytes(text.begin(),text.end()));
}
inline Bytes Owner(const Package& package) {
 auto text=std::string("EFF_OFFLINE_CACHE_OWNER\t1\n")+package.id+"\t"+PathIdentity(package.root)+"\n";
 return Bytes(text.begin(),text.end());
}
inline std::vector<std::string> OwnerFields(const Bytes& bytes) {
 std::istringstream rows(Text(bytes.data(),bytes.size()));std::string header,row,extra;
 Require(bool(std::getline(rows,header))&&bool(std::getline(rows,row))&&!std::getline(rows,extra),"offline cache owner rows");
 auto fields=TabFields(Trim(row));
 Require(TableHeader(Trim(header),"EFF_OFFLINE_CACHE_OWNER")&&(fields.size()==1||fields.size()==2)&&
         !fields.front().empty()&&DigestText(fields.back()),"offline cache owner field structure");
 return fields;
}
inline bool OwnerMatches(const Bytes& actual,const Bytes& expected) {
 return OwnerFields(actual)==OwnerFields(expected);
}
inline void Claim(const std::filesystem::path& cache,const Package& package) {
 auto owner=Owner(package);auto path=cache/"owner.tsv";
 if(std::filesystem::exists(path)) {
  Require(OwnerMatches(Read(path),owner),"offline cache belongs to another Mod/source directory; select a separate cache");
  return;
 }
 // Adopt older caches only when their published Mod and source folder match.
 // The caller holds the cache-scoped build mutex throughout validation/write.
 if(std::filesystem::exists(cache/"current.tsv")) {
  auto manifest=ReadManifest(cache,ReadCurrent(cache),package.root.parent_path());
  Require(manifest.package.id==package.id &&
          PathIdentity(manifest.package.root)==PathIdentity(package.root),
          "offline legacy cache belongs to another Mod/source directory");
 }
 std::filesystem::create_directories(cache);
 EiemOfflineCompile::AtomicSave(path,owner);
}
inline void ClaimSet(const std::filesystem::path& cache,const std::filesystem::path& mods) {
 auto text=std::string("EFF_OFFLINE_CACHE_OWNER\t2\n")+PathIdentity(mods)+"\n";
 Bytes owner(text.begin(),text.end());auto path=cache/"owner.tsv";
 if(std::filesystem::exists(path)) {
  auto previous=Read(path);if(OwnerMatches(previous,owner))return;
  // A legacy single-author cache can be adopted by its containing Mod set.
  Require(std::filesystem::exists(cache/"current.tsv"),"offline cache belongs to another Mod/source directory");
  auto manifest=ReadManifest(cache,ReadCurrent(cache),mods);
  Require(!manifest.composed && OwnerMatches(previous,Owner(manifest.package)) &&
          PathIdentity(manifest.package.root.parent_path())==PathIdentity(mods),"offline cache belongs to another Mod/source directory");
 } else if(std::filesystem::exists(cache/"current.tsv")) {
  auto manifest=ReadManifest(cache,ReadCurrent(cache),mods);
  for(const auto& p:Authors(manifest)) Require(PathIdentity(p.root.parent_path())==PathIdentity(mods),"offline legacy cache belongs to another Mod set");
 }
 std::filesystem::create_directories(cache);EiemOfflineCompile::AtomicSave(path,owner);
}
} // namespace EiemOfflineCache

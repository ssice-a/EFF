#pragma once
#include "eiem_offline_resource_package.h"

// A deliberately separate author/debug protocol: no global index, TypeTree
// interpreter, compiler or Unity object appears on the DLL's disk thread.
namespace EiemEffectDebugPacket {
using namespace EiemDirectory;
constexpr const char* structure = "EFF_EFFECT_DEBUG_NATIVE/inactive-root+le64";
using Bundle=EiemEffectNative::Bundle;
using Manifest=EiemEffectNative::Manifest;
inline std::string Digest(const Bytes& b){auto d=Sha(b.data(),b.size());return Hex(d.data(),d.size());}
inline bool HexDigest(const std::string& s){return s.size()==64&&s.find_first_not_of("0123456789ABCDEF")==s.npos;}
inline std::filesystem::path Under(const std::filesystem::path& root,const std::string& relative){
 Require(!relative.empty()&&relative.find('\\')==relative.npos&&relative.find(':')==relative.npos,"effect relative path differs");
 auto tail=std::filesystem::u8path(relative);Require(!tail.is_absolute(),"effect absolute bundle path refused");
 for(const auto& part:tail)Require(part!=".."&&part!=".","effect path traversal refused");
 auto base=std::filesystem::canonical(root),path=std::filesystem::canonical(base/tail);
 auto inside=path.lexically_relative(base);Require(!inside.empty()&&!inside.is_absolute(),"effect bundle escapes build directory");
 for(const auto& part:inside)Require(part!="..","effect bundle link escapes build directory");
 return path;
}
inline std::shared_ptr<const Manifest> ReadManifest(const std::filesystem::path& file){
 Require(file.is_absolute()&&file.filename()=="runtime.bin","effect PREPARE requires absolute runtime.bin");
 auto bytes=Read(file);EiemOfflinePackage::Reader r{bytes};auto m=std::make_shared<Manifest>();
 Require(r.S()==structure,"effect debug packet structure differs");m->key=r.S();m->prefab=r.S();m->rootName=r.S();
 Require(HexDigest(m->key)&&m->prefab.rfind("assets/",0)==0&&m->prefab.size()>7&&m->prefab.find("..") ==m->prefab.npos&&!m->rootName.empty(),"effect debug identity differs");
 m->root=r.Size();m->particles=r.Size();auto count=r.Count();Require(count&&m->root<count,"effect debug root bundle absent");
 std::set<std::string> names;
 for(size_t i=0;i<count;++i){Bundle b;b.file=r.S();b.hash=r.S();Require(b.file.rfind("runtime/bundles/",0)==0&&HexDigest(b.hash)&&names.insert(b.file).second,"effect bundle identity duplicate/invalid");
  auto n=r.Count();std::set<size_t> deps;for(size_t j=0;j<n;++j){auto d=r.Size();Require(d<count&&d!=i&&deps.insert(d).second,"effect dependency duplicate/outside graph");b.dependencies.push_back(d);}m->bundles.push_back(std::move(b));
 }
 Require(r.p==bytes.size(),"effect debug manifest trailing bytes");
 // All bytes are immutable before reaching Unity, including dependency cycles.
 for(auto& b:m->bundles){auto data=std::make_shared<const Bytes>(Read(Under(file.parent_path(),b.file)));Require(Digest(*data)==b.hash,"effect bundle SHA256 differs");Require(EiemEffectNative::Archive(*data),"effect archive is not the game native envelope; rebuild with the updated AnimeStudio");b.bytes=std::move(data);}
 return m;
}
} // namespace EiemEffectDebugPacket

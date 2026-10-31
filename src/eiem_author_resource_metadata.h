#pragma once
#include "eiem_runtime_author_directory.h"

// Type-specific author metadata travels inside the resource file envelope.
// mod.ini remains the only source of cross-resource relationships.
namespace EiemAuthorResourceMetadata {
namespace D = EiemRuntimeAuthorDirectory;
inline constexpr const char* structure = "EFF_AUTHOR_RESOURCE_METADATA/3";

inline D::Writer Start(unsigned kind) {
  EiemDirectory::Require(kind >= 1 && kind <= 7, "unknown author metadata kind");
  D::Writer writer;
  writer.S(structure);
  writer.N(kind);
  return writer;
}

inline D::Reader Open(const EiemDirectory::Bytes& bytes, unsigned kind) {
  D::Reader reader{bytes};
  EiemDirectory::Require(reader.S() == structure, "author resource metadata structure differs");
  EiemDirectory::Require(reader.U() == kind, "author resource metadata kind differs");
  return reader;
}

inline EiemDirectory::Bytes EncodeMesh(const D::MeshAsset& mesh) {
  auto w = Start(1);
  w.S(mesh.part); w.S(mesh.file); w.S(mesh.path); w.S(mesh.marker); w.S(mesh.hash);
  w.S(mesh.material); w.N(mesh.vertices); w.N(mesh.indices); w.Strings(mesh.bones);
  w.Strings(mesh.shapes); w.N(mesh.binding.hashes.size());
  for (auto value : mesh.binding.hashes) w.N(value);
  w.N(mesh.binding.boneBounds.size());
  for (const auto& bounds : mesh.binding.boneBounds) {
    for (auto value : bounds.first) w.D(value);
    for (auto value : bounds.second) w.D(value);
  }
  w.Strings(mesh.fields);
  w.N(mesh.nativeBytes); w.N(mesh.authorBytes); w.S(mesh.authorHash);
  return std::move(w.b);
}

inline D::MeshAsset DecodeMesh(const EiemDirectory::Bytes& bytes) {
  auto r = Open(bytes, 1); D::MeshAsset mesh;
  mesh.part=r.S(); mesh.file=r.S(); mesh.path=r.S(); mesh.marker=r.S(); mesh.hash=r.S();
  mesh.material=r.S(); mesh.vertices=r.U(); mesh.indices=r.U(); mesh.bones=r.Strings();
  mesh.shapes=r.Strings(); auto count=r.Count(); mesh.binding.hashes.reserve(count);
  for(size_t i=0;i<count;++i) mesh.binding.hashes.push_back(r.U());
  count=r.Count(); mesh.binding.boneBounds.resize(count);
  for(auto& bounds:mesh.binding.boneBounds){for(auto& value:bounds.first)value=r.D();for(auto& value:bounds.second)value=r.D();}
  mesh.fields=r.Strings();
  auto nativeBytes=r.N(),authorBytes=r.N();
  EiemDirectory::Require(nativeBytes<=SIZE_MAX&&authorBytes<=SIZE_MAX,"author Mesh payload extent exceeds address space");
  mesh.nativeBytes=size_t(nativeBytes);mesh.authorBytes=size_t(authorBytes);mesh.authorHash=r.S();
  EiemDirectory::Require((mesh.authorBytes==0&&mesh.authorHash.empty())||
    (mesh.nativeBytes>0&&mesh.authorBytes>0&&mesh.authorHash.size()==64&&
     mesh.authorHash.find_first_not_of("0123456789ABCDEFabcdef")==std::string::npos),
    "author Mesh author-data declaration differs");
  EiemDirectory::Require(r.p==bytes.size(), "author Mesh metadata trailing data");
  EiemMeshBindingMetadata::Validate(mesh.binding, mesh.bones.size());
  return mesh;
}

inline EiemDirectory::Bytes EncodeMaterial(const D::MaterialEntry& material) {
  D::Writer w;w.S("EFF_AUTHOR_MATERIAL_EDITS/1");w.S(material.id);w.S(material.file);
  w.S(material.source.path);w.S(material.source.asset);w.N(material.authorBytes);w.S(material.hash);
  return std::move(w.b);
}

inline D::MaterialEntry DecodeMaterial(const EiemDirectory::Bytes& bytes) {
  D::Reader r{bytes};EiemDirectory::Require(r.S()=="EFF_AUTHOR_MATERIAL_EDITS/1","author Material edit metadata structure differs");
  D::MaterialEntry material;material.id=r.S();material.file=r.S();material.source={r.S(),r.S()};
  auto author=r.N();EiemDirectory::Require(author<=SIZE_MAX,"author Material extent overflow");material.authorBytes=size_t(author);material.hash=r.S();
  EiemDirectory::Require(material.authorBytes&&material.hash.size()==64&&
    material.hash.find_first_not_of("0123456789ABCDEFabcdef")==std::string::npos,
    "author Material edit payload declaration differs");
  EiemDirectory::Require(r.p==bytes.size(), "author Material metadata trailing data");
  return material;
}

inline EiemDirectory::Bytes EncodeTexture(const D::TextureAsset& texture) {
  auto w=Start(3); w.S(texture.id); w.S(texture.file); w.S(texture.ddsFile); w.S(texture.path);
  w.S(texture.hash); w.S(texture.cab); w.N(uint64_t(texture.pathId)); w.N(texture.pixelOffset);
  w.N(texture.pixelBytes); w.N(texture.width); w.N(texture.height); w.N(texture.mips); w.N(texture.format);
  return std::move(w.b);
}

inline D::TextureAsset DecodeTexture(const EiemDirectory::Bytes& bytes) {
  auto r=Open(bytes,3); D::TextureAsset texture; texture.id=r.S(); texture.file=r.S(); texture.ddsFile=r.S();
  texture.path=r.S(); texture.hash=r.S(); texture.cab=r.S(); texture.pathId=int64_t(r.N());
  auto offset=r.N(), extent=r.N(); EiemDirectory::Require(offset<=SIZE_MAX&&extent<=SIZE_MAX,
    "author Texture span exceeds address space"); texture.pixelOffset=size_t(offset);texture.pixelBytes=size_t(extent);
  texture.width=r.U();texture.height=r.U();texture.mips=r.U();texture.format=r.U();
  EiemDirectory::Require(r.p==bytes.size(), "author Texture metadata trailing data");
  return texture;
}

inline EiemDirectory::Bytes EncodeAuxiliary(const D::Auxiliary& resource) {
  auto w=Start(resource.kind==5?4:5); w.S(resource.id); w.S(resource.file); w.S(resource.hash);
  w.N(resource.kind); w.N(resource.bytes); w.N(resource.targets.size());
  for(const auto& target:resource.targets){w.S(target.path);w.S(target.asset);}
  w.N(resource.authorBytes);w.S(resource.authorHash);
  return std::move(w.b);
}

inline D::Auxiliary DecodeAuxiliary(const EiemDirectory::Bytes& bytes, unsigned envelopeKind) {
  auto r=Open(bytes,envelopeKind); D::Auxiliary resource; resource.id=r.S();resource.file=r.S();resource.hash=r.S();
  resource.kind=r.U();auto extent=r.N();EiemDirectory::Require(extent<=SIZE_MAX,"author resource extent exceeds address space");resource.bytes=size_t(extent);
  auto count=r.Count();for(size_t i=0;i<count;++i){auto path=r.S(),asset=r.S();
    EiemDirectory::Require(resource.targets.emplace(EiemDirectory::Identity{std::move(path),std::move(asset)}).second,
      "duplicate author auxiliary target");}
  auto author=r.N();EiemDirectory::Require(author<=SIZE_MAX,"author auxiliary snapshot size overflow");
  resource.authorBytes=size_t(author);resource.authorHash=r.S();
  EiemDirectory::Require(resource.kind==5&&resource.bytes>=80&&resource.authorBytes>0&&
    resource.hash.size()==64&&resource.hash.find_first_not_of("0123456789ABCDEF")==std::string::npos&&
    resource.authorHash.size()==64&&resource.authorHash.find_first_not_of("0123456789ABCDEF")==std::string::npos&&
    !resource.targets.empty(),"author Physics native/editable declarations differ");
  EiemDirectory::Require(r.p==bytes.size(), "author auxiliary metadata trailing data");return resource;
}

inline EiemDirectory::Bytes EncodeEffect(const std::string& id,const std::string& file,
                                         const std::string& hash,size_t extent) {
  auto w=Start(5);w.S(id);w.S(file);w.S(hash);w.N(extent);return std::move(w.b);
}

struct Effect {std::string id,file,hash;size_t bytes=0;};
inline Effect DecodeEffect(const EiemDirectory::Bytes& bytes) {
  auto r=Open(bytes,5);Effect effect;effect.id=r.S();effect.file=r.S();effect.hash=r.S();
  auto extent=r.N();EiemDirectory::Require(extent<=SIZE_MAX,"author effect extent exceeds address space");
  effect.bytes=size_t(extent);EiemDirectory::Require(r.p==bytes.size(),"author effect metadata trailing data");
  return effect;
}

inline EiemDirectory::Bytes EncodeEffectAuthor(const std::string& id,const std::string& file,
                                               size_t extent,const std::string& hash) {
  auto w=Start(7);w.S(id);w.S(file);w.N(extent);w.S(hash);return std::move(w.b);
}

struct EffectAuthor {std::string id,file,hash;size_t bytes=0;};
inline EffectAuthor DecodeEffectAuthor(const EiemDirectory::Bytes& bytes) {
  auto r=Open(bytes,7);EffectAuthor author;author.id=r.S();author.file=r.S();auto extent=r.N();
  EiemDirectory::Require(extent<=SIZE_MAX,"author Effect snapshot extent exceeds address space");
  author.bytes=size_t(extent);author.hash=r.S();
  EiemDirectory::Require(author.id.rfind("ResourceEffectAuthor",0)==0&&!author.file.empty()&&author.bytes&&
    author.hash.size()==64&&author.hash.find_first_not_of("0123456789ABCDEFabcdef")==std::string::npos,
    "author Effect snapshot declaration differs");
  EiemDirectory::Require(r.p==bytes.size(),"author Effect snapshot metadata trailing data");return author;
}

inline EiemDirectory::Bytes EncodeControls(const EiemAuthorControlMetadata::State& state) {
  auto w=Start(6); EiemAuthorControlMetadata::Write(w,state); return std::move(w.b);
}

inline EiemAuthorControlMetadata::State DecodeControls(const EiemDirectory::Bytes& bytes) {
  auto r=Open(bytes,6); auto state=EiemAuthorControlMetadata::Read(r);
  EiemDirectory::Require(r.p==bytes.size(), "author controls metadata trailing data");return state;
}
} // namespace EiemAuthorResourceMetadata

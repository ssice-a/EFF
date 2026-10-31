#include "../../src/eiem_author_resource_metadata.h"
#include "../../src/eiem_author_resource_file.h"
#include <iostream>

using namespace EiemDirectory;
namespace M=EiemAuthorResourceMetadata;
namespace D=EiemRuntimeAuthorDirectory;

int main(){try{
  D::MeshAsset mesh;mesh.part="PartBody";mesh.file="meshes/body.mesh";mesh.path="assets/body.mesh";
  mesh.marker="Body";mesh.hash=std::string(64,'A');mesh.material="MaterialBody";mesh.vertices=3;mesh.indices=3;
  mesh.fields={"m_IndexBuffer","m_Name","m_VertexData"};
  auto meshRoundtrip=M::DecodeMesh(M::EncodeMesh(mesh));
  Require(meshRoundtrip.part==mesh.part&&meshRoundtrip.fields==mesh.fields&&meshRoundtrip.vertices==3,
    "Mesh author metadata roundtrip differs");

  Bytes nativeMesh{'N','A','T','I','V','E'};
  Bytes authorMesh{'E','F','F','M','E','S','H',0,6,0,0,0};
  mesh.nativeBytes=nativeMesh.size();mesh.authorBytes=authorMesh.size();
  auto authorHash=Sha(authorMesh.data(),authorMesh.size());
  mesh.authorHash=Hex(authorHash.data(),authorHash.size());
  Bytes meshPayload=nativeMesh;meshPayload.insert(meshPayload.end(),authorMesh.begin(),authorMesh.end());
  auto meshFile=EiemAuthorResourceFile::Encode(1,M::EncodeMesh(mesh),meshPayload);
  auto meshRead=[&](size_t at,unsigned char* output,size_t size){
    Require(at<=meshFile.size()&&size<=meshFile.size()-at,"Mesh author resource read exceeds file");
    memcpy(output,meshFile.data()+at,size);
  };
  auto meshLayout=EiemAuthorResourceFile::Parse(meshFile.size(),meshRead);
  auto describedMesh=M::DecodeMesh(EiemAuthorResourceFile::ReadMetadata(meshLayout,meshRead));
  Require(meshLayout.kind==1&&describedMesh.nativeBytes==nativeMesh.size()&&
      describedMesh.authorBytes==authorMesh.size()&&
      describedMesh.nativeBytes+describedMesh.authorBytes==meshLayout.payloadBytes,
    "Mesh native/editable resource extents differ");
  Bytes recoveredAuthor(describedMesh.authorBytes);
  meshRead(meshLayout.payloadOffset+describedMesh.nativeBytes,recoveredAuthor.data(),recoveredAuthor.size());
  auto recoveredHash=Sha(recoveredAuthor.data(),recoveredAuthor.size());
  Require(recoveredAuthor==authorMesh&&Hex(recoveredHash.data(),recoveredHash.size())==describedMesh.authorHash,
    "Mesh editable author snapshot does not roundtrip with its native resource");

  D::MaterialEntry material;material.id="MaterialBody";material.file="native/materials/body.effres";
  material.hash=std::string(64,'B');material.source={"assets/base/body.mat","BaseMaterial"};
  std::string authorText="format=EFFMAT\nversion=1\noverrides=true\nsource=assets/base/body.mat\nname=BaseMaterial\nfloat._Gloss=.4\n";
  Bytes authorMaterial(authorText.begin(),authorText.end());material.authorBytes=authorMaterial.size();
  auto materialAuthorHash=Sha(authorMaterial.data(),authorMaterial.size());material.hash=Hex(materialAuthorHash.data(),materialAuthorHash.size());
  auto materialRoundtrip=M::DecodeMaterial(M::EncodeMaterial(material));
  Require(materialRoundtrip.source.path==material.source.path&&materialRoundtrip.source.asset==material.source.asset&&
      materialRoundtrip.authorBytes==authorMaterial.size()&&materialRoundtrip.hash==material.hash,
    "sparse Material edit metadata roundtrip differs");
  Bytes materialPayload=authorMaterial;
  auto materialFile=EiemAuthorResourceFile::Encode(2,M::EncodeMaterial(material),materialPayload);
  auto materialRead=[&](size_t at,unsigned char* output,size_t size){
    Require(at<=materialFile.size()&&size<=materialFile.size()-at,"Material author resource read exceeds file");
    memcpy(output,materialFile.data()+at,size);
  };
  auto materialLayout=EiemAuthorResourceFile::Parse(materialFile.size(),materialRead);
  auto describedMaterial=M::DecodeMaterial(EiemAuthorResourceFile::ReadMetadata(materialLayout,materialRead));
  Require(materialLayout.kind==2&&describedMaterial.authorBytes==materialLayout.payloadBytes,
    "Material edit resource extent differs");
  Bytes recoveredMaterial(describedMaterial.authorBytes);
  materialRead(materialLayout.payloadOffset,recoveredMaterial.data(),recoveredMaterial.size());
  auto recoveredMaterialHash=Sha(recoveredMaterial.data(),recoveredMaterial.size());
  Require(recoveredMaterial==authorMaterial&&Hex(recoveredMaterialHash.data(),recoveredMaterialHash.size())==describedMaterial.hash,
    "sparse Material edits do not roundtrip independently of native game data");

  D::TextureAsset texture;texture.id="TextureBody";texture.file="textures/body.texmeta";
  texture.ddsFile="editable/body.dds";texture.path="assets/body.tex";texture.hash=std::string(64,'C');
  texture.cab="CAB-0011223344556677";texture.pathId=23;texture.pixelOffset=300;texture.pixelBytes=16;
  texture.width=4;texture.height=4;texture.mips=1;texture.format=25;
  auto textureRoundtrip=M::DecodeTexture(M::EncodeTexture(texture));
  Require(textureRoundtrip.ddsFile==texture.ddsFile&&textureRoundtrip.pixelOffset==300&&textureRoundtrip.format==25,
    "Texture/DDS metadata roundtrip differs");

  D::Auxiliary physics;physics.id="ResourcePhysics";physics.file="resources/physics.bin";
  physics.hash=std::string(64,'D');physics.kind=5;physics.bytes=80;
  physics.authorBytes=128;physics.authorHash=std::string(64,'E');
  physics.targets.insert({"assets/body.mesh","Body"});
  auto physicsRoundtrip=M::DecodeAuxiliary(M::EncodeAuxiliary(physics),4);
  Require(physicsRoundtrip.targets.size()==1&&
      !(*physicsRoundtrip.targets.begin()<*physics.targets.begin())&&
      !(*physics.targets.begin()<*physicsRoundtrip.targets.begin())&&physicsRoundtrip.bytes==80&&
      physicsRoundtrip.authorBytes==128&&physicsRoundtrip.authorHash==physics.authorHash,
    "Physics metadata roundtrip differs");
  EiemAuthorControlMetadata::State controls;
  auto controlsRoundtrip=M::DecodeControls(M::EncodeControls(controls));
  Require(controlsRoundtrip.defaults.empty(),"controls metadata roundtrip differs");
  std::string effectAuthorHash(64,'F');
  auto effectAuthor=M::DecodeEffectAuthor(M::EncodeEffectAuthor(
    "ResourceEffectAuthor_Test","native/effect-author/test.effres",256,effectAuthorHash));
  Require(effectAuthor.id=="ResourceEffectAuthor_Test"&&effectAuthor.bytes==256&&
    effectAuthor.hash==effectAuthorHash,"Effect author metadata roundtrip differs");
  std::cout<<"PASS typed Mesh, Material, Texture/DDS, auxiliary, Effect author and controls metadata roundtrip\n";
  return 0;
}catch(const std::exception& error){std::cerr<<"FAIL: "<<error.what()<<'\n';return 1;}}

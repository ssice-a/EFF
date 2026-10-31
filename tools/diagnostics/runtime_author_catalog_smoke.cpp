#include "../../src/eiem_demand_author_resources.h"
#include "../../src/eiem_demand_material_assembly.h"
#include <fstream>
#include <iostream>

using namespace EiemDirectory;
namespace D=EiemRuntimeAuthorDirectory;
namespace M=EiemAuthorResourceMetadata;
namespace F=EiemAuthorResourceFile;
namespace A=EiemDemandAuthorResources;
namespace R=EiemNativeRandomAccess;
namespace Native=EiemDemandNativeAssembly;
namespace MatAsm=EiemDemandMaterialAssembly;

static EiemNativeStandaloneInput::Input DummyMaterialInput(const EiemDemandAuthorResources::Material& material){
  auto bytes=Bytes(material.declaration.id.begin(),material.declaration.id.end());auto digest=Sha(bytes.data(),bytes.size());
  auto token=Hex(digest.data(),digest.size());EiemNativeStandaloneInput::Input input;
  input.cab="CAB-"+token.substr(0,32);input.bundle="eff/"+token+".ab";input.path="assets/eff/"+token;
  input.asset={input.cab,1};auto literal=std::make_shared<const Bytes>(Bytes{1});
  input.view=std::make_shared<const EiemInputSpanView::View>(
    std::vector<EiemInputSpanView::Span>{{EiemInputSpanView::literalSource,0,literal->size(),literal}},
    std::vector<size_t>{},EiemInputSpanView::ReadRange{});
  return input;
}

static void Save(const std::filesystem::path& path,const Bytes& bytes){
  std::filesystem::create_directories(path.parent_path());
  std::ofstream output(path,std::ios::binary|std::ios::trunc);
  Require(bool(output),"smoke resource output open failed");
  output.write(reinterpret_cast<const char*>(bytes.data()),std::streamsize(bytes.size()));
  Require(bool(output),"smoke resource output write failed");
}
static void SaveText(const std::filesystem::path& path,const std::string& value){
  std::filesystem::create_directories(path.parent_path());
  std::ofstream output(path,std::ios::binary|std::ios::trunc);
  output<<value;Require(bool(output),"smoke Mod INI output failed");
}
template<class F> static bool Fails(F&& action){try{action();return false;}catch(const std::exception&){return true;}}

static int SmokeRealMod(const std::filesystem::path& root){
  auto begin=std::chrono::steady_clock::now();auto catalog=std::make_shared<A::Catalog>(root);const auto& directory=catalog->Directory();
  Require(!directory.meshes.empty()&&!directory.materials.empty()&&!directory.textures.empty(),
    "real format 3 Mod lacks Mesh/Material/Texture resources");
  Require(catalog->OpenMeshes()==0&&catalog->OpenMaterials()==0&&catalog->OpenTextures()==0&&catalog->OpenPhysics()==0,
    "format 3 startup opened a native resource payload");
  size_t meshBytes=0,materialBytes=0,textureBytes=0,physicsBytes=0;
  for(const auto& declaration:directory.meshes){auto mesh=catalog->GetMesh(declaration.part);
    Require(mesh->object.fields.size()>=declaration.fields.size(),"real Mesh selected fields are absent");meshBytes+=mesh->metadataReadBytes;}
  std::map<std::string,std::shared_ptr<const A::Texture>> texturesById;
  for(const auto& declaration:directory.textures){auto texture=catalog->GetTexture(declaration.id);
    texturesById.emplace(declaration.id,texture);
    Require(texture->input&&texture->input->Size()>texture->declaration.pixelOffset,"real Texture native view is empty");
    auto dds=std::make_shared<R::File>(A::AssetPath(root,declaration.ddsFile),0);Bytes header(std::min(dds->Size(),size_t(148)));
    dds->ReadAt(0,header.data(),header.size());auto layout=EiemDdsTexture::Parse(header,dds->Size());
    size_t count=std::min(declaration.pixelBytes,size_t(64));Bytes expected(count),actual(count);
    dds->ReadAt(layout.offset,expected.data(),count);Require(texture->input->ReadAt(declaration.pixelOffset,actual.data(),count)==count&&actual==expected,
      "real DDS pixels differ from the native game input view");
    auto metadata=std::make_shared<R::File>(A::AssetPath(root,declaration.file),0);
    auto envelope=A::AuthorLayout(metadata,3);auto suffixBytes=envelope.payloadBytes-declaration.pixelOffset;
    Require(texture->input->Size()==envelope.payloadBytes+declaration.pixelBytes,
      "real DDS reconstructed input is truncated after pixels");
    expected.resize(suffixBytes);actual.resize(suffixBytes);
    metadata->ReadAt(envelope.payloadOffset+declaration.pixelOffset,expected.data(),suffixBytes);
    Require(texture->input->ReadAt(declaration.pixelOffset+declaration.pixelBytes,actual.data(),suffixBytes)==suffixBytes&&actual==expected,
      "real DDS native suffix differs from metadata file");
    textureBytes+=texture->metadataReadBytes;}
  for(const auto& declaration:directory.materials){auto material=catalog->GetMaterial(declaration.id);
    Require(material->authored.source==declaration.source.path,"real sparse Material source identity differs");
    std::map<std::string,std::string> bindings;for(const auto& row:material->authored.textures)bindings.emplace(row.first,row.second);
    Require(bindings==declaration.textures,"real sparse Material texture bindings differ");materialBytes+=material->metadataReadBytes;
    for(const auto& binding:declaration.textures)
      Require(material->textureAssets.at(binding.first)==texturesById.at(binding.second),
        "Material reopened or failed to retain its cached Texture dependency");}
  std::set<Identity> targets;for(const auto& rule:directory.rules)for(const auto& target:rule.targets)targets.insert(target.second);
  for(const auto& physics:catalog->GetPhysics(targets))physicsBytes+=physics->backing?ResourceSize(*physics->backing):0;
  Native::Author author(catalog);std::vector<MatAsm::Use> uses;size_t renderer=1;
  for(const auto& mesh:directory.meshes)uses.push_back({{int64_t(renderer++),mesh.part},mesh.material});
  if(!uses.empty())uses.push_back({{int64_t(renderer++),uses.front().key.second},uses.front().material});
  size_t prepareCalls=0;auto shared=MatAsm::PrepareShared(author,uses,[&](const auto& material,const auto&){
    ++prepareCalls;return DummyMaterialInput(material);});
  std::set<std::string> usedMaterials;for(const auto& use:uses)usedMaterials.insert(use.material);
  Require(shared.references.size()==uses.size()&&shared.inputs.size()==usedMaterials.size()&&
    prepareCalls==usedMaterials.size(),"real Format 3 Material variant sharing differs by authored Material ID");
  Require(catalog->OpenMeshes()==directory.meshes.size()&&catalog->OpenMaterials()==directory.materials.size()&&
    catalog->OpenTextures()==directory.textures.size(),"real format 3 resource cache counts differ");
  size_t effectBundles=0,effectBytes=0;
  if(!directory.runtime.effects.empty()){
    Package package;EiemAuthorControlMetadata::Apply(directory.runtime,package);catalog->BindEffects(package);
    for(const auto& effect:package.effects)for(const auto& bundle:effect.second.native->bundles){
      Require(bool(bundle.open),"real format 3 Effect bundle is not demand-openable");
      auto view=bundle.open();Require(view&&view->Size()==bundle.Size(),"real format 3 Effect view extent differs");
      Bytes header(48);Require(view->ReadAt(0,header.data(),header.size())==header.size()&&
        EiemEffectNative::Archive(header.data(),view->Size()),"real format 3 Effect view is not a native game archive");
      ++effectBundles;effectBytes+=view->Size();
    }
  }
  std::cout<<"PASS real format 3 Catalog mod="<<directory.mod<<" meshes="<<directory.meshes.size()<<
    " materials="<<directory.materials.size()<<" textures="<<directory.textures.size()<<" rules="<<directory.rules.size()<<
    " physics="<<catalog->OpenPhysics()<<" rendererMaterialUses="<<shared.references.size()<<" materialVariants="<<shared.inputs.size()<<" prepareCalls="<<prepareCalls<<" meshMetadataBytes="<<meshBytes<<" materialMetadataBytes="<<materialBytes<<
    " textureMetadataBytes="<<textureBytes<<" physicsBytes="<<physicsBytes<<" effectBundles="<<effectBundles<<" effectBytes="<<effectBytes<<" ms="<<
    std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count()<<"\n";
  return 0;
}

int main(int argc,char** argv){
  if(argc==2){try{return SmokeRealMod(std::filesystem::u8path(argv[1]));}
    catch(const std::exception& error){std::cerr<<"FAIL real format 3 Mod: "<<error.what()<<'\n';return 1;}}
  if(argc!=1){std::cerr<<"usage: runtime_author_catalog_smoke [format-3-Mod]\n";return 2;}
  auto root=std::filesystem::temp_directory_path()/
    ("eff-format3-catalog-"+std::to_string(GetTickCount64()));
  try{
    D::MeshAsset mesh;mesh.part="PartBody";mesh.file="meshes/body.mesh";mesh.path="assets/body.mesh";
    mesh.marker="Body";mesh.hash=std::string(64,'A');mesh.material="MaterialBody";
    mesh.vertices=3;mesh.indices=3;mesh.fields={"m_IndexBuffer","m_Name","m_VertexData"};
    Bytes nativeMesh{1,2,3};Bytes authorMesh{'E','F','F','M','E','S','H',0,6,0,0,0};
    mesh.nativeBytes=nativeMesh.size();mesh.authorBytes=authorMesh.size();auto authorMeshHash=Sha(authorMesh.data(),authorMesh.size());mesh.authorHash=Hex(authorMeshHash.data(),authorMeshHash.size());
    D::MaterialEntry material;material.id="MaterialBody";material.file="custom/materials/body.author";
    material.hash=std::string(64,'B');material.source={"assets/edited/body.mat","EditedMaterial"};
    std::string authorText="format=EFFMAT\nversion=1\noverrides=true\nsource=assets/edited/body.mat\n"
      "name=EditedMaterial\n";
    Bytes authorMaterial(authorText.begin(),authorText.end());material.authorBytes=authorMaterial.size();
    auto authorMaterialHash=Sha(authorMaterial.data(),authorMaterial.size());material.hash=Hex(authorMaterialHash.data(),authorMaterialHash.size());
    D::TextureAsset texture;texture.id="TextureBody";texture.file="textures/body.tex";
    texture.ddsFile="editable/body.dds";texture.path="assets/base/body.tex";
    texture.hash=std::string(64,'C');texture.cab="CAB-ABCDEF0123456789";texture.pathId=23;
    texture.pixelOffset=1;texture.pixelBytes=4;texture.width=1;texture.height=1;texture.mips=1;texture.format=4;
    nativeMesh.insert(nativeMesh.end(),authorMesh.begin(),authorMesh.end());
    Save(root/mesh.file,F::Encode(1,M::EncodeMesh(mesh),nativeMesh));
    Save(root/material.file,F::Encode(2,M::EncodeMaterial(material),authorMaterial));
    Save(root/texture.file,F::Encode(3,M::EncodeTexture(texture),Bytes{7,8,9}));
    auto dds=EiemDdsTexture::Header(1,1,1,4,false);dds.insert(dds.end(),{1,2,3,4});
    Save(root/"artist/albedo-final.dds",dds);
    EiemAuthorControlMetadata::State controls;EiemAuthorControlMetadata::PartState state;
    state.visible=EiemExpression::Make();state.visible->number=1;controls.parts.emplace("PartBody",state);
    Save(root/"resources/controls.effres",F::Encode(6,M::EncodeControls(controls),{}));
    SaveText(root/"mod.ini",
      "[Mod]\nformat=3\nid=CatalogSmoke\nkey_switch_enabled=1\n\n"
      "[Constants]\n\n"
      "[MeshBody]\npath=custom/geometry/body.author\n\n"
      "[MaterialBody]\npath=custom/materials/body.author\nsource.path=assets/edited/body.mat\nsource.asset=EditedMaterial\n\n"
      "[TextureBody]\npath=custom/texture/albedo.author\ndds=artist/albedo-final.dds\n\n"
      "[MeshUnhit]\npath=missing/geometry/unhit.author\n\n"
      "[MaterialUnhit]\npath=missing/materials/unhit.author\nsource.path=assets/unhit/material.mat\nsource.asset=UnhitMaterial\ntexture._BaseMap=TextureUnhit\n\n"
      "[TextureUnhit]\npath=missing/textures/unhit.author\ndds=missing/textures/unhit.dds\n\n"
      "[ResourceControls]\ntype=controls\npath=resources/controls.effres\n\n"
      "[PartBody]\nmesh=MeshBody\nmaterial=MaterialBody\nvisible=1\n\n"
      "[PartUnhit]\nmesh=MeshUnhit\nmaterial=MaterialUnhit\nvisible=1\n\n"
      "[ReplaceBody]\ntarget.path=assets/base/body.mesh\ntarget.asset=Body\nparts=PartBody\n\n"
      "[ReplaceUnhit]\ntarget.path=assets/unhit/mesh.mesh\ntarget.asset=Unhit\nparts=PartUnhit\n\n"
      "[ResourceUnhitPhysics]\ntype=physics\npath=missing/physics/unhit.author\ntargets=ReplaceUnhit\n");
    SaveText(root/"assets.bin","must not be opened by format 3");

    std::filesystem::create_directories(root/"custom/geometry");
    std::filesystem::create_directories(root/"custom/materials");
    std::filesystem::create_directories(root/"custom/texture");
    std::filesystem::rename(root/mesh.file,root/"custom/geometry/body.author");
    std::filesystem::rename(root/material.file,root/"custom/materials/body.author");
    std::filesystem::rename(root/texture.file,root/"custom/texture/albedo.author");

    A::Catalog catalog(root);const auto& directory=catalog.Directory();
    Require(directory.mod=="CatalogSmoke"&&directory.meshes.size()==2&&
      directory.materials.size()==2&&directory.textures.size()==2&&directory.rules.size()==2,
      "format 3 Catalog relationships differ");
    Require(directory.meshes[0].part=="PartBody"&&directory.meshes[0].material=="MaterialBody"&&
      directory.meshes[0].file=="custom/geometry/body.author"&&
      directory.materials[0].file=="custom/materials/body.author"&&
      directory.materials[0].source.path=="assets/edited/body.mat"&&
      directory.materials[0].textures.empty()&&
      directory.textures[0].file=="custom/texture/albedo.author"&&
      directory.textures[0].ddsFile=="artist/albedo-final.dds"&&directory.runtime.parts.count("PartBody"),
      "format 3 Catalog resource metadata differs");
    Require(catalog.OpenMeshes()==0&&catalog.OpenMaterials()==0&&catalog.OpenTextures()==0&&catalog.OpenPhysics()==0,
      "format 3 startup opened resource payloads");
    Require(Fails([&]{catalog.GetMesh("PartUnhit");})&&
      Fails([&]{catalog.GetMaterial("MaterialUnhit");})&&
      Fails([&]{catalog.GetTexture("TextureUnhit");})&&
      Fails([&]{catalog.GetPhysics({Identity{"assets/unhit/mesh.mesh","Unhit"}});}),
      "missing unhit resources were not deferred until their own request");
    Require(catalog.OpenMeshes()==0&&catalog.OpenMaterials()==0&&catalog.OpenTextures()==0&&catalog.OpenPhysics()==0,
      "failed unhit resource requests polluted the asset caches");
    auto opened=catalog.GetMaterial("MaterialBody");
    Require(opened->authored.source=="assets/edited/body.mat"&&opened->authored.textures.empty(),
      "format 3 Catalog did not read sparse Material edits independently of a native snapshot");
    Native::Author nativeAuthor(std::make_shared<A::Catalog>(root));
    size_t prepareCalls=0;std::vector<MatAsm::Use> uses{{{1,"PartBody"},"MaterialBody"},{{2,"PartBody"},"MaterialBody"}};
    auto shared=MatAsm::PrepareShared(nativeAuthor,uses,[&](const auto& material,const auto&){++prepareCalls;return DummyMaterialInput(material);});
    Require(shared.references.size()==2&&shared.inputs.size()==1&&prepareCalls==1,
      "one sparse Material edit generated duplicate variants for multiple Renderer uses");
    std::filesystem::remove_all(root);
    std::cout<<"PASS format 3 Catalog defers missing unhit Mesh/Material/Texture/Physics files and reads mod.ini without assets.bin\n";
    return 0;
  }catch(const std::exception& error){std::error_code ignored;std::filesystem::remove_all(root,ignored);
    std::cerr<<"FAIL: "<<error.what()<<'\n';return 1;}
}

#define NOMINMAX
#include "../../src/eiem_demand_author_resources.h"
#include "../../src/eiem_demand_material_assembly.h"
#include "../../src/eiem_runtime_shape_identity.h"
#include <fstream>
#include <iostream>
#include <cstring>

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
static EiemNativeStandaloneInput::Input DummyMaterialInput(const std::string& cab,int64_t pathId,const Bytes& bytes){
  EiemNativeStandaloneInput::Input input;input.cab=cab;input.bundle="eff/shared.ab";input.path="assets/eff/shared";
  input.asset={cab,pathId};auto literal=std::make_shared<const Bytes>(bytes);
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
static void PutU32(Bytes& bytes,uint32_t value){for(unsigned i=0;i<4;++i)bytes.push_back(static_cast<unsigned char>(value>>(i*8)));}
static void PutI32(Bytes& bytes,int32_t value){PutU32(bytes,static_cast<uint32_t>(value));}
static void PutF32(Bytes& bytes,float value){uint32_t bits=0;std::memcpy(&bits,&value,sizeof(bits));PutU32(bytes,bits);}
static void PutString16(Bytes& bytes,const std::string& value){Require(!value.empty()&&value.size()<=65535,"smoke animation string invalid");bytes.push_back(static_cast<unsigned char>(value.size()));bytes.push_back(static_cast<unsigned char>(value.size()>>8));bytes.insert(bytes.end(),value.begin(),value.end());}

static int SmokeStagedMeshMetadata(const std::filesystem::path& root){
  auto catalog=std::make_shared<A::Catalog>(root);const auto& directory=catalog->Directory();
  Require(!directory.meshes.empty(),"metadata regression Mod has no Mesh resources");
  auto source=std::make_shared<Native::Author>(catalog);
  auto carrier=Native::Author::Compose({source});
  auto selected=std::find_if(directory.meshes.begin(),directory.meshes.end(),[](const auto& row){
    return row.part=="PartS_actor_chen_cloth_01_lod0_1.001";
  });
  if(selected==directory.meshes.end())selected=directory.meshes.begin();
  const auto part=EiemComposedPackage::Prefix(directory.mod)+selected->part;
  const auto carrierMesh=carrier->GetMesh(part);
  EiemNativeInput::BuiltMeshPart staged;staged.part=part;
  const auto resource=carrier->package.meshes.at(part).resource;
  Require(carrier->bindings.count(resource)&&carrier->markers.count(part),
    "carrier request did not register the selected Mesh metadata");
  auto consumer=Native::Author::Compose({source});
  Require(consumer->bindings.empty()&&consumer->markers.empty(),
    "new consumer composition unexpectedly retained carrier-local metadata");
  Native::EnsureStagedMeshMetadata(*consumer,{staged});
  Require(consumer->bindings.count(resource)&&consumer->markers.at(part)==carrierMesh->declaration.marker&&
    catalog->OpenMeshes()==1,
    "consumer request did not restore only the staged Mesh metadata from the shared catalog");
  std::cout<<"PASS staged Mesh metadata restored across request compositions mod="<<directory.mod
    <<" part="<<selected->part<<" openMeshResources="<<catalog->OpenMeshes()<<"\n";
  return 0;
}

static int SmokeRealMod(const std::filesystem::path& root){
  auto begin=std::chrono::steady_clock::now();auto catalog=std::make_shared<A::Catalog>(root);const auto& directory=catalog->Directory();
  Require(!directory.meshes.empty()&&!directory.materials.empty()&&!directory.textures.empty(),
    "real format 3 Mod lacks Mesh/Material/Texture resources");
  Require(catalog->OpenMeshes()==0&&catalog->OpenMaterials()==0&&catalog->OpenTextures()==0&&catalog->OpenPhysics()==0,
    "format 3 startup opened a native resource payload");
  Native::Author controlAuthor(catalog);const auto published=controlAuthor.package;
  for(const auto& row:published.effects){const auto& effect=row.second;
    if(effect.part.empty()||effect.bone.empty())continue;
    const auto& owner=published.meshes.at(published.parts.at(effect.part).mesh);
    auto declaration=std::find_if(directory.meshes.begin(),directory.meshes.end(),[&](const auto& mesh){return mesh.part==effect.part;});
    Require(declaration!=directory.meshes.end(),"real effect owner Mesh declaration is missing");
    auto metadata=A::ReadMeshMetadata(root,*declaration,effect.part);
    Require(!owner.bones.empty()&&owner.bones==metadata.declaration.bones&&catalog->OpenMeshes()==0,
      "real effect control publication lost its ordered owner palette or opened native payload");
    std::cout<<"PASS real effect owner metadata mod="<<published.id<<" part="<<effect.part
      <<" bones="<<owner.bones.size()<<" openMeshResources=0\n";
  }
  size_t meshBytes=0,materialBytes=0,textureBytes=0,physicsBytes=0;
  for(const auto& declaration:directory.meshes){try{auto mesh=catalog->GetMesh(declaration.part);
    Require(mesh->object.fields.size()>=declaration.fields.size(),"real Mesh selected fields are absent");meshBytes+=mesh->metadataReadBytes;
  }catch(const std::exception& error){throw std::runtime_error("real Mesh "+declaration.part+" ("+declaration.file+"): "+error.what());}}
  std::map<std::string,std::shared_ptr<const A::Texture>> texturesById;
  for(const auto& declaration:directory.textures){try{auto texture=catalog->GetTexture(declaration.id);
    texturesById.emplace(declaration.id,texture);
    Require(texture->input&&texture->input->Size()>0,"real Texture native view is empty");
    auto dds=std::make_shared<R::File>(A::AssetPath(root,declaration.ddsFile),0);Bytes header(std::min(dds->Size(),size_t(148)));
    dds->ReadAt(0,header.data(),header.size());auto layout=EiemDdsTexture::Parse(header,dds->Size());
    Require(texture->replacement.width==layout.width&&texture->replacement.height==layout.height&&
      texture->replacement.mips==layout.mips&&texture->replacement.format==layout.format,
      "runtime Texture layout did not come from the DDS header");
    R::Reader native(texture->input->Size(),[view=texture->input](size_t at,unsigned char* out,size_t n){
      Require(view->ReadAt(at,out,n)==n,"runtime Texture view read was truncated");});
    const auto& member=native.directory.members.front();auto nativeLayout=native.Layout(member);
    auto assets=native.Assets(member,nativeLayout);Require(assets.size()==1,"runtime Texture output asset is ambiguous");
    const auto& asset=assets.front();
    auto fields=native.Fields(member,nativeLayout,asset.pathId,
      {"m_Width","m_Height","m_MipCount","m_TextureFormat","m_CompleteImageSize"});
    Require(Integer(fields.at("m_Width"))==layout.width&&Integer(fields.at("m_Height"))==layout.height&&
      Integer(fields.at("m_MipCount"))==layout.mips&&Integer(fields.at("m_TextureFormat"))==layout.format&&
      Integer(fields.at("m_CompleteImageSize"))==layout.bytes,
      "native Texture2D fields do not match the replacement DDS");
    const auto& image=native.Ranges(member,nativeLayout,asset.pathId).at("image data");
    Bytes imageLength(4);native.ReadAt(image.offset,imageLength.data(),imageLength.size());
    Require(Le(imageLength,0,4)==layout.bytes,"native Texture2D image length differs from DDS");
    size_t count=std::min(layout.bytes,size_t(64));Bytes expected(count),actual(count);
    dds->ReadAt(layout.offset,expected.data(),count);native.ReadAt(image.offset+4,actual.data(),count);
    auto mismatch=std::mismatch(expected.begin(),expected.end(),actual.begin());
    Require(mismatch.first==expected.end(),"real DDS pixel differs from native game view at byte "+
      std::to_string(size_t(mismatch.first-expected.begin()))+" (expected="+
      std::to_string(*mismatch.first)+", actual="+std::to_string(*mismatch.second)+")");
    textureBytes+=texture->metadataReadBytes;
  }catch(const std::exception& error){throw std::runtime_error("real Texture "+declaration.id+" ("+declaration.file+"): "+error.what());}}
  for(const auto& declaration:directory.materials){try{auto material=catalog->GetMaterial(declaration.id);
    Require(material->authored.source==declaration.source.path,"real sparse Material source identity differs");
    std::map<std::string,std::string> bindings;for(const auto& row:material->authored.textures)bindings.emplace(row.first,row.second);
    Require(bindings==declaration.textures,"real sparse Material texture bindings differ");materialBytes+=material->metadataReadBytes;
    for(const auto& binding:declaration.textures)
      Require(material->textureAssets.at(binding.first)==texturesById.at(binding.second),
        "Material reopened or failed to retain its cached Texture dependency");
  }catch(const std::exception& error){throw std::runtime_error("real Material "+declaration.id+" ("+declaration.file+"): "+error.what());}}
  std::set<Identity> targets;for(const auto& rule:directory.rules)for(const auto& target:rule.targets)targets.insert(target.second);
  for(const auto& physics:catalog->GetPhysics(targets))
    for(const auto& native:physics->document.templates)physicsBytes+=native.second.bytes.size();
  SmokeStagedMeshMetadata(root);
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
  if(argc==3&&std::string(argv[1])=="--staged-metadata"){
    try{return SmokeStagedMeshMetadata(std::filesystem::u8path(argv[2]));}
    catch(const std::exception& error){std::cerr<<"FAIL staged Mesh metadata: "<<error.what()<<'\n';return 1;}}
  if(argc==2){try{return SmokeRealMod(std::filesystem::u8path(argv[1]));}
    catch(const std::exception& error){std::cerr<<"FAIL real format 3 Mod: "<<error.what()<<'\n';return 1;}}
  if(argc!=1){std::cerr<<"usage: runtime_author_catalog_smoke [format-3-Mod]\n";return 2;}
  auto root=std::filesystem::temp_directory_path()/
    ("eff-format3-catalog-"+std::to_string(GetTickCount64()));
  try{
    D::MeshAsset mesh;mesh.part="PartBody";mesh.file="meshes/body.mesh";mesh.path="assets/body.mesh";
    mesh.marker="Body";mesh.hash=std::string(64,'A');mesh.material="MaterialBody";
    mesh.vertices=3;mesh.indices=3;mesh.shapes={"Smile"};mesh.fields={"m_IndexBuffer","m_Name","m_VertexData"};
    Bytes nativeMesh{1,2,3};Bytes authorMesh{'E','F','F','M','E','S','H',0,6,0,0,0};
    mesh.nativeBytes=nativeMesh.size();mesh.authorBytes=authorMesh.size();auto authorMeshHash=Sha(authorMesh.data(),authorMesh.size());mesh.authorHash=Hex(authorMeshHash.data(),authorMeshHash.size());
    D::MaterialEntry material;material.id="MaterialBody";material.file="custom/materials/body.author";
    material.hash=std::string(64,'B');material.source={"assets/edited/body.mat","EditedMaterial"};
    std::string authorText="format=EFFMAT\nversion=1\noverrides=true\nsource=assets/edited/body.mat\n"
      "name=EditedMaterial\n";
    Bytes authorMaterial(authorText.begin(),authorText.end());material.authorBytes=authorMaterial.size();
    auto authorMaterialHash=Sha(authorMaterial.data(),authorMaterial.size());material.hash=Hex(authorMaterialHash.data(),authorMaterialHash.size());
    D::TextureAsset texture;texture.id="TextureBody";texture.file="textures/body.tex";
    texture.ddsFile="editable/body.dds";
    nativeMesh.insert(nativeMesh.end(),authorMesh.begin(),authorMesh.end());
    Save(root/mesh.file,F::Encode(1,M::EncodeMesh(mesh),nativeMesh));
    Save(root/material.file,F::Encode(2,M::EncodeMaterial(material),authorMaterial));
    D::MaterialEntry twin=material;twin.id="MaterialTwin";twin.file="custom/materials/twin.author";
    Save(root/twin.file,F::Encode(2,M::EncodeMaterial(twin),authorMaterial));
    Save(root/texture.file,F::Encode(3,{},Bytes{7,8,9}));
    auto dds=EiemDdsTexture::Header(1,1,1,4,false);dds.insert(dds.end(),{1,2,3,4});
    Save(root/"artist/albedo-final.dds",dds);
    EiemAuthorControlMetadata::State controls;EiemAuthorControlMetadata::PartState state;
    controls.defaults.emplace("$smile",0);controls.persistent.insert("$smile");
    controls.controls.emplace("$smile",Control{"Smile",0,1});
    state.visible=EiemExpression::Make();state.visible->number=1;state.shapes.emplace("Smile","$smile");
    controls.parts.emplace("PartBody",state);
    controls.defaults.emplace("$anim_speed_shared",1);controls.persistent.insert("$anim_speed_shared");
    ShapeAnimation mixed;mixed.id="shared";mixed.label="Mixed";mixed.speedVariable="$anim_speed_shared";
    mixed.duration=1;mixed.speed=1;mixed.loop=true;mixed.autoplay=false;
    mixed.tracks.emplace("$smile",std::vector<float>{0,.5f,1});
    ShapeAnimation::BoneTrack bone;bone.skeleton="WeaponRig";bone.path="WeaponRoot/FxAnchor";
    bone.samples={{{0,0,0,0,0,0,1,1,1,1}},{{0,.25f,0,0,0,0,1,1,1,1}},{{0,.5f,0,0,0,0,1,1,1,1}}};
    mixed.boneTracks.push_back(std::move(bone));controls.animations.emplace(mixed.id,std::move(mixed));
    Save(root/"resources/controls.effres",F::Encode(6,M::EncodeControls(controls),{}));
    Bytes skeletonPayload{'E','F','F','S','K','A','1',0};PutU32(skeletonPayload,1);PutU32(skeletonPayload,2);
    PutString16(skeletonPayload,"unity-y-up-left-handed");PutString16(skeletonPayload,"WeaponRoot");PutI32(skeletonPayload,-1);
    for(float value:{0.f,0.f,0.f,0.f,0.f,0.f,1.f,1.f,1.f,1.f})PutF32(skeletonPayload,value);
    PutString16(skeletonPayload,"WeaponRoot/FxAnchor");PutI32(skeletonPayload,0);
    for(float value:{0.f,0.f,1.f,0.f,0.f,0.f,1.f,1.f,1.f,1.f})PutF32(skeletonPayload,value);
    D::Writer skeletonMetadata;skeletonMetadata.S(M::structure);skeletonMetadata.N(8);
    skeletonMetadata.S("WeaponRig");skeletonMetadata.S("skeletons/rig.effres");
    skeletonMetadata.N(skeletonPayload.size());auto skeletonHash=Sha(skeletonPayload.data(),skeletonPayload.size());
    skeletonMetadata.S(Hex(skeletonHash.data(),skeletonHash.size()));
    Save(root/"skeletons/rig.effres",F::Encode(8,skeletonMetadata.b,skeletonPayload));
    SaveText(root/"mod.ini",
      "[Mod]\nformat=3\nid=CatalogSmoke\nkey_switch_enabled=1\n\n"
      "[Constants]\n\n"
      "[MeshBody]\npath=custom/geometry/body.author\nskeleton=WeaponRig\n\n"
      "[MaterialBody]\npath=custom/materials/body.author\nsource.path=assets/edited/body.mat\nsource.asset=EditedMaterial\n\n"
      "[MaterialTwin]\npath=custom/materials/twin.author\nsource.path=assets/edited/body.mat\nsource.asset=EditedMaterial\n\n"
      "[TextureBody]\npath=custom/texture/albedo.author\ndds=artist/albedo-final.dds\n\n"
      "[MeshUnhit]\npath=missing/geometry/unhit.author\n\n"
      "[MaterialUnhit]\npath=missing/materials/unhit.author\nsource.path=assets/unhit/material.mat\nsource.asset=UnhitMaterial\ntexture._BaseMap=TextureUnhit\n\n"
      "[TextureUnhit]\npath=missing/textures/unhit.author\ndds=missing/textures/unhit.dds\n\n"
      "[ResourceControls]\ntype=controls\npath=resources/controls.effres\n\n"
      "[SkeletonWeaponRig]\ntype=skeleton\npath=skeletons/rig.effres\n\n"
      "[ShapeAnimationshared]\npath=resources/controls.effres\n\n"
      "[PartBody]\nmesh=MeshBody\nmaterial=MaterialBody\nvisible=1\n\n"
      "[PartUnhit]\nmesh=MeshUnhit\nmaterial_slot=2\nvisible=1\n\n"
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
      directory.materials.size()==3&&directory.textures.size()==2&&directory.rules.size()==2,
      "format 3 Catalog relationships differ");
    Require(directory.meshes[0].shapes==std::vector<std::string>{"Smile"},
      "format 3 Catalog did not read the controlled Mesh channel descriptor");
    Require(directory.skeletons.size()==1&&directory.runtime.animations.count("shared")&&
      directory.runtime.animations.at("shared").tracks.count("$smile")&&
      directory.runtime.animations.at("shared").boneTracks.size()==1&&
      directory.runtime.animations.at("shared").boneTracks.front().samples.size()==3,
      "format 3 Catalog did not keep Shape and Bone tracks in one sampled clip");
    auto skeleton=catalog.GetSkeleton("WeaponRig");
    Require(skeleton->paths.count("WeaponRoot/FxAnchor")&&catalog.OpenSkeletons()==1,
      "format 3 Catalog did not resolve the mixed clip Bone path through its Skeleton");
    Require(directory.meshes[0].part=="PartBody"&&directory.meshes[0].material=="MaterialBody"&&
      directory.meshes[0].file=="custom/geometry/body.author"&&
      directory.meshes[0].vertices==3&&directory.meshes[0].shapes==std::vector<std::string>{"Smile"}&&
      directory.materials[0].file=="custom/materials/body.author"&&
      directory.materials[0].source.path=="assets/edited/body.mat"&&
      directory.materials[0].textures.empty()&&
      directory.textures[0].file=="custom/texture/albedo.author"&&
      directory.textures[0].ddsFile=="artist/albedo-final.dds"&&directory.runtime.parts.count("PartBody"),
      "format 3 Catalog resource metadata differs");
    Require(catalog.OpenMeshes()==0&&catalog.OpenMaterials()==0&&catalog.OpenTextures()==0&&catalog.OpenPhysics()==0,
      "format 3 startup opened resource payloads");
    Native::Author shapeAuthor(std::make_shared<A::Catalog>(root));
    const auto& shapePackage=shapeAuthor.package;
    const auto& shapeMesh=shapePackage.meshes.at("PartBody");
    const auto& shapePart=shapePackage.parts.at("PartBody");
    const auto& inheritedPart=shapePackage.parts.at("PartUnhit");
    auto shapeMarker=RuntimePartName(shapePackage,"PartBody",Identity{"assets/base/body.mesh","Body"});
    Require(shapeMesh.shapes==std::vector<std::string>{"Smile"}&&shapeMesh.vertices==3&&
      shapePart.shapes.at("Smile")=="$smile"&&shapeMarker.find("__EFF_SHAPE_")!=std::string::npos&&
      inheritedPart.material.empty()&&inheritedPart.materialSlot==2&&
      shapeAuthor.catalog->OpenMeshes()==0,
      "startup author metadata lost source Material slot or opened native Mesh payload");
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
    std::vector<MatAsm::Use> equivalentUses{{{3,"PartBody"},"MaterialBody"},{{4,"PartBody"},"MaterialTwin"}};
    auto equivalentBuilder=[](const auto&,const auto&){return DummyMaterialInput("CAB-equivalent",7,Bytes{4,5,6});};
    auto equivalent=MatAsm::PrepareShared(nativeAuthor,equivalentUses,equivalentBuilder);
    Require(equivalent.references.size()==2&&equivalent.inputs.size()==1&&equivalent.variants==1,
      "equivalent Material inputs with distinct View objects were not deduplicated");
    auto conflictingBytes=[](const auto& material,const auto&){
      return DummyMaterialInput("CAB-equivalent",7,material.declaration.id=="MaterialBody"?Bytes{4,5,6}:Bytes{4,5,7});};
    Require(Fails([&]{MatAsm::PrepareShared(nativeAuthor,equivalentUses,conflictingBytes);}),
      "different Material bytes with the same CAB were accepted");
    auto conflictingIdentity=[](const auto& material,const auto&){
      return DummyMaterialInput("CAB-equivalent",material.declaration.id=="MaterialBody"?7:8,Bytes{4,5,6});};
    Require(Fails([&]{MatAsm::PrepareShared(nativeAuthor,equivalentUses,conflictingIdentity);}),
      "different Material assets with the same CAB were accepted");
    auto left=MatAsm::PrepareShared(nativeAuthor,{equivalentUses[0]},equivalentBuilder);
    auto right=MatAsm::PrepareShared(nativeAuthor,{equivalentUses[1]},equivalentBuilder);
    MatAsm::Merge(left,std::move(right));
    Require(left.references.size()==2&&left.inputs.size()==1,
      "equivalent Material inputs failed to merge across dependency groups");
    auto conflictLeft=MatAsm::PrepareShared(nativeAuthor,{equivalentUses[0]},equivalentBuilder);
    auto conflictRight=MatAsm::PrepareShared(nativeAuthor,{equivalentUses[1]},conflictingBytes);
    Require(Fails([&]{MatAsm::Merge(conflictLeft,std::move(conflictRight));}),
      "different Material bytes with the same CAB were merged across dependency groups");
    std::filesystem::remove_all(root);
    std::cout<<"PASS format 3 Catalog defers missing unhit Mesh/Material/Texture/Physics files and reads mod.ini without assets.bin\n";
    return 0;
  }catch(const std::exception& error){std::error_code ignored;std::filesystem::remove_all(root,ignored);
    std::cerr<<"FAIL: "<<error.what()<<'\n';return 1;}
}

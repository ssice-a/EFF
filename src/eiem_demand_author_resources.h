#pragma once
#include "eiem_author_controls_ini.h"
#include "eiem_demand_generation_diff.h"
#include "eiem_runtime_author_directory.h"
#include "eiem_runtime_author_manifest.h"
#include "eiem_dds_texture.h"
#include "eiem_author_material_edits.h"
#include "eiem_author_resource_file.h"
#include "eiem_author_resource_metadata.h"
#include "eiem_native_object_graft.h"
#include "eiem_native_standalone_input.h"
#include "eiem_physics_input_builder.h"
#include "eiem_author_bone_animation.h"

// Immutable authored inputs. Opening a catalog reads its small declaration;
// native assets are opened on the first actual demand and held by that
// generation's input views. No source game snapshot or consumer list is used.
namespace EiemDemandAuthorResources {
using namespace EiemNativeInput;
namespace D=EiemRuntimeAuthorDirectory;namespace R=EiemNativeRandomAccess;
namespace G=EiemNativeObjectGraft;
namespace P=EiemNativeSerializedSplice;namespace S=EiemNativeSpliceView;namespace V=EiemInputSpanView;
struct Mesh {
  D::MeshAsset declaration;
  G::AuthorObject object;
  size_t metadataReadBytes=0;
};
struct MeshSnapshot {
  std::shared_ptr<R::File> file;
  EiemAuthorResourceFile::Layout envelope;
  D::MeshAsset declaration;
  std::array<unsigned char,32> metadataDigest{};
};
struct Texture {
  D::TextureAsset declaration;
  std::shared_ptr<const EiemInputSpanView::View> input;
  EiemDdsTexture::Layout replacement;
  std::string cab,name;
  int64_t id=0;
  size_t metadataReadBytes=0;
  std::filesystem::path ownerRoot;
  Identity source;
  EiemRuntimeAuthorManifest::TextureSettings settings;
  std::shared_ptr<R::File> dds;
};
using TextureResolver=std::function<std::shared_ptr<const Texture>(const Texture&)>;
struct Material {
  std::filesystem::path ownerRoot;
  D::MaterialEntry declaration;
  EiemAuthorMaterialEdits::Author authored;
  std::map<std::string,NativeReference> textures;
  std::map<std::string,std::shared_ptr<const Texture>> textureAssets;
  size_t metadataReadBytes=0;
};
struct Physics {
  EiemPhysicsInput::Document document;
};
inline void RebindPhysics(EiemPhysicsInput::Document& document,const std::set<Identity>& original,
 const EiemRuntimeAuthorManifest::Resource& declaration,const std::vector<EiemMeshTargetDirectory::Target>& targets){
 std::map<Identity,std::set<Identity>> mapping;std::set<Identity> current;
 for(const auto& target:targets)if(std::find(declaration.targetRules.begin(),declaration.targetRules.end(),target.rule)!=declaration.targetRules.end())current.insert(target.source);
 if(!declaration.targetBindings.empty()){
  Require(declaration.targetBindings.size()==original.size(),"Physics source binding coverage differs");
  for(const auto& binding:declaration.targetBindings){Require(original.count(binding.first),"Physics binding source is absent from immutable resource");
   for(const auto& target:targets)if(target.rule==binding.second.first&&target.lod==binding.second.second)mapping[binding.first].insert(target.source);
  }
 }else if(current.size()==original.size()&&std::equal(current.begin(),current.end(),original.begin(),[](const auto& a,const auto& b){return !(a<b)&&!(b<a);})){for(const auto& source:original)mapping[source].insert(source);}
 else if(declaration.targetRules.size()==1){for(const auto& source:original)mapping[source]=current;}
 else throw std::runtime_error("legacy Physics with multiple retargeted rules requires explicit bind.N rule/LOD/source declarations; re-export this Mod");
 for(auto it=document.groups.begin();it!=document.groups.end();){std::set<Identity> rebound;
  for(const auto& source:it->targets){Require(original.count(source),"Physics group target is absent from original metadata");auto found=mapping.find(source);if(found!=mapping.end())rebound.insert(found->second.begin(),found->second.end());}
  it->targets=std::move(rebound);if(it->targets.empty())it=document.groups.erase(it);else ++it;
 }
}
inline std::filesystem::path AssetPath(const std::filesystem::path& root,const std::string& file) {
  D::Relative(file);auto base=std::filesystem::canonical(root),path=std::filesystem::canonical(base/std::filesystem::u8path(file));
  auto relative=path.lexically_relative(base);Require(!relative.empty()&&!relative.is_absolute(),"native author asset outside Mod");
  for(const auto& part:relative)Require(part!="..","native author link escapes Mod");return path;
}
inline EiemAuthorResourceFile::Layout AuthorLayout(const std::shared_ptr<R::File>& file,unsigned kind) {
  auto read=[file](size_t at,unsigned char* output,size_t size){file->ReadAt(at,output,size);};
  auto layout=EiemAuthorResourceFile::Parse(file->Size(),read);
  Require(layout.kind==kind,"native author resource kind differs");return layout;
}
inline MeshSnapshot ReadMeshMetadata(const std::filesystem::path& root,const D::MeshAsset& declaration,
                                     const std::string& partId) {
  auto file=std::make_shared<R::File>(AssetPath(root,declaration.file),0);
  Bytes metadata;
  auto layout=EiemAuthorResourceFile::Parse(file->Size(),
    [file](size_t at,unsigned char* output,size_t size){file->ReadAt(at,output,size);},&metadata);
  Require(layout.kind==1,"native author Mesh metadata kind differs");
  auto described=EiemAuthorResourceMetadata::DecodeMesh(metadata);
  Require(described.part==partId&&described.nativeBytes>0&&described.authorBytes>0&&
    AddSize(described.nativeBytes,described.authorBytes)==layout.payloadBytes,
    "native/editable author Mesh metadata extents differ");
  file->VerifyStable();
  return {std::move(file),layout,std::move(described),Sha(metadata.data(),metadata.size())};
}
inline Bytes AuthorMetadata(const std::shared_ptr<R::File>& file,const EiemAuthorResourceFile::Layout& layout) {
  return EiemAuthorResourceFile::ReadMetadata(layout,[file](size_t at,unsigned char* output,size_t size){file->ReadAt(at,output,size);});
}
inline std::shared_ptr<const EiemInputSpanView::View> OpenEffectInput(
    const std::filesystem::path& root,const EiemEffectNative::Bundle& declaration) {
  auto file=std::make_shared<R::File>(AssetPath(root,declaration.file),0,true);
  auto layout=AuthorLayout(file,5);
  Require(layout.payloadBytes==declaration.Size(),"native effect input extent differs");
  auto metadata=EiemAuthorResourceFile::ReadMetadata(layout,
    [file](size_t at,unsigned char* out,size_t n){file->ReadAt(at,out,n);});
  auto described=EiemAuthorResourceMetadata::DecodeEffect(metadata);
  Require(described.hash==declaration.hash&&described.bytes==declaration.Size(),
    "native effect resource metadata differs from controls declaration");
  Bytes header(48);
  Require(layout.payloadBytes>=header.size(),"native effect input header is truncated");
  file->ReadAt(layout.payloadOffset,header.data(),header.size());
  Require(EiemEffectNative::Archive(header.data(),layout.payloadBytes),"native effect input envelope differs");
  file->VerifyStable();
  return std::make_shared<const EiemInputSpanView::View>(
    std::vector<EiemInputSpanView::Span>{{0,layout.payloadOffset,layout.payloadBytes,{}}},
    std::vector<size_t>{file->Size()},
    [file](size_t,size_t at,unsigned char* out,size_t n){file->ReadAt(at,out,n);});
}
inline std::shared_ptr<const Mesh> OpenMesh(const std::filesystem::path& root,const D::MeshAsset& declaration,
                                            const MeshSnapshot* snapshot=nullptr) {
  auto file=snapshot?snapshot->file:std::make_shared<R::File>(AssetPath(root,declaration.file),0);
  file->Freeze();
  Bytes metadata;
  auto envelope=EiemAuthorResourceFile::Parse(file->Size(),
    [file](size_t at,unsigned char* out,size_t n){file->ReadAt(at,out,n);},&metadata);
  Require(envelope.kind==1,"native author Mesh metadata kind differs");
  Require(!snapshot||snapshot->metadataDigest==Sha(metadata.data(),metadata.size()),
    "control-dependent Mesh changed before F10; reload the Mod generation");
  auto described=snapshot?snapshot->declaration:
      EiemAuthorResourceMetadata::DecodeMesh(metadata);
  Require(described.part==declaration.part&&described.nativeBytes>0&&described.authorBytes>0&&
    AddSize(described.nativeBytes,described.authorBytes)==envelope.payloadBytes,
    "native/editable author Mesh payload extents differ");
  described.file=declaration.file;described.material=declaration.material;
  described.materialSlot=declaration.materialSlot;described.skeleton=declaration.skeleton;
  auto read=[file,base=envelope.payloadOffset,extent=described.nativeBytes](size_t at,unsigned char* out,size_t n){
    Require(at<=extent&&n<=extent-at,"native Mesh read exceeds its payload range");file->ReadAt(base+at,out,n);};
  R::Reader reader(described.nativeBytes,read);
  Require(reader.header.bytes==described.nativeBytes&&reader.directory.members.size()==1,"native author Mesh input extent/members differ");
  const auto& member=reader.directory.members[0];auto layout=reader.Layout(member);
  Require((member.flags&4)&&layout.externals.empty()&&layout.objects.size()==2,"native author Mesh is not an isolated local asset");
  auto names=reader.Assets(member,layout);Require(names.size()==1,"native author Mesh directory is ambiguous");const auto& asset=names.front();
  Require(asset.classId==43&&asset.cab==member.name&&asset.path==described.path&&asset.asset==described.marker,"native author Mesh declaration/identity differs");
  auto result=std::make_shared<Mesh>();result->declaration=std::move(described);result->object=G::Describe(reader,0,layout,asset.pathId,read);
  for(const auto& field:result->declaration.fields)Require(result->object.fields.count(field),"native author Mesh selected field absent: "+field);
  file->VerifyStable();result->metadataReadBytes=reader.stats.encodedBytes;return result;
}
inline std::shared_ptr<const Texture> OpenTexture(const std::filesystem::path& root,
    const D::TextureAsset& declaration,
    const EiemRuntimeAuthorManifest::TextureSettings& settings = {}) {
  if(declaration.file.empty()){
    auto result=std::make_shared<Texture>();result->declaration=declaration;result->ownerRoot=root;result->settings=settings;
    result->dds=std::make_shared<R::File>(AssetPath(root,declaration.ddsFile),0,true);
    Bytes header(std::min(result->dds->Size(),size_t(148)));result->dds->ReadAt(0,header.data(),header.size());
    result->replacement=EiemDdsTexture::Parse(header,result->dds->Size());result->metadataReadBytes=header.size();result->dds->VerifyStable();return result;
  }
  auto textureFile=std::make_shared<R::File>(AssetPath(root,declaration.file),0,true);
  auto envelope=AuthorLayout(textureFile,3);
  Require(!envelope.metadataBytes&&envelope.payloadBytes,
    "native author Texture must contain a Texture2D scaffold only");
  const auto payloadOffset=envelope.payloadOffset;
  R::ReadRange read=[textureFile,payloadOffset,payloadBytes=envelope.payloadBytes](size_t at,unsigned char* out,size_t n){
    Require(at<=payloadBytes&&n<=payloadBytes-at,"native author Texture read exceeds its payload");
    textureFile->ReadAt(payloadOffset+at,out,n);
  };
  R::Reader reader(envelope.payloadBytes,read);
  Require(reader.header.bytes==envelope.payloadBytes&&reader.directory.members.size()==1,
    "native author Texture scaffold extent/members differ");
  const auto& member=reader.directory.members.front();auto layout=reader.Layout(member);
  Require((member.flags&4)&&layout.externals.empty()&&layout.objects.size()==2,
    "native author Texture is not an isolated local asset");
  auto names=reader.Assets(member,layout);Require(names.size()==1,"native author Texture directory is ambiguous");
  const auto& asset=names.front();
  Require(asset.classId==28,"native author Texture asset is not Texture2D");

  auto dds=std::make_shared<R::File>(AssetPath(root,declaration.ddsFile),0,true);
  Bytes ddsHeader(std::min(dds->Size(),size_t(148)));dds->ReadAt(0,ddsHeader.data(),ddsHeader.size());
  auto ddsLayout=EiemDdsTexture::Parse(ddsHeader,dds->Size());
  Require(ddsLayout.bytes<=INT_MAX&&asset.cab==member.name,
    "replacement DDS/Texture2D scaffold identity differs");
  std::set<std::string> mutableFields{"m_Width","m_Height","m_MipCount","m_TextureFormat",
    "m_CompleteImageSize","m_ColorSpace","m_TextureSettings"};
  auto objectRanges=reader.Ranges(member,layout,asset.pathId);std::set<std::string> projected;
  for(const auto& field:mutableFields)if(objectRanges.count(field))projected.insert(field);
  for(const auto* required:{"m_Width","m_Height","m_MipCount","m_TextureFormat",
                            "m_CompleteImageSize","m_ColorSpace","m_TextureSettings"})
    Require(projected.count(required),std::string("native author Texture update field absent: ")+required);
  const auto& image=objectRanges.at("image data");Bytes imageLength(4);
  reader.ReadAt(image.offset,imageLength.data(),imageLength.size());
  Require(Le(imageLength,0,4)==0,"native author Texture scaffold image data is not empty");
  auto fields=reader.Fields(member,layout,asset.pathId,projected);
  auto result=std::make_shared<Texture>();result->declaration=declaration;result->cab=member.name;result->id=asset.pathId;result->name=asset.asset;
  const auto& textureType=layout.types.at(layout.objects.at(asset.pathId).type);
  SetInteger(textureType,fields.at("m_Width"),ddsLayout.width);
  SetInteger(textureType,fields.at("m_Height"),ddsLayout.height);
  SetInteger(textureType,fields.at("m_MipCount"),ddsLayout.mips);
  SetInteger(textureType,fields.at("m_TextureFormat"),ddsLayout.format);
  SetInteger(textureType,fields.at("m_CompleteImageSize"),ddsLayout.bytes);
  SetInteger(textureType,fields.at("m_ColorSpace"),settings.linear?0:1);
  auto& sampling=fields.at("m_TextureSettings");
  SetInteger(textureType,Child(textureType,sampling,"m_FilterMode"),settings.filter);
  SetInteger(textureType,Child(textureType,sampling,"m_Aniso"),settings.aniso);
  SetFloat(textureType,Child(textureType,sampling,"m_MipBias"),settings.mipBias);
  for(const auto* field:{"m_WrapU","m_WrapV","m_WrapW"})
    SetInteger(textureType,Child(textureType,sampling,field),settings.wrap);

  auto ddsView=std::make_shared<const V::View>(std::vector<V::Span>{{0,ddsLayout.offset,ddsLayout.bytes,{}}},
    std::vector<size_t>{dds->Size()},[dds](size_t source,size_t at,unsigned char* out,size_t n){
      Require(!source,"native author Texture DDS source index differs");dds->ReadAt(at,out,n);});
  const auto& object=layout.objects.at(asset.pathId);const auto& type=layout.types.at(object.type);
  auto objectBegin=AddSize(member.offset,object.offset);P::RangePayload payload;size_t output=0;
  for(auto node:type.nodes.at(0).children){const auto& name=type.nodes.at(node).name;const auto& field=objectRanges.at(name);
    if(G::HasAlignment(type,node))Require(((field.offset-objectBegin)&3)==(output&3),"replacement DDS changes a Texture field alignment");
    if(name=="image data"){
      Require(type.nodes.at(node).type=="TypelessData","native author Texture inline data schema differs");
      Bytes sizeBytes;AppendInteger(sizeBytes,ddsLayout.bytes,4);auto sizeLiteral=std::make_shared<const Bytes>(std::move(sizeBytes));
      payload.pieces.push_back({0,sizeLiteral->size(),sizeLiteral,{}});output=AddSize(output,sizeLiteral->size());
      payload.pieces.push_back({0,ddsLayout.bytes,{},ddsView});output=AddSize(output,ddsLayout.bytes);
      if(type.nodes.at(node).align){auto padding=(size_t(0)-output)&3;if(padding){auto pad=std::make_shared<const Bytes>(padding,0);payload.pieces.push_back({0,pad->size(),pad,{}});output=AddSize(output,pad->size());}}
    }else if(auto edit=fields.find(name);edit!=fields.end()){
      Bytes encoded(output&3);WriteValue(type,edit->second,encoded);encoded.erase(encoded.begin(),encoded.begin()+(output&3));
      auto bytes=std::make_shared<const Bytes>(std::move(encoded));payload.pieces.push_back({0,bytes->size(),bytes,{}});output=AddSize(output,bytes->size());
    }else{
      payload.pieces.push_back({field.offset-member.offset,field.size,{},{}});output=AddSize(output,field.size);
    }
  }
  if(type.nodes.at(0).align&&(output&3)){auto pad=std::make_shared<const Bytes>(4-(output&3),0);payload.pieces.push_back({0,pad->size(),pad,{}});output=AddSize(output,pad->size());}
  std::map<int64_t,P::RangePayload> edits;edits.emplace(asset.pathId,std::move(payload));
  auto plan=P::BuildRanges(reader,0,layout,edits);auto prepared=S::BuildRanges(reader,{plan.rewrite});
  result->input=prepared.Bind(read);result->replacement=ddsLayout;
  textureFile->VerifyStable();dds->VerifyStable();result->metadataReadBytes=reader.stats.encodedBytes;return result;
}
inline std::set<NativeReference> MaterialReferences(const Type& type,const Value& value,
    const SerializedLayout& layout,const std::string& cab,size_t depth=0) {
  Require(depth<type.nodes.size(),"native author Material reference schema cycle");std::set<NativeReference> result;
  const auto& node=type.nodes.at(value.node);
  if(node.type.rfind("PPtr<",0)==0){auto file=Integer(Child(type,value,"m_FileID"));auto id=int64_t(Integer(Child(type,value,"m_PathID")));
    Require(file<=layout.externals.size(),"native author Material external reference index differs");
    if(id){NativeReference ref{file?layout.externals[size_t(file)-1]:cab,id};Require(!ref.first.empty(),"native author Material dependency CAB absent");result.insert(std::move(ref));}return result;}
  for(const auto& child:value.children){auto nested=MaterialReferences(type,child,layout,cab,depth+1);result.insert(nested.begin(),nested.end());}return result;
}
inline std::shared_ptr<Material> OpenMaterial(const std::filesystem::path& root,const D::MaterialEntry& declaration,
    bool allowIniTextureRebind=false) {
  auto file=std::make_shared<R::File>(AssetPath(root,declaration.file),0);auto envelope=AuthorLayout(file,2);
  auto metadata=EiemAuthorResourceFile::ReadMetadata(envelope,[file](size_t at,unsigned char* out,size_t n){file->ReadAt(at,out,n);});
  auto described=EiemAuthorResourceMetadata::DecodeMaterial(metadata);
  Require(described.id==declaration.id&&described.source.path==declaration.source.path&&
    described.source.asset==declaration.source.asset&&envelope.payloadBytes==described.authorBytes&&
    described.authorBytes<=4*1024*1024,
    "author Material edit metadata differs from mod.ini");
  described.file=declaration.file;described.textures=declaration.textures;
  Bytes bytes(described.authorBytes);file->ReadAt(envelope.payloadOffset,bytes.data(),bytes.size());auto hash=Sha(bytes.data(),bytes.size());
  Require(Hex(hash.data(),hash.size())==described.hash,"author Material edit checksum differs");
  auto result=std::make_shared<Material>();result->ownerRoot=root;result->declaration=std::move(described);
  result->authored=EiemAuthorMaterialEdits::Parse(bytes);
  Require(result->authored.source==declaration.source.path,"author Material source path differs from mod.ini");
  if(allowIniTextureRebind)Require(result->authored.textures.empty(),
    "INI-owned Material texture bindings are duplicated in the author snapshot");
  else Require(result->authored.textures==declaration.textures,
    "author Material texture bindings differ from mod.ini");
  // The new writer leaves cross-resource bindings in INI so they can be
  // rebound without rewriting this snapshot. Old packages still validate the
  // former duplicated representation above.
  result->authored.textures.clear();
  for(const auto& binding:declaration.textures)result->authored.textures.emplace(binding.first,binding.second);
  file->VerifyStable();result->metadataReadBytes=bytes.size();return result;
}
class Catalog {
  std::filesystem::path root_;
  EiemRuntimeAuthorManifest::Manifest manifest_;
  D::Directory directory_;
  std::map<std::string,size_t> parts_;
  std::map<std::string,MeshSnapshot> meshSnapshots_;
  std::map<std::string,size_t> textureIds_;
  std::map<std::string,size_t> materialIds_;
  std::map<std::string,size_t> skeletonIds_;
  std::map<std::string,std::string> effectPaths_;
  std::mutex mutex_;
  std::map<std::string,std::shared_ptr<const Mesh>> meshes_;
  std::map<std::string,std::shared_ptr<const Texture>> textures_;
  std::map<std::string,std::shared_ptr<const Material>> materials_;
  std::map<std::string,std::shared_ptr<const EiemAuthorBoneAnimation::Skeleton>> skeletons_;
  std::map<std::string,std::shared_ptr<const Physics>> physics_;
public:
  const std::vector<EiemMeshTargetDirectory::Target>& Targets() const{return manifest_.targets;}
  struct CacheCounts {size_t meshes=0,textures=0,materials=0,physics=0;};
  explicit Catalog(const std::filesystem::path& root):root_(std::filesystem::canonical(root)),manifest_(EiemRuntimeAuthorManifest::Read(root_)) {
    directory_.mod=manifest_.id;
    std::map<std::string,D::MeshAsset> meshResources;std::set<std::string> effectFiles;
    std::set<std::string> declaredShapeAnimations,declaredMaterialAnimations,declaredEffectAttachments;
    for(const auto& row:manifest_.resources){const auto& declaration=row.second;
      if(declaration.kind==1){
        D::MeshAsset mesh;mesh.file=declaration.path;mesh.part=row.first;mesh.skeleton=declaration.skeleton;
        meshResources.emplace(row.first,std::move(mesh));
      }else if(declaration.kind==2){
        D::MaterialEntry material;material.id=row.first;material.file=declaration.path;
        material.source=declaration.source;material.textures=declaration.textureBindings;
        directory_.materials.push_back(std::move(material));
      }else if(declaration.kind==3){
        D::TextureAsset texture;texture.id=row.first;texture.file=declaration.path;texture.ddsFile=declaration.dds;texture.source=declaration.source;
        directory_.textures.push_back(std::move(texture));
      }else if(declaration.type=="skeleton"){
        Require(declaration.kind==8,"format 3 Skeleton kind differs");
        directory_.skeletons.push_back({declaration.skeleton,declaration.path});
      }else if(declaration.type=="controls"){
        Require(declaration.kind==6,"format 3 Controls kind differs");
        if(manifest_.inlineControls)continue;
        auto file=std::make_shared<R::File>(AssetPath(root_,declaration.path),0);auto layout=AuthorLayout(file,6);
        auto metadata=EiemAuthorResourceFile::ReadMetadata(layout,[file](size_t at,unsigned char* out,size_t n){file->ReadAt(at,out,n);});
        directory_.runtime=EiemAuthorResourceMetadata::DecodeControls(metadata);
      }else if(declaration.type=="effect"){
        Require(declaration.kind==5,"format 3 Effect kind differs");
        Require(effectFiles.insert(declaration.path).second,"format 3 Effect file is declared more than once");
        auto file=std::make_shared<R::File>(AssetPath(root_,declaration.path),0);auto layout=AuthorLayout(file,5);
        auto effect=EiemAuthorResourceMetadata::DecodeEffect(AuthorMetadata(file,layout));
        Require(layout.payloadBytes==effect.bytes,"format 3 Effect resource extent differs");
        Require(effectPaths_.emplace(effect.file,declaration.path).second,"format 3 Effect source file is declared more than once");
      }else if(declaration.type=="effect-author"){
        Require(declaration.kind==7,"format 3 Effect author kind differs");
      }else if(declaration.type=="animation-samples"){
        Require(declaration.kind==9,"format 3 Animation sample kind differs");
      }else if(declaration.type=="shape-animation"){
        Require(declaration.kind==6,"format 3 ShapeAnimation kind differs");
        declaredShapeAnimations.insert(row.first.substr(strlen("ShapeAnimation")));
      }else if(declaration.type=="material-animation"){
        Require(declaration.kind==6,"format 3 MaterialAnimation kind differs");
        declaredMaterialAnimations.insert(row.first.substr(strlen("MaterialAnimation")));
      }else if(declaration.type=="effect-attachment"){
        Require(declaration.kind==6,"format 3 EffectAttachment kind differs");
        declaredEffectAttachments.insert(row.first.substr(strlen("EffectAttachment")));
      }else if(declaration.type=="physics"){
        D::Auxiliary auxiliary;auxiliary.id=row.first;auxiliary.file=declaration.path;auxiliary.kind=5;
        for(const auto& target:manifest_.targets)
          if(std::find(declaration.targetRules.begin(),declaration.targetRules.end(),target.rule)!=declaration.targetRules.end())
            auxiliary.targets.insert(target.source);
        directory_.auxiliary.push_back(std::move(auxiliary));
      }else if(declaration.kind>=4){
        throw std::runtime_error("unsupported format 3 runtime resource type: "+declaration.id);
      }
    }
    if(manifest_.inlineControls){
      directory_.runtime=EiemAuthorControlsIni::Read(manifest_.sections,
        [&](const std::string& id){
          auto declaration=manifest_.resources.find(id);Require(declaration!=manifest_.resources.end()&&declaration->second.type=="animation-samples",
            "INI Animation sample resource is unresolved: "+id);
          auto file=std::make_shared<R::File>(AssetPath(root_,declaration->second.path),0,true);
          Require(file->Size()<=64*1024*1024,"Animation sample file exceeds 64 MiB");
          Bytes bytes(file->Size());file->ReadAt(0,bytes.data(),bytes.size());file->VerifyStable();return EiemAuthorControlsIni::DecodeSamples(bytes);
        },
        [&](const std::string& id){
          auto declaration=manifest_.resources.find(id);Require(declaration!=manifest_.resources.end()&&declaration->second.type=="effect",
            "INI Effect bundle resource is unresolved: "+id);
          auto file=std::make_shared<R::File>(AssetPath(root_,declaration->second.path),0);auto layout=AuthorLayout(file,5);
          auto effect=EiemAuthorResourceMetadata::DecodeEffect(AuthorMetadata(file,layout));
          Require(layout.payloadBytes==effect.bytes,"native Effect resource extent differs");return effect;
        });
    }
    for(auto& attachment:directory_.runtime.effects){
      auto native=std::make_shared<EiemEffectNative::Manifest>(*attachment.second.native);
      for(auto& bundle:native->bundles){auto moved=effectPaths_.find(bundle.file);if(moved!=effectPaths_.end())bundle.file=moved->second;}
      attachment.second.native=std::move(native);
    }
    std::set<std::string> expectedShapeAnimations,expectedEffectFiles;
    for(const auto& animation:directory_.runtime.animations)
      if(!directory_.runtime.materialAnimations.count(animation.first))expectedShapeAnimations.insert(animation.first);
    if(!manifest_.inlineControls){
      Require(declaredShapeAnimations==expectedShapeAnimations,"format 3 ShapeAnimation declarations differ from Controls resource");
      Require(declaredMaterialAnimations==directory_.runtime.materialAnimations,"format 3 MaterialAnimation declarations differ from Controls resource");
    }
    std::set<std::string> expectedEffectAttachments;
    for(const auto& effect:directory_.runtime.effects){
      expectedEffectAttachments.insert(effect.first);
      for(const auto& input:effect.second.native->bundles)expectedEffectFiles.insert(input.file);
    }
    if(!manifest_.inlineControls)Require(declaredEffectAttachments==expectedEffectAttachments,"format 3 EffectAttachment declarations differ from Controls resource");
    Require(effectFiles==expectedEffectFiles,"format 3 Effect resources differ from Controls resource bundle inputs");
    // Immutable controls need channel descriptors and bone-mounted effects
    // need the ordered owner palette before publication. Request-local native
    // compositions cannot fill these declarations later. Read only metadata
    // for those dependencies; Mesh geometry and native inputs stay lazy.
    std::map<std::string,std::string> controlMeshResources;
    for(const auto& state:directory_.runtime.parts)if(!state.second.shapes.empty()){
      auto part=manifest_.parts.find(state.first);
      Require(part!=manifest_.parts.end(),"shape-controlled Part is absent from the author manifest");
      controlMeshResources.emplace(part->second.mesh,state.first);
    }
    for(const auto& row:directory_.runtime.effects){const auto& effect=row.second;
      if(effect.part.empty())continue;
      auto part=manifest_.parts.find(effect.part);
      Require(part!=manifest_.parts.end(),"effect owner Part is absent from the author manifest");
      if(!effect.bone.empty())controlMeshResources.emplace(part->second.mesh,effect.part);
    }
    for(const auto& row:controlMeshResources){
      auto mesh=meshResources.find(row.first);
      Require(mesh!=meshResources.end(),"control-dependent Mesh resource is absent from the author manifest");
      auto snapshot=ReadMeshMetadata(root_,mesh->second,row.second);
      auto described=snapshot.declaration;
      mesh->second.vertices=described.vertices;
      mesh->second.indices=described.indices;
      mesh->second.bones=std::move(described.bones);
      mesh->second.shapes=std::move(described.shapes);
      Require(meshSnapshots_.emplace(mesh->second.file,std::move(snapshot)).second,
              "control-dependent Mesh resource file is duplicated");
    }
    for(const auto& state:directory_.runtime.parts)if(!state.second.shapes.empty()){
      const auto& part=manifest_.parts.at(state.first);
      const auto& channels=meshResources.at(part.mesh).shapes;
      for(const auto& binding:state.second.shapes)
        Require(std::find(channels.begin(),channels.end(),binding.first)!=channels.end(),
          "shape control channel is absent from its Mesh metadata: "+state.first+"/"+binding.first);
    }
    for(const auto& row:manifest_.parts){auto mesh=meshResources.find(row.second.mesh);
      Require(mesh!=meshResources.end(),"format 3 Part Mesh metadata is absent");auto part=mesh->second;
      part.part=row.first;part.material=row.second.material;part.materialSlot=row.second.materialSlot;directory_.meshes.push_back(std::move(part));}
    std::map<std::string,D::ReplaceRule> rules;
    for(const auto& target:manifest_.targets){auto& rule=rules[target.rule];rule.id=target.rule;rule.targets.emplace(target.lod,target.source);
      rule.parts=target.partIds;rule.skip=target.skip;}
    for(auto& row:rules)directory_.rules.push_back(std::move(row.second));
    for(size_t i=0;i<directory_.meshes.size();++i)parts_.emplace(directory_.meshes[i].part,i);
    for(size_t i=0;i<directory_.textures.size();++i)textureIds_.emplace(directory_.textures[i].id,i);
    for(size_t i=0;i<directory_.materials.size();++i)materialIds_.emplace(directory_.materials[i].id,i);
    for(size_t i=0;i<directory_.skeletons.size();++i)skeletonIds_.emplace(directory_.skeletons[i].id,i);
  }
  const D::Directory& Directory()const{return directory_;}
  const std::filesystem::path& Root()const{return root_;}
  std::map<std::string,std::string> FileFingerprints()const {
    std::map<std::string,std::string> result;
    auto add=[&](const std::string& relative){
      if(relative.empty())return;
      auto path=AssetPath(root_,relative);
      result.emplace(relative,EiemDemandGenerationDiff::FileFingerprint(path));
    };
    add("mod.ini");
    for(const auto& row:manifest_.resources){
      add(row.second.path);
      add(row.second.dds);
    }
    return result;
  }
  std::string ResourceDeclarations()const{
    std::string result="EFF_RESOURCE_DECLARATIONS/1\n";
    for(const auto& section:manifest_.sections){const auto& id=section.id;
      if(id=="Constants"||Begins(id,"Parameter")||Begins(id,"Action")||Begins(id,"Event")||Begins(id,"Binding")||Begins(id,"UI")||Begins(id,"Control")||Begins(id,"ShapeControl")||Begins(id,"Key")||Begins(id,"Animation")||Begins(id,"MaterialTrack"))continue;
      result+="["+id+"]\n";for(const auto& field:section.values){if(id=="Mod"&&(field.first=="key_switch_enabled"||field.first=="controls"))continue;
        if(Begins(id,"Part")&&(field.first=="visible"||Begins(field.first,"shape.")))continue;
        result+=field.first+"="+field.second+"\n";}
    }return result;
  }
  void BindEffects(Package& package)const {
    for(auto& row:package.effects){auto manifest=std::make_shared<EiemEffectNative::Manifest>(*row.second.native);
      for(auto& input:manifest->bundles){auto root=root_;auto declaration=input;
        input.open=[root,declaration](){return OpenEffectInput(root,declaration);};
      }row.second.native=std::move(manifest);
    }
  }
  std::shared_ptr<const Mesh> GetMesh(const std::string& part) {
    std::lock_guard<std::mutex> lock(mutex_);auto found=meshes_.find(part);if(found!=meshes_.end())return found->second;
    const auto& declaration=directory_.meshes.at(parts_.at(part));
    auto snapshot=meshSnapshots_.find(declaration.file);
    auto asset=OpenMesh(root_,declaration,snapshot==meshSnapshots_.end()?nullptr:&snapshot->second);
    meshes_.emplace(part,asset);return asset;
  }
  size_t OpenMeshes(){std::lock_guard<std::mutex> lock(mutex_);return meshes_.size();}
  std::shared_ptr<const EiemAuthorBoneAnimation::Skeleton> GetSkeleton(const std::string& id) {
    std::lock_guard<std::mutex> lock(mutex_);auto found=skeletons_.find(id);if(found!=skeletons_.end())return found->second;
    auto index=skeletonIds_.find(id);Require(index!=skeletonIds_.end(),"author Skeleton declaration is absent: "+id);
    const auto& declaration=directory_.skeletons.at(index->second);
    auto file=std::make_shared<R::File>(AssetPath(root_,declaration.file),0);auto layout=AuthorLayout(file,8);
    auto metadata=EiemAuthorResourceMetadata::DecodeSkeleton(AuthorMetadata(file,layout));
    Require(metadata.id==id&&metadata.file==declaration.file&&metadata.bytes==layout.payloadBytes,
      "author Skeleton metadata differs from mod.ini");
    Bytes payload(metadata.bytes);file->ReadAt(layout.payloadOffset,payload.data(),payload.size());
    auto digest=Sha(payload.data(),payload.size());Require(Hex(digest.data(),digest.size())==metadata.hash,
      "author Skeleton payload checksum differs");
    auto value=std::make_shared<const EiemAuthorBoneAnimation::Skeleton>(
      EiemAuthorBoneAnimation::DecodeSkeleton(payload.data(),payload.size()));
    for(const auto& animation:directory_.runtime.animations)for(const auto& track:animation.second.boneTracks)
      if(track.skeleton==id)Require(value->paths.count(track.path),
        "author Animation bone path is absent from Skeleton: "+animation.first+"/"+track.path);
    file->VerifyStable();skeletons_.emplace(id,value);return value;
  }
  size_t OpenSkeletons(){std::lock_guard<std::mutex> lock(mutex_);return skeletons_.size();}
private:
  std::shared_ptr<const Texture> GetTextureLocked(const std::string& id,const TextureResolver& resolve={}) {
    auto index=textureIds_.find(id);Require(index!=textureIds_.end(),"author Texture declaration is absent: "+id);
    const auto& entry=directory_.textures.at(index->second);const auto key=entry.file.empty()?id:entry.file;
    auto found=textures_.find(key);std::shared_ptr<const Texture> asset;
    if(found!=textures_.end())asset=found->second;
    else{auto opened=OpenTexture(root_,entry,manifest_.resources.at(id).textureSettings);
      auto described=std::make_shared<Texture>(*opened);described->source=manifest_.resources.at(id).source;asset=described;}
    if(!asset->input&&resolve)asset=resolve(*asset);
    textures_[key]=asset;return asset;
  }
public:
  std::shared_ptr<const Texture> GetTexture(const std::string& id) {
    std::lock_guard<std::mutex> lock(mutex_);return GetTextureLocked(id);
  }
  size_t OpenTextures(){std::lock_guard<std::mutex> lock(mutex_);return textures_.size();}
  std::shared_ptr<const Material> GetMaterial(const std::string& id,const TextureResolver& resolve={}) {
    std::lock_guard<std::mutex> lock(mutex_);auto found=materials_.find(id);
    if(found!=materials_.end()&&(!resolve||found->second->textures.size()==found->second->textureAssets.size()))return found->second;
    auto asset=OpenMaterial(root_,directory_.materials.at(materialIds_.at(id)),manifest_.inlineControls);
    for(const auto& binding:asset->declaration.textures){
      auto textureAsset=GetTextureLocked(binding.second,resolve);
      if(textureAsset->input)asset->textures.emplace(binding.first,NativeReference{textureAsset->cab,textureAsset->id});
      asset->textureAssets.emplace(binding.first,std::move(textureAsset));}
    materials_[id]=asset;return asset;
  }
  size_t OpenMaterials(){std::lock_guard<std::mutex> lock(mutex_);return materials_.size();}
  std::shared_ptr<const Texture> FindTexture(const NativeReference& reference) {
    std::lock_guard<std::mutex> lock(mutex_);
    for(const auto& row:textures_)if(row.second->cab==reference.first&&row.second->id==reference.second)return row.second;
    return {};
  }
  std::vector<std::shared_ptr<const Physics>> GetPhysics(const std::set<Identity>& targets) {
    std::vector<std::shared_ptr<const Physics>> result;std::lock_guard<std::mutex> lock(mutex_);
    for(const auto& row:manifest_.resources){const auto& declaration=row.second;if(declaration.type!="physics")continue;
      std::set<Identity> declaredTargets;bool relevant=false;
      for(const auto& target:manifest_.targets)if(std::find(declaration.targetRules.begin(),declaration.targetRules.end(),target.rule)!=declaration.targetRules.end()){
        declaredTargets.insert(target.source);relevant|=targets.count(target.source)!=0;}
      if(!relevant)continue;
      auto file=std::make_shared<R::File>(AssetPath(root_,declaration.path),0);auto layout=AuthorLayout(file,4);
      auto metadata=AuthorMetadata(file,layout);auto entry=EiemAuthorResourceMetadata::DecodeAuxiliary(metadata,4);
      D::Relative(entry.file);Require(entry.id==row.first&&!entry.file.empty()&&!entry.targets.empty()&&
        entry.bytes<=16*1024*1024&&entry.authorBytes>0&&AddSize(entry.bytes,entry.authorBytes)==layout.payloadBytes,
        "native/editable Physics resource declaration differs from mod.ini");
      auto found=physics_.find(entry.id);if(found==physics_.end()){
        auto bytes=std::make_shared<Bytes>(entry.bytes);file->ReadAt(layout.payloadOffset,bytes->data(),bytes->size());file->VerifyStable();auto hash=Sha(bytes->data(),bytes->size());Require(Hex(hash.data(),hash.size())==entry.hash,"native Physics content checksum differs");
        auto asset=std::make_shared<Physics>();asset->document=EiemPhysicsInput::DecodePayload(bytes,entry.file);
        std::set<Identity> actual;for(const auto& group:asset->document.groups)actual.insert(group.targets.begin(),group.targets.end());
        Require(actual.size()==entry.targets.size()&&std::equal(actual.begin(),actual.end(),entry.targets.begin(),[](const auto& a,const auto& b){return !(a<b)&&!(b<a);}),"native Physics target declaration differs");
        RebindPhysics(asset->document,actual,declaration,manifest_.targets);
        found=physics_.emplace(entry.id,std::move(asset)).first;
      }result.push_back(found->second);
    }return result;
  }
  size_t OpenPhysics(){std::lock_guard<std::mutex> lock(mutex_);return physics_.size();}
  CacheCounts ClearAssetCaches(){std::lock_guard<std::mutex> lock(mutex_);CacheCounts counts{meshes_.size(),textures_.size(),materials_.size(),physics_.size()};
    meshes_.clear();textures_.clear();materials_.clear();physics_.clear();return counts;}
};
} // namespace EiemDemandAuthorResources

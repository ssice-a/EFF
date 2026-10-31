#pragma once
#include "eiem_runtime_author_directory.h"
#include "eiem_runtime_author_manifest.h"
#include "eiem_author_material_edits.h"
#include "eiem_author_resource_file.h"
#include "eiem_author_resource_metadata.h"
#include "eiem_native_object_graft.h"
#include "eiem_native_standalone_input.h"
#include "eiem_physics_input_builder.h"

// Immutable authored inputs. Opening a catalog reads its small declaration;
// native assets are opened on the first actual demand and held by that
// generation's input views. No source game snapshot or consumer list is used.
namespace EiemDemandAuthorResources {
using namespace EiemNativeInput;
namespace D=EiemRuntimeAuthorDirectory;namespace R=EiemNativeRandomAccess;
namespace G=EiemNativeObjectGraft;
struct Mesh {
  D::MeshAsset declaration;
  G::AuthorObject object;
  size_t metadataReadBytes=0;
};
struct Texture {
  D::TextureAsset declaration;
  G::AuthorObject object;
  std::shared_ptr<const EiemInputSpanView::View> input;
  std::string cab,name;
  int64_t id=0;
  size_t metadataReadBytes=0;
};
struct Material {
  std::filesystem::path ownerRoot;
  D::MaterialEntry declaration;
  EiemAuthorMaterialEdits::Author authored;
  std::map<std::string,NativeReference> textures;
  std::map<std::string,std::shared_ptr<const Texture>> textureAssets;
  size_t metadataReadBytes=0;
};
struct Physics {
  std::shared_ptr<const Resource> backing;
  EiemPhysicsInput::Document document;
};
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
inline Bytes AuthorMetadata(const std::shared_ptr<R::File>& file,const EiemAuthorResourceFile::Layout& layout) {
  return EiemAuthorResourceFile::ReadMetadata(layout,[file](size_t at,unsigned char* output,size_t size){file->ReadAt(at,output,size);});
}
inline std::shared_ptr<const EiemInputSpanView::View> OpenEffectInput(
    const std::filesystem::path& root,const EiemEffectNative::Bundle& declaration) {
  auto file=std::make_shared<R::File>(AssetPath(root,declaration.file),0);
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
inline std::shared_ptr<const Mesh> OpenMesh(const std::filesystem::path& root,const D::MeshAsset& declaration) {
  auto file=std::make_shared<R::File>(AssetPath(root,declaration.file),0);auto envelope=AuthorLayout(file,1);
  auto described=EiemAuthorResourceMetadata::DecodeMesh(AuthorMetadata(file,envelope));
  Require(described.part==declaration.part&&described.nativeBytes>0&&described.authorBytes>0&&
    AddSize(described.nativeBytes,described.authorBytes)==envelope.payloadBytes,
    "native/editable author Mesh payload extents differ");
  described.file=declaration.file;described.material=declaration.material;
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
inline std::shared_ptr<const Texture> OpenTexture(const std::filesystem::path& root,const D::TextureAsset& declaration) {
  auto metadata=std::make_shared<R::File>(AssetPath(root,declaration.file),0);
  auto envelope=AuthorLayout(metadata,3);auto described=EiemAuthorResourceMetadata::DecodeTexture(AuthorMetadata(metadata,envelope));
  Require(described.id==declaration.id&&described.width&&described.height&&described.mips,
    "native author Texture identity/layout differs");
  described.file=declaration.file;described.ddsFile=declaration.ddsFile;
  const auto metadataBytes=envelope.payloadBytes;
  auto dds=std::make_shared<R::File>(AssetPath(root,described.ddsFile),0);
  Bytes ddsHeader(std::min(dds->Size(),size_t(148)));dds->ReadAt(0,ddsHeader.data(),ddsHeader.size());
  auto ddsLayout=EiemDdsTexture::Parse(ddsHeader,dds->Size());
  EiemDdsTexture::Matches(ddsLayout,described.width,described.height,described.mips,described.format,false);
  Require(ddsLayout.bytes==described.pixelBytes&&described.pixelOffset<=metadataBytes,"replacement DDS payload differs from native texture extent");
  auto fullSize=AddSize(metadataBytes,described.pixelBytes);
  Require(fullSize<=INT64_MAX,"native author Texture input exceeds stream extent");
  auto read=[metadata,base=envelope.payloadOffset,pixelOffset=described.pixelOffset,pixelBytes=described.pixelBytes,fullSize](size_t at,unsigned char* out,size_t n){
    Require(at<=fullSize&&n<=fullSize-at,"native author Texture metadata read exceeds extent");
    while(n){
      if(at<pixelOffset){auto take=std::min(n,pixelOffset-at);metadata->ReadAt(base+at,out,take);at+=take;out+=take;n-=take;continue;}
      auto pixelEnd=AddSize(pixelOffset,pixelBytes);
      if(at<pixelEnd){auto take=std::min(n,pixelEnd-at);memset(out,0,take);at+=take;out+=take;n-=take;continue;}
      auto source=at-pixelBytes;metadata->ReadAt(base+source,out,n);return;
    }
  };
  R::Reader reader(fullSize,read);
  Require(reader.header.bytes==fullSize&&reader.directory.members.size()==1,"native author Texture extent/members differ");
  const auto& member=reader.directory.members[0];auto layout=reader.Layout(member);
  Require((member.flags&4)&&layout.externals.empty()&&layout.objects.size()==2,"native author Texture is not an isolated local asset");
  auto names=reader.Assets(member,layout);Require(names.size()==1,"native author Texture directory is ambiguous");const auto& asset=names.front();
  Require(asset.classId==28&&asset.cab==member.name&&asset.path==described.path,"native author Texture declaration/identity differs");
  auto fields=reader.Fields(member,layout,asset.pathId,{"m_Width","m_Height","m_MipCount","m_TextureFormat","m_ColorSpace"});
  Require(Integer(fields.at("m_Width"))==described.width&&Integer(fields.at("m_Height"))==described.height&&Integer(fields.at("m_MipCount"))==described.mips&&Integer(fields.at("m_TextureFormat"))==described.format,"native author Texture dimensions/format differ");
  auto result=std::make_shared<Texture>();result->declaration=std::move(described);result->cab=member.name;result->id=asset.pathId;result->name=asset.asset;
  Require(result->cab==result->declaration.cab&&result->id==result->declaration.pathId,"native author Texture CAB/PathID differs");
  std::vector<EiemInputSpanView::Span> spans;
  spans.push_back({0,0,result->declaration.pixelOffset,{}});
  spans.push_back({1,ddsLayout.offset,ddsLayout.bytes,{}});
  // The metadata file has already had the pixels removed. Its suffix starts
  // at pixelOffset, not pixelOffset + pixelBytes in the reconstructed input.
  if(result->declaration.pixelOffset<metadataBytes)spans.push_back({0,AddSize(envelope.payloadOffset,result->declaration.pixelOffset),metadataBytes-result->declaration.pixelOffset,{}});
  spans.front().offset=envelope.payloadOffset;
  result->input=std::make_shared<const EiemInputSpanView::View>(std::move(spans),std::vector<size_t>{metadata->Size(),dds->Size()},
    [metadata,dds](size_t source,size_t at,unsigned char* out,size_t n){if(source==0)metadata->ReadAt(at,out,n);else{Require(source==1,"native author Texture source index differs");dds->ReadAt(at,out,n);}});
  result->object=G::Describe(reader,0,layout,asset.pathId,read);metadata->VerifyStable();dds->VerifyStable();result->metadataReadBytes=reader.stats.encodedBytes;return result;
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
inline std::shared_ptr<Material> OpenMaterial(const std::filesystem::path& root,const D::MaterialEntry& declaration) {
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
  std::map<std::string,std::string> bindings;for(const auto& row:result->authored.textures)bindings.emplace(row.first,row.second);
  Require(bindings==declaration.textures,"author Material texture bindings differ from mod.ini");
  file->VerifyStable();result->metadataReadBytes=bytes.size();return result;
}
class Catalog {
  std::filesystem::path root_;
  EiemRuntimeAuthorManifest::Manifest manifest_;
  D::Directory directory_;
  std::map<std::string,size_t> parts_;
  std::map<std::string,size_t> textureIds_;
  std::map<std::string,size_t> materialIds_;
  std::map<std::string,std::string> effectPaths_;
  std::mutex mutex_;
  std::map<std::string,std::shared_ptr<const Mesh>> meshes_;
  std::map<std::string,std::shared_ptr<const Texture>> textures_;
  std::map<std::string,std::shared_ptr<const Material>> materials_;
  std::map<std::string,std::shared_ptr<const Physics>> physics_;
public:
  struct CacheCounts {size_t meshes=0,textures=0,materials=0,physics=0;};
  explicit Catalog(const std::filesystem::path& root):root_(std::filesystem::canonical(root)),manifest_(EiemRuntimeAuthorManifest::Read(root_)) {
    directory_.mod=manifest_.id;
    std::map<std::string,D::MeshAsset> meshResources;std::set<std::string> effectFiles;
    std::set<std::string> declaredShapeAnimations,declaredMaterialAnimations,declaredEffectAttachments;
    for(const auto& row:manifest_.resources){const auto& declaration=row.second;
      if(declaration.kind==1){
        D::MeshAsset mesh;mesh.file=declaration.path;mesh.part=row.first;
        meshResources.emplace(row.first,std::move(mesh));
      }else if(declaration.kind==2){
        D::MaterialEntry material;material.id=row.first;material.file=declaration.path;
        material.source=declaration.source;material.textures=declaration.textureBindings;
        directory_.materials.push_back(std::move(material));
      }else if(declaration.kind==3){
        D::TextureAsset texture;texture.id=row.first;texture.file=declaration.path;texture.ddsFile=declaration.dds;
        directory_.textures.push_back(std::move(texture));
      }else if(declaration.type=="controls"){
        Require(declaration.kind==6,"format 3 Controls kind differs");
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
    for(auto& attachment:directory_.runtime.effects){
      auto native=std::make_shared<EiemEffectNative::Manifest>(*attachment.second.native);
      for(auto& bundle:native->bundles){auto moved=effectPaths_.find(bundle.file);if(moved!=effectPaths_.end())bundle.file=moved->second;}
      attachment.second.native=std::move(native);
    }
    std::set<std::string> expectedShapeAnimations,expectedEffectFiles;
    for(const auto& animation:directory_.runtime.animations)
      if(!directory_.runtime.materialAnimations.count(animation.first))expectedShapeAnimations.insert(animation.first);
    Require(declaredShapeAnimations==expectedShapeAnimations,"format 3 ShapeAnimation declarations differ from Controls resource");
    Require(declaredMaterialAnimations==directory_.runtime.materialAnimations,"format 3 MaterialAnimation declarations differ from Controls resource");
    std::set<std::string> expectedEffectAttachments;
    for(const auto& effect:directory_.runtime.effects){
      expectedEffectAttachments.insert(effect.first);
      for(const auto& input:effect.second.native->bundles)expectedEffectFiles.insert(input.file);
    }
    Require(declaredEffectAttachments==expectedEffectAttachments,"format 3 EffectAttachment declarations differ from Controls resource");
    Require(effectFiles==expectedEffectFiles,"format 3 Effect resources differ from Controls resource bundle inputs");
    for(const auto& row:manifest_.parts){auto mesh=meshResources.find(row.second.mesh);
      Require(mesh!=meshResources.end(),"format 3 Part Mesh metadata is absent");auto part=mesh->second;
      part.part=row.first;part.material=row.second.material;directory_.meshes.push_back(std::move(part));}
    std::map<std::string,D::ReplaceRule> rules;
    for(const auto& target:manifest_.targets){auto& rule=rules[target.rule];rule.id=target.rule;rule.targets.emplace(target.lod,target.source);
      rule.parts=target.partIds;rule.skip=target.skip;}
    for(auto& row:rules)directory_.rules.push_back(std::move(row.second));
    for(size_t i=0;i<directory_.meshes.size();++i)parts_.emplace(directory_.meshes[i].part,i);
    for(size_t i=0;i<directory_.textures.size();++i)textureIds_.emplace(directory_.textures[i].id,i);
    for(size_t i=0;i<directory_.materials.size();++i)materialIds_.emplace(directory_.materials[i].id,i);
  }
  const D::Directory& Directory()const{return directory_;}
  const std::filesystem::path& Root()const{return root_;}
  void BindEffects(Package& package)const {
    for(auto& row:package.effects){auto manifest=std::make_shared<EiemEffectNative::Manifest>(*row.second.native);
      for(auto& input:manifest->bundles){auto root=root_;auto declaration=input;
        input.open=[root,declaration](){return OpenEffectInput(root,declaration);};
      }row.second.native=std::move(manifest);
    }
  }
  std::shared_ptr<const Mesh> GetMesh(const std::string& part) {
    std::lock_guard<std::mutex> lock(mutex_);auto found=meshes_.find(part);if(found!=meshes_.end())return found->second;
    auto asset=OpenMesh(root_,directory_.meshes.at(parts_.at(part)));meshes_.emplace(part,asset);return asset;
  }
  size_t OpenMeshes(){std::lock_guard<std::mutex> lock(mutex_);return meshes_.size();}
private:
  std::shared_ptr<const Texture> GetTextureLocked(const std::string& id) {
    auto index=textureIds_.find(id);Require(index!=textureIds_.end(),"author Texture declaration is absent: "+id);
    const auto& entry=directory_.textures.at(index->second);auto found=textures_.find(entry.file);if(found!=textures_.end())return found->second;
    auto asset=OpenTexture(root_,entry);textures_.emplace(entry.file,asset);return asset;
  }
public:
  std::shared_ptr<const Texture> GetTexture(const std::string& id) {
    std::lock_guard<std::mutex> lock(mutex_);return GetTextureLocked(id);
  }
  size_t OpenTextures(){std::lock_guard<std::mutex> lock(mutex_);return textures_.size();}
  std::shared_ptr<const Material> GetMaterial(const std::string& id) {
    std::lock_guard<std::mutex> lock(mutex_);auto found=materials_.find(id);if(found!=materials_.end())return found->second;
    auto asset=OpenMaterial(root_,directory_.materials.at(materialIds_.at(id)));
    for(const auto& binding:asset->declaration.textures){
      auto textureAsset=GetTextureLocked(binding.second);
      asset->textures.emplace(binding.first,NativeReference{textureAsset->cab,textureAsset->id});
      asset->textureAssets.emplace(binding.first,std::move(textureAsset));}
    materials_.emplace(id,asset);return asset;
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
      Require(entry.id==row.first&&entry.file==declaration.path&&entry.targets.size()==declaredTargets.size()&&
        std::equal(entry.targets.begin(),entry.targets.end(),declaredTargets.begin(),[](const auto& a,const auto& b){return !(a<b)&&!(b<a);})&&
        entry.bytes<=16*1024*1024&&entry.authorBytes>0&&AddSize(entry.bytes,entry.authorBytes)==layout.payloadBytes,
        "native/editable Physics resource declaration differs from mod.ini");
      auto found=physics_.find(entry.id);if(found==physics_.end()){
        Bytes bytes(entry.bytes);file->ReadAt(layout.payloadOffset,bytes.data(),bytes.size());file->VerifyStable();auto hash=Sha(bytes.data(),bytes.size());Require(Hex(hash.data(),hash.size())==entry.hash,"native Physics content checksum differs");
        auto asset=std::make_shared<Physics>();asset->backing=std::make_shared<const Resource>(EiemDirectory::Decode(std::move(bytes),5));asset->document=EiemPhysicsInput::Decode(*asset->backing);
        std::set<Identity> actual;for(const auto& group:asset->document.groups)actual.insert(group.targets.begin(),group.targets.end());
        Require(actual.size()==entry.targets.size()&&std::equal(actual.begin(),actual.end(),entry.targets.begin(),[](const auto& a,const auto& b){return !(a<b)&&!(b<a);}),"native Physics target declaration differs");
        found=physics_.emplace(entry.id,std::move(asset)).first;
      }result.push_back(found->second);
    }return result;
  }
  size_t OpenPhysics(){std::lock_guard<std::mutex> lock(mutex_);return physics_.size();}
  CacheCounts ClearAssetCaches(){std::lock_guard<std::mutex> lock(mutex_);CacheCounts counts{meshes_.size(),textures_.size(),materials_.size(),physics_.size()};
    meshes_.clear();textures_.clear();materials_.clear();physics_.clear();return counts;}
};
} // namespace EiemDemandAuthorResources

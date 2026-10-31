#pragma once
#include "eiem_mesh_input_builder.h"
#include "eiem_texture_input_builder.h"
#include "eiem_mesh_binding_metadata.h"
#include <cfloat>
#include <functional>
#include <chrono>
#include <mutex>

namespace EiemNativeInput {
inline bool IsTransformClass(int kind) { return kind == 4 || kind == 224; }
using MeshSchemaIndex = std::map<std::array<unsigned char, 16>, Type>;
using MeshBoneHashIndex = std::map<NativeReference,std::vector<uint32_t>>;
inline std::vector<uint32_t> BoneHashes(const Type& type,const Value& field){
  const auto& values=ArrayValue(type,field);std::vector<uint32_t> hashes;
  if(!values.raw.empty()){
    Require(values.raw.size()==values.count*4,"Mesh bone hash byte count differs");
    for(size_t i=0;i<values.count;++i)hashes.push_back(uint32_t(Le(values.raw,i*4,4)));
  }else for(const auto& value:values.children)hashes.push_back(uint32_t(Integer(value)));
  Require(hashes.size()==values.count,"Mesh bone hash array count differs");return hashes;
}
inline std::map<size_t,int64_t> OriginalBoneBinding(const std::vector<uint32_t>& sourceHashes,
    const std::vector<int64_t>& nativeBones,const std::vector<uint32_t>& authorHashes) {
  // Some native PFBs retain a shorter or different palette than their Mesh.
  // It need not be a prefix: do not infer any slot binding from this pair.
  // ResolveBones still requires a unique binding from the concrete hierarchy.
  if(sourceHashes.size()!=nativeBones.size())return {};
  std::map<uint32_t,size_t> slots;
  for(size_t i=0;i<sourceHashes.size();++i){auto found=slots.find(sourceHashes[i]);
    if(found==slots.end())slots.emplace(sourceHashes[i],i);else found->second=SIZE_MAX;}
  std::map<size_t,int64_t> result;
  for(size_t i=0;i<authorHashes.size();++i){auto found=slots.find(authorHashes[i]);if(found==slots.end())continue;
    Require(found->second!=SIZE_MAX,"source Mesh bone hash is ambiguous");result.emplace(i,nativeBones.at(found->second));}
  return result;
}
inline NativeReference NullableReference(const Type& type, const Value& value,
                                          const SerializedLayout& layout,
                                          const std::string& cab) {
  auto file = Integer(Child(type, value, "m_FileID"));
  Require(file <= layout.externals.size(), "serialized dependency file index");
  return {file ? layout.externals[(size_t)file - 1] : cab,
          (int64_t)Integer(Child(type, value, "m_PathID"))};
}
inline NativeReference CanonicalSerializedAssetReference(NativeReference reference) {
  reference.first = SerializedAssetCabKey(std::move(reference.first));
  return reference;
}
struct MeshConsumer {int64_t renderer=0,filter=0;NativeReference mesh;};
inline std::vector<NativeReference> RendererMaterials(const Type& type,const Value& renderer,
    const SerializedLayout& layout,const std::string& cab) {
  std::vector<NativeReference> result;const auto& materials=ConstArray(type,Child(type,renderer,"m_Materials"));
  for(const auto& entry:materials.children){auto material=NullableReference(type,entry,layout,cab);if(material.second)result.push_back(std::move(material));}
  return result;
}
// Static Mesh references belong to MeshFilter. Pair only the components that
// the owning GameObject actually references; names and scene state play no role.
template<class Project> inline std::vector<MeshConsumer> ProjectedMeshConsumers(const SerializedLayout& layout,
    const std::string& cab,const std::set<NativeReference>& selected,const Project& project,
    std::string (*canonical)(std::string)=nullptr) {
  std::set<NativeReference> lookup=selected;
  if(canonical){lookup.clear();for(auto reference:selected){reference.first=canonical(std::move(reference.first));lookup.insert(std::move(reference));}}
  std::vector<MeshConsumer> result;
  for(auto id:layout.order){const auto& type=layout.types.at(layout.objects.at(id).type);
    if(type.classId!=137&&type.classId!=33)continue;
    auto value=project(id,{"m_Mesh","m_GameObject"});auto mesh=NullableReference(type,Child(type,value,"m_Mesh"),layout,cab);
    auto key=mesh;if(canonical)key.first=canonical(key.first);
    if(!mesh.second||!lookup.count(key))continue;
    if(type.classId==137){result.push_back({id,0,mesh});continue;}
    auto owner=NullableReference(type,Child(type,value,"m_GameObject"),layout,cab);
    Require(owner.first==cab&&owner.second&&layout.objects.count(owner.second),"static MeshFilter has no local GameObject");
    const auto& gt=layout.types.at(layout.objects.at(owner.second).type);Require(gt.classId==1,"static MeshFilter owner is not a GameObject");
    auto go=project(owner.second,{"m_Component"});int64_t renderer=0;size_t filters=0,renderers=0;
    for(const auto& entry:ConstArray(gt,Child(gt,go,"m_Component")).children){auto ref=NullableReference(gt,Child(gt,entry,"component"),layout,cab);
      // Unrelated missing/script references are preserved. Only the selected
      // visual pair needs locally resolvable components.
      if(ref.first!=cab||!ref.second||!layout.objects.count(ref.second))continue;
      const auto& ct=layout.types.at(layout.objects.at(ref.second).type);
      if(ct.classId!=23&&ct.classId!=33)continue;
      auto component=project(ref.second,{"m_GameObject"});
      Require(NullableReference(ct,Child(ct,component,"m_GameObject"),layout,cab)==owner,"static component/GameObject ownership differs");
      if(ct.classId==33){++filters;Require(ref.second==id,"static GameObject has another MeshFilter");}
      else {++renderers;renderer=ref.second;}
    }
    Require(filters==1,"static MeshFilter owner component reference differs");
    if(!renderers)continue; // A MeshFilter alone is not a visual consumer.
    Require(renderers==1,"static MeshFilter has multiple referenced MeshRenderers");
    result.push_back({renderer,id,mesh});
  }return result;
}
inline std::vector<MeshConsumer> MeshConsumers(const Bytes& bytes,const SerializedLayout& layout,
    const std::string& cab,const std::set<NativeReference>& selected,std::string (*canonical)(std::string)=nullptr) {
  return ProjectedMeshConsumers(layout,cab,selected,[&](int64_t id,const std::set<std::string>& fields){
    return ObjectFields(bytes,layout,id,fields);
  },canonical);
}
inline double FloatValue(const Type& type, const Value& value) {
  Require(type.nodes.at(value.node).type == "float" && value.raw.size() == 4,
          "serialized float schema");
  float number;
  memcpy(&number, value.raw.data(), 4);
  Require(std::isfinite(number), "nonfinite serialized geometry float");
  return number;
}
using Vector3 = std::array<double, 3>;
using AffineMatrix = std::array<double,16>;
inline AffineMatrix AffineIdentity(){return {1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};}
inline std::string BoundsOriginName(size_t rootSlot,const AffineMatrix& boneToRoot,const std::string& resource){
  Require(!resource.empty(),"Renderer bounds author resource absent");
  static_assert(sizeof(AffineMatrix)==128,"Renderer bounds context encoding differs");
  const auto* bytes=reinterpret_cast<const unsigned char*>(boneToRoot.data());
  std::string name="@bounds:"+std::to_string(rootSlot)+":";constexpr char digits[]="0123456789abcdef";
  for(size_t i=0;i<sizeof(AffineMatrix);++i){name.push_back(digits[bytes[i]>>4]);name.push_back(digits[bytes[i]&15]);}
  return name+":"+resource;
}
inline bool DecodeBoundsOrigin(const std::string& name,size_t& rootSlot,AffineMatrix& boneToRoot,std::string& resource){
  constexpr size_t prefix=8;if(name.compare(0,prefix,"@bounds:"))return false;
  auto separator=name.find(':',prefix);if(separator==std::string::npos||separator==prefix)return false;
  size_t slot=0;for(size_t i=prefix;i<separator;++i){auto c=name[i];if(c<'0'||c>'9'||slot>(SIZE_MAX-size_t(c-'0'))/10)return false;slot=slot*10+size_t(c-'0');}
  auto begin=separator+1;if(name.size()-begin<=257||name[begin+256]!=':')return false;
  auto hex=[](char c)->int{return c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:-1;};
  AffineMatrix matrix{};auto* bytes=reinterpret_cast<unsigned char*>(matrix.data());
  for(size_t i=0;i<sizeof(AffineMatrix);++i){auto hi=hex(name[begin+i*2]),lo=hex(name[begin+i*2+1]);if(hi<0||lo<0)return false;bytes[i]=static_cast<unsigned char>((hi<<4)|lo);}
  for(auto value:matrix)if(!std::isfinite(value))return false;
  rootSlot=slot;boneToRoot=matrix;resource=name.substr(begin+257);return true;
}
inline void BindBoundsOrigins(const Type& type,Value& aabb,const std::string& resource){
  Bytes bytes;bytes.reserve(24);for(const char* field:{"m_Center","m_Extent"})for(char axis:std::string("xyz")){
    const auto& value=Child(type,Child(type,aabb,field),std::string(1,axis));Require(value.raw.size()==4,"Renderer bounds float width differs");bytes.insert(bytes.end(),value.raw.begin(),value.raw.end());
  }
  auto owned=std::make_shared<const Bytes>(std::move(bytes));size_t offset=0;
  for(const char* field:{"m_Center","m_Extent"})for(char axis:std::string("xyz")){
    auto& value=Child(type,Child(type,aabb,field),std::string(1,axis));value.rawOrigins=EiemAuthorBytes::MakeRange(resource,owned,offset,4);offset+=4;
  }
}
inline AffineMatrix AffineMultiply(const AffineMatrix& a,const AffineMatrix& b){
  AffineMatrix result{};for(size_t r=0;r<4;++r)for(size_t c=0;c<4;++c)for(size_t k=0;k<4;++k)result[r*4+c]+=a[r*4+k]*b[k*4+c];return result;
}
inline AffineMatrix AffineInverse(const AffineMatrix& a){
  double det=a[0]*(a[5]*a[10]-a[6]*a[9])-a[1]*(a[4]*a[10]-a[6]*a[8])+a[2]*(a[4]*a[9]-a[5]*a[8]);
  Require(std::isfinite(det)&&det!=0,"Renderer root bindpose has singular Transform");auto b=AffineIdentity();
  b[0]=(a[5]*a[10]-a[6]*a[9])/det;b[1]=(a[2]*a[9]-a[1]*a[10])/det;b[2]=(a[1]*a[6]-a[2]*a[5])/det;
  b[4]=(a[6]*a[8]-a[4]*a[10])/det;b[5]=(a[0]*a[10]-a[2]*a[8])/det;b[6]=(a[2]*a[4]-a[0]*a[6])/det;
  b[8]=(a[4]*a[9]-a[5]*a[8])/det;b[9]=(a[1]*a[8]-a[0]*a[9])/det;b[10]=(a[0]*a[5]-a[1]*a[4])/det;
  for(size_t r=0;r<3;++r)b[r*4+3]=-(b[r*4]*a[3]+b[r*4+1]*a[7]+b[r*4+2]*a[11]);return b;
}
inline Vector3 ReadVector(const Type& type, const Value& value) {
  return {FloatValue(type, Child(type, value, "x")), FloatValue(type, Child(type, value, "y")),
          FloatValue(type, Child(type, value, "z"))};
}
inline void SetVector(const Type& type, Value& value, const Vector3& vector) {
  for (size_t i = 0; i < 3; ++i) SetFloat(type, Child(type, value, std::string(1, "xyz"[i])), vector[i]);
}
inline Value AuthorField(const Type& type, const Value& source,
                           const Resource& resource, const char* name) {
  const auto& block = resource.blocks.at(name);
  Bytes bytes(resource.bytes.begin() + block.offset, resource.bytes.begin() + block.offset + block.size);
  return DecodeValue(type, Child(type, source, name).node, bytes);
}
struct MeshEnvelope {std::vector<Vector3> lower,upper;};
inline MeshEnvelope PartEnvelope(const Type& type,const Value& source,const Resource& resource,const Mesh& mesh) {
  auto vertex = AuthorField(type, source, resource, "m_VertexData");
  const auto& channels = ConstArray(type, Child(type, vertex, "m_Channels"));
  Require(channels.children.size() == 14 && mesh.vertices,
          "Renderer bounds geometry/palette count");
  const auto& position = channels.children[0];
  Require(Integer(Child(type, position, "stream")) == 0 &&
              Integer(Child(type, position, "offset")) == 0 &&
              Integer(Child(type, position, "format")) == 0 &&
              Integer(Child(type, position, "dimension")) == 3,
          "unsupported Renderer position stream");
  const auto storage = DescribeMeshVertexStorage(type, vertex);
  const auto& data = Child(type, vertex, "m_DataSize").raw;
  Require(storage.count == mesh.vertices && uint64_t(mesh.vertices) * storage.stride[0] <= data.size(),
          "Renderer bounds vertex range");
  std::vector<Vector3> lower(mesh.vertices), upper(mesh.vertices);
  for (size_t i = 0; i < mesh.vertices; ++i)
    for (size_t axis = 0; axis < 3; ++axis) {
      float number;
      memcpy(&number, data.data() + i * storage.stride[0] + axis * 4, 4);
      Require(std::isfinite(number), "Renderer bounds nonfinite position");
      lower[i][axis] = upper[i][axis] = number;
    }
  auto shapes = AuthorField(type, source, resource, "m_Shapes");
  const auto& shapeVertices = ConstArray(type, Child(type, shapes, "vertices"));
  const auto& frames = ConstArray(type, Child(type, shapes, "shapes"));
  const auto& weights = ConstArray(type, Child(type, shapes, "fullWeights"));
  Require(weights.raw.size() == frames.count * 4, "shape bounds frame weights");
  // Add each channel's envelope so simultaneous shape weights remain inside it.
  for (const auto& channel : ConstArray(type, Child(type, shapes, "channels")).children) {
    auto first = Integer(Child(type, channel, "frameIndex")), count = Integer(Child(type, channel, "frameCount"));
    Require(count && first <= frames.count && count <= frames.count - first, "shape bounds channel range");
    std::vector<Vector3> low(mesh.vertices), high(mesh.vertices);
    for (size_t f = (size_t)first; f < first + count; ++f) {
      const auto& frame = frames.children[f];
      auto start = Integer(Child(type, frame, "firstVertex")), length = Integer(Child(type, frame, "vertexCount"));
      Require(start <= shapeVertices.count && length <= shapeVertices.count - start, "shape bounds vertex range");
      float weight;
      memcpy(&weight, weights.raw.data() + f * 4, 4);
      Require(std::isfinite(weight) && weight > 0, "shape bounds positive frame weight required");
      std::set<size_t> visited;
      for (size_t v = (size_t)start; v < start + length; ++v) {
        const auto& item = shapeVertices.children[v];
        auto index = Integer(Child(type, item, "index"));
        Require(index < mesh.vertices && visited.insert((size_t)index).second, "shape bounds duplicate/out-of-range vertex");
        auto delta = ReadVector(type, Child(type, item, "vertex"));
        for (size_t axis = 0; axis < 3; ++axis) {
          low[(size_t)index][axis] = std::min(low[(size_t)index][axis], delta[axis] * (100. / weight));
          high[(size_t)index][axis] = std::max(high[(size_t)index][axis], delta[axis] * (100. / weight));
        }
      }
    }
    for (size_t i = 0; i < mesh.vertices; ++i)
      for (size_t axis = 0; axis < 3; ++axis) { lower[i][axis] += low[i][axis]; upper[i][axis] += high[i][axis]; }
  }
  return {std::move(lower),std::move(upper)};
}
inline std::pair<Vector3,Vector3> EnvelopeBounds(const MeshEnvelope& envelope,const AffineMatrix& matrix) {
  Require(!envelope.lower.empty()&&envelope.lower.size()==envelope.upper.size(),"Mesh bounds envelope count differs");
  Vector3 minimum{DBL_MAX, DBL_MAX, DBL_MAX}, maximum{-DBL_MAX, -DBL_MAX, -DBL_MAX};
  for(size_t i=0;i<envelope.lower.size();++i)for(size_t axis=0;axis<3;++axis){double center=matrix[axis*4+3],radius=0;
    for(size_t k=0;k<3;++k){center+=matrix[axis*4+k]*((envelope.lower[i][k]+envelope.upper[i][k])*.5);radius+=std::abs(matrix[axis*4+k])*((envelope.upper[i][k]-envelope.lower[i][k])*.5);}
    minimum[axis]=std::min(minimum[axis],center-radius);maximum[axis]=std::max(maximum[axis],center+radius);
  }
  Vector3 center,extent;for(size_t i=0;i<3;++i){center[i]=(minimum[i]+maximum[i])*.5;extent[i]=(maximum[i]-minimum[i])*.5;}return {center,extent};
}
inline AffineMatrix RootBindPose(const Type& type,const Value& bindposes,size_t rootSlot) {
  const auto& palette=ConstArray(type,bindposes);Require(rootSlot<palette.count,"Renderer root bindpose slot absent");
  AffineMatrix matrix{};for(size_t r=0;r<4;++r)for(size_t c=0;c<4;++c)matrix[r*4+c]=FloatValue(type,Child(type,palette.children[rootSlot],"e"+std::to_string(r)+std::to_string(c)));
  Require(matrix[12]==0&&matrix[13]==0&&matrix[14]==0&&matrix[15]==1,"Renderer bounds requires affine root bindpose");return matrix;
}
inline std::pair<Vector3, Vector3> PartBounds(const Type& type, const Value& source,
    const Resource& resource,const Mesh& mesh,size_t rootSlot,const AffineMatrix& boneToRoot=AffineIdentity()) {
  auto envelope=PartEnvelope(type,source,resource,mesh);
  auto bindposes = AuthorField(type, source, resource, "m_BindPose");
  const auto& palette = ConstArray(type, bindposes);
  Require(palette.count == mesh.bones.size(), "Renderer bounds bindpose count");
  return EnvelopeBounds(envelope,AffineMultiply(boneToRoot,RootBindPose(type,bindposes,rootSlot)));
}
inline EiemMeshBindingMetadata::Mesh BuildMeshBindingMetadata(const Type& type,const Value& source,const Resource& resource,const Mesh& mesh) {
  EiemMeshBindingMetadata::Mesh result;if(mesh.bones.empty())return result;
  result.hashes=BoneHashes(type,AuthorField(type,source,resource,"m_BoneNameHashes"));
  auto envelope=PartEnvelope(type,source,resource,mesh);auto poses=AuthorField(type,source,resource,"m_BindPose");
  for(size_t i=0;i<mesh.bones.size();++i)result.boneBounds.push_back(EnvelopeBounds(envelope,RootBindPose(type,poses,i)));
  EiemMeshBindingMetadata::Validate(result,mesh.bones.size());return result;
}
struct RendererPartWrite {
  std::string cab, rule, part, meshCab, materialCab;
  int64_t sourceRenderer = 0, renderer = 0, owner = 0, transform = 0, parent = 0, mesh = 0, material = 0;
  std::vector<int64_t> bones;
};
struct RendererInputBuild {
  Bytes bytes;
  size_t matches = 0, skipped = 0, changedObjects = 0, addedObjects = 0, lodGroups = 0, preloadReferences = 0;
  std::vector<RendererPartWrite> parts;
  std::vector<std::pair<std::string,MeshConsumer>> consumers;
};
using MaterialPreloadIndex = std::map<NativeReference, std::set<NativeReference>>;
using RendererMaterialKey = std::pair<int64_t, std::string>;
using RendererMaterialReferences = std::map<RendererMaterialKey, NativeReference>;
inline void AddMaterialPreloads(MaterialPreloadIndex& index,const Bytes& bytes,const std::string& cab,
    const SerializedLayout& layout,const std::vector<Asset>& assets,const Package& package) {
    std::map<SerializedAssetKey,NativeReference> selected;
    std::set<SerializedAssetKey> found;
    for(const auto& asset:assets)
      if(asset.classId==21)for(const auto& material:package.materials)
        if(material.second.source.path==asset.path&&material.second.source.asset==asset.asset)
          selected.emplace(SerializedAssetKey{SerializedAssetCabKey(asset.cab),asset.pathId},
                           NativeReference{asset.cab,asset.pathId});
    if(selected.empty())return;
    for(const auto& row:layout.objects) {
      const auto& type=layout.types.at(row.second.type);if(type.classId!=142)continue;
      auto value=ObjectValue(bytes,layout,row.first);
      const auto& preloads=ConstArray(type,Child(type,value,"m_PreloadTable"));
      for(const auto& entry:ConstArray(type,Child(type,value,"m_Container")).children) {
        const auto& info=Child(type,entry,"second");
        auto first=Integer(Child(type,info,"preloadIndex")),count=Integer(Child(type,info,"preloadSize"));
        Require(first<=preloads.children.size()&&count<=preloads.children.size()-first,"Material source preload interval");
        std::set<SerializedAssetKey> intervalMaterials;
        auto root=NullableReference(type,Child(type,info,"asset"),layout,cab);
        if(root.second) {
          auto key=SerializedAssetKey{SerializedAssetCabKey(root.first),root.second};
          if(selected.count(key))intervalMaterials.insert(std::move(key));
        }
        std::set<NativeReference> dependencies;
        for(size_t i=size_t(first);i<size_t(first+count);++i) {
          auto ref=NullableReference(type,preloads.children[i],layout,cab);
          if(!ref.second)continue;
          auto key=SerializedAssetKey{SerializedAssetCabKey(ref.first),ref.second};
          if(selected.count(key))intervalMaterials.insert(std::move(key));
          dependencies.insert(std::move(ref));
        }
        // Materials are often preload-only assets under a Prefab container,
        // rather than roots in m_Container. Inherit that entry's verified
        // preload interval so their Texture/Shader references stay available.
        for(const auto& key:intervalMaterials) {
          auto material=selected.find(key);
          auto& closure=index[material->second];
          closure.insert(dependencies.begin(),dependencies.end());
          found.insert(key);
        }
      }
    }
    std::set<SerializedAssetKey> expected;
    for(const auto& row:selected)expected.insert(row.first);
    Require(found==expected,"Material source preload closure absent");
}
inline SerializedNamedAssets NamedSerializedAssets(const EndfieldInput::Bundle& bundle);
inline std::vector<Asset> SerializedReferencedAssets(const Bytes& bytes,const std::string& cab,
    const SerializedLayout& layout,const SerializedNamedAssets* named);
inline void AddMaterialPreloads(MaterialPreloadIndex& index, const Bytes& input, const Package& package) {
  auto bundle=EndfieldInput::Decode(input);
  const auto named=NamedSerializedAssets(bundle);
  for(const auto& member:bundle.members)if(SerializedHeader(member.bytes)) {
    auto layout=ParseSerialized(member.bytes);
    auto assets=SerializedReferencedAssets(member.bytes,member.name,layout,&named);
    AddMaterialPreloads(index,member.bytes,member.name,layout,assets,package);
  }
}
// Scoped to one immutable author Package/build. Skeleton pointers and palettes
// are still resolved for each Renderer; only the authored local AABB is reused.
struct RendererBoundsCache {
  std::mutex mutex;
  const Package* package = nullptr;
  std::map<std::tuple<std::string,size_t,AffineMatrix>, std::pair<Vector3,Vector3>> values;
  size_t hits = 0, misses = 0;
  double computeMs = 0;
};
inline int64_t RendererObjectId(const std::string& cab,const Package& package,const char* kind,
    int64_t renderer,const std::string& part) {
  auto label=cab+":"+package.id+":"+kind+":"+std::to_string(renderer)+":"+part;
  auto hash=Sha((const unsigned char*)label.data(),label.size());return (int64_t)Le(Bytes(hash.begin(),hash.end()),0,8);
}
class RendererMemberBuilder {
  const Bytes& source;
  const std::string cab;
  const Package& package;
  const AssetIndex& assets;
  const std::vector<BuiltMeshPart>& meshes;
  using Project = std::function<Value(int64_t,const std::set<std::string>&)>;
  Project project;
  std::function<Value(int64_t)> decode;
  std::map<int64_t,Bytes> encodedChanges;
  bool built = false;
  static const Bytes& EmptySource() { static const Bytes bytes; return bytes; }
  Value FieldsOf(int64_t id,const std::set<std::string>& fields) const {
    return project?project(id,fields):ObjectFields(source,layout,id,fields);
  }
  const AuthorByteSpans* sourceOrigins = nullptr;
  bool trackOrigins = false;
  std::map<int64_t, AuthorByteSpans> objectOrigins;
  SerializedLayout layout;
  std::map<int64_t, Value> values;
  std::map<int64_t, Value> hierarchyValues;
  std::map<int64_t, int64_t> transforms, parents, roots;
  std::map<int64_t, std::string> paths;
  std::map<int64_t, std::map<std::string, int64_t>> hierarchy;
  std::map<int64_t, std::vector<int64_t>> rendererExpansion;
  std::map<int64_t, std::vector<NativeReference>> preloadExpansion;
  const MaterialPreloadIndex* materialPreloads = nullptr;
  bool retainSourceMaterials = false;
  const std::map<std::string,EiemMeshBindingMetadata::Mesh>* authorMetadata = nullptr;
  const std::map<std::string,std::string>* authorMarkers = nullptr;
  const RendererMaterialReferences* materialReferences = nullptr;
  const MaterialPreloadIndex* materialClosures = nullptr;
  std::map<int64_t, std::map<uint32_t,int64_t>> skeletonBindings;
  std::set<int64_t> changed, allocated;
  std::vector<AddedObject> added;
  RendererInputBuild result;
  RendererBoundsCache localBounds;
  RendererBoundsCache* boundsCache = &localBounds;
  std::function<void(const char*,double)> cost;
  void Cost(const char* name,std::chrono::steady_clock::time_point start) const {
    if(cost)cost(name,std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count());
  }
  const Type& Schema(int64_t id) const { return layout.types.at(layout.objects.at(id).type); }
  Value& Tree(int64_t id, int expected = 0) {
    Require(layout.objects.count(id) && (!expected || Schema(id).classId == expected ||
            (expected == 4 && IsTransformClass(Schema(id).classId))), "Renderer local object absent/class mismatch");
    auto found = values.find(id);
    if (found == values.end()) {
      auto start=std::chrono::steady_clock::now();
      found = values.emplace(id, decode?decode(id):ObjectValue(source, layout, id, sourceOrigins)).first;
      Cost("renderer-object-decode",start);
    }
    return found->second;
  }
  const Value& InspectHierarchy(int64_t id) {
    auto edited=values.find(id);if(edited!=values.end())return edited->second;
    auto found=hierarchyValues.find(id);if(found==hierarchyValues.end())
      found=hierarchyValues.emplace(id,FieldsOf(id,{"m_GameObject","m_Name","m_Children","m_Father",
          "m_LocalPosition","m_LocalRotation","m_LocalScale"})).first;
    return found->second;
  }
  int64_t Local(const Type& type, const Value& pointer, int expected) {
    auto reference = NullableReference(type, pointer, layout, cab);
    Require(reference.first == cab && reference.second, "Renderer hierarchy/bone pointer must be local and nonnull");
    Require(layout.objects.count(reference.second)&&(!expected||Schema(reference.second).classId==expected||
            (expected==4&&IsTransformClass(Schema(reference.second).classId))),"Renderer local object absent/class mismatch");
    return reference.second;
  }
  int64_t Allocate(const char* kind, int64_t renderer, const std::string& part) {
    auto id = RendererObjectId(cab,package,kind,renderer,part);
    Require(id && !layout.objects.count(id) && allocated.insert(id).second, "Renderer added PathID collision");
    return id;
  }
  void Add(int64_t id, int64_t donor, const Value& value) {
    added.push_back({id, layout.objects.at(donor).type, EncodeValue(Schema(donor), value, trackOrigins?&objectOrigins[id]:nullptr)});
  }
  std::vector<int64_t> ResolveBones(int64_t ownerTransform, int64_t rootBone,
      const Mesh& mesh,const std::vector<uint32_t>& sourceHashes,const std::vector<int64_t>& nativeBones) const {
    std::map<size_t,int64_t> original;
    if(!sourceHashes.empty()){
      std::vector<uint32_t> author;
      if(authorMetadata)author=authorMetadata->at(mesh.resource).hashes;
      else {const auto& resource=*package.resources.at(mesh.resource);auto model=EmptyValueForMesh(resource);
        author=BoneHashes(model.first,AuthorField(model.first,model.second,resource,"m_BoneNameHashes"));}
      Require(author.size()==mesh.bones.size(),"author Mesh bone hash count differs");
      original=OriginalBoneBinding(sourceHashes,nativeBones,author);
      // Lower LOD palettes may omit a bone used by the authored LOD0 Mesh.
      // Other native Renderers on this exact root bone retain its true binding.
      auto skeleton=skeletonBindings.find(rootBone);
      if(skeleton!=skeletonBindings.end())for(size_t slot=0;slot<author.size();++slot)if(!original.count(slot)){
        auto bound=skeleton->second.find(author[slot]);
        if(bound!=skeleton->second.end()&&bound->second)original.emplace(slot,bound->second);
      }
    }
    std::set<int64_t> ownerAncestors;
    for (auto id = ownerTransform; id; id = parents.at(id)) ownerAncestors.insert(id);
    const auto& index = hierarchy.at(roots.at(ownerTransform));
    std::vector<std::vector<int64_t>> candidates;
    size_t bestMatches=0;std::string firstMissing;
    for (auto anchor = rootBone; anchor; anchor = parents.at(anchor)) {
      if (!ownerAncestors.count(anchor)) continue;
      std::vector<int64_t> bones;
      auto prefix = paths.at(anchor);
      std::string missing;
      for (size_t slot=0;slot<mesh.bones.size();++slot) {
        const auto& path=mesh.bones[slot];auto donor=original.find(slot);
        if(donor!=original.end()){
          Require(roots.at(donor->second)==roots.at(ownerTransform),"native Renderer bone belongs to another hierarchy");
          bones.push_back(donor->second);continue;
        }
        auto found = index.find(prefix.empty() ? path : prefix + "/" + path);
        if (found == index.end() || !found->second) { if(missing.empty())missing=path;continue; }
        bones.push_back(found->second);
      }
      if(bones.size()>bestMatches){bestMatches=bones.size();firstMissing=missing;}
      if (missing.empty() && !bones.empty() &&
          std::find(candidates.begin(),candidates.end(),bones)==candidates.end())
        candidates.push_back(std::move(bones));
    }
    Require(candidates.size() == 1, "Renderer bone paths have no unique local hierarchy anchor: CAB="+cab+
        " owner="+paths.at(ownerTransform)+" rootBone="+paths.at(rootBone)+
        " candidates="+std::to_string(candidates.size())+
        " matched="+std::to_string(bestMatches)+"/"+std::to_string(mesh.bones.size())+" firstMissing="+firstMissing+
        " resource="+mesh.resource);
    return candidates.front();
  }
  void Hierarchy() {
    for (auto id : layout.order) {
      if (!IsTransformClass(Schema(id).classId)) continue;
      const auto& type = Schema(id);
      const auto& tree = InspectHierarchy(id);
      auto owner = Local(type, Child(type, tree, "m_GameObject"), 1);
      Require(transforms.emplace(owner, id).second, "GameObject has multiple Transforms");
      auto parent = NullableReference(type, Child(type, tree, "m_Father"), layout, cab);
      Require(parent.first == cab && (!parent.second || IsTransformClass(Schema(parent.second).classId)), "nonlocal/invalid Transform parent");
      parents[id] = parent.second;
    }
    std::set<int64_t> visiting;
    std::function<void(int64_t)> visit = [&](int64_t id) {
      if (roots.count(id)) return;
      Require(visiting.insert(id).second, "Transform hierarchy cycle/depth");
      auto parent = parents.at(id);
      if (parent) {
        visit(parent);
        auto owner = Local(Schema(id), Child(Schema(id), InspectHierarchy(id), "m_GameObject"), 1);
        const auto& name = Child(Schema(owner), InspectHierarchy(owner), "m_Name");
        auto text = Text(name.raw.data(), name.raw.size());
        // Preserve raw names here. Flattened paths are marked ambiguous below
        // and rejected only if an authored bone lookup actually uses one.
        paths[id] = paths[parent].empty() ? text : paths[parent] + "/" + text;
        roots[id] = roots[parent];
      } else { roots[id] = id; paths[id] = ""; }
      auto& index = hierarchy[roots[id]];
      auto found = index.find(paths[id]);
      if (found == index.end()) index.emplace(paths[id], id);
      else found->second = 0; // Only selected ambiguous bone paths are rejected.
      visiting.erase(id);
    };
    for (const auto& parent : parents) visit(parent.first);
    for (const auto& row : parents) {
      const auto& type = Schema(row.first);
      for (const auto& child : ConstArray(type, Child(type, InspectHierarchy(row.first), "m_Children")).children) {
        auto id = Local(type, child, 4);
        Require(parents.at(id) == row.first, "Transform child/father reference mismatch");
      }
      if (row.second) {
        size_t count = 0;
        auto& parentType = Schema(row.second);
        for (const auto& child : ConstArray(parentType, Child(parentType, InspectHierarchy(row.second), "m_Children")).children)
          if (Local(parentType, child, 4) == row.first) ++count;
        Require(count == 1, "Transform absent/duplicate in parent children");
      }
    }
  }
  bool Preloads(const Type& type, Value& bundle) {
    auto& preloads = ArrayValue(type, Child(type, bundle, "m_PreloadTable"));
    std::vector<Value> output;
    std::vector<size_t> offsets{0};
    std::set<int64_t> referenced;
    for (const auto& pointer : preloads.children) {
      output.push_back(pointer);
      auto reference = NullableReference(type, pointer, layout, cab);
      auto extras = preloadExpansion.find(reference.second);
      if (reference.first == cab && extras != preloadExpansion.end()) {
        referenced.insert(reference.second);
        for (const auto& item : extras->second) {
          auto value = ArrayElement(type, preloads);
          SetReference(type, value, item, layout, cab);
          output.push_back(std::move(value));
          ++result.preloadReferences;
        }
      }
      offsets.push_back(output.size());
    }
    for (const auto& row : preloadExpansion)
      Require(referenced.count(row.first), "Renderer expansion absent from AssetBundle preload closure");
    if (output.size() == preloads.count) return false;
    auto update = [&](Value& info) {
      auto& start = Child(type, info, "preloadIndex"); auto& size = Child(type, info, "preloadSize");
      auto first = Integer(start), count = Integer(size);
      Require(first < offsets.size() && count <= offsets.size() - 1 - first, "Renderer source preload interval");
      SetInteger(type, start, offsets[(size_t)first]);
      SetInteger(type, size, offsets[(size_t)(first + count)] - offsets[(size_t)first]);
    };
    for (const char* name : {"m_Container", "m_HashContainer"})
      for (auto& pair : ArrayValue(type, Child(type, bundle, name)).children) update(Child(type, pair, "second"));
    update(Child(type, bundle, "m_MainAsset"));
    preloads.children = std::move(output); preloads.count = preloads.children.size();
    return true;
  }
public:
  RendererMemberBuilder(const Bytes& bytes, const std::string& name, const Package& mod,
                          const AssetIndex& graph, const std::vector<BuiltMeshPart>& meshParts)
      : source(bytes), cab(name), package(mod), assets(graph), meshes(meshParts), layout(ParseSerialized(bytes)) {}
  // Native demand inputs provide objects and fields on demand. This path never
  // reads or allocates the complete SerializedFile, including unrelated Meshes
  // or inline pixel data that may share the consumer's member.
  RendererMemberBuilder(SerializedLayout metadata, const std::string& name, const Package& mod,
                          const AssetIndex& graph, const std::vector<BuiltMeshPart>& meshParts,
                          Project fields, std::function<Value(int64_t)> object)
      : source(EmptySource()), cab(name), package(mod), assets(graph), meshes(meshParts),
        project(std::move(fields)), decode(std::move(object)), layout(std::move(metadata)) {
    Require(project&&decode,"Renderer projected object source absent");
  }
  RendererInputBuild Build(const AuthorByteSpans* inputOrigins=nullptr,AuthorByteSpans* outputOrigins=nullptr) {
    Require(!built,"Renderer member builder reused");built=true;
    Require(!project||(!inputOrigins&&!outputOrigins),"Renderer projected input cannot use full-file provenance");
    sourceOrigins=inputOrigins;trackOrigins=outputOrigins!=nullptr;
    auto start=std::chrono::steady_clock::now();
    std::map<NativeReference, Identity> selected;
    for (const auto& target : package.targetIndex) {
      auto found = assets.find(target.first);
      if (found == assets.end()) continue;
      Require(found->second.classId == 43, "Renderer target is not a Mesh");
      Require(selected.emplace(CanonicalSerializedAssetReference({found->second.cab, found->second.pathId}), target.first).second,
              "Renderer target aliases are ambiguous");
    }
    std::set<NativeReference> references;for(const auto& row:selected)references.insert(row.first);
    auto matches=ProjectedMeshConsumers(layout,cab,references,[&](int64_t id,const std::set<std::string>& fields){return FieldsOf(id,fields);},SerializedAssetCabKey);
    for(const auto& match:matches)result.consumers.emplace_back(cab,match);
    Cost("renderer-consumers",start);
    if (matches.empty()) { result.bytes = source;if(outputOrigins)*outputOrigins=inputOrigins?*inputOrigins:AuthorByteSpans{};return result; }
    start=std::chrono::steady_clock::now();
    Hierarchy();
    Cost("renderer-hierarchy",start);start=std::chrono::steady_clock::now();
    for(const auto& matched:matches) {
      if(matched.filter)continue;
      const auto& target=selected.at(CanonicalSerializedAssetReference(matched.mesh));
      auto part=std::find_if(meshes.begin(),meshes.end(),[&](const BuiltMeshPart& p){return !(p.target<target)&&!(target<p.target);});
      if(part==meshes.end()||part->sourceBoneHashes.empty())continue;
      const auto& type=Schema(matched.renderer);const auto& renderer=Tree(matched.renderer);
      auto root=Local(type,Child(type,renderer,"m_RootBone"),4);
      const auto& bones=ConstArray(type,Child(type,renderer,"m_Bones"));
      // Only complete positional pairs can donate hashes to other Renderers.
      // A mismatched pair is resolved below using the existing hierarchy path.
      if(bones.children.size()!=part->sourceBoneHashes.size())continue;
      auto& bindings=skeletonBindings[root];
      for(size_t slot=0;slot<bones.children.size();++slot){auto bone=Local(type,bones.children[slot],4);
        Require(roots.at(bone)==roots.at(root),"native LOD bone belongs to another hierarchy");
        auto found=bindings.emplace(part->sourceBoneHashes[slot],bone);
        if(!found.second&&found.first->second!=bone)found.first->second=0;
      }
    }
    Cost("renderer-native-palettes",start);start=std::chrono::steady_clock::now();
    for (const auto& matched : matches) {
      auto rid = matched.renderer;
      const auto& target = selected.at(CanonicalSerializedAssetReference(matched.mesh));
      const bool skinned=matched.filter==0;
      const auto& ruleId = package.targetIndex.at(target);
      const auto& rule = package.rules.at(ruleId);
      const auto& type = Schema(rid);
      const auto original = Tree(rid);
      ++result.matches;
      if (rule.skip) {
        SetInteger(type, Child(type, Tree(rid), "m_Enabled"), 0);
        changed.insert(rid); rendererExpansion[rid] = {}; ++result.skipped;
        continue;
      }
      auto owner = Local(type, Child(type, original, "m_GameObject"), 1);
      Require(transforms.count(owner), "Renderer owner lacks Transform");
      auto tid = transforms.at(owner);
      const auto& ownerNameField=Child(Schema(owner),Tree(owner),"m_Name");
      const auto originalOwnerName=Text(ownerNameField.raw.data(),ownerNameField.raw.size());
      int64_t rootBone=0;
      std::vector<int64_t> nativeBones;
      if(skinned){rootBone = Local(type, Child(type, original, "m_RootBone"), 4);
        // Native Prefabs can leave this optional pointer empty. Keep its value;
        // rootBone and the explicit palette still define the authored binding.
        const auto& skinningRoot = Child(type, original, "m_SkinningRoot");
        if(NullableReference(type, skinningRoot, layout, cab).second)Local(type, skinningRoot, 4);
        Require(roots.at(rootBone) == roots.at(tid), "Renderer root bone belongs to another hierarchy");
        for(const auto& bone:ConstArray(type,Child(type,original,"m_Bones")).children)nativeBones.push_back(Local(type,bone,4));}
      size_t rendererComponents = 0, transformComponents = 0;
      const auto& goType = Schema(owner);
      for (const auto& item : ConstArray(goType, Child(goType, Tree(owner), "m_Component")).children) {
        auto reference = NullableReference(goType, Child(goType, item, "component"), layout, cab);
        rendererComponents += reference == NativeReference{cab, rid};
        transformComponents += reference == NativeReference{cab, tid};
      }
      Require(rendererComponents == 1 && transformComponents == 1, "Renderer/GameObject component reference mismatch");
      std::set<NativeReference> dependencies;
      for (size_t i = 0; i < rule.parts.size(); ++i) {
        const auto& pid = rule.parts[i];
        const auto& part = package.parts.at(pid);
        const auto rendererMarker=authorMarkers?authorMarkers->at(pid):RuntimePartName(package,pid,target);
        const auto& mesh = package.meshes.at(part.mesh);
        auto nativeMesh = std::find_if(meshes.begin(), meshes.end(), [&](const BuiltMeshPart& item) {
          return item.part == pid && !(item.target < target) && !(target < item.target);
        });
        Require(nativeMesh != meshes.end(), "Renderer Mesh dependency not rebuilt");
        auto sourceMesh = assets.find(target);
        Require(sourceMesh != assets.end() && (authorMetadata || sourceMesh->second.type == package.resources.at(mesh.resource)->type),
                "Renderer author Mesh schema differs from current target");
        NativeReference materialSource,materialReference;std::vector<NativeReference> retainedMaterials;
        if(retainSourceMaterials){
          retainedMaterials=RendererMaterials(type,original,layout,cab);if(!retainedMaterials.empty())materialReference=retainedMaterials.front();
        }else if(materialReferences){
          auto variant=materialReferences->find({rid,pid});
          Require(variant!=materialReferences->end(),"Renderer material variant missing for source Renderer/Part");
          materialReference=variant->second;
        }
        else{auto material=assets.find(package.materials.at(part.material).source);
          Require(material!=assets.end()&&material->second.classId==21,"Renderer Material dependency absent from current graph");
          materialSource={material->second.cab,material->second.pathId};
          materialReference={material->second.cab,AllocateMaterialId(material->second.cab,package,part.material)};}
        Require(skinned||(mesh.bones.empty()&&mesh.shapes.empty()&&part.shapes.empty()),
                "static MeshRenderer parts require unskinned geometry without runtime shape channels");
        auto bones = skinned?ResolveBones(tid, rootBone, mesh,nativeMesh->sourceBoneHashes,nativeBones):std::vector<int64_t>{};
        auto renderer = original;
        int64_t newRid = rid, gid = owner, newTid = tid, newFilter=matched.filter;
        if (i) {
          newRid = Allocate("renderer", rid, pid); gid = Allocate("go", rid, pid); newTid = Allocate("transform", rid, pid);
          if(!skinned)newFilter=Allocate("filter",rid,pid);
          auto go = Tree(owner);
          SetString(goType, Child(goType, go, "m_Name"),RendererPartTag(originalOwnerName,rendererMarker));
          auto& components = ArrayValue(goType, Child(goType, go, "m_Component"));
          components.children.clear();
          std::vector<int64_t> componentIds{newTid};if(!skinned)componentIds.push_back(newFilter);componentIds.push_back(newRid);
          for (auto component : componentIds) {
            auto entry = ArrayElement(goType, components);
            SetReference(goType, Child(goType, entry, "component"), {cab, component}, layout, cab);
            components.children.push_back(std::move(entry));
          }
          components.count = components.children.size();
          Add(gid, owner, go);
          const auto& transformType = Schema(tid);
          auto transform = Tree(tid);
          SetReference(transformType, Child(transformType, transform, "m_GameObject"), {cab, gid}, layout, cab);
          SetReference(transformType, Child(transformType, transform, "m_Father"), {cab, tid}, layout, cab);
          auto& children = ArrayValue(transformType, Child(transformType, transform, "m_Children"));
          children.children.clear(); children.count = 0;
          SetVector(transformType, Child(transformType, transform, "m_LocalPosition"), {0, 0, 0});
          SetVector(transformType, Child(transformType, transform, "m_LocalScale"), {1, 1, 1});
          auto& rotation = Child(transformType, transform, "m_LocalRotation");
          SetVector(transformType, rotation, {0, 0, 0}); SetFloat(transformType, Child(transformType, rotation, "w"), 1);
          auto& parentChildren = ArrayValue(transformType, Child(transformType, Tree(tid), "m_Children"));
          for (auto& field : transform.children)
            if (transformType.nodes[field.node].name == "m_RootOrder") SetInteger(transformType, field, parentChildren.count);
          auto child = ArrayElement(transformType, parentChildren);
          SetReference(transformType, child, {cab, newTid}, layout, cab);
          parentChildren.children.push_back(std::move(child)); parentChildren.count = parentChildren.children.size();
          changed.insert(tid); Add(newTid, tid, transform);
          for (auto id : {gid, newTid, newRid}) dependencies.insert({cab, id});
          if(!skinned)dependencies.insert({cab,newFilter});
        } else {
          // Keep the native owner's name and hierarchy path stable. The first
          // authored part is identified by its Mesh marker; only generated
          // renderer owners (the i>0 branch above) need a provenance tag.
        }
        SetReference(type, Child(type, renderer, "m_GameObject"), {cab, gid}, layout, cab);
        if(skinned)SetReference(type, Child(type, renderer, "m_Mesh"), {nativeMesh->cab, nativeMesh->id}, layout, cab);
        else {
          const auto& ft=Schema(matched.filter);auto filter=Tree(matched.filter);
          SetReference(ft,Child(ft,filter,"m_GameObject"),{cab,gid},layout,cab);
          SetReference(ft,Child(ft,filter,"m_Mesh"),{nativeMesh->cab,nativeMesh->id},layout,cab);
          if(i)Add(newFilter,matched.filter,filter);else {Tree(matched.filter)=std::move(filter);changed.insert(matched.filter);}
          // Author geometry is not part of the original static batch or donor
          // vertex stream. Let Unity render the replacement Mesh on its own.
          for(auto& field:renderer.children){auto name=type.nodes[field.node].name;
            if(name=="m_StaticBatchInfo"){for(auto& n:field.children)SetInteger(type,n,0);}
            else if(name=="m_StaticBatchRoot"||name=="m_AdditionalVertexStreams"||name=="m_EnlightenVertexStream")SetReference(type,field,{cab,0},layout,cab);
          }
        }
        auto& materials = ArrayValue(type, Child(type, renderer, "m_Materials"));
        auto materialId=materialReference.second;
        if(!retainSourceMaterials){auto materialPointer=ArrayElement(type,materials);SetReference(type,materialPointer,materialReference,layout,cab);
          materials.children={std::move(materialPointer)};materials.count=1;}
        if(skinned){auto& palette = ArrayValue(type, Child(type, renderer, "m_Bones"));
        palette.children.clear();
        for (auto id : bones) {
          auto pointer = ArrayElement(type, palette); SetReference(type, pointer, {cab, id}, layout, cab);
          palette.children.push_back(std::move(pointer)); dependencies.insert({cab, id});
        }
        palette.count = bones.size();
        auto& weights = ArrayValue(type, Child(type, renderer, "m_BlendShapeWeights"));
        Require(Primitive(type.nodes[type.nodes[weights.node].children[1]].type) == 4, "Renderer shape weight schema");
        weights.children.clear(); weights.raw.clear(); weights.rawOrigins.clear(); weights.count = mesh.shapes.size();
        for (size_t shape=0;shape<mesh.shapes.size();++shape) {
          auto weight = ArrayElement(type, weights);
          SetFloat(type, weight, 0); // Runtime applies author state after registration.
          weights.raw.insert(weights.raw.end(), weight.raw.begin(), weight.raw.end());
        }
        // Dynamic visibility is combined with the native enabled request at runtime.
        // Preserve the original Renderer flag so inactive native consumers stay inactive.
        // Every part retains the source enabled flag; all visibility expressions,
        // including constants, are applied by the runtime binding.
        SetInteger(type, Child(type, renderer, "m_DirtyAABB"), 0);
        // Mesh schema is part of the validated author input; no original PFB is published in the Mod.
        const auto& resource = *package.resources.at(mesh.resource);
        auto rootSlot=size_t(std::find(bones.begin(),bones.end(),rootBone)-bones.begin());
        auto boneToRoot=AffineIdentity();
        // Unity permits an unweighted rootBone outside the Mesh palette.
        // Convert one authored bindpose through this exact prefab hierarchy.
        if(rootSlot==bones.size()){rootSlot=0;boneToRoot=AffineMultiply(AffineInverse(WorldMatrix(rootBone)),WorldMatrix(bones.front()));}
        auto bounds = Bounds(resource, mesh,rootSlot,boneToRoot);
        auto& aabb = Child(type, renderer, "m_AABB");
        SetVector(type, Child(type, aabb, "m_Center"), bounds.first);
        SetVector(type, Child(type, aabb, "m_Extent"), bounds.second);
        if(trackOrigins)BindBoundsOrigins(type,aabb,BoundsOriginName(rootSlot,boneToRoot,mesh.resource));
        }
        if (i) Add(newRid, rid, renderer);
        else { Tree(rid) = std::move(renderer); changed.insert(rid); }
        rendererExpansion[rid].push_back(newRid);
        dependencies.insert({nativeMesh->cab, nativeMesh->id});
        if(retainSourceMaterials)dependencies.insert(retainedMaterials.begin(),retainedMaterials.end());
        else dependencies.insert(materialReference);
        if(materialClosures) {
          auto closure=materialClosures->find(materialReference);
          Require(closure!=materialClosures->end(),"Renderer author Material preload closure absent");
          dependencies.insert(closure->second.begin(),closure->second.end());
        } else if(materialPreloads) {
          const auto& sources=retainSourceMaterials?retainedMaterials:std::vector<NativeReference>{materialSource};
          for(const auto& sourceMaterial:sources){auto closure=materialPreloads->find(sourceMaterial);
            Require(closure!=materialPreloads->end(),"Renderer Material preload closure absent");
            dependencies.insert(closure->second.begin(),closure->second.end());}
        }
        result.parts.push_back({cab, ruleId, pid, nativeMesh->cab, materialReference.first,
            rid, newRid, gid, newTid, tid, nativeMesh->id, materialId, std::move(bones)});
      }
      preloadExpansion[rid] = {dependencies.begin(), dependencies.end()};
    }
    Cost("renderer-parts",start);start=std::chrono::steady_clock::now();
    for (auto id : layout.order) {
      // These typed authored lists enumerate Renderers just like LOD arrays.
      // Scalar references keep the native anchor; list entries expand 0/1/N.
      if (Schema(id).classId == 114) {
        const auto& type=Schema(id);bool hasList=false;
        for(auto n:type.nodes.at(0).children)hasList|=type.nodes.at(n).name=="customRenderers";
        if(hasList){auto& list=ArrayValue(type,Child(type,Tree(id),"customRenderers"));std::vector<Value> output;bool modified=false;
          for(const auto& item:list.children){auto reference=NullableReference(type,item,layout,cab);auto expansion=rendererExpansion.find(reference.second);
            if(reference.first!=cab||expansion==rendererExpansion.end())output.push_back(item);
            else{for(auto renderer:expansion->second){auto entry=item;SetReference(type,entry,{cab,renderer},layout,cab);output.push_back(std::move(entry));}modified|=expansion->second!=std::vector<int64_t>{reference.second};}
          }list.children=std::move(output);list.count=list.children.size();if(modified)changed.insert(id);
        }
      }
      if (Schema(id).classId != 205) continue;
      const auto& type = Schema(id); auto& tree = Tree(id); bool modified = false;
      for (auto& lod : ArrayValue(type, Child(type, tree, "m_LODs")).children) {
        auto& renderers = ArrayValue(type, Child(type, lod, "renderers"));
        std::vector<Value> output;
        for (const auto& item : renderers.children) {
          auto reference = NullableReference(type, Child(type, item, "renderer"), layout, cab);
          auto expansion = rendererExpansion.find(reference.second);
          if (reference.first != cab || expansion == rendererExpansion.end()) output.push_back(item);
          else {
            for (auto renderer : expansion->second) {
              auto entry = item; SetReference(type, Child(type, entry, "renderer"), {cab, renderer}, layout, cab);
              output.push_back(std::move(entry));
            }
            modified |= expansion->second != std::vector<int64_t>{reference.second};
          }
        }
        renderers.children = std::move(output); renderers.count = renderers.children.size();
      }
      if (modified) { changed.insert(id); ++result.lodGroups; }
    }
    Cost("renderer-lists",start);start=std::chrono::steady_clock::now();
    if (!preloadExpansion.empty()) {
      size_t bundles = 0;
      for (auto id : layout.order) if (Schema(id).classId == 142) {
        ++bundles;
        if (Preloads(Schema(id), Tree(id))) changed.insert(id);
      }
      Require(bundles == 1, "Renderer expansion requires one current AssetBundle");
    }
    Cost("renderer-preloads",start);start=std::chrono::steady_clock::now();
    for (auto id : changed) encodedChanges.emplace(id, EncodeValue(Schema(id), Tree(id),trackOrigins?&objectOrigins[id]:nullptr));
    result.changedObjects = encodedChanges.size(); result.addedObjects = added.size();
    if(!project)result.bytes = BuildSerialized(source, layout, encodedChanges, added,{},{},{},inputOrigins,&objectOrigins,outputOrigins);
    Cost("renderer-serialize",start);
    return result;
  }
  const SerializedLayout& OutputLayout() const {Require(built,"Renderer edits requested before Build");return layout;}
  std::map<int64_t,Bytes> TakeChanges() {Require(built,"Renderer edits requested before Build");return std::move(encodedChanges);}
  std::vector<AddedObject> TakeAdded() {Require(built,"Renderer additions requested before Build");return std::move(added);}
private:
  // Supplied by the caller from the selected current Mesh input schema.
  const MeshSchemaIndex* meshSchemas = nullptr;
  std::pair<Type, Value> EmptyValueForMesh(const Resource& resource) const {
    Require(meshSchemas && meshSchemas->count(resource.type), "Renderer bounds needs current Mesh TypeTree");
    const auto& type = meshSchemas->at(resource.type);
    return {type, EmptyValue(type, 0)};
  }
  AffineMatrix WorldMatrix(int64_t id){
    auto result=AffineIdentity();for(auto node=id;node;node=parents.at(node)){
      const auto& type=Schema(node);const auto& tree=InspectHierarchy(node);auto p=ReadVector(type,Child(type,tree,"m_LocalPosition")),s=ReadVector(type,Child(type,tree,"m_LocalScale"));
      const auto& rotation=Child(type,tree,"m_LocalRotation");double x=FloatValue(type,Child(type,rotation,"x")),y=FloatValue(type,Child(type,rotation,"y")),z=FloatValue(type,Child(type,rotation,"z")),w=FloatValue(type,Child(type,rotation,"w"));
      auto norm=x*x+y*y+z*z+w*w;Require(norm>0&&std::isfinite(norm),"Renderer bounds invalid Transform rotation");auto k=2/norm;
      AffineMatrix local{(1-k*(y*y+z*z))*s[0],k*(x*y-z*w)*s[1],k*(x*z+y*w)*s[2],p[0],
        k*(x*y+z*w)*s[0],(1-k*(x*x+z*z))*s[1],k*(y*z-x*w)*s[2],p[1],
        k*(x*z-y*w)*s[0],k*(y*z+x*w)*s[1],(1-k*(x*x+y*y))*s[2],p[2],0,0,0,1};result=AffineMultiply(local,result);
    }return result;
  }
  std::pair<Vector3,Vector3> Bounds(const Resource& resource, const Mesh& mesh, size_t rootSlot,const AffineMatrix& boneToRoot) {
    if(authorMetadata){
      const auto& bounds=authorMetadata->at(mesh.resource).boneBounds.at(rootSlot);
      if(boneToRoot==AffineIdentity())return bounds;
      Vector3 center{},extent{};
      for(size_t i=0;i<3;++i){center[i]=boneToRoot[i*4+3];for(size_t k=0;k<3;++k){
        center[i]+=boneToRoot[i*4+k]*bounds.first[k];extent[i]+=std::abs(boneToRoot[i*4+k])*bounds.second[k];}}
      return {center,extent};
    }
    Require(boundsCache,"Renderer bounds cache absent");std::lock_guard<std::mutex> lock(boundsCache->mutex);
    Require(boundsCache && (!boundsCache->package || boundsCache->package == &package), "Renderer bounds cache author scope differs");boundsCache->package=&package;
    auto key=std::make_tuple(mesh.resource,rootSlot,boneToRoot);auto found=boundsCache->values.find(key);
    if(found!=boundsCache->values.end()){++boundsCache->hits;return found->second;}
    auto start=std::chrono::steady_clock::now();auto boundsTemplate=EmptyValueForMesh(resource);
    auto bounds=PartBounds(boundsTemplate.first,boundsTemplate.second,resource,mesh,rootSlot,boneToRoot);
    boundsCache->values.emplace(std::move(key),bounds);++boundsCache->misses;
    boundsCache->computeMs+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();return bounds;
  }
public:
  void SetAuthorMetadata(const std::map<std::string,EiemMeshBindingMetadata::Mesh>& metadata) {
    Require(!built,"Renderer author metadata supplied after Build");
    std::set<std::string> checked;
    for(const auto& targetMesh:meshes){
      const auto& part=package.parts.at(targetMesh.part);const auto& mesh=package.meshes.at(part.mesh);
      if(!checked.insert(mesh.resource).second)continue;
      auto binding=metadata.find(mesh.resource);
      Require(binding!=metadata.end(),"Renderer author Mesh metadata is absent: "+mesh.resource);
      EiemMeshBindingMetadata::Validate(binding->second,mesh.bones.size());
    }
    authorMetadata=&metadata;
  }
  void SetAuthorMarkers(const std::map<std::string,std::string>& markers) {authorMarkers=&markers;}
  void SetMaterialReferences(const RendererMaterialReferences& references) {materialReferences=&references;}
  void SetMaterialClosures(const MaterialPreloadIndex& closures) {materialClosures=&closures;}
  void SetCost(const std::function<void(const char*,double)>& callback) {cost=callback;}
  void SetMaterialPreloads(const MaterialPreloadIndex& preloads) { materialPreloads = &preloads; }
  void UseSourceMaterials() { retainSourceMaterials = true; }
  void SetMeshSchemas(const MeshSchemaIndex& schemas) { meshSchemas = &schemas; }
  void SetBoundsCache(RendererBoundsCache& cache) {
    std::lock_guard<std::mutex> lock(cache.mutex);
    Require(!cache.package || cache.package == &package, "Renderer bounds cache cannot cross author Packages");
    cache.package=&package;boundsCache=&cache;
  }
};
inline void AddMeshSchemas(MeshSchemaIndex& schemas,const Bytes& bytes,const std::string& cab,
    const SerializedLayout& layout,const std::vector<Asset>& assets,
    MeshBoneHashIndex* palettes,const Package* package) {
    for (const auto& type : layout.types) if (type.classId == 43) schemas.emplace(type.hash, type);
    if(palettes&&package)for(const auto& asset:assets){
      if(asset.classId!=43||!package->targetIndex.count({asset.path,asset.asset}))continue;
      const auto& type=layout.types.at(layout.objects.at(asset.pathId).type);auto value=ObjectValue(bytes,layout,asset.pathId);
      Require(palettes->emplace(NativeReference{cab,asset.pathId},BoneHashes(type,Child(type,value,"m_BoneNameHashes"))).second,
          "duplicate source Mesh bone palette");
    }
}
inline void AddMeshSchemas(MeshSchemaIndex& schemas, const Bytes& input,
                          MeshBoneHashIndex* palettes=nullptr,const Package* package=nullptr) {
  auto bundle=EndfieldInput::Decode(input);
  for(const auto& member:bundle.members)if(SerializedHeader(member.bytes)) {
    auto layout=ParseSerialized(member.bytes);
    auto assets=palettes&&package?SerializedAssets(member.bytes,member.name,layout):std::vector<Asset>{};
    AddMeshSchemas(schemas,member.bytes,member.name,layout,assets,palettes,package);
  }
}
inline SerializedNamedAssets NamedSerializedAssets(const EndfieldInput::Bundle& bundle) {
  SerializedNamedAssets result;
  for (const auto& member : bundle.members) {
    if (!SerializedHeader(member.bytes)) continue;
    auto layout = ParseSerialized(member.bytes);
    auto local = EiemNativeInput::NamedSerializedAssets(member.bytes, member.name, layout);
    for (auto& row : local) {
      auto found = result.find(row.first);
      Require(found == result.end() ||
                  (found->second.classId == row.second.classId &&
                   found->second.asset == row.second.asset &&
                   found->second.type == row.second.type),
              "duplicate named asset across source CABs");
      result.emplace(row.first, std::move(row.second));
    }
  }
  return result;
}
// The AssetBundle container names its root Prefab, while the Mesh/Material/
// Texture objects used by that Prefab are usually only present in the
// container's preload table. Expose those references under the same logical
// path so author source identities such as `prefab + M_material` can resolve.
inline std::vector<Asset> SerializedReferencedAssets(
    const Bytes& bytes, const std::string& cab, const SerializedLayout& layout,
    const SerializedNamedAssets* named = nullptr) {
  auto result = SerializedAssets(bytes, cab, layout, named);
  SerializedNamedAssets local;
  if (!named) local = EiemNativeInput::NamedSerializedAssets(bytes, cab, layout);
  const auto& lookup = named ? *named : local;
  std::set<std::tuple<std::string, std::string, std::string, int64_t>> seen;
  for (const auto& asset : result)
    if (asset.classId == 43 || asset.classId == 21 || asset.classId == 28)
      seen.emplace(asset.path, asset.asset, asset.cab, asset.pathId);
  for (const auto& row : layout.objects) {
    const auto& bundleType = layout.types.at(row.second.type);
    if (bundleType.classId != 142) continue;
    auto bundle = ObjectValue(bytes, layout, row.first);
    const auto& preloads = ConstArray(bundleType, Child(bundleType, bundle, "m_PreloadTable"));
    for (const auto& entry : ConstArray(bundleType, Child(bundleType, bundle, "m_Container")).children) {
      const auto& info = Child(bundleType, entry, "second");
      const auto& firstValue = Child(bundleType, entry, "first");
      auto first = Text(firstValue.raw.data(), firstValue.raw.size());
      auto preloadIndex = size_t(Integer(Child(bundleType, info, "preloadIndex")));
      auto preloadSize = size_t(Integer(Child(bundleType, info, "preloadSize")));
      Require(preloadIndex <= preloads.children.size() &&
                  preloadSize <= preloads.children.size() - preloadIndex,
              "AssetBundle preload interval");
      for (size_t i = preloadIndex; i < preloadIndex + preloadSize; ++i) {
        auto reference = NullableReference(bundleType, preloads.children[i], layout, cab);
        if (!reference.second) continue;
        auto found = lookup.find({SerializedAssetCabKey(reference.first), reference.second});
        if (found == lookup.end()) continue;
        const auto& source = found->second;
        if (source.classId != 43 && source.classId != 21 && source.classId != 28) continue;
        Asset asset = source;
        asset.path = first;
        std::transform(asset.path.begin(), asset.path.end(), asset.path.begin(),
                       [](unsigned char c) { return char(std::tolower(c)); });
        auto key = std::make_tuple(asset.path, asset.asset, asset.cab, asset.pathId);
        if (seen.insert(key).second) result.push_back(std::move(asset));
      }
    }
  }
  return result;
}
// All source indexes describe the same immutable input. Decode its bundle,
// TypeTrees and asset table once; retain each index's own validation rules.
inline void AddSourceInventory(AssetIndex& assets,MeshSchemaIndex& schemas,MeshBoneHashIndex& palettes,
    MaterialPreloadIndex& preloads,const EndfieldInput::Bundle& bundle,const Package& package,
    std::map<std::string,std::string>* bundleOwners=nullptr) {
  const auto named = NamedSerializedAssets(bundle);
  std::string bundleName;
  for(const auto& member:bundle.members)if(SerializedHeader(member.bytes)) {
    auto layout=ParseSerialized(member.bytes);auto catalog=SerializedReferencedAssets(member.bytes,member.name,layout,&named);
    if(bundleOwners)for(const auto& row:layout.objects)if(layout.types.at(row.second.type).classId==142){const auto& type=layout.types.at(row.second.type);auto value=ObjectValue(member.bytes,layout,row.first);const auto& name=Child(type,value,"m_Name");auto text=Text(name.raw.data(),name.raw.size());Require(bundleName.empty()||bundleName==text,"source inventory Bundle identity ambiguous");bundleName=std::move(text);}
    AddMeshSchemas(schemas,member.bytes,member.name,layout,catalog,&palettes,&package);
    AddMaterialPreloads(preloads,member.bytes,member.name,layout,catalog,package);
    AddAssets(assets,std::move(catalog));
  }
  if(bundleOwners){Require(!bundleName.empty(),"source inventory native Bundle identity absent");for(const auto& member:bundle.members){auto inserted=bundleOwners->emplace(member.name,bundleName);Require(inserted.second||inserted.first->second==bundleName,"source inventory CAB has multiple Bundle owners");}}
}
inline void AddSourceInventory(AssetIndex& assets,MeshSchemaIndex& schemas,MeshBoneHashIndex& palettes,
    MaterialPreloadIndex& preloads,const Bytes& input,const Package& package) {
  AddSourceInventory(assets,schemas,palettes,preloads,EndfieldInput::Decode(input),package);
}
inline RendererInputBuild BuildRendererInput(EndfieldInput::Bundle& bundle, const Package& package,
                                               const AssetIndex& assets,
                                               const std::vector<BuiltMeshPart>& meshParts,
                                               const MeshSchemaIndex& schemas,
                                               RendererBoundsCache* sharedBounds = nullptr,
                                               const MaterialPreloadIndex* materialPreloads = nullptr,
                                               const std::function<void(const char*,double)>& cost = {}) {
  RendererBoundsCache localBounds;auto& bounds=sharedBounds?*sharedBounds:localBounds;
  RendererInputBuild result;
  for (auto& member : bundle.members) {
    if (!SerializedHeader(member.bytes)) continue;
    RendererMemberBuilder builder(member.bytes, member.name, package, assets, meshParts);
    builder.SetCost(cost);
    builder.SetMeshSchemas(schemas);
    builder.SetBoundsCache(bounds);
    if(materialPreloads)builder.SetMaterialPreloads(*materialPreloads);
    auto rebuilt = builder.Build(&member.origins,&member.origins);
    result.matches += rebuilt.matches; result.skipped += rebuilt.skipped;
    result.changedObjects += rebuilt.changedObjects; result.addedObjects += rebuilt.addedObjects;
    result.lodGroups += rebuilt.lodGroups; result.preloadReferences += rebuilt.preloadReferences;
    result.parts.insert(result.parts.end(), rebuilt.parts.begin(), rebuilt.parts.end());
    result.consumers.insert(result.consumers.end(),rebuilt.consumers.begin(),rebuilt.consumers.end());
    member.bytes = std::move(rebuilt.bytes);
  }
  return result;
}
inline RendererInputBuild BuildRendererInput(const Bytes& source, const Package& package,
                                               const AssetIndex& assets,
                                               const std::vector<BuiltMeshPart>& meshParts,
                                               const MeshSchemaIndex& schemas,
                                               RendererBoundsCache* sharedBounds = nullptr,
                                               const MaterialPreloadIndex* materialPreloads = nullptr,
                                               const std::function<void(const char*,double)>& cost = {}) {
  auto bundle = EndfieldInput::Decode(source);
  auto result = BuildRendererInput(bundle, package, assets, meshParts, schemas, sharedBounds, materialPreloads, cost);
  result.bytes = result.changedObjects ? EndfieldInput::Encode(source, bundle.members) : source;
  auto checked = EndfieldInput::Decode(result.bytes);
  Require(checked.members.size() == bundle.members.size(), "Renderer rebuilt member count");
  for (size_t i = 0; i < bundle.members.size(); ++i)
    Require(checked.members[i].name == bundle.members[i].name && checked.members[i].flags == bundle.members[i].flags &&
                checked.members[i].bytes == bundle.members[i].bytes, "Renderer rebuilt member identity/content differs");
  return result;
}
}  // namespace EiemNativeInput

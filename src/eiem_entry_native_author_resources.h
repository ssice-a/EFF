#pragma once
#include "eiem_runtime_resource_payload.h"
#include "eiem_entry_author_materials.h"

// Creates independent native objects through metadata-bound Unity APIs. No
// consumer serialization, private native layout, compiler, or Bundle replay.
namespace NativeAuthorResources {
using RuntimeShapes::Ref;using RuntimeShapes::Call;using RuntimeShapes::Scalar;
namespace Payload=EiemRuntimePayload;
struct Api {
 void *meshClass=nullptr,*matrixClass=nullptr,*intClass=nullptr,*textureClass=nullptr;
 void *meshCtor=nullptr,*setName=nullptr,*setBindPoses=nullptr,*setIndices=nullptr,*setBounds=nullptr,*addShape=nullptr,*setSubmeshCount=nullptr,*setIndexFormat=nullptr;
 void *textureCtor=nullptr,*textureCtor6=nullptr,*textureRaw=nullptr,*textureApply=nullptr,*filter=nullptr,*wrap=nullptr,*aniso=nullptr,*bias=nullptr;
 void *attributeCount=nullptr,*attribute=nullptr,*textureWidth=nullptr,*textureHeight=nullptr,*textureMips=nullptr;
} api;
inline void* InheritedMethod(void* klass,const char* name,std::initializer_list<const char*> parameters,const char* returns){
 for(auto current=klass;current;current=il2cpp_class_get_parent(current))if(auto method=Exact(current,name,parameters,returns,false))return method;
 return nullptr;
}
inline void* LocalBoundsSetter(void* klass){
 auto method=InheritedMethod(klass,"set_localBounds",{"UnityEngine.Bounds"},"System.Void");
 return method?method:InheritedMethod(klass,"set_localBounds_Injected",{"UnityEngine.Bounds&"},"System.Void");
}
inline void DescribeAvailableMethods(){
 const std::set<std::string> names={".ctor","SetIndices","SetIndicesImpl","set_subMeshCount","set_indexFormat","set_bounds","set_bindposes","AddBlendShapeFrame","get_vertexAttributeCount","GetVertexAttribute","LoadRawTextureData","LoadRawTextureDataImpl","Apply","ApplyImpl","SetTexture","HasProperty"};
 for(auto klass:{api.meshClass,api.textureClass,AuthorMaterials::api.klass}){if(!klass)continue;void* iterator=nullptr;while(auto m=il2cpp_class_get_methods(klass,&iterator)){auto name=il2cpp_method_get_name(m);if(!name||!names.count(name))continue;std::string signature=il2cpp_class_get_name(klass);signature+="::";signature+=name;signature+='(';auto count=il2cpp_method_get_param_count(m);for(uint32_t i=0;i<count;++i){if(i)signature+=',';signature+=StartupDependencies::TypeName(il2cpp_method_get_param(m,i));}signature+=")->";signature+=StartupDependencies::TypeName(il2cpp_method_get_return_type(m));Log("[MESH-NATIVE-AVAILABLE-API] signature=%s",signature.c_str());}}
}
inline void Destroy(const Ref& object) noexcept {if(object.Get())try{void* args[]={object.Get()};Call(AuthorMaterials::api.destroy,nullptr,args);}catch(...){}}
struct Owned {
 Ref object;bool accepted=false;
 ~Owned(){if(!accepted)Destroy(object);}
};
inline void Bind(void** assemblies,size_t count){
 auto method=[](void* klass,const char* name,std::initializer_list<const char*> params,const char* returns){return FullResourceReload::Method(klass,name,params,returns,false);};
 api.meshClass=FindClass("UnityEngine","Mesh",assemblies,count);api.matrixClass=FindClass("UnityEngine","Matrix4x4",assemblies,count);api.intClass=FindClass("System","Int32",assemblies,count);api.textureClass=FindClass("UnityEngine","Texture2D",assemblies,count);
 auto object=FindClass("UnityEngine","Object",assemblies,count);auto texture=FindClass("UnityEngine","Texture",assemblies,count);
 api.meshCtor=method(api.meshClass,".ctor",{},"System.Void");api.setName=method(object,"set_name",{"System.String"},"System.Void");
 api.setBindPoses=method(api.meshClass,"set_bindposes",{"UnityEngine.Matrix4x4[]"},"System.Void");
 api.setIndices=method(api.meshClass,"SetIndices",{"System.Int32[]","UnityEngine.MeshTopology","System.Int32","System.Boolean","System.Int32"},"System.Void");
 api.setSubmeshCount=method(api.meshClass,"set_subMeshCount",{"System.Int32"},"System.Void");api.setIndexFormat=method(api.meshClass,"set_indexFormat",{"UnityEngine.Rendering.IndexFormat"},"System.Void");
 api.setBounds=method(api.meshClass,"set_bounds",{"UnityEngine.Bounds"},"System.Void");
 api.addShape=method(api.meshClass,"AddBlendShapeFrame",{"System.String","System.Single","UnityEngine.Vector3[]","UnityEngine.Vector3[]","UnityEngine.Vector3[]"},"System.Void");
 api.attributeCount=method(api.meshClass,"get_vertexAttributeCount",{},"System.Int32");api.attribute=method(api.meshClass,"GetVertexAttribute",{"System.Int32"},"UnityEngine.Rendering.VertexAttributeDescriptor");
 api.textureCtor=Exact(api.textureClass,".ctor",{"System.Int32","System.Int32","UnityEngine.TextureFormat","System.Int32","System.Boolean"},"System.Void",false);
 api.textureCtor6=Exact(api.textureClass,".ctor",{"System.Int32","System.Int32","UnityEngine.TextureFormat","System.Int32","System.Boolean","System.Boolean"},"System.Void",false);
 api.textureRaw=method(api.textureClass,"LoadRawTextureData",{"System.IntPtr","System.Int32"},"System.Void");api.textureApply=method(api.textureClass,"Apply",{"System.Boolean","System.Boolean"},"System.Void");
 api.filter=method(texture,"set_filterMode",{"UnityEngine.FilterMode"},"System.Void");api.wrap=method(texture,"set_wrapMode",{"UnityEngine.TextureWrapMode"},"System.Void");api.aniso=method(texture,"set_anisoLevel",{"System.Int32"},"System.Void");api.bias=method(texture,"set_mipMapBias",{"System.Single"},"System.Void");
 api.textureWidth=method(texture,"get_width",{},"System.Int32");api.textureHeight=method(texture,"get_height",{},"System.Int32");api.textureMips=method(texture,"get_mipmapCount",{},"System.Int32");
 AuthorMaterials::Bind(assemblies,count,false);
 for(auto value:{api.meshClass,api.matrixClass,api.intClass,api.textureClass,api.meshCtor,api.setName,api.setBindPoses,api.setIndices,api.setBounds,api.addShape,api.attributeCount,api.attribute,api.textureRaw,api.textureApply,api.filter,api.wrap,api.aniso,api.bias,api.textureWidth,api.textureHeight,api.textureMips})Require(value,"direct native author resource API absent");
 Require(api.textureCtor||api.textureCtor6,"direct native Texture2D constructor absent");
 uint32_t alignment=0;Require(il2cpp_class_value_size(api.matrixClass,&alignment)==64,"native bindpose struct layout differs");
 Log("[MESH-NATIVE-API] directMeshCreation=1 directTextureCreation=1 materialCopy=1 nativePrivateWrites=0 compilerInvoked=0");
}
template<class T> Ref Array(void* klass,const T* data,size_t count){
 auto array=Ref::Make(il2cpp_array_new(klass,count),false);Require(array.Get(),"native author managed array allocation failed");
 if(count)memcpy((char*)array.Get()+32,data,MultiplySize(count,sizeof(T)));return array;
}
inline void Name(void* object,const std::string& name){auto text=Ref::Make(il2cpp_string_new(name.c_str()),false);void* args[]={text.Get()};Call(api.setName,object,args);}
inline Ref CreateMesh(const Payload::Mesh& data,const std::string& name){
 Require(RuntimeShapes::Main(),"author Mesh creation must run on Unity thread");
 Owned owned{Ref::Make(il2cpp_object_new(api.meshClass),false)};Require(owned.object.Get(),"author Mesh allocation failed");auto mesh=owned.object.Get();Call(api.meshCtor,mesh);Name(mesh,name);
 constexpr size_t widths[]={4,2,1,1,2,2,1,1,2,2,4,4};
 // A public API dimension is a component count, never the serialized flag
 // byte. Passing 0x24 as 36 could overread a four-component source buffer.
 // Packed frames need their own decoder before this creation path is used.
 Require(!data.packedFrame,"packed frame native creation awaits verified public-channel conversion");
 unsigned flaggedUVs=0;
 for(size_t channel=0;channel<data.channels.size();++channel){auto flags=data.channels[channel].dimension&0xf0;if(!flags)continue;Require(channel>=4&&channel<12&&flags==0x20,"unknown custom author vertex encoding");++flaggedUVs;}
 if(flaggedUVs)Log("[MESH-NATIVE-VERTEX-MAPPING] flaggedUvAttributes=%u serializedUvFlags=32 publicUvComponentCounts=1 uvValuesPreserved=1 shaderInterpretationRequiresLiveAcceptance=1",flaggedUVs);
 for(size_t channel=0;channel<data.channels.size();++channel){const auto& c=data.channels[channel];auto dim=c.dimension&15;if(!dim)continue;auto width=dim*widths[c.format];Bytes contiguous(MultiplySize(data.vertices,width));
  for(size_t vertex=0;vertex<data.vertices;++vertex)memcpy(contiguous.data()+vertex*width,data.vertexData.data+data.start[c.stream]+vertex*data.stride[c.stream]+c.offset,width);
  int attr=int(channel),format=int(c.format),dimension=int(dim),count=int(data.vertices),start=0,flags=0;void* pointer=contiguous.data();void* args[]={&attr,&format,&dimension,&pointer,&count,&start,&count,&flags};
  try{Call(RuntimeShapes::api.setChannel,mesh,args);}catch(const std::exception& e){throw std::runtime_error("native author channel "+std::to_string(channel)+" format "+std::to_string(format)+" dimension "+std::to_string(dimension)+": "+e.what());}
 }
 auto matrices=Payload::BindPoses(data);auto poses=Array(api.matrixClass,matrices.data(),matrices.size());void* bindArgs[]={poses.Get()};Call(api.setBindPoses,mesh,bindArgs);
 std::vector<int32_t> indices(data.indices);auto indexWidth=data.indexFormat?4u:2u;for(size_t i=0;i<indices.size();++i)indices[i]=int32_t(Le(data.indexData.data,data.indexData.size,i*indexWidth,indexWidth));
 auto indexArray=Array(api.intClass,indices.data(),indices.size());int topology=0,submesh=0,baseVertex=0,indexFormat=int(data.indexFormat),submeshCount=1;void* formatArgs[]={&indexFormat};Call(api.setIndexFormat,mesh,formatArgs);void* submeshArgs[]={&submeshCount};Call(api.setSubmeshCount,mesh,submeshArgs);unsigned char calculate=0;void* indexArgs[]={indexArray.Get(),&topology,&submesh,&calculate,&baseVertex};Call(api.setIndices,mesh,indexArgs);
 if(!data.frames.empty()){
  std::vector<EiemShapeSnapshot::Vec3> position(data.vertices),normal(data.vertices),tangent(data.vertices);
  for(const auto& channel:data.shapes){auto label=Ref::Make(il2cpp_string_new(channel.name.c_str()),false);
   for(size_t fi=channel.first;fi<size_t(channel.first)+channel.count;++fi){const auto& frame=data.frames[fi];std::fill(position.begin(),position.end(),EiemShapeSnapshot::Vec3{});std::fill(normal.begin(),normal.end(),EiemShapeSnapshot::Vec3{});std::fill(tangent.begin(),tangent.end(),EiemShapeSnapshot::Vec3{});
    for(size_t vi=frame.first;vi<size_t(frame.first)+frame.count;++vi){const auto& v=data.shapeVertices[vi];position[v.index]=v.position;if(frame.normals)normal[v.index]=v.normal;if(frame.tangents)tangent[v.index]=v.tangent;}
    auto p=Array(RuntimeShapes::api.vec3Class,position.data(),position.size()),n=Array(RuntimeShapes::api.vec3Class,normal.data(),normal.size()),t=Array(RuntimeShapes::api.vec3Class,tangent.data(),tangent.size());float weight=frame.weight;void* args[]={label.Get(),&weight,p.Get(),n.Get(),t.Get()};Call(api.addShape,mesh,args);
   }
  }
 }
 auto bounds=data.bounds;void* boundsArgs[]={bounds.data()};Call(api.setBounds,mesh,boundsArgs);
 Require(Scalar<int>(RuntimeShapes::api.vertexCount,mesh)==int(data.vertices)&&Scalar<int>(RuntimeShapes::api.shapeCount,mesh)==int(data.shapes.size()),"created Mesh vertex/shape counts differ");
 auto after=RuntimeShapes::Array<std::array<float,16>>(Call(RuntimeShapes::api.bindPoses,mesh),matrices.size());Require(after==matrices,"created Mesh bindposes differ");
 void* readIndexArgs[]={&submesh,&calculate};auto readIndices=RuntimeShapes::Array<int32_t>(Call(RuntimeShapes::api.indices,mesh,readIndexArgs),indices.size());Require(readIndices==indices,"created Mesh indices differ");
 auto count=Scalar<int>(api.attributeCount,mesh);size_t expected=0;for(auto c:data.channels)expected+=bool(c.dimension&15);Require(count>=0&&size_t(count)==expected,"created Mesh attribute count differs");
 for(int i=0;i<count;++i){void* args[]={&i};auto boxed=Ref::Make(Call(api.attribute,mesh,args),false);std::array<int,4> descriptor{};Require(boxed.Get()&&CopyRaw(il2cpp_object_unbox(boxed.Get()),descriptor.data(),sizeof(descriptor)),"native vertex descriptor readback failed");Require(descriptor[0]>=0&&descriptor[0]<14,"native vertex descriptor attribute differs");const auto& c=data.channels[descriptor[0]];Require(descriptor[1]==int(c.format)&&descriptor[2]==int(c.dimension&15),"native API changed author vertex format/component count");}
 if(!data.bones.empty()){auto weights=Ref::Make(Call(RuntimeShapes::api.boneWeights,mesh),false);size_t n=0;Require(ArrayLength(weights.Get(),&n)&&n==data.vertices,"native Mesh skin weights unavailable after creation");}
 for(size_t i=0;i<data.shapes.size();++i){int index=int(i);void* args[]={&index};Require(String(Call(RuntimeShapes::api.shapeName,mesh,args))==data.shapes[i].name&&Scalar<int>(RuntimeShapes::api.frameCount,mesh,args)==int(data.shapes[i].count),"native shape channel readback differs");}
 owned.accepted=true;return owned.object;
}
inline Ref CreateTexture(const Payload::Texture& data,const std::string& name){
 Require(RuntimeShapes::Main(),"author Texture creation must run on Unity thread");Owned owned{Ref::Make(il2cpp_object_new(api.textureClass),false)};Require(owned.object.Get(),"author Texture allocation failed");auto object=owned.object.Get();
 int width=int(data.width),height=int(data.height),format=int(data.format),mips=int(data.mips);unsigned char linear=data.linear,uninitialized=1;void* ctor[]={&width,&height,&format,&mips,&linear,&uninitialized};Call(api.textureCtor6?api.textureCtor6:api.textureCtor,object,ctor);Name(object,name);
 void* pixels=const_cast<unsigned char*>(data.pixels.data);int size=int(data.pixels.size);void* load[]={&pixels,&size};Call(api.textureRaw,object,load);
 int filter=int(data.filter),wrap=int(data.wrap),aniso=int(data.aniso);float bias=data.bias;void* a[]={&filter};Call(api.filter,object,a);a[0]=&wrap;Call(api.wrap,object,a);a[0]=&aniso;Call(api.aniso,object,a);a[0]=&bias;Call(api.bias,object,a);
 unsigned char recalculate=0,discardCpu=1;void* apply[]={&recalculate,&discardCpu};Call(api.textureApply,object,apply);
 Require(Scalar<int>(api.textureWidth,object)==width&&Scalar<int>(api.textureHeight,object)==height&&Scalar<int>(api.textureMips,object)==mips,"created Texture dimensions/mips differ");owned.accepted=true;return owned.object;
}
} // namespace NativeAuthorResources

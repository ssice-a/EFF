#pragma once
#include "eiem_runtime_author_directory.h"
#include "eiem_material_operations.h"

// Only applies offline typed author operations to a private native Material.
// No TypeTree parsing, image conversion or game-resource mutation occurs here.
namespace AuthorMaterials {
using RuntimeShapes::Ref;using RuntimeShapes::Call;using RuntimeShapes::Scalar;
struct Api {
 void *klass=nullptr,*clone=nullptr,*has=nullptr,*setFloat=nullptr,*setInteger=nullptr;
 void *setColor=nullptr,*setScale=nullptr,*setOffset=nullptr,*enableKeyword=nullptr,*disableKeyword=nullptr;
 void *setTag=nullptr,*setQueue=nullptr,*setTexture=nullptr,*destroy=nullptr;
} api;
inline void Bind(void** assemblies,size_t count,bool allOperations=true){
 auto operation=[&](const char* name,std::initializer_list<const char*> parameters){return allOperations?FullResourceReload::Method(api.klass,name,parameters,"System.Void",false):Exact(api.klass,name,parameters,"System.Void",false);};
 api.klass=FindClass("UnityEngine","Material",assemblies,count);
 api.clone=FullResourceReload::Method(api.klass,".ctor",{"UnityEngine.Material"},"System.Void",false);
 api.has=FullResourceReload::Method(api.klass,"HasProperty",{"System.String"},"System.Boolean",false);
 api.setFloat=operation("SetFloat",{"System.String","System.Single"});
 api.setInteger=operation("SetInteger",{"System.String","System.Int32"});
 api.setColor=operation("SetColor",{"System.String","UnityEngine.Color"});
 api.setScale=operation("SetTextureScale",{"System.String","UnityEngine.Vector2"});
 api.setOffset=operation("SetTextureOffset",{"System.String","UnityEngine.Vector2"});
 api.enableKeyword=operation("EnableKeyword",{"System.String"});
 api.disableKeyword=operation("DisableKeyword",{"System.String"});
 api.setTag=operation("SetOverrideTag",{"System.String","System.String"});
 api.setQueue=operation("set_renderQueue",{"System.Int32"});
 api.setTexture=FullResourceReload::Method(api.klass,"SetTexture",{"System.String","UnityEngine.Texture"},"System.Void",false);
 api.destroy=FullResourceReload::Method(FindClass("UnityEngine","Object",assemblies,count),"DestroyImmediate",{"UnityEngine.Object"},"System.Void",true);
}
inline Ref Property(void* material,const std::string& name){
 auto key=Ref::Make(il2cpp_string_new(name.c_str()),false);void* args[]={key.Get()};
 Require(Scalar<unsigned char>(api.has,material,args),"current Material shader lacks authored property: "+name);return key;
}
inline Ref Create(void* donor,const EiemMaterialOperations::Edits& e,const std::map<std::string,Ref>& textures){
 Require(RuntimeShapes::Main()&&RuntimeShapes::Alive(donor),"author Material donor unavailable on Unity thread");
 auto material=Ref::Make(il2cpp_object_new(api.klass),false);Require(material.Get(),"private Material allocation failed");
 void* cloneArgs[]={donor};Call(api.clone,material.Get(),cloneArgs);
 struct FailureCleanup {Ref material;bool accepted=false;~FailureCleanup(){if(!accepted&&material.Get()&&api.destroy)try{void* args[]={material.Get()};Call(api.destroy,nullptr,args);}catch(...){}}} cleanup{material};
 for(const auto& x:e.floats){auto key=Property(material.Get(),x.first);Require(std::abs(x.second)<=FLT_MAX,"Material float cannot fit native field");float value=float(x.second);void* args[]={key.Get(),&value};Call(api.setFloat,material.Get(),args);}
 for(const auto& x:e.integers){auto key=Property(material.Get(),x.first);Require(x.second>=INT32_MIN&&x.second<=INT32_MAX,"Material integer cannot fit native field");int32_t value=int32_t(x.second);void* args[]={key.Get(),&value};Call(api.setInteger,material.Get(),args);}
 for(const auto& x:e.colors){auto key=Property(material.Get(),x.first);Color value{};float* channels=&value.r;for(size_t i=0;i<4;++i){Require(std::abs(x.second[i])<=FLT_MAX,"Material color cannot fit native field");channels[i]=float(x.second[i]);}void* args[]={key.Get(),&value};Call(api.setColor,material.Get(),args);}
 auto vectors=[&](void* method,const auto& values){for(const auto& x:values){auto key=Property(material.Get(),x.first);Require(std::abs(x.second[0])<=FLT_MAX&&std::abs(x.second[1])<=FLT_MAX,"Material texture transform cannot fit native field");Vector2 value{float(x.second[0]),float(x.second[1])};void* args[]={key.Get(),&value};Call(method,material.Get(),args);}};
 vectors(api.setScale,e.scales);vectors(api.setOffset,e.offsets);
 for(const auto& x:e.keywords){auto key=Ref::Make(il2cpp_string_new(x.first.c_str()),false);void* args[]={key.Get()};Call(x.second?api.enableKeyword:api.disableKeyword,material.Get(),args);}
 for(const auto& x:e.tags){auto key=Ref::Make(il2cpp_string_new(x.first.c_str()),false),value=Ref::Make(il2cpp_string_new(x.second.c_str()),false);void* args[]={key.Get(),value.Get()};Call(api.setTag,material.Get(),args);}
 if(e.queue){int32_t value=*e.queue;void* args[]={&value};Call(api.setQueue,material.Get(),args);}
 for(const auto& x:textures){auto key=Property(material.Get(),x.first);Require(RuntimeShapes::Alive(x.second.Get()),"author Material texture is not loaded");void* args[]={key.Get(),x.second.Get()};Call(api.setTexture,material.Get(),args);}
 cleanup.accepted=true;return material;
}
}

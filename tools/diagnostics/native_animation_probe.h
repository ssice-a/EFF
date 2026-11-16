#pragma once
#include <cmath>
#include <type_traits>

// Included inside Entry only by the opt-in build. No Lua-generated wrapper is
// needed. All metadata is checked before loading the fixture or creating objects.
namespace NativeAnimationProbe {
using RuntimeShapes::Ref;
using RuntimeShapes::Call;
using RuntimeShapes::Scalar;
using V2=std::array<float,2>;
using V3=std::array<float,3>;
using V4=std::array<float,4>;
bool attempted=false;
struct Api {
 void *gameObject=nullptr,*mesh=nullptr,*material=nullptr,*block=nullptr,*vec3=nullptr;
 Ref skinType,clipType,animatorType;
 void *goCtor=nullptr,*meshCtor=nullptr,*materialCtor=nullptr,*blockCtor=nullptr,*destroy=nullptr;
 void *active=nullptr,*transform=nullptr,*parent=nullptr,*component=nullptr,*vertices=nullptr,*addShape=nullptr;
 void *setMesh=nullptr,*setMaterial=nullptr,*getMaterial=nullptr,*rendererEnabled=nullptr,*shader=nullptr,*propertyId=nullptr,*hasMaterialProperty=nullptr;
 void *setFloat=nullptr,*getFloat=nullptr,*setColor=nullptr,*getColor=nullptr,*setScale=nullptr,*getScale=nullptr,*setOffset=nullptr,*getOffset=nullptr;
 void *getGlobalBlock=nullptr,*getSlotBlock=nullptr,*clearBlock=nullptr,*emptyBlock=nullptr,*hasBlockProperty=nullptr;
 void *blockFloat=nullptr,*blockColor=nullptr,*blockVector=nullptr,*position=nullptr,*rotation=nullptr,*shape=nullptr;
 void *loadAsset=nullptr,*sample=nullptr,*length=nullptr,*legacy=nullptr;
 bool(*emptyBlockIcall)(void*)=nullptr;
 bool(*hasBlockPropertyIcall)(void*,int)=nullptr;
 void(*blockColorIcall)(void*,int,V4*)=nullptr;
 void *setBlockFloat=nullptr,*setBlockColor=nullptr;
};
Api Bind(){
 Require(il2cpp_domain_get&&il2cpp_domain_get_assemblies&&il2cpp_class_get_type&&il2cpp_type_get_object&&
         il2cpp_class_value_size&&il2cpp_object_new&&il2cpp_array_new,"probe metadata APIs unavailable");
 size_t count=0;auto assemblies=il2cpp_domain_get_assemblies(il2cpp_domain_get(),&count);Api a;
 auto klass=[&](const char* name){auto c=FindClass("UnityEngine",name,assemblies,count);Require(c,std::string("probe class unavailable: ")+name);return c;};
 unsigned missing=0;
 auto method=[&](void* c,const char* name,std::initializer_list<const char*> args,const char* result,bool isStatic=false){
  auto m=Exact(c,name,args,result,isStatic);if(!m){++missing;Log("[NATIVE-ANIMATION-PROBE] phase=missing-api owner=%s name=%s arguments=%zu",il2cpp_class_get_name(c),name,args.size());}return m;
 };
 auto optional=[](void* c,const char* name,std::initializer_list<const char*> args,const char* result){
  return Exact(c,name,args,result,false);
 };
 auto type=[](void* c){auto t=Ref::Make(il2cpp_type_get_object(il2cpp_class_get_type(c)),false);Require(t.Get(),"probe managed type unavailable");return t;};
 a.gameObject=klass("GameObject");a.mesh=klass("Mesh");a.material=klass("Material");a.block=klass("MaterialPropertyBlock");
 auto skin=klass("SkinnedMeshRenderer"),clip=klass("AnimationClip"),renderer=klass("Renderer"),transform=klass("Transform"),shader=klass("Shader");
 a.skinType=type(skin);a.clipType=type(clip);a.animatorType=type(klass("Animator"));a.vec3=klass("Vector3");
 for(auto row:{std::make_pair(klass("Vector2"),8),std::make_pair(a.vec3,12),std::make_pair(klass("Quaternion"),16),std::make_pair(klass("Vector4"),16),std::make_pair(klass("Color"),16)}){
  uint32_t alignment=0;Require(il2cpp_class_value_size(row.first,&alignment)==row.second,"probe value type layout differs");
 }
 a.goCtor=method(a.gameObject,".ctor",{"System.String"},"System.Void");a.meshCtor=method(a.mesh,".ctor",{},"System.Void");
 a.materialCtor=method(a.material,".ctor",{"UnityEngine.Shader"},"System.Void");a.blockCtor=method(a.block,".ctor",{},"System.Void");
 a.destroy=method(klass("Object"),"DestroyImmediate",{"UnityEngine.Object","System.Boolean"},"System.Void",true);
 a.active=method(a.gameObject,"SetActive",{"System.Boolean"},"System.Void");a.transform=method(a.gameObject,"get_transform",{},"UnityEngine.Transform");
 a.parent=method(transform,"SetParent",{"UnityEngine.Transform","System.Boolean"},"System.Void");
 a.component=method(a.gameObject,"AddComponent",{"System.Type"},"UnityEngine.Component");
 a.vertices=method(a.mesh,"set_vertices",{"UnityEngine.Vector3[]"},"System.Void");
 a.addShape=method(a.mesh,"AddBlendShapeFrame",{"System.String","System.Single","UnityEngine.Vector3[]","UnityEngine.Vector3[]","UnityEngine.Vector3[]"},"System.Void");
 a.setMesh=method(skin,"set_sharedMesh",{"UnityEngine.Mesh"},"System.Void");
 a.setMaterial=method(renderer,"set_sharedMaterial",{"UnityEngine.Material"},"System.Void");a.getMaterial=method(renderer,"get_sharedMaterial",{},"UnityEngine.Material");
 a.rendererEnabled=method(renderer,"set_enabled",{"System.Boolean"},"System.Void");
 a.shader=method(shader,"Find",{"System.String"},"UnityEngine.Shader",true);a.propertyId=method(shader,"PropertyToID",{"System.String"},"System.Int32",true);
 a.hasMaterialProperty=method(a.material,"HasProperty",{"System.String"},"System.Boolean");
 a.setFloat=method(a.material,"SetFloat",{"System.String","System.Single"},"System.Void");a.getFloat=method(a.material,"GetFloat",{"System.String"},"System.Single");
 a.setColor=method(a.material,"SetColor",{"System.String","UnityEngine.Color"},"System.Void");a.getColor=method(a.material,"GetColor",{"System.String"},"UnityEngine.Color");
 a.setScale=method(a.material,"SetTextureScale",{"System.String","UnityEngine.Vector2"},"System.Void");a.getScale=method(a.material,"GetTextureScale",{"System.String"},"UnityEngine.Vector2");
 a.setOffset=method(a.material,"SetTextureOffset",{"System.String","UnityEngine.Vector2"},"System.Void");a.getOffset=method(a.material,"GetTextureOffset",{"System.String"},"UnityEngine.Vector2");
 a.getGlobalBlock=method(renderer,"GetPropertyBlock",{"UnityEngine.MaterialPropertyBlock"},"System.Void");
 a.getSlotBlock=method(renderer,"GetPropertyBlock",{"UnityEngine.MaterialPropertyBlock","System.Int32"},"System.Void");
 a.clearBlock=method(a.block,"Clear",{},"System.Void");a.emptyBlock=optional(a.block,"get_isEmpty",{},"System.Boolean");a.hasBlockProperty=optional(a.block,"HasProperty",{"System.Int32"},"System.Boolean");
 a.blockFloat=method(a.block,"GetFloat",{"System.Int32"},"System.Single");a.blockColor=optional(a.block,"GetColor",{"System.Int32"},"UnityEngine.Color");
 a.blockVector=method(a.block,"GetVector",{"System.Int32"},"UnityEngine.Vector4");
 a.setBlockFloat=method(a.block,"SetFloat",{"System.Int32","System.Single"},"System.Void");
 a.setBlockColor=method(a.block,"SetColor",{"System.Int32","UnityEngine.Color"},"System.Void");
 if(il2cpp_resolve_icall){
  if(!a.emptyBlock)a.emptyBlockIcall=(decltype(a.emptyBlockIcall))il2cpp_resolve_icall("UnityEngine.MaterialPropertyBlock::get_isEmpty");
  if(!a.hasBlockProperty)a.hasBlockPropertyIcall=(decltype(a.hasBlockPropertyIcall))il2cpp_resolve_icall("UnityEngine.MaterialPropertyBlock::HasPropertyImpl");
  if(!a.blockColor)a.blockColorIcall=(decltype(a.blockColorIcall))il2cpp_resolve_icall("UnityEngine.MaterialPropertyBlock::GetColorImpl_Injected");
 }
 for(auto row:{std::make_pair(bool(a.emptyBlock||a.emptyBlockIcall),"MaterialPropertyBlock.isEmpty"),
               std::make_pair(bool(a.hasBlockProperty||a.hasBlockPropertyIcall),"MaterialPropertyBlock.HasProperty"),
               std::make_pair(bool(a.blockColor||a.blockColorIcall),"MaterialPropertyBlock.GetColor")})
  if(!row.first){++missing;Log("[NATIVE-ANIMATION-PROBE] phase=missing-api owner=native-fallback name=%s",row.second);}
 a.position=method(transform,"get_localPosition",{},"UnityEngine.Vector3");a.rotation=method(transform,"get_localRotation",{},"UnityEngine.Quaternion");
 a.shape=method(skin,"GetBlendShapeWeight",{"System.Int32"},"System.Single");
 a.loadAsset=method(klass("AssetBundle"),"LoadAsset_Internal",{"System.String","System.Type"},"UnityEngine.Object");
 a.sample=method(clip,"SampleAnimation",{"UnityEngine.GameObject","System.Single"},"System.Void");
 a.length=method(clip,"get_length",{},"System.Single");a.legacy=method(clip,"get_legacy",{},"System.Boolean");
 Require(!missing,"probe API preflight missing "+std::to_string(missing)+" bindings; see missing-api records");
 return a;
}
void Near(float actual,float expected,const char* name){
 Require(std::isfinite(actual)&&std::abs(actual-expected)<0.003f,std::string(name)+": expected="+std::to_string(expected)+" actual="+std::to_string(actual));
}
struct Run {
 Api a;Ref bundle;
 std::vector<Ref> consumers,resources;
 std::set<void*> owned;
 std::vector<std::string> mismatches;
 bool closed=false;
 explicit Run(Api value):a(std::move(value)){consumers.reserve(8);resources.reserve(8);}
 bool Close() noexcept {
  if(closed)return true;closed=true;bool ok=true;
  auto destroy=[&](const Ref& value){try{unsigned char assets=0;void* args[]={value.Get(),&assets};Call(a.destroy,nullptr,args);}catch(...){ok=false;}};
  // Destroy probe consumers synchronously before their Mesh/Material or Clip.
  for(auto i=consumers.rbegin();i!=consumers.rend();++i)destroy(*i);
  for(auto i=resources.rbegin();i!=resources.rend();++i)destroy(*i);
  if(bundle.Get())try{NativeBundle::Unload(bundle.Get(),true);}catch(...){ok=false;}
  Log("[NATIVE-ANIMATION-PROBE] phase=cleanup result=%s",ok?"PASS":"FAIL");return ok;
 }
 ~Run(){if(!closed)Close();}
 Ref New(void* c,int owner=0){auto o=Ref::Make(il2cpp_object_new(c),false);Require(o.Get(),"probe object allocation failed");
  if(owner){(owner==1?consumers:resources).push_back(o);owned.insert(o.Get());}return o;
 }
 Ref Text(const char* text){return Ref::Make(il2cpp_string_new(text),false);}
 void SampleNear(float actual,float expected,const char* name){
  if(std::isfinite(actual)&&std::abs(actual-expected)<0.003f)return;
  mismatches.push_back(std::string(name)+": expected="+std::to_string(expected)+" actual="+std::to_string(actual));
  Log("[NATIVE-ANIMATION-PROBE] phase=sample-mismatch detail=%s",mismatches.back().c_str());
 }
 bool Empty(void* block){return a.emptyBlock?Scalar<unsigned char>(a.emptyBlock,block)!=0:a.emptyBlockIcall(block);}
 bool Has(void* block,int id){void* key[]={&id};return a.hasBlockProperty?Scalar<unsigned char>(a.hasBlockProperty,block,key)!=0:a.hasBlockPropertyIcall(block,id);}
 V4 BlockColor(void* block,int id){void* key[]={&id};if(a.blockColor)return Scalar<V4>(a.blockColor,block,key);V4 value{};a.blockColorIcall(block,id,&value);return value;}
 void CheckBlockAdapters(){
  auto block=New(a.block);Call(a.blockCtor,block.Get());auto name=Text("_EFF_ProbeOnly");void* property[]={name.Get()};int id=Scalar<int>(a.propertyId,nullptr,property);
  Require(Empty(block.Get())&&!Has(block.Get(),id),"fresh probe block is not empty");float value=0.375f;void* args[]={&id,&value};Call(a.setBlockFloat,block.Get(),args);
  Require(!Empty(block.Get())&&Has(block.Get(),id),"probe block float presence differs");void* key[]={&id};Near(Scalar<float>(a.blockFloat,block.Get(),key),value,"probe block float readback");
  Call(a.clearBlock,block.Get());V4 color{0.2f,0.4f,0.6f,0.8f};void* colors[]={&id,color.data()};Call(a.setBlockColor,block.Get(),colors);
  Require(Has(block.Get(),id),"probe block color absent");auto actual=BlockColor(block.Get(),id);for(size_t i=0;i<4;++i)Near(actual[i],color[i],"probe block color readback");
  Call(a.clearBlock,block.Get());Require(Empty(block.Get())&&!Has(block.Get(),id),"probe block clear failed");
  Log("[NATIVE-ANIMATION-PROBE] phase=block-adapters result=PASS emptyIcall=%d hasIcall=%d colorIcall=%d",int(bool(a.emptyBlockIcall)),int(bool(a.hasBlockPropertyIcall)),int(bool(a.blockColorIcall)));
 }
 Ref Vectors(const std::array<V3,3>& data){
  auto array=Ref::Make(il2cpp_array_new(a.vec3,data.size()),false);size_t n=0;
  Require(array.Get()&&ArrayLength(array.Get(),&n)&&n==data.size(),"probe vector array layout differs");
  memcpy(static_cast<char*>(array.Get())+32,data.data(),sizeof(data));return array;
 }
 struct Instance {Ref root,pivot,renderer;};
 Instance InstanceOf(const char* name,const Ref& mesh,const Ref& material){
  auto go=[&](const char* label){auto root=New(a.gameObject,1);auto text=Text(label);void* args[]={text.Get()};Call(a.goCtor,root.Get(),args);return root;};
  auto root=go(name);unsigned char active=0;void* inactive[]={&active};Call(a.active,root.Get(),inactive);
  auto rootTransform=Ref::Make(Call(a.transform,root.Get()),false);
  auto child=[&](const char* label){auto object=go(label);auto t=Ref::Make(Call(a.transform,object.Get()),false);unsigned char keep=0;void* args[]={rootTransform.Get(),&keep};Call(a.parent,t.Get(),args);return std::make_pair(object,t);};
  auto pivot=child("Pivot"),surface=child("Surface");void* component[]={a.skinType.Get()};auto renderer=Ref::Make(Call(a.component,surface.first.Get(),component),false);Require(renderer.Get(),"probe Renderer missing");
  void* args[]={mesh.Get()};Call(a.setMesh,renderer.Get(),args);args[0]=material.Get();Call(a.setMaterial,renderer.Get(),args);
  // Non-Legacy sampling in the player requires a native Animator. Activate
  // this isolated hierarchy for its lifecycle, with rendering disabled.
  unsigned char visible=0;void* disabled[]={&visible};Call(a.rendererEnabled,renderer.Get(),disabled);
  void* animatorArgs[]={a.animatorType.Get()};Require(Call(a.component,root.Get(),animatorArgs),"probe Animator missing");
  active=1;Call(a.active,root.Get(),inactive);
  Log("[NATIVE-ANIMATION-PROBE] phase=instance-ready name=%s animator=1 active=1 rendererEnabled=0 controllerAssigned=0",name);
  return {root,pivot.second,renderer};
 }
 Ref CurrentMaterial(const Ref& renderer){
  auto material=Ref::Make(Call(a.getMaterial,renderer.Get()),false);Require(material.Get(),"probe sampled Material missing");
  if(owned.insert(material.Get()).second)resources.push_back(material);return material;
 }
 void Execute(const std::filesystem::path& path){
  CheckBlockAdapters();
  EiemNativeRandomAccess::File file(path,0);Require(file.Size()&&file.Size()<=1024*1024,"probe bundle exceeds fixture budget");
  auto bytes=std::make_shared<EiemNativeInput::Bytes>(file.Size());file.ReadAt(0,bytes->data(),bytes->size());
  Log("[NATIVE-ANIMATION-PROBE] phase=load-bundle loader=native-archive-stream bytes=%zu",bytes->size());
  bundle=Ref::Make(NativeBundle::LoadBytes(std::shared_ptr<const EiemNativeInput::Bytes>(bytes),0,false),false);Require(bundle.Get(),"probe native bundle did not load");
  auto label=Text("assets/eff/diagnostics/mixed-animation.anim");void* load[]={label.Get(),a.clipType.Get()};
  Log("[NATIVE-ANIMATION-PROBE] phase=load-clip");
  auto clip=Ref::Make(Call(a.loadAsset,bundle.Get(),load),false);Require(clip.Get(),"probe Clip asset missing");Near(Scalar<float>(a.length,clip.Get()),1,"duration");
  Require(!Scalar<unsigned char>(a.legacy,clip.Get()),"probe requires non-Legacy Clip");
  Log("[NATIVE-ANIMATION-PROBE] phase=clip-ready duration=1 legacy=0");
  auto shaderName=Text("UI/Default");void* find[]={shaderName.Get()};auto shader=Ref::Make(Call(a.shader,nullptr,find),false);Require(shader.Get(),"probe UI/Default shader unavailable");
  auto material=New(a.material,2);void* ctor[]={shader.Get()};Call(a.materialCtor,material.Get(),ctor);
  auto colorName=Text("_Color"),floatName=Text("_Stencil"),uvName=Text("_MainTex_ST"),textureName=Text("_MainTex");
  for(auto name:{colorName.Get(),floatName.Get(),textureName.Get()}){void* args[]={name};Require(Scalar<unsigned char>(a.hasMaterialProperty,material.Get(),args),"probe shader property unavailable");}
  V4 color{0.2f,0.3f,0.4f,0.9f};V2 scale{1,1},offset{0,0};float stencil=2;
  void* c[]={colorName.Get(),color.data()};Call(a.setColor,material.Get(),c);void* f[]={floatName.Get(),&stencil};Call(a.setFloat,material.Get(),f);
  void* sc[]={textureName.Get(),scale.data()};Call(a.setScale,material.Get(),sc);void* of[]={textureName.Get(),offset.data()};Call(a.setOffset,material.Get(),of);
  auto mesh=New(a.mesh,2);Call(a.meshCtor,mesh.Get());auto vertices=Vectors({V3{0,0,0},V3{1,0,0},V3{0,1,0}});void* set[]={vertices.Get()};Call(a.vertices,mesh.Get(),set);
  auto delta=Vectors({V3{0,0,1},V3{0,0,1},V3{0,0,1}}),zero=Vectors({V3{},V3{},V3{}});auto pulse=Text("Pulse");float weight=100;
  void* shape[]={pulse.Get(),&weight,delta.Get(),zero.Get(),zero.Get()};Call(a.addShape,mesh.Get(),shape);
  auto first=InstanceOf("EFF_NativeAnimationProbe_A",mesh,material),other=InstanceOf("EFF_NativeAnimationProbe_B",mesh,material);
  auto global=New(a.block),slot=New(a.block);Call(a.blockCtor,global.Get());Call(a.blockCtor,slot.Get());
  auto effectiveBlock=[&](const Ref& renderer,int id){
   Call(a.clearBlock,global.Get());Call(a.clearBlock,slot.Get());void* g[]={global.Get()};Call(a.getGlobalBlock,renderer.Get(),g);
   int index=0;void* s[]={slot.Get(),&index};Call(a.getSlotBlock,renderer.Get(),s);
   auto block=Empty(slot.Get())?global.Get():slot.Get();return Has(block,id)?block:nullptr;
  };
  auto read=[&](const Ref& renderer,void* name,void* getter,void* blockGetter,auto tag){
   using T=decltype(tag);void* property[]={name};int id=Scalar<int>(a.propertyId,nullptr,property);void* key[]={&id};
   auto block=effectiveBlock(renderer,id);if(block){
    if constexpr(std::is_same_v<T,V4>){if(name==colorName.Get())return BlockColor(block,id);}
    return Scalar<T>(blockGetter,block,key);
   }
   return Scalar<T>(getter,CurrentMaterial(renderer).Get(),property);
  };
  auto readUv=[&](const Ref& renderer){
   void* property[]={uvName.Get()};int id=Scalar<int>(a.propertyId,nullptr,property);void* key[]={&id};
   auto block=effectiveBlock(renderer,id);if(block)return Scalar<V4>(a.blockVector,block,key);
   auto current=CurrentMaterial(renderer);void* texture[]={textureName.Get()};auto scale=Scalar<V2>(a.getScale,current.Get(),texture),offset=Scalar<V2>(a.getOffset,current.Get(),texture);
   return V4{scale[0],scale[1],offset[0],offset[1]};
  };
  auto verify=[&](const Ref& renderer,float u){
   int index=0;void* shapeArgs[]={&index};SampleNear(Scalar<float>(a.shape,renderer.Get(),shapeArgs),75*u,"shape.Pulse");
   SampleNear(read(renderer,floatName.Get(),a.getFloat,a.blockFloat,float{}),0.1f+0.7f*u,"material.float");
   auto c=read(renderer,colorName.Get(),a.getColor,a.blockColor,V4{});SampleNear(c[0],1-u,"color.r");SampleNear(c[1],u,"color.g");SampleNear(c[2],0,"color.b");SampleNear(c[3],1-0.5f*u,"color.a");
   auto st=readUv(renderer);SampleNear(st[0],1+u,"uv.scale.x");SampleNear(st[1],1+u,"uv.scale.y");SampleNear(st[2],0.5f*u,"uv.offset.x");SampleNear(st[3],0.25f*u,"uv.offset.y");
  };
  Log("[NATIVE-ANIMATION-PROBE] phase=sample target=temporary-animator-objects rendererEnabled=0 gameModelsModified=0");
  for(float time:{0.f,0.25f,0.5f,0.75f,1.f}){
   void* args[]={first.root.Get(),&time};Log("[NATIVE-ANIMATION-PROBE] phase=sample-call time=%.2f",time);Call(a.sample,clip.Get(),args);Log("[NATIVE-ANIMATION-PROBE] phase=sample-return time=%.2f",time);float u=time<=0.5f?time*2:(1-time)*2;
   auto p=Scalar<V3>(a.position,first.pivot.Get());SampleNear(p[0],u,"position.x");SampleNear(p[1],2*u,"position.y");SampleNear(p[2],3*u,"position.z");
   SampleNear(Scalar<V4>(a.rotation,first.pivot.Get())[1],std::sin(3.14159265f*u/4),"rotation.y");verify(first.renderer,u);
   int index=0;void* shapeArgs[]={&index};SampleNear(Scalar<float>(a.shape,other.renderer.Get(),shapeArgs),0,"instance B shape unchanged");
   auto otherPosition=Scalar<V3>(a.position,other.pivot.Get());for(float value:otherPosition)SampleNear(value,0,"instance B position unchanged");
   auto otherColor=read(other.renderer,colorName.Get(),a.getColor,a.blockColor,V4{});SampleNear(otherColor[0],0.2f,"instance B color unchanged");
   SampleNear(read(other.renderer,floatName.Get(),a.getFloat,a.blockFloat,float{}),2,"instance B material float unchanged");
   auto otherUv=readUv(other.renderer);for(size_t i=0;i<4;++i)SampleNear(otherUv[i],i<2?1.f:0.f,"instance B UV unchanged");
   void* c[]={colorName.Get()};SampleNear(Scalar<V4>(a.getColor,material.Get(),c)[0],0.2f,"shared Material unchanged");void* f[]={floatName.Get()};SampleNear(Scalar<float>(a.getFloat,material.Get(),f),2,"shared Material float unchanged");
   Log("[NATIVE-ANIMATION-PROBE] phase=sample-result time=%.2f cumulativeMismatches=%zu",time,mismatches.size());
  }
  Require(mismatches.empty(),"native Clip sample mismatch count="+std::to_string(mismatches.size()));
 }
};
void Frame(){
 if(attempted||!NativeBundle::enabled.load()||!RuntimeShapes::enabled.load())return;
 auto path=gameDirectory/L"plugin/diagnostics/native-animation-probe/mixed-animation.ab";
 std::error_code error;if(!std::filesystem::is_regular_file(path,error))return;
 attempted=true;
 try{
  Require(RuntimeShapes::Main(),"probe requires Unity main thread");
  Log("[NATIVE-ANIMATION-PROBE] phase=preflight modelBindingRequired=0 luaRequired=0");
  auto api=Bind();{Run run(std::move(api));run.Execute(path);Require(run.Close(),"probe cleanup failed");}
  Log("[NATIVE-ANIMATION-PROBE] result=PASS nativeClip=1 samples=5 transform=1 shape=1 materialFloat=1 materialColor=1 uv=1 instanceIsolation=1");
 }catch(const std::exception& failure){Log("[NATIVE-ANIMATION-PROBE] result=FAIL reason=%s",failure.what());}
 catch(...){Log("[NATIVE-ANIMATION-PROBE] result=FAIL reason=unknown");}
}
}

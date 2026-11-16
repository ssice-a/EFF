#pragma once
// Included inside FullResourceReload. A UI weapon owns a raw prefab handle
// independently of the world's BaseModelViewPart. Its native bundle list is
// authoritative even while its GameObject is hidden or not parented yet.
struct UIWeaponCollection {
 void *field=nullptr,*count=nullptr,*item=nullptr,*path=nullptr,*handle=nullptr;
 bool array=false;
};
struct UIWeaponDependenciesApi {
 Ref componentType;
 std::vector<UIWeaponCollection> collections;
} uiWeaponDependenciesApi;

void BindUIWeaponDependencies(void* klass){
 Require(klass,"UI weapon owner type unavailable");
 UIWeaponDependenciesApi bound;bound.componentType=Type(klass);
 for(const char* name:{"m_weaponBundles","m_staticWeaponBundles"}){
  UIWeaponCollection row;row.field=Field(klass,name,nullptr);
  auto type=il2cpp_field_get_type(row.field),collection=il2cpp_class_from_type(type);
  const auto code=il2cpp_type_get_type(type);row.array=code==0x14||code==0x1d;
  void* element=nullptr;
  if(row.array)element=il2cpp_class_get_element_class(collection);
  else{
   Require(code==0x15&&!InheritsClass(collection,"System","ValueType"),"UI weapon bundles are not a reference collection");
   row.count=Method(collection,"get_Count",{},"System.Int32",false);
   void* iterator=nullptr;while(auto method=il2cpp_class_get_methods(collection,&iterator)){
    if(strcmp(il2cpp_method_get_name(method),"get_Item")||il2cpp_method_get_param_count(method)!=1)continue;
    auto parameter=il2cpp_type_get_name(il2cpp_method_get_param(method,0));
    const bool indexed=parameter&&!strcmp(parameter,"System.Int32");if(parameter)il2cpp_free((void*)parameter);
    uint32_t impl=0;if(!indexed||(il2cpp_method_get_flags(method,&impl)&0x10))continue;
    Require(!row.item,"ambiguous UI weapon bundle indexer");row.item=method;
    auto result=il2cpp_method_get_return_type(method);
    Require(!il2cpp_type_is_byref(result),"UI weapon bundle indexer returns a byref");
    element=il2cpp_class_from_type(result);
   }
   Require(row.item&&((MInfo*)row.item)->mp,"UI weapon bundle indexer unavailable");
  }
  Require(element&&!InheritsClass(element,"System","ValueType"),"UI weapon bundle is not a managed owner");
  row.path=Field(element,"modelPath","System.String");
  row.handle=Field(element,"weaponAssetHandle","Beyond.Resource.FAssetProxyHandle");
  bound.collections.push_back(row);
 }
 uiWeaponDependenciesApi=std::move(bound);
 Log("[FULL-RELOAD-UI-WEAPON-API] nativeBundleLists=2 typedPrefabHandles=1 hiddenOwnersIncluded=1");
}

std::vector<PrefabDependency> UIWeaponDependencies(void* root){
 std::vector<PrefabDependency> result;
 Require(uiWeaponDependenciesApi.componentType.Get(),"UI weapon dependency API unavailable");
 void* componentArgs[]={uiWeaponDependenciesApi.componentType.Get()};
 auto components=Ref::Make(Call(api.directComponents,root,componentArgs),false);
 auto values=ReferenceArray(components.Get());
 uint32_t alignment=0;auto size=il2cpp_class_value_size(api.handleClass,&alignment);
 Require(size>0,"UI weapon prefab handle size differs");
 for(auto component:values)if(component)for(const auto& collection:uiWeaponDependenciesApi.collections){
  void* value=nullptr;il2cpp_field_get_value(component,collection.field,&value);if(!value)continue;
  auto held=Ref::Make(value,false);std::vector<void*> bundles;
  if(collection.array)bundles=ReferenceArray(held.Get());
  else{
   auto count=RuntimeShapes::Scalar<int>(collection.count,held.Get());Require(count>=0,"UI weapon bundle count is negative");
   for(int i=0;i<count;++i){void* args[]={&i};bundles.push_back(Call(collection.item,held.Get(),args));}
  }
  for(auto bundle:bundles)if(bundle){
   void* label=nullptr;il2cpp_field_get_value(bundle,collection.path,&label);auto path=String(label);if(path.empty())continue;
   Bytes handle(size);il2cpp_field_get_value(bundle,collection.handle,handle.data());
   auto raw=RawAssetFromHandle(handle.data());
   result.push_back({NormalizeAssetPath(path),std::move(raw)});
  }
 }
 return result;
}

#pragma once

namespace EiemModelPoolDiscard {
struct Binding {void* method=nullptr;bool unloadArgument=false;};
template<class Resolve> Binding Bind(Resolve resolve){
 if(auto method=resolve(true))return {method,true};
 return {resolve(false),false};
}
template<class Invoke> void Discard(const Binding& binding,void* manager,void* path,Invoke invoke){
 // Clearing the object pool must not force-release assets still protected by
 // game consumers or the reload transaction's source leases.
 if(binding.unloadArgument){unsigned char unloadImmediately=0;void* args[]={path,&unloadImmediately};invoke(binding.method,manager,args);}
 else{void* args[]={path};invoke(binding.method,manager,args);}
}
}

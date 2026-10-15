#include "../../src/eiem_model_pool_discard.h"
#include <iostream>
#include <stdexcept>
int main(){try{
 auto require=[](bool value,const char* reason){if(!value)throw std::runtime_error(reason);};
 for(bool updated:{false,true}){
  auto binding=EiemModelPoolDiscard::Bind([&](bool extra)->void*{return extra==updated?reinterpret_cast<void*>(23):nullptr;});
  require(binding.method==reinterpret_cast<void*>(23)&&binding.unloadArgument==updated,"updated model pool signature not bound");
  unsigned calls=0;EiemModelPoolDiscard::Discard(binding,reinterpret_cast<void*>(1),reinterpret_cast<void*>(2),[&](void* method,void* receiver,void** args){
   require(method==binding.method&&receiver==reinterpret_cast<void*>(1)&&args[0]==reinterpret_cast<void*>(2),"pool discard receiver/path changed");
   if(updated)require(args[1]&&*static_cast<unsigned char*>(args[1])==0,"pool discard forced shared asset unload");++calls;
  });require(calls==1,"pool discard invocation repeated");
 }
 auto absent=EiemModelPoolDiscard::Bind([](bool)->void*{return nullptr;});require(!absent.method,"unknown pool signature accepted");
 std::cout<<"PASS model pool discard: exact legacy/updated binding, receiver/path ABI, deferred shared-resource unloading and unknown signature rejection\n";return 0;
 }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}

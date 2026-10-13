#include <cstdlib>
#include <new>
#include <iostream>

static bool countAllocations = false;
static size_t allocations = 0;
void* operator new(size_t size) {
 if(countAllocations)++allocations;
 if(auto p=std::malloc(size?size:1))return p;
 throw std::bad_alloc();
}
void operator delete(void* p) noexcept {std::free(p);}
void operator delete(void* p,size_t) noexcept {std::free(p);}
#include "../../src/eiem_resource_directory.h"

int main(){try{
 using EiemDirectory::Require;
 const std::string dynamic="dynamic validation failure with context";
 countAllocations=true;
 for(size_t i=0;i<1000;++i){
  Require(i<1000,"successful serialized validation must not allocate an error message");
  Require(i<1000,dynamic);
 }
 countAllocations=false;
 Require(allocations==0,"successful literal validation allocated error messages");
 for(bool literal:{false,true}){
  bool rejected=false;
  try{if(literal)Require(false,"literal validation failure");else Require(false,dynamic);}
  catch(const std::runtime_error& error){rejected=true;Require(error.what()==(literal?std::string("literal validation failure"):dynamic),"validation failure message changed");}
  Require(rejected,"failed validation did not throw");
 }
 std::cout<<"PASS allocation-free successful validation and unchanged literal/dynamic failures\n";return 0;
 }catch(const std::exception& error){countAllocations=false;std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}
}

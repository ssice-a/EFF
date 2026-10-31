#include "../../src/eiem_runtime_resource_payload.h"
#include <iostream>
using namespace EiemDirectory;
namespace P=EiemRuntimePayload;

static void Put(Bytes& bytes,size_t offset,uint64_t value,size_t width){
 for(size_t i=0;i<width;++i)bytes.at(offset+i)=static_cast<unsigned char>(value>>(i*8));
}
static Resource MakeTextureResource(unsigned format,unsigned width,unsigned height,unsigned mips,size_t payloadBytes){
 Resource resource;resource.kind=4;resource.bytes.resize(80+52+payloadBytes);
 memcpy(resource.bytes.data(),"EFFRES2\0",8);resource.byteSize=resource.bytes.size();
 resource.blocks.emplace("@texture",Block{80,52,{}});
 resource.blocks.emplace("pixels",Block{132,payloadBytes,{}});
 Put(resource.bytes,80,2,4);Put(resource.bytes,84,width,4);Put(resource.bytes,88,height,4);
 Put(resource.bytes,92,format,4);Put(resource.bytes,96,mips,4);
 for(size_t i=100;i<132;++i)resource.bytes[i]=static_cast<unsigned char>(i);
 for(size_t i=132;i<resource.bytes.size();++i)resource.bytes[i]=static_cast<unsigned char>(i);
 return resource;
}
static size_t BcnBytes(unsigned width,unsigned height,unsigned mips){
 size_t total=0;for(unsigned i=0;i<mips;++i){total+=size_t(std::max(1u,(width+3)/4))*std::max(1u,(height+3)/4)*16;width=std::max(1u,width/2);height=std::max(1u,height/2);}return total;
}
int main(){try{
 for(auto format:{25u,27u}){
  auto expected=BcnBytes(5,7,3);auto resource=MakeTextureResource(format,5,7,3,expected);
  Settings settings{{"linear",format==27?"true":"false"}};
  auto texture=P::DecodeTexture(resource,settings);
  Require(texture.format==format&&texture.width==5&&texture.height==7&&texture.mips==3&&texture.pixels.size==expected,"BCn Texture fields/extent differ");
  Require(texture.linear==(format==27),"Texture color space differs");
  auto broken=MakeTextureResource(format,5,7,3,expected-1);
  try{(void)P::DecodeTexture(broken,settings);throw std::runtime_error("invalid BCn mip extent accepted");}catch(const std::runtime_error& error){Require(std::string(error.what())!="invalid BCn mip extent accepted","invalid BCn mip extent accepted");}
 }
 auto raw=MakeTextureResource(4,2,2,2,20);Put(raw.bytes,80,1,4);Put(raw.bytes,92,4,4);Put(raw.bytes,96,2,4);raw.blocks["@texture"].size=20;raw.blocks["pixels"]={100,20,{}};raw.bytes.resize(120);raw.byteSize=120;
 auto legacy=P::DecodeTexture(raw,Settings{{"linear","false"}});Require(legacy.format==4&&legacy.pixels.size==20,"RGBA32 compatibility differs");
 std::cout<<"PASS compressed Texture2D payloads BC5/BC7 odd extents, mip sizing, color-space flags and legacy RGBA32\n";return 0;
}catch(const std::exception& error){std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}}

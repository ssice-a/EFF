#include "../../src/eiem_runtime_resource_payload.h"
#include <chrono>
#include <iostream>

using namespace EiemDirectory;
namespace P=EiemRuntimePayload;
template<class F> void Reject(F f,const char* name){bool rejected=false;try{f();}catch(const std::exception&){rejected=true;}Require(rejected,std::string("invalid resource accepted: ")+name);}
int main(int argc,char** argv){try{
 Require(argc==2,"usage: runtime_resource_payload_smoke <existing Mod directory>");auto root=std::filesystem::u8path(argv[1]);auto begin=std::chrono::steady_clock::now();auto sections=ParseIni(Read(root/"mod.ini"));
 size_t meshes=0,textures=0,vertices=0,shapes=0,bytes=0,packed=0,flagged=0;std::set<std::string> files;
 for(const auto& section:sections){bool mesh=Begins(section.id,"Mesh"),texture=Begins(section.id,"Texture");if(!mesh&&!texture)continue;auto path=section.values.at("path");if(!files.insert(path).second)continue;
  auto resource=Decode(Read(root/std::filesystem::u8path(path)),mesh?1:4);bytes+=ResourceSize(resource);
  if(mesh){auto m=P::DecodeMesh(resource);++meshes;vertices+=m.vertices;shapes+=m.shapes.size();packed+=m.packedFrame;for(auto c:m.channels)flagged+=bool(c.dimension&0xf0);
   auto matrices=P::BindPoses(m);Require(matrices.size()==m.bones.size(),"matrix count differs");
   auto broken=resource;auto field=broken.blocks.at("m_IndexBuffer");std::fill_n(broken.bytes.begin()+field.offset+4,m.indexFormat?4:2,static_cast<unsigned char>(255));Reject([&]{P::DecodeMesh(broken);},"out-of-range native indices");
   broken=resource;field=broken.blocks.at("m_VertexData");broken.bytes.at(field.offset+4)=13;Reject([&]{P::DecodeMesh(broken);},"unknown vertex descriptor count");
   if(!m.bones.empty()){broken=resource;auto dataOffset=size_t(m.vertexData.data-ResourceData(resource));const auto& channel=m.channels[12];std::fill_n(broken.bytes.begin()+dataOffset+m.start[channel.stream]+channel.offset,8,static_cast<unsigned char>(0));Reject([&]{P::DecodeMesh(broken);},"zero skin weights");}
   std::cout<<"MESH "<<section.id<<" vertices="<<m.vertices<<" indices="<<m.indices<<" bones="<<m.bones.size()<<" shapes="<<m.shapes.size()<<" packed="<<m.packedFrame<<" borrowedVertexBytes="<<m.vertexData.size<<'\n';
  }else{auto t=P::DecodeTexture(resource,section.values);++textures;Require(t.pixels.data>=ResourceData(resource)&&t.pixels.data+t.pixels.size<=ResourceData(resource)+ResourceSize(resource),"pixels were not borrowed from verified resource");auto broken=resource;auto field=broken.blocks.at("@texture");broken.bytes.at(field.offset+4)^=1;Reject([&]{P::DecodeTexture(broken,section.values);},"incorrect mip extent");}
 }
 Require(meshes&&textures,"existing Mod did not cover Mesh and Texture");std::cout<<"PASS real author payloads meshFiles="<<meshes<<" textureFiles="<<textures<<" vertices="<<vertices<<" shapes="<<shapes<<" packedFrames="<<packed<<" flaggedAttributes="<<flagged<<" bytesRead="<<bytes<<" ms="<<std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count()<<" gameIndexReads=0 compiledReads=0 consumerPfbReads=0\n";return 0;
 }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}

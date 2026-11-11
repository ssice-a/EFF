#include "../../src/eiem_dds_texture.h"
#include "../../src/eiem_input_span_view.h"
#include <iostream>
using namespace EiemNativeInput;
template<class F>void Rejected(F f){bool rejected=false;try{f();}catch(const std::exception&){rejected=true;}Require(rejected,"invalid replacement DDS accepted");}
int main(){try{
 namespace D=EiemDdsTexture;
 for(auto format:{4u,25u,27u})for(bool linear:{false,true}){
  auto header=D::Header(5,7,3,format,linear);auto extent=D::MipBytes(5,7,3,format);auto layout=D::Parse(header,header.size()+extent);
  Require(layout.width==5&&layout.height==7&&layout.mips==3&&layout.format==format&&layout.linear==(format==27||linear),"DDS authoritative layout differs");
  Require(layout.offset==148&&layout.bytes==extent,"DDS encoded mip range differs");
  Rejected([&]{D::Parse(header,header.size()+extent-1);});
  auto photoshop=header;photoshop[24]=1;auto ps=D::Parse(photoshop,photoshop.size()+extent);
  Require(ps.width==layout.width&&ps.bytes==extent,"Photoshop 2D depth=1 rejected");
  auto volume=photoshop;volume[24]=2;Rejected([&]{D::Parse(volume,volume.size()+extent);});
  volume=photoshop;volume[10]|=0x80;Rejected([&]{D::Parse(volume,volume.size()+extent);});
  volume=photoshop;volume[113]=2;Rejected([&]{D::Parse(volume,volume.size()+extent);});
  auto invalid=header;invalid[140]=2;Rejected([&]{D::Parse(invalid,invalid.size()+extent);});
  invalid=header;invalid[144]=5;Rejected([&]{D::Parse(invalid,invalid.size()+extent);});
 }
 // Native metadata and editable DDS pixels are disjoint disk spans. A read
 // crossing both boundaries must have exactly the original native bytes.
 auto metadata=std::make_shared<const Bytes>(Bytes{1,2,3,4,5,6});auto dds=D::Header(4,4,1,25,false);dds.insert(dds.end(),16,0x51);size_t reads=0;
 EiemInputSpanView::View view({{0,0,3,{}},{1,148,16,{}},{0,3,3,{}}},{metadata->size(),dds.size()},[&](size_t source,size_t at,unsigned char* out,size_t n){++reads;const auto& bytes=source?dds:*metadata;memcpy(out,bytes.data()+at,n);});
 Bytes actual(view.Size());Require(reads==0,"DDS pixels were read during view preparation");view.ReadAt(0,actual.data(),actual.size());
 Bytes expected{1,2,3};expected.insert(expected.end(),16,0x51);expected.insert(expected.end(),{4,5,6});Require(actual==expected,"DDS span native reconstruction differs");
 dds[148]=0x73;view.ReadAt(3,actual.data(),1);Require(actual[0]==0x73,"replacement DDS still uses baked pixel data");
 std::cout<<"PASS editable DDS headers, source color semantics, exact mip extent, rejection and zero-copy native span delivery\n";return 0;
 }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}

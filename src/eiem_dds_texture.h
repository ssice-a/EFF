#pragma once
#include "eiem_native_input.h"

// DDS is an editable disk container. The native loader receives only the
// already GPU-encoded mip range; no image decoder or compressor runs here.
namespace EiemDdsTexture {
using namespace EiemNativeInput;
struct Layout {unsigned width=0,height=0,mips=0,format=0;bool linear=false,colorKnown=false;size_t offset=0,bytes=0;};
inline size_t MipBytes(unsigned width,unsigned height,unsigned mips,unsigned format){
 Require(width&&height&&width<=16384&&height<=16384&&mips,"DDS dimensions/mips invalid");
 unsigned maximum=1;for(auto n=std::max(width,height);n>1;n>>=1)++maximum;
 Require(mips<=maximum&&(format==4||format==25||format==27),"DDS format/mip count unsupported (use RGBA8, BC7 or BC5)");
 size_t bytes=0;for(unsigned i=0;i<mips;++i){bytes=AddSize(bytes,format==4?MultiplySize(MultiplySize(width,height),4):MultiplySize(MultiplySize(std::max(1u,(width+3)/4),std::max(1u,(height+3)/4)),16));width=std::max(1u,width/2);height=std::max(1u,height/2);}return bytes;
}
inline Layout Parse(const Bytes& header,size_t fileBytes){
 Require(header.size()>=128&&fileBytes>=128&&!memcmp(header.data(),"DDS ",4)&&Le(header,4,4)==124&&Le(header,76,4)==32,"DDS header invalid");
 auto flags=Le(header,8,4);Require((flags&0x1007)==0x1007&&!Le(header,112,4)&&!Le(header,24,4),"DDS must be one ordinary 2D texture");
 Layout result;result.height=unsigned(Le(header,12,4));result.width=unsigned(Le(header,16,4));result.mips=unsigned(Le(header,28,4));if(!result.mips)result.mips=1;
 auto pixelFlags=Le(header,80,4),fourcc=Le(header,84,4);result.offset=128;
 if((pixelFlags&4)&&fourcc==0x30315844){
  Require(header.size()>=148&&fileBytes>=148&&Le(header,132,4)==3&&Le(header,136,4)==0&&Le(header,140,4)==1&&Le(header,144,4)<=4,"DDS DX10 array/cube/dimension/alpha mode invalid");
  auto format=Le(header,128,4);result.colorKnown=true;result.linear=format!=29&&format!=99;
  result.format=format==28||format==29?4:format==83?27:format==98||format==99?25:0;result.offset=148;
 }else if((pixelFlags&4)&&(fourcc==0x32495441||fourcc==0x55354342)){result.format=27;result.colorKnown=false;}
 else if((pixelFlags&0x41)==0x41&&Le(header,88,4)==32&&Le(header,92,4)==0xff&&Le(header,96,4)==0xff00&&Le(header,100,4)==0xff0000&&Le(header,104,4)==0xff000000){result.format=4;result.colorKnown=false;}
 result.bytes=MipBytes(result.width,result.height,result.mips,result.format);
 Require(result.offset<=fileBytes&&result.bytes==fileBytes-result.offset,"DDS mip data extent differs");return result;
}
inline void Matches(const Layout& value,unsigned width,unsigned height,unsigned mips,unsigned format,bool linear){
 Require(value.width==width&&value.height==height&&value.mips==mips&&value.format==format,"replacement DDS dimensions, format or mip count differ from exported texture");
 // DDS color flags describe the encoded file. The material binding and native
 // Texture2D metadata retain the authored sampling semantics.
 (void)linear;
}
inline Bytes Header(unsigned width,unsigned height,unsigned mips,unsigned format,bool linear){
 auto bytes=MipBytes(width,height,mips,format);Require(bytes<=UINT32_MAX,"DDS mip data too large");Bytes result(148);memcpy(result.data(),"DDS ",4);
 auto put=[&](size_t at,uint32_t n){for(unsigned i=0;i<4;++i)result.at(at+i)=static_cast<unsigned char>(n>>(8*i));};
 put(4,124);put(8,0x1007|(format==4?8:0x80000)|(mips>1?0x20000:0));put(12,height);put(16,width);
 put(20,format==4?width*4:uint32_t(MipBytes(width,height,1,format)));put(28,mips);put(76,32);put(80,4);put(84,0x30315844);put(108,0x1000|(mips>1?0x400008:0));
 put(128,format==4?(linear?28:29):format==27?83:linear?98:99);put(132,3);put(140,1);return result;
}
} // namespace EiemDdsTexture

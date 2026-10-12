#pragma once
#include "eiem_input_span_view.h"

// Author-side native directory encoder. Data blocks remain references to the
// original encoded game bytes or to compiled author bytes. The DLL only reads
// the resulting spans; it does not call this writer.
namespace EiemNativeViewWriter {
using namespace EiemNativeInput;
struct Block {size_t decoded=0,encoded=0;uint16_t flags=0;};
struct Member {std::string name;uint32_t flags=0;size_t offset=0,size=0;};
struct OriginalBlock {Block block;size_t offset=0,decodedOffset=0;};
struct Directory {std::vector<OriginalBlock> blocks;std::vector<Member> members;size_t decoded=0;};
inline Directory ReadDirectory(const Bytes& input){
 auto h=EndfieldInput::ReadHeader(input);Require(h.bytes==input.size(),"offline native view original input extent differs");
 size_t infoAt=h.flags&0x80?input.size()-h.compressedInfo:h.prefix;
 Bytes info(input.begin()+infoAt,input.begin()+infoAt+h.compressedInfo);auto mode=h.flags&63;
 if(mode){Require(mode==2||mode==3,"offline native view directory mode differs");EndfieldInput::CryptBlock(info);info=Lz4(info.data(),info.size(),h.decodedInfo);}else Require(info.size()==h.decodedInfo,"offline native view directory extent differs");
 Reader r(info,0,true);Directory result;size_t offset=h.prefix;
 if(!(h.flags&0x80))offset=AddSize(offset,h.flags&0x200?AlignSize(h.compressedInfo,16):h.compressedInfo);
 auto count=EndfieldInput::Count(r,0x23F77B8A,0x91CE0A4F);Require(count<=(r.limit-r.p)/10,"offline native view block table truncated");
 for(size_t i=0;i<count;++i){auto a=uint16_t(r.Get(2)),b=uint16_t(r.Get(2)),c=uint16_t(r.Get(2)),ef=uint16_t(r.Get(2)^0x9CD6),d=uint16_t(r.Get(2));auto lo=uint8_t(ef),hi=uint8_t(ef>>8);
  Block block{EndfieldInput::Size32(a,c),EndfieldInput::Size32(b,d),uint16_t(c^EndfieldInput::Rol16(uint16_t((uint16_t(lo^hi)<<8)|lo),14)^0x523F)};
  auto end=h.flags&0x80?infoAt:input.size();Require(block.decoded&&block.encoded&&(block.flags==0||block.flags==5)&&(block.flags||block.decoded==block.encoded)&&offset<=end&&block.encoded<=end-offset,"offline native view original block extent differs");
  result.blocks.push_back({block,offset,result.decoded});offset=AddSize(offset,block.encoded);result.decoded=AddSize(result.decoded,block.decoded);
 }
 Require(offset==(h.flags&0x80?infoAt:input.size()),"offline native view original data extent differs");
 count=EndfieldInput::Count(r,0x6B0AE55D,0xE4C1D9F2);Require(count<=(r.limit-r.p)/22,"offline native view member table truncated");std::set<std::string> names;
 for(size_t i=0;i<count;++i){auto a=uint32_t(r.Get(4))^0x8E06A9F8,b=uint32_t(r.Get(4)),c=uint32_t(r.Get(4)),d=uint32_t(r.Get(4));std::string name;
  for(;;){auto byte=uint8_t(r.Get(1));if(!byte)break;byte^=uint8_t(name.size()^0x97);Require(byte&&byte<128,"offline native view member name encoding differs");name+=char(byte);}
  auto e=uint32_t(r.Get(4));auto lo=uint16_t(a),hi=uint16_t(a>>16);
  Member member{name,EndfieldInput::Rol32((uint32_t(uint16_t(hi^lo))<<16)|lo,14)^0xF13927C4^b,size_t(EndfieldInput::Size64(d,c)),size_t(EndfieldInput::Size64(b,e))};
  Require(!name.empty()&&names.insert(name).second&&member.offset<=result.decoded&&member.size<=result.decoded-member.offset,"offline native view original member extent differs");result.members.push_back(std::move(member));
 }Require(r.p==info.size(),"offline native view directory suffix");return result;
}
inline Bytes Prefix(const Bytes& sourceHeader,const std::vector<Block>& blocks,const std::vector<Member>& members){
 EndfieldInput::ReadHeader(sourceHeader);
 Require(!blocks.empty()&&!members.empty()&&blocks.size()<=UINT32_MAX&&members.size()<=UINT32_MAX,"compiled native view directory count differs");
 Bytes info;size_t encoded=0,decoded=0;
 EndfieldInput::Append(info,EndfieldInput::EncodeCount(uint32_t(blocks.size()),0x23F77B8A,0x91CE0A4F),4);
 for(const auto& block:blocks){
  Require(block.decoded&&block.encoded&&block.decoded<=UINT32_MAX&&block.encoded<=UINT32_MAX&&(block.flags==0||block.flags==5)&&(block.flags||block.decoded==block.encoded),"compiled native view block representation differs");
  encoded=AddSize(encoded,block.encoded);decoded=AddSize(decoded,block.decoded);
  auto a=EndfieldInput::Encode32(uint32_t(block.decoded)),b=EndfieldInput::Encode32(uint32_t(block.encoded));
  auto value=EndfieldInput::Rol16(uint16_t(a.second^block.flags^0x523F),2);
  auto lo=uint8_t(value),hi=uint8_t((value>>8)^lo);
  EndfieldInput::Append(info,a.first,2);EndfieldInput::Append(info,b.first,2);EndfieldInput::Append(info,a.second,2);
  EndfieldInput::Append(info,((uint16_t(hi)<<8)|lo)^0x9CD6,2);EndfieldInput::Append(info,b.second,2);
 }
 EndfieldInput::Append(info,EndfieldInput::EncodeCount(uint32_t(members.size()),0x6B0AE55D,0xE4C1D9F2),4);
 std::set<std::string> names;
 for(const auto& member:members){
  Require(!member.name.empty()&&names.insert(member.name).second&&member.offset<=decoded&&member.size<=decoded-member.offset,"compiled native view member identity/range differs");
  auto size=EndfieldInput::Encode64(member.size),offset=EndfieldInput::Encode64(member.offset);
  auto flags=EndfieldInput::Rol32(member.flags^0xF13927C4^size.first,18);
  auto low=uint16_t(flags),high=uint16_t((flags>>16)^low);
  EndfieldInput::Append(info,((uint32_t(high)<<16)|low)^0x8E06A9F8,4);
  EndfieldInput::Append(info,size.first,4);EndfieldInput::Append(info,offset.second,4);EndfieldInput::Append(info,offset.first,4);
  for(size_t j=0;j<member.name.size();++j){auto byte=(unsigned char)member.name[j],coded=uint8_t(byte^uint8_t(j^0x97));Require(byte&&byte<128&&coded,"compiled native view member name encoding differs");EndfieldInput::Append(info,coded,1);}
  EndfieldInput::Append(info,0,1);EndfieldInput::Append(info,size.second,4);
 }
 Require(info.size()<=UINT32_MAX,"compiled native view directory size representation differs");
 Bytes prefix(sourceHeader.begin(),sourceHeader.begin()+48);prefix.insert(prefix.end(),info.begin(),info.end());prefix.resize(AddSize(48,AlignSize(info.size(),16)));
 auto infoSize=EndfieldInput::Encode32(uint32_t(info.size()));auto fileSize=EndfieldInput::Encode64(AddSize(prefix.size(),encoded));
 Reader reader(sourceHeader,10,true);auto f2=uint32_t(reader.Get(4));
 EndfieldInput::Put(prefix,8,infoSize.second,2);EndfieldInput::Put(prefix,14,8^f2,4);EndfieldInput::Put(prefix,18,fileSize.second,4);
 EndfieldInput::Put(prefix,22,0x240^f2^0xA7F49310,4);EndfieldInput::Put(prefix,26,infoSize.first,2);
 EndfieldInput::Put(prefix,32,infoSize.second,2);EndfieldInput::Put(prefix,34,fileSize.first,4);EndfieldInput::Put(prefix,38,infoSize.first,2);
 return prefix;
}
} // namespace EiemNativeViewWriter

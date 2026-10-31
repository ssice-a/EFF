#pragma once
#include "eiem_author_texture_bundle.h"
#include "eiem_dds_texture.h"
#include "eiem_native_random_access.h"

namespace EiemAuthorTextureDds {
using namespace EiemNativeInput;
struct Asset {Bytes metadata,dds;size_t offset=0,pixels=0;};
inline Asset Separate(const EiemAuthorMeshBundle::Asset& input,const Texture& texture,const Resource& resource){
 auto archive=EndfieldInput::Decode(input.bytes);Require(archive.members.size()==1,"DDS texture native input must have one member");
 const auto& member=archive.members.front();const auto& native=member.bytes;
 auto prefix=EiemNativeViewWriter::Prefix(EiemAuthorMeshBundle::EnvelopeHeader(),{{native.size(),native.size(),0}},{{member.name,4,0,native.size()}});
 auto read=[&](size_t at,unsigned char* out,size_t n){if(at<prefix.size()){auto count=std::min(n,prefix.size()-at);memcpy(out,prefix.data()+at,count);at+=count;out+=count;n-=count;}if(n){auto p=at-prefix.size();Require(p<=native.size()&&n<=native.size()-p,"DDS native source range differs");memcpy(out,native.data()+p,n);}};
 EiemNativeRandomAccess::Reader reader(prefix.size()+native.size(),read);const auto& entry=reader.directory.members.front();auto layout=reader.Layout(entry);
 auto fields=reader.Ranges(entry,layout,input.meshId);const auto& image=fields.at("image data");
 const auto& pixels=resource.blocks.at("pixels");auto begin=AddSize(image.offset,4);
 Require(Le(native,image.offset,4)==pixels.size&&begin<=native.size()&&pixels.size<=native.size()-begin&&
  !memcmp(native.data()+begin,resource.bytes.data()+pixels.offset,pixels.size),"DDS native image range differs from authored mip bytes");
 Asset result;result.offset=AddSize(prefix.size(),begin);result.pixels=pixels.size;result.metadata=std::move(prefix);
 result.metadata.insert(result.metadata.end(),native.begin(),native.begin()+begin);
 result.metadata.insert(result.metadata.end(),native.begin()+begin+pixels.size,native.end());
 result.dds=EiemDdsTexture::Header(texture.width,texture.height,texture.mips,texture.format,texture.linear);
 result.dds.insert(result.dds.end(),resource.bytes.begin()+pixels.offset,resource.bytes.begin()+pixels.offset+pixels.size);
 auto dds=EiemDdsTexture::Parse(result.dds,result.dds.size());EiemDdsTexture::Matches(dds,texture.width,texture.height,texture.mips,texture.format,texture.linear);
 Require(result.metadata.size()+result.pixels==reader.header.bytes,"DDS split native extent differs");return result;
}
} // namespace EiemAuthorTextureDds

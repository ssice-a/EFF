#pragma once
#include "eiem_author_mesh_bundle.h"
namespace EiemAuthorTextureBundle {
using namespace EiemNativeInput;
inline EiemAuthorMeshBundle::Asset Build(const Texture& texture,const Resource& resource,
 const Bytes& source,const std::string& cab,int64_t id,const std::string& path,const std::string& identity){
 Require(resource.kind==4&&resource.blocks.count("pixels"),"author Texture pixels absent");
 auto layout=ParseSerialized(source);const auto& type=layout.types.at(layout.objects.at(id).type);
 Require(type.classId==28,"author Texture template is not Texture2D");auto value=ObjectValue(source,layout,id);
 const auto& pixels=resource.blocks.at("pixels");size_t expected=0;unsigned w=texture.width,h=texture.height;
 for(unsigned i=0;i<texture.mips;++i){expected=AddSize(expected,MultiplySize(MultiplySize(w,h),4));w=std::max(1u,w/2);h=std::max(1u,h/2);}
 Require(expected==pixels.size&&texture.width&&texture.height&&texture.mips,"author Texture mip byte extent differs");
 Require(ArrayValue(type,Child(type,value,"m_PlatformBlob")).count==0&&Integer(Child(type,value,"m_ImageCount"))==1&&Integer(Child(type,value,"m_TextureDimension"))==2,"author Texture template layout differs");
 for(const auto& field:std::vector<std::pair<const char*,uint64_t>>{{"m_Width",texture.width},{"m_Height",texture.height},{"m_CompleteImageSize",pixels.size},{"m_MipsStripped",0},{"m_TextureFormat",4},{"m_MipCount",texture.mips},{"m_IsPreProcessed",0},{"m_StreamingMipmaps",0},{"m_ColorSpace",texture.linear?0:1}})SetInteger(type,Child(type,value,field.first),field.second);
 auto& stream=Child(type,value,"m_StreamData");SetInteger(type,Child(type,stream,"offset"),0);SetInteger(type,Child(type,stream,"size"),0);SetString(type,Child(type,stream,"path"),"");
 auto& image=Child(type,value,"image data");Require(type.nodes.at(image.node).type=="TypelessData","author Texture inline data layout differs");image.raw.assign(resource.bytes.begin()+pixels.offset,resource.bytes.begin()+pixels.offset+pixels.size);
 auto& settings=Child(type,value,"m_TextureSettings");SetInteger(type,Child(type,settings,"m_FilterMode"),texture.filter);SetInteger(type,Child(type,settings,"m_Aniso"),texture.aniso);SetFloat(type,Child(type,settings,"m_MipBias"),texture.bias);
 for(const char* field:{"m_WrapU","m_WrapV","m_WrapW"})SetInteger(type,Child(type,settings,field),texture.wrap);
 SetString(type,Child(type,value,"m_Name"),identity);auto encoded=EncodeValue(type,value);auto hash=Sha(encoded.data(),encoded.size());
 return EiemAuthorMeshBundle::Isolate(source,cab,id,path,value,Hex(hash.data(),hash.size()),28);
}
}

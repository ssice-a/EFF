#pragma once
#include "eiem_author_mesh_bundle.h"
namespace EiemAuthorTextureBundle {
using namespace EiemNativeInput;
inline EiemAuthorMeshBundle::Asset BuildScaffold(const Bytes& source,const std::string& cab,
 int64_t id,const std::string& path,const std::string& identity){
 auto layout=ParseSerialized(source);const auto& type=layout.types.at(layout.objects.at(id).type);
 Require(type.classId==28,"author Texture template is not Texture2D");auto value=ObjectValue(source,layout,id);
 Require(ArrayValue(type,Child(type,value,"m_PlatformBlob")).count==0&&
         Integer(Child(type,value,"m_ImageCount"))==1&&
         Integer(Child(type,value,"m_TextureDimension"))==2,
         "author Texture template layout differs");
 SetInteger(type,Child(type,value,"m_CompleteImageSize"),0);
 for(const auto& field:std::vector<std::pair<const char*,uint64_t>>{
       {"m_MipsStripped",0},{"m_IsPreProcessed",0},{"m_StreamingMipmaps",0}})
   SetInteger(type,Child(type,value,field.first),field.second);
 auto& stream=Child(type,value,"m_StreamData");
 SetInteger(type,Child(type,stream,"offset"),0);SetInteger(type,Child(type,stream,"size"),0);
 SetString(type,Child(type,stream,"path"),"");
 auto& image=Child(type,value,"image data");
 Require(type.nodes.at(image.node).type=="TypelessData","author Texture inline data layout differs");
 image.raw.clear();image.rawOrigins.clear();
 SetString(type,Child(type,value,"m_Name"),identity);
 auto encoded=EncodeValue(type,value);auto hash=Sha(encoded.data(),encoded.size());
 return EiemAuthorMeshBundle::Isolate(source,cab,id,path,value,Hex(hash.data(),hash.size()),28);
}
}

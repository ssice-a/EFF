#pragma once
#include "eiem_demand_author_resources.h"
#include "eiem_demand_generation_diff.h"
#include "eiem_native_standalone_input.h"

// DDS owns the pixels. INI owns the resource identity and sampler. The current
// game's Texture2D schema supplies only the native serialization structure.
namespace EiemDemandTextureBuild {
using namespace EiemNativeInput;
namespace A=EiemDemandAuthorResources;namespace R=EiemNativeRandomAccess;
namespace P=EiemNativeSerializedSplice;
inline std::shared_ptr<const A::Texture> Build(const A::Texture& author,R::Reader& reader,
    size_t memberIndex,const SerializedLayout& layout,int64_t sourceId,R::ReadRange lease){
  Require(author.dds&&!author.input,"DDS Texture builder requires an unopened native input");
  const auto& member=reader.directory.members.at(memberIndex);const auto& type=layout.types.at(layout.objects.at(sourceId).type);
  Require(type.classId==28,"logical Texture template is not Texture2D");
  std::set<std::string> selected;
  for(auto node:type.nodes[0].children){const auto& name=type.nodes[node].name;if(name!="image data")selected.insert(name);}
  auto fields=reader.Fields(member,layout,sourceId,selected);
  Require(ArrayValue(type,fields.at("m_PlatformBlob")).count==0&&Integer(fields.at("m_ImageCount"))==1&&Integer(fields.at("m_TextureDimension"))==2,
    "logical Texture template must be an ordinary 2D texture");
  const auto& d=author.replacement;Require(d.bytes<=INT_MAX,"DDS pixels exceed native Texture2D size");
  for(const auto& edit:std::vector<std::pair<const char*,uint64_t>>{{"m_Width",d.width},{"m_Height",d.height},
      {"m_MipCount",d.mips},{"m_TextureFormat",d.format},{"m_CompleteImageSize",d.bytes},
      {"m_MipsStripped",0},{"m_IsPreProcessed",0},{"m_StreamingMipmaps",0}})
    SetInteger(type,fields.at(edit.first),edit.second);
  SetString(type,fields.at("m_Name"),author.declaration.id);
  if(author.settings.hasLinear)SetInteger(type,fields.at("m_ColorSpace"),author.settings.linear?0:1);
  auto& sampling=fields.at("m_TextureSettings");SetInteger(type,Child(type,sampling,"m_FilterMode"),author.settings.filter);
  SetInteger(type,Child(type,sampling,"m_Aniso"),author.settings.aniso);SetFloat(type,Child(type,sampling,"m_MipBias"),author.settings.mipBias);
  for(const auto* name:{"m_WrapU","m_WrapV","m_WrapW"})SetInteger(type,Child(type,sampling,name),author.settings.wrap);
  auto& stream=fields.at("m_StreamData");SetString(type,Child(type,stream,"path"),"");
  SetInteger(type,Child(type,stream,"offset"),0);SetInteger(type,Child(type,stream,"size"),0);
  auto dds=author.dds;auto pixels=std::make_shared<const EiemInputSpanView::View>(
    std::vector<EiemInputSpanView::Span>{{0,d.offset,d.bytes,{}}},std::vector<size_t>{dds->Size()},
    [dds](size_t source,size_t at,unsigned char* out,size_t n){Require(!source,"DDS source index differs");dds->ReadAt(at,out,n);});
  P::RangePayload payload;size_t output=0;
  for(auto node:type.nodes[0].children){const auto& name=type.nodes[node].name;
    if(name=="image data"){
      Require(type.nodes[node].type=="TypelessData","Texture2D image schema differs");
      Bytes count;AppendInteger(count,d.bytes,4);auto literal=std::make_shared<const Bytes>(std::move(count));
      payload.pieces.push_back({0,4,literal,{}});payload.pieces.push_back({0,d.bytes,{},pixels});output+=4+d.bytes;
      if(type.nodes[node].align&&(output&3)){auto pad=std::make_shared<const Bytes>(4-(output&3),0);payload.pieces.push_back({0,pad->size(),pad,{}});output+=pad->size();}
    }else{Bytes encoded(output&3);WriteValue(type,fields.at(name),encoded);encoded.erase(encoded.begin(),encoded.begin()+(output&3));
      auto literal=std::make_shared<const Bytes>(std::move(encoded));payload.pieces.push_back({0,literal->size(),literal,{}});output+=literal->size();}
  }
  if(type.nodes[0].align&&(output&3)){auto pad=std::make_shared<const Bytes>(4-(output&3),0);payload.pieces.push_back({0,pad->size(),pad,{}});}
  EiemRuntimeAuthorDirectory::Writer identity;identity.S("eff-dds-texture/1");identity.S(author.ownerRoot.u8string());
  identity.S(author.declaration.id);identity.S(author.declaration.ddsFile);identity.S(author.source.path);identity.S(author.source.asset);
  identity.S(EiemDemandGenerationDiff::FileFingerprint(A::AssetPath(author.ownerRoot,author.declaration.ddsFile)));
  identity.S(member.name);identity.N(uint64_t(sourceId));identity.b.insert(identity.b.end(),type.nativeRecord.begin(),type.nativeRecord.end());
  for(const auto& piece:payload.pieces)if(piece.bytes)identity.b.insert(identity.b.end(),piece.bytes->begin(),piece.bytes->end());
  auto digest=Sha(identity.b.data(),identity.b.size());auto native=EiemNativeStandaloneInput::Build(reader,memberIndex,layout,sourceId,layout,
    std::move(payload),Hex(digest.data(),digest.size()),{},std::move(lease));
  auto result=std::make_shared<A::Texture>(author);result->input=native.view;result->cab=native.cab;result->id=native.asset.second;result->name=author.declaration.id;
  result->metadataReadBytes+=reader.stats.encodedBytes;dds->VerifyStable();return result;
}
}

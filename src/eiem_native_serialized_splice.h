#pragma once
#include "eiem_native_splice_view.h"

// Produce a native SerializedFile from metadata and object ranges. Unchanged
// objects are source references; callers need read/encode only changed objects.
// This is input preparation, with no Unity object construction or load calls.
namespace EiemNativeSerializedSplice {
using namespace EiemNativeInput;
namespace S=EiemNativeSpliceView;
using Payload=std::shared_ptr<const Bytes>;
struct Added {int64_t id=0;size_t type=0;Payload bytes;};
struct RangePayload {
  std::vector<S::Piece> pieces;
  size_t Size() const {size_t size=0;for(const auto& piece:pieces)size=AddSize(size,piece.size);return size;}
};
struct AddedRanges {int64_t id=0;size_t type=0;RangePayload payload;};
struct Plan {
  S::Rewrite rewrite;
  size_t size=0,retainedBytes=0,changedBytes=0,metadataBytes=0;
};
inline Plan BuildRanges(EiemNativeRandomAccess::Reader& reader,size_t memberIndex,const SerializedLayout& layout,
    const std::map<int64_t,RangePayload>& replacements,const std::vector<AddedRanges>& added={},
    const std::set<int64_t>& removed={},const std::vector<Bytes>& addedTypes={},const std::vector<Bytes>& addedScripts={}) {
  const auto& member=reader.directory.members.at(memberIndex);
  Require(layout.dataOffset<=member.size,"native object plan metadata exceeds member");
  auto prefix=reader.ReadAt(member.offset,layout.dataOffset,reader.limits.metadataBytes);
  auto native=ParseSerialized(prefix,member.size);
  // Reject a layout accidentally taken from another member or a stale source.
  Require(native.dataOffset==layout.dataOffset&&native.metadataSize==layout.metadataSize&&
      native.countOffset==layout.countOffset&&native.tableEnd==layout.tableEnd&&native.order==layout.order&&native.objects.size()==layout.objects.size()&&
      native.types.size()+addedTypes.size()==layout.types.size()&&ExternalPrefix(native.externals,layout.externals),"native object plan layout/source identity differs");
  for(size_t i=0;i<native.types.size();++i)Require(native.types[i].nativeRecord==layout.types[i].nativeRecord,"native object plan changed an existing type record");
  for(size_t i=0;i<addedTypes.size();++i)Require(layout.types[native.types.size()+i].nativeRecord==addedTypes[i],"native object plan added type record differs");
  for(const auto& row:native.objects){const auto& object=layout.objects.at(row.first);const auto& original=row.second;
    Require(object.offset==original.offset&&object.size==original.size&&object.type==original.type,"native object plan changed source object extent/type");}
  for(const auto& row:replacements)Require(layout.objects.count(row.first)&&!removed.count(row.first),"native object plan replacement absent/deleted");
  for(auto id:removed)Require(layout.objects.count(id),"native object plan removed object absent");
  Plan result;result.rewrite.member=memberIndex;
  bool unchanged=replacements.empty()&&added.empty()&&removed.empty()&&addedTypes.empty()&&addedScripts.empty()&&native.externals==layout.externals;
  if(unchanged){result.size=member.size;result.retainedBytes=member.size;result.rewrite.pieces.push_back({0,member.size,{}});return result;}
  auto count=AddSize(layout.order.size()-removed.size(),added.size());
  auto metadata=BuildSerializedMetadata(prefix,layout,count,addedTypes,addedScripts);
  result.size=metadata.dataOffset;result.metadataBytes=metadata.prefix.size();
  std::map<int64_t,Object> objects;
  auto original=[&](size_t at,size_t size){if(!size)return;auto& pieces=result.rewrite.pieces;
    if(!pieces.empty()&&!pieces.back().bytes&&!pieces.back().view&&AddSize(pieces.back().offset,pieces.back().size)==at)pieces.back().size=AddSize(pieces.back().size,size);
    else pieces.push_back({at,size,{}});
    result.retainedBytes=AddSize(result.retainedBytes,size);
  };
  size_t previousOriginalEnd=layout.dataOffset;
  auto append=[&](int64_t id,size_t type,size_t sourceOffset,size_t size,const RangePayload* payload){
    Require(id&&type<layout.types.size()&&size<=UINT_MAX,"native object plan identity/type/size differs");
    Require(!layout.objects.count(id)||(!payload||replacements.count(id)),"native object plan added PathID collides");
    auto aligned=AlignSize(result.size,8);
    if(aligned!=result.size){auto old=layout.objects.find(id);auto paddingSize=aligned-result.size;
      if(old!=layout.objects.end()&&old->second.offset>=previousOriginalEnd&&old->second.offset-previousOriginalEnd==paddingSize)original(previousOriginalEnd,paddingSize);
      else {auto padding=std::make_shared<const Bytes>(paddingSize,static_cast<unsigned char>(0));result.rewrite.pieces.push_back({0,padding->size(),std::move(padding)});}}
    Require(objects.emplace(id,Object{id,aligned,size,type}).second,"native object plan duplicate PathID");
    if(payload){result.changedBytes=AddSize(result.changedBytes,size);result.rewrite.pieces.insert(result.rewrite.pieces.end(),payload->pieces.begin(),payload->pieces.end());}else original(sourceOffset,size);
    result.size=AddSize(aligned,size);
    auto old=layout.objects.find(id);if(old!=layout.objects.end())previousOriginalEnd=AddSize(old->second.offset,old->second.size);
  };
  for(auto id:layout.order){if(removed.count(id))continue;const auto& object=layout.objects.at(id);auto changed=replacements.find(id);
    if(changed==replacements.end())append(id,object.type,object.offset,object.size,nullptr);
    else append(id,object.type,0,changed->second.Size(),&changed->second);
  }
  for(const auto& object:added){Require(!layout.objects.count(object.id),"native object plan added identity collides");append(object.id,object.type,0,object.payload.Size(),&object.payload);}
  size_t row=metadata.table;
  for(const auto& entry:objects){const auto& object=entry.second;
    WriteInteger(metadata.prefix,row,uint64_t(object.id),8);WriteInteger(metadata.prefix,row+8,object.offset-metadata.dataOffset,8);
    WriteInteger(metadata.prefix,row+16,object.size,4);WriteInteger(metadata.prefix,row+20,object.type,4);row+=24;
  }
  WriteInteger(metadata.prefix,24,result.size,8,true);
  auto checked=ParseSerialized(metadata.prefix,result.size);
  Require(checked.order.size()==count&&checked.externals==layout.externals&&checked.types.size()==layout.types.size()&&std::is_sorted(checked.order.begin(),checked.order.end()),"native object plan metadata validation failed");
  for(const auto& object:objects){const auto& actual=checked.objects.at(object.first);Require(actual.offset==object.second.offset&&actual.size==object.second.size&&actual.type==object.second.type,"native object plan object table validation failed");}
  auto owned=std::make_shared<const Bytes>(std::move(metadata.prefix));result.rewrite.pieces.insert(result.rewrite.pieces.begin(),{0,owned->size(),owned});return result;
}
inline Plan Build(EiemNativeRandomAccess::Reader& reader,size_t memberIndex,const SerializedLayout& layout,
    const std::map<int64_t,Payload>& replacements,const std::vector<Added>& added={},
    const std::set<int64_t>& removed={},const std::vector<Bytes>& addedTypes={},const std::vector<Bytes>& addedScripts={}) {
  std::map<int64_t,RangePayload> ranges;for(const auto& row:replacements){Require(bool(row.second),"native object plan replacement payload absent");ranges[row.first].pieces.push_back({0,row.second->size(),row.second,{}});}
  std::vector<AddedRanges> extra;for(const auto& object:added){Require(bool(object.bytes),"native object plan added payload absent");extra.push_back({object.id,object.type,{{{0,object.bytes->size(),object.bytes,{}}}}});}
  return BuildRanges(reader,memberIndex,layout,ranges,extra,removed,addedTypes,addedScripts);
}
} // namespace EiemNativeSerializedSplice

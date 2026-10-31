#pragma once
#include "eiem_native_serialized_splice.h"

// One member, one published object table. Later assembly stages read earlier
// edits through this overlay, including newly declared bones and components.
// Only edited objects are encoded; original payloads remain source ranges.
namespace EiemNativeEditTransaction {
using namespace EiemNativeInput;
namespace R=EiemNativeRandomAccess;namespace P=EiemNativeSerializedSplice;
class Member {
  R::Reader& reader_;
  size_t index_;
  SerializedLayout base_,layout_;
  std::map<int64_t,P::RangePayload> edited_;
  std::vector<int64_t> added_;
  std::vector<Bytes> scripts_;
  void ReadPayload(const P::RangePayload& payload,size_t at,unsigned char* out,size_t size) {
    auto total=payload.Size();Require(at<=total&&size<=total-at,"native staged read exceeds object");
    size_t first=0;
    for(const auto& piece:payload.pieces){
      auto end=AddSize(first,piece.size);
      if(at<end&&size){auto local=at-first,count=std::min(size,piece.size-local);
        if(piece.bytes)std::memcpy(out,piece.bytes->data()+local,count);
        else if(piece.view)Require(piece.view->ReadAt(AddSize(piece.offset,local),out,count)==count,"native staged author range truncated");
        else reader_.ReadAt(AddSize(reader_.directory.members.at(index_).offset,AddSize(piece.offset,local)),out,count);
        at+=count;out+=count;size-=count;
      }
      first=end;if(!size)break;
    }
    Require(!size,"native staged range did not cover read");
  }
  struct Cursor {
    Member& owner;const P::RangePayload& payload;size_t position=0,end=0,base=0;
    void Skip(size_t n){Require(position<=end&&n<=end-position,"native staged field exceeds object");position+=n;}
    uint32_t Count(){uint32_t n=0;Require(position<=end&&4<=end-position,"native staged count truncated");
      owner.ReadPayload(payload,position,reinterpret_cast<unsigned char*>(&n),4);position+=4;
      Require(n<=INT_MAX,"native staged count invalid");return n;}
  };
public:
  Member(R::Reader& reader,size_t index,SerializedLayout layout):reader_(reader),index_(index),base_(layout),layout_(std::move(layout)){}
  const SerializedLayout& Layout()const{return layout_;}
  const SerializedLayout& SourceLayout()const{return base_;}
  const std::string& Cab()const{return reader_.directory.members.at(index_).name;}
  Value Object(int64_t id) {
    auto staged=edited_.find(id);if(staged!=edited_.end()){
      auto size=staged->second.Size();Require(size<=reader_.limits.fieldBytes,"native staged object allocation budget exhausted; project fields instead");
      Bytes bytes(size);ReadPayload(staged->second,0,bytes.data(),size);
      return DecodeValue(layout_.types.at(layout_.objects.at(id).type),0,bytes);
    }
    return reader_.Object(reader_.directory.members.at(index_),base_,id);
  }
  Value Project(int64_t id,const std::set<std::string>& fields) {
    auto staged=edited_.find(id);
    if(staged!=edited_.end()){
      const auto& type=layout_.types.at(layout_.objects.at(id).type);
      Cursor cursor{*this,staged->second,0,staged->second.Size(),0};Value result;
      for(auto child:type.nodes.at(0).children){auto at=cursor.position;R::Reader::Skip(type,child,cursor);
        if(!fields.count(type.nodes.at(child).name))continue;
        auto size=cursor.position-at,padding=at&3;Require(size<=reader_.limits.fieldBytes,"native staged field allocation budget exhausted");
        Bytes bytes(AddSize(padding,size));ReadPayload(staged->second,at,bytes.data()+padding,size);
        EiemNativeInput::Reader input(bytes,padding);auto value=ReadValue(type,child,input,0);
        Require(input.p==bytes.size(),"native staged projected field extent differs");result.children.push_back(std::move(value));
      }
      return result;
    }
    return reader_.Project(reader_.directory.members.at(index_),base_,id,fields);
  }
  void Stage(const SerializedLayout& output,std::map<int64_t,Bytes> changes,
      std::vector<AddedObject> additions={},const std::vector<Bytes>& types={},const std::vector<Bytes>& scripts={}) {
    std::map<int64_t,P::RangePayload> ranges;std::vector<P::AddedRanges> added;
    for(auto& row:changes){auto bytes=std::make_shared<const Bytes>(std::move(row.second));ranges[row.first].pieces.push_back({0,bytes->size(),bytes,{}});}
    for(auto& row:additions){auto bytes=std::make_shared<const Bytes>(std::move(row.bytes));added.push_back({row.id,row.type,{{{0,bytes->size(),bytes,{}}}}});}
    StageRanges(output,std::move(ranges),std::move(added),types,scripts);
  }
  void StageRanges(const SerializedLayout& output,std::map<int64_t,P::RangePayload> changes,
      std::vector<P::AddedRanges> additions={},const std::vector<Bytes>& types={},const std::vector<Bytes>& scripts={}) {
    Require(output.objects.size()==layout_.objects.size()&&output.order==layout_.order&&
      output.types.size()==AddSize(layout_.types.size(),types.size())&&ExternalPrefix(layout_.externals,output.externals),"native assembly stage changed existing metadata");
    for(size_t i=0;i<layout_.types.size();++i)Require(output.types.at(i).nativeRecord==layout_.types.at(i).nativeRecord,"native assembly stage replaced existing type");
    for(size_t i=0;i<types.size();++i)Require(output.types.at(layout_.types.size()+i).nativeRecord==types[i],"native assembly added type record differs");
    for(const auto& old:layout_.objects){const auto& next=output.objects.at(old.first);Require(old.second.offset==next.offset&&old.second.size==next.size&&old.second.type==next.type,"native assembly stage changed existing object descriptor");}
    for(const auto& change:changes)Require(layout_.objects.count(change.first),"native assembly stage edits absent object");
    std::set<int64_t> ids;for(const auto& object:additions)Require(object.id&&!layout_.objects.count(object.id)&&ids.insert(object.id).second&&object.type<output.types.size(),"native assembly added object collides or type absent");
    auto validate=[&](const P::RangePayload& payload){Require(payload.Size()<=UINT_MAX,"native staged object exceeds wire size");
      for(const auto& piece:payload.pieces){Require(!piece.bytes||!piece.view,"native staged piece has two owners");
        if(piece.bytes)Require(piece.size==piece.bytes->size(),"native staged literal extent differs");
        else if(piece.view)Require(piece.offset<=piece.view->Size()&&piece.size<=piece.view->Size()-piece.offset,"native staged author range exceeds view");
        else {auto size=reader_.directory.members.at(index_).size;Require(piece.offset<=size&&piece.size<=size-piece.offset,"native staged source range exceeds member");}}
    };
    for(const auto& row:changes)validate(row.second);for(const auto& row:additions)validate(row.payload);
    for(const auto& record:scripts)Require(record.size()==12,"native assembly script width differs");
    layout_=output;
    for(auto& change:changes)edited_[change.first]=std::move(change.second);
    for(auto& object:additions){layout_.objects.emplace(object.id,EiemNativeInput::Object{object.id,0,object.payload.Size(),object.type});layout_.order.push_back(object.id);added_.push_back(object.id);edited_[object.id]=std::move(object.payload);}
    scripts_.insert(scripts_.end(),scripts.begin(),scripts.end());layout_.scriptCount=AddSize(base_.scriptCount,scripts_.size());
  }
  P::Plan Commit() {
    auto final=base_;final.types=layout_.types;final.externals=layout_.externals;
    std::map<int64_t,P::RangePayload> replacements;for(const auto& row:edited_)if(base_.objects.count(row.first))replacements.emplace(row);
    std::vector<P::AddedRanges> additions;for(auto id:added_)additions.push_back({id,layout_.objects.at(id).type,edited_.at(id)});
    std::vector<Bytes> types;for(size_t i=base_.types.size();i<final.types.size();++i)types.push_back(final.types[i].nativeRecord);
    return P::BuildRanges(reader_,index_,final,replacements,additions,{},types,scripts_);
  }
  size_t ChangedObjects()const{return edited_.size()-added_.size();}
  size_t AddedObjects()const{return added_.size();}
};
} // namespace EiemNativeEditTransaction

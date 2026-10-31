#pragma once
#include "eiem_native_random_access.h"
#include "eiem_input_span_view.h"

// Prepare only changed ranges and block edges. Whole untouched compressed
// blocks remain encoded source ranges, read later by the native loader.
namespace EiemNativeSpliceView {
using namespace EiemNativeInput;
namespace R=EiemNativeRandomAccess;
namespace V=EiemInputSpanView;
struct Edit {
  size_t member=0,offset=0,erase=0;
  std::shared_ptr<const Bytes> bytes;
};
// Source ranges are relative to the original member and may be reordered.
// A literal owns exactly size bytes. This also permits object-table writers
// to repack native objects without reading their unchanged payloads.
struct Piece {
  size_t offset=0,size=0;
  std::shared_ptr<const Bytes> bytes;
  // Independently leased author bytes, already in native wire encoding.
  // offset/size select a range without reading it during preparation.
  std::shared_ptr<const V::View> view;
};
struct Rewrite {size_t member=0;std::vector<Piece> pieces;};
struct Prepared {
  std::vector<EiemNativeViewWriter::Block> blocks;
  std::vector<EiemNativeViewWriter::Member> members;
  std::vector<V::Span> spans;
  std::vector<std::shared_ptr<const V::View>> authorViews;
  size_t sourceSize=0,size=0,decoded=0,reusedEncoded=0,literalBytes=0,streamedAuthorBytes=0;
  // The callback must own its source lease. No Reader or temporary File is
  // captured here, and preparing the view does not consume original spans.
  std::shared_ptr<const V::View> Bind(R::ReadRange source) const {
    Require(bool(source),"native splice source lease absent");
    std::vector<size_t> sizes{sourceSize};for(const auto& view:authorViews)sizes.push_back(view->Size());
    return std::make_shared<const V::View>(spans,sizes,
      [source=std::move(source),authors=authorViews](size_t id,size_t at,unsigned char* out,size_t n){
        if(!id)source(at,out,n);else Require(authors.at(id-1)->ReadAt(at,out,n)==n,"native splice author source truncated");
      });
  }
};
inline Prepared BuildRanges(R::Reader& reader,const std::vector<Rewrite>& rewrites,size_t literalBudget=64*1024*1024) {
  Prepared result;result.sourceSize=reader.header.bytes;
  if(rewrites.empty()){
    result.members=reader.directory.members;result.size=result.sourceSize;result.decoded=reader.directory.decoded;result.reusedEncoded=result.sourceSize;
    for(const auto& block:reader.directory.blocks)result.blocks.push_back(block.block);
    result.spans.push_back({0,0,result.size,{}});return result;
  }
  std::map<size_t,const Rewrite*> byMember;
  for(const auto& rewrite:rewrites){Require(rewrite.member<reader.directory.members.size()&&byMember.emplace(rewrite.member,&rewrite).second,"native splice rewrite member absent/duplicated");
    const auto& member=reader.directory.members[rewrite.member];
    for(const auto& piece:rewrite.pieces){Require(!piece.bytes||!piece.view,"native splice has two piece owners");
      if(piece.view)Require(piece.offset<=piece.view->Size()&&piece.size<=piece.view->Size()-piece.offset,"native splice author view extent differs");
      else if(piece.bytes)Require(piece.size==piece.bytes->size(),"native splice literal extent differs");
      else Require(piece.offset<=member.size&&piece.size<=member.size-piece.offset,"native splice original piece exceeds member");}}
  bool identity=true;
  for(const auto& row:byMember){const auto& pieces=row.second->pieces;const auto& member=reader.directory.members[row.first];
    identity&=pieces.size()==1&&!pieces[0].bytes&&!pieces[0].view&&pieces[0].offset==0&&pieces[0].size==member.size;}
  if(identity)return BuildRanges(reader,{},literalBudget);
  auto literal=[&](std::shared_ptr<const Bytes> bytes){if(bytes->empty())return;
    Require(result.literalBytes<=literalBudget&&bytes->size()<=literalBudget-result.literalBytes,"native splice literal budget exhausted");
    result.literalBytes+=bytes->size();result.decoded=AddSize(result.decoded,bytes->size());
    result.blocks.push_back({bytes->size(),bytes->size(),0});result.spans.push_back({V::literalSource,0,bytes->size(),std::move(bytes)});
  };
  std::map<const V::View*,size_t> authorIds;
  auto authored=[&](const Piece& piece){if(!piece.size)return;
    auto inserted=authorIds.emplace(piece.view.get(),result.authorViews.size()+1);
    if(inserted.second)result.authorViews.push_back(piece.view);
    // Native block table widths are uint32. Split large ranges without an
    // artificial author-resource size limit or an allocation proportional to it.
    for(size_t at=piece.offset,left=piece.size;left;){auto n=std::min(left,size_t(UINT32_MAX));
      result.blocks.push_back({n,n,0});result.spans.push_back({inserted.first->second,at,n,{}});
      result.streamedAuthorBytes=AddSize(result.streamedAuthorBytes,n);result.decoded=AddSize(result.decoded,n);at+=n;left-=n;
    }
  };
  auto source=[&](size_t at,size_t size){
    Require(at<=reader.directory.decoded&&size<=reader.directory.decoded-at,"native splice original range differs");
    if(!size)return;
    auto it=std::upper_bound(reader.directory.blocks.begin(),reader.directory.blocks.end(),at,[](size_t p,const auto& block){return p<block.decodedOffset;});
    Require(it!=reader.directory.blocks.begin(),"native splice original block absent");--it;
    while(size){auto local=at-it->decodedOffset,n=std::min(size,it->block.decoded-local);Require(n,"native splice original block made no progress");
      if(!it->block.flags||(!local&&n==it->block.decoded)){
        auto encoded=it->block.flags?it->block.encoded:n;auto offset=AddSize(it->offset,it->block.flags?0:local);
        result.blocks.push_back({n,encoded,it->block.flags});result.spans.push_back({0,offset,encoded,{}});result.reusedEncoded=AddSize(result.reusedEncoded,encoded);result.decoded=AddSize(result.decoded,n);
      }else literal(std::make_shared<const Bytes>(reader.ReadAt(at,n,literalBudget-result.literalBytes)));
      at+=n;size-=n;++it;
    }
  };
  std::vector<size_t> order;for(size_t i=0;i<reader.directory.members.size();++i)order.push_back(i);
  std::sort(order.begin(),order.end(),[&](size_t a,size_t b){return reader.directory.members[a].offset<reader.directory.members[b].offset;});
  result.members.resize(order.size());size_t previous=0;
  for(auto index:order){const auto& member=reader.directory.members[index];Require(member.offset>=previous,"native splice members overlap");source(previous,member.offset-previous);
    // Keep the native member alignment after variable-size edits. Original
    // inter-member bytes are retained; only the necessary padding is added.
    auto aligned=AlignSize(result.decoded,16);if(aligned!=result.decoded)literal(std::make_shared<const Bytes>(aligned-result.decoded,static_cast<unsigned char>(0)));
    auto start=result.decoded;auto rewrite=byMember.find(index);
    if(rewrite==byMember.end())source(member.offset,member.size);
    else for(const auto& piece:rewrite->second->pieces){if(piece.view)authored(piece);else if(piece.bytes)literal(piece.bytes);else source(AddSize(member.offset,piece.offset),piece.size);}
    result.members[index]={member.name,member.flags,start,result.decoded-start};previous=AddSize(member.offset,member.size);
  }
  source(previous,reader.directory.decoded-previous);
  auto prefix=std::make_shared<const Bytes>(EiemNativeViewWriter::Prefix(reader.headerBytes,result.blocks,result.members));
  result.spans.insert(result.spans.begin(),{V::literalSource,0,prefix->size(),prefix});result.size=prefix->size();
  for(const auto& block:result.blocks)result.size=AddSize(result.size,block.encoded);
  Require(EndfieldInput::ReadHeader(*prefix).bytes==result.size,"native splice output extent differs");return result;
}
inline Prepared Build(R::Reader& reader,const std::vector<Edit>& edits,size_t literalBudget=64*1024*1024) {
  std::map<size_t,std::vector<const Edit*>> byMember;
  for(const auto& edit:edits){Require(edit.member<reader.directory.members.size()&&edit.bytes,"native splice edit member/payload absent");
    const auto& member=reader.directory.members[edit.member];Require(edit.offset<=member.size&&edit.erase<=member.size-edit.offset,"native splice edit exceeds member");
    if(edit.erase||!edit.bytes->empty())byMember[edit.member].push_back(&edit);}
  std::vector<Rewrite> rewrites;
  for(auto& entry:byMember){auto& rows=entry.second;std::sort(rows.begin(),rows.end(),[](auto a,auto b){return a->offset<b->offset;});
    Rewrite rewrite;rewrite.member=entry.first;size_t at=0,last=0;bool first=true;
    for(const auto* edit:rows){Require(first||(edit->offset>=at&&edit->offset!=last),"native splice edits overlap or share an ambiguous insertion boundary");
      rewrite.pieces.push_back({at,edit->offset-at,{}});rewrite.pieces.push_back({0,edit->bytes->size(),edit->bytes});
      last=edit->offset;at=AddSize(edit->offset,edit->erase);first=false;}
    rewrite.pieces.push_back({at,reader.directory.members[entry.first].size-at,{}});rewrites.push_back(std::move(rewrite));
  }
  return BuildRanges(reader,rewrites,literalBudget);
}
} // namespace EiemNativeSpliceView

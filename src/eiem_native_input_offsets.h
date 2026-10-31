#pragma once
#include "eiem_loading_generation.h"
#include "eiem_input_span_view.h"
#include <cstring>

// The offline member spans and namespace sites also describe the uncompressed
// native envelope. Relocation needs no object parsing or stream reconstruction.
namespace EiemNativeInputOffsets {
using namespace EiemNativeInput;
using Names=EiemLoadingGeneration::Names;
struct Member {std::string name;size_t nameOffset=0,dataOffset=0,size=0;};
struct Layout {size_t size=0,infoSize=0;std::vector<Member> members;};
inline Layout Describe(const std::vector<std::pair<std::string,size_t>>& members){
 Layout layout;size_t data=0;
 for(const auto& member:members){Require(!member.first.empty(),"native offset member name empty");data=AddSize(AlignSize(data,16),member.second);}
 Require(data&&!members.empty()&&members.size()<=UINT32_MAX,"native offset member extent differs");
 auto blocks=data/UINT32_MAX+(data%UINT32_MAX!=0);Require(blocks<=UINT32_MAX,"native offset block field overflow");
 size_t nameOffset=AddSize(48,AddSize(8,MultiplySize(blocks,10))),dataOffset=0;
 for(const auto& member:members){layout.members.push_back({member.first,AddSize(nameOffset,16),dataOffset=AlignSize(dataOffset,16),member.second});nameOffset=AddSize(nameOffset,AddSize(21,member.first.size()));dataOffset=AddSize(dataOffset,member.second);}
 layout.infoSize=nameOffset-48;Require(layout.infoSize<=UINT32_MAX,"native offset directory field overflow");
 auto prefix=AddSize(48,AlignSize(layout.infoSize,16));layout.size=AddSize(prefix,data);
 for(auto& member:layout.members)member.dataOffset=AddSize(member.dataOffset,prefix);
 return layout;
}
inline Layout Describe(const std::vector<EndfieldInput::Member>& members){
 std::vector<std::pair<std::string,size_t>> extents;for(const auto& member:members)extents.emplace_back(member.name,member.bytes.size());return Describe(extents);
}
inline std::string Name(const std::string& before,const Names& names){
 auto member=names.members.find(before);if(member!=names.members.end())return member->second;
 auto bundle=names.bundles.find(before);if(bundle!=names.bundles.end())return bundle->second;
 return EiemLoadingGeneration::RemapPath(before,names);
}
template<class Recipes> inline std::vector<EiemLoadingGeneration::Patch> Patches(const Bytes& bytes,const Layout& layout,const Recipes& recipes,const Names& from,const Names& to){
 auto header=EndfieldInput::ReadHeader(bytes);
 Require(header.flags==0x240&&header.bytes==bytes.size()&&bytes.size()==layout.size&&header.compressedInfo==layout.infoSize&&header.decodedInfo==layout.infoSize,"native offset envelope differs");
 std::map<std::string,const Member*> members;for(const auto& member:layout.members)Require(members.emplace(member.name,&member).second,"native offset duplicate member");
 std::vector<EiemLoadingGeneration::Patch> patches;std::set<std::string> seen;
 for(const auto& recipe:recipes){auto member=members.find(recipe.member);Require(member!=members.end()&&seen.insert(recipe.member).second,"native offset namespace member differs");
  for(const auto& patch:recipe.patches){Require(patch.offset<=member->second->size&&patch.before.size()<=member->second->size-patch.offset,"native offset namespace site exceeds member");
   patches.push_back({AddSize(member->second->dataOffset,patch.offset),Name(patch.before,from),Name(patch.before,to)});}
 }
 for(const auto& member:layout.members){auto before=Name(member.name,from),after=Name(member.name,to);Require(before.size()==member.name.size()&&after.size()==member.name.size(),"native offset directory name width differs");
  for(size_t j=0;j<before.size();++j){before[j]=char((unsigned char)before[j]^(unsigned char)(j^0x97));after[j]=char((unsigned char)after[j]^(unsigned char)(j^0x97));}
  patches.push_back({member.nameOffset,std::move(before),std::move(after)});
 }
 return patches;
}
template<class Recipes> inline Bytes Relocate(const Bytes& bytes,const Layout& layout,const Recipes& recipes,const Names& from,const Names& to){
 auto patches=Patches(bytes,layout,recipes,from,to);
 // Immutable input ownership establishes the content hash; all exact field
 // preimages, widths and overlap still get checked before writing a new copy.
 return EiemLoadingGeneration::ApplySites(bytes,std::move(patches));
}
template<class Recipes> inline void RelocateInPlace(Bytes& bytes,const Layout& layout,const Recipes& recipes,const Names& from,const Names& to){
 auto patches=Patches(bytes,layout,recipes,from,to);EiemLoadingGeneration::ApplySitesInPlace(bytes,std::move(patches));
}
template<class Recipes> inline std::shared_ptr<const EiemInputSpanView::View> RelocateView(
 const std::shared_ptr<const Bytes>& bytes,const Layout& layout,const Recipes& recipes,const Names& from,const Names& to){
 Require(bool(bytes),"native offset view backing absent");auto patches=Patches(*bytes,layout,recipes,from,to);EiemLoadingGeneration::ValidateSites(*bytes,patches);
 std::vector<EiemInputSpanView::Span> spans;spans.reserve(patches.size()*2+1);auto literals=std::make_shared<Bytes>();size_t literalSize=0;
 for(const auto& patch:patches)if(patch.before!=patch.after)literalSize=AddSize(literalSize,patch.after.size());literals->reserve(literalSize);
 size_t position=0;
 for(const auto& patch:patches){if(patch.before==patch.after)continue;if(patch.offset>position){EiemInputSpanView::Span span;span.source=0;span.offset=position;span.size=patch.offset-position;spans.push_back(std::move(span));}
  if(!patch.after.empty()){EiemInputSpanView::Span span;span.source=EiemInputSpanView::literalSource;span.offset=literals->size();span.size=patch.after.size();literals->insert(literals->end(),patch.after.begin(),patch.after.end());span.literal=literals;spans.push_back(std::move(span));}
  position=patch.offset+patch.before.size();
 }
 if(position<bytes->size()){EiemInputSpanView::Span span;span.source=0;span.offset=position;span.size=bytes->size()-position;spans.push_back(std::move(span));}
 std::vector<size_t> sourceSizes{bytes->size()};auto read=[bytes](size_t source,size_t offset,unsigned char* output,size_t count){Require(source==0&&offset<=bytes->size()&&count<=bytes->size()-offset,"native offset view source range differs");std::memcpy(output,bytes->data()+offset,count);};
 auto view=std::make_shared<const EiemInputSpanView::View>(std::move(spans),sourceSizes,std::move(read));Require(view->Size()==bytes->size(),"native offset view extent differs");return view;
}
} // namespace EiemNativeInputOffsets

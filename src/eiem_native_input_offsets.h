#pragma once
#include "eiem_loading_generation.h"

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
template<class Recipes> inline Bytes Relocate(const Bytes& bytes,const Layout& layout,const Recipes& recipes,const Names& from,const Names& to){
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
 // Immutable input ownership establishes the content hash; all exact field
 // preimages, widths and overlap still get checked before writing a new copy.
 return EiemLoadingGeneration::ApplySites(bytes,std::move(patches));
}
} // namespace EiemNativeInputOffsets

#pragma once
#include "eiem_serialized_writer.h"

// Expand only preload entries already used by a native input. Each entry's
// dependencies are explicit CAB/PathID pairs; FileIDs belong to the destination
// SerializedFile and must never be copied from a different CAB.
namespace EiemNativeInput {
using NativePreloadExpansion=std::map<NativeReference,std::vector<NativeReference>>;
inline bool ExpandNativePreloads(const Type& type,Value& bundle,SerializedLayout& layout,
    const std::string& cab,const NativePreloadExpansion& expansion,std::set<NativeReference>& referenced) {
  auto& preloads=ArrayValue(type,Child(type,bundle,"m_PreloadTable"));
  Require(preloads.count==preloads.children.size(),"native preload table extent differs");
  auto reference=[&](const Value& pointer){auto path=int64_t(Integer(Child(type,pointer,"m_PathID")));
    auto file=Integer(Child(type,pointer,"m_FileID"));Require(file<=layout.externals.size(),"native preload external index differs");
    return NativeReference{file?layout.externals[size_t(file)-1]:cab,path};};
  // Index only relevant identities. Copying a complete dependency set for
  // every overlapping PFB interval would multiply memory by container count.
  std::map<NativeReference,std::vector<size_t>> positions;
  for(const auto& row:expansion)for(const auto& dependency:row.second){
    Require(!dependency.first.empty()&&dependency.second,"native preload dependency is null");positions.emplace(dependency,std::vector<size_t>{});}
  for(size_t i=0;i<preloads.children.size();++i){auto found=positions.find(reference(preloads.children[i]));if(found!=positions.end())found->second.push_back(i);}
  struct Interval {Value* info;size_t first,count;std::set<NativeReference> inserted;};std::vector<Interval> intervals;
  auto interval=[&](Value& info){auto first=Integer(Child(type,info,"preloadIndex")),count=Integer(Child(type,info,"preloadSize"));
    Require(first<=preloads.children.size()&&count<=preloads.children.size()-first,"native source preload interval differs");
    intervals.push_back({&info,size_t(first),size_t(count),{}});};
  for(const char* name:{"m_Container","m_HashContainer"})
    for(auto& entry:ArrayValue(type,Child(type,bundle,name)).children)interval(Child(type,entry,"second"));
  interval(Child(type,bundle,"m_MainAsset"));
  std::vector<Value> output;std::vector<size_t> offsets{0};
  for(size_t i=0;i<preloads.children.size();++i){const auto& pointer=preloads.children[i];
    output.push_back(pointer);auto original=reference(pointer);
    if(original.second){auto found=expansion.find(original);
      if(found!=expansion.end()){
        referenced.insert(original);std::set<NativeReference> inserted{original};
        std::vector<Interval*> covering;for(auto& range:intervals)if(i>=range.first&&i-range.first<range.count)covering.push_back(&range);
        for(const auto& dependency:found->second){Require(!dependency.first.empty()&&dependency.second,"native preload dependency is null");
          if(!inserted.insert(dependency).second)continue;
          const auto& locations=positions.at(dependency);bool present=true;
          for(auto* range:covering){auto at=std::lower_bound(locations.begin(),locations.end(),range->first);
            present&=range->inserted.count(dependency)||(at!=locations.end()&&*at-range->first<range->count);}
          if(present)continue;
          auto value=ArrayElement(type,preloads);SetReference(type,value,dependency,layout,cab);output.push_back(std::move(value));
          for(auto* range:covering)range->inserted.insert(dependency);
        }
      }
    }
    offsets.push_back(output.size());
  }
  if(output.size()==preloads.count)return false;
  for(auto& range:intervals){SetInteger(type,Child(type,*range.info,"preloadIndex"),offsets[range.first]);
    SetInteger(type,Child(type,*range.info,"preloadSize"),offsets[range.first+range.count]-offsets[range.first]);}
  preloads.children=std::move(output);preloads.count=preloads.children.size();return true;
}
} // namespace EiemNativeInput

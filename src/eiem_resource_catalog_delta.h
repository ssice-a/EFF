#pragma once
#include "eiem_consumer_update_records.h"

// Mounted catalog input only: the adapter must supply the game's resolved
// logical entries and content identities, not an assumed disk-mount order.
// No file reads, Unity calls or per-frame work occur in this differencer.
namespace EiemResourceCatalogDelta {
using namespace EiemNativeInput;
struct Entry {std::string content,location;std::set<std::string> dependencies;};
struct Catalog {std::string domain;std::map<std::string,Entry> entries;};
struct Difference {std::set<std::string> added,changed,removed,relocated,affected;};
inline void Validate(const Catalog& catalog){Require(!catalog.domain.empty(),"mounted catalog domain absent");
 for(const auto& row:catalog.entries){Require(!row.first.empty()&&!row.second.content.empty()&&!row.second.location.empty(),"mounted source identity incomplete");
  for(const auto& dependency:row.second.dependencies)Require(dependency!=row.first&&catalog.entries.count(dependency),"mounted source dependency missing/self-referencing");}
}
inline Difference Compare(const Catalog& previous,const Catalog& current){Validate(previous);Validate(current);Difference result;
 for(const auto& row:current.entries){auto old=previous.entries.find(row.first);if(old==previous.entries.end()){result.added.insert(row.first);continue;}
  if(previous.domain!=current.domain||row.second.content!=old->second.content||row.second.dependencies!=old->second.dependencies)result.changed.insert(row.first);
  else if(row.second.location!=old->second.location)result.relocated.insert(row.first);
 }
 for(const auto& row:previous.entries)if(!current.entries.count(row.first))result.removed.insert(row.first);
 result.affected=result.added;result.affected.insert(result.changed.begin(),result.changed.end());result.affected.insert(result.removed.begin(),result.removed.end());
 std::map<std::string,std::set<std::string>> reverse;for(const auto* catalog:{&previous,&current})for(const auto& row:catalog->entries)for(const auto& dep:row.second.dependencies)reverse[dep].insert(row.first);
 std::vector<std::string> queue(result.affected.begin(),result.affected.end());while(!queue.empty()){auto source=queue.back();queue.pop_back();auto found=reverse.find(source);if(found==reverse.end())continue;
  for(const auto& owner:found->second)if(result.affected.insert(owner).second)queue.push_back(owner);
 }return result;
}
inline Bytes Encode(const Catalog& catalog){Validate(catalog);EiemOfflinePackage::Writer w;w.S("EFF_MOUNTED_SOURCE_CATALOG/content+location+dependencies/le64");w.S(catalog.domain);w.N(catalog.entries.size());
 for(const auto& row:catalog.entries){w.S(row.first);w.S(row.second.content);w.S(row.second.location);w.Set(row.second.dependencies);}return w.b;
}
inline Catalog Decode(const Bytes& bytes){EiemOfflinePackage::Reader r{bytes};Require(r.S()=="EFF_MOUNTED_SOURCE_CATALOG/content+location+dependencies/le64","mounted source catalog structure differs");Catalog result;result.domain=r.S();auto n=r.Count();
 for(size_t i=0;i<n;++i){auto source=r.S();Entry entry;entry.content=r.S();entry.location=r.S();entry.dependencies=r.Set();Require(result.entries.emplace(std::move(source),std::move(entry)).second,"mounted source catalog duplicate");}
 Require(r.p==bytes.size()&&Encode(result)==bytes,"mounted source catalog suffix/noncanonical");return result;
}
// Reverse dependency filtering has already supplied candidates. Neither a
// completed directory nor an unchanged file excuses an absent content decision.
inline std::set<std::string> Pending(const std::map<std::string,EiemConsumerUpdateRecords::Stamp>& candidates,const Catalog& previous,const Catalog& current,const EiemConsumerUpdateRecords::Decisions& completed){
 auto difference=Compare(previous,current);std::set<std::string> pending;
 for(const auto& candidate:candidates){Require(current.entries.count(candidate.first),"candidate absent from mounted catalog");EiemConsumerUpdateRecords::Validate(candidate.second);auto old=completed.find(candidate.first);
  if(difference.affected.count(candidate.first)||old==completed.end()||!EiemConsumerUpdateRecords::Valid(old->second,candidate.second))pending.insert(candidate.first);
 }return pending;
}
} // namespace EiemResourceCatalogDelta

#pragma once
#include "eiem_offline_resource_package.h"

// Completed content decisions are functional incremental-build state, not a
// Unity-object cache. Positive and negative results have identical validity.
namespace EiemConsumerUpdateRecords {
using namespace EiemNativeInput;
struct Stamp {std::string source,dependencies,rules,algorithm;};
inline bool operator==(const Stamp& a,const Stamp& b){return std::tie(a.source,a.dependencies,a.rules,a.algorithm)==std::tie(b.source,b.dependencies,b.rules,b.algorithm);}
struct Root {std::string path,cab;int64_t id=0;std::set<int64_t> renderers;};
struct Decision {Stamp stamp;std::vector<Root> roots;};
using Decisions=std::map<std::string,Decision>;
inline bool Valid(const Decision& record,const Stamp& stamp){return record.stamp==stamp;}
inline void Validate(const Stamp& stamp){Require(DigestText(stamp.source)&&DigestText(stamp.dependencies)&&DigestText(stamp.rules)&&!stamp.algorithm.empty(),"consumer decision validity incomplete");}
inline Bytes Encode(const Decisions& records){EiemOfflinePackage::Writer w;w.S("EFF_CONSUMER_DECISIONS/content+dependencies+rules/le64");w.N(records.size());
 for(const auto& row:records){Require(!row.first.empty(),"consumer decision source identity empty");const auto& d=row.second;Validate(d.stamp);w.S(row.first);w.S(d.stamp.source);w.S(d.stamp.dependencies);w.S(d.stamp.rules);w.S(d.stamp.algorithm);w.N(d.roots.size());
  std::set<std::tuple<std::string,std::string,int64_t>> seen;for(const auto& root:d.roots){Require(!root.path.empty()&&!root.cab.empty()&&root.id&&!root.renderers.empty()&&seen.emplace(root.path,root.cab,root.id).second,"consumer root incomplete/duplicated");w.S(root.path);w.S(root.cab);w.N(uint64_t(root.id));w.N(root.renderers.size());for(auto id:root.renderers){Require(id!=0,"consumer Renderer identity empty");w.N(uint64_t(id));}}
 }return w.b;
}
inline Decisions Decode(const Bytes& bytes){EiemOfflinePackage::Reader r{bytes};Require(r.S()=="EFF_CONSUMER_DECISIONS/content+dependencies+rules/le64","consumer decision structure differs");Decisions records;auto n=r.Count();
 for(size_t i=0;i<n;++i){auto source=r.S();Decision d;d.stamp={r.S(),r.S(),r.S(),r.S()};auto roots=r.Count();for(size_t j=0;j<roots;++j){Root root;root.path=r.S();root.cab=r.S();root.id=int64_t(r.N());auto count=r.Count();for(size_t k=0;k<count;++k)Require(root.renderers.insert(int64_t(r.N())).second,"consumer Renderer duplicated");d.roots.push_back(std::move(root));}Require(records.emplace(source,std::move(d)).second,"consumer source decision duplicated");}
 Require(r.p==bytes.size()&&Encode(records)==bytes,"consumer decision suffix/noncanonical records");return records;
}
// Candidate stamps come from the current catalog/dependency filter. Sources
// absent from the completed directory snapshot remain unknown and are queued.
inline std::set<std::string> Pending(const std::map<std::string,Stamp>& candidates,const Decisions& completed){std::set<std::string> pending;
 for(const auto& row:candidates){Validate(row.second);auto old=completed.find(row.first);if(old==completed.end()||!Valid(old->second,row.second))pending.insert(row.first);}return pending;
}
} // namespace EiemConsumerUpdateRecords

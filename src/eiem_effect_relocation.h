#pragma once
#include "eiem_effect_native_types.h"
#include "eiem_input_span_view.h"

// Author-side logical references are fixed-width native slots. Resolve only
// those resources when this effect is demanded, before Unity deserialization.
// The payload remains immutable; a small overlay owns the changed addresses.
namespace EiemEffectRelocation {
using namespace EiemNativeInput;
namespace V=EiemInputSpanView;
using Reference=std::pair<std::string,int64_t>;
using Resolver=std::function<Reference(const EiemEffectNative::GameResource&)>;
using AuthorResolver=std::function<Reference(const EiemEffectNative::AuthorResource&)>;
template<class Resource,class Resolve> inline void Collect(const V::View& source,size_t bundle,
    const std::vector<Resource>& resources,const Resolve& resolve,std::map<size_t,Bytes>& patches){
 for(const auto& resource:resources){
  if(std::none_of(resource.patches.begin(),resource.patches.end(),[&](const auto& p){return p.bundle==bundle;}))continue;
  auto target=resolve(resource);
  if(target.first.rfind("cab-",0)==0)target.first.replace(0,4,"CAB-");
  Require(target.first.size()==36&&target.first.rfind("CAB-",0)==0&&target.second&&
      target.first.find_first_not_of("0123456789abcdefABCDEF",4)==std::string::npos,
      "effect logical resource returned an invalid native identity");
  for(const auto& site:resource.patches){if(site.bundle!=bundle)continue;Require(site.kind<=1,"effect relocation patch kind differs");
   Bytes before,after;if(!site.kind){before.assign(resource.relocationCab.begin(),resource.relocationCab.end());after.assign(target.first.begin(),target.first.end());}
   else {for(unsigned i=0;i<8;++i){before.push_back(static_cast<unsigned char>(resource.relocationId>>(8*i)));after.push_back(static_cast<unsigned char>(uint64_t(target.second)>>(8*i)));}}
   Require(before.size()==after.size()&&site.offset<=source.Size()&&before.size()<=source.Size()-site.offset,"effect relocation range differs");
   Bytes actual(before.size());Require(source.ReadAt(site.offset,actual.data(),actual.size())==actual.size()&&actual==before,
       "effect relocation source slot differs");
   auto inserted=patches.emplace(site.offset,after);Require(inserted.second||inserted.first->second==after,"conflicting effect resource relocations");
  }
 }
}
inline std::shared_ptr<const V::View> Bind(std::shared_ptr<const V::View> source,size_t bundle,
    const std::vector<EiemEffectNative::GameResource>& resources,const Resolver& resolve,
    const std::vector<EiemEffectNative::AuthorResource>& authors={},const AuthorResolver& resolveAuthor={}){
 Require(bool(source),"effect relocation input absent");std::map<size_t,Bytes> patches;
 Collect(*source,bundle,resources,resolve,patches);
 Collect(*source,bundle,authors,resolveAuthor,patches);
 if(patches.empty())return source;
 std::vector<V::Span> spans;size_t at=0;
 for(auto& patch:patches){Require(patch.first>=at,"overlapping effect resource relocations");if(patch.first>at)spans.push_back({0,at,patch.first-at,{}});
  auto literal=std::make_shared<const Bytes>(std::move(patch.second));spans.push_back({V::literalSource,0,literal->size(),literal});at=AddSize(patch.first,literal->size());}
 if(at<source->Size())spans.push_back({0,at,source->Size()-at,{}});
 return std::make_shared<const V::View>(std::move(spans),std::vector<size_t>{source->Size()},
   [source](size_t id,size_t offset,unsigned char* out,size_t n){Require(!id&&source->ReadAt(offset,out,n)==n,"effect relocation source truncated");});
}
}

#pragma once
#include "eiem_input_span_view.h"
#include "eiem_loading_generation.h"
#include "eiem_resource_recipe.h"
#include "eiem_readonly_file_range.h"

namespace EiemCompiledInputView {
using namespace EiemNativeInput;
struct Site {size_t span=0,offset=0;std::string before;bool directory=false;};
// Original addresses encoded input bytes, not decoded CAB bytes. Author and
// ExpandedFrame address the author's immutable buffers; only literal sites
// carry equal-width generation names. No runtime decoder/writer is needed.
struct Recipe {
 std::vector<EiemResourceRecipe::Span> spans;
 std::vector<Site> names;
 size_t size=0;
 std::string sourceHash;
 std::string startupHash;
};
inline std::string Remap(const std::string& before,const EiemLoadingGeneration::Names& names){
 auto member=names.members.find(before);if(member!=names.members.end())return member->second;
 auto bundle=names.bundles.find(before);if(bundle!=names.bundles.end())return bundle->second;
 return EiemLoadingGeneration::RemapPath(before,names);
}
using Buffer=EiemReadonlyFileRange::Buffer;
using Buffers=std::map<std::pair<unsigned,std::string>,Buffer>;
inline std::string Identity(const Recipe& recipe,const EiemLoadingGeneration::Names& names){
 Require(!recipe.startupHash.empty(),"compiled view has no offline byte proof");
 std::string value="compiled-span-identity\n"+recipe.startupHash;
 for(const auto& site:recipe.names){auto after=Remap(site.before,names);value+='\n'+std::to_string(site.span)+":"+std::to_string(site.offset)+":"+std::to_string(site.directory)+":"+std::to_string(after.size())+":"+after;}
 auto hash=Sha((const unsigned char*)value.data(),value.size());return Hex(hash.data(),hash.size());
}
inline std::shared_ptr<const EiemInputSpanView::View> Bind(const Recipe& recipe,const Buffers& sources,
 const EiemLoadingGeneration::Names& names={}){
 std::vector<Buffer> buffers;std::map<std::pair<unsigned,std::string>,size_t> ids;
 auto add=[&](unsigned kind,const std::string& name){auto found=ids.find({kind,name});if(found!=ids.end())return found->second;auto bytes=sources.at({kind,name});Require(bool(bytes),"compiled view buffer ownership absent");auto id=buffers.size();buffers.push_back(std::move(bytes));ids.emplace(std::make_pair(kind,name),id);return id;};
 // Index name sites once rather than scanning every site for every literal span.
 std::vector<size_t> heads(recipe.spans.size(),SIZE_MAX),next(recipe.names.size(),SIZE_MAX);
 for(size_t n=recipe.names.size();n-->0;){auto span=recipe.names[n].span;Require(span<recipe.spans.size()&&recipe.spans[span].kind==EiemResourceRecipe::Literal,"compiled view namespace must address a literal");next[n]=heads[span];heads[span]=n;}
 std::vector<EiemInputSpanView::Span> spans;spans.reserve(recipe.spans.size());size_t size=0;
 for(size_t i=0;i<recipe.spans.size();++i){const auto& source=recipe.spans[i];EiemInputSpanView::Span span;span.offset=source.offset;span.size=source.size;size=AddSize(size,span.size);
  switch(source.kind){
  case EiemResourceRecipe::Original:Require(source.source.empty(),"compiled view original source identity differs");span.source=add(source.kind,source.source);break;
  case EiemResourceRecipe::Author:case EiemResourceRecipe::ExpandedFrame:span.source=add(source.kind,source.source);break;
  case EiemResourceRecipe::Literal:{auto bytes=std::make_shared<Bytes>(source.literal);Require(source.source.empty()&&!source.offset&&bytes->size()==source.size,"compiled view literal extent differs");
   for(size_t n=heads[i];n!=SIZE_MAX;n=next[n]){const auto& site=recipe.names[n];auto after=Remap(site.before,names);Require(after.size()==site.before.size()&&site.offset<=bytes->size()&&site.before.size()<=bytes->size()-site.offset,"compiled view namespace extent differs");
    for(size_t j=0;j<site.before.size();++j){auto mask=site.directory?uint8_t(j^0x97):uint8_t(0);Require((*bytes)[site.offset+j]==(uint8_t(site.before[j])^mask),"compiled view namespace preimage differs");(*bytes)[site.offset+j]=uint8_t(after[j])^mask;}
   }span.literal=std::move(bytes);break;}
  default:Require(false,"compiled view source kind differs");
  }spans.push_back(std::move(span));
 }
 Require(size==recipe.size,"compiled native view total extent differs");
 for(const auto& site:recipe.names)Require(site.span<recipe.spans.size()&&recipe.spans[site.span].kind==EiemResourceRecipe::Literal,"compiled view namespace must address literal bytes");
 std::vector<size_t> sizes;for(const auto& buffer:buffers)sizes.push_back(buffer.Size());
 return std::make_shared<const EiemInputSpanView::View>(std::move(spans),sizes,[buffers=std::move(buffers)](size_t source,size_t offset,unsigned char* output,size_t count){buffers.at(source).Read(offset,output,count);});
}
} // namespace EiemCompiledInputView

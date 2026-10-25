#pragma once
#include <algorithm>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

// Explicit compiler provenance. Sources are immutable author buffers owned by
// the surrounding compile; these descriptors never enter the game format.
// Equal bytes alone do not establish a resource dependency.
namespace EiemAuthorBytes {
using Bytes=std::vector<unsigned char>;
struct Span {
 size_t output=0;
 std::string resource;
 size_t offset=0,size=0;
 const Bytes* bytes=nullptr;
 std::shared_ptr<const Bytes> owned;
};
using Spans=std::vector<Span>;
inline void Require(bool valid,const char* message){if(!valid)throw std::runtime_error(message);}
inline size_t End(size_t begin,size_t size){Require(size<=std::numeric_limits<size_t>::max()-begin,"author origin extent overflow");return begin+size;}
inline void ValidateOrder(const Spans& spans){
 size_t end=0;for(const auto& span:spans){
  Require(span.size&&span.output>=end,"author origins overlap or are unordered");end=End(span.output,span.size);
  Require(!span.resource.empty()&&span.bytes&&span.offset<=span.bytes->size()&&span.size<=span.bytes->size()-span.offset,"author origin source range differs");
 }
}
inline void ValidateAt(const Bytes& encoded,size_t begin,size_t size,const Spans& spans){
 Require(begin<=encoded.size()&&size<=encoded.size()-begin,"author origin validation range differs");
 ValidateOrder(spans);for(const auto& span:spans){
  Require(span.output<=size&&span.size<=size-span.output,"author origin output range differs");
  Require(std::equal(encoded.begin()+begin+span.output,encoded.begin()+begin+span.output+span.size,span.bytes->begin()+span.offset),"author origin bytes differ from explicit source");
 }
}
inline void Validate(const Bytes& encoded,const Spans& spans){ValidateAt(encoded,0,encoded.size(),spans);}
inline void Append(Spans& output,const Span& span){
 Require(span.size&&span.bytes&&!span.resource.empty()&&span.offset<=span.bytes->size()&&span.size<=span.bytes->size()-span.offset,"author origin append source range differs");
 End(span.output,span.size);
 if(!output.empty()){
  auto& previous=output.back();auto end=End(previous.output,previous.size);
  Require(span.output>=end,"author origin append overlaps previous range");
  if(span.output==end&&previous.resource==span.resource&&previous.bytes==span.bytes&&End(previous.offset,previous.size)==span.offset){previous.size=End(previous.size,span.size);if(span.owned)previous.owned=span.owned;return;}
 }
 output.push_back(span);
}
inline void Append(Spans& output,const Spans& spans){for(const auto& span:spans)Append(output,span);}
inline Spans MakeRange(const std::string& resource,const Bytes& bytes,size_t offset,size_t size,size_t output=0){
 Spans result;if(size)Append(result,Span{output,resource,offset,size,&bytes});return result;
}
inline Spans MakeRange(const std::string& resource,std::shared_ptr<const Bytes> bytes,size_t offset,size_t size,size_t output=0){
 Require(bool(bytes),"author owned origin source absent");Spans result;if(size){auto pointer=bytes.get();Append(result,Span{output,resource,offset,size,pointer,std::move(bytes)});}return result;
}
inline Spans Slice(const Spans& spans,size_t begin,size_t size,size_t output=0){
 Spans result;auto end=End(begin,size);End(output,size);
 // Inputs are kept ordered by Append. Start at the first intersecting range
 // without rescanning thousands of earlier fields for every decoded leaf.
 auto found=std::lower_bound(spans.begin(),spans.end(),begin,[](const Span& span,size_t position){return End(span.output,span.size)<=position;});
 for(;found!=spans.end()&&found->output<end;++found){
  auto first=std::max(begin,found->output),last=std::min(end,End(found->output,found->size));
  if(first<last){auto span=*found;span.output=End(output,first-begin);span.offset=End(found->offset,first-found->output);span.size=last-first;Append(result,span);}
 }
 return result;
}
inline Spans Matching(const Bytes& encoded,const Spans& spans){
 ValidateOrder(spans);Spans result;
 for(const auto& span:spans){
  // Direct raw writes may invalidate just one range. Never keep stale
  // provenance, and never discover a replacement by searching equal bytes.
  if(span.output<=encoded.size()&&span.size<=encoded.size()-span.output&&
     std::equal(encoded.begin()+span.output,encoded.begin()+span.output+span.size,span.bytes->begin()+span.offset))Append(result,span);
 }
 return result;
}
} // namespace EiemAuthorBytes

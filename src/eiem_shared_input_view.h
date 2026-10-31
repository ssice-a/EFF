#pragma once
#include "eiem_compiled_input_view.h"
#include "eiem_native_view_writer.h"

// Composition owns small native records, while resource streams remain ranges
// of the immutable game/author buffers. No pixel stream is materialized here.
namespace EiemSharedInputView {
using namespace EiemNativeInput;
struct Stream {uint32_t flags=0;size_t size=0;std::vector<EiemResourceRecipe::Span> spans;};
struct Prepared {EiemCompiledInputView::Recipe recipe;EiemCompiledInputView::Buffers buffers;};
inline EiemResourceRecipe::Span Literal(Bytes bytes){auto size=bytes.size();return {EiemResourceRecipe::Literal,{},0,size,std::move(bytes)};}
inline void Add(Stream& stream,EiemResourceRecipe::Span span){if(!span.size)return;stream.size=AddSize(stream.size,span.size);stream.spans.push_back(std::move(span));}
inline const unsigned char* Data(const EiemCompiledInputView::Buffer& buffer){return buffer.bytes?buffer.bytes->data():buffer.file->Data();}
inline EiemResourceRecipe::Span Original(const std::string& name,const Bytes& bytes,Prepared& output){
 auto key="original/"+name;if(!output.buffers.count({EiemResourceRecipe::Author,key})){auto source=std::make_shared<const Bytes>(bytes);output.buffers.emplace(std::make_pair(EiemResourceRecipe::Author,key),source);}return {EiemResourceRecipe::Author,key,0,bytes.size(),{}};
}
template<class Artifact> inline void Append(Stream& stream,const std::shared_ptr<const Artifact>& artifact,size_t mod,const EiemResourceRecipe::Member& recipe,const Bytes& original,const std::map<std::string,const EndfieldInput::Member*>& originals,Prepared& output){
 Require(recipe.size>=original.size(),"compiled stream changes its original prefix");size_t position=0;
 for(const auto& piece:recipe.spans){Require(piece.size&&piece.size<=recipe.size-position,"shared stream recipe extent differs");EiemCompiledInputView::Buffer buffer;std::string key="mod/"+std::to_string(mod)+"/"+std::to_string(piece.kind)+"/"+piece.source;if(piece.kind==EiemResourceRecipe::Literal)key+="/"+recipe.name+"/"+std::to_string(position);
  switch(piece.kind){case EiemResourceRecipe::Original:{auto source=originals.find(piece.source);Require(source!=originals.end(),"shared original stream absent");Original(piece.source,source->second->bytes,output);buffer=output.buffers.at({EiemResourceRecipe::Author,"original/"+piece.source});break;}
  case EiemResourceRecipe::Author:{const auto&r=artifact->manifest.package.resources.at(piece.source);buffer=r->mapped?EiemCompiledInputView::Buffer(r->mapped):EiemCompiledInputView::Buffer(std::shared_ptr<const Bytes>(r,&r->bytes));break;}
  case EiemResourceRecipe::ExpandedFrame:buffer=EiemCompiledInputView::Buffer(std::shared_ptr<const Bytes>(artifact,&artifact->preparedFrames.at(piece.source)));break;
  case EiemResourceRecipe::Literal:Require(piece.offset==0&&piece.literal.size()==piece.size,"shared stream literal extent differs");buffer=EiemCompiledInputView::Buffer(std::shared_ptr<const Bytes>(artifact,&piece.literal));break;
  default:Require(false,"shared stream source kind differs");}
  Require(piece.offset<=buffer.Size()&&piece.size<=buffer.Size()-piece.offset,"shared stream source range differs");if(position<original.size()){auto count=std::min(piece.size,original.size()-position);Require(std::memcmp(Data(buffer)+piece.offset,original.data()+position,count)==0,"compiled stream changes its original prefix");}
  if(position+piece.size>original.size()){auto skip=position<original.size()?original.size()-position:0;auto count=piece.size-skip;output.buffers.emplace(std::make_pair(EiemResourceRecipe::Author,key),buffer);Add(stream,{EiemResourceRecipe::Author,key,piece.offset+skip,count,{}});}
  position+=piece.size;
 }Require(position==recipe.size,"shared stream recipe coverage differs");
}
template<class Namespaces> inline void Finish(const Bytes& input,std::vector<EndfieldInput::Member> members,const std::map<std::string,Stream>& streams,const Namespaces& namespaces,Prepared& output){
 using namespace EiemResourceRecipe;auto& recipe=output.recipe;recipe.spans={Literal({})};size_t logical=0;std::vector<EiemNativeViewWriter::Member> directory;
 auto add=[&](Span span){logical=AddSize(logical,span.size);recipe.spans.push_back(std::move(span));};
 for(auto& member:members){auto aligned=AlignSize(logical,16);if(aligned!=logical)add(Literal(Bytes(aligned-logical)));auto offset=logical;auto stream=streams.find(member.name);
  if(stream!=streams.end()){for(const auto&span:stream->second.spans)add(span);directory.push_back({member.name,member.flags,offset,stream->second.size});continue;}
  auto span=recipe.spans.size();for(const auto&n:namespaces)if(n.member==member.name)for(const auto&patch:n.patches)recipe.names.push_back({span,patch.offset,patch.before,false});auto size=member.bytes.size();if(size)add(Literal(std::move(member.bytes)));directory.push_back({member.name,member.flags,offset,size});
 }
 std::vector<EiemNativeViewWriter::Block> blocks;for(size_t p=0;p<logical;){auto size=std::min(logical-p,size_t(UINT32_MAX));blocks.push_back({size,size,0});p+=size;}
 recipe.spans[0]=Literal(EiemNativeViewWriter::Prefix(input,blocks,directory));const auto&prefix=recipe.spans[0].literal;auto header=EndfieldInput::ReadHeader(prefix);EiemNativeInput::Reader r(prefix,48,true,48+header.compressedInfo);auto count=EndfieldInput::Count(r,0x23F77B8A,0x91CE0A4F);r.Skip(MultiplySize(count,10));count=EndfieldInput::Count(r,0x6B0AE55D,0xE4C1D9F2);Require(count==directory.size(),"shared native directory count differs");for(const auto&member:directory){r.Skip(16);recipe.names.push_back({0,r.p,member.name,true});r.Skip(member.name.size()+5);}recipe.size=AddSize(prefix.size(),logical);Require(recipe.size==header.bytes,"shared native view extent differs");
}
} // namespace EiemSharedInputView

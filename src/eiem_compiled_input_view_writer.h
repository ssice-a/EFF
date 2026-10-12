#pragma once
#include "eiem_compiled_mod.h"
#include "eiem_compiled_input_view.h"
#include "eiem_native_view_writer.h"

// Author-only. Rebuild serialized metadata once; unchanged object bodies and
// original resource streams address the game's intact compressed blocks.
namespace EiemCompiledInputViewWriter {
using namespace EiemCompiledMod;
using EiemCompiledInputView::Recipe;
struct Window {const EndfieldInput::Member* edited=nullptr;const EndfieldInput::Member* original=nullptr;EiemNativeViewWriter::Member entry;const SerializedLayout* layout=nullptr;Bytes metadata;};
inline Recipe Build(size_t index,const Plan& plan,const std::map<std::string,MemberProof>& proofs,const Bytes& input,
 const EndfieldInput::Bundle& source,const EndfieldInput::Bundle& edited,
 const EiemResourceRecipe::Dictionary& author){
 auto directory=EiemNativeViewWriter::ReadDirectory(input);
 std::map<std::string,const EndfieldInput::Member*> old;for(const auto& member:source.members)old.emplace(member.name,&member);
 std::map<std::string,EiemNativeViewWriter::Member> originalEntries;for(const auto& member:directory.members)originalEntries.emplace(member.name,member);
 std::vector<Window> windows;size_t metadataSize=0;
 for(const auto& member:edited.members)if(Serialized(member.bytes)){
  Window window;window.edited=&member;window.original=old.at(member.name);window.layout=&proofs.at(member.name).after;
  Require(window.layout->dataOffset<=member.bytes.size(),"compiled view proven metadata extent differs");
  window.metadata.assign(member.bytes.begin(),member.bytes.begin()+window.layout->dataOffset);
  metadataSize=AlignSize(metadataSize,16);window.entry={member.name,member.flags,metadataSize,0};metadataSize=AddSize(metadataSize,window.metadata.size());windows.push_back(std::move(window));
 }
 metadataSize=AlignSize(metadataSize,16);Require(metadataSize,"compiled view lacks serialized metadata");
 Recipe result;result.sourceHash=plan.originals.at(index).hash;
 // Slot zero receives the fully encoded header/directory after all positions
 // are known. Slot one contains only rewritten serialized metadata.
 result.spans.push_back({EiemResourceRecipe::Literal,{},{},0,{}});
 result.spans.push_back({EiemResourceRecipe::Literal,{},{},metadataSize,Bytes(metadataSize)});
 std::vector<EiemNativeViewWriter::Block> blocks{{metadataSize,metadataSize,0}};
 for(const auto& block:directory.blocks){blocks.push_back(block.block);result.spans.push_back({EiemResourceRecipe::Original,{},block.offset,block.block.encoded,{}});}
 size_t logical=AddSize(metadataSize,directory.decoded);std::vector<EiemNativeViewWriter::Member> members;
 EiemResourceRecipe::Dictionary empty({});
 auto literal=[&](Bytes bytes){if(bytes.empty())return SIZE_MAX;size_t n=result.spans.size();result.spans.push_back({EiemResourceRecipe::Literal,{},0,bytes.size(),std::move(bytes)});logical=AddSize(logical,result.spans.back().size);return n;};
 auto append=[&](const Bytes& bytes){if(bytes.empty())return;EndfieldInput::Member member{"",0,bytes};auto recipe=EiemResourceRecipe::Describe(member,empty,author);
  for(auto& span:recipe.spans){logical=AddSize(logical,span.size);result.spans.push_back(std::move(span));}
 };
 auto align=[&]{auto next=AlignSize(logical,16);if(next!=logical)literal(Bytes(next-logical));};
 // Appended stream bytes remain shared data, not embedded in CAB object
 // records. Their new offsets are computed here, before the DLL sees them.
 std::map<std::string,size_t> appendedStreams;
 for(const auto& member:edited.members)if(!Serialized(member.bytes)){
  auto before=old.find(member.name);size_t originalSize=before==old.end()?0:before->second->bytes.size();
  Require(member.bytes.size()>=originalSize&&(before==old.end()||std::equal(before->second->bytes.begin(),before->second->bytes.end(),member.bytes.begin())),"compiled view stream is not an append-only extension");
  align();auto suffix=logical;appendedStreams.emplace(member.name,suffix);
  append(Bytes(member.bytes.begin()+originalSize,member.bytes.end()));
  auto original=originalEntries.find(member.name);auto begin=original==originalEntries.end()?suffix:AddSize(metadataSize,original->second.offset);
  members.push_back({member.name,member.flags,begin,0});
 }
 for(auto& window:windows){const auto& member=*window.edited;const auto& proof=proofs.at(member.name);const auto& layout=*window.layout;const auto& recipes=plan.namespaceRecipes.at(index);
  auto names=std::find_if(recipes.begin(),recipes.end(),[&](const auto& recipe){return recipe.member==member.name;});EiemLoadingGeneration::Recipe calculated;const std::vector<EiemLoadingGeneration::Patch>* patches=nullptr;
  if(names!=recipes.end())patches=&names->patches;else{EiemLoadingGeneration::PrepareSerialized(member.bytes,plan.names,&calculated);patches=&calculated.patches;}
  for(const auto& patch:*patches)if(patch.offset<window.metadata.size())result.names.push_back({1,AddSize(window.entry.offset,patch.offset),patch.before,false});
  for(size_t row=0;row<layout.order.size();++row){auto id=layout.order[row];const auto& object=layout.objects.at(id);std::vector<EiemLoadingGeneration::Patch> objectNames;
   for(const auto& patch:*patches)if(patch.offset>=object.offset&&patch.offset-object.offset<object.size){Require(patch.before.size()<=object.size-(patch.offset-object.offset),"compiled view namespace crosses an object boundary");objectNames.push_back({patch.offset-object.offset,patch.before,{}});}
   auto before=proof.before.objects.find(id);bool unchanged=objectNames.empty()&&before!=proof.before.objects.end()&&before->second.size==object.size&&std::equal(member.bytes.begin()+object.offset,member.bytes.begin()+object.offset+object.size,window.original->bytes.begin()+before->second.offset);
   size_t position=0;
   if(unchanged)position=AddSize(metadataSize,AddSize(originalEntries.at(member.name).offset,before->second.offset));
   else{Bytes bytes(member.bytes.begin()+object.offset,member.bytes.begin()+object.offset+object.size);
    for(const auto& site:proof.streams)if(site.object==id){auto original=old.find(site.member);auto size=original==old.end()?0:original->second->bytes.size();if(site.value<size)continue;
     auto stream=std::find_if(members.begin(),members.end(),[&](const auto& member){return member.name==site.member;});Require(stream!=members.end(),"compiled view stream relocation target absent");
     auto offset=AddSize(appendedStreams.at(site.member),size_t(site.value)-size);Require(offset>=stream->offset,"compiled view relocated stream offset precedes stream");WriteInteger(bytes,site.offset,offset-stream->offset,site.width);
    }
    align();position=logical;std::sort(objectNames.begin(),objectNames.end(),[](const auto& a,const auto& b){return a.offset<b.offset;});size_t cursor=0;
    for(const auto& patch:objectNames){Require(patch.offset>=cursor&&std::equal(patch.before.begin(),patch.before.end(),bytes.begin()+patch.offset),"compiled view namespace overlap/preimage differs");append(Bytes(bytes.begin()+cursor,bytes.begin()+patch.offset));auto span=literal(Bytes(bytes.begin()+patch.offset,bytes.begin()+patch.offset+patch.before.size()));result.names.push_back({span,0,patch.before,false});cursor=patch.offset+patch.before.size();}
    append(Bytes(bytes.begin()+cursor,bytes.end()));
   }
   Require(position>=metadataSize,"compiled view object precedes shared original data");WriteInteger(window.metadata,layout.tableStart+row*24+8,position-metadataSize,8);
  }
 }
 for(auto& window:windows){window.entry.size=logical-window.entry.offset;WriteInteger(window.metadata,24,window.entry.size,8,true);WriteInteger(window.metadata,32,metadataSize-window.entry.offset,8,true);
  std::copy(window.metadata.begin(),window.metadata.end(),result.spans[1].literal.begin()+window.entry.offset);members.push_back(std::move(window.entry));}
 for(auto& member:members)if(!member.size)member.size=logical-member.offset;
 // Follow the observed source chunk granularity, independent of recipe span
 // boundaries. Many tiny literal/author spans still form ordinary data blocks.
 auto chunkSize=directory.blocks.front().block.decoded;
 size_t appended=logical-AddSize(metadataSize,directory.decoded);while(appended){auto size=std::min(appended,chunkSize);blocks.push_back({size,size,0});appended-=size;}
 result.spans[0].literal=EiemNativeViewWriter::Prefix(input,blocks,members);result.spans[0].size=result.spans[0].literal.size();
 // Directory name payloads use the format's position-dependent XOR grammar.
 const auto& prefix=result.spans[0].literal;auto h=EndfieldInput::ReadHeader(prefix);EiemNativeInput::Reader reader(prefix,48,true,48+h.compressedInfo);auto count=EndfieldInput::Count(reader,0x23F77B8A,0x91CE0A4F);reader.Skip(MultiplySize(count,10));count=EndfieldInput::Count(reader,0x6B0AE55D,0xE4C1D9F2);Require(count==members.size(),"compiled view output member count differs");
 for(const auto& member:members){reader.Skip(16);auto offset=reader.p;reader.Skip(member.name.size()+1+4);result.names.push_back({0,offset,member.name,true});}
 for(const auto& span:result.spans)result.size=AddSize(result.size,span.size);Require(result.size==h.bytes,"compiled view encoded span extent differs");
 EiemCompiledInputView::Buffers buffers;buffers.emplace(std::make_pair(EiemResourceRecipe::Original,std::string{}),std::shared_ptr<const Bytes>(&input,[](const Bytes*){}));
 for(const auto& buffer:author.buffers)buffers.emplace(std::make_pair(buffer.kind,buffer.name),std::shared_ptr<const Bytes>(buffer.bytes,[](const Bytes*){}));
 result.startupHash=EiemInputSpanView::Digest(*EiemCompiledInputView::Bind(result,buffers));return result;
}
inline Recipe Build(size_t index,const Artifact& artifact,const Bytes& input,const EndfieldInput::Bundle& source,const EndfieldInput::Bundle& edited,const EiemResourceRecipe::Dictionary& author){
 return Build(index,artifact.manifest.plan,artifact.members,input,source,edited,author);
}
} // namespace EiemCompiledInputViewWriter

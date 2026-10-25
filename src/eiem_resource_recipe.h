#pragma once
#include "eiem_mesh_frame_storage.h"
#include <unordered_map>

// Offline-proven reconstruction. Large bytes remain in the game's members or
// author resources. The descriptor carries spans and small changed metadata.
namespace EiemResourceRecipe {
using namespace EiemNativeInput;
enum Kind : unsigned { Original, Author, ExpandedFrame, Literal };
struct Span {Kind kind=Literal;std::string source;size_t offset=0,size=0;Bytes literal;};
struct Member {std::string name,hash;uint32_t flags=0;size_t size=0;std::vector<Span> spans;};
struct Frame {std::string resource,hash;size_t offset=0,size=0,count=0;std::array<size_t,4> stride{},start{};};
inline std::string Digest(const Bytes& bytes){auto hash=Sha(bytes.data(),bytes.size());return Hex(hash.data(),hash.size());}
inline Bytes ExpandFrame(const Frame& frame,const Bytes& resource){
 Require(frame.count&&frame.stride[0]==16&&frame.start[0]==0&&frame.offset<=resource.size()&&frame.size<=resource.size()-frame.offset,"recipe author frame range differs");
 MeshVertexLayout after;after.count=frame.count;after.stride[0]=40;
 for(size_t s=0;s<4;++s){
  Require(frame.start[s]<=frame.size&&MultiplySize(frame.count,frame.stride[s])<=frame.size-frame.start[s],"recipe frame stream range differs");
  if(s)after.stride[s]=frame.stride[s];after.start[s]=after.bytes;
  after.bytes=AlignSize(AddSize(after.bytes,MultiplySize(after.count,after.stride[s])),16);
 }
 Bytes bytes(after.bytes);const auto* input=resource.data()+frame.offset;
 for(size_t v=0;v<frame.count;++v){uint32_t word=0;std::memcpy(&word,input+v*16+12,4);auto decoded=DecodePackedMeshFrame(word);std::memcpy(bytes.data()+v*40,input+v*16,12);std::memcpy(bytes.data()+v*40+12,decoded.data(),28);}
 for(size_t s=1;s<4;++s)std::memcpy(bytes.data()+after.start[s],input+frame.start[s],frame.count*frame.stride[s]);
 Require(frame.hash.empty()||Digest(bytes)==frame.hash,"recipe expanded author frame digest differs");return bytes;
}
inline std::vector<Frame> DescribeFrames(const Package& package,const std::map<std::array<unsigned char,16>,Type>& schemas){
 std::vector<Frame> frames;std::set<std::string> seen;
 for(const auto& mesh:package.meshes){if(mesh.second.shapes.empty()||!seen.insert(mesh.second.resource).second)continue;
  const auto& resource=*package.resources.at(mesh.second.resource);const auto& type=schemas.at(resource.type);size_t node=SIZE_MAX;
  for(auto child:type.nodes.at(0).children)if(type.nodes.at(child).name=="m_VertexData")node=child;
  Require(node!=SIZE_MAX,"recipe Mesh vertex field absent");const auto& block=resource.blocks.at("m_VertexData");
  Bytes field(resource.bytes.begin()+block.offset,resource.bytes.begin()+block.offset+block.size);auto vertex=DecodeValue(type,node,field);auto layout=DescribeMeshVertexStorage(type,vertex);
  const auto& channels=ArrayValue(type,Child(type,vertex,"m_Channels")).children;
  if(layout.stride[0]==40&&Integer(Child(type,channels.at(1),"dimension"))==3&&Integer(Child(type,channels.at(2),"dimension"))==4)continue;
  Require(layout.stride[0]==16&&Integer(Child(type,channels.at(1),"dimension"))==0x31,"recipe shaped frame convention differs");
  const auto& raw=Child(type,vertex,"m_DataSize").raw;auto data=std::search(field.begin(),field.end(),raw.begin(),raw.end());Require(!raw.empty()&&data!=field.end(),"recipe author vertex bytes absent");
  Frame frame;frame.resource=mesh.second.resource;frame.offset=block.offset+size_t(data-field.begin());frame.size=raw.size();frame.count=layout.count;
  for(size_t s=0;s<4;++s){frame.stride[s]=layout.stride[s];frame.start[s]=layout.start[s];}
  frame.hash=Digest(ExpandFrame(frame,resource.bytes));frames.push_back(std::move(frame));
 }return frames;
}
struct Buffer {Kind kind=Original;std::string name;const Bytes* bytes=nullptr;};
struct Location {size_t buffer=0,offset=0;};
inline uint64_t Anchor(const unsigned char* bytes){uint64_t value=1469598103934665603ull;for(size_t i=0;i<32;++i)value=(value^bytes[i])*1099511628211ull;return value;}
struct Dictionary {
 std::vector<Buffer> buffers;std::unordered_map<uint64_t,Location> anchors;
 std::vector<uint64_t> prefixFilter;size_t prefixMask=0;
 static uint64_t Prefix(const unsigned char* bytes){uint64_t value=0;std::memcpy(&value,bytes,8);value^=value>>33;value*=0xff51afd7ed558ccdull;value^=value>>33;value*=0xc4ceb9fe1a85ec53ull;return value^(value>>33);}
 explicit Dictionary(std::vector<Buffer> sources):buffers(std::move(sources)){
  for(size_t i=0;i<buffers.size();++i){const auto& bytes=*buffers[i].bytes;if(bytes.size()<32)continue;
   for(size_t p=0;p<=bytes.size()-32;p+=64)anchors.emplace(Anchor(bytes.data()+p),Location{i,p});
  }
  // Negative-only filter: a candidate still uses the original 32-byte hash,
  // anchor choice and exact comparison. False positives only cost a lookup.
  if(!anchors.empty()){
   size_t bits=64;auto wanted=MultiplySize(anchors.size(),16);while(bits<wanted)bits=MultiplySize(bits,2);
   prefixFilter.resize(bits/64);prefixMask=bits-1;
   for(const auto& entry:anchors){const auto& location=entry.second;auto hash=Prefix(buffers[location.buffer].bytes->data()+location.offset);auto a=size_t(hash)&prefixMask,b=size_t(hash>>32)&prefixMask;prefixFilter[a/64]|=uint64_t(1)<<(a%64);prefixFilter[b/64]|=uint64_t(1)<<(b%64);}
  }
 }
 bool MayMatch(const unsigned char* bytes)const{
  if(prefixFilter.empty())return false;auto hash=Prefix(bytes);auto a=size_t(hash)&prefixMask,b=size_t(hash>>32)&prefixMask;
  return (prefixFilter[a/64]&(uint64_t(1)<<(a%64)))&&(prefixFilter[b/64]&(uint64_t(1)<<(b%64)));
 }
 size_t Match(const Bytes& bytes,size_t offset,uint64_t hash,Location& location)const{
  auto found=anchors.find(hash);if(found==anchors.end())return 0;const auto& source=*buffers[found->second.buffer].bytes;auto limit=std::min(bytes.size()-offset,source.size()-found->second.offset);size_t length=0;
  // Most hits cover a complete author stream. Compare long equal runs in
  // blocks, then find the exact first different byte without changing spans.
  const auto* left=bytes.data()+offset;const auto* right=source.data()+found->second.offset;
  while(limit-length>=1024&&std::memcmp(left+length,right+length,1024)==0)length+=1024;
  while(limit-length>=64&&std::memcmp(left+length,right+length,64)==0)length+=64;
  while(length<limit&&left[length]==right[length])++length;
  if(length<32)return 0;location=found->second;return length;
 }
};
// Transient provenance, never part of the compiled file. Callers can identify
// untouched object bodies and original stream prefixes without byte searching.
struct OriginalSpan {size_t output=0,buffer=0,offset=0,size=0;};
inline Member Describe(const EndfieldInput::Member& output,const Dictionary& original,const Dictionary& author,
                       const std::vector<OriginalSpan>& known={},bool explicitOrigins=false){
 size_t end=0;
 for(const auto& span:known){
  Require(span.buffer<original.buffers.size(),"recipe original provenance buffer absent");const auto& bytes=*original.buffers[span.buffer].bytes;
  Require(span.size&&span.output>=end&&span.output<=output.bytes.size()&&span.size<=output.bytes.size()-span.output&&span.offset<=bytes.size()&&span.size<=bytes.size()-span.offset,"recipe original provenance range differs");
  Require(std::equal(output.bytes.begin()+span.output,output.bytes.begin()+span.output+span.size,bytes.begin()+span.offset),"recipe original provenance content differs");end=span.output+span.size;
 }
 Member member;member.name=output.name;member.flags=output.flags;member.size=output.bytes.size();member.hash=Digest(output.bytes);
 auto literal=[&](unsigned char byte){if(member.spans.empty()||member.spans.back().kind!=Literal)member.spans.push_back({});auto& span=member.spans.back();span.literal.push_back(byte);++span.size;};
 if(explicitOrigins)EiemAuthorBytes::Validate(output.bytes,output.origins);
 size_t provenance=0,authored=0;
 for(size_t p=0;p<output.bytes.size();){size_t length=0;Location old,edit;const Dictionary* dictionary=nullptr;Location location;
  while(provenance<known.size()&&known[provenance].output+known[provenance].size<=p)++provenance;
  if(explicitOrigins&&authored<output.origins.size()&&output.origins[authored].output==p){
   const auto& span=output.origins[authored++];auto kind=Author;auto source=span.resource;
   if(source.rfind("@frame:",0)==0){kind=ExpandedFrame;source=source.substr(7);}else if(source.rfind("@bounds:",0)==0)kind=ExpandedFrame;
   member.spans.push_back({kind,std::move(source),span.offset,span.size,{}});p+=span.size;continue;
  }
  if(provenance<known.size()&&known[provenance].output<=p){const auto& span=known[provenance];dictionary=&original;location={span.buffer,span.offset+p-span.output};length=span.output+span.size-p;}
  else if(output.bytes.size()-p>=32){auto bytes=output.bytes.data()+p;auto maybeOld=original.MayMatch(bytes),maybeEdit=!explicitOrigins&&author.MayMatch(bytes);
   if(maybeOld||maybeEdit){auto hash=Anchor(bytes);auto a=maybeOld?original.Match(output.bytes,p,hash,old):0,b=maybeEdit?author.Match(output.bytes,p,hash,edit):0;if(a||b){dictionary=a>=b?&original:&author;location=a>=b?old:edit;length=std::max(a,b);if(provenance<known.size())length=std::min(length,known[provenance].output-p);}}
  }
  if(explicitOrigins&&authored<output.origins.size()&&length)length=std::min(length,output.origins[authored].output-p);
  if(!length){literal(output.bytes[p++]);continue;}const auto& source=dictionary->buffers[location.buffer];
  if(!member.spans.empty()&&member.spans.back().kind==source.kind&&member.spans.back().source==source.name&&AddSize(member.spans.back().offset,member.spans.back().size)==location.offset)member.spans.back().size=AddSize(member.spans.back().size,length);
  else member.spans.push_back({source.kind,source.name,location.offset,length,{}});p+=length;
 }return member;
}
inline Bytes Apply(const Member& member,const std::map<std::string,const Bytes*>& originals,const Package& package,const std::map<std::string,Bytes>& frames,bool verify=true,const std::map<std::string,const Bytes*>* generated=nullptr){
 Bytes output;Require(member.size<=output.max_size(),"recipe output size cannot be represented");output.reserve(member.size);
 for(const auto& span:member.spans){Require(span.size&&span.size<=member.size-output.size(),"recipe output span extent differs");const Bytes* source=nullptr;
  switch(span.kind){case Original:Require(originals.count(span.source),"recipe original member absent");source=originals.at(span.source);break;
   case Author:Require(package.resources.count(span.source),"recipe author resource absent");source=&package.resources.at(span.source)->bytes;break;
   case ExpandedFrame:if(generated&&generated->count(span.source))source=generated->at(span.source);else{Require(frames.count(span.source),"recipe expanded frame absent");source=&frames.at(span.source);}break;
   case Literal:Require(span.offset==0&&span.source.empty()&&span.literal.size()==span.size,"recipe literal extent differs");source=&span.literal;break;
   default:Require(false,"recipe source kind differs");}
  Require(span.offset<=source->size()&&span.size<=source->size()-span.offset,"recipe source span extent differs");output.insert(output.end(),source->begin()+span.offset,source->begin()+span.offset+span.size);
 }
 Require(output.size()==member.size&&(!verify||Digest(output)==member.hash),"recipe reconstructed member digest differs");return output;
}
} // namespace EiemResourceRecipe

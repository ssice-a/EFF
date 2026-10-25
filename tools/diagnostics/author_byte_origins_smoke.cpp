#define NOMINMAX
// The origin layer must remain usable before any native-input definitions.
#include "../../src/eiem_author_byte_origins.h"
#include "../../src/eiem_serialized_writer.h"
#include <iostream>

using namespace EiemNativeInput;
namespace {
template<class F> void Reject(F&& f){
 bool rejected=false;try{f();}catch(const std::exception&){rejected=true;}
 Require(rejected,"invalid author provenance accepted");
}
void N(Bytes& bytes,uint64_t value,size_t size){AppendInteger(bytes,value,size);}
void Z(Bytes& bytes,const std::string& text){bytes.insert(bytes.end(),text.begin(),text.end());bytes.push_back(0);}
Type ValueSchema(){
 Type type;type.classId=43;type.hash[0]=43;
 type.nodes={
  {"Object","Base",false,{1,2,3,4,5,8,11}},
  {"int","first",false,{}},{"int","second",false,{}},
  {"string","label",false,{}},{"TypelessData","blob",true,{}},
  {"Array","weights",false,{6,7}},{"int","size",false,{}},{"float","data",false,{}},
  {"PPtr<Object>","pointer",false,{9,10}},{"int","m_FileID",false,{}},{"SInt64","m_PathID",false,{}},
  {"bool","tail",true,{}}
 };
 PrepareFixedWidths(type);return type;
}
Value Sample(const Type& type,int number=7,const std::string& label="abc"){
 auto value=EmptyValue(type,0);
 SetInteger(type,Child(type,value,"first"),number);SetInteger(type,Child(type,value,"second"),number);
 SetString(type,Child(type,value,"label"),label);
 Child(type,value,"blob").raw={10,20,30};
 auto& weights=Child(type,value,"weights");weights.count=2;weights.raw={0,0,128,63,0,0,0,64};
 auto& pointer=Child(type,value,"pointer");SetInteger(type,Child(type,pointer,"m_PathID"),19);
 SetInteger(type,Child(type,value,"tail"),1);return value;
}
bool Covers(const AuthorByteSpans& spans,size_t offset){
 for(const auto& span:spans)if(span.output<=offset&&offset-span.output<span.size)return true;
 return false;
}
void SameOrigins(const AuthorByteSpans& actual,const AuthorByteSpans& expected){
 Require(actual.size()==expected.size(),"author provenance span count differs");
 for(size_t i=0;i<actual.size();++i){auto& a=actual[i];auto& b=expected[i];
  Require(a.output==b.output&&a.resource==b.resource&&a.offset==b.offset&&a.size==b.size&&a.bytes==b.bytes,
          "author provenance identity/range differs");
 }
}
Bytes Wrapped(const Bytes& bytes,size_t prefix){Bytes result(prefix,0xcc);result.insert(result.end(),bytes.begin(),bytes.end());result.push_back(0xdd);return result;}

void ValueSemantics(){
 auto type=ValueSchema();auto expected=EncodeValue(type,Sample(type));Require(expected.size()==52,"author fixture changed");
 auto resource=Wrapped(expected,7);
 auto value=DecodeAuthorValue(type,0,expected,"resources/mesh.bin",resource,7);
 AuthorByteSpans spans;Require(EncodeValue(type,value,&spans)==expected,"provenance changed serialized bytes");
 EiemAuthorBytes::Validate(expected,spans);
 // Count/length headers and alignment bytes are structure, never payloads.
 for(size_t i=0;i<expected.size();++i){bool payload=i<8||(i>=12&&i<15)||(i>=20&&i<23)||(i>=28&&i<49);
  Require(Covers(spans,i)==payload,"author dependency included structure or omitted payload");
 }
 for(const auto& span:spans)Require(span.resource=="resources/mesh.bin"&&span.offset==span.output+7,"author source offset differs");
 auto copy=value;AuthorByteSpans copied;Require(EncodeValue(type,copy,&copied)==expected,"copied author Value changed bytes");SameOrigins(copied,spans);
 Require(EncodeValue(type,DecodeValue(type,0,expected))==expected,"legacy codec changed bytes");

 // Explicitly distinct author resources can contain identical numeric bytes.
 Bytes same={7,0,0,0};auto a=Wrapped(same,3),b=Wrapped(same,5);
 Child(type,copy,"first")=DecodeAuthorValue(type,1,same,"resources/a.bin",a,3);
 Child(type,copy,"second")=DecodeAuthorValue(type,2,same,"resources/b.bin",b,5);
 EncodeValue(type,copy,&copied);
 Require(copied[0].resource=="resources/a.bin"&&copied[0].offset==3&&copied[0].size==4&&
         copied[1].resource=="resources/b.bin"&&copied[1].offset==5&&copied[1].size==4,
         "equal bytes were mistaken for a resource dependency");

 // A generated setter loses provenance even if its bytes happen to be equal.
 SetInteger(type,Child(type,copy,"first"),7);SetString(type,Child(type,copy,"label"),"abc");
 EncodeValue(type,copy,&copied);Require(!Covers(copied,0)&&Covers(copied,4)&&!Covers(copied,12),"same-value setter kept author dependency");
 SerializedLayout refs;SetReference(type,Child(type,copy,"pointer"),{"CAB-local",19},refs,"CAB-local");
 EncodeValue(type,copy,&copied);Require(!Covers(copied,36)&&!Covers(copied,40),"reference setter kept author dependency");
 // A direct byte conversion must invalidate only its own raw origin.
 Child(type,copy,"second").raw[0]=9;
 auto changed=EncodeValue(type,copy,&copied);Require(!Covers(copied,4)&&Covers(copied,20)&&Covers(copied,28),"stale raw origin survived or unrelated origin was lost");
 EiemAuthorBytes::Validate(changed,copied);

 // Empty arrays/blobs preserve their structural length with no zero-length spans.
 auto empty=Sample(type);Child(type,empty,"weights").count=0;Child(type,empty,"weights").raw.clear();
 Child(type,empty,"blob").raw.clear();SetString(type,Child(type,empty,"label"),"");
 auto emptyBytes=EncodeValue(type,empty);auto emptyResource=Wrapped(emptyBytes,2);
 auto decoded=DecodeAuthorValue(type,0,emptyBytes,"resources/empty.bin",emptyResource,2);
 Require(EncodeValue(type,decoded,&copied)==emptyBytes,"empty author fields changed bytes");EiemAuthorBytes::Validate(emptyBytes,copied);
 std::cout<<"PASS typed raw values: identity, copies, headers/padding, packed arrays, setters and stale-byte filtering\n";
}

// Minimal native SerializedFile writer, independent of any Prefab/Mesh builder.
Bytes Member(const Type& type,const std::map<int64_t,Bytes>& objects){
 Bytes metadata;Z(metadata,"author-provenance-fixture");N(metadata,0,4);N(metadata,1,1);N(metadata,1,4);
 N(metadata,type.classId,4);N(metadata,0,1);N(metadata,65535,2);
 if(type.classId==114){N(metadata,0,8);N(metadata,0,8);}
 metadata.insert(metadata.end(),type.hash.begin(),type.hash.end());Bytes strings,nodes;
 auto emit=[&](auto&& self,size_t index,unsigned depth)->void{
  const auto& node=type.nodes[index];auto typeOffset=strings.size();Z(strings,node.type);auto nameOffset=strings.size();Z(strings,node.name);
  N(nodes,1,2);N(nodes,depth,1);N(nodes,0,1);N(nodes,typeOffset,4);N(nodes,nameOffset,4);N(nodes,0,8);N(nodes,node.align?0x4000:0,4);N(nodes,0,8);
  for(auto child:node.children)self(self,child,depth+1);
 };emit(emit,0,0);
 N(metadata,type.nodes.size(),4);N(metadata,strings.size(),4);metadata.insert(metadata.end(),nodes.begin(),nodes.end());metadata.insert(metadata.end(),strings.begin(),strings.end());N(metadata,0,4);
 N(metadata,objects.size(),4);if(!objects.empty())while(metadata.size()%4)metadata.push_back(0);
 Bytes data;for(const auto& object:objects){while(data.size()%8)data.push_back(0);N(metadata,uint64_t(object.first),8);N(metadata,data.size(),8);N(metadata,object.second.size(),4);N(metadata,0,4);data.insert(data.end(),object.second.begin(),object.second.end());}
 N(metadata,0,4);N(metadata,0,4);Bytes bytes(48);bytes.insert(bytes.end(),metadata.begin(),metadata.end());while(bytes.size()%16)bytes.push_back(0);
 WriteInteger(bytes,20,metadata.size(),4,true);WriteInteger(bytes,32,bytes.size(),8,true);bytes.insert(bytes.end(),data.begin(),data.end());WriteInteger(bytes,24,bytes.size(),8,true);
 ParseSerialized(bytes);return bytes;
}
void MemberRelocation(){
 auto type=ValueSchema();auto first=EncodeValue(type,Sample(type,7,"first"));auto middle=EncodeValue(type,Sample(type,8,"middle"));auto last=EncodeValue(type,Sample(type,9,"last"));
 const auto before=Member(type,{{10,first},{20,middle},{30,last}});auto layout=ParseSerialized(before);
 auto source10=Wrapped(first,1),source20=Wrapped(middle,2),source30=Wrapped(last,3);
 AuthorByteSpans sourceOrigins;
 auto track=[&](int64_t id,const Bytes& object,const Bytes& resource,size_t prefix){
  auto value=DecodeAuthorValue(type,0,object,"resources/old"+std::to_string(id)+".bin",resource,prefix);AuthorByteSpans origins;
  EncodeValue(type,value,&origins);EiemAuthorBytes::Append(sourceOrigins,EiemAuthorBytes::Slice(origins,0,object.size(),layout.objects.at(id).offset));
 };track(10,first,source10,1);track(20,middle,source20,2);track(30,last,source30,3);EiemAuthorBytes::Validate(before,sourceOrigins);
 AuthorByteSpans noChange;auto unchanged=BuildSerialized(before,layout,{},{},{},{},{},&sourceOrigins,nullptr,&noChange);
 Require(unchanged==before,"no-change member writer changed bytes");SameOrigins(noChange,sourceOrigins);
 // In/out span aliases are supported, including the no-change branch.
 auto aliased=sourceOrigins;BuildSerialized(before,layout,{},{},{},{},{},&aliased,nullptr,&aliased);SameOrigins(aliased,sourceOrigins);

 auto replacementBytes=EncodeValue(type,Sample(type,70,"replacement-longer-than-the-original"));auto additionBytes=EncodeValue(type,Sample(type,71,"added"));
 auto replacementResource=Wrapped(replacementBytes,5),additionResource=Wrapped(additionBytes,6);
 auto replacement=DecodeAuthorValue(type,0,replacementBytes,"resources/replacement.bin",replacementResource,5);
 auto addition=DecodeAuthorValue(type,0,additionBytes,"resources/addition.bin",additionResource,6);
 std::map<int64_t,AuthorByteSpans> objectOrigins;
 auto encodedReplacement=EncodeValue(type,replacement,&objectOrigins[10]);auto encodedAddition=EncodeValue(type,addition,&objectOrigins[-4]);
 AuthorByteSpans outputOrigins;
 auto output=BuildSerialized(before,layout,{{10,encodedReplacement}},{{-4,0,encodedAddition}},{30},{},{},&sourceOrigins,&objectOrigins,&outputOrigins);
 Require(output==BuildSerialized(before,layout,{{10,encodedReplacement}},{{-4,0,encodedAddition}},{30}),"provenance changed native member bytes");
 auto rebuilt=ParseSerialized(output);Require(rebuilt.order==std::vector<int64_t>({-4,10,20})&&!rebuilt.objects.count(30),"native object records were not sorted/removed");
 EiemAuthorBytes::Validate(output,outputOrigins);
 for(auto row:std::vector<std::pair<int64_t,std::string>>{{-4,"resources/addition.bin"},{10,"resources/replacement.bin"},{20,"resources/old20.bin"}}){
  auto object=rebuilt.objects.at(row.first);auto relative=EiemAuthorBytes::Slice(outputOrigins,object.offset,object.size);
  Require(!relative.empty(),"rebuilt object lost its author origins");for(const auto& span:relative)Require(span.resource==row.second,"member relocation confused author resource");
  // Re-decoding a previously rebuilt member must preserve its explicit origin.
  auto decoded=ObjectValue(output,rebuilt,row.first,&outputOrigins);AuthorByteSpans reencoded;EncodeValue(type,decoded,&reencoded);SameOrigins(reencoded,relative);
 }
 for(const auto& span:outputOrigins)Require(span.resource!="resources/old10.bin"&&span.resource!="resources/old30.bin","replaced/deleted object retained source dependency");
 auto second=BuildSerialized(output,rebuilt,{{20,middle}},{},{},{},{},&outputOrigins,nullptr,&aliased);
 Require(!EiemAuthorBytes::Slice(aliased,ParseSerialized(second).objects.at(20).offset,middle.size()).size(),"untracked replacement inherited source origins");
 EiemAuthorBytes::Validate(second,aliased);
 auto empty=BuildSerialized(before,layout,{},{},{10,20,30},{},{},&sourceOrigins,nullptr,&aliased);
 Require(ParseSerialized(empty).objects.empty()&&aliased.empty(),"removing every object retained provenance");
 auto zero=Member(type,{});auto zeroLayout=ParseSerialized(zero);Require(BuildSerialized(zero,zeroLayout,{},{},{},{},{},nullptr,nullptr,&aliased)==zero&&aliased.empty(),"empty no-change member retained prior output origins");
 std::cout<<"PASS native member rebuild: replacement/addition/deletion, sorted records, aliasing and re-decoding\n";
}

void OwnedAndInvalidRanges(){
 std::weak_ptr<const Bytes> weak;AuthorByteSpans spans;
 {
  auto derived=std::make_shared<const Bytes>(Bytes{1,2,3,4});weak=derived;
  auto first=EiemAuthorBytes::MakeRange("@frame:mesh",derived,0,2);auto second=EiemAuthorBytes::MakeRange("@frame:mesh",derived,2,2,2);
  EiemAuthorBytes::Append(first,second);Require(first.size()==1&&first.front().size==4,"contiguous owned author source did not coalesce");
  spans=EiemAuthorBytes::Slice(first,1,2,3);
 }
 Require(!weak.expired()&&spans.front().owned&&spans.front().bytes==spans.front().owned.get(),"derived byte source lifetime was lost");
 Require(spans.front().output==3&&spans.front().offset==1&&spans.front().size==2,"owned slice range differs");
 EiemAuthorBytes::Validate(Bytes{0,0,0,2,3},spans);spans.clear();Require(weak.expired(),"derived byte source retained after all spans were released");
 Bytes bytes={1,2,3,4};auto valid=EiemAuthorBytes::MakeRange("resources/a.bin",bytes,0,4);
 auto stale=bytes;stale[0]=99;Require(EiemAuthorBytes::Matching(stale,valid).empty(),"changed bytes retained provenance");
 Reject([&]{EiemAuthorBytes::Validate(stale,valid);});
 Reject([&]{auto invalid=valid;invalid.push_back(invalid.front());EiemAuthorBytes::Validate(bytes,invalid);});
 Reject([&]{auto invalid=valid;invalid.front().offset=2;EiemAuthorBytes::Validate(bytes,invalid);});
 Reject([&]{auto invalid=valid;invalid.front().output=SIZE_MAX;EiemAuthorBytes::Validate(bytes,invalid);});
 Reject([&]{EiemAuthorBytes::MakeRange("",bytes,0,4);});
 Reject([&]{EiemAuthorBytes::MakeRange("resources/a.bin",bytes,3,2);});
 Reject([&]{EiemAuthorBytes::Slice(valid,SIZE_MAX,1);});
 Reject([&]{auto invalid=valid;invalid.front().output=1;EiemAuthorBytes::Validate(bytes,invalid);});
 auto type=ValueSchema();auto encoded=EncodeValue(type,Sample(type));auto wrong=encoded;wrong[0]^=1;
 auto wrongOrigins=EiemAuthorBytes::MakeRange("resources/wrong.bin",wrong,0,wrong.size());
 Reject([&]{DecodeValue(type,0,encoded,&wrongOrigins);});
 std::cout<<"PASS author source ownership, extent/overlap rejection and explicit-source validation\n";
}
} // namespace
int main(){try{ValueSemantics();MemberRelocation();OwnedAndInvalidRanges();return 0;}catch(const std::exception& e){std::cerr<<"FAIL author byte provenance: "<<e.what()<<'\n';return 1;}}

#define main StaticAssemblyFixtureMain
#include "static_mesh_assembly_smoke.cpp"
#undef main
#include "../../src/eiem_native_random_access.h"
#include "../../src/eiem_native_serialized_splice.h"
#include "../../src/eiem_mesh_target_directory.h"
#include "../../src/eiem_demand_mesh_preparation.h"
#include "../../src/eiem_author_mesh_bundle.h"
#include "../../src/eiem_native_splice_view.h"
#include "../../src/eiem_native_object_graft.h"
#include "../../src/eiem_demand_native_assembly.h"
#include <chrono>

namespace R=EiemNativeRandomAccess;
namespace S=EiemNativeSpliceView;
template<class F> void RejectRead(F&& f,const char* label) {
  bool rejected=false;try{f();}catch(const std::exception&){rejected=true;}
  Require(rejected,std::string("invalid partial input accepted: ")+label);
}
void SparseInput() {
  constexpr size_t size=size_t(1024)*1024*1024;
  auto prefix=EiemNativeViewWriter::Prefix(EiemAuthorMeshBundle::EnvelopeHeader(),
      {{size,size,0}},{{"CAB-small",4,0,128},{"unrelated.resS",0,128,size-128}});
  size_t bytes=0;
  R::Reader reader(prefix.size()+size,[&](size_t at,unsigned char* out,size_t n){
    bytes+=n;for(size_t i=0;i<n;++i)out[i]=at+i<prefix.size()?prefix[at+i]:static_cast<unsigned char>(((at+i-prefix.size())*7)&255);
  });
  Require(bytes==48+reader.header.compressedInfo,"opening input read data blocks");
  auto value=reader.ReadAt(0,128,128);for(size_t i=0;i<value.size();++i)Require(value[i]==static_cast<unsigned char>((i*7)&255),"sparse input bytes differ");
  auto before=bytes;
  RejectRead([&]{reader.ReadAt(size-1,2,2);},"decoded bounds");
  RejectRead([&]{reader.ReadAt(0,4096,32);},"allocation budget");
  Require(bytes==before&&!reader.stats.inflatedBlocks&&bytes<1024,"1 GiB source was read/materialized for a 128-byte request");
  std::cout<<"PASS sparse 1GiB input bytesRead="<<bytes<<" requestedPayload=128 inflatedBlocks=0\n";
}
Bytes LiteralBlock(const Bytes& raw) {
  Bytes encoded;auto n=std::min(size_t(15),raw.size());encoded.push_back(static_cast<unsigned char>((n&3)|((n&12)<<2)));
  if(n==15){auto remaining=raw.size()-15;while(remaining>=255){encoded.push_back(255);remaining-=255;}encoded.push_back(static_cast<unsigned char>(remaining));}
  encoded.insert(encoded.end(),raw.begin(),raw.end());EndfieldInput::CryptBlock(encoded);return encoded;
}
void CompressedRanges() {
  Bytes first(4096),second(4096),third(4096);for(size_t i=0;i<4096;++i){first[i]=static_cast<unsigned char>(i);second[i]=static_cast<unsigned char>(i*3);third[i]=static_cast<unsigned char>(i*11);}
  auto a=LiteralBlock(first),b=LiteralBlock(third);
  auto input=EiemNativeViewWriter::Prefix(EiemAuthorMeshBundle::EnvelopeHeader(),
      {{first.size(),a.size(),5},{second.size(),second.size(),0},{third.size(),b.size(),5}},{{"CAB-ranges",4,0,12288}});
  auto prefix=input.size();input.insert(input.end(),a.begin(),a.end());input.insert(input.end(),second.begin(),second.end());input.insert(input.end(),b.begin(),b.end());
  R::Limits limits;limits.cacheBytes=64;
  R::Reader reader(input.size(),[&](size_t p,unsigned char* out,size_t n){std::memcpy(out,input.data()+p,n);},limits);
  auto v=reader.ReadAt(4090,20,20);Bytes expected(first.end()-6,first.end());expected.insert(expected.end(),second.begin(),second.begin()+14);
  Require(v==expected&&reader.stats.inflatedBlocks==1,"cross-block request decoded unrelated compressed block");
  auto before=reader.stats.encodedBytes;reader.ReadAt(1,7,7);
  Require(reader.stats.encodedBytes==before&&reader.stats.inflatedBlocks==1,"large current block was re-inflated for every field");
  reader.ReadAt(8192,9,9);Require(reader.stats.inflatedBlocks==2&&reader.stats.pinnedBytes==4096,"bounded pinned block did not rotate");
  auto decoded=EndfieldInput::Decode(input);Bytes all=first;all.insert(all.end(),second.begin(),second.end());all.insert(all.end(),third.begin(),third.end());
  Require(decoded.members.at(0).bytes==all,"compressed fixture differs from production decoder");
  R::Limits tight;tight.blockBytes=100;
  R::Reader limited(input.size(),[&](size_t p,unsigned char* out,size_t n){std::memcpy(out,input.data()+p,n);},tight);
  RejectRead([&]{limited.ReadAt(0,1,1);},"compression working budget");
  auto truncated=input;truncated.resize(prefix+1);
  RejectRead([&]{R::Reader invalid(truncated.size(),[&](size_t p,unsigned char* out,size_t n){std::memcpy(out,truncated.data()+p,n);});},"truncated source");
  std::cout<<"PASS compressed cross-block reads, bounded cache, malformed input, production decode agreement\n";
}
Bytes Materialize(const std::shared_ptr<const EiemInputSpanView::View>& view) {
  Bytes bytes(view->Size());Require(view->ReadAt(0,bytes.data(),bytes.size())==bytes.size(),"splice view truncated");return bytes;
}
void SpliceCompressed() {
  auto first=Bytes(4096,3),middle=Bytes(4096,5),last=Bytes(4096,7);
  auto a=LiteralBlock(first),b=LiteralBlock(last);
  auto input=std::make_shared<Bytes>(EiemNativeViewWriter::Prefix(EiemAuthorMeshBundle::EnvelopeHeader(),
    {{4096,a.size(),5},{4096,4096,0},{4096,b.size(),5}},{{"CAB-edit",4,0,12288}}));
  input->insert(input->end(),a.begin(),a.end());input->insert(input->end(),middle.begin(),middle.end());input->insert(input->end(),b.begin(),b.end());
  auto read=[input](size_t at,unsigned char* out,size_t n){Require(at<=input->size()&&n<=input->size()-at,"splice source read invalid");std::memcpy(out,input->data()+at,n);};
  R::Reader reader(input->size(),read);auto before=reader.stats.encodedBytes;
  auto unchanged=S::Build(reader,{});Require(reader.stats.encodedBytes==before&&Materialize(unchanged.Bind(read))==*input,"no-op splice read/changed original blocks");
  auto one=std::make_shared<const Bytes>(Bytes{11,12,13,14});auto two=std::make_shared<const Bytes>(Bytes{21,22,23,24,25,26,27});
  auto changed=S::Build(reader,{{0,30,2,one},{0,100,3,two}});
  Require(reader.stats.inflatedBlocks==1&&changed.reusedEncoded>=4096+b.size(),"splice inflated/copied untouched compressed data");
  auto expected=first;expected.insert(expected.end(),middle.begin(),middle.end());expected.insert(expected.end(),last.begin(),last.end());
  expected.erase(expected.begin()+100,expected.begin()+103);expected.insert(expected.begin()+100,two->begin(),two->end());
  expected.erase(expected.begin()+30,expected.begin()+32);expected.insert(expected.begin()+30,one->begin(),one->end());
  auto decoded=EndfieldInput::Decode(Materialize(changed.Bind(read)));
  Require(decoded.members.size()==1&&decoded.members[0].bytes==expected,"variable splice differs from full decode oracle");
  RejectRead([&]{S::Build(reader,{{0,30,4,one},{0,31,2,two}});},"overlapping splice");
  RejectRead([&]{S::Build(reader,{{0,30,0,one},{0,30,0,two}});},"ambiguous insertions");
  RejectRead([&]{S::Build(reader,{{0,12288,1,one}});},"splice member bound");
  RejectRead([&]{S::Build(reader,{{0,30,2,one}},1);},"splice literal budget");
  std::cout<<"PASS compressed splice: changed-block-only decode, native original blocks retained, variable-size edits and invalid ranges\n";
}
void SpliceMetadata() {
  Fixture fixture;auto raw=fixture.BytesOf();auto layout=ParseSerialized(raw);
  auto source=std::make_shared<Bytes>(Bundle(raw,fixture.cab));
  auto read=[source](size_t at,unsigned char* out,size_t n){std::memcpy(out,source->data()+at,n);};
  R::Reader reader(source->size(),read);
  auto value=ObjectValue(raw,layout,10);const auto& type=layout.types.at(layout.objects.at(10).type);
  auto old=Child(type,value,"m_Name").raw;SetString(type,Child(type,value,"m_Name"),std::string(old.begin(),old.end())+std::string(16,'x'));
  auto object=std::make_shared<const Bytes>(EncodeValue(type,value));
  auto oracle=BuildSerialized(raw,layout,{{10,*object}});auto after=ParseSerialized(oracle);
  auto metadata=std::make_shared<const Bytes>(oracle.begin(),oracle.begin()+after.dataOffset);
  auto prepared=S::Build(reader,{{0,0,layout.dataOffset,metadata},{0,layout.objects.at(10).offset,layout.objects.at(10).size,object}});
  auto decoded=EndfieldInput::Decode(Materialize(prepared.Bind(read)));
  Require(decoded.members.at(0).bytes==oracle,"spliced native metadata/object bytes differ from existing writer");
  auto checked=ParseSerialized(decoded.members.at(0).bytes);Require(checked.externals==layout.externals&&checked.types.size()==layout.types.size(),"splice lost native externals/types");
  for(size_t i=0;i<layout.types.size();++i)Require(checked.types[i].nativeRecord==layout.types[i].nativeRecord,"splice lost native type metadata");
  for(const auto& row:layout.objects)if(row.first!=10){const auto& out=checked.objects.at(row.first);Require(row.second.size==out.size&&std::equal(raw.begin()+row.second.offset,raw.begin()+row.second.offset+row.second.size,decoded.members[0].bytes.begin()+out.offset),"splice changed unrelated serialized object");}
  std::cout<<"PASS variable native object edit: metadata, signed object table, payloads and native type records match existing writer\n";
}
void SpliceSparse() {
  constexpr size_t size=size_t(1024)*1024*1024;
  auto prefix=std::make_shared<const Bytes>(EiemNativeViewWriter::Prefix(EiemAuthorMeshBundle::EnvelopeHeader(),{{size,size,0}},{{"tiny",0,0,128},{"large.resS",0,128,size-128}}));
  size_t bytes=0;auto read=[prefix,&bytes](size_t at,unsigned char* out,size_t n){bytes+=n;for(size_t i=0;i<n;++i)out[i]=at+i<prefix->size()?(*prefix)[at+i]:static_cast<unsigned char>((at+i-prefix->size())&255);};
  R::Reader reader(prefix->size()+size,read);auto before=bytes;
  auto edit=std::make_shared<const Bytes>(Bytes{99});auto prepared=S::Build(reader,{{0,16,1,edit}});auto view=prepared.Bind(read);
  Require(bytes==before&&!reader.stats.inflatedBlocks&&prepared.literalBytes==1&&prepared.reusedEncoded==size-1,"1 GiB splice eagerly read/copied unchanged data");
  auto outputHeader=Bytes(48);view->ReadAt(0,outputHeader.data(),48);auto h=EndfieldInput::ReadHeader(outputHeader);auto data=48+AlignSize(h.compressedInfo,16);
  Bytes value(32);view->ReadAt(data,value.data(),value.size());for(size_t i=0;i<value.size();++i)Require(value[i]==(i==16?99:static_cast<unsigned char>(i)),"sparse splice bytes differ");
  Require(bytes-before<128,"sparse view read unrelated payload");
  std::cout<<"PASS 1GiB splice: preparePayloadReads=0 replacementBytes=1 originalSourceRanges="<<prepared.reusedEncoded<<"\n";
}
void SpliceDifferential() {
  uint32_t state=72341;auto random=[&](){state^=state<<13;state^=state>>17;state^=state<<5;return state;};
  for(size_t trial=0;trial<64;++trial){Bytes decoded(12288);for(auto& b:decoded)b=static_cast<unsigned char>(random());
    std::vector<Bytes> encoded;std::vector<EiemNativeViewWriter::Block> blocks;
    for(size_t i=0;i<6;++i){Bytes part(decoded.begin()+i*2048,decoded.begin()+(i+1)*2048);bool compressed=(random()&1)!=0;auto b=compressed?LiteralBlock(part):part;blocks.push_back({2048,b.size(),uint16_t(compressed?5:0)});encoded.push_back(std::move(b));}
    std::vector<EiemNativeViewWriter::Member> members{{"one",0,0,3000},{"two",0,4096,3000},{"three",0,8192,3000}};
    auto input=std::make_shared<Bytes>(EiemNativeViewWriter::Prefix(EiemAuthorMeshBundle::EnvelopeHeader(),blocks,members));for(const auto& b:encoded)input->insert(input->end(),b.begin(),b.end());
    auto read=[input](size_t at,unsigned char* out,size_t n){std::memcpy(out,input->data()+at,n);};R::Reader reader(input->size(),read);
    std::vector<S::Edit> edits;std::vector<Bytes> expected;
    for(size_t i=0;i<3;++i){const auto& member=members[i];Bytes data(decoded.begin()+member.offset,decoded.begin()+member.offset+member.size);auto at=random()%2990,erase=random()%10;auto added=std::make_shared<Bytes>(random()%24);for(auto& b:*added)b=static_cast<unsigned char>(random());
      data.erase(data.begin()+at,data.begin()+at+erase);data.insert(data.begin()+at,added->begin(),added->end());edits.push_back({i,at,erase,added});expected.push_back(std::move(data));}
    auto prepared=S::Build(reader,edits);auto output=EndfieldInput::Decode(Materialize(prepared.Bind(read)));Require(output.members.size()==3,"splice lost native members");
    for(size_t i=0;i<3;++i)Require(output.members[i].bytes==expected[i]&&output.members[i].name==members[i].name,"randomized splice differs from independent vector oracle");
  }
  std::cout<<"PASS 64 deterministic differential splice inputs: mixed compressed blocks, gaps, multiple members and variable edits\n";
}
void SerializedPlanDifferential() {
  namespace P=EiemNativeSerializedSplice;
  Fixture fixture;auto raw=fixture.BytesOf();auto native=ParseSerialized(raw);
  uint32_t state=94621;auto random=[&](){state^=state<<13;state^=state>>17;state^=state<<5;return state;};
  auto source=std::make_shared<const Bytes>(Bundle(raw,fixture.cab));
  auto read=[source](size_t at,unsigned char* out,size_t n){std::memcpy(out,source->data()+at,n);};
  for(size_t trial=0;trial<96;++trial){R::Reader reader(source->size(),read);auto layout=reader.Layout(reader.directory.members[0]);
    std::map<int64_t,Bytes> changes;std::map<int64_t,P::Payload> shared;std::set<int64_t> removed;
    std::vector<AddedObject> added;std::vector<P::Added> sharedAdded;std::vector<Bytes> types,scripts;
    if(trial%4!=0){auto value=ObjectValue(raw,native,10);const auto& type=layout.types[layout.objects.at(10).type];
      SetString(type,Child(type,value,"m_Name"),std::string(random()%73,'x'));changes.emplace(10,EncodeValue(type,value));}
    if(trial%3==0)removed.insert(23);
    if(trial%5==0){auto type=layout.types[0];type.nativeRecord[7]^=17;type.hash[0]^=17;types.push_back(type.nativeRecord);layout.types.push_back(std::move(type));}
    if(trial%7==0)scripts.push_back(Bytes(12,static_cast<unsigned char>(trial)));
    if(trial%2==0)layout.externals.push_back("CAB-added-external");
    if(trial%3!=1){auto value=ObjectValue(raw,native,20);SetString(layout.types[0],Child(layout.types[0],value,"m_Name"),"added");
      added.push_back({-int64_t(trial)-1,types.empty()?size_t(0):layout.types.size()-1,EncodeValue(layout.types[0],value)});}
    for(const auto& row:changes)shared.emplace(row.first,std::make_shared<const Bytes>(row.second));
    for(const auto& row:added)sharedAdded.push_back({row.id,row.type,std::make_shared<const Bytes>(row.bytes)});
    auto plan=P::Build(reader,0,layout,shared,sharedAdded,removed,types,scripts);
    auto prepared=S::BuildRanges(reader,{plan.rewrite});auto actual=EndfieldInput::Decode(Materialize(prepared.Bind(read))).members[0].bytes;
    auto expected=BuildSerialized(raw,layout,changes,added,removed,types,scripts);
    Require(actual==expected,"metadata/object range plan differs from complete writer");
    auto checked=ParseSerialized(actual);Require(checked.externals==layout.externals,"object plan lost external references");
    for(size_t i=0;i<native.types.size();++i)Require(native.types[i].nativeRecord==checked.types[i].nativeRecord,"object plan changed original native type record");
    for(const auto& row:native.objects)if(!changes.count(row.first)&&!removed.count(row.first)){const auto& out=checked.objects.at(row.first);Require(row.second.size==out.size&&std::equal(raw.begin()+row.second.offset,raw.begin()+row.second.offset+row.second.size,actual.begin()+out.offset),"object plan changed unrelated payload");}
  }
  R::Reader reader(source->size(),read);auto owned=std::make_shared<const Bytes>(Bytes{1});
  RejectRead([&]{P::Build(reader,0,native,{{999,owned}});},"object plan absent replacement");
  RejectRead([&]{P::Build(reader,0,native,{{10,owned}},{},{10});},"object plan replace/remove conflict");
  RejectRead([&]{P::Build(reader,0,native,{},{{10,0,owned}});},"object plan added collision");
  RejectRead([&]{P::Build(reader,0,native,{},{{-1,0,owned},{-1,0,owned}});},"object plan repeated added identity");
  RejectRead([&]{P::Build(reader,0,native,{},{},{999});},"object plan absent deletion");
  auto wrong=native;++wrong.objects.at(10).size;RejectRead([&]{P::Build(reader,0,wrong,{{10,owned}});},"object plan stale source extent");
  auto noChange=P::Build(reader,0,native,{});Require(noChange.changedBytes==0&&noChange.retainedBytes==raw.size(),"empty object plan did work");
  std::set<int64_t> all(native.order.begin(),native.order.end());auto empty=P::Build(reader,0,native,{},{},all);
  auto emptyInput=EndfieldInput::Decode(Materialize(S::BuildRanges(reader,{empty.rewrite}).Bind(read)));
  Require(ParseSerialized(emptyInput.members[0].bytes).objects.empty(),"object plan failed zero-object output");
  std::cout<<"PASS 96 serialized range/full-writer differentials: grow/shrink/add/delete, signed IDs, native types/scripts/externals, collision and stale-layout rejection\n";
}
void SerializedPlanSparse() {
  namespace P=EiemNativeSerializedSplice;
  Fixture fixture;auto raw=fixture.BytesOf();auto layout=ParseSerialized(raw);auto last=std::max_element(layout.objects.begin(),layout.objects.end(),[](const auto& a,const auto& b){return a.second.offset<b.second.offset;});
  constexpr size_t huge=size_t(1024)*1024*1024;auto logicalSize=AddSize(last->second.offset,huge);
  auto metadata=std::make_shared<Bytes>(raw.begin(),raw.begin()+layout.dataOffset);
  auto slot=std::find(layout.order.begin(),layout.order.end(),last->first)-layout.order.begin();
  WriteInteger(*metadata,layout.tableStart+size_t(slot)*24+16,huge,4);WriteInteger(*metadata,24,logicalSize,8,true);
  auto envelope=std::make_shared<const Bytes>(EiemNativeViewWriter::Prefix(EiemAuthorMeshBundle::EnvelopeHeader(),{{logicalSize,logicalSize,0}},{{fixture.cab,4,0,logicalSize}}));
  size_t reads=0,payloadReads=0;
  auto read=[envelope,metadata,&reads,&payloadReads](size_t at,unsigned char* out,size_t n){reads+=n;
    for(size_t i=0;i<n;++i){auto p=at+i;if(p<envelope->size())out[i]=(*envelope)[p];else{p-=envelope->size();if(p<metadata->size())out[i]=(*metadata)[p];else{++payloadReads;out[i]=static_cast<unsigned char>(p&255);}}}};
  R::Reader reader(envelope->size()+logicalSize,read);auto actual=reader.Layout(reader.directory.members[0]);
  auto value=ObjectValue(raw,layout,10);const auto& type=layout.types[layout.objects.at(10).type];SetString(type,Child(type,value,"m_Name"),"longer-range-test-name");auto object=std::make_shared<const Bytes>(EncodeValue(type,value));
  auto plan=P::Build(reader,0,actual,{{10,object}});auto prepared=S::BuildRanges(reader,{plan.rewrite});
  Require(!payloadReads&&!reader.stats.inflatedBlocks&&reads<128*1024&&prepared.literalBytes<128*1024&&plan.retainedBytes>=huge,"serialized range preparation touched/copied huge unrelated payload");
  auto view=prepared.Bind(read);auto header=Bytes(48);view->ReadAt(0,header.data(),header.size());
  Require(EndfieldInput::ReadHeader(header).bytes==view->Size(),"serialized range view header extent differs");
  std::cout<<"PASS 1GiB serialized input: unchangedPayloadPrepareReads="<<payloadReads<<" totalPrepareReadBytes="<<reads<<" ownedLiteralBytes="<<prepared.literalBytes<<"\n";
}
void ProjectedRendererAssembly() {
  namespace P=EiemNativeSerializedSplice;
  Fixture fixture;auto raw=fixture.BytesOf();auto encoded=std::make_shared<const Bytes>(Bundle(raw,fixture.cab));
  auto read=[encoded](size_t at,unsigned char* out,size_t n){std::memcpy(out,encoded->data()+at,n);};
  Identity target{"assets/static.mesh","Static"},material{"assets/material.mat","Mat"};
  for(size_t count:{size_t(0),size_t(1),size_t(3)})for(bool borrowed:{false,true})for(bool external:{false,true}) {
    Package package;package.id="projected";package.targetIndex[target]="R";auto& rule=package.rules["R"];rule.skip=!count;
    auto resource=std::make_shared<Resource>();resource->type=fixture.types[5].hash;
    package.resources["mesh"]=resource;package.meshes["M"]={"mesh",3,3,{},{}};package.materials["A"].source=material;
    AssetIndex assets;assets[target].cab=fixture.cab;assets[target].pathId=100;assets[target].type=resource->type;assets[target].classId=43;
    assets[material].cab=external?"CAB-cccccccccccccccccccccccccccccccc":fixture.cab;assets[material].pathId=200;assets[material].classId=21;
    std::vector<BuiltMeshPart> parts;
    for(size_t i=0;i<count;++i){auto id="P"+std::to_string(i);rule.parts.push_back(id);package.parts[id].mesh="M";package.parts[id].material="A";
      parts.push_back({target,"R",id,fixture.cab,100,int64_t(100+i),3,3,{}});}
    RendererMemberBuilder oracle(raw,fixture.cab,package,assets,parts);if(borrowed)oracle.UseSourceMaterials();auto full=oracle.Build();
    R::Reader reader(encoded->size(),read);const auto& member=reader.directory.members[0];auto layout=reader.Layout(member);
    RendererMemberBuilder sparse(layout,member.name,package,assets,parts,
      [&](int64_t id,const std::set<std::string>& fields){return reader.Project(member,layout,id,fields);},
      [&](int64_t id){return reader.Object(member,layout,id);});
    if(borrowed)sparse.UseSourceMaterials();auto built=sparse.Build();Require(built.bytes.empty(),"projected Renderer allocated a complete member");
    std::map<int64_t,P::Payload> changes;for(auto& row:sparse.TakeChanges())changes.emplace(row.first,std::make_shared<const Bytes>(std::move(row.second)));
    std::vector<P::Added> added;for(auto& object:sparse.TakeAdded())added.push_back({object.id,object.type,std::make_shared<const Bytes>(std::move(object.bytes))});
    auto plan=P::Build(reader,0,sparse.OutputLayout(),changes,added);auto output=EndfieldInput::Decode(Materialize(S::BuildRanges(reader,{plan.rewrite}).Bind(read)));
    Require(output.members[0].bytes==full.bytes&&built.parts.size()==full.parts.size()&&built.matches==2&&built.addedObjects==full.addedObjects,"projected Renderer changed 0/1/N assembly, static component pairing, LOD/helper/preload expansion or external Material references");
    RejectRead([&]{sparse.Build();},"Renderer builder reuse");
  }
  // An unrelated inline Texture in the SAME SerializedFile must not be read,
  // decoded or copied just because another object references a selected Mesh.
  fixture.types.push_back(Schema(28,{{"TypelessData","image data",{}}}));fixture.Add(400,fixture.types.size()-1);
  raw=fixture.BytesOf();auto original=ParseSerialized(raw);auto logicalSize=original.objects.at(400).offset+size_t(1024)*1024*1024;
  auto metadata=std::make_shared<Bytes>(raw.begin(),raw.begin()+original.dataOffset);auto slot=std::find(original.order.begin(),original.order.end(),400)-original.order.begin();
  WriteInteger(*metadata,original.tableStart+size_t(slot)*24+16,size_t(1024)*1024*1024,4);WriteInteger(*metadata,24,logicalSize,8,true);
  auto envelope=std::make_shared<const Bytes>(EiemNativeViewWriter::Prefix(EiemAuthorMeshBundle::EnvelopeHeader(),{{logicalSize,logicalSize,0}},{{fixture.cab,4,0,logicalSize}}));
  size_t untouchedReads=0;auto pixelStart=original.objects.at(400).offset;
  auto source=[&,metadata,envelope](size_t at,unsigned char* out,size_t n){for(size_t i=0;i<n;++i){auto p=at+i;if(p<envelope->size())out[i]=(*envelope)[p];else{p-=envelope->size();
    if(p<metadata->size())out[i]=(*metadata)[p];else if(p<pixelStart)out[i]=raw[p];else{++untouchedReads;out[i]=0;}}}};
  R::Reader reader(envelope->size()+logicalSize,source);const auto& member=reader.directory.members[0];auto layout=reader.Layout(member);
  Package package;package.id="sparse";package.targetIndex[target]="R";package.rules["R"].skip=true;AssetIndex assets;assets[target].cab=fixture.cab;assets[target].pathId=100;assets[target].classId=43;
  std::vector<BuiltMeshPart> parts;RendererMemberBuilder builder(layout,member.name,package,assets,parts,
    [&](int64_t id,const std::set<std::string>& fields){return reader.Project(member,layout,id,fields);},[&](int64_t id){return reader.Object(member,layout,id);});
  auto result=builder.Build();std::map<int64_t,P::Payload> changes;for(auto& row:builder.TakeChanges())changes.emplace(row.first,std::make_shared<const Bytes>(std::move(row.second)));
  auto plan=P::Build(reader,0,builder.OutputLayout(),changes);auto view=S::BuildRanges(reader,{plan.rewrite});
  Require(result.matches==2&&changes.size()==4&&result.bytes.empty()&&!untouchedReads&&reader.stats.encodedBytes<128*1024&&view.literalBytes<128*1024,"projected Renderer touched an unrelated 1GiB Texture or allocated a complete consumer member");
  std::cout<<"PASS projected Renderer: 12 full-writer differentials, 0/1/N, LOD/helper/preloads, static pairing, cross-CAB Materials; 1GiB unrelated inline Texture bytesRead="<<untouchedReads<<" prepareBytes="<<reader.stats.encodedBytes<<'\n';
}
void StreamedAuthorGraft() {
  namespace G=EiemNativeObjectGraft;namespace P=EiemNativeSerializedSplice;
  size_t streamed=0;
  for(size_t trial=0;trial<64;++trial){
    Fixture af,cf;
    af.types[5]=Schema(43,{{"UInt8","flag",{}},{"string","m_Name",{}},
      {"TypelessData","geometry",{}},{"UInt8","tail",{}}});
    cf.types[5]=Schema(43,{{"UInt8","gameAdded",{}},{"UInt8","flag",{}},
      {"string","m_Name",{}},{"TypelessData","geometry",{}},{"UInt8","tail",{}}});
    af.types[5].nodes[0].align=cf.types[5].nodes[0].align=bool(trial&1);
    af.objects.at(100)={5,EmptyValue(af.types[5],0)};cf.objects.at(100)={5,EmptyValue(cf.types[5],0)};
    auto fill=[&](Fixture& f,bool authored){auto& t=f.types[5];auto& v=f.V(100);
      SetString(t,Child(t,v,"m_Name"),std::string(trial%13+(authored?2:5),authored?'a':'g'));
      SetInteger(t,Child(t,v,"flag"),authored?19:27);SetInteger(t,Child(t,v,"tail"),authored?33:47);
      Child(t,v,"geometry").raw=Bytes(trial*7+3,authored?61:72);
      if(!authored)SetInteger(t,Child(t,v,"gameAdded"),99);
    };fill(af,true);fill(cf,false);
    auto ab=af.BytesOf(),cb=cf.BytesOf();auto author=std::make_shared<const Bytes>(Bundle(ab,af.cab));auto current=std::make_shared<const Bytes>(Bundle(cb,cf.cab));
    auto ar=[author](size_t at,unsigned char* out,size_t n){std::memcpy(out,author->data()+at,n);};
    auto cr=[current](size_t at,unsigned char* out,size_t n){std::memcpy(out,current->data()+at,n);};
    R::Reader a(author->size(),ar),c(current->size(),cr);auto al=a.Layout(a.directory.members[0]),cl=c.Layout(c.directory.members[0]);
    auto described=G::Describe(a,0,al,100,ar);std::set<std::string> selected{"geometry","flag"};if(trial&2)selected.insert("m_Name");
    std::map<std::string,Value> overrides;
    auto expected=cf.V(100);const auto& type=cl.types[cl.objects.at(100).type];
    for(const auto& name:selected){auto copy=Child(af.types[5],af.V(100),name);copy.node=Child(type,expected,name).node;Child(type,expected,name)=std::move(copy);}
    if(trial&4){auto name=Child(type,expected,"m_Name");SetString(type,name,"override");Child(type,expected,"m_Name")=name;overrides["m_Name"]=name;}
    auto payload=G::Compose(c,0,cl,100,described,selected,overrides);
    auto plan=P::BuildRanges(c,0,cl,{{100,payload}});auto prepared=S::BuildRanges(c,{plan.rewrite});
    auto actual=EndfieldInput::Decode(Materialize(prepared.Bind(cr))).members[0].bytes;
    auto oracle=BuildSerialized(cb,cl,{{100,EncodeValue(type,expected)}});
    Require(actual==oracle,"streamed author graft changed padding, native metadata, overrides or undeclared current fields");streamed+=prepared.streamedAuthorBytes;
    RejectRead([&]{G::Compose(c,0,cl,100,described,{"absent"});},"absent selected field");
    auto wrong=described;wrong.type.classId=28;RejectRead([&]{G::Compose(c,0,cl,100,wrong,selected);},"graft class mismatch");
  }
  // A virtual author payload larger than a native uint32 block must be split
  // without reading or owning it. Concurrent cursors retain the author lease.
  auto source=std::make_shared<const Bytes>(Bundle(Bytes{1,2,3},"CAB-host"));
  auto read=[source](size_t at,unsigned char* out,size_t n){std::memcpy(out,source->data()+at,n);};R::Reader reader(source->size(),read);
  constexpr size_t huge=size_t(UINT32_MAX)+4097;auto counter=std::make_shared<std::atomic<size_t>>(0);std::weak_ptr<std::atomic<size_t>> weak=counter;
  auto external=std::make_shared<const EiemInputSpanView::View>(std::vector<EiemInputSpanView::Span>{{0,0,huge,{}}},std::vector<size_t>{huge},
    [counter](size_t,size_t at,unsigned char* out,size_t n){*counter+=n;for(size_t i=0;i<n;++i)out[i]=static_cast<unsigned char>((at+i)*13);});
  S::Rewrite rewrite{0,{{0,huge,{},external}}};auto prepared=S::BuildRanges(reader,{rewrite});
  Require(!counter->load()&&prepared.streamedAuthorBytes==huge&&prepared.literalBytes==0&&prepared.blocks.size()==2&&prepared.authorViews.size()==1,
      "large author source was prepared eagerly or not split by native width");
  auto view=prepared.Bind(read);auto prefix=view->Size()-huge;prepared={};rewrite={};external.reset();counter.reset();Require(!weak.expired(),"streamed author lease lost at preparation return");
  std::atomic<bool> correct{true};std::vector<std::thread> threads;
  for(size_t t=0;t<4;++t)threads.emplace_back([&,t]{try{EiemInputSpanView::Cursor cursor(view);for(size_t k=0;k<32;++k){auto at=size_t(UINT32_MAX)-33+t*47+k*71;cursor.Seek(int64_t(prefix+at),0);Bytes bytes(32);Require(cursor.Read(bytes.data(),bytes.size())==bytes.size(),"author cursor truncated");for(size_t i=0;i<bytes.size();++i)if(bytes[i]!=static_cast<unsigned char>((at+i)*13))correct=false;}}catch(...){correct=false;}});
  for(auto& thread:threads)thread.join();Require(correct,"streamed author concurrent cursors differ");view.reset();Require(weak.expired(),"streamed author lease not retired with final view");
  std::cout<<"PASS 64 native author graft differentials: current added fields, source hash independence, field alignment/overrides; streamedBytes="<<streamed<<" virtualAuthorBytes="<<huge<<" preparePayloadReads=0 concurrentCursors=4 finalLeaseReleased=1\n";
}
void PreloadIntervals() {
  Fixture f;const auto& type=f.T(300);auto& bundle=f.V(300);auto& preloads=ArrayValue(type,Child(type,bundle,"m_PreloadTable"));
  const NativeReference anchor{f.cab,100},a{"CAB-shader",501},b{"CAB-texture",602},local{f.cab,777};
  std::vector<NativeReference> input{anchor,a,anchor,b,{f.cab,20},anchor};preloads.children.clear();
  for(const auto& ref:input){auto value=ArrayElement(type,preloads);SetReference(type,value,ref,f.refs,f.cab);preloads.children.push_back(std::move(value));}preloads.count=input.size();
  std::vector<std::pair<size_t,size_t>> ranges{{0,2},{1,3},{2,4},{6,0},{0,0},{0,6},{1,1}};
  auto& container=ArrayValue(type,Child(type,bundle,"m_Container"));container.children.clear();
  for(size_t i=0;i<ranges.size();++i){auto entry=ArrayElement(type,container);SetString(type,Child(type,entry,"first"),"assets/interval-"+std::to_string(i));auto& info=Child(type,entry,"second");
    SetInteger(type,Child(type,info,"preloadIndex"),ranges[i].first);SetInteger(type,Child(type,info,"preloadSize"),ranges[i].second);container.children.push_back(std::move(entry));}container.count=container.children.size();
  auto& main=Child(type,bundle,"m_MainAsset");SetInteger(type,Child(type,main,"preloadIndex"),6);SetInteger(type,Child(type,main,"preloadSize"),0);
  auto raw=f.BytesOf();auto layout=ParseSerialized(raw);Require(layout.externals==f.refs.externals,"fixture lost cross-CAB references");
  const auto originalExternals=layout.externals;std::set<NativeReference> referenced;NativePreloadExpansion expansion{{anchor,{a,b,local,local,anchor}}};
  Require(ExpandNativePreloads(type,bundle,layout,f.cab,expansion,referenced)&&referenced==std::set<NativeReference>{anchor},"native preload expansion missed repeated anchor");
  const auto& output=ConstArray(type,Child(type,bundle,"m_PreloadTable"));
  for(size_t i=0;i<ranges.size();++i){const auto& info=Child(type,container.children[i],"second");auto first=Integer(Child(type,info,"preloadIndex")),count=Integer(Child(type,info,"preloadSize"));
    Require(first<=output.count&&count<=output.count-first,"native expanded interval exceeds table");
    std::set<NativeReference> actual;for(size_t j=size_t(first);j<first+count;++j)actual.insert(NullableReference(type,output.children[j],layout,f.cab));
    std::set<NativeReference> expected;for(size_t j=ranges[i].first;j<ranges[i].first+ranges[i].second;++j)expected.insert(input[j]);
    if(expected.count(anchor))expected.insert({a,b,local});Require(actual==expected,"native overlapping preload interval lost or added unrelated dependency");
  }
  auto once=EncodeValue(type,bundle);Require(!ExpandNativePreloads(type,bundle,layout,f.cab,expansion,referenced)&&once==EncodeValue(type,bundle)&&layout.externals==originalExternals,"native preload replay grows table or changes external prefix");
  auto invalid=bundle;SetInteger(type,Child(type,Child(type,invalid,"m_MainAsset"),"preloadSize"),1);Rejected([&]{ExpandNativePreloads(type,invalid,layout,f.cab,{},referenced);});
  Rejected([&]{ExpandNativePreloads(type,bundle,layout,f.cab,{{anchor,{{"",1}}}},referenced);});
  std::cout<<"PASS native preload intervals: cross-CAB references, overlapping/nested/empty ranges, repeated anchors, minimal closure and idempotent replay\n";
}
void NativeCatalogAssembly() {
  namespace D=EiemRuntimeAuthorDirectory;namespace N=EiemDemandNativeAssembly;
  namespace M=EiemAuthorResourceMetadata;namespace F=EiemAuthorResourceFile;
  auto folder=std::filesystem::temp_directory_path()/("eff-native-catalog-"+std::to_string(GetCurrentProcessId()));
  Require(!std::filesystem::exists(folder),"native catalog fixture exists");std::filesystem::create_directories(folder/"native/materials");std::filesystem::create_directories(folder/"native/meshes");
  struct Cleanup{std::filesystem::path root;~Cleanup(){std::error_code ignored;std::filesystem::remove_all(root,ignored);}}cleanup{folder};
  Fixture fixture;fixture.types[5]=Schema(43,{{"string","m_Name",{}},{"TypelessData","m_VertexData",{}},{"TypelessData","m_IndexBuffer",{}}});
  fixture.objects.at(100)={5,EmptyValue(fixture.types[5],0)};auto& mt=fixture.types[5];SetString(mt,Child(mt,fixture.V(100),"m_Name"),"Static");
  Child(mt,fixture.V(100),"m_VertexData").raw=Bytes(4096,17);Child(mt,fixture.V(100),"m_IndexBuffer").raw=Bytes(6,1);
  auto& bt=fixture.types[4];
  for(const char* name:{"m_Name","m_AssetBundleName"}){auto n=bt.nodes.size();bt.nodes.push_back({"string",name,false,{}});bt.nodes[0].children.push_back(n);fixture.V(300).children.push_back(EmptyValue(bt,n));}
  auto dn=bt.nodes.size();bt.nodes.push_back({"vector","m_Dependencies",false,{dn+1}});bt.nodes.push_back({"Array","Array",false,{dn+2,dn+3}});bt.nodes.push_back({"int","size",false,{}});bt.nodes.push_back({"string","data",false,{}});bt.nodes[0].children.push_back(dn);fixture.V(300).children.push_back(EmptyValue(bt,dn));
  auto& hashes=ArrayValue(bt,Child(bt,fixture.V(300),"m_HashContainer"));auto hashEntry=ArrayElement(bt,hashes);SetInteger(bt,Child(bt,hashEntry,"first"),42);SetReference(bt,Child(bt,Child(bt,hashEntry,"second"),"asset"),{fixture.cab,100},fixture.refs,fixture.cab);hashes.children={hashEntry};hashes.count=1;
  // The two native renderers intentionally select different original materials.
  fixture.Add(201,6);auto& secondMaterials=ArrayValue(fixture.T(23),Child(fixture.T(23),fixture.V(23),"m_Materials"));secondMaterials.children.clear();secondMaterials.count=0;
  fixture.List(23,"m_Materials",{201});auto raw=fixture.BytesOf();auto source=std::make_shared<const Bytes>(Bundle(raw,fixture.cab));
  auto read=[source](size_t at,unsigned char* out,size_t n){memcpy(out,source->data()+at,n);};
  for(size_t count:{size_t(0),size_t(1),size_t(3)}){
    D::Directory directory;directory.mod="native-fixture";directory.rules.push_back({"ReplaceStatic",{{0,{"assets/static.mesh","Static"}}},{},!count});
    D::MaterialEntry material;material.id="MaterialBody";material.file="native/materials/material.effres";material.source={"assets/game/material.mat","GameMaterial"};
    Bytes authorMaterial{'f','o','r','m','a','t','=','E','F','F','M','A','T','\n'};material.authorBytes=authorMaterial.size();auto materialAuthorHash=Sha(authorMaterial.data(),authorMaterial.size());material.hash=Hex(materialAuthorHash.data(),materialAuthorHash.size());directory.materials.push_back(material);
    EiemResourceReloadPlan::Save(folder/std::filesystem::u8path(material.file),F::Encode(2,M::EncodeMaterial(material),authorMaterial));
    for(size_t i=0;i<count;++i){D::MeshAsset mesh;mesh.part="PartP"+std::to_string(i);mesh.file="native/meshes/"+mesh.part+".effres";mesh.path="assets/static.mesh";mesh.marker="native-P"+std::to_string(i);mesh.material="MaterialBody";mesh.vertices=3;mesh.indices=3;mesh.fields={"m_Name","m_VertexData","m_IndexBuffer"};
      auto value=fixture.V(100);SetString(mt,Child(mt,value,"m_Name"),mesh.marker);Child(mt,value,"m_VertexData").raw.assign(1024*1024+37,static_cast<unsigned char>(i+3));
      auto asset=EiemAuthorMeshBundle::Isolate(raw,fixture.cab,100,mesh.path,value,std::string(64,static_cast<char>('a'+i)));auto digest=Sha(asset.bytes.data(),asset.bytes.size());mesh.hash=Hex(digest.data(),digest.size());
      Bytes editable{'E','F','F','M','E','S','H',0,6,0,0,0};mesh.nativeBytes=asset.bytes.size();mesh.authorBytes=editable.size();auto editableHash=Sha(editable.data(),editable.size());mesh.authorHash=Hex(editableHash.data(),editableHash.size());
      Bytes payload=asset.bytes;payload.insert(payload.end(),editable.begin(),editable.end());EiemResourceReloadPlan::Save(folder/std::filesystem::u8path(mesh.file),F::Encode(1,M::EncodeMesh(mesh),payload));directory.meshes.push_back(mesh);directory.rules.front().parts.push_back(mesh.part);
    }
    EiemAuthorControlMetadata::State controls;EiemResourceReloadPlan::Save(folder/"native/controls.effres",F::Encode(6,M::EncodeControls(controls),{}));
    std::ostringstream ini;ini<<"[Mod]\nformat=3\nid=native-fixture\nkey_switch_enabled=1\n\n[Constants]\n\n"
      <<"[MaterialBody]\npath="<<material.file<<"\nsource.path="<<material.source.path<<"\nsource.asset="<<material.source.asset<<"\n\n";
    ini<<"[ResourceControls]\ntype=controls\npath=native/controls.effres\n\n";
    for(const auto& mesh:directory.meshes){auto suffix=mesh.part.substr(strlen("Part"));ini<<"[Mesh"<<suffix<<"]\npath="<<mesh.file<<"\n\n["<<mesh.part<<"]\nmesh=Mesh"<<suffix<<"\nmaterial=MaterialBody\nvisible=1\n\n";}
    ini<<"[MeshUnhit]\npath=missing/unhit.mesh.effres\n\n"
      <<"[PartUnhit]\nmesh=MeshUnhit\nmaterial=MaterialBody\nvisible=1\n\n";
    ini<<"[ReplaceStatic]\ntarget.path=assets/static.mesh\ntarget.asset=Static\n";
    if(count){ini<<"parts=";for(size_t i=0;i<count;++i){if(i)ini<<',';ini<<"PartP"<<i;}ini<<'\n';}else ini<<"handling=skip\n";
    ini<<"\n[ReplaceUnhit]\ntarget.path=assets/unhit.mesh\ntarget.asset=Unhit\nparts=PartUnhit\n";
    {std::ofstream output(folder/"mod.ini",std::ios::binary|std::ios::trunc);output<<ini.str();Require(bool(output),"native Catalog Mod INI output failed");}
    EiemResourceReloadPlan::Save(folder/"assets.bin",Bytes{'o','l','d'});auto catalog=std::make_shared<EiemDemandAuthorResources::Catalog>(folder);
    Require(!catalog->OpenMeshes(),"native catalog eagerly opened an authored asset");N::Author author(catalog);R::Reader current(source->size(),read);auto layout=current.Layout(current.directory.members[0]);
    AssetIndex targets;targets[{"assets/static.mesh","Static"}]={};auto& target=targets.begin()->second;target.cab=fixture.cab;target.pathId=100;target.classId=43;
    auto carrier=N::PrepareCarrier(current,0,layout,author,targets);auto prepared=S::BuildRanges(current,{carrier.plan.rewrite});auto view=prepared.Bind(read);
    Require(carrier.parts.size()==count&&carrier.addedObjects==(count?count-1:0)&&catalog->OpenMeshes()==count,"native 0/1/N carrier resource count differs");
    if(count){auto first=catalog->GetMesh("PartP0");Require(first->metadataReadBytes<64*1024&&first->object.size>1024*1024,"native metadata read includes authored vertex payload");
      std::vector<std::thread> threads;std::atomic<bool> same{true};for(size_t i=0;i<4;++i)threads.emplace_back([&]{if(catalog->GetMesh("PartP0")!=first)same=false;});for(auto& thread:threads)thread.join();Require(same,"concurrent author asset requests duplicated a native input");}
    // Reopen the carrier VIEW, without serializing a temporary complete input.
    auto viewRead=[view](size_t at,unsigned char* out,size_t n){Require(view->ReadAt(at,out,n)==n,"carrier view read truncated");};R::Reader consumer(view->Size(),viewRead);auto updated=consumer.Layout(consumer.directory.members[0]);
    auto built=N::PrepareConsumer(consumer,0,updated,author,targets,carrier.parts);auto output=S::BuildRanges(consumer,{built.plan.rewrite});auto actual=EndfieldInput::Decode(Materialize(output.Bind(viewRead)));auto after=ParseSerialized(actual.members[0].bytes);
    // Carrier and consumer may live in the same actual native input. Publish
    // one object table without materializing/reopening a full intermediate.
    EiemNativeEditTransaction::Member transaction(current,0,layout);
    auto stagedCarrier=N::StageCarrier(transaction,current,0,author,targets);
    if(count){auto projected=transaction.Project(100,{"m_Name"});
      Require(Text(Child(mt,projected,"m_Name").raw.data(),Child(mt,projected,"m_Name").raw.size())=="native-P0","range overlay projection returned the old Mesh");
      auto bounded=current.limits.fieldBytes;current.limits.fieldBytes=1024;
      Rejected([&]{transaction.Object(100);});current.limits.fieldBytes=bounded;
    }
    auto stagedConsumer=N::StageConsumer(transaction,author,targets,stagedCarrier.parts);
    auto single=S::BuildRanges(current,{transaction.Commit().rewrite});
    auto combined=EndfieldInput::Decode(Materialize(single.Bind(read)));auto cl=ParseSerialized(combined.members[0].bytes);
    Require(cl.order==after.order&&cl.externals==after.externals&&stagedConsumer.result.matches==built.result.matches,"single native assembly transaction lost objects or dependencies");
    for(auto id:after.order){const auto& a=after.objects.at(id);const auto& c=cl.objects.at(id);
      Require(a.type==c.type&&a.size==c.size&&std::equal(actual.members[0].bytes.begin()+a.offset,actual.members[0].bytes.begin()+a.offset+a.size,combined.members[0].bytes.begin()+c.offset),"single native transaction differs from staged oracle");}
    Require(built.result.matches==2&&built.result.parts.size()==count*2,"native consumer 0/1/N did not cover both Prefab roots");
    std::map<std::string,NativeReference> shared;
    for(const auto& part:built.result.parts){auto inserted=shared.emplace(part.part,NativeReference{part.meshCab,part.mesh});Require(inserted.second||inserted.first->second==NativeReference{part.meshCab,part.mesh},"native consumers duplicated an authored Mesh");
      const auto& type=after.types.at(after.objects.at(part.renderer).type);auto value=ObjectValue(actual.members[0].bytes,after,part.renderer);const auto& materials=ConstArray(type,Child(type,value,"m_Materials"));
      Require(NullableReference(type,materials.children[0],after,fixture.cab).second==(part.sourceRenderer==13?200:201),"source-material mode reused another renderer's Material: source="+std::to_string(part.sourceRenderer)+" renderer="+std::to_string(part.renderer)+" material="+std::to_string(NullableReference(type,materials.children[0],after,fixture.cab).second));}
    Require(shared.size()==count,"native carrier generated duplicate resources per consumer");
    if(count){
      EiemNativeEditTransaction::Member variants(current,0,layout);auto variantCarrier=N::StageCarrier(variants,current,0,author,targets);
      auto materialType=layout.objects.at(200).type;auto variant200=EncodeValue(layout.types.at(materialType),variants.Object(200));
      auto variant201=EncodeValue(layout.types.at(materialType),variants.Object(201));
      variants.Stage(variants.Layout(),{},{{210,materialType,std::move(variant200)},{211,materialType,std::move(variant201)}});
      RendererMaterialReferences references;MaterialPreloadIndex closures;
      for(const auto& part:variantCarrier.parts)for(auto sourceRenderer:{int64_t(13),int64_t(23)}){
        NativeReference selected{fixture.cab,sourceRenderer==13?210:211};
        references.emplace(RendererMaterialKey{sourceRenderer,part.part},selected);closures[selected].insert(selected);
      }
      auto assembled=N::StageConsumer(variants,author,targets,variantCarrier.parts,&references,&closures);
      auto variantView=S::BuildRanges(current,{variants.Commit().rewrite}).Bind(read);Bytes bytes(variantView->Size());
      Require(variantView->ReadAt(0,bytes.data(),bytes.size())==bytes.size(),"per-Renderer material variant view truncated");
      auto result=EndfieldInput::Decode(bytes);auto variantLayout=ParseSerialized(result.members[0].bytes);
      Require(assembled.result.parts.size()==count*2,"per-Renderer material mapping lost an authored Part");
      for(const auto& part:assembled.result.parts){const auto& type=variantLayout.types.at(variantLayout.objects.at(part.renderer).type);
        auto renderer=ObjectValue(result.members[0].bytes,variantLayout,part.renderer);const auto& materials=ConstArray(type,Child(type,renderer,"m_Materials"));
        const auto expected=part.sourceRenderer==13?210:211;
        Require(NullableReference(type,materials.children.front(),variantLayout,fixture.cab)==NativeReference{fixture.cab,expected},
          "per-Renderer material variant crossed source donor: source="+std::to_string(part.sourceRenderer)+" actual="+
          std::to_string(NullableReference(type,materials.children.front(),variantLayout,fixture.cab).second)+" expected="+std::to_string(expected));
      }
      std::set<NativeReference> preloadReferences;bool foundBundle=false;
      for(auto id:variantLayout.order){const auto& type=variantLayout.types.at(variantLayout.objects.at(id).type);if(type.classId!=142)continue;
        foundBundle=true;auto bundle=ObjectValue(result.members[0].bytes,variantLayout,id);
        for(const auto& reference:ConstArray(type,Child(type,bundle,"m_PreloadTable")).children)
          preloadReferences.insert(NullableReference(type,reference,variantLayout,fixture.cab));
      }
      Require(foundBundle&&preloadReferences.count({fixture.cab,210})&&preloadReferences.count({fixture.cab,211}),
        "per-Renderer Material variants are absent from native preload closure");
    }
  }
  std::cout<<"PASS native catalog/assembly: lazy file metadata, 0/1/N shared Meshes, two Prefab roots, original per-renderer Materials, LOD/helper/preload expansion and concurrent asset deduplication\n";
}
void RetainedSourceLease() {
  auto folder=std::filesystem::temp_directory_path()/("eff-demand-source-lease-"+std::to_string(GetCurrentProcessId()));Require(!std::filesystem::exists(folder),"source lease fixture directory exists");std::filesystem::create_directory(folder);
  auto path=folder/"source.bundle",retired=folder/"retired.bundle";Bytes data(4096);for(size_t i=0;i<data.size();++i)data[i]=static_cast<unsigned char>(i&255);
  auto input=Bundle(data,"lease");{std::ofstream out(path,std::ios::binary);out.write(reinterpret_cast<const char*>(input.data()),input.size());}
  std::shared_ptr<const EiemInputSpanView::View> view;std::weak_ptr<R::File> weak;
  {auto file=std::make_shared<R::File>(path,0);weak=file;auto read=[file](size_t at,unsigned char* out,size_t n){file->ReadAt(at,out,n);};R::Reader reader(file->Size(),read);auto prepared=S::Build(reader,{});view=prepared.Bind(read);}
  Require(!weak.expired(),"native view lost its source lease after preparation");
  std::filesystem::rename(path,retired);{std::ofstream out(path,std::ios::binary);out<<"new generation";}std::filesystem::remove(retired);
  std::atomic<bool> correct{true};std::vector<std::thread> threads;
  for(size_t ti=0;ti<4;++ti)threads.emplace_back([&,ti]{try{EiemInputSpanView::Cursor cursor(view);for(size_t k=0;k<40;++k){auto at=(ti*977+k*67)%(input.size()-64);cursor.Seek(int64_t(at),0);Bytes bytes(64);Require(cursor.Read(bytes.data(),bytes.size())==bytes.size(),"retained source short read");if(!std::equal(bytes.begin(),bytes.end(),input.begin()+at))correct=false;}}catch(...){correct=false;}});
  for(auto& thread:threads)thread.join();Require(correct,"concurrent streams reopened the changed path or lost source position isolation");
  view.reset();Require(weak.expired(),"native view source lease survived final cursor retirement");std::filesystem::remove(path);std::filesystem::remove(folder);
  std::cout<<"PASS source lifetime: prepare scope ended, rename/delete/replace allowed, four independent cursors retain old bytes, final view releases handle\n";
}
void InspectObserved(const std::filesystem::path& path,uint64_t offset) {
  R::File file(path,offset);R::Reader reader(file.Size(),[&](size_t at,unsigned char* out,size_t n){file.ReadAt(at,out,n);});
  for(const auto& member:reader.directory.members){if(!(member.flags&4))continue;auto layout=reader.Layout(member);for(const auto& asset:reader.Assets(member,layout))std::cout<<"OBSERVED asset="<<asset.path<<" name="<<asset.asset<<" class="<<asset.classId<<" cab="<<asset.cab<<" pathId="<<asset.pathId<<'\n';}
  file.VerifyStable();std::cout<<"OBSERVED inputBytes="<<reader.header.bytes<<" readBytes="<<reader.stats.encodedBytes<<" decodedBlocks="<<reader.stats.inflatedBlocks<<'\n';
}
void InspectNativeAuthor(const std::filesystem::path& path,uint64_t offset,const std::filesystem::path& nativeRoot,
    const std::string& assetName,const std::filesystem::path& consumerPath,uint64_t consumerOffset) {
  namespace N=EiemDemandNativeAssembly;
  auto begin=std::chrono::steady_clock::now();auto catalog=std::make_shared<EiemDemandAuthorResources::Catalog>(nativeRoot);N::Author author(catalog);
  auto declarationMs=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count();Require(!catalog->OpenMeshes(),"catalog eagerly opened native Mesh payloads");
  auto file=std::make_shared<R::File>(path,offset);auto read=[file](size_t at,unsigned char* out,size_t n){file->ReadAt(at,out,n);};R::Reader reader(file->Size(),read);
  AssetIndex targets;std::vector<BuiltMeshPart> parts;size_t matches=0;
  for(size_t mi=0;mi<reader.directory.members.size();++mi){const auto& member=reader.directory.members[mi];if(!(member.flags&4))continue;auto layout=reader.Layout(member);
    for(const auto& asset:reader.Assets(member,layout))if(asset.asset==assetName&&author.package.targetIndex.count({asset.path,asset.asset}))targets.emplace(Identity{asset.path,asset.asset},asset);
    if(targets.empty())continue;auto started=std::chrono::steady_clock::now();auto carrier=N::PrepareCarrier(reader,mi,layout,author,targets);
    auto prepared=S::BuildRanges(reader,{carrier.plan.rewrite});auto elapsed=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count();
    Require(!carrier.parts.empty()&&prepared.streamedAuthorBytes,"native carrier did not stream author data");
    auto actual=EndfieldInput::Decode(Materialize(prepared.Bind(read)));auto after=ParseSerialized(actual.members[mi].bytes);
    for(const auto& part:carrier.parts){auto mesh=catalog->GetMesh(part.part);const auto& type=after.types.at(after.objects.at(part.id).type);auto value=ObjectValue(actual.members[mi].bytes,after,part.id);
      Require(Text(Child(type,value,"m_Name").raw.data(),Child(type,value,"m_Name").raw.size())==mesh->declaration.marker,"native carrier lost stable author marker");
      Require(type.nativeRecord==layout.types.at(layout.objects.at(part.sourceId).type).nativeRecord,"native carrier changed current game metadata");
      auto originalRanges=reader.Ranges(member,layout,part.sourceId);std::set<std::string> selected(mesh->declaration.fields.begin(),mesh->declaration.fields.end());
      for(const auto& field:originalRanges)if(!selected.count(field.first)){
        auto original=reader.Fields(member,layout,part.sourceId,{field.first});
        Bytes a,e;WriteValue(type,Child(type,value,field.first),a);WriteValue(type,original.at(field.first),e);
        Require(a==e,"native carrier changed an unselected current field");
      }
      std::cout<<"NATIVE-AUTHOR part="<<part.part<<" authorMetadataReadBytes="<<mesh->metadataReadBytes<<" authorObjectBytes="<<mesh->object.size<<" bones="<<mesh->declaration.bones.size()<<'\n';
    }
    std::cout<<"NATIVE-CARRIER parts="<<carrier.parts.size()<<" changedObjects="<<carrier.changedObjects<<" addedObjects="<<carrier.addedObjects<<" streamedAuthorBytes="<<prepared.streamedAuthorBytes<<" literalBytes="<<prepared.literalBytes<<" catalogMs="<<declarationMs<<" prepareMs="<<elapsed<<" payloadDecodedByEff=0 oracleExcluded=1\n";
    parts.insert(parts.end(),carrier.parts.begin(),carrier.parts.end());++matches;
  }
  Require(matches==1,"native author fixture did not resolve one actual target member");file->VerifyStable();
  if(consumerPath.empty())return;auto consumerFile=std::make_shared<R::File>(consumerPath,consumerOffset);
  auto consumerRead=[consumerFile](size_t at,unsigned char* out,size_t n){consumerFile->ReadAt(at,out,n);};R::Reader consumer(consumerFile->Size(),consumerRead);size_t rendererMatches=0;
  for(size_t mi=0;mi<consumer.directory.members.size();++mi){const auto& member=consumer.directory.members[mi];if(!(member.flags&4))continue;auto layout=consumer.Layout(member);auto started=std::chrono::steady_clock::now();
    auto built=N::PrepareConsumer(consumer,mi,layout,author,targets,parts);if(!built.result.matches)continue;auto prepared=S::BuildRanges(consumer,{built.plan.rewrite});
    auto elapsed=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count();
    auto actual=EndfieldInput::Decode(Materialize(prepared.Bind(consumerRead)));auto after=ParseSerialized(actual.members[mi].bytes);
    for(const auto& bound:built.result.parts){const auto& type=after.types.at(after.objects.at(bound.renderer).type);auto value=ObjectValue(actual.members[mi].bytes,after,bound.renderer);
      Require(NullableReference(type,Child(type,value,"m_Mesh"),after,member.name)==NativeReference{bound.meshCab,bound.mesh},"native consumer shared Mesh reference differs");
      Require(ConstArray(type,Child(type,value,"m_Bones")).count==author.package.meshes.at(author.package.parts.at(bound.part).mesh).bones.size(),"native consumer bone palette incomplete");
    }
    rendererMatches+=built.result.matches;
    std::cout<<"NATIVE-CONSUMER matches="<<built.result.matches<<" parts="<<built.result.parts.size()<<" changedObjects="<<built.result.changedObjects<<" addedObjects="<<built.result.addedObjects<<" literalBytes="<<prepared.literalBytes<<" retainedEncodedBytes="<<prepared.reusedEncoded<<" prepareMs="<<elapsed<<" vertexBoundsComputed=0 authorPayloadDecoded=0 oracleExcluded=1\n";
  }
  consumerFile->VerifyStable();Require(rendererMatches,"native consumer did not match actual target references");
}
void InspectAuthorObserved(const std::filesystem::path& path,uint64_t offset,const std::filesystem::path& mod,const std::string& assetName,
    const std::filesystem::path& consumerPath={},uint64_t consumerOffset=0) {
  namespace P=EiemNativeSerializedSplice;
  auto begin=std::chrono::steady_clock::now();auto file=std::make_shared<R::File>(path,offset);
  auto read=[file](size_t at,unsigned char* out,size_t n){file->ReadAt(at,out,n);};R::Reader reader(file->Size(),read);
  auto targets=EiemMeshTargetDirectory::Read(mod);auto target=std::find_if(targets.begin(),targets.end(),[&](const auto& value){return value.source.asset==assetName;});
  Require(target!=targets.end()&&!target->skip&&target->parts==1,"observed author fixture requires exactly one declared Mesh part");
  size_t matches=0;
  for(size_t index=0;index<reader.directory.members.size();++index){const auto& member=reader.directory.members[index];if(!(member.flags&4))continue;
    auto layout=reader.Layout(member);auto assets=reader.Assets(member,layout);
    for(const auto& asset:assets){if(asset.cab!=member.name||asset.path!=target->source.path||asset.asset!=target->source.asset||asset.classId!=43)continue;
      const auto& object=layout.objects.at(asset.pathId);const auto& type=layout.types.at(object.type);
      auto original=reader.ReadAt(member.offset+object.offset,object.size,reader.limits.fieldBytes);auto value=DecodeValue(type,0,original);
      auto author=EiemDemandMeshPreparation::ReadAuthor(reader,index,layout,*target,asset);
      const auto& resource=*author->resource;const auto& mesh=author->mesh;auto modified=DecodeValue(type,0,*author->object);
      auto& sourcePalette=ArrayValue(type,Child(type,value,"m_BoneNameHashes"));auto& authorPalette=ArrayValue(type,Child(type,modified,"m_BoneNameHashes"));
      std::set<uint32_t> sourceHashes;for(size_t i=0;i<sourcePalette.raw.size();i+=4)sourceHashes.insert(uint32_t(Le(sourcePalette.raw,i,4)));
      size_t absent=0;for(size_t i=0;i<authorPalette.raw.size();i+=4)absent+=!sourceHashes.count(uint32_t(Le(authorPalette.raw,i,4)));
      auto encoded=author->object;auto plan=EiemDemandMeshPreparation::MeshPlan(reader,index,layout,*author);auto prepared=S::BuildRanges(reader,{plan.rewrite});
      file->VerifyStable();auto ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count();
      auto view=prepared.Bind(read);auto rewritten=EndfieldInput::Decode(Materialize(view));auto checked=ParseSerialized(rewritten.members[index].bytes);
      auto actual=ObjectValue(rewritten.members[index].bytes,checked,asset.pathId);
      Require(EncodeValue(type,actual)==*encoded&&checked.types.at(object.type).nativeRecord==type.nativeRecord,"observed author native payload/metadata differs");
      std::cout<<"AUTHOR-OBSERVED asset="<<asset.asset<<" pathId="<<asset.pathId<<" vertices="<<mesh.vertices<<" sourceBones="<<sourcePalette.count<<" authorBones="<<authorPalette.count<<" authorBonesAbsentFromOriginalPalette="<<absent
          <<" inputReadBytes="<<reader.stats.encodedBytes<<" authorResourceBytes="<<resource.bytes.size()<<" viewLiteralBytes="<<prepared.literalBytes<<" retainedEncodedBytes="<<prepared.reusedEncoded<<" prepareMs="<<ms<<" oracleExcluded=1 nativeSubmit=0 rendererBoneRebindingStillRequired=1\n";++matches;
      if(!consumerPath.empty()){
        auto consumerFile=std::make_shared<R::File>(consumerPath,consumerOffset);auto consumerRead=[consumerFile](size_t at,unsigned char* out,size_t n){consumerFile->ReadAt(at,out,n);};R::Reader consumer(consumerFile->Size(),consumerRead);
        size_t rendererCount=0;
        for(size_t mi=0;mi<consumer.directory.members.size();++mi){const auto& cm=consumer.directory.members[mi];if(!(cm.flags&4))continue;
          auto cl=consumer.Layout(cm);auto consumerBegin=std::chrono::steady_clock::now();
          auto preparedConsumer=EiemDemandMeshPreparation::PrepareConsumer(consumer,mi,cl,*author);if(!preparedConsumer.matches)continue;
          auto consumerPrepared=S::BuildRanges(consumer,{preparedConsumer.plan.rewrite});
          auto consumerMs=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-consumerBegin).count();
          auto actualInput=EndfieldInput::Decode(Materialize(consumerPrepared.Bind(consumerRead)));auto after=ParseSerialized(actualInput.members[mi].bytes);
          auto bytes=consumer.ReadAt(cm.offset,cm.size,consumer.limits.readBytes);auto refs=MeshConsumers(bytes,cl,cm.name,{{asset.cab,asset.pathId}});
          Require(refs.size()==preparedConsumer.matches&&after.order==cl.order,"observed consumer lost exact matches or changed object identities");
          for(const auto& reference:refs){const auto& rt=cl.types.at(cl.objects.at(reference.renderer).type);auto original=ObjectValue(bytes,cl,reference.renderer);auto changed=ObjectValue(actualInput.members[mi].bytes,after,reference.renderer);
            Require(ConstArray(rt,Child(rt,changed,"m_Bones")).count==mesh.bones.size(),"observed renderer bone count differs");
            Require(NullableReference(rt,Child(rt,changed,"m_Mesh"),after,cm.name)==NativeReference{asset.cab,asset.pathId},"observed renderer lost shared native Mesh identity");
            const auto& oldMaterials=ConstArray(rt,Child(rt,original,"m_Materials"));const auto& newMaterials=ConstArray(rt,Child(rt,changed,"m_Materials"));
            Require(newMaterials.count==1&&!oldMaterials.children.empty()&&NullableReference(rt,newMaterials.children[0],after,cm.name)==NullableReference(rt,oldMaterials.children[0],cl,cm.name),"acceptance changed selected original material reference");
          }
          for(size_t ti=0;ti<cl.types.size();++ti)Require(cl.types[ti].nativeRecord==after.types[ti].nativeRecord,"consumer lost native type metadata");
          std::cout<<"CONSUMER-OBSERVED cab="<<cm.name<<" rendererMatches="<<preparedConsumer.matches<<" authorBones="<<mesh.bones.size()<<" changedObjects="<<preparedConsumer.changedObjects<<" ownedLiteralBytes="<<consumerPrepared.literalBytes<<" retainedEncodedBytes="<<consumerPrepared.reusedEncoded<<" prepareMs="<<consumerMs<<" nativeSubmit=0 sharedMeshIdentityPreserved=1\n";rendererCount+=preparedConsumer.matches;
        }
        consumerFile->VerifyStable();Require(rendererCount,"observed consumer did not reference author Mesh");
      }
    }
  }
  Require(matches==1,"observed input did not contain exactly one requested Mesh identity");
}
void RealInputs(const std::filesystem::path& game,const std::filesystem::path& mod) {
  auto selection=EiemStaticSources::LoadMod(mod,game);size_t count=0;
  std::vector<std::string> chosen;
  auto choose=[&](const std::string& key){if(std::find(chosen.begin(),chosen.end(),key)==chosen.end())chosen.push_back(key);};
  for(const auto& asset:selection.assets)if(asset.first.find("chen/models/s_actor_chen_body_01_lod0.asset")!=std::string::npos)choose(asset.second);
  for(const auto& key:selection.candidates){choose(key);if(chosen.size()>=3)break;}
  if(chosen.size()<3)for(const auto& row:selection.inputs){choose(row.first);if(chosen.size()>=3)break;}
  // Only fixture selection uses the old source table. The reader takes the
  // observed file range and never reads this table or discovers consumers.
  for(const auto& key:chosen) {
    const auto& source=selection.inputs.at(key);auto begin=std::chrono::steady_clock::now();R::File file(source.physical,source.offset);
    R::Limits limits;limits.readBytes=128*1024*1024;
    R::Reader reader(file.Size(),[&](size_t at,unsigned char* out,size_t n){file.ReadAt(at,out,n);},limits);
    auto opened=std::chrono::steady_clock::now();auto directoryBytes=reader.stats.encodedBytes;
    size_t layouts=0,refs=0,assets=0;std::vector<std::pair<size_t,SerializedLayout>> parsed;
    for(size_t i=0;i<reader.directory.members.size();++i){const auto& m=reader.directory.members[i];if(!(m.flags&4))continue;
      auto layout=reader.Layout(m);auto names=reader.Assets(m,layout);auto hashes=reader.HashAssets(m,layout);assets+=names.size();++layouts;
      for(size_t a=0;a<std::min(names.size(),size_t(2));++a)std::cout<<"ASSET path="<<names[a].path<<" name="<<names[a].asset<<" id="<<names[a].pathId<<" class="<<names[a].classId<<'\n';
      if(!hashes.empty())std::cout<<"HASH entries="<<hashes.size()<<" first="<<hashes.begin()->first<<" id="<<hashes.begin()->second.pathId<<'\n';
      for(auto id:layout.order){const auto& t=layout.types.at(layout.objects.at(id).type);if(t.classId!=137&&t.classId!=33)continue;
        auto fields=reader.Fields(m,layout,id,{"m_Mesh","m_GameObject"});Require(fields.size()==2,"real Mesh reference projection incomplete");++refs;}
      parsed.emplace_back(i,std::move(layout));
    }
    file.VerifyStable();auto inspected=std::chrono::steady_clock::now();auto stats=reader.stats;
    // Read the full reference AFTER measuring partial access. This is a
    // differential oracle, never part of runtime preparation.
    auto encoded=EndfieldInput::Snapshot(source.physical,source.offset);auto full=EndfieldInput::Decode(encoded);
    for(const auto& p:parsed){const auto& m=reader.directory.members[p.first];auto complete=ParseSerialized(full.members[p.first].bytes);const auto& partial=p.second;
      Require(partial.order==complete.order&&partial.externals==complete.externals&&partial.types.size()==complete.types.size()&&partial.dataOffset==complete.dataOffset,"real metadata prefix differs from full decoder");
      for(size_t t=0;t<partial.types.size();++t)Require(partial.types[t].nativeRecord==complete.types[t].nativeRecord,"metadata native record was lost");
      auto hashes=reader.HashAssets(m,partial);size_t expectedHashes=0;
      for(auto id:complete.order){const auto& t=complete.types.at(complete.objects.at(id).type);if(t.classId!=142)continue;
        if(!std::any_of(t.nodes[0].children.begin(),t.nodes[0].children.end(),[&](size_t n){return t.nodes[n].name=="m_HashContainer";}))continue;
        auto container=ObjectFields(full.members[p.first].bytes,complete,id,{"m_HashContainer"});
        for(const auto& row:ArrayValue(t,Child(t,container,"m_HashContainer")).children){auto assetKey=Integer(Child(t,row,"first"));const auto& ptr=Child(t,Child(t,row,"second"),"asset");auto external=Integer(Child(t,ptr,"m_FileID"));
          auto found=hashes.find(assetKey);Require(found!=hashes.end()&&found->second.pathId==int64_t(Integer(Child(t,ptr,"m_PathID")))&&found->second.cab==(external?complete.externals.at(size_t(external)-1):m.name),"partial native asset-key mapping differs from full parser");++expectedHashes;}
      }
      Require(hashes.size()==expectedHashes,"partial hash container count differs");
      for(auto id:partial.order){const auto& o=partial.objects.at(id);const auto& q=complete.objects.at(id);Require(o.offset==q.offset&&o.size==q.size&&o.type==q.type,"partial object table differs");
        const auto& t=partial.types[o.type];if(t.classId!=137&&t.classId!=33)continue;
        auto fields=reader.Fields(m,partial,id,{"m_Mesh","m_GameObject"});auto expected=ObjectFields(full.members[p.first].bytes,complete,id,{"m_Mesh","m_GameObject"});
        for(const auto& f:fields)Require(EncodeValue(t,f.second)==EncodeValue(t,Child(t,expected,f.first)),"partial Renderer reference differs from full parser");
      }
    }
    auto ms=[](auto a,auto b){return std::chrono::duration<double,std::milli>(b-a).count();};
    std::cout<<"REAL "<<key<<" inputBytes="<<reader.header.bytes<<" directoryReadBytes="<<directoryBytes<<" partialReadBytes="<<stats.encodedBytes<<" inflatedBlocks="<<stats.inflatedBlocks<<" totalBlocks="<<reader.directory.blocks.size()<<" inflatedBytes="<<stats.inflatedBytes<<" layouts="<<layouts<<" assets="<<assets<<" renderers="<<refs<<" openMs="<<ms(begin,opened)<<" inspectMs="<<ms(opened,inspected)<<" oracleExcluded=1\n";
    if(++count==3)break;
  }
  Require(count,"no real input fixtures inspected");
}
int main(int argc,char** argv) {try{if(argc==8&&std::string(argv[1])=="--native-author-observed"){InspectNativeAuthor(std::filesystem::u8path(argv[2]),std::stoull(argv[3]),std::filesystem::u8path(argv[6]),argv[7],std::filesystem::u8path(argv[4]),std::stoull(argv[5]));return 0;}if(argc==4&&std::string(argv[1])=="--inspect"){InspectObserved(std::filesystem::u8path(argv[2]),std::stoull(argv[3]));return 0;}if(argc==6&&std::string(argv[1])=="--author-observed"){InspectAuthorObserved(std::filesystem::u8path(argv[2]),std::stoull(argv[3]),std::filesystem::u8path(argv[4]),argv[5]);return 0;}if(argc==8&&std::string(argv[1])=="--author-consumer-observed"){InspectAuthorObserved(std::filesystem::u8path(argv[2]),std::stoull(argv[3]),std::filesystem::u8path(argv[6]),argv[7],std::filesystem::u8path(argv[4]),std::stoull(argv[5]));return 0;}SparseInput();CompressedRanges();SpliceCompressed();SpliceMetadata();SpliceSparse();SpliceDifferential();SerializedPlanDifferential();SerializedPlanSparse();ProjectedRendererAssembly();StreamedAuthorGraft();PreloadIntervals();NativeCatalogAssembly();RetainedSourceLease();if(argc==3)RealInputs(std::filesystem::u8path(argv[1]),std::filesystem::u8path(argv[2]));else Require(argc==1,"usage: native_random_access_smoke [game mod]");std::cout<<"PASS native random access; no author payloads or native game objects created\n";return 0;}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}


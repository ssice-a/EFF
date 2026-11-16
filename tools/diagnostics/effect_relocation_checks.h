#pragma once
#include "../../src/eiem_effect_relocation.h"

namespace EffectRelocationTest {
inline void Checks(){
 using namespace EiemNativeInput;
 namespace R=EiemEffectRelocation;namespace V=EiemInputSpanView;
 const std::string marker="CAB-11111111111111111111111111111111",target="CAB-22222222222222222222222222222222";
 auto bytes=std::make_shared<Bytes>(128,0xA5);std::copy(marker.begin(),marker.end(),bytes->begin()+10);
 for(size_t i=0;i<8;++i)(*bytes)[64+i]=i?0:1;
 auto view=std::make_shared<const V::View>(std::vector<V::Span>{{V::literalSource,0,bytes->size(),bytes}},std::vector<size_t>{},V::ReadRange{});
 EiemEffectNative::GameResource resource;resource.path="assets/game/star.png";resource.type="Texture2D";
 resource.relocationCab=marker;resource.relocationId=1;resource.patches={{0,10,0},{0,64,1}};
 unsigned calls=0;
 auto resolve=[&](const auto& input)->R::Reference{++calls;Require(input.path==resource.path,"logical path changed before runtime resolution");return {target,-42};};
 auto bound=R::Bind(view,0,{resource},resolve);Bytes actual(128);bound->ReadAt(0,actual.data(),actual.size());
 auto expected=*bytes;std::copy(target.begin(),target.end(),expected.begin()+10);
 for(size_t i=0;i<8;++i)expected[64+i]=static_cast<unsigned char>(uint64_t(-42)>>(8*i));
 Require(actual==expected&&*bytes!=expected&&calls==1,"logical reference overlay changed unrelated bytes or original input");
 unsigned char cross[12];bound->ReadAt(60,cross,12);Require(std::equal(cross,cross+12,expected.begin()+60),"partial relocation reads differ");
 Require(R::Bind(view,1,{resource},resolve)==view&&calls==1,"unrelated bundle resolved a logical resource");
 auto lower=R::Bind(view,0,{resource},[&](const auto&)->R::Reference{return {"cab-"+target.substr(4),-42};});
 Require(V::SameBytes(*lower,*bound),"game CAB prefix casing changed logical binding");
 auto reject=[&](auto input,const char* label){bool failed=false;try{R::Bind(view,0,{input},resolve);}catch(const std::exception&){failed=true;}Require(failed,label);};
 auto bad=resource;bad.patches[0].offset=100;reject(bad,"out of range relocation accepted");
 bad=resource;bad.patches[0].offset=11;reject(bad,"incorrect CAB source bytes accepted");
 bad=resource;bad.relocationId=2;reject(bad,"incorrect PathID source bytes accepted");
 bad=resource;bad.patches[0].kind=2;reject(bad,"unknown relocation kind accepted");
 bad=resource;bad.patches.push_back({0,12,1});reject(bad,"overlapping relocation accepted");
 bool failed=false;try{R::Bind(view,0,{resource},[](const auto&)->R::Reference{return {"CAB-zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz",2};});}catch(const std::exception&){failed=true;}
 Require(failed,"invalid runtime CAB accepted");
 EiemEffectNative::AuthorResource author;author.resource="MaterialGlow";author.type="Material";
 author.relocationCab=marker;author.relocationId=1;author.patches=resource.patches;unsigned authorCalls=0;
 auto resolveAuthor=[&](const auto& input)->R::Reference{++authorCalls;Require(input.resource=="MaterialGlow","effect lost its shared INI Resource ID");return {target,-42};};
 auto authorBound=R::Bind(view,0,{},R::Resolver{},{author},resolveAuthor);
 Require(V::SameBytes(*authorBound,*bound)&&authorCalls==1,"author Resource binding diverged from native logical-resource relocation");
 Require(R::Bind(view,1,{},R::Resolver{},{author},resolveAuthor)==view&&authorCalls==1,"unused author Resource was opened");
 failed=false;try{R::Bind(view,0,{resource},resolve,{author},[](const auto&)->R::Reference{return {"CAB-33333333333333333333333333333333",2};});}catch(const std::exception&){failed=true;}
 Require(failed,"conflicting author/game relocations accepted");
 std::cout<<"PASS logical effect relocations: exact paths, signed native IDs, immutable range overlay, lazy resolution, malformed slot rejection\n";
}
}

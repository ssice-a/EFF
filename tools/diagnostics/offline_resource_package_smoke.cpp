#include "../../src/eiem_offline_resource_package.h"
#include "../../src/eiem_offline_cache.h"
#include <iostream>
using namespace EiemOfflinePackage;
Manifest Fixture(){
 Manifest m;m.baseline=std::string(64,'B');auto& p=m.package;p.id="LZY";p.root="mods/LZY";
 auto mesh=std::make_shared<Resource>();mesh->kind=1;mesh->bytes={1,2,3,4};p.resources.emplace("meshes/body.mesh",mesh);
 auto material=std::make_shared<Resource>();material->kind=2;material->bytes={5,6,7,8};p.resources.emplace("materials/body.mat",material);
 p.meshes.emplace("body",Mesh{"meshes/body.mesh",2,3,{"Root/Bip/Bone"},{"Shape"}});
 p.materials.emplace("body",EiemDirectory::Material{"materials/body.mat",{"assets/body.mat","body"},{}});
 p.defaults.emplace("$switch",0);p.defaults.emplace("$shape",.25);p.persistent={"$switch","$shape"};p.controls.emplace("$shape",Control{"Shape",0,1});
 std::string error;auto expr=EiemExpressionParser("$switch == 0",error).Parse();Require(bool(expr),"fixture expression");p.parts.emplace("body",EiemDirectory::Part{"body","body",expr,{{"Shape","$shape"}}});
 p.rules.emplace("body",Rule{{"body"},{{0,{"assets/body.asset","body"}}},false});p.targetIndex.emplace(Identity{"assets/body.asset","body"},"body");
 p.bindings.push_back(Binding{"switch","$switch","cycle",{VK_NUMPAD1,0},{0,1},1});
 auto& plan=m.plan;plan.key=std::string(64,'C');plan.author=PackageDigest(p);plan.contract=InputContractDigest(p);plan.inputs.push_back(Input{"input-001.vfs",std::string(64,'D'),"main/example.ab",{}, {}});plan.closure={0};
 Root root;root.path="assets/model.prefab";root.markers={{"marker",1}};root.cab="CAB-"+std::string(32,'a');root.source=1;
 EiemRenderAssemblyDescription::Source source;source.cab=root.cab;source.id=2;source.renderer={"4:Body:0/",0};EiemRenderAssemblyDescription::Target target;target.renderer=source.renderer;target.marker="marker";target.bones={"4:Root:0/3:Bip:0/4:Bone:0/"};source.targets.push_back(target);root.assembly.sources.push_back(source);
 plan.roots.push_back(root);plan.parts.emplace("marker",EiemResourceReloadPlan::Part{"body","marker","body",2,{"Root/Bip/Bone"},{"Shape"}});plan.materials.emplace("body",EiemResourceReloadPlan::Material{"body",{{"_BaseMap",true,false,{}}}});plan.meshParts=1;plan.rendererParts=1;plan.materialMatches=1;plan.rendererBoundsHits=2384;return m;
}
Manifest SetFixture(unsigned count){
 auto m=Fixture();auto original=m.package;m.composed=true;m.packages.clear();
 for(unsigned i=0;i<count;++i){auto p=original;p.id=i?"OtherCharacter"+std::to_string(i):"LZY";p.root=std::filesystem::path("mods")/p.id;
  if(i){auto target=Identity{"assets/other-character"+std::to_string(i)+"/body.asset","body"};p.rules.at("body").targets={{0,target}};p.targetIndex.clear();p.targetIndex.emplace(target,"body");}
  m.packages.push_back(std::move(p));
 }
 m.package=EiemComposedPackage::Compose(m.packages,true);auto& plan=m.plan;plan.composed=true;plan.author=PackageDigest(m.package);plan.contract=InputContractDigest(m.package);
 plan.parts.clear();plan.materials.clear();plan.roots.front().markers.clear();
 auto material=count?m.package.materials.begin()->first:std::string{};
 for(const auto& p:m.packages){auto marker=p.id+"_marker";auto id=EiemComposedPackage::Name(p.id,"body");plan.parts.emplace(marker,EiemResourceReloadPlan::Part{id,marker,material,2,{"Root/Bip/Bone"},{"Shape"}});plan.roots.front().markers.emplace(marker,1);}
 if(count)plan.materials.emplace(material,EiemResourceReloadPlan::Material{"body",{{"_BaseMap",true,false,{}}}});
 auto& descriptor=plan.roots.front().assembly.sources.front();descriptor.targets.clear();
 for(unsigned i=0;i<std::max(1u,count);++i){EiemRenderAssemblyDescription::Target t;t.renderer={"4:Body:0/",i};t.marker=count?m.packages[i].id+"_marker":"";t.extension=i>0;descriptor.targets.push_back(t);}
 plan.meshParts=plan.rendererParts=count;plan.materialMatches=count?1:0;return m;
}
// Test-only legacy writer: historical headers omit the two layout booleans.
Bytes LegacyBytes(const Manifest& v,const std::string& tag){
 auto bytes=Encode(v);EiemOfflinePackage::Reader header{bytes};header.S();header.B();header.B();header.S();
 Writer old;old.S(tag);old.S(std::string(64,'A'));old.Raw(bytes.data()+header.p,bytes.size()-header.p);return old.b;
}
int main(){try{
 auto m=Fixture();auto bytes=Encode(m);auto roundtrip=Decode(bytes,"candidate","mods");Require(Encode(roundtrip)==bytes,"manifest changed during roundtrip");
 Require(roundtrip.package.parts.at("body").visible->Evaluate({{"$switch",0}})==1&&roundtrip.package.parts.at("body").visible->Evaluate({{"$switch",1}})==0,"compiled expression behavior changed");
 Require(roundtrip.package.root==std::filesystem::path("mods/LZY")&&roundtrip.plan.parts.at("marker").bones==m.plan.parts.at("marker").bones&&roundtrip.package.persistent==m.package.persistent&&roundtrip.package.bindings.front().key.vk==VK_NUMPAD1,"compiled author metadata lost");
 auto reject=[](const Bytes& b){bool rejected=false;try{Decode(b,"candidate","mods");}catch(const std::exception&){rejected=true;}Require(rejected,"malformed offline manifest accepted");};
 auto bad=bytes;bad[8]^=1;reject(bad);bad=bytes;bad.pop_back();reject(bad);bad=bytes;std::fill(bad.begin(),bad.begin()+8,255);reject(bad);
 auto invalid=m;invalid.package.defaults.at("$shape")=NAN;bool rejected=false;try{Encode(invalid);}catch(const std::exception&){rejected=true;}Require(rejected,"nonfinite author encoded");
 invalid=Fixture();invalid.package.root="mods/..";reject(Encode(invalid));invalid=Fixture();invalid.package.parts.at("body").visible->right.reset();reject(Encode(invalid));
 invalid=Fixture();invalid.plan.author=std::string(64,'F');reject(Encode(invalid));invalid=Fixture();invalid.plan.roots[0].input=8;reject(Encode(invalid));
 invalid=Fixture();invalid.plan.parts.at("marker").vertices=3;reject(Encode(invalid));
 for(unsigned count:{0u,1u,2u,65u}){auto set=SetFixture(count);auto encoded=Encode(set);auto decoded=Decode(encoded,"candidate","mods");
  Require(decoded.composed&&decoded.plan.composed&&Authors(decoded).size()==count&&Encode(decoded)==encoded,"0/1/N Mod set manifest roundtrip differs");
  for(const auto& author:Authors(decoded)){auto part=EiemComposedPackage::Name(author.id,"body");Require(decoded.package.partOwners.at(part)==std::make_pair(author.id,std::string("body")),"original control owner lost during composition");
   Require(decoded.package.parts.at(part).visible->Evaluate(decoded.package.defaults)==1,"composed variable expression domain differs");}
 }
 // Both historical layouts remain readable by either compiler build.
 for(bool addresses:{false,true})for(bool composed:{false,true}){auto old=composed?SetFixture(2):Fixture();old.rootAssembly=addresses;
  auto tag=std::string(addresses?"EFF_NATIVE_PACK_V36_ASSEMBLY_1":"EFF_NATIVE_PACK_V35_1")+(composed?"_MODSET_1":"");
  auto encoded=LegacyBytes(old,tag);auto decoded=Decode(encoded,"candidate","mods");auto rewritten=Encode(decoded);
  Require(decoded.rootAssembly==addresses&&decoded.composed==composed&&rewritten==Encode(old)&&rewritten!=encoded,"legacy read/current rewrite differs");}
 for(bool addresses:{false,true})for(bool composed:{false,true}){
  auto old=composed?SetFixture(2):Fixture();old.rootAssembly=addresses;
  auto encoded=LegacyBytes(old,"EFF_NATIVE_PACK_ARBITRARY_RELEASE_999");auto decoded=Decode(encoded,"candidate","mods");Require(decoded.rootAssembly==addresses&&decoded.composed==composed&&Encode(decoded)==Encode(old),"same legacy structure rejected unknown release label");
 }
 auto large=Fixture();for(unsigned i=1;i<600;++i)large.plan.inputs.push_back(Input{"input-"+std::to_string(i)+".vfs",std::string(64,'D'),"main/example.ab",{}, {}});
 for(unsigned i=1;i<140;++i){auto root=large.plan.roots.front();root.path="assets/model-"+std::to_string(i)+".prefab";root.input=i;large.plan.roots.push_back(root);large.plan.closure.push_back(i);}
 for(unsigned i=0;i<600;++i)large.package.resources.emplace("materials/extra-"+std::to_string(i)+".mat",large.package.resources.at("materials/body.mat"));
 large.plan.author=PackageDigest(large.package);large.plan.contract=InputContractDigest(large.package);large.plan.opaque=100001;
 auto expanded=Decode(Encode(large),"candidate","mods");Require(expanded.plan.inputs.size()==600&&expanded.plan.roots.size()==140&&expanded.package.resources.size()==602&&expanded.plan.opaque==100001,"capacity policy still rejects valid descriptors");
 std::string expression;for(unsigned i=0;i<600;++i)expression+=(i?" && ":"")+std::string("$switch == 0");
 std::string expressionError;auto longExpression=EiemExpressionParser(expression,expressionError).Parse();
 Require(longExpression&&longExpression->Evaluate({{"$switch",0}})==1&&longExpression->Evaluate({{"$switch",1}})==0,"long expression failed without a policy ceiling");
 Writer expressionWriter;expressionWriter.Expr(longExpression);EiemOfflinePackage::Reader expressionReader{expressionWriter.b};auto expressionCopy=expressionReader.Expr();
 Require(expressionReader.p==expressionWriter.b.size()&&expressionCopy->Evaluate({{"$switch",0}})==1,"long expression descriptor rejected");
 {
  std::string deep="$switch == 0";for(unsigned i=1;i<20000;++i)deep+=" && $switch == 0";
  auto parsed=EiemExpressionParser(deep,expressionError).Parse();Require(parsed&&parsed->Evaluate({{"$switch",0}})==1,"deep expression evaluation failed");
  auto renamed=EiemComposedPackage::Expression(parsed,"owner_");EiemVariables vars{{"$owner_switch",0}};
  Require(renamed->Validate(vars,expressionError)&&renamed->Evaluate(vars)==1,"deep composed expression failed");
  Writer encoded;encoded.Expr(renamed);EiemOfflinePackage::Reader reader{encoded.b};auto restored=reader.Expr();
  Require(reader.p==encoded.b.size()&&restored->Evaluate(vars)==1,"deep expression roundtrip failed");
  // Leaving the scope also checks that last-reference release is iterative.
 }
 for(const auto& sample:std::vector<std::pair<std::string,double>>{{"!false && -2 < +1",1},{"(1 || $missing) && 2 == 2",1},{"0 && $missing",0},{"1 < 2 == true",1},{"!!(2 >= 3)",0}}){
  auto parsed=EiemExpressionParser(sample.first,expressionError).Parse();Require(parsed&&parsed->Evaluate({})==sample.second,"expression precedence/short circuit changed");}
 for(const auto& sample:{"", "()", "1 &&", "1 2", "!(1", "1)"})Require(!EiemExpressionParser(sample,expressionError).Parse(),"malformed expression accepted");
 // A raw native field larger than the former 64 MiB writer ceiling.
 Type rawType;rawType.nodes.push_back({"TypelessData","payload",false,{}});Value raw;raw.raw.resize(65ull*1024*1024,7);
 auto rawBytes=EncodeValue(rawType,raw);Require(DecodeValue(rawType,0,rawBytes).raw==raw.raw,"large serialized field rejected");
 std::string utf8(1024*1024-1,'a');utf8+="\xF0\x9F\x98\x80";utf8+='b';
 Require(Text(reinterpret_cast<const unsigned char*>(utf8.data()),utf8.size())==utf8,"UTF-8 chunk boundary rejected codepoint");
 utf8.pop_back();utf8.pop_back();rejected=false;try{Text(reinterpret_cast<const unsigned char*>(utf8.data()),utf8.size());}catch(const std::exception&){rejected=true;}Require(rejected,"truncated UTF-8 accepted at chunk boundary");
 Bytes native(48);Bytes metadata={'s','o','u','r','c','e',0};AppendInteger(metadata,0,4);metadata.push_back(1);for(unsigned i=0;i<4;++i)AppendInteger(metadata,0,4);
 native.insert(native.end(),metadata.begin(),metadata.end());WriteInteger(native,8,999,4,true);WriteInteger(native,20,metadata.size(),4,true);WriteInteger(native,24,native.size(),8,true);WriteInteger(native,32,native.size(),8,true);
 Require(SerializedHeader(native)&&ParseSerialized(native).objects.empty(),"serialized release label rejected identical extended structure");
 auto broken=native;WriteInteger(broken,24,native.size()+1,8,true);Require(!SerializedHeader(broken),"invalid serialized total size accepted");
 // UnityFS layout is selected by complete ranges, even when its label would
 // conventionally choose the other header alignment.
 auto bundle=[&](unsigned label,bool padded,bool infoAtEnd){
  auto append=[](Bytes& bytes,uint64_t value,size_t width){auto offset=bytes.size();bytes.resize(offset+width);WriteInteger(bytes,offset,value,width,true);};
  Bytes info(16);append(info,1,4);append(info,native.size(),4);append(info,native.size(),4);append(info,0,2);
  append(info,1,4);append(info,0,8);append(info,native.size(),8);append(info,0,4);
  const std::string member="CAB-structure";info.insert(info.end(),member.begin(),member.end());info.push_back(0);
  Bytes output={'U','n','i','t','y','F','S',0};append(output,label,4);
  for(const auto& value:{std::string("engine"),std::string("arbitrary-release")}){output.insert(output.end(),value.begin(),value.end());output.push_back(0);}
  const auto sizeOffset=output.size();append(output,0,8);append(output,info.size(),4);append(output,info.size(),4);append(output,infoAtEnd?0x80:0,4);
  if(padded)output.resize(AlignSize(output.size(),16),0);
  const auto& first=infoAtEnd?native:info;const auto& second=infoAtEnd?info:native;
  output.insert(output.end(),first.begin(),first.end());output.insert(output.end(),second.begin(),second.end());WriteInteger(output,sizeOffset,output.size(),8,true);return output;
 };
 for(bool padded:{false,true})for(bool atEnd:{false,true})for(unsigned label:{6u,999u})
  Require(EiemNativeInput::Inspect(bundle(label,padded,atEnd)).empty(),"UnityFS release label chose header layout");
 auto malformedBundle=bundle(999,true,false);malformedBundle.pop_back();rejected=false;
 try{EiemNativeInput::Inspect(malformedBundle);}catch(const std::exception&){rejected=true;}Require(rejected,"truncated UnityFS structure accepted");
 auto authors=SetFixture(2).packages;auto composed=EiemComposedPackage::Compose(authors,true);
 Require(composed.materials.size()==1&&composed.meshes.size()==2&&composed.targetIndex.size()==2,"distinct character targets/shared Material not composed once");
 bool conflict=false;auto collision=authors;collision[1].materials.at("body").resource="materials/different.mat";auto resource=std::make_shared<Resource>();resource->kind=2;resource->bytes={9};collision[1].resources.emplace("materials/different.mat",resource);
 try{EiemComposedPackage::Compose(collision,true);}catch(const std::exception&){conflict=true;}Require(conflict,"conflicting shared native Material silently replaced");
 collision=authors;collision[1].rules=collision[0].rules;conflict=false;try{EiemComposedPackage::Compose(collision,true);}catch(const std::exception&){conflict=true;}Require(conflict,"conflicting native Mesh targets silently replaced");
 auto cache=std::filesystem::temp_directory_path()/("eff-offline-smoke-"+std::to_string(GetCurrentProcessId()));auto folder=cache/"generations"/m.plan.key;std::filesystem::create_directories(folder);Save(folder/"manifest.bin",bytes);Pointer pointer{m.plan.key,Hash(bytes)};Publish(cache,pointer);
 Require(ReadCurrent(cache)==pointer&&ReadManifest(cache,pointer,"mods").plan.author==m.plan.author,"atomic publication not readable");bad=bytes;bad.back()^=1;Save(folder/"manifest.bin",bad);rejected=false;try{ReadManifest(cache,pointer,"mods");}catch(const std::exception&){rejected=true;}Require(rejected&&ReadCurrent(cache)==pointer,"changed payload bypassed published SHA");
 Save(folder/"manifest.bin",bytes);auto author=m.package;author.root=cache/"mods/LZY";std::filesystem::create_directories(author.root);
 // Reference-only manifests keep payloads in the author files. Optional
 // namespace proofs are compressed in the same descriptor, not another PFB.
 auto rules=m;rules.plan.rulesOnly=true;auto& rulePlan=rules.plan;
 for(const auto& resource:rules.package.resources){auto path=cache/"mods/LZY"/std::filesystem::u8path(resource.first);std::filesystem::create_directories(path.parent_path());Save(path,resource.second->bytes);}
 rulePlan.originals.push_back({rulePlan.inputs.front().file,std::string(64,'E'),cache/"source.bin",0,128});rulePlan.startupHashes={std::string(64,'F')};rulePlan.rendererRules.resize(1);
 auto oldCab=rulePlan.roots.front().cab,newCab="CAB-"+std::string(32,'b');rulePlan.names.members.emplace(oldCab,newCab);
 auto legacyRules=Encode(rules);auto decodedRules=Decode(legacyRules,folder,cache/"mods");Require(decodedRules.plan.namespaceRecipes.empty()&&Encode(decodedRules)==legacyRules,"reference rules without namespace proof lost compatibility");
 rulePlan.namespaceRecipes={{{oldCab,std::string(64,'E'),{{64,oldCab,newCab}}}}};auto provenRules=Encode(rules);decodedRules=Decode(provenRules,folder,cache/"mods");
 Require(Encode(decodedRules)==provenRules&&decodedRules.plan.namespaceRecipes.front().front().patches.front().offset==64,"compressed namespace proof roundtrip differs");
 auto rejectRules=[&](const Manifest& invalid){bool rejected=false;try{Decode(Encode(invalid),folder,cache/"mods");}catch(const std::exception&){rejected=true;}Require(rejected,"malformed namespace proof accepted");};
 auto malformedRules=rules;malformedRules.plan.namespaceRecipes[0][0].hash="invalid";rejectRules(malformedRules);
 malformedRules=rules;malformedRules.plan.namespaceRecipes[0][0].member="absent-member";rejectRules(malformedRules);
 malformedRules=rules;malformedRules.plan.namespaceRecipes[0][0].patches[0].after.pop_back();rejectRules(malformedRules);
 malformedRules=rules;malformedRules.plan.namespaceRecipes[0][0].patches[0].after=oldCab;rejectRules(malformedRules);
 malformedRules=rules;malformedRules.plan.namespaceRecipes[0].push_back(malformedRules.plan.namespaceRecipes[0][0]);rejectRules(malformedRules);
 malformedRules=rules;malformedRules.plan.namespaceRecipes.emplace_back();rejectRules(malformedRules);
 auto copyRules=rules;copyRules.plan.spanRecipes=true;EiemResourceRecipe::Member member;member.name=oldCab;member.flags=4;member.size=6;Bytes rebuilt={2,3,7,8,9,10};member.hash=Hash(rebuilt);
 member.spans={{EiemResourceRecipe::Author,"meshes/body.mesh",1,2,{}},{EiemResourceRecipe::Literal,{},0,4,{7,8,9,10}}};copyRules.plan.memberRecipes={{member}};
 auto copied=Decode(Encode(copyRules),folder,cache/"mods");Require(copied.plan.spanRecipes&&Encode(copied)==Encode(copyRules)&&EiemResourceRecipe::Apply(copied.plan.memberRecipes[0][0],{},copied.package,{})==rebuilt,"compressed copy recipe roundtrip/output differs");
 malformedRules=copyRules;malformedRules.plan.memberRecipes[0][0].spans[0].offset=SIZE_MAX;rejectRules(malformedRules);
 malformedRules=copyRules;malformedRules.plan.memberRecipes[0][0].spans[0].source="materials/absent.mat";rejectRules(malformedRules);
 malformedRules=copyRules;malformedRules.plan.memberRecipes[0][0].spans[1].literal.pop_back();rejectRules(malformedRules);
 malformedRules=copyRules;malformedRules.plan.memberRecipes[0][0].size=7;rejectRules(malformedRules);
 malformedRules=copyRules;malformedRules.plan.memberRecipes[0].push_back(member);rejectRules(malformedRules);
 // Reordered and repeated source/author spans, insertions and deletions are
 // reconstructed exactly; corrupt sources cannot bypass the final digest.
 Bytes source(1024),payload(512);for(size_t i=0;i<source.size();++i)source[i]=unsigned char(i*73+i/7);for(size_t i=0;i<payload.size();++i)payload[i]=unsigned char(i*31+i/11);
 EndfieldInput::Member output{oldCab,4,{}};output.bytes.insert(output.bytes.end(),source.begin()+111,source.begin()+711);output.bytes.insert(output.bytes.end(),payload.begin()+31,payload.end());output.bytes.insert(output.bytes.end(),{1,0,2,0});output.bytes.insert(output.bytes.end(),source.begin()+16,source.begin()+80);
 EiemResourceRecipe::Dictionary sourceDictionary({{EiemResourceRecipe::Original,oldCab,&source}}),authorDictionary({{EiemResourceRecipe::Author,"meshes/body.mesh",&payload}});
 auto recipe=EiemResourceRecipe::Describe(output,sourceDictionary,authorDictionary);auto copyPackage=rules.package;auto payloadResource=std::make_shared<Resource>(*copyPackage.resources.at("meshes/body.mesh"));payloadResource->bytes=payload;copyPackage.resources["meshes/body.mesh"]=payloadResource;
 Require(EiemResourceRecipe::Apply(recipe,{{oldCab,&source}},copyPackage,{})==output.bytes,"offline discovered copy spans differ");auto originalSpan=std::find_if(recipe.spans.begin(),recipe.spans.end(),[](const auto& span){return span.kind==EiemResourceRecipe::Original;});Require(originalSpan!=recipe.spans.end(),"fixture never copied its original source");source[originalSpan->offset]^=1;bool recipeRejected=false;try{EiemResourceRecipe::Apply(recipe,{{oldCab,&source}},copyPackage,{});}catch(const std::exception&){recipeRejected=true;}Require(recipeRejected,"changed source copy bypassed member digest");
 Bytes packedFrame(48);const float position[]={1,2,3};for(size_t i=0;i<2;++i){memcpy(packedFrame.data()+i*16,position,12);WriteInteger(packedFrame,i*16+12,0x40000000,4);}for(size_t i=32;i<48;++i)packedFrame[i]=unsigned char(i);
 EiemResourceRecipe::Frame frame;frame.resource="meshes/body.mesh";frame.size=48;frame.count=2;frame.stride={16,8,0,0};frame.start={0,32,48,48};auto expandedFrame=EiemResourceRecipe::ExpandFrame(frame,packedFrame);
 Require(expandedFrame.size()==96&&std::equal(packedFrame.begin()+32,packedFrame.end(),expandedFrame.begin()+80),"offline described frame expansion lost secondary streams");
 for(size_t i=0;i<2;++i){float actual[10];memcpy(actual,expandedFrame.data()+i*40,40);Require(actual[0]==1&&actual[1]==2&&actual[2]==3&&actual[3]==0&&actual[4]==0&&actual[5]==1&&std::abs(actual[6]+.70710678f)<1e-6&&std::abs(actual[7]-.70710678f)<1e-6&&actual[8]==0&&actual[9]==-1,"offline described frame expansion changed positions/normal/tangent");}
 frame.hash=Hash(expandedFrame);Require(EiemResourceRecipe::ExpandFrame(frame,packedFrame)==expandedFrame,"verified frame expansion differs");packedFrame[0]^=1;recipeRejected=false;try{EiemResourceRecipe::ExpandFrame(frame,packedFrame);}catch(const std::exception&){recipeRejected=true;}Require(recipeRejected,"changed frame source bypassed expected output digest");
 EiemOfflineCache::Claim(cache,author);auto singleOwner=Read(cache/"owner.tsv");EiemOfflineCache::ClaimSet(cache,cache/"mods");auto setOwner=Read(cache/"owner.tsv");
 Require(singleOwner!=setOwner&&ReadCurrent(cache)==pointer,"single-author cache upgrade lost its publication");
 EiemOfflineCache::ClaimSet(cache,cache/"mods");rejected=false;try{EiemOfflineCache::ClaimSet(cache,cache/"foreign-mods");}catch(const std::exception&){rejected=true;}
 Require(rejected&&Read(cache/"owner.tsv")==setOwner&&ReadCurrent(cache)==pointer,"foreign Mod set took over the upgraded cache");
 std::filesystem::remove_all(cache);std::cout<<"PASS offline compiled package: 65 Mods, 600 inputs/resources, 140 roots, 65 MiB fields; structure compatibility across build labels; malformed extents/AST/references/SHA rejection and atomic publication\n";
 }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}

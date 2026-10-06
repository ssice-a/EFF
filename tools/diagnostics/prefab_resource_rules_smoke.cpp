#define NOMINMAX
#include "../../src/eiem_prefab_resource_rules.h"
#include <iostream>
using namespace EiemOfflinePackage;
int wmain(int argc,wchar_t** argv){try{
 Require(argc==4,"prefab_resource_rules_smoke cache mods baseline");auto cache=std::filesystem::path(argv[1]);auto mods=std::filesystem::path(argv[2]);auto pointer=ReadCurrent(cache);auto m=ReadManifest(cache,pointer,mods);auto folder=cache/"generations"/pointer.key;
 Require(m.plan.rulesOnly,"RED candidate still publishes complete Prefab inputs");auto bytes=Read(folder/"manifest.bin");
 Require(!std::filesystem::exists(folder/"startup"),"complete startup Prefabs were persisted");for(const auto& file:std::filesystem::recursive_directory_iterator(folder))Require(file.path().extension()!=".vfs","complete resource/Prefab was persisted");
 auto plan=std::make_shared<const Plan>(m.plan);EiemPrefabRules::Context context(plan,m.package);size_t rules=0,roots=0;uint64_t nativeBytes=0;std::set<size_t> prefabInputs;for(const auto& root:plan->roots)prefabInputs.insert(root.input);
 for(size_t i=0;i<plan->inputs.size();++i){rules+=plan->rendererRules[i].size();auto output=context.Materialize(i,false);nativeBytes+=output.size();auto current=EndfieldInput::Decode(output);auto original=EndfieldInput::Decode(ReadSource({},plan->originals[i]));
  for(size_t j=0;j<original.members.size();++j){const auto& before=original.members[j];const auto& after=current.members[j];if(!Serialized(before.bytes))continue;auto old=ParseSerialized(before.bytes),now=ParseSerialized(after.bytes);
   for(const auto& object:old.objects){const auto& t=old.types.at(object.second.type);if(t.classId!=21&&(t.classId!=137||std::any_of(plan->rendererRules[i].begin(),plan->rendererRules[i].end(),[&](const auto& rule){return rule.cab==before.name&&rule.id==object.first;})))continue;
    const auto& actual=now.objects.at(object.first);Require(actual.size==object.second.size&&std::equal(before.bytes.begin()+object.second.offset,before.bytes.begin()+object.second.offset+object.second.size,after.bytes.begin()+actual.offset),"undeclared Renderer/original Material was modified");
   }
  }
  auto isolated=context.Materialize(i,true);Require(Hash(isolated)==plan->inputs[i].hash,"runtime isolated rules differ");if(prefabInputs.count(i))++roots;
 }
 auto damaged=bytes;damaged.back()^=0x80;bool rejected=false;try{auto check=Decode(damaged,folder,mods);rejected=Hash(damaged)!=pointer.hash;}catch(const std::exception&){rejected=true;}Require(rejected,"damaged rule descriptor accepted");
 auto bad=m;bool damagedAnchor=false;if(bad.plan.spanRecipes){for(size_t i=0;i<bad.plan.memberRecipes.size()&&!damagedAnchor;++i)for(auto& member:bad.plan.memberRecipes[i]){auto span=std::find_if(member.spans.begin(),member.spans.end(),[](const auto& s){return s.kind==EiemResourceRecipe::Original;});if(span==member.spans.end())continue;span->offset=SIZE_MAX;auto wrong=std::make_shared<const Plan>(bad.plan);EiemPrefabRules::Context c(wrong,bad.package);bool rejects=false;try{c.Materialize(i,false);}catch(const std::exception&){rejects=true;}Require(rejects,"wrong original copy anchor accepted");damagedAnchor=true;break;}}
 else for(auto& input:bad.plan.rendererRules)if(!input.empty()){input.front().id=INT64_MAX;auto wrong=std::make_shared<const Plan>(bad.plan);EiemPrefabRules::Context c(wrong,bad.package);bool rejects=false;try{c.Materialize(size_t(&input-bad.plan.rendererRules.data()),false);}catch(const std::exception&){rejects=true;}Require(rejects,"wrong Renderer anchor accepted");damagedAnchor=true;break;}
 Require(damagedAnchor&&bytes.size()<nativeBytes,"rule fixture lost anchor corruption coverage or saved complete native inputs");
 std::cout<<"PASS prefab rules: bytes="<<bytes.size()<<" inputs="<<plan->inputs.size()<<" prefabInputs="<<roots<<" exactRendererRules="<<rules<<" runtimeMaterializedBytes="<<nativeBytes<<" noCompletePrefabFiles=1 authorPayloadsByReference=1 undeclaredRenderersPreserved=1 startupAndF10SameRules=1 wrongAnchorRejected=1 nativeAcceptance=0\n";
 }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}

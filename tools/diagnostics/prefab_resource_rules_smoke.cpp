#define NOMINMAX
#include "../../src/eiem_prefab_resource_rules.h"
#include <iostream>
using namespace EiemOfflinePackage;
size_t VerifyMaterialCopies(const Manifest& manifest,const Bytes& original,const Bytes& current,
 const std::string& cab,const SerializedLayout& before,const SerializedLayout& after,
 const std::map<Identity,std::vector<std::string>>& copies,
 std::vector<std::pair<TextureSlot,NativeReference>>& metadataChecks){
 size_t count=0;const auto& package=manifest.package;
 for(const auto& asset:SerializedAssets(original,cab,before)){
  if(asset.classId!=21)continue;auto declarations=copies.find(Identity{asset.path,asset.asset});
  if(declarations==copies.end()||declarations->second.size()<2)continue;
  const auto& type=before.types.at(before.objects.at(asset.pathId).type);
  auto source=ObjectValue(original,before,asset.pathId);std::set<int64_t> ids;
  for(const auto& section:declarations->second){
   const auto& material=package.materials.at(section);auto id=AllocateMaterialId(cab,package,section);
   Require(id!=asset.pathId&&ids.insert(id).second&&after.objects.count(id),"shared-template Material copies not independent");
   auto expected=source;
   for(const auto& block:package.resources.at(material.resource)->blocks){
    auto& field=Child(type,expected,block.first);const auto& resource=*package.resources.at(material.resource);
    Bytes data(resource.bytes.begin()+block.second.offset,resource.bytes.begin()+block.second.offset+block.second.size);
    field=DecodeValue(type,field.node,data);
   }
   auto& environments=ArrayValue(type,Child(type,Child(type,expected,"m_SavedProperties"),"m_TexEnvs"));
   for(const auto& edit:material.textures){bool found=false;
    for(auto& pair:environments.children){const auto& name=Child(type,pair,"first");if(Text(name.raw.data(),name.raw.size())!=edit.first)continue;
     Require(!found,"duplicate cloned Material property");found=true;
     auto& pointer=Child(type,Child(type,pair,"second"),"m_Texture");auto target=Reference(type,pointer,before,cab);
     SetReference(type,pointer,{target.first,AllocateTextureId(target.first,package,edit.second,package.textures.at(edit.second))},after,cab);
    }Require(found,"cloned Material texture property absent");
   }
   auto actual=ObjectValue(current,after,id);
   Require(EncodeValue(type,actual)==EncodeValue(type,expected),"shared-template Material edits leaked between copies");
   Require(manifest.plan.materials.at(section).name==Name(type,actual)&&manifest.plan.materials.at(section).slots.size()==environments.children.size(),"cloned Material metadata coverage differs");
   const auto& slots=manifest.plan.materials.at(section).slots;
   for(size_t i=0;i<environments.children.size();++i){const auto& pair=environments.children[i];const auto& name=Child(type,pair,"first");
    auto ref=NullableReference(type,Child(type,Child(type,pair,"second"),"m_Texture"),after,cab);
    Require(slots[i].property==Text(name.raw.data(),name.raw.size())&&slots[i].nonnull==(ref.second!=0),"cloned Material slot metadata differs");
    metadataChecks.push_back({slots[i],ref});
   }
   ++count;
  }
 }return count;
}
int wmain(int argc,wchar_t** argv){try{
 Require(argc==4,"prefab_resource_rules_smoke cache mods baseline");auto cache=std::filesystem::path(argv[1]);auto mods=std::filesystem::path(argv[2]);auto pointer=ReadCurrent(cache);auto m=ReadManifest(cache,pointer,mods);auto folder=cache/"generations"/pointer.key;
 Require(m.plan.rulesOnly,"RED candidate still publishes complete Prefab inputs");auto bytes=Read(folder/"manifest.bin");
 Require(!std::filesystem::exists(folder/"startup"),"complete startup Prefabs were persisted");for(const auto& file:std::filesystem::recursive_directory_iterator(folder))Require(file.path().extension()!=".vfs","complete resource/Prefab was persisted");
 std::map<Identity,std::vector<std::string>> materialCopies;for(const auto& material:m.package.materials)materialCopies[material.second.source].push_back(material.first);
 size_t expectedCopies=0,verifiedCopies=0;for(const auto& copies:materialCopies)if(copies.second.size()>1)expectedCopies+=copies.second.size();
 std::vector<std::pair<TextureSlot,NativeReference>> metadataChecks;std::map<NativeReference,EiemResourceReloadPlan::Texture> nativeTextures;
 auto plan=std::make_shared<const Plan>(m.plan);EiemPrefabRules::Context context(plan,m.package);size_t rules=0,roots=0;uint64_t nativeBytes=0;std::set<size_t> prefabInputs;for(const auto& root:plan->roots)prefabInputs.insert(root.input);
 for(size_t i=0;i<plan->inputs.size();++i){rules+=plan->rendererRules[i].size();auto output=context.Materialize(i,false);nativeBytes+=output.size();auto current=EndfieldInput::Decode(output);auto original=EndfieldInput::Decode(ReadSource({},plan->originals[i]));
  for(size_t j=0;j<original.members.size();++j){const auto& before=original.members[j];const auto& after=current.members[j];if(!Serialized(before.bytes))continue;auto old=ParseSerialized(before.bytes),now=ParseSerialized(after.bytes);
   for(const auto& object:old.objects){const auto& t=old.types.at(object.second.type);if(t.classId!=21&&(t.classId!=137||std::any_of(plan->rendererRules[i].begin(),plan->rendererRules[i].end(),[&](const auto& rule){return rule.cab==before.name&&rule.id==object.first;})))continue;
    const auto& actual=now.objects.at(object.first);Require(actual.size==object.second.size&&std::equal(before.bytes.begin()+object.second.offset,before.bytes.begin()+object.second.offset+object.second.size,after.bytes.begin()+actual.offset),"undeclared Renderer/original Material was modified");
   }
   verifiedCopies+=VerifyMaterialCopies(m,before.bytes,after.bytes,before.name,old,now,materialCopies,metadataChecks);
   if(expectedCopies)for(const auto& row:now.objects)if(now.types.at(row.second.type).classId==28){
    const auto& type=now.types.at(row.second.type);auto value=ObjectValue(after.bytes,now,row.first);
    nativeTextures.emplace(NativeReference{after.name,row.first},EiemResourceReloadPlan::Texture{Name(type,value),int(Integer(Child(type,value,"m_Width"))),int(Integer(Child(type,value,"m_Height"))),int(Integer(Child(type,value,"m_MipCount")))});
   }
  }
  auto isolated=context.Materialize(i,true);Require(Hash(isolated)==plan->inputs[i].hash,"runtime isolated rules differ");if(prefabInputs.count(i))++roots;
 }
 auto damaged=bytes;damaged.back()^=0x80;bool rejected=false;try{auto check=Decode(damaged,folder,mods);rejected=Hash(damaged)!=pointer.hash;}catch(const std::exception&){rejected=true;}Require(rejected,"damaged rule descriptor accepted");
 auto bad=m;bool damagedAnchor=false;if(bad.plan.spanRecipes){for(size_t i=0;i<bad.plan.memberRecipes.size()&&!damagedAnchor;++i)for(auto& member:bad.plan.memberRecipes[i]){auto span=std::find_if(member.spans.begin(),member.spans.end(),[](const auto& s){return s.kind==EiemResourceRecipe::Original;});if(span==member.spans.end())continue;span->offset=SIZE_MAX;auto wrong=std::make_shared<const Plan>(bad.plan);EiemPrefabRules::Context c(wrong,bad.package);bool rejects=false;try{c.Materialize(i,false);}catch(const std::exception&){rejects=true;}Require(rejects,"wrong original copy anchor accepted");damagedAnchor=true;break;}}
 else for(auto& input:bad.plan.rendererRules)if(!input.empty()){input.front().id=INT64_MAX;auto wrong=std::make_shared<const Plan>(bad.plan);EiemPrefabRules::Context c(wrong,bad.package);bool rejects=false;try{c.Materialize(size_t(&input-bad.plan.rendererRules.data()),false);}catch(const std::exception&){rejects=true;}Require(rejects,"wrong Renderer anchor accepted");damagedAnchor=true;break;}
 Require(damagedAnchor&&bytes.size()<nativeBytes,"rule fixture lost anchor corruption coverage or saved complete native inputs");
 Require(verifiedCopies==expectedCopies,"shared-template Material clone coverage differs");
 for(const auto& check:metadataChecks){const auto& slot=check.first;auto actual=nativeTextures.find(check.second);
  Require(slot.local==(actual!=nativeTextures.end()),"cloned Material texture locality metadata differs");
  if(slot.local)Require(slot.texture.name==actual->second.name&&slot.texture.width==actual->second.width&&slot.texture.height==actual->second.height&&slot.texture.mips==actual->second.mips,"cloned Material texture metadata leaked between copies");
 }
 std::cout<<"PASS prefab rules: bytes="<<bytes.size()<<" inputs="<<plan->inputs.size()<<" prefabInputs="<<roots<<" exactRendererRules="<<rules<<" runtimeMaterializedBytes="<<nativeBytes<<" independentMaterialCopies="<<verifiedCopies<<" noCompletePrefabFiles=1 authorPayloadsByReference=1 undeclaredRenderersPreserved=1 startupAndF10SameRules=1 wrongAnchorRejected=1 nativeAcceptance=0\n";
 }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}

#pragma once
namespace WeaponRetargetTest {
using namespace Entry;
void Checks(){
 namespace D=DemandLoadingProbe;namespace N=EiemDemandNativeAssembly;
 const auto previousGame=gameDirectory;const auto previous=std::atomic_load(&D::installedGeneration);
 auto root=std::filesystem::temp_directory_path()/("eff-retarget-runtime-"+std::to_string(GetCurrentProcessId()));
 Require(!std::filesystem::exists(root),"retarget fixture already exists");
 struct Cleanup {std::filesystem::path root,game;std::shared_ptr<const D::InstalledGeneration> generation;
  ~Cleanup(){gameDirectory=game;std::atomic_store(&D::installedGeneration,generation);std::error_code error;std::filesystem::remove_all(root,error);}} cleanup{root,previousGame,previous};
 gameDirectory=root;std::atomic_store(&D::installedGeneration,std::shared_ptr<const D::InstalledGeneration>{});
 auto save=[](const std::filesystem::path& path,const std::string& text){std::filesystem::create_directories(path.parent_path());std::ofstream output(path,std::ios::binary);output<<text;Require(bool(output),"retarget fixture write failed");};
 auto install=[&](const std::string& folder,const std::string& target){auto mod=root/"plugin/mods"/folder;
  save(mod/"meshes/weapon.effres","deferred Mesh payload");save(mod/"materials/weapon.effres","deferred Material payload");
  save(mod/"mod.ini","[Mod]\nformat=3\nid=SharedWeapon\ncontrols=ini\n\n[Constants]\n$toggle=1\n\n[MeshWeapon]\npath=meshes/weapon.effres\n\n[MaterialWeapon]\npath=materials/weapon.effres\nsource.path=assets/author/material.mat\nsource.asset=AuthorMaterial\n\n[PartWeapon]\nmesh=MeshWeapon\nmaterial=MaterialWeapon\nvisible=$toggle\n\n[ReplaceWeapon]\ntarget.path=assets/"+target+".fbx\ntarget.asset="+target+"\ntarget.bundle=bundles/windows/main/old.ab\nparts=PartWeapon\n");return mod;};
 auto a=install("a","SwordA");auto first=D::BuildInstalledGeneration();Require(first->authors.size()==1,"single installed author rejected");auto aId=first->authors.begin()->first;
 D::PublishInstalledGeneration(first);auto b=install("b","SwordB");auto both=D::BuildInstalledGeneration();
 Require(both->authors.size()==2&&both->targets.size()==2&&both->authors.count(aId),"same-ID copies were skipped or changed existing instance identity");
 std::vector<std::shared_ptr<N::Author>> authors;std::set<std::string> ids;
 for(const auto& row:both->authors){ids.insert(row.first);authors.push_back(row.second);Require(row.second->package.id==row.first,"author runtime identity differs");}
 Require(both->targets[0].mod!=both->targets[1].mod,"target routes share author identity");
 for(auto& author:authors){ShapeAnimation animation;animation.id="loop";animation.speedVariable="$toggle";animation.duration=1;animation.tracks["$toggle"]={0,1};author->package.shapeAnimations["loop"]=animation;
  EffectAttachment effect;effect.id="trail";effect.part="PartWeapon";effect.variable="$toggle";effect.targetPrefab="assets/author/effect.prefab";effect.targetPath="assets/old.fbx";effect.targetAsset="Old";effect.native=EffectTest::Manifest('W');author->package.effects["trail"]=effect;}
 auto composed=N::Author::Compose(authors);
 Require(composed->meshSources.size()==2&&composed->materialSources.size()==2,"copy assembly lost native resource sources");
 for(const auto& author:authors){auto prefix=EiemComposedPackage::Prefix(author->package.id);Require(composed->package.defaults.count("$"+prefix+"toggle")&&composed->meshSources.at(prefix+"PartWeapon").first->Root()==author->catalog->Root()&&composed->materialSources.count(prefix+"MaterialWeapon"),"copy source/variable prefix does not match native package");
 }
 auto graph=std::make_shared<std::vector<Package>>();for(const auto& author:authors)graph->push_back(author->package);
 auto controls=RuntimeControls::Prepare(graph,EiemGlobalConfig{});
 Require(controls.variables.size()==2&&controls.animations.size()==2,"installed runtime control graph lost per-copy state");
 controls.variables.at(authors[0]->package.id)["$toggle"]=0;
 Require(controls.variables.at(authors[1]->package.id).at("$toggle")==1,"variables leaked between installed copies");
 for(const auto& author:authors){const auto& package=author->package;const auto& target=package.rules.at("ReplaceWeapon").targets.begin()->second;
  auto marker=RuntimePartName(package,"PartWeapon",target);Require(controls.specs.at(marker).package==package.id&&package.effects.at("trail").part=="PartWeapon","retargeted Part marker does not identify its runtime effect owner");}
 D::PublishInstalledGeneration(both);install("a","SwordC");auto retarget=D::BuildInstalledGeneration();
 Require(retarget->authors.size()==2&&retarget->delta.modifiedPackages==std::set<std::string>{aId}&&retarget->delta.targets.count({"assets/sworda.fbx","SwordA"})&&retarget->delta.targets.count({"assets/swordc.fbx","SwordC"}),"INI target edit did not isolate F10 old/new owners");
 // Persistent state and Ins key settings are stored per directory and must
 // validate the author's INI identity, not the generated runtime instance ID.
 EiemEntryPreferences saved;EiemModKeySettings settings;
 auto pa=retarget->authors.at(aId)->package;auto pb=retarget->authors.at(EiemComposedPackage::InstalledInstanceId("SharedWeapon",b))->package;
 pa.persistent.insert("$toggle");pb.persistent.insert("$toggle");
 saved.Capture(pa,{{"$toggle",0}});saved.Capture(pb,{{"$toggle",1}});saved.Flush(Log,true);
 Require(EiemEntryPreferences::Read(a/"state.ini","SharedWeapon",true).at("$toggle")==0&&EiemEntryPreferences::Read(b/"state.ini","SharedWeapon",true).at("$toggle")==1,"installed copies lost or shared persistent state");
 settings.Capture(pa,false);settings.Capture(pb,true);settings.Flush(Log,true);
 auto keyFlag=[](const std::filesystem::path& root){for(const auto& section:EiemDirectory::ParseIni(EiemDirectory::Read(root/"mod.ini")))if(section.id=="Mod")return section.values.at("key_switch_enabled");return std::string{};};
 Require(keyFlag(a)=="0"&&keyFlag(b)=="1","installed copy key settings could not save against its declared INI ID");
 D::PublishInstalledGeneration(retarget);std::filesystem::remove_all(b);auto removed=D::BuildInstalledGeneration();
 Require(removed->authors.size()==1&&removed->authors.count(aId)&&removed->delta.removedPackages.size()==1,"removing a copy changed its surviving instance");
 std::cout<<"PASS production installed-generation retarget: same-ID copies, stable add/remove identity, native source prefixes, independent variables/animation/effect Parts, persistent state/key settings, F10 old/new target delta; payloads stay deferred\n";
}
}

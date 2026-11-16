#define NOMINMAX
#include "../../src/eiem_demand_author_resources.h"
#include "../../src/eiem_effect_relocation.h"
#include <iostream>

using namespace EiemDirectory;
int main(int argc,char** argv){try{
 Require(argc==2||(argc==3&&std::string(argv[1])=="--relocations"),"expected a real effect Mod directory");
 auto root=std::filesystem::path(argv[argc-1]);
 EiemDemandAuthorResources::Catalog catalog(root);
 const auto& effects=catalog.Directory().runtime.effects;
 if(argc==3){
  Require(effects.size()==1,"expected one logical effect attachment");auto native=effects.begin()->second.native;
  auto selected=std::find_if(native->gameResources.begin(),native->gameResources.end(),[](const auto& r){return !r.patches.empty();});
  Require(selected!=native->gameResources.end()&&selected->asset.empty(),"bare logical path lost its relocation or optional asset name");
  for(size_t i=0;i<native->bundles.size();++i){auto source=EiemDemandAuthorResources::OpenEffectInput(root,native->bundles[i]);
   auto before=EiemInputSpanView::Digest(*source);
   auto bound=EiemEffectRelocation::Bind(source,i,native->gameResources,[](const auto&)->EiemEffectRelocation::Reference{return {"CAB-99999999999999999999999999999999",-42};});
   Require(EiemInputSpanView::Digest(*source)==before,"runtime reference binding mutated the Mod input");
   auto patched=std::any_of(selected->patches.begin(),selected->patches.end(),[i](const auto& p){return p.bundle==i;});
   Require((EiemInputSpanView::Digest(*bound)!=before)==patched,"runtime logical reference patches were lost in production Catalog");
  }
  auto sections=ParseIni(Read(root/"mod.ini"));
  auto parse=[&](const auto& input){return EiemAuthorControlsIni::Read(input,
   [](const std::string&)->EiemAuthorControlsIni::Samples{throw std::runtime_error("unexpected Animation samples");},
   [&](const std::string& id){auto resource=std::find_if(input.begin(),input.end(),[&](const auto& s){return s.id==id;});Require(resource!=input.end(),"effect resource absent");
    auto file=std::make_shared<EiemNativeRandomAccess::File>(root/std::filesystem::u8path(resource->values.at("path")),0);
    return EiemAuthorResourceMetadata::DecodeEffect(EiemDemandAuthorResources::AuthorMetadata(file,EiemDemandAuthorResources::AuthorLayout(file,5)));});};
  auto valid=parse(sections).effects.begin()->second.native->key;
  auto section=std::find_if(sections.begin(),sections.end(),[](const auto& s){return Begins(s.id,"EffectAttachment");});
  auto field=std::find_if(section->values.begin(),section->values.end(),[](const auto& v){return v.first.find(".patch.0.offset")!=std::string::npos;});
  Require(field!=section->values.end(),"relocation site missing from INI");auto offset=field->second;field->second="18446744073709551615";
  bool rejected=false;try{parse(sections);}catch(const std::exception&){rejected=true;}Require(rejected,"out of range INI relocation accepted");field->second=offset;
  field->second=std::to_string(std::stoull(offset)+1);Require(parse(sections).effects.begin()->second.native->key!=valid,"INI relocation edit failed to invalidate native generation");
  std::cout<<"PASS real logical references: production INI/Catalog, optional asset, immutable runtime overlay, malformed range rejection, generation invalidation\n";
  return 0;
 }
 Require(effects.size()==1,"expected the captured two-emitter attachment");
 auto original=effects.begin()->second.native;
 const auto& attachment=effects.begin()->second;
 if(!attachment.lodTargets.empty()){
  Require(attachment.lodTargets.size()==3&&attachment.lodAnchor&&attachment.lodAnchor->path=="Mesh_all"&&attachment.lodAnchor->nodes.size()==2,"production Catalog lost stable LOD mount assembly");
  for(size_t i=0;i<attachment.lodTargets.size();++i)Require(attachment.lodTargets[i].asset=="S_wpn_sword_0022_01_lod"+std::to_string(i+1),"production Catalog changed LOD target identity");
  Require(std::abs(attachment.lodAnchor->nodes[1].rotation[0]+.7071068f)<.00001f,"production Catalog lost source Renderer rotation");
 }
 Require(original&&original->particles==2&&original->gameResources.size()==1,"real effect game dependencies were lost");
 const auto& resource=original->gameResources.front();
 Require(resource.type=="Material"&&resource.sourcePrefab=="assets/beyond/dynamicassets/gameplay/effects/prefabs/p_fxbat_liino_attack_05_star_hit_small.prefab"&&
  resource.sourceRoot=="P_fxbat_liino_attack_05_star_hit_small","production Catalog lost logical source Prefab identity");
 auto sections=ParseIni(Read(root/"mod.ini"));
 auto parse=[](const auto& input){return EiemAuthorControlsIni::Read(input,
  [](const std::string&)->EiemAuthorControlsIni::Samples{throw std::runtime_error("fixture should not read animation samples");},
  [](const std::string&){return EiemAuthorResourceMetadata::Effect{"ResourceEffect","effects/test.effres",std::string(64,'A'),64};});};
 auto key=parse(sections).effects.begin()->second.native->key;
 auto effect=std::find_if(sections.begin(),sections.end(),[](const auto& s){return Begins(s.id,"EffectAttachment");});
 Require(effect!=sections.end(),"effect declaration missing");
 effect->values.at("game.0.source_root")="DifferentRoot";
 Require(parse(sections).effects.begin()->second.native->key!=key,"source identity edit failed to invalidate native generation");
 effect->values.erase("game.0.source_root");
 Require(parse(sections).effects.begin()->second.native->gameResources.front().sourceRoot.empty(),"logical source Prefab wrongly requires a local root name");
 effect->values["game.0.source_root"]="Root";effect->values.erase("game.0.source_prefab");bool rejected=false;
 try{parse(sections);}catch(const std::exception&){rejected=true;}
 Require(rejected,"partial source Prefab declaration was accepted");
 effect->values.erase("game.0.source_root");
 Require(parse(sections).effects.begin()->second.native->gameResources.front().sourcePrefab.empty(),"legacy direct resource declaration lost compatibility");
 if(attachment.lodAnchor){auto valid=sections;effect->values["lod_anchor"]=".";Require(parse(sections).effects.begin()->second.lodAnchor->path.empty(),"root LOD ancestor marker was not decoded");effect->values["lod_anchor.1.rotation"]="0,0,0,0";rejected=false;try{parse(sections);}catch(const std::exception&){rejected=true;}Require(rejected,"invalid LOD anchor quaternion accepted");sections=valid;
  auto& values=std::find_if(sections.begin(),sections.end(),[](const auto& s){return Begins(s.id,"EffectAttachment");})->values;values.erase("lod_anchor");rejected=false;try{parse(sections);}catch(const std::exception&){rejected=true;}Require(rejected,"static LOD targets accepted without stable anchor");}
 std::cout<<"PASS real Mod -> production Catalog: two emitters, logical game source, generation invalidation, incomplete-source rejection, legacy compatibility\n";
 return 0;
}catch(const std::exception& e){std::cerr<<"FAIL effect dependencies: "<<e.what()<<'\n';return 1;}}

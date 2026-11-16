#define NOMINMAX
#include "../../src/eiem_author_controls_ini.h"
#include "../../src/eiem_shape_animation_player.h"
#include "../../src/eiem_demand_native_assembly.h"
#include <iostream>
using namespace EiemDirectory;
int main(int argc,char** argv){try{
 if(argc==2){auto catalog=std::make_shared<EiemDemandAuthorResources::Catalog>(std::filesystem::path(argv[1]));EiemDemandNativeAssembly::Author author(catalog);
  Require(author.package.effects.size()==1&&author.package.shapeAnimations.size()==1,"real effect package lost mount/animation declarations");
  auto& output=author.package.shapeAnimations.begin()->second.nativeOutputs.at(0);auto view=output.open();Bytes header(48);Require(view&&view->ReadAt(0,header.data(),header.size())==header.size()&&EiemEffectNative::Archive(header.data(),view->Size()),"real authored native Clip input differs");
  Require(author.package.effects.begin()->second.hierarchy&&author.package.effects.begin()->second.native->bundles.at(0).open,"real attachment hierarchy/native input unresolved");
  std::cout<<"PASS real Blender export -> production Catalog/assembly -> lazy immutable native Animation input\n";
 }
 const std::string text=R"ini([Mod]
format=3
id=Mounts
controls=ini
[Constants]
persist $speed=1
$fx=1
$shape=0
[ControlFx]
variable=$fx
label=Effect
min=0
max=1
[ControlShape]
variable=$shape
label=Shape
min=0
max=1
[HierarchyOrbit]
node.0.path=Pivot
node.0.label=Orbit
node.1.path=Pivot/Tip
node.1.position=0,1,0
[ResourceClip]
type=animation-clip
path=animations/spin.ab
[ResourceEffect]
type=effect
path=effects/ring.effres
[AnimationSpin]
duration=1
speed_variable=$speed
speed=1
loop=1
autoplay=1
key=NUMPAD5
native.0.hierarchy=Orbit
native.0.resource=ResourceClip
native.0.asset=assets/eff/animations/spin.anim
[EffectAttachmentRing]
mode=attach_mesh
variable=$fx
target.prefab=assets/game/weapon.prefab
target.path=assets/game/weapon.mesh
target.asset=Weapon
prefab=assets/eff/ring.prefab
root_name=Ring
bundles=ResourceEffect
root=0
particles=0
hierarchy=Orbit
node=Pivot/Tip
)ini";
 auto sections=ParseIni(Bytes(text.begin(),text.end()));
 auto read=[](const std::vector<Section>& sections){return EiemAuthorControlsIni::Read(sections,
   [](const std::string&){EiemAuthorControlsIni::Samples s;s.scalar={{0,1,0}};return s;},
   [](const std::string&){return EiemAuthorResourceMetadata::Effect{"ResourceEffect","effects/ring.effres",std::string(64,'A'),64};});};
 auto state=read(sections);Require(state.effects.at("Ring").hierarchy->nodes.size()==2,"hierarchy missing");
 auto signedScale=sections;for(auto& s:signedScale)if(s.id=="HierarchyOrbit")s.values["node.1.scale"]="-1,0,2";
 auto scaled=read(signedScale);Require(scaled.effects.at("Ring").hierarchy->nodes.at(1).scale==std::array<float,3>{-1,0,2},"signed/zero Transform scale rejected");
 Package p;p.id="Mounts";p.defaults=state.defaults;p.persistent=state.persistent;p.controls=state.controls;p.shapeAnimations=state.animations;
 EiemShapeAnimation::Player player;player.Prepare(p);const auto& clip=p.shapeAnimations.at("Spin");player.Advance(p,p.defaults,.25);Require(player.clips.at("Spin").time==.25,"native-only shared clock stopped");
 player.Pause(clip);player.Advance(p,p.defaults,.5);Require(player.clips.at("Spin").time==.25,"pause changed mount clock");player.Play(clip);player.Advance(p,p.defaults,1);Require(player.clips.at("Spin").time==.25,"native looping phase differs");
 auto other=clip;other.id="Other";other.speedVariable="$other";other.autoplay=false;p.defaults["$other"]=1;p.persistent.insert("$other");p.shapeAnimations["Other"]=other;player.Prepare(p);player.Play(other);Require(!player.clips.at("Spin").playing,"same hierarchy retained conflicting clip");
 auto rejected=[&](std::vector<Section> bad){bool fail=false;try{read(bad);}catch(const std::exception&){fail=true;}Require(fail,"invalid mount declaration accepted");};
 auto change=[&](const char* section,const char* key,const char* value){auto bad=sections;for(auto& s:bad)if(s.id==section)s.values[key]=value;return bad;};
 rejected(change("HierarchyOrbit","node.1.path","Missing/Tip"));rejected(change("HierarchyOrbit","node.1.rotation","0,0,0,0"));rejected(change("EffectAttachmentRing","node","Missing"));rejected(change("AnimationSpin","native.0.resource","Absent"));rejected(change("ResourceClip","path","../escape.ab"));
 auto mixed=sections;for(auto& s:mixed)if(s.id=="AnimationSpin"){s.values["samples"]="Samples";s.values["track.0"]="$shape";}
 auto mixedState=read(mixed);p.shapeAnimations=mixedState.animations;player.Prepare(p);player.Advance(p,p.defaults,.25);Require(player.output.at("$shape")==.5&&player.clips.at("Spin").time==.25,"mixed outputs use different clocks");
 bool binaryRejected=false;try{EiemRuntimeAuthorDirectory::Writer w;EiemAuthorControlMetadata::Write(w,state);}catch(...){binaryRejected=true;}Require(binaryRejected,"binary controls silently lost native mount data");
 std::cout<<"PASS author mounts: INI hierarchy/provenance validation, native-only/mixed shared clocks, loop/pause/conflicts and no binary data loss\n";
 }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}

#pragma once
#include "eiem_entry_preferences.h"
#include "eiem_shape_snapshot.h"
#include "eiem_control_panel.h"
#include "eiem_mod_key_settings.h"
// Numeric author state has no dependency on Unity Meshes or capture lifetime.
namespace RuntimeControls {
struct Spec {
  std::string package, part, marker, meshResource;
  unsigned vertices = 0;
  std::vector<std::string> channels;
  std::map<std::string,std::string> controls;
  std::shared_ptr<EiemExpression> visible;
  std::string material;
};

std::map<std::string,Spec> specs;
std::map<std::string,EiemVariables> variables;
std::map<std::string,std::vector<Binding>> keys;
std::map<std::string,EiemShapeAnimation::Player> animations;
std::shared_ptr<const std::vector<Package>> controlGraph;
EiemEntryPreferences preferences;
EiemModKeySettings keySettings;
std::map<std::string,bool> held;
EiemControlPanel::Store panel;
struct AnimationKey {size_t package=0;std::string clip;};
std::vector<AnimationKey> animationKeys;
std::vector<size_t> animationPackages;
std::string selected, uiHold;
std::map<std::string,bool> keySwitchEnabled;
uint64_t revision=0, scopeRevision=0;
uint64_t valueRevision=0;
std::map<std::string,EiemVariables> effectiveValues;
std::map<std::string,std::set<std::string>> continuousChannels;
std::map<std::string,std::string> programErrors;
std::map<std::string,std::string> effectiveErrors;
uint64_t effectiveRevision=UINT64_MAX;
const EiemVariables& EffectiveValues(const std::string& package){
  if(effectiveRevision!=valueRevision){
    if(controlGraph)for(const auto& p:*controlGraph){try{
      auto values=variables.at(p.id);auto player=animations.find(p.id);if(player!=animations.end())for(const auto& v:player->second.output)values[v.first]=v.second;
      if(p.program)values=EiemControlProgram::Evaluate(*p.program,std::move(values));effectiveValues[p.id]=std::move(values);effectiveErrors.erase(p.id);
    }catch(const std::exception& e){if(!effectiveValues.count(p.id))effectiveValues[p.id]=variables.at(p.id);if(effectiveErrors[p.id]!=e.what())Log("[CONTROL-PROGRAM-GAP] mod=%s reason=%s",p.id.c_str(),e.what());effectiveErrors[p.id]=e.what();}}
    effectiveRevision=valueRevision;
  }
  auto found=effectiveValues.find(package);return found==effectiveValues.end()?variables.at(package):found->second;
}
struct State {
  std::map<std::string,Spec> specs;
  std::map<std::string,EiemVariables> variables;
  std::map<std::string,std::vector<Binding>> keys;
  std::map<std::string,EiemShapeAnimation::Player> animations;
  std::map<std::string,bool> held;
  std::shared_ptr<const std::vector<Package>> graph;
  std::string selected;
  std::map<std::string,bool> keySwitchEnabled;
  std::shared_ptr<EiemControlPanel::Snapshot> panelSnapshot;
  std::vector<AnimationKey> animationKeys;
  std::vector<size_t> animationPackages;
  std::map<std::string,std::set<std::string>> continuousChannels;
  std::map<std::string,EiemVariables> effective;
};
bool Selectable(const Package& p) { return bool(p.program)||!p.bindings.empty() || !p.controls.empty() || !p.shapeAnimations.empty(); }
bool HasKeys(const Package& p){return (p.program&&!p.program->events.empty())||!p.bindings.empty()||std::any_of(p.shapeAnimations.begin(),p.shapeAnimations.end(),[](const auto& row){return row.second.key.vk!=0;});}
void AnimationSnapshot(EiemControlPanel::Snapshot& view,const std::map<std::string,EiemShapeAnimation::Player>& source){for(const auto& p:source)view.animations[p.first]=p.second.clips;}
void PreparePanel(State& state) {
  // Runs before property writes, preserving no-throw graph publication.
  state.panelSnapshot=std::make_shared<EiemControlPanel::Snapshot>();
  state.panelSnapshot->graph=state.graph;
  state.animationKeys.clear();state.animationPackages.clear();
  state.continuousChannels.clear();
  for(size_t i=0;i<state.graph->size();++i){const auto& p=state.graph->at(i);if(!p.shapeAnimations.empty())state.animationPackages.push_back(i);
    if(p.program)for(const auto& row:p.program->parameters)state.continuousChannels[p.id].insert(row.first);
    for(const auto& row:p.shapeAnimations)if(row.second.key.vk)state.animationKeys.push_back({i,row.first});}
  if (state.selected.empty()) {
    for (const auto& p:*state.graph) if (!p.bindings.empty()) {state.selected=p.id;break;}
    if (state.selected.empty()) for (const auto& p:*state.graph) if (Selectable(p)) {state.selected=p.id;break;}
  }
  state.panelSnapshot->selected=state.selected;
  state.panelSnapshot->keySwitchEnabled=state.keySwitchEnabled;
  state.panelSnapshot->values=state.variables;
  for(const auto& p:*state.graph){auto values=state.variables.at(p.id);for(const auto& v:state.animations.at(p.id).output)values[v.first]=v.second;if(p.program)values=EiemControlProgram::Evaluate(*p.program,std::move(values));state.panelSnapshot->effective[p.id]=std::move(values);}
  state.effective=state.panelSnapshot->effective;
  AnimationSnapshot(*state.panelSnapshot,state.animations);
}
void PublishPanel() {
  auto view=std::make_shared<EiemControlPanel::Snapshot>();
  view->revision=revision;view->scope=scopeRevision;view->selected=selected;
  view->keySwitchEnabled=keySwitchEnabled;view->graph=controlGraph;view->values=variables;
  if(controlGraph)for(const auto& p:*controlGraph)view->effective[p.id]=EffectiveValues(p.id);view->errors=programErrors;for(const auto& error:effectiveErrors)view->errors[error.first]=error.second;
  AnimationSnapshot(*view,animations);
  panel.Publish(std::move(view));
}
bool Down(const EiemKeyChord& key) {
  return (GetAsyncKeyState(key.vk)&0x8000) &&
    (!(key.modifiers&MOD_SHIFT) || (GetAsyncKeyState(VK_SHIFT)&0x8000)) &&
    (!(key.modifiers&MOD_CONTROL) || (GetAsyncKeyState(VK_CONTROL)&0x8000)) &&
    (!(key.modifiers&MOD_ALT) || (GetAsyncKeyState(VK_MENU)&0x8000));
}
void PreviewPreferences(State& state) {
  auto live=std::atomic_load(&published);
  for(const auto& p:*state.graph) {
    bool changedOwner=false;
    if(live) for(const auto& old:*live) if(old.root==p.root&&old.id!=p.id) changedOwner=true;
    try {state.variables.at(p.id)=preferences.Preview(p,changedOwner);}
    catch(const std::exception& e) {Log("[CONTROL-STATE-GAP] load=%s reason=%s usingDefaults=1",p.id.c_str(),e.what());}
  }
}
void PrepareAnimations(State& state,const Package& package) {
  auto& player=state.animations[package.id];player.Prepare(package);
  for(const auto& clip:package.shapeAnimations)
    player.clips.at(clip.first).held=clip.second.key.vk&&Down(clip.second.key);
}
State Prepare(const std::shared_ptr<const std::vector<Package>>& graph,const EiemGlobalConfig& config) {
  Require(bool(graph),"runtime control graph unavailable");
  State state;state.graph=graph;
  for (const auto& package:*graph) {
    auto validateKey=[&](const EiemKeyChord& key){Require(!key.vk||(key.vk!=config.reload.vk&&key.vk!=config.gui.vk),"Mod shortcut overlaps a global shortcut: "+package.id);};
    for(const auto& key:package.bindings)validateKey(key.key);
    for(const auto& clip:package.shapeAnimations)validateKey(clip.second.key);
    if(package.program)for(const auto& event:package.program->events){validateKey(event.key);state.held[package.id+"/"+event.id]=Down(event.key);}
    state.variables[package.id]=package.defaults;state.keys[package.id]=package.bindings;
    state.keySwitchEnabled[package.id]=keySettings.Preview(package);
    PrepareAnimations(state,package);
    for(const auto& rule:package.rules) if(!rule.second.skip)
      for(const auto& target:rule.second.targets) for(const auto& partId:rule.second.parts) {
        const auto& part=package.parts.at(partId);const auto& mesh=package.meshes.at(part.mesh);
        for(const auto& marker:{RuntimePartName(package,partId,target.second),LegacyRuntimePartName(package,partId,target.second)}) {
          Spec spec{package.id,partId,marker,part.mesh,mesh.vertices,mesh.shapes,part.shapes,part.visible};
          spec.material=part.material;Require(state.specs.emplace(marker,std::move(spec)).second,"runtime part provenance collision");
        }
      }
  }
  PreviewPreferences(state);PreparePanel(state);return state;
}
void AppendPackages(State& state,const std::shared_ptr<const std::vector<Package>>& extra,
                    const EiemGlobalConfig& config) {
  if(!extra||extra->empty())return;
  Require(bool(state.graph),"runtime control base graph unavailable");
  auto combined=std::make_shared<std::vector<Package>>(*state.graph);
  const size_t offset=combined->size();
  auto add=Prepare(extra,config);
  for(const auto& package:*extra){
    Require(std::none_of(combined->begin(),combined->end(),[&](const Package& old){return old.id==package.id;}),
      "runtime standalone Mod id overlaps model Mod");
    combined->push_back(package);
  }
  state.specs.insert(std::make_move_iterator(add.specs.begin()),std::make_move_iterator(add.specs.end()));
  state.variables.insert(std::make_move_iterator(add.variables.begin()),std::make_move_iterator(add.variables.end()));
  state.keys.insert(std::make_move_iterator(add.keys.begin()),std::make_move_iterator(add.keys.end()));
  state.animations.insert(std::make_move_iterator(add.animations.begin()),std::make_move_iterator(add.animations.end()));
  state.held.insert(std::make_move_iterator(add.held.begin()),std::make_move_iterator(add.held.end()));
  state.keySwitchEnabled.insert(std::make_move_iterator(add.keySwitchEnabled.begin()),std::make_move_iterator(add.keySwitchEnabled.end()));
  for(auto key:add.animationKeys){key.package+=offset;state.animationKeys.push_back(std::move(key));}
  for(auto index:add.animationPackages)state.animationPackages.push_back(index+offset);
  if(state.selected.empty())state.selected=std::move(add.selected);
  state.graph=std::move(combined);PreparePanel(state);
}
void Swap(State& state) noexcept {
  specs.swap(state.specs);variables.swap(state.variables);keys.swap(state.keys);animations.swap(state.animations);
  held.swap(state.held);controlGraph.swap(state.graph);
  animationKeys.swap(state.animationKeys);animationPackages.swap(state.animationPackages);
  continuousChannels.swap(state.continuousChannels);
  selected.swap(state.selected);keySwitchEnabled.swap(state.keySwitchEnabled);uiHold.clear();
  ++revision;++scopeRevision;++valueRevision;
  effectiveValues.swap(state.effective);effectiveRevision=valueRevision;programErrors.clear();effectiveErrors.clear();
  if(state.panelSnapshot) {
    state.panelSnapshot->revision=revision;state.panelSnapshot->scope=scopeRevision;
    panel.Publish(std::move(state.panelSnapshot));
  }
}
void LoadControls(const std::shared_ptr<const std::vector<Package>>& graph) {
  auto next=Prepare(graph,settings.Get());
  Swap(next);
}
bool CompatibleAnimations(const Package& a,const Package& b){
  if(a.shapeAnimations.size()!=b.shapeAnimations.size())return false;
  for(const auto& row:a.shapeAnimations){auto found=b.shapeAnimations.find(row.first);if(found==b.shapeAnimations.end())return false;
    const auto& x=row.second;const auto& y=found->second;
    if(x.duration!=y.duration||x.loop!=y.loop||x.speedVariable!=y.speedVariable||x.tracks!=y.tracks||x.boneTracks.size()!=y.boneTracks.size())return false;
    for(size_t i=0;i<x.boneTracks.size();++i){const auto& u=x.boneTracks[i];const auto& v=y.boneTracks[i];if(u.skeleton!=v.skeleton||u.path!=v.path||u.samples!=v.samples)return false;}
  }return true;
}
void MergeLive(State& next,bool (*sample)(const EiemKeyChord&)=Down) {
  Require(bool(next.graph) && sample,"reload controls candidate missing");
  for(const auto& p:*next.graph) {
    next.keySwitchEnabled[p.id]=keySettings.Preview(p);
    const Package* previous=nullptr;
    if(controlGraph) for(const auto& old:*controlGraph) if(old.id==p.id && old.root==p.root) previous=&old;
    auto oldValues=variables.find(p.id);
    for(const auto& name:p.persistent) if(previous && previous->persistent.count(name) && oldValues!=variables.end() && oldValues->second.count(name)) {
      if(p.program&&p.program->parameters.count(name))continue;
      double value=oldValues->second.at(name);
      try {EiemEntryPreferences::Validate(p,{{name,value}});next.variables.at(p.id).at(name)=value;}
      catch(const std::exception&) {next.variables.at(p.id).at(name)=p.defaults.at(name);Log("[ENTRY-RELOAD-VALUE] package=%s variable=%s invalidOldDomain=1 defaultUsed=1",p.id.c_str(),name.c_str());}
    }
    if(p.program&&previous&&previous->program&&oldValues!=variables.end())for(const auto& row:p.program->parameters){const auto& d=row.second;auto old=previous->program->parameters.find(row.first);
      if(d.computed)continue;
      next.variables.at(p.id).at(row.first)=p.defaults.at(row.first);
      if(!d.retain||old==previous->program->parameters.end()||old->second.type!=d.type||!oldValues->second.count(row.first))continue;
      try{auto value=oldValues->second.at(row.first);d.Validate(value);next.variables.at(p.id).at(row.first)=value;}catch(const std::exception&){}
    }
    for(const auto& key:p.bindings) next.held[p.id+"/"+key.id]=sample(key.key);
    if(previous&&CompatibleAnimations(*previous,p)&&animations.count(p.id))next.animations.at(p.id)=animations.at(p.id);
    for(const auto& clip:p.shapeAnimations)next.animations.at(p.id).clips.at(clip.first).held=clip.second.key.vk&&sample(clip.second.key);
    if(p.program)for(const auto& event:p.program->events)next.held[p.id+"/"+event.id]=sample(event.key);
  }
  next.selected.clear();
  for(const auto& p:*next.graph) if(p.id==selected && Selectable(p)) {
    // A reused ID from another author directory is a different control owner.
    if(controlGraph) for(const auto& old:*controlGraph)
      if(old.id==p.id && old.root==p.root) next.selected=p.id;
  }
  PreparePanel(next);
}
double Advance(const Binding& key,double value,double elapsed) {
  if(key.type=="hold") return EiemShapeSnapshot::Advance(value,key.values.front(),key.speed,elapsed);
  if(key.type=="press") return key.values.front();
  auto it=std::find(key.values.begin(),key.values.end(),value);
  return it==key.values.end() || ++it==key.values.end()?key.values.front():*it;
}
void Capture(const std::set<std::string>& changed) {
  if(!changed.empty())++valueRevision;
  if(controlGraph) for(const auto& p:*controlGraph) if(changed.count(p.id)) preferences.Capture(p,variables.at(p.id));
}
bool RunAction(const Package& p,const std::string& id,double elapsed,std::set<std::string>& changed){
  if(!p.program)return false;
  try{
    const auto& effective=EffectiveValues(p.id);
    auto result=EiemControlProgram::Execute(*p.program,id,variables.at(p.id),elapsed,&effective);
    EiemEntryPreferences::Validate(p,result.values);
    auto& player=animations.at(p.id);for(const auto& name:result.written)player.Manual(p,name);
    bool different=result.values!=variables.at(p.id);variables.at(p.id)=std::move(result.values);
    for(const auto& command:result.commands){const auto& clip=p.shapeAnimations.at(command.second);if(command.first==EiemControlProgram::Instruction::Play)player.Play(clip);else if(command.first==EiemControlProgram::Instruction::Pause)player.Pause(clip);else player.Stop(clip);}
    if(different)changed.insert(p.id);++valueRevision;programErrors.erase(p.id);return true;
  }catch(const std::exception& e){if(programErrors[p.id]!=e.what())Log("[CONTROL-ACTION-GAP] mod=%s action=%s reason=%s",p.id.c_str(),id.c_str(),e.what());programErrors[p.id]=e.what();return true;}
}
void PanelCommands(bool focused,bool (*sample)(const EiemKeyChord&)=Down) {
  bool publish=false;std::set<std::string> changed;
  for(const auto& command:panel.Drain()) {
    using EiemControlPanel::Action;
    if(!focused || !panel.open || !controlGraph || command.revision!=revision ||
       (command.action!=Action::Keyboard && command.scope!=scopeRevision)) continue;
    const Package* owner=nullptr;
    for(const auto& p:*controlGraph) if(p.id==command.package && Selectable(p)) owner=&p;
    if(!owner) continue;
    if(command.action==Action::Keyboard){
      if(!HasKeys(*owner)||(command.value!=0&&command.value!=1))continue;
      bool enabled=command.value!=0;if(keySwitchEnabled.at(owner->id)!=enabled){
        keySwitchEnabled.at(owner->id)=enabled;keySettings.Capture(*owner,enabled);publish=true;
        for(const auto& key:owner->bindings)held[owner->id+"/"+key.id]=sample(key.key);
        for(const auto& clip:owner->shapeAnimations)animations.at(owner->id).clips.at(clip.first).held=clip.second.key.vk&&sample(clip.second.key);
         if(owner->program)for(const auto& event:owner->program->events)held[owner->id+"/"+event.id]=sample(event.key);
      }continue;
    }
    if(command.action==Action::Select) {
      selected=owner->id;
      ++scopeRevision;uiHold.clear();
      // Resample every binding so selecting a target with a held chord cannot
      // turn it into a fresh cycle/press, even between polling frames.
      for(const auto& p:keys) for(const auto& key:p.second) held[p.first+"/"+key.id]=sample(key.key);
      for(const auto& p:*controlGraph)if(p.program)for(const auto& event:p.program->events)held[p.id+"/"+event.id]=sample(event.key);
      publish=true;continue;
    }
    if(owner->id!=selected) continue;
    if(command.action==Action::InvokeAction){auto a=owner->program?owner->program->actions.find(command.name):std::map<std::string,EiemControlProgram::Action>::const_iterator{};
      if(owner->program&&a!=owner->program->actions.end()&&a->second.exposed)publish=RunAction(*owner,command.name,0,changed)||publish;continue;}
    auto& values=variables.at(owner->id);
    auto animation=owner->shapeAnimations.find(command.name);
    if(command.action==Action::AnimationPlay||command.action==Action::AnimationPause||command.action==Action::AnimationStop||command.action==Action::AnimationSpeed||command.action==Action::AnimationReset){
      if(animation==owner->shapeAnimations.end())continue;const auto& clip=animation->second;auto& player=animations.at(owner->id);auto& playback=player.clips.at(clip.id);
      if(command.action==Action::AnimationSpeed||command.action==Action::AnimationReset){double speed=command.action==Action::AnimationReset?clip.speed:command.value;if(!std::isfinite(speed)||speed<=0)continue;values.at(clip.speedVariable)=speed;changed.insert(owner->id);}
      else if(command.action==Action::AnimationPlay)player.Play(clip);
      else if(command.action==Action::AnimationStop)player.Stop(clip);
      else if(playback.playing)player.Pause(clip);
      publish=true;continue;
    }
    if(command.action==Action::Value) {
      auto control=owner->controls.find(command.name);const EiemControlProgram::Parameter* parameter=nullptr;
      if(owner->program){auto d=owner->program->parameters.find(command.name);if(d!=owner->program->parameters.end())parameter=&d->second;}
      if(parameter){if(!parameter->exposed||parameter->computed)continue;try{parameter->Validate(command.value);}catch(const std::exception&){continue;}}
      else if(control==owner->controls.end() || !std::isfinite(command.value) ||command.value<control->second.minimum || command.value>control->second.maximum) continue;
      try {EiemEntryPreferences::Validate(*owner,{{command.name,command.value}});}catch(const std::exception&){continue;}
      publish=animations.at(owner->id).Manual(*owner,command.name)||publish;
      if(values.at(command.name)!=command.value) {values.at(command.name)=command.value;changed.insert(owner->id);}
    } else {
      auto binding=std::find_if(owner->bindings.begin(),owner->bindings.end(),[&](const Binding& b){return b.id==command.name;});
      if(binding==owner->bindings.end()) continue;
      if(command.action==Action::Hold && binding->type=="hold" && (command.value==0 || command.value==1)) {
        if(command.value==1) uiHold=binding->id;
        else if(uiHold==binding->id) uiHold.clear();
      }
      else if(command.action==Action::Invoke && binding->type!="hold") {
        publish=animations.at(owner->id).Manual(*owner,binding->variable)||publish;
        auto& value=values.at(binding->variable);double before=value;value=Advance(*binding,value,0);
        if(value!=before) changed.insert(owner->id);
      }
    }
  }
  if(!focused || !panel.open) uiHold.clear();
  if(publish)++valueRevision;Capture(changed);if(publish || !changed.empty()) PublishPanel();
}
void PollControls(bool focused,double elapsed,bool (*sample)(const EiemKeyChord&)=Down) {
  Require(std::isfinite(elapsed) && elapsed>=0 && sample,"runtime input sample");
  std::set<std::string> changed;bool publish=false;
  for (const auto& package:keys) for (const auto& key:package.second) {
    auto id=package.first+"/"+key.id;
    // Observe physical edges even while unfocused; returning to the game with
    // an already-held cycle key must not manufacture a new press.
    bool physical=sample(key.key),previous=held[id];held[id]=physical;
    bool down=focused && keySwitchEnabled.at(package.first) && !panel.open && physical;
    bool uiDown=focused && panel.open && package.first==selected && key.type=="hold" && key.id==uiHold;
    auto& value=variables.at(package.first).at(key.variable);double before=value;
    if (uiDown || down && (key.type=="hold" || !previous)) {if(controlGraph)for(const auto& p:*controlGraph)if(p.id==package.first)publish=animations.at(p.id).Manual(p,key.variable)||publish;value=Advance(key,value,elapsed);}
    if (before!=value) changed.insert(package.first);
    if (physical!=previous || before!=value && key.type!="hold")
      Log("[CONTROL-RUNTIME-KEY] package=%s binding=%s vk=%u down=%d focused=%d enabled=%d panel=%d value=%.6g",package.first.c_str(),key.id.c_str(),key.key.vk,int(physical),int(focused),int(keySwitchEnabled.at(package.first)),int(panel.open.load()),value);
  }
  if(controlGraph)for(const auto& binding:animationKeys){const auto& p=controlGraph->at(binding.package);const auto& clip=p.shapeAnimations.at(binding.clip);auto& player=animations.at(p.id);auto& state=player.clips.at(clip.id);bool physical=sample(clip.key),previous=state.held;state.held=physical;if(focused&&!panel.open&&keySwitchEnabled.at(p.id)&&physical&&!previous){if(state.playing)player.Stop(clip);else player.Play(clip);publish=true;}}
  if(publish||!changed.empty())++valueRevision;
  if(controlGraph)for(const auto& p:*controlGraph)if(p.program)for(const auto& event:p.program->events){auto& old=held[p.id+"/"+event.id];bool physical=sample(event.key),before=old;old=physical;
    bool fire=event.trigger=="hold"?physical:event.trigger=="release"?before&&!physical:!before&&physical;
    if(focused&&!panel.open&&keySwitchEnabled.at(p.id)&&fire)publish=RunAction(p,event.action,event.trigger=="hold"?elapsed:0,changed)||publish;
  }
  if(publish)++valueRevision;
  Capture(changed);if(publish||!changed.empty()) PublishPanel();
}
void AdvanceAnimations(double elapsed){bool publish=false,advanced=false;if(controlGraph)for(auto index:animationPackages){const auto& p=controlGraph->at(index);auto& player=animations.at(p.id);publish=player.Advance(p,variables.at(p.id),elapsed)||publish;advanced|=player.LastVisited()!=0;}if(advanced)++valueRevision;if(publish||(advanced&&panel.open))PublishPanel();}
double ShapeValue(const std::string& package,const std::string& variable){return EffectiveValues(package).at(variable);}
} // namespace RuntimeControls

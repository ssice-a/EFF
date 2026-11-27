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
  std::string material, skeleton;
  bool afterimage = true;
};

constexpr const char* originalRendererKey = "__EFF_F11_ORIGINAL__";
const Spec& OriginalRendererSpec() {
  static const Spec spec = [] {
    Spec value;
    value.marker = originalRendererKey;
    value.visible = EiemExpression::Make();
    value.visible->number = 1;
    return value;
  }();
  return spec;
}
bool showOriginals = false;
uint64_t originalToggleRevision = 0;
EiemControlPanel::Shortcut originalShortcut;
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
std::map<std::string,bool> customGuiEnabled;
uint64_t revision=0, scopeRevision=0;
uint64_t valueRevision=0;
std::map<std::string,EiemVariables> effectiveValues;
std::map<std::string,std::set<std::string>> continuousChannels;
std::map<std::string,std::string> programErrors;
std::map<std::string,std::string> effectiveErrors;
std::map<std::string,std::string> animationErrors;
std::map<std::string,uint64_t> stateRevisions,effectiveRevisions;
std::map<std::string,const Package*> controlOwners;
#if defined(EIEM_PANEL_TEST)
std::map<std::string,size_t> effectiveEvaluations;
#endif
const EiemVariables& EffectiveValues(const std::string& package){
  auto owner=controlOwners.find(package);
  if(owner==controlOwners.end())return variables.at(package);
  const auto& p=*owner->second;auto current=stateRevisions.at(package);
  auto cached=effectiveRevisions.find(package);
  if(cached==effectiveRevisions.end()||cached->second!=current){
    try{
#if defined(EIEM_PANEL_TEST)
      ++effectiveEvaluations[p.id];
#endif
      auto values=variables.at(p.id);auto player=animations.find(p.id);if(player!=animations.end())for(const auto& v:player->second.output)values[v.first]=v.second;
      if(p.program)values=EiemControlProgram::Evaluate(*p.program,std::move(values));effectiveValues[p.id]=std::move(values);effectiveErrors.erase(p.id);
    }catch(const std::exception& e){if(!effectiveValues.count(p.id))effectiveValues[p.id]=variables.at(p.id);if(effectiveErrors[p.id]!=e.what())Log("[CONTROL-PROGRAM-GAP] mod=%s reason=%s",p.id.c_str(),e.what());effectiveErrors[p.id]=e.what();}
    effectiveRevisions[package]=current;
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
  std::map<std::string,bool> customGuiEnabled;
  std::shared_ptr<EiemControlPanel::Snapshot> panelSnapshot;
  std::vector<AnimationKey> animationKeys;
  std::vector<size_t> animationPackages;
  std::map<std::string,std::set<std::string>> continuousChannels;
  std::map<std::string,EiemVariables> effective;
  std::map<std::string,uint64_t> stateRevisions,effectiveRevisions;
  std::map<std::string,const Package*> owners;
};
bool Selectable(const Package& p) { return !p.customGui.script.empty()||bool(p.program)||!p.bindings.empty() || !p.controls.empty() || !p.shapeAnimations.empty(); }
bool HasKeys(const Package& p){return p.customGui.key.vk||(p.program&&!p.program->events.empty())||!p.bindings.empty()||std::any_of(p.shapeAnimations.begin(),p.shapeAnimations.end(),[](const auto& row){return row.second.key.vk!=0;});}
void AnimationSnapshot(EiemControlPanel::Snapshot& view,const std::map<std::string,EiemShapeAnimation::Player>& source){for(const auto& p:source)view.animations[p.first]=p.second.clips;}
void PreparePanel(State& state) {
  // Runs before property writes, preserving no-throw graph publication.
  state.panelSnapshot=std::make_shared<EiemControlPanel::Snapshot>();
  state.panelSnapshot->graph=state.graph;
  state.animationKeys.clear();state.animationPackages.clear();
  state.continuousChannels.clear();
  state.stateRevisions.clear();state.effectiveRevisions.clear();state.owners.clear();
  for(size_t i=0;i<state.graph->size();++i){const auto& p=state.graph->at(i);if(!p.shapeAnimations.empty())state.animationPackages.push_back(i);
    state.owners.emplace(p.id,&p);state.stateRevisions.emplace(p.id,0);state.effectiveRevisions.emplace(p.id,0);
    if(p.program)for(const auto& row:p.program->parameters)state.continuousChannels[p.id].insert(row.first);
    for(const auto& row:p.shapeAnimations)if(row.second.key.vk)state.animationKeys.push_back({i,row.first});}
  if (state.selected.empty()) {
    for (const auto& p:*state.graph) if (!p.bindings.empty()) {state.selected=p.id;break;}
    if (state.selected.empty()) for (const auto& p:*state.graph) if (Selectable(p)) {state.selected=p.id;break;}
  }
  state.panelSnapshot->selected=state.selected;
  state.panelSnapshot->keySwitchEnabled=state.keySwitchEnabled;
  state.panelSnapshot->customGuiEnabled=state.customGuiEnabled;
  state.panelSnapshot->values=state.variables;
  for(const auto& p:*state.graph){auto values=state.variables.at(p.id);for(const auto& v:state.animations.at(p.id).output)values[v.first]=v.second;if(p.program)values=EiemControlProgram::Evaluate(*p.program,std::move(values));state.panelSnapshot->effective[p.id]=std::move(values);}
  state.effective=state.panelSnapshot->effective;
  AnimationSnapshot(*state.panelSnapshot,state.animations);
}
void PublishPanel() {
  auto view=std::make_shared<EiemControlPanel::Snapshot>();
  view->revision=revision;view->scope=scopeRevision;view->selected=selected;
  view->keySwitchEnabled=keySwitchEnabled;view->graph=controlGraph;view->values=variables;
  view->customGuiEnabled=customGuiEnabled;
  if(controlGraph)for(const auto& p:*controlGraph)view->effective[p.id]=EffectiveValues(p.id);view->errors=programErrors;for(const auto& error:effectiveErrors)view->errors[error.first]=error.second;for(const auto& error:animationErrors)view->errors[error.first]=error.second;
  AnimationSnapshot(*view,animations);
  panel.Publish(std::move(view));
}
bool Down(const EiemKeyChord& key) {
  return (GetAsyncKeyState(key.vk)&0x8000) &&
    (!(key.modifiers&MOD_SHIFT) || (GetAsyncKeyState(VK_SHIFT)&0x8000)) &&
    (!(key.modifiers&MOD_CONTROL) || (GetAsyncKeyState(VK_CONTROL)&0x8000)) &&
    (!(key.modifiers&MOD_ALT) || (GetAsyncKeyState(VK_MENU)&0x8000));
}
// Sample the physical edge separately from extra modifiers so releasing an
// unrelated modifier (e.g. Ctrl in Ctrl+F11 when toggle_mods=F11) never turns
// into a plain shortcut press, while configured chords (e.g. Ctrl+F11) match
// their exact modifier mask.
void PollOriginalShortcut(EiemKeyChord shortcut, bool keyDown, UINT activeModifiers, bool focused) {
  if (!shortcut.vk) return;
  const bool exactModifiers = activeModifiers == shortcut.modifiers;
  if (originalShortcut.Press(shortcut, keyDown, focused) && exactModifiers) {
    showOriginals = !showOriginals;
    originalToggleRevision = ++valueRevision;
    Log("[GLOBAL-MOD-VISIBILITY] original=%d vk=%u modifiers=%u", int(showOriginals), shortcut.vk, shortcut.modifiers);
  }
}
void PreviewPreferences(State& state) {
  auto live=controlGraph;
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
    auto validateKey=[&](const EiemKeyChord& key){Require(!key.vk||(key.vk!=config.reload.vk&&key.vk!=config.gui.vk&&key.vk!=config.toggleMods.vk),"Mod shortcut overlaps a global shortcut: "+package.id);};
    for(const auto& key:package.bindings)validateKey(key.key);
    for(const auto& clip:package.shapeAnimations)validateKey(clip.second.key);
    if(package.program)for(const auto& event:package.program->events){validateKey(event.key);state.held[package.id+"/"+event.id]=Down(event.key);}
    state.variables[package.id]=package.defaults;state.keys[package.id]=package.bindings;
    state.keySwitchEnabled[package.id]=keySettings.Preview(package);
    if(!package.customGui.script.empty())state.customGuiEnabled[package.id]=keySettings.Preview(package,true);
    PrepareAnimations(state,package);
    for(const auto& rule:package.rules) if(!rule.second.skip)
      for(const auto& target:rule.second.targets) for(const auto& partId:rule.second.parts) {
        const auto& part=package.parts.at(partId);const auto& mesh=package.meshes.at(part.mesh);
        for(const auto& marker:{RuntimePartName(package,partId,target.second),LegacyRuntimePartName(package,partId,target.second)}) {
          Spec spec{package.id,partId,marker,part.mesh,mesh.vertices,mesh.shapes,part.shapes,part.visible};
          spec.material=part.material;spec.skeleton=mesh.skeleton;
          spec.afterimage=part.afterimage;
          Require(state.specs.emplace(marker,std::move(spec)).second,"runtime part provenance collision");
        }
      }
  }
  PreviewPreferences(state);PreparePanel(state);return state;
}
std::shared_ptr<const std::vector<Package>> InstalledGraph(
    const std::shared_ptr<const std::vector<Package>>& models,
    const std::shared_ptr<const std::vector<Package>>& standalone) {
  Require(bool(models),"runtime model control graph unavailable");
  auto graph=std::make_shared<std::vector<Package>>(*models);
  if(standalone)graph->insert(graph->end(),standalone->begin(),standalone->end());
  std::set<std::string> ids;
  for(const auto& package:*graph)Require(ids.insert(package.id).second,"runtime installed Mod id collision: "+package.id);
  return graph;
}
State PrepareInstalled(const std::shared_ptr<const std::vector<Package>>& models,
    const std::shared_ptr<const std::vector<Package>>& standalone,const EiemGlobalConfig& config) {
  return Prepare(InstalledGraph(models,standalone),config);
}
void Swap(State& state) noexcept {
  specs.swap(state.specs);variables.swap(state.variables);keys.swap(state.keys);animations.swap(state.animations);
  held.swap(state.held);controlGraph.swap(state.graph);
  animationKeys.swap(state.animationKeys);animationPackages.swap(state.animationPackages);
  continuousChannels.swap(state.continuousChannels);
  selected.swap(state.selected);keySwitchEnabled.swap(state.keySwitchEnabled);customGuiEnabled.swap(state.customGuiEnabled);uiHold.clear();
  ++revision;++scopeRevision;++valueRevision;
  effectiveValues.swap(state.effective);stateRevisions.swap(state.stateRevisions);effectiveRevisions.swap(state.effectiveRevisions);controlOwners.swap(state.owners);programErrors.clear();effectiveErrors.clear();animationErrors.clear();
#if defined(EIEM_PANEL_TEST)
  effectiveEvaluations.clear();
#endif
  if(state.panelSnapshot) {
    state.panelSnapshot->revision=revision;state.panelSnapshot->scope=scopeRevision;
    panel.Publish(std::move(state.panelSnapshot));
  }
}
void LoadControls(const std::shared_ptr<const std::vector<Package>>& graph) {
  auto next=PrepareInstalled(graph,DemandLoadingProbe::StandalonePackages(DemandLoadingProbe::CurrentInstalledGeneration()),settings.Get());
  Swap(next);
}
void MergeLive(State& next,bool (*sample)(const EiemKeyChord&)=Down) {
  Require(bool(next.graph) && sample,"reload controls candidate missing");
  for(const auto& p:*next.graph) {
    next.keySwitchEnabled[p.id]=keySettings.Preview(p);
    if(!p.customGui.script.empty())next.customGuiEnabled[p.id]=keySettings.Preview(p,true);
    const Package* previous=nullptr;
    if(controlGraph) for(const auto& old:*controlGraph) if(old.id==p.id && old.root==p.root) previous=&old;
    auto oldValues=variables.find(p.id);
    // Retain actual runtime edits, not an untouched default from the previous
    // author generation. This applies to every retained variable, including
    // animation speed, without giving a channel its own reload policy.
    auto retained=[&](const std::string& name,double value){
      auto before=previous->defaults.find(name);
      return before!=previous->defaults.end()&&value==before->second?p.defaults.at(name):value;
    };
    for(const auto& name:p.persistent) if(previous && previous->persistent.count(name) && oldValues!=variables.end() && oldValues->second.count(name)) {
      if(p.program&&p.program->parameters.count(name))continue;
      double value=retained(name,oldValues->second.at(name));
      try {EiemEntryPreferences::Validate(p,{{name,value}});next.variables.at(p.id).at(name)=value;}
      catch(const std::exception&) {next.variables.at(p.id).at(name)=p.defaults.at(name);Log("[ENTRY-RELOAD-VALUE] package=%s variable=%s invalidOldDomain=1 defaultUsed=1",p.id.c_str(),name.c_str());}
    }
    // A variable is the stable state identity. Section IDs and display labels
    // may change without resetting compatible values; readonly/type/range and
    // explicit reload=reset remain authoritative.
    if(p.program&&previous&&previous->program&&oldValues!=variables.end())for(const auto& row:p.program->parameters){const auto& d=row.second;auto old=previous->program->parameters.find(row.first);
      if(d.computed)continue;
      next.variables.at(p.id).at(row.first)=p.defaults.at(row.first);
      if(!d.retain||old==previous->program->parameters.end()||old->second.computed||old->second.type!=d.type||!oldValues->second.count(row.first))continue;
      try{auto value=retained(row.first,oldValues->second.at(row.first));d.Validate(value);next.variables.at(p.id).at(row.first)=value;}catch(const std::exception&){}
    }
    for(const auto& key:p.bindings) next.held[p.id+"/"+key.id]=sample(key.key);
    if(previous&&animations.count(p.id))next.animations.at(p.id).RetainCompatible(p,*previous,animations.at(p.id));
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
  if(controlGraph) for(const auto& p:*controlGraph) if(changed.count(p.id)) preferences.Capture(p,variables.at(p.id));
}
void Changed(const std::string& package){++stateRevisions[package];++valueRevision;}
using PlaybackCommand=std::pair<EiemControlProgram::Instruction::Op,std::string>;
// Every input submits a delta. Validate it before touching either base values
// or animation ownership; only a fully prepared candidate can become live.
bool CommitValues(const Package& p,const EiemVariables& updates,const std::vector<PlaybackCommand>& commands,
                  std::set<std::string>& changed){
  for(const auto& value:updates)if(p.program){auto d=p.program->parameters.find(value.first);
    Require(d==p.program->parameters.end()||!d->second.computed,"computed parameter is read only: "+value.first);}
  EiemEntryPreferences::Validate(p,updates);
  auto& live=variables.at(p.id);bool different=false;
  for(const auto& value:updates)different|=live.at(value.first)!=value.second;
  auto& player=animations.at(p.id);bool playback=!commands.empty();
  for(const auto& value:updates){auto owners=player.trackOwners.find(value.first);if(owners!=player.trackOwners.end())
    for(const auto& id:owners->second)playback|=player.clips.at(id).playing;}
  std::unique_ptr<EiemShapeAnimation::Player> nextPlayer;
  if(playback){nextPlayer=std::make_unique<EiemShapeAnimation::Player>(player);
    for(const auto& value:updates)nextPlayer->Manual(p,value.first);
    for(const auto& command:commands){const auto& clip=p.shapeAnimations.at(command.second);
      if(command.first==EiemControlProgram::Instruction::Play)nextPlayer->Play(clip);
      else if(command.first==EiemControlProgram::Instruction::Pause)nextPlayer->Pause(clip);
      else{Require(command.first==EiemControlProgram::Instruction::Stop,"invalid playback command");nextPlayer->Stop(clip);}}
  }
  EiemVariables nextValues;
  if(different){nextValues=live;for(const auto& value:updates)nextValues.at(value.first)=value.second;changed.insert(p.id);}
  if(different)live.swap(nextValues);
  if(nextPlayer)std::swap(player,*nextPlayer);
  if(different||playback)Changed(p.id);
  return different||playback;
}
bool ControlError(const Package& p,const std::string& input,const std::exception& e){
  bool different=programErrors[p.id]!=e.what();
  if(different)Log("[CONTROL-ACTION-GAP] mod=%s input=%s reason=%s",p.id.c_str(),input.c_str(),e.what());
  programErrors[p.id]=e.what();return different;
}
bool SubmitValues(const Package& p,const EiemVariables& updates,const std::vector<PlaybackCommand>& commands,
                  std::set<std::string>& changed,const std::string& input){
  try{bool publish=CommitValues(p,updates,commands,changed);return programErrors.erase(p.id)!=0||publish;}
  catch(const std::exception& e){return ControlError(p,input,e);}
}
bool RunAction(const Package& p,const std::string& id,double elapsed,std::set<std::string>& changed){
  if(!p.program)return false;
  try{
    const auto& effective=EffectiveValues(p.id);
    auto result=EiemControlProgram::Execute(*p.program,id,variables.at(p.id),elapsed,&effective);
    EiemVariables updates;for(const auto& name:result.written)updates.emplace(name,result.values.at(name));
    return SubmitValues(p,updates,result.commands,changed,id);
  }catch(const std::exception& e){return ControlError(p,id,e);}
}
bool RunGuiRequests(const Package& p,const EiemCustomGui::Requests& requests,std::set<std::string>& changed) {
  using Op=EiemCustomGui::Operation;
  using Instruction=EiemControlProgram::Instruction;
  try {
    Require(requests.size()<=64,"too many GUI requests");
    auto values=variables.at(p.id);
    auto effective=EffectiveValues(p.id);
    EiemVariables updates;
    std::vector<PlaybackCommand> playback;
    auto stage=[&](const std::string& name,double value){
      if(p.program) {
        auto parameter=p.program->parameters.find(name);
        Require(parameter==p.program->parameters.end()||!parameter->second.computed,"computed parameter is read only: "+name);
      }
      EiemEntryPreferences::Validate(p,{{name,value}});
      values.at(name)=effective.at(name)=updates[name]=value;
    };
    for(const auto& request:requests) {
      if(request.operation==Op::Set)stage(request.name,request.value);
      else if(request.operation==Op::Invoke) {
        Require(p.program&&p.program->actions.count(request.name),"GUI action absent: "+request.name);
        auto result=EiemControlProgram::Execute(*p.program,request.name,values,0,&effective);
        for(const auto& name:result.written)stage(name,result.values.at(name));
        playback.insert(playback.end(),result.commands.begin(),result.commands.end());
      } else if(request.operation==Op::Trigger) {
        auto binding=std::find_if(p.bindings.begin(),p.bindings.end(),[&](const Binding& b){return b.id==request.name;});
        Require(binding!=p.bindings.end()&&binding->type!="hold","GUI requires a cycle/press binding: "+request.name);
        stage(binding->variable,Advance(*binding,values.at(binding->variable),0));
      } else {
        Require(p.shapeAnimations.count(request.name),"GUI clip absent: "+request.name);
        Require(request.operation==Op::Play||request.operation==Op::Pause||request.operation==Op::Stop,"invalid GUI operation");
        auto op=request.operation==Op::Play?Instruction::Play:request.operation==Op::Pause?Instruction::Pause:Instruction::Stop;
        playback.emplace_back(op,request.name);
      }
      Require(playback.size()<=256,"too many GUI playback operations");
    }
    return SubmitValues(p,updates,playback,changed,"custom GUI");
  } catch(const std::exception& e) {return ControlError(p,"custom GUI",e);}
}
void PanelCommands(bool focused,bool (*sample)(const EiemKeyChord&)=Down) {
  bool publish=false;std::set<std::string> changed;
  for(const auto& command:panel.Drain()) {
    using EiemControlPanel::Action;
    const bool scripted=command.action==Action::GuiRequests;
    if(!focused || !controlGraph || command.revision!=revision ||
       (!scripted&&!panel.open) ||
       (!scripted&&command.action!=Action::Keyboard&&command.action!=Action::GuiEnabled&&command.scope!=scopeRevision)) continue;
    const Package* owner=nullptr;
    for(const auto& p:*controlGraph) if(p.id==command.package && Selectable(p)) owner=&p;
    if(!owner) continue;
    if(command.action==Action::GuiEnabled) {
      auto enabled=customGuiEnabled.find(owner->id);
      if(enabled==customGuiEnabled.end()||(command.value!=0&&command.value!=1))continue;
      if(enabled->second!=(command.value!=0)) {
        enabled->second=command.value!=0;
        if(!enabled->second)panel.CloseGui(owner->id);
        keySettings.Capture(*owner,enabled->second,true);publish=true;
      }
      continue;
    }
    if(scripted) {
      auto enabled=customGuiEnabled.find(owner->id);
      if(enabled!=customGuiEnabled.end()&&enabled->second&&command.scope&&panel.GuiSession(owner->id)==command.scope)
        publish=RunGuiRequests(*owner,command.requests,changed)||publish;
      continue;
    }
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
      if(command.action==Action::AnimationSpeed||command.action==Action::AnimationReset){
        if(owner->program){auto d=owner->program->parameters.find(clip.speedVariable);if(d!=owner->program->parameters.end()&&(!d->second.exposed||d->second.computed))continue;}
        double speed=command.action==Action::AnimationReset?clip.speed:command.value;
        publish=SubmitValues(*owner,{{clip.speedVariable,speed}},{},changed,clip.id)||publish;
      }else if(command.action!=Action::AnimationPause||playback.playing){
        auto op=command.action==Action::AnimationPlay?EiemControlProgram::Instruction::Play:
          command.action==Action::AnimationStop?EiemControlProgram::Instruction::Stop:EiemControlProgram::Instruction::Pause;
        publish=SubmitValues(*owner,{},{{op,clip.id}},changed,clip.id)||publish;
      }continue;
    }
    if(command.action==Action::Value) {
      auto control=owner->controls.find(command.name);const EiemControlProgram::Parameter* parameter=nullptr;
      if(owner->program){auto d=owner->program->parameters.find(command.name);if(d!=owner->program->parameters.end())parameter=&d->second;}
      if(parameter){if(!parameter->exposed||parameter->computed)continue;try{parameter->Validate(command.value);}catch(const std::exception&){continue;}}
      else if(control==owner->controls.end() || !std::isfinite(command.value) ||command.value<control->second.minimum || command.value>control->second.maximum) continue;
      publish=SubmitValues(*owner,{{command.name,command.value}},{},changed,command.name)||publish;
    } else {
      auto binding=std::find_if(owner->bindings.begin(),owner->bindings.end(),[&](const Binding& b){return b.id==command.name;});
      if(binding==owner->bindings.end()) continue;
      if(command.action==Action::Hold && binding->type=="hold" && (command.value==0 || command.value==1)) {
        if(command.value==1) uiHold=binding->id;
        else if(uiHold==binding->id) uiHold.clear();
      }
      else if(command.action==Action::Invoke && binding->type!="hold") {
        publish=SubmitValues(*owner,{{binding->variable,Advance(*binding,values.at(binding->variable),0)}},{},changed,binding->id)||publish;
      }
    }
  }
  if(!focused || !panel.open) uiHold.clear();
  Capture(changed);if(publish || !changed.empty()) PublishPanel();
}
void PollControls(bool focused,double elapsed,bool (*sample)(const EiemKeyChord&)=Down) {
  Require(std::isfinite(elapsed) && elapsed>=0 && sample,"runtime input sample");
  std::set<std::string> changed;bool publish=false;
  const bool panelVisible=panel.Visible();
  for (const auto& package:keys){
    const Package* owner=nullptr;if(controlGraph)for(const auto& p:*controlGraph)if(p.id==package.first)owner=&p;
    if(!owner)continue;
    for (const auto& key:package.second) {
    auto id=package.first+"/"+key.id;
    // Observe physical edges even while unfocused; returning to the game with
    // an already-held cycle key must not manufacture a new press.
    bool physical=sample(key.key),previous=held[id];held[id]=physical;
    bool down=focused && keySwitchEnabled.at(package.first) && !panelVisible && physical;
    bool uiDown=focused && panel.open && package.first==selected && key.type=="hold" && key.id==uiHold;
    double before=variables.at(package.first).at(key.variable);
    if (uiDown || down && (key.type=="hold" || !previous))
      publish=SubmitValues(*owner,{{key.variable,Advance(key,before,elapsed)}},{},changed,key.id)||publish;
    double value=variables.at(package.first).at(key.variable);
    if (physical!=previous || before!=value && key.type!="hold")
      Log("[CONTROL-RUNTIME-KEY] package=%s binding=%s vk=%u down=%d focused=%d enabled=%d panel=%d value=%.6g",package.first.c_str(),key.id.c_str(),key.key.vk,int(physical),int(focused),int(keySwitchEnabled.at(package.first)),int(panel.open.load()),value);
  }}
  if(controlGraph)for(const auto& binding:animationKeys){const auto& p=controlGraph->at(binding.package);const auto& clip=p.shapeAnimations.at(binding.clip);auto& player=animations.at(p.id);auto& state=player.clips.at(clip.id);bool physical=sample(clip.key),previous=state.held;state.held=physical;if(focused&&!panelVisible&&keySwitchEnabled.at(p.id)&&physical&&!previous){auto op=state.playing?EiemControlProgram::Instruction::Stop:EiemControlProgram::Instruction::Play;publish=SubmitValues(p,{},{{op,clip.id}},changed,clip.id)||publish;}}
  if(controlGraph)for(const auto& p:*controlGraph)if(p.program)for(const auto& event:p.program->events){auto& old=held[p.id+"/"+event.id];bool physical=sample(event.key),before=old;old=physical;
    bool fire=event.trigger=="hold"?physical:event.trigger=="release"?before&&!physical:!before&&physical;
    if(focused&&!panelVisible&&keySwitchEnabled.at(p.id)&&fire)publish=RunAction(p,event.action,event.trigger=="hold"?elapsed:0,changed)||publish;
  }
  Capture(changed);if(publish||!changed.empty()) PublishPanel();
}
void AdvanceAnimations(double elapsed){
  bool publish=false,advanced=false;
  if(controlGraph)for(auto index:animationPackages){const auto& p=controlGraph->at(index);
    try{auto& player=animations.at(p.id);bool changed=player.Advance(p,variables.at(p.id),elapsed);publish|=changed;publish|=animationErrors.erase(p.id)!=0;
      if(changed||player.LastVisited()!=0){Changed(p.id);advanced=true;}
    }catch(const std::exception& e){bool different=animationErrors[p.id]!=e.what();
      if(different)Log("[CONTROL-ANIMATION-GAP] mod=%s reason=%s",p.id.c_str(),e.what());
      animationErrors[p.id]=e.what();publish|=different;
    }
  }
  if(publish||(advanced&&panel.Visible()))PublishPanel();
}
double ShapeValue(const std::string& package,const std::string& variable){return EffectiveValues(package).at(variable);}
} // namespace RuntimeControls

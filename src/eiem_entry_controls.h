#pragma once
#include "eiem_entry_preferences.h"
#include "eiem_shape_snapshot.h"
#include "eiem_control_panel.h"
// Numeric author state has no dependency on Unity Meshes or capture lifetime.
namespace RuntimeControls {
struct Spec {
  std::string package, part, marker;
  unsigned vertices = 0;
  std::vector<std::string> channels;
  std::map<std::string,std::string> controls;
  std::shared_ptr<EiemExpression> visible;
};

std::map<std::string,Spec> specs;
std::map<std::string,EiemVariables> variables;
std::map<std::string,std::vector<Binding>> keys;
std::shared_ptr<const std::vector<Package>> controlGraph;
EiemEntryPreferences preferences;
std::map<std::string,bool> held;
EiemControlPanel::Store panel;
std::string selected, uiHold;
bool keyboard=true;
uint64_t revision=0, scopeRevision=0;
struct State {
  std::map<std::string,Spec> specs;
  std::map<std::string,EiemVariables> variables;
  std::map<std::string,std::vector<Binding>> keys;
  std::map<std::string,bool> held;
  std::shared_ptr<const std::vector<Package>> graph;
  std::string selected;
  bool keyboard=true;
  std::shared_ptr<EiemControlPanel::Snapshot> panelSnapshot;
};
bool Selectable(const Package& p) { return !p.bindings.empty() || !p.controls.empty(); }
void PreparePanel(State& state) {
  // Runs before property writes, preserving no-throw graph publication.
  state.panelSnapshot=std::make_shared<EiemControlPanel::Snapshot>();
  state.panelSnapshot->graph=state.graph;
  if (state.selected.empty()) {
    for (const auto& p:*state.graph) if (!p.bindings.empty()) {state.selected=p.id;break;}
    if (state.selected.empty()) for (const auto& p:*state.graph) if (Selectable(p)) {state.selected=p.id;break;}
  }
  state.panelSnapshot->selected=state.selected;
  state.panelSnapshot->keyboard=state.keyboard;
  state.panelSnapshot->values=state.variables;
}
void PublishPanel() {
  auto view=std::make_shared<EiemControlPanel::Snapshot>();
  view->revision=revision;view->scope=scopeRevision;view->selected=selected;
  view->keyboard=keyboard;view->graph=controlGraph;view->values=variables;
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
State Prepare(const std::shared_ptr<const std::vector<Package>>& graph,const EiemGlobalConfig& config) {
  Require(bool(graph),"runtime control graph unavailable");
  State state;state.graph=graph;
  for (const auto& package:*graph) {
    state.variables[package.id]=package.defaults;state.keys[package.id]=package.bindings;
    for(const auto& key:package.bindings)
      // Chords permit additional modifiers, so bindings with the same base
      // key can overlap even when their modifier lists differ.
      Require(key.key.vk!=config.reload.vk && key.key.vk!=config.gui.vk,
              "Mod key overlaps reserved global shortcut: "+package.id+"/"+key.id);
    for(const auto& rule:package.rules) if(!rule.second.skip)
      for(const auto& target:rule.second.targets) for(const auto& partId:rule.second.parts) {
        const auto& part=package.parts.at(partId);const auto& mesh=package.meshes.at(part.mesh);
        Spec spec{package.id,partId,RuntimePartName(package,partId,target.second),mesh.vertices,mesh.shapes,part.shapes,part.visible};
        Require(state.specs.emplace(spec.marker,std::move(spec)).second,"runtime part provenance collision");
      }
  }
  PreviewPreferences(state);PreparePanel(state);return state;
}
void Swap(State& state) noexcept {
  specs.swap(state.specs);variables.swap(state.variables);keys.swap(state.keys);
  held.swap(state.held);controlGraph.swap(state.graph);
  selected.swap(state.selected);keyboard=state.keyboard;uiHold.clear();
  ++revision;++scopeRevision;
  if(state.panelSnapshot) {
    state.panelSnapshot->revision=revision;state.panelSnapshot->scope=scopeRevision;
    panel.Publish(std::move(state.panelSnapshot));
  }
}
void LoadControls(const std::shared_ptr<const std::vector<Package>>& graph) {
  auto next=Prepare(graph,settings.Get());
  Swap(next);
}
void MergeLive(State& next,bool (*sample)(const EiemKeyChord&)=Down) {
  Require(bool(next.graph) && sample,"reload controls candidate missing");
  for(const auto& p:*next.graph) {
    const Package* previous=nullptr;
    if(controlGraph) for(const auto& old:*controlGraph) if(old.id==p.id && old.root==p.root) previous=&old;
    auto oldValues=variables.find(p.id);
    for(const auto& name:p.persistent) if(previous && previous->persistent.count(name) && oldValues!=variables.end() && oldValues->second.count(name)) {
      double value=oldValues->second.at(name);
      try {EiemEntryPreferences::Validate(p,{{name,value}});next.variables.at(p.id).at(name)=value;}
      catch(const std::exception&) {next.variables.at(p.id).at(name)=p.defaults.at(name);Log("[ENTRY-RELOAD-VALUE] package=%s variable=%s invalidOldDomain=1 defaultUsed=1",p.id.c_str(),name.c_str());}
    }
    for(const auto& key:p.bindings) next.held[p.id+"/"+key.id]=sample(key.key);
  }
  next.selected.clear();next.keyboard=keyboard;
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
void PanelCommands(bool focused,bool (*sample)(const EiemKeyChord&)=Down) {
  bool publish=false;std::set<std::string> changed;
  for(const auto& command:panel.Drain()) {
    using EiemControlPanel::Action;
    if(!focused || !panel.open || !controlGraph || command.revision!=revision || command.scope!=scopeRevision) continue;
    const Package* owner=nullptr;
    for(const auto& p:*controlGraph) if(p.id==command.package && Selectable(p)) owner=&p;
    if(!owner) continue;
    if(command.action==Action::Select || command.action==Action::Keyboard) {
      if(command.action==Action::Select) selected=owner->id;
      else {if(owner->id!=selected || (command.value!=0 && command.value!=1)) continue;keyboard=command.value!=0;}
      ++scopeRevision;uiHold.clear();
      // Resample every binding so selecting a target with a held chord cannot
      // turn it into a fresh cycle/press, even between polling frames.
      for(const auto& p:keys) for(const auto& key:p.second) held[p.first+"/"+key.id]=sample(key.key);
      publish=true;continue;
    }
    if(owner->id!=selected) continue;
    auto& values=variables.at(owner->id);
    if(command.action==Action::Value) {
      auto control=owner->controls.find(command.name);
      if(control==owner->controls.end() || !std::isfinite(command.value) ||
         command.value<control->second.minimum || command.value>control->second.maximum) continue;
      try {EiemEntryPreferences::Validate(*owner,{{command.name,command.value}});}catch(const std::exception&){continue;}
      if(values.at(command.name)!=command.value) {values.at(command.name)=command.value;changed.insert(owner->id);}
    } else {
      auto binding=std::find_if(owner->bindings.begin(),owner->bindings.end(),[&](const Binding& b){return b.id==command.name;});
      if(binding==owner->bindings.end()) continue;
      if(command.action==Action::Hold && binding->type=="hold" && (command.value==0 || command.value==1)) {
        if(command.value==1) uiHold=binding->id;
        else if(uiHold==binding->id) uiHold.clear();
      }
      else if(command.action==Action::Invoke && binding->type!="hold") {
        auto& value=values.at(binding->variable);double before=value;value=Advance(*binding,value,0);
        if(value!=before) changed.insert(owner->id);
      }
    }
  }
  if(!focused || !panel.open) uiHold.clear();
  Capture(changed);if(publish || !changed.empty()) PublishPanel();
}
void PollControls(bool focused,double elapsed,bool (*sample)(const EiemKeyChord&)=Down) {
  Require(std::isfinite(elapsed) && elapsed>=0 && sample,"runtime input sample");
  std::set<std::string> changed;
  for (const auto& package:keys) for (const auto& key:package.second) {
    auto id=package.first+"/"+key.id;
    // Observe physical edges even while unfocused; returning to the game with
    // an already-held cycle key must not manufacture a new press.
    bool physical=sample(key.key),previous=held[id];held[id]=physical;
    bool down=focused && keyboard && !panel.open && package.first==selected && physical;
    bool uiDown=focused && panel.open && package.first==selected && key.type=="hold" && key.id==uiHold;
    auto& value=variables.at(package.first).at(key.variable);double before=value;
    if (uiDown || down && (key.type=="hold" || !previous)) value=Advance(key,value,elapsed);
    if (before!=value) changed.insert(package.first);
    if (physical!=previous || before!=value && key.type!="hold")
      Log("[CONTROL-RUNTIME-KEY] package=%s binding=%s vk=%u down=%d focused=%d selected=%d keyboard=%d panel=%d value=%.6g",package.first.c_str(),key.id.c_str(),key.key.vk,int(physical),int(focused),int(package.first==selected),int(keyboard),int(panel.open.load()),value);
  }
  Capture(changed);if(!changed.empty()) PublishPanel();
}
} // namespace RuntimeControls

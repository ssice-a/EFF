#pragma once
#include "eiem_entry_preferences.h"
#include "eiem_shape_snapshot.h"
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
struct State {
  std::map<std::string,Spec> specs;
  std::map<std::string,EiemVariables> variables;
  std::map<std::string,std::vector<Binding>> keys;
  std::map<std::string,bool> held;
  std::shared_ptr<const std::vector<Package>> graph;
};
bool Down(const EiemKeyChord& key) {
  return (GetAsyncKeyState(key.vk)&0x8000) &&
    (!(key.modifiers&MOD_SHIFT) || (GetAsyncKeyState(VK_SHIFT)&0x8000)) &&
    (!(key.modifiers&MOD_CONTROL) || (GetAsyncKeyState(VK_CONTROL)&0x8000)) &&
    (!(key.modifiers&MOD_ALT) || (GetAsyncKeyState(VK_MENU)&0x8000));
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
  return state;
}
void Swap(State& state) noexcept {
  specs.swap(state.specs);variables.swap(state.variables);keys.swap(state.keys);
  held.swap(state.held);controlGraph.swap(state.graph);
}
void LoadControls(const std::shared_ptr<const std::vector<Package>>& graph) {
  auto next=Prepare(graph,settings.Get());
  for(const auto& p:*graph) {
    try {next.variables[p.id]=preferences.Load(p);}
    catch(const std::exception& e) {Log("[CONTROL-STATE-GAP] load=%s reason=%s usingDefaults=1",p.id.c_str(),e.what());}
  }
  Swap(next);
}
void MergeLive(State& next,bool (*sample)(const EiemKeyChord&)=Down) {
  Require(bool(next.graph) && sample,"reload controls candidate missing");
  for(const auto& p:*next.graph) {
    const Package* previous=nullptr;
    if(controlGraph) for(const auto& old:*controlGraph) if(old.id==p.id) previous=&old;
    auto oldValues=variables.find(p.id);
    for(const auto& name:p.persistent) if(previous && previous->persistent.count(name) && oldValues!=variables.end() && oldValues->second.count(name)) {
      double value=oldValues->second.at(name);
      try {EiemEntryPreferences::Validate(p,{{name,value}});next.variables.at(p.id).at(name)=value;}
      catch(const std::exception&) {Log("[ENTRY-RELOAD-VALUE] package=%s variable=%s invalidOldDomain=1 defaultUsed=1",p.id.c_str(),name.c_str());}
    }
    for(const auto& key:p.bindings) next.held[p.id+"/"+key.id]=sample(key.key);
  }
}
void PollControls(bool focused,double elapsed,bool (*sample)(const EiemKeyChord&)=Down) {
  Require(std::isfinite(elapsed) && elapsed>=0 && sample,"runtime input sample");
  std::set<std::string> changed;
  for (const auto& package:keys) for (const auto& key:package.second) {
    auto id=package.first+"/"+key.id;
    // Observe physical edges even while unfocused; returning to the game with
    // an already-held cycle key must not manufacture a new press.
    bool physical=sample(key.key),previous=held[id];held[id]=physical;
    bool down=focused && physical;
    auto& value=variables.at(package.first).at(key.variable);double before=value;
    if (down && (key.type=="hold" || !previous)) {
      if (key.type=="hold") value=EiemShapeSnapshot::Advance(value,key.values.front(),key.speed,elapsed);
      else if (key.type=="press") value=key.values.front();
      else {auto it=std::find(key.values.begin(),key.values.end(),value);value=it==key.values.end() || ++it==key.values.end()?key.values.front():*it;}
    }
    if (before!=value) changed.insert(package.first);
    if (physical!=previous || before!=value && key.type!="hold")
      Log("[CONTROL-RUNTIME-KEY] package=%s binding=%s vk=%u down=%d focused=%d value=%.6g",package.first.c_str(),key.id.c_str(),key.key.vk,int(physical),int(focused),value);
  }
  if (controlGraph) for (const auto& package:*controlGraph) if (changed.count(package.id)) preferences.Capture(package,variables.at(package.id));
}
} // namespace RuntimeControls

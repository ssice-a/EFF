#pragma once
// Worker: disk/strict graph parsing only. Unity frame: public property transaction
// followed by no-throw publication. Native loading files and capture snapshots
// retain their process/native-consumer lifetime across a configuration commit.
namespace EntryReload {
struct Candidate {
  unsigned base=0;
  RuntimeControls::State controls;
  EiemEntrySettings::Snapshot config;
  std::shared_ptr<const EiemNativeInput::InputSubstitutions> inputs;
};
std::mutex mutex;
std::shared_ptr<Candidate> pending;
struct Mutation {
  void* renderer=nullptr;
  int index=-1; // -1 means public Renderer.enabled.
  float before=0,target=0;
};
void Write(const Mutation& m,float value) {
  if(m.index<0) {
    struct Guard {bool old=RuntimeShapes::visibilityWriting;Guard(){RuntimeShapes::visibilityWriting=true;}~Guard(){RuntimeShapes::visibilityWriting=old;}} guard;
    RuntimeShapes::originalEnabledSetter(m.renderer,value!=0);
    Require((RuntimeShapes::Scalar<unsigned char>(RuntimeShapes::api.getEnabled,m.renderer)!=0)==(value!=0),"reload visibility readback mismatch");
  } else {
    int index=m.index;void* args[]={&index,&value};
    RuntimeShapes::Call(RuntimeShapes::api.setWeight,m.renderer,args);
    void* readArgs[]={&index};
    Require(std::abs(RuntimeShapes::Scalar<float>(RuntimeShapes::api.getWeight,m.renderer,readArgs)-value)<.0001f,"reload shape readback mismatch");
  }
}
void ApplyControls(RuntimeControls::State& next,bool (*sample)(const EiemKeyChord&)=RuntimeControls::Down) {
  RuntimeControls::MergeLive(next,sample);
  std::vector<Mutation> changes;
  for(const auto& item:RuntimeShapes::bound) {
    const auto& b=item.second;auto renderer=b.renderer.Get(),mesh=b.mesh.Get();
    if(!RuntimeShapes::Alive(renderer) || !RuntimeShapes::Alive(mesh) || RuntimeShapes::SourceMesh(renderer)!=mesh) continue;
    const auto& old=RuntimeControls::specs.at(b.marker);const auto& spec=next.specs.at(b.marker);
    Require(old.channels==spec.channels && old.vertices==spec.vertices,"reload live Mesh layout changed");
    const auto& values=next.variables.at(spec.package);
    float before=RuntimeShapes::Scalar<unsigned char>(RuntimeShapes::api.getEnabled,renderer)!=0;
    float target=b.modelReady ? b.nativeEnabled && (!spec.visible || spec.visible->Evaluate(values)!=0) : before;
    if(before!=target) changes.push_back({renderer,-1,before,target});
    // Formerly owned channels return to zero when their binding is removed.
    // Unowned native channels retain their current value.
    std::set<std::string> channels;
    for(const auto& c:old.controls) channels.insert(c.first);
    for(const auto& c:spec.controls) channels.insert(c.first);
    for(const auto& channel:channels) {
      int index=b.indices.at(channel);void* args[]={&index};
      before=RuntimeShapes::Scalar<float>(RuntimeShapes::api.getWeight,renderer,args);
      auto control=spec.controls.find(channel);
      target=control==spec.controls.end()?0:float(values.at(control->second)*100.0);
      Require(std::isfinite(before) && std::isfinite(target),"reload nonfinite shape value");
      if(before!=target) changes.push_back({renderer,index,before,target});
    }
  }
  size_t attempted=0;
  try {
    for(const auto& change:changes) {++attempted;Write(change,change.target);}
  } catch(...) {
    bool restored=true;
    while(attempted) {const auto& m=changes[--attempted];try {Write(m,m.before);}catch(const std::exception& e) {restored=false;Log("[ENTRY-RELOAD-ROLLBACK-GAP] renderer=%p reason=%s",m.renderer,e.what());}}
    Log("[ENTRY-RELOAD-ROLLBACK] writesRestored=%d graphPublished=0",int(restored));
    throw;
  }
  RuntimeControls::Swap(next); // No allocation/Unity calls after successful writes.
}
std::shared_ptr<Candidate> Validate(std::shared_ptr<const std::vector<Package>> next,const EiemGlobalConfig& config) {
  auto result=std::make_shared<Candidate>();result->base=generation.load();
  auto old=std::atomic_load(&published);
  Require(old && next && next->size()==old->size(),"resource/package topology reload requires rebuilt native inputs");
  for(size_t i=0;i<old->size();++i)
    Require(next->at(i).id==old->at(i).id && EiemNativeInput::InputContractDigest(next->at(i))==EiemNativeInput::InputContractDigest(old->at(i)),
            "Mesh/material/texture/Renderer input contract changed; resource reload is not implemented in v26");
  result->controls=RuntimeControls::Prepare(next,config);
  result->config=EiemEntrySettings::Prepare(config);
  if(auto inputs=std::atomic_load(&substitutions)) {
    auto composed=EiemComposedPackage::Compose(*next,inputs->composed);
    Require(!inputs->inputContractDigest.empty() && inputs->inputContractDigest==EiemNativeInput::InputContractDigest(composed),
            "configuration reload requires a matching native input contract");
    auto clone=std::make_shared<EiemNativeInput::InputSubstitutions>(*inputs);
    clone->generation=result->base+1;clone->packageDigest=EiemNativeInput::PackageDigest(composed);result->inputs=clone;
  }
  Require(result->base==generation.load(),"reload generation changed during preparation");
  return result;
}
void Prepare() {
  auto probeStart=ReloadObservation::Clock();
  try {
    Require(RuntimeShapes::enabled,"Unity frame/control module is not armed");
    auto config=EiemEntrySettings::Read(gameDirectory);
    auto compiled=EiemCompiledModSet::Read(gameDirectory);
    auto graph=std::make_shared<const std::vector<Package>>(compiled?compiled->manifest.packages:std::vector<Package>{});
    auto next=Validate(graph,config);
    {std::lock_guard<std::mutex> lock(mutex);pending=std::move(next);}
    Log("[ENTRY-RELOAD] validated publication=pending-Unity-frame resourcesUnchanged=1");
    Observe([&]{ReloadObservation::Prepared(true,probeStart,nullptr);});
  } catch(const std::exception& e) {
    // A later rejected request supersedes an older not-yet-committed candidate.
    {std::lock_guard<std::mutex> lock(mutex);pending.reset();}
    Log("[ENTRY-RELOAD] rejected reason=%s previousGenerationRetained=%u",e.what(),generation.load());
    Observe([&]{ReloadObservation::Prepared(false,probeStart,e.what());});
  }
}
void Commit() {
  std::shared_ptr<Candidate> next;
  {std::lock_guard<std::mutex> lock(mutex);next.swap(pending);}
  if(!next) return;
  auto probeStart=ReloadObservation::Clock();
  try {
    Require(RuntimeShapes::Main() && next->base==generation.load(),"reload is stale or not on Unity thread");
    auto graph=next->controls.graph;auto newGeneration=next->base+1;
    ApplyControls(next->controls);
    std::atomic_store(&substitutions,next->inputs);
    std::atomic_store(&published,graph);
    settings.PublishPrepared(next->config);
    RuntimeShapes::packageGeneration=newGeneration;
    RuntimeShapes::checkedGeneration=newGeneration;
    generation.store(newGeneration);
    // Do not clear cached geometry, active capture groups or their snapshots.
    Log("[ENTRY-RELOAD] committed=%u mods=%zu partIdentities=%zu liveRenderers=%zu captureGroupsRetained=%zu cameraFadeDisabled=%d reloadVk=%u modifiers=%u",
        newGeneration,graph->size(),RuntimeControls::specs.size(),RuntimeShapes::bound.size(),RuntimeShapes::groups.size(),int(next->config->disableCameraFade),next->config->reload.vk,next->config->reload.modifiers);
    Observe([&]{ReloadObservation::Committed(probeStart,true);});
    EntryCameraFade::Report();
    for(const auto& p:*graph) try {RuntimeControls::preferences.Capture(p,RuntimeControls::variables.at(p.id));}
      catch(const std::exception& e) {Log("[CONTROL-STATE-GAP] reloadCapture=%s",e.what());}
  } catch(const std::exception& e) {Log("[ENTRY-RELOAD] commitRejected reason=%s previousGenerationRetained=%u",e.what(),generation.load());Observe([&]{ReloadObservation::Committed(probeStart,false);});}
}
} // namespace EntryReload
void CommitPendingReload() {
  EntryReload::Commit();
  FullResourceReload::Frame();
  ::Entry::EffectDebug::Frame();
  ::Entry::RuntimeAttachments::Frame();
#if defined(EIEM_LOAD_PERF)
  // Read existing main-thread collections only, once per five seconds. No
  // scene queries or additional Unity calls are needed for workload labels.
  if(EiemLoadPerformance::RuntimeSampleDue()){
    using W=EiemLoadPerformance::Workload;using EiemLoadPerformance::Set;
    Set(W::Mods,(RuntimeControls::controlGraph?RuntimeControls::controlGraph->size():0)+RuntimeAttachments::standalone->size());
    Set(W::Parts,FullResourceReload::liveParts.size());Set(W::UIModels,FullResourceReload::liveUIModels.size());Set(W::PendingUI,FullResourceReload::pendingUIModels.size());
    Set(W::ShapeBindings,RuntimeShapes::bound.size());Set(W::ShapeCache,RuntimeShapes::cached.size());Set(W::CaptureGroups,RuntimeShapes::groups.size());Set(W::CaptureSnapshots,RuntimeShapes::liveSnapshots);
    Set(W::MaterialBindings,RuntimeMaterials::bound.size());size_t active=0,channels=0;
    for(const auto& row:RuntimeControls::animations){active+=row.second.ActiveCount();channels+=row.second.output.size();}
    Set(W::ActiveAnimations,active);Set(W::AnimationChannels,channels);Set(W::EffectBindings,RuntimeAttachments::bindings.size());size_t instances=0,quarantined=0;uint64_t archiveBytes=0;
    for(const auto& row:RuntimeAttachments::bindings)instances+=bool(row.second.instance);
    for(const auto& g:EffectDebug::generations){quarantined+=g->quarantined;for(const auto& b:g->manifest->bundles)archiveBytes+=b.Size();}
    for(const auto& instance:EffectDebug::retiredInstances)quarantined+=instance->quarantined;
    Set(W::EffectInstances,instances);Set(W::EffectGenerations,EffectDebug::generations.size());Set(W::EffectRetired,EffectDebug::retiredInstances.size());Set(W::EffectQuarantined,quarantined);Set(W::EffectArchiveBytes,archiveBytes);
    Set(W::RetainedGenerations,FullResourceReload::retained.size());auto routes=std::atomic_load(&FullResourceReload::routes);Set(W::Routes,routes?routes->size():0);
    EiemLoadPerformance::FinishRuntimeSample();
  }
#endif
}

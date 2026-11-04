#pragma once

void CommitPendingReload() {
  DemandGenerationReload::Frame();
  NativeBundle::CollectMemory();
  ::Entry::EffectDebug::Frame();
  ::Entry::RuntimeAttachments::Frame();
#if defined(EIEM_LOAD_PERF)
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

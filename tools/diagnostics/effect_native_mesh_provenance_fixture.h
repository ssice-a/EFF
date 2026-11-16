#pragma once
namespace EffectNativeMeshFixture {
namespace D=Entry::DemandLoadingProbe;
// Starts at the production source-observation callback. The pure/native VFS
// regression verifies the compact descriptor's disk construction separately.
struct Scope {
 std::shared_ptr<const D::InstalledGeneration> previous;
 decltype(D::manifests) previousManifests;
 bool enabled=false;unsigned proofs=0;
 std::shared_ptr<D::InstalledGeneration> generation;
 std::shared_ptr<D::Manifest> manifest;
 Scope():previous(std::atomic_load(&D::installedGeneration)),previousManifests(D::manifests),enabled(D::enabled.load()),proofs(D::nativeProofs.load()){
  generation=std::make_shared<D::InstalledGeneration>();generation->serial=0xB160;
  manifest=std::make_shared<D::Manifest>();manifest->serial=0xB161;manifest->catalog=generation;
  manifest->object=Entry::RuntimeShapes::Ref::Make(Test::dependencyManifest,true);
  D::manifests.clear();D::manifests[Test::dependencyManifest]=manifest;D::enabled=true;D::nativeProofs=128;
  std::atomic_store(&D::installedGeneration,std::shared_ptr<const D::InstalledGeneration>(generation));
 }
 void Add(const std::string& path,const std::vector<EiemDemandMeshProvenance::Use>& uses){
  auto proof=std::make_shared<EiemDemandMeshProvenance::Prefab>();proof->path=path;proof->uses=uses;
  for(const auto& use:uses)generation->observedMeshTargets.insert(use.identities.begin(),use.identities.end());
  manifest->provenancePrefabs[path]=std::move(proof);
 }
 ~Scope(){
  D::manifests=std::move(previousManifests);std::atomic_store(&D::installedGeneration,previous);
  manifest.reset();generation.reset();D::enabled=enabled;D::nativeProofs=proofs;
 }
};
}

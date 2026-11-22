#pragma once

namespace RuntimeBones {
using RuntimeShapes::Call;
using RuntimeShapes::Ref;
using EiemAuthorBoneAnimation::Rotation;
using EiemAuthorBoneAnimation::RotationRetarget;
using EiemAuthorBoneAnimation::TransformSample;
using EiemShapeAnimation::BoneTarget;

struct LiveRotation {
  Ref source;
  RotationRetarget mapping;
  std::optional<Rotation> applied;
};

struct Bone {
  Ref transform;
  std::string package;
  BoneTarget target;
  TransformSample rest;
  std::optional<TransformSample> applied;
  size_t consumers = 0;
  bool failed = false;
  std::optional<LiveRotation> live;
};
std::map<void*,Bone> bones;
std::map<void*,std::vector<void*>> renderers;
size_t liveCount = 0;

struct Methods {
  void *parent = nullptr, *position = nullptr, *rotation = nullptr, *scale = nullptr;
  void *getRotation = nullptr;
};
Methods methods;
void BindMethods() {
  if(methods.parent)return;
  auto klass=RuntimeShapes::api.transformClass;
  Methods next;
  next.parent=Exact(klass,"get_parent",{},"UnityEngine.Transform",false);
  next.position=Exact(klass,"set_localPosition",{"UnityEngine.Vector3"},"System.Void",false);
  next.rotation=Exact(klass,"set_localRotation",{"UnityEngine.Quaternion"},"System.Void",false);
  next.scale=Exact(klass,"set_localScale",{"UnityEngine.Vector3"},"System.Void",false);
  Require(next.parent&&next.position&&next.rotation&&next.scale,"author bone Transform API absent");
  methods=next;
}

void Write(void* transform,const TransformSample& sample) {
  auto values=sample;
  void* args[]={values.data()};
  Call(methods.position,transform,args);
  args[0]=values.data()+3;Call(methods.rotation,transform,args);
  args[0]=values.data()+7;Call(methods.scale,transform,args);
}

void WriteRotation(void* transform, Rotation rotation) {
  void* args[] = {rotation.data()};
  Call(methods.rotation, transform, args);
}

Rotation RestRotation(const Bone& bone) {
  return {bone.rest[3], bone.rest[4], bone.rest[5], bone.rest[6]};
}

bool SameRotation(const Rotation& a, const Rotation& b) {
  const auto sign = a[0]*b[0] + a[1]*b[1] + a[2]*b[2] + a[3]*b[3] < 0 ? -1.f : 1.f;
  for (size_t i = 0; i < a.size(); ++i)
    if (std::abs(a[i] - sign*b[i]) > 1e-6f) return false;
  return true;
}

void* SkeletonAnchor(void* target, const EiemAuthorBoneAnimation::Node& node) {
  size_t levels = node.path.empty() ? 0 : size_t(std::count(node.path.begin(), node.path.end(), '/')) + 1;
  while (levels--) {
    Require(RuntimeShapes::Alive(target), "retarget target ancestry retired: " + node.path);
    target = Call(methods.parent, target);
  }
  Require(RuntimeShapes::Alive(target), "retarget skeleton anchor absent: " + node.path);
  return target;
}

EiemAuthorBoneAnimation::RetargetRoutes ReadRoutes(void* anchor, const RuntimeControls::Spec& spec,
    const EiemAuthorBoneAnimation::Skeleton& skeleton) {
  const auto prefix = EiemAuthorBoneAnimation::RetargetRoutePrefix(EiemComposedPackage::Name(spec.package, spec.skeleton));
  const auto& api = FullResourceReload::api;
  Require(api.childCount && api.getChild, "retarget prefab child APIs absent");
  const auto count = RuntimeShapes::Scalar<int>(api.childCount, anchor);
  std::string encoded;
  for (int i = count; i-- > 0;) {
    void* args[] = {&i};
    auto child = Call(api.getChild, anchor, args);
    if (!RuntimeShapes::Alive(child)) continue;
    auto label = String(Call(RuntimeShapes::api.name, child));
    if (label.compare(0, prefix.size(), prefix) != 0) continue;
    Require(encoded.empty(), "multiple retarget bindings on one skeleton anchor");
    encoded = std::move(label);
  }
  if (encoded.empty()) return {};
  return EiemAuthorBoneAnimation::DecodeRetargetRoutes(std::string_view(encoded).substr(prefix.size()), skeleton);
}

Ref ResolveSource(void* anchor, const EiemAuthorBoneAnimation::LocalBoneRoute& route) {
  const auto& api = FullResourceReload::api;
  auto source = anchor;
  for (auto ordinal : route) {
    Require(ordinal < RuntimeShapes::Scalar<int>(api.childCount, source), "retarget source ordinal absent");
    void* args[] = {&ordinal};
    auto child = Call(api.getChild, source, args);
    Require(RuntimeShapes::Alive(child) && Call(methods.parent, child) == source,
            "retarget source outside its local hierarchy");
    source = child;
  }
  return Ref::Make(source, false);
}

void Forget(void* renderer) noexcept {
  auto found=renderers.find(renderer);
  if(found==renderers.end())return;
  for(auto key:found->second){
    auto bone=bones.find(key);
    if(--bone->second.consumers)continue;
    try{
      auto transform=bone->second.transform.Get();
      if(bone->second.live && bone->second.live->applied && RuntimeShapes::Alive(transform))
        WriteRotation(transform, RestRotation(bone->second));
      else if(bone->second.applied&&RuntimeShapes::Alive(transform))Write(transform,bone->second.rest);
    }catch(const std::exception& error){
      Log("[BONE-ANIMATION-RELEASE-GAP] reason=%s",error.what());
    }
    if (bone->second.live) --liveCount;
    bones.erase(bone);
  }
  renderers.erase(found);
}

void BindPalette(void* renderer,const RuntimeControls::Spec& spec,
                 const EiemAuthorBoneAnimation::Skeleton& skeleton,const std::vector<std::string>& palette,
                 const std::set<std::string>& animatedPaths) {
    auto used=EiemAuthorBoneAnimation::AuthorBoneClosure(skeleton,{palette.begin(),palette.end()});
    std::map<std::string,size_t> indices;
    const auto driven = [&](size_t i) {
      return skeleton.retargets.count(i) || animatedPaths.count(skeleton.nodes[i].path);
    };
    bool hasAnimatedBone=false;
    for(size_t i=0;i<used.size();++i)if(used[i]){
      indices.emplace(skeleton.nodes[i].path,i);
      hasAnimatedBone=hasAnimatedBone||driven(i);
    }
    if(!hasAnimatedBone)return;
    BindMethods();
    auto array=Ref::Make(Call(RuntimeShapes::api.bones,renderer),false);
    auto slots=FullResourceReload::ReferenceArray(array.Get(),palette.size());
    std::map<size_t,Ref> targets;
    for(size_t slot=0;slot<palette.size();++slot){
      auto index=indices.find(palette[slot]);
      if(index==indices.end())continue;
      auto transform=slots[slot];
      for(auto i=int32_t(index->second);i>=0;i=skeleton.nodes[size_t(i)].parent){
        const auto& node=skeleton.nodes[size_t(i)];
        Require(RuntimeShapes::Alive(transform),"author bone instance retired: "+node.path);
        if(!node.source&&!node.path.empty())
          Require(String(Call(RuntimeShapes::api.name,transform))==node.path.substr(node.path.find_last_of('/')+1),
                  "author bone instance path differs: "+node.path);
        if(node.source)break;
        auto existing=targets.find(size_t(i));
        if(existing!=targets.end()){
          Require(existing->second.Get()==transform,"author bone slot shares a different parent: "+node.path);
          break;
        }
        if(driven(size_t(i)))targets.emplace(size_t(i),Ref::Make(transform,false));
        if(node.parent>=0)transform=Call(methods.parent,transform);
      }
    }
    auto& keys=renderers[renderer];keys.reserve(targets.size());
    std::map<void*,EiemAuthorBoneAnimation::RetargetRoutes> routes;
    for(auto& entry:targets){
      const auto& node=skeleton.nodes[entry.first];
      auto transform=entry.second.Get();
      BoneTarget target{spec.skeleton,node.path};
      auto found=bones.find(transform);
      if(found==bones.end()){
        std::optional<LiveRotation> live;
        auto mapping = skeleton.retargets.find(entry.first);
        if (mapping != skeleton.retargets.end()) {
          try {
            if (!methods.getRotation)
              methods.getRotation = Exact(RuntimeShapes::api.transformClass, "get_localRotation", {},
                                          "UnityEngine.Quaternion", false);
            Require(methods.getRotation, "retarget local rotation API absent");
            auto anchor = SkeletonAnchor(transform, node);
            auto foundRoutes = routes.find(anchor);
            if(foundRoutes==routes.end())foundRoutes=routes.emplace(anchor,ReadRoutes(anchor,spec,skeleton)).first;
            auto route = foundRoutes->second.find(entry.first);
            Require(route!=foundRoutes->second.end(), "no unique native Mesh palette source in the assembled prefab");
            live.emplace(LiveRotation{ResolveSource(anchor,route->second),mapping->second,{}});
          } catch (const std::exception& error) {
            Log("[BONE-RETARGET-BIND-GAP] package=%s part=%s target=%s reason=%s",
                spec.package.c_str(), spec.part.c_str(), node.path.c_str(), error.what());
            continue;
          }
        }
        TransformSample rest;
        std::copy(node.position.begin(),node.position.end(),rest.begin());
        std::copy(node.rotation.begin(),node.rotation.end(),rest.begin()+3);
        std::copy(node.scale.begin(),node.scale.end(),rest.begin()+7);
        found=bones.emplace(transform,Bone{std::move(entry.second),spec.package,std::move(target),rest,
                                          {},0,false,std::move(live)}).first;
        if (found->second.live) ++liveCount;
      }else Require(found->second.package==spec.package&&found->second.target==target,
                    "author bone instance has competing Mod or Skeleton ownership: "+node.path);
      keys.push_back(transform);
      ++found->second.consumers;
    }
}

void Register(void* renderer,const RuntimeControls::Spec& spec) {
  if(renderers.count(renderer))return;
  try{
    if(!RuntimeShapes::Alive(renderer)||RuntimeShapes::RendererKind(renderer)!=137)return;
    auto resources=DemandLoadingProbe::CurrentInstalledGeneration();
    if(!resources)return;
    auto author=resources->authors.find(spec.package);
    if(author==resources->authors.end())return;
    auto& catalog=*author->second->catalog;
    auto skeleton=catalog.GetSkeleton(spec.skeleton);
    const std::set<std::string> noPaths;
    const auto* paths = &noPaths;
    auto player = RuntimeControls::animations.find(spec.package);
    if (player != RuntimeControls::animations.end()) {
      auto found = player->second.bonePaths.find(spec.skeleton);
      if (found != player->second.bonePaths.end()) paths = &found->second;
    }
    if (paths->empty() && skeleton->retargets.empty()) return;
    if(std::none_of(skeleton->nodes.begin(),skeleton->nodes.end(),[](const auto& node){return !node.source;}))return;
    BindPalette(renderer,spec,*skeleton,catalog.BonePalette(spec.part),*paths);
  }catch(const std::exception& error){
    Forget(renderer);
    Log("[BONE-ANIMATION-BIND-GAP] package=%s part=%s reason=%s",spec.package.c_str(),spec.part.c_str(),error.what());
  }
}

void RefreshControls(const std::set<std::string>& packages) {
  if(packages.empty())return;
  for(auto& row:RuntimeShapes::bound){
    auto& binding=row.second;
    if(!binding.modelReady)continue;
    auto spec=RuntimeControls::specs.find(binding.marker);
    if(spec==RuntimeControls::specs.end()||spec->second.skeleton.empty()||!packages.count(spec->second.package))continue;
    auto renderer=binding.renderer.Get();
    if(renderer!=row.first)continue;
    Forget(renderer);
    Register(renderer,spec->second);
  }
}

void ApplyLiveRotation(Bone& bone) {
  auto& live = *bone.live;
  auto source = live.source.Get();
  if (!RuntimeShapes::Alive(source)) {
    auto transform = bone.transform.Get();
    if (live.applied && RuntimeShapes::Alive(transform)) WriteRotation(transform, RestRotation(bone));
    live.applied.reset();
    bone.failed = true;
    return;
  }
  const auto current = RuntimeShapes::Scalar<Rotation>(methods.getRotation, source);
  const auto rotation = EiemAuthorBoneAnimation::RetargetRotation(live.mapping, current);
  if (SameRotation(rotation, live.applied.value_or(RestRotation(bone)))) return;
  auto transform = bone.transform.Get();
  if (!RuntimeShapes::Alive(transform)) {
    live.applied.reset();
    bone.failed = true;
    return;
  }
  const bool firstWrite = !live.applied;
  live.applied = rotation;
  WriteRotation(transform, rotation);
  if (firstWrite)
    Log("[BONE-RETARGET-ACTIVE] package=%s target=%s", bone.package.c_str(), bone.target.second.c_str());
}

void FrameRetargets() {
  // Update can already have run from the model tick. The source must be read
  // after the game's LateUpdate even when the shared clip clock has advanced.
  if (!liveCount || !RuntimeShapes::enabled || !RuntimeShapes::Main()) return;
  static int lastFrame = -1;
  if (lastFrame == RuntimeShapes::lastFrame) return;
  lastFrame = RuntimeShapes::lastFrame;
  for (auto& [transform, bone] : bones) {
    if (!bone.live || bone.failed) continue;
    try { ApplyLiveRotation(bone); }
    catch (const std::exception& error) {
      bone.failed = true;
      Log("[BONE-RETARGET-WRITE-GAP] package=%s target=%s reason=%s",
          bone.package.c_str(), bone.target.second.c_str(), error.what());
    }
  }
}

void Frame() {
  for(auto& row:bones){
    auto& bone=row.second;
    if(bone.failed||bone.live)continue;
    const TransformSample* sample=nullptr;
    auto player=RuntimeControls::animations.find(bone.package);
    if(player!=RuntimeControls::animations.end()){
      auto output=player->second.boneOutput.find(bone.target);
      if(output!=player->second.boneOutput.end())sample=&output->second;
    }
    if(sample&&bone.applied&&*sample==*bone.applied)continue;
    if(!sample&&!bone.applied)continue;
    try{
      auto transform=bone.transform.Get();
      if(!RuntimeShapes::Alive(transform)){bone.applied.reset();bone.failed=true;continue;}
      if(sample){
        bone.applied=*sample;
        Write(transform,*sample);
      }else{
        Write(transform,bone.rest);
        bone.applied.reset();
      }
    }catch(const std::exception& error){
      bone.failed=true;
      Log("[BONE-ANIMATION-WRITE-GAP] package=%s path=%s reason=%s",
          bone.package.c_str(),bone.target.second.c_str(),error.what());
    }
  }
}
} // namespace RuntimeBones

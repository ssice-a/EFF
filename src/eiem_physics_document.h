#pragma once
#include <cmath>
#include <cstdint>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <memory>

struct EiemPhysicsNativeValue;
struct EiemPhysicsNativeComponent;

// Authoring interchange only. No native factory, selection index or Animator
// buffer mapping is implied by a successfully decoded document.
struct EiemPhysicsAuthorNode { std::string bone; uint8_t role = 0; };
struct EiemPhysicsAuthorCurveKey {
  float time=0,value=1,inSlope=0,outSlope=0;
  uint32_t weightedMode=0;
  float inWeight=1.0f/3.0f,outWeight=1.0f/3.0f;
};
struct EiemPhysicsAuthorRadius {
  float value=.006f;
  uint8_t useCurve=1;
  std::vector<EiemPhysicsAuthorCurveKey> keys={
    {0,1,0,0,0,1.0f/3.0f,1.0f/3.0f},
    {1,1,0,0,0,1.0f/3.0f,1.0f/3.0f}};
  int32_t preInfinity=2,postInfinity=2,rotationOrder=4;
};
struct EiemPhysicsAuthorParameter {
  std::string path;
  uint8_t floating=0;
  float floatingValue=0;
  int32_t integerValue=0;
};
struct EiemPhysicsAuthorCollider {
  std::string id, name, bone;
  uint8_t shape = 0;
  float position[3] = {}, rotation[4] = {0,0,0,1};
  float radius = 0, endRadius = 0, span = 0; // native length = span + both radii
  uint8_t alignedOnCenter = 1;
};
struct EiemPhysicsAuthorGroup {
  std::string id, name;
  std::vector<EiemPhysicsAuthorNode> nodes;
  // gravity, stablizationTimeAfterReset, gravityFalloff, blendWeight, animationPoseRatio
  float parameters[5] = {};
  // Author format v5. Native source graph v2 is a separate resource kind.
  EiemPhysicsAuthorRadius radius;
  // Primitive ClothSerializeData edits. Topology and runtime-result fields
  // are forbidden by the wire validator.
  std::vector<EiemPhysicsAuthorParameter> nativeParameters;
  std::vector<std::string> colliders;
};
struct EiemPhysicsDocument {
  uint32_t version=5;
  std::string id, skeleton;
  std::vector<EiemPhysicsAuthorCollider> colliders;
  std::vector<EiemPhysicsAuthorGroup> groups;
  std::shared_ptr<const EiemPhysicsNativeValue> native;
  std::vector<EiemPhysicsNativeComponent> nativeComponents;
};

static bool EiemPhysicsAuthorId(const std::string &s) {
  if (s.size()!=32) return false;
  for (char c:s) if (!(c>='0'&&c<='9') && !(c>='a'&&c<='f')) return false;
  return true;
}
static bool EiemPhysicsAuthorPath(const std::string &s, bool empty=false) {
  if (s.empty()) return empty;
  if (s.size()>4096 || s.find_first_of("\\:")!=std::string::npos || s.find('\0')!=std::string::npos) return false;
  size_t start=0;
  for (;;) {
    const auto slash=s.find('/',start);
    const auto part=s.substr(start,slash-start);
    if (part.empty() || part=="." || part=="..") return false;
    if (slash==std::string::npos) return true;
    start=slash+1;
  }
}
static bool EiemPhysicsNativeParameterPath(const std::string &s) {
  static const std::set<std::string> forbidden={
    "sourceRenderers","paintMaps","rootBones","ignoreFromRootBones",
    "colliderList","verificationResult"};
  constexpr const char prefix[]="serializeData.";
  if (s.size()<=sizeof(prefix)-1 || s.size()>4096 || s.compare(0,sizeof(prefix)-1,prefix)) return false;
  size_t start=sizeof(prefix)-1; bool first=true;
  while (start<s.size()) {
    const auto end=s.find('.',start);
    const auto part=s.substr(start,end-start);
    if (part.empty()) return false;
    bool numeric=true;
    for (char c:part) if (c<'0'||c>'9') {numeric=false;break;}
    if (!numeric) {
      if (!((part[0]>='A'&&part[0]<='Z')||(part[0]>='a'&&part[0]<='z')||part[0]=='_')) return false;
      for (char c:part) if (!((c>='A'&&c<='Z')||(c>='a'&&c<='z')||
                              (c>='0'&&c<='9')||c=='_')) return false;
    }
    if (first && (numeric || forbidden.count(part))) return false;
    if (part=="m_PathID" || part=="arrayBytes") return false;
    first=false;
    if (end==std::string::npos) return true;
    start=end+1;
  }
  return false;
}
static bool EiemValidatePhysicsAuthor(const EiemPhysicsDocument &d, std::string &error) {
  auto fail=[&](const char *why) { error=why; return false; };
  if (d.version!=5 || !EiemPhysicsAuthorId(d.id) || !EiemPhysicsAuthorPath(d.skeleton) ||
      d.skeleton.size()<9 || d.skeleton.substr(d.skeleton.size()-9)!=".skeleton" ||
      d.groups.empty() || d.groups.size()>1024 || d.colliders.size()>4096)
    return fail("Invalid Physics authoring header/dependencies");
  std::set<std::string> ids, colliders, used;
  auto unique=[&](const std::string &id,const std::string &name) {
    return EiemPhysicsAuthorId(id) && ids.insert(id).second && !name.empty() && name.size()<=4096 && name.find('\0')==std::string::npos;
  };
  for (const auto &c:d.colliders) {
    if (!unique(c.id,c.name) || !EiemPhysicsAuthorPath(c.bone,true) || c.shape>1 ||
        !std::isfinite(c.radius) || c.radius<=0 ||
        (!std::isfinite(c.endRadius) || c.endRadius<=0) ||
        !std::isfinite(c.span) || c.span<0 || c.alignedOnCenter>1 ||
        (c.shape==0 && (c.span!=0 || c.endRadius!=c.radius)) ||
        (c.shape==1 && c.alignedOnCenter &&
         c.span<std::abs(c.radius-c.endRadius)))
      return fail("Invalid Physics collider");
    for (float f:c.position) if (!std::isfinite(f)) return fail("Non-finite Physics position");
    double norm=0;
    for (float f:c.rotation) { if (!std::isfinite(f)) return fail("Non-finite Physics rotation"); norm+=double(f)*f; }
    if (std::abs(norm-1)>0.001) return fail("Unnormalized Physics rotation");
    colliders.insert(c.id);
  }
  for (const auto &g:d.groups) {
    if (!unique(g.id,g.name) || g.nodes.size()<2 || g.nodes.size()>16384 || g.colliders.size()>4096)
      return fail("Invalid Physics group");
    std::set<std::string> nodes, refs;
    bool moving=false; size_t roots=0;
    for (const auto &n:g.nodes) {
      if (!EiemPhysicsAuthorPath(n.bone,true) || n.role>2 || !nodes.insert(n.bone).second)
        return fail("Invalid Physics node");
      moving=moving || n.role==1;
    }
    for (const auto &n:g.nodes) {
      const auto slash=n.bone.find_last_of('/');
      const std::string parent=slash==std::string::npos ? "" : n.bone.substr(0,slash);
      if (n.bone.empty() || !nodes.count(parent)) {
        ++roots;
        if (n.role!=0) return fail("Physics root must be fixed");
      } else if (n.role==0) return fail("Only Physics roots can be fixed");
    }
    if (!roots || !moving) return fail("Physics group needs fixed roots and moving nodes");
    for (size_t i=0;i<5;++i)
      if (!std::isfinite(g.parameters[i]) || g.parameters[i]<0 || (i>=2 && g.parameters[i]>1))
        return fail("Invalid Physics scalar");

    if (!std::isfinite(g.radius.value) || g.radius.value<=0 || g.radius.useCurve>1 ||
        g.radius.keys.size()<2 || g.radius.keys.size()>64 ||
        g.radius.rotationOrder<0 || g.radius.rotationOrder>5)
      return fail("Invalid Physics node radius");
    float previous=-1;
    for (const auto &key:g.radius.keys) {
      if (!std::isfinite(key.time) || !std::isfinite(key.value) ||
          !std::isfinite(key.inSlope) || !std::isfinite(key.outSlope) ||
          !std::isfinite(key.inWeight) || !std::isfinite(key.outWeight) ||
          key.time<0 || key.time>1 || key.time<=previous || key.value<0 ||
          key.weightedMode>3 || key.inWeight<0 || key.inWeight>1 ||
          key.outWeight<0 || key.outWeight>1)
        return fail("Invalid Physics node radius curve");
      previous=key.time;
    }

    if (g.nativeParameters.size()>4096) return fail("Invalid native Physics parameter count");
    std::set<std::string> paths;
    for (const auto &parameter:g.nativeParameters) {
      if (!EiemPhysicsNativeParameterPath(parameter.path) || parameter.floating>1 ||
          !paths.insert(parameter.path).second ||
          (parameter.floating && !std::isfinite(parameter.floatingValue)) ||
          (!parameter.floating && (parameter.integerValue<INT32_MIN || parameter.integerValue>INT32_MAX)))
        return fail("Invalid native Physics parameter");
    }

    for (const auto &ref:g.colliders) {
      if (!colliders.count(ref) || !refs.insert(ref).second) return fail("Invalid Physics collider reference");
      used.insert(ref);
    }
  }
  if (used!=colliders) return fail("Unreferenced Physics collider");
  error.clear(); return true;
}

static bool EiemPhysicsAuthorUtf8(const std::string &s) {
  for (size_t i=0;i<s.size();) {
    const auto c=uint8_t(s[i++]);
    if (c<0x80) { if (!c) return false; continue; }
    uint32_t value=0, minimum=0; size_t count=0;
    if (c>=0xc2 && c<=0xdf) {value=c&31; count=1; minimum=0x80;}
    else if (c>=0xe0 && c<=0xef) {value=c&15; count=2; minimum=0x800;}
    else if (c>=0xf0 && c<=0xf4) {value=c&7; count=3; minimum=0x10000;}
    else return false;
    if (i+count>s.size()) return false;
    while (count--) {const auto next=uint8_t(s[i++]); if ((next&0xc0)!=0x80) return false; value=(value<<6)|(next&63);}
    if (value<minimum || value>0x10ffff || (value>=0xd800 && value<=0xdfff)) return false;
  }
  return true;
}

#include "eiem_physics_native_document.h"

template<typename Reader> struct EiemPhysicsAuthorReader {
  Reader &source; size_t remaining=16*1024*1024;
  bool Bytes(void *out,size_t size) {
    if (size>remaining) return false;
    remaining-=size; return source.Bytes(out,size);
  }
  template<typename T> bool Value(T &out) { return Bytes(&out,sizeof(out)); }
  bool Count(uint32_t &out,uint32_t maximum) { return Value(out) && out<=maximum; }
  bool String(std::string &out) {
    uint32_t count=0;
    if (!Count(count,4096)) return false;
    out.resize(count);
    return (!count || Bytes(out.data(),count)) && EiemPhysicsAuthorUtf8(out);
  }
};

template<typename Reader>
static bool EiemReadPhysicsAuthor(Reader &source,EiemPhysicsDocument &out,std::string &error) {
  EiemPhysicsAuthorReader<Reader> r{source}; EiemPhysicsDocument next;
  auto invalid=[&] { error="Invalid, unsupported or truncated Physics authoring resource"; return false; };
  char magic[8]; uint32_t version=0,count=0; std::string purpose,coordinate,backend;
  if (!r.Bytes(magic,8) || std::string(magic,8)!=std::string("EFFPHYS\0",8) || !r.Value(version)) return invalid();
  if (version==2) {
    auto tree=std::make_shared<EiemPhysicsNativeValue>();
    if (!EiemReadPhysicsNativeValue(r,*tree) || !source.End()) return invalid();
    if (!EiemValidatePhysicsNative(*tree,next.id,next.skeleton,next.nativeComponents,error)) return false;
    next.version=2; next.native=std::move(tree); out=std::move(next); error.clear(); return true;
  }
  if (version!=5 || !r.String(purpose) || purpose!="authoring" || !r.String(coordinate) || coordinate!="unity-y-up-left-handed" ||
      !r.String(backend) || backend!="BeyondDynamicBone" || !r.String(next.id) || !r.String(next.skeleton) ||
      !r.Count(count,4096)) return invalid();
  next.version=version; next.colliders.resize(count);
  for (auto &c:next.colliders) {
    if (!r.String(c.id) || !r.String(c.name) || !r.String(c.bone) || !r.Value(c.shape) ||
        !r.Bytes(c.position,sizeof(c.position)) || !r.Bytes(c.rotation,sizeof(c.rotation)) ||
        !r.Value(c.radius) || !r.Value(c.endRadius) ||
        !r.Value(c.span) || !r.Value(c.alignedOnCenter)) return invalid();
  }
  if (!r.Count(count,1024)) return invalid();
  next.groups.resize(count);
  for (auto &g:next.groups) {
    if (!r.String(g.id) || !r.String(g.name) || !r.Count(count,16384)) return invalid();
    g.nodes.resize(count);
    for (auto &n:g.nodes) if (!r.String(n.bone) || !r.Value(n.role)) return invalid();
    if (!r.Bytes(g.parameters,sizeof(g.parameters))) return invalid();

    if (!r.Value(g.radius.value) || !r.Value(g.radius.useCurve) || !r.Count(count,64)) return invalid();
    g.radius.keys.resize(count);
    for (auto &key:g.radius.keys)
      if (!r.Value(key.time) || !r.Value(key.value) || !r.Value(key.inSlope) ||
          !r.Value(key.outSlope) || !r.Value(key.weightedMode) ||
          !r.Value(key.inWeight) || !r.Value(key.outWeight)) return invalid();
    if (!r.Value(g.radius.preInfinity) || !r.Value(g.radius.postInfinity) ||
        !r.Value(g.radius.rotationOrder)) return invalid();

    if (!r.Count(count,4096)) return invalid();
    g.nativeParameters.resize(count);
    for (auto &parameter:g.nativeParameters) {
      if (!r.String(parameter.path) || !r.Value(parameter.floating) || parameter.floating>1) return invalid();
      if (parameter.floating) {
        if (!r.Value(parameter.floatingValue)) return invalid();
      } else if (!r.Value(parameter.integerValue)) return invalid();
    }

    if (!r.Count(count,4096)) return invalid();
    g.colliders.resize(count);
    for (auto &ref:g.colliders) if (!r.String(ref)) return invalid();
  }
  if (!source.End()) return invalid();
  if (!EiemValidatePhysicsAuthor(next,error)) return false;
  out=std::move(next); error.clear(); return true;
}

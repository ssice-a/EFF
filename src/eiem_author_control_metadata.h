#pragma once
#include "eiem_resource_directory.h"
#include <cfloat>

// Author declarations only. Native effect archives stay in separate lazy files;
// sampled animation tracks are already encoded before the game starts.
namespace EiemAuthorControlMetadata {
using namespace EiemDirectory;
struct PartState {std::shared_ptr<EiemExpression> visible;std::map<std::string,std::string> shapes;};
struct State {
  EiemVariables defaults;
  std::set<std::string> persistent;
  std::map<std::string,Control> controls;
  std::vector<Binding> bindings;
  std::map<std::string,PartState> parts;
  std::map<std::string,ShapeAnimation> animations;
  std::set<std::string> materialAnimations;
  std::map<std::string,MaterialTrack> materialTracks;
  std::map<std::string,EffectAttachment> effects;
  bool keySwitchEnabled=true;
};
inline State Capture(const Package& p){
  State s;s.defaults=p.defaults;s.persistent=p.persistent;s.controls=p.controls;s.bindings=p.bindings;
  s.animations=p.shapeAnimations;s.materialAnimations=p.materialAnimationIds;s.materialTracks=p.materialTracks;s.effects=p.effects;s.keySwitchEnabled=p.keySwitchEnabled;
  for(const auto& row:p.parts)s.parts.emplace(row.first,PartState{row.second.visible,row.second.shapes});return s;
}
inline void Apply(const State& s,Package& p){
  p.defaults=s.defaults;p.persistent=s.persistent;p.controls=s.controls;p.bindings=s.bindings;
  p.shapeAnimations=s.animations;p.materialAnimationIds=s.materialAnimations;p.materialTracks=s.materialTracks;p.effects=s.effects;p.keySwitchEnabled=s.keySwitchEnabled;
  for(auto& row:p.parts){auto found=s.parts.find(row.first);
    if(found==s.parts.end()){row.second.visible=EiemExpression::Make();row.second.visible->number=1;}
    else{row.second.visible=found->second.visible;row.second.shapes=found->second.shapes;}}
}
inline void Validate(const State& s){
  for(const auto& value:s.defaults)Require(EiemVariableName(value.first)&&std::isfinite(value.second),"author control default differs");
  for(const auto& name:s.persistent)Require(s.defaults.count(name),"author persistent variable absent");
  for(const auto& row:s.controls){const auto& c=row.second;Require(s.defaults.count(row.first)&&std::isfinite(c.minimum)&&std::isfinite(c.maximum)&&c.minimum<=c.maximum&&s.defaults.at(row.first)>=c.minimum&&s.defaults.at(row.first)<=c.maximum,"author control domain differs");}
  std::set<std::string> keys;
  for(const auto& b:s.bindings){Require(!b.id.empty()&&keys.insert(b.id).second&&s.defaults.count(b.variable)&&b.key.vk&&b.key.vk<=255&&!(b.key.modifiers&~(MOD_SHIFT|MOD_ALT|MOD_CONTROL))&&std::isfinite(b.speed)&&b.speed>0&&!b.values.empty()&&(b.type=="cycle"||b.type=="press"||b.type=="hold"),"author key binding differs");
    for(auto value:b.values)Require(std::isfinite(value),"author key value nonfinite");}
  for(const auto& row:s.parts){std::string error;Require(!row.first.empty()&&row.second.visible,"author Part visibility absent");
    std::map<const EiemExpression*,unsigned char> seen;std::vector<std::pair<const EiemExpression*,bool>> pending{{row.second.visible.get(),false}};
    while(!pending.empty()){auto next=pending.back();pending.pop_back();auto node=next.first;if(next.second){seen[node]=2;continue;}Require(seen[node]!=1,"author visibility expression cycle");if(seen[node]==2)continue;seen[node]=1;
      auto op=node->op;bool leaf=op==EiemExpression::Number||op==EiemExpression::Variable,unary=op==EiemExpression::Not||op==EiemExpression::Negative;Require(op<=EiemExpression::Or&&std::isfinite(node->number)&&(leaf?(!node->left&&!node->right):(unary?(bool(node->left)&&!node->right):(bool(node->left)&&bool(node->right)))),"author visibility expression arity/opcode differs");
      if(op==EiemExpression::Variable)Require(s.defaults.count(node->variable),"author visibility variable absent");pending.push_back({node,true});if(node->right)pending.push_back({node->right.get(),false});if(node->left)pending.push_back({node->left.get(),false});}
    for(const auto& shape:row.second.shapes)Require(!shape.first.empty()&&s.controls.count(shape.second),"author Part shape variable absent");}
  Package p;p.defaults=s.defaults;p.persistent=s.persistent;p.controls=s.controls;p.shapeAnimations=s.animations;
  for(const auto& row:s.animations){const auto& a=row.second;Require(row.first==a.id&&!a.id.empty()&&std::isfinite(a.duration)&&a.duration>0&&std::isfinite(a.speed)&&a.speed>0&&!a.tracks.empty()&&a.key.vk<=255&&!(a.key.modifiers&~(MOD_SHIFT|MOD_ALT|MOD_CONTROL)),"author sampled animation differs");
    for(const auto& track:a.tracks){Require(track.second.size()>=2,"author sampled animation lacks samples");for(auto value:track.second)Require(std::isfinite(value),"author sampled animation value nonfinite");}}
  ValidateShapeAnimations(p);
  for(const auto& id:s.materialAnimations)Require(s.animations.count(id),"author material animation absent");
  std::set<std::tuple<std::string,std::string,unsigned>> channels;
  for(const auto& row:s.materialTracks){const auto& t=row.second;Require(s.controls.count(row.first)&&!t.material.empty()&&!t.property.empty()&&(t.kind=="FLOAT"||t.kind=="COLOR"||t.kind=="VECTOR")&&t.component<(t.kind=="FLOAT"?1u:4u)&&channels.emplace(t.material,t.property,t.component).second,"author material channel differs");}
  for(const auto& row:s.effects){const auto& e=row.second;Require(row.first==e.id&&!e.id.empty()&&s.controls.count(e.variable)&&s.controls.at(e.variable).minimum==0&&s.controls.at(e.variable).maximum==1&&(s.defaults.at(e.variable)==0||s.defaults.at(e.variable)==1)&&e.native&&(!e.part.empty()||!e.targetPath.empty()),"author effect attachment differs");
    for(auto v:e.position)Require(std::isfinite(v),"author effect position nonfinite");for(auto v:e.scale)Require(std::isfinite(v),"author effect scale nonfinite");double norm=0;for(auto v:e.rotation){Require(std::isfinite(v),"author effect rotation nonfinite");norm+=double(v)*v;}Require(std::abs(norm-1)<1e-3,"author effect rotation not normalized");
    const auto& m=*e.native;Require(m.key.size()==64&&m.key.find_first_not_of("0123456789ABCDEF")==std::string::npos&&m.prefab.rfind("assets/",0)==0&&!m.rootName.empty()&&m.root<m.bundles.size(),"author effect native manifest differs");
    std::vector<unsigned char> visit(m.bundles.size());std::function<void(size_t)> walk=[&](size_t i){Require(visit.at(i)!=1,"author effect dependency cycle");if(visit[i]==2)return;visit[i]=1;
      const auto& b=m.bundles[i];Require(!b.file.empty()&&b.Size()>=48&&b.hash.size()==64&&b.hash.find_first_not_of("0123456789ABCDEF")==std::string::npos,"author effect native input differs");std::set<size_t> edges;
      for(auto d:b.dependencies){Require(d<m.bundles.size()&&d!=i&&edges.insert(d).second,"author effect dependency differs");walk(d);}visit[i]=2;};
    for(size_t i=0;i<m.bundles.size();++i)walk(i);
  }
}
template<class W>void Write(W& w,const State& s){
  Validate(s);w.N(s.keySwitchEnabled);std::map<std::string,double> defaults(s.defaults.begin(),s.defaults.end());w.N(defaults.size());for(const auto& row:defaults){w.S(row.first);w.D(row.second);}
  w.Strings({s.persistent.begin(),s.persistent.end()});w.N(s.controls.size());for(const auto& row:s.controls){w.S(row.first);w.S(row.second.label);w.D(row.second.minimum);w.D(row.second.maximum);}
  w.N(s.bindings.size());for(const auto& b:s.bindings){w.S(b.id);w.S(b.variable);w.S(b.type);w.N(b.key.vk);w.N(b.key.modifiers);w.D(b.speed);w.S(b.label);w.N(b.values.size());for(auto v:b.values)w.D(v);}
  w.N(s.parts.size());for(const auto& row:s.parts){w.S(row.first);w.Expr(row.second.visible);w.N(row.second.shapes.size());for(const auto& v:row.second.shapes){w.S(v.first);w.S(v.second);}}
  w.N(s.animations.size());for(const auto& row:s.animations){const auto& a=row.second;w.S(a.id);w.S(a.label);w.S(a.speedVariable);w.N(a.key.vk);w.N(a.key.modifiers);w.D(a.duration);w.D(a.speed);w.N(a.loop);w.N(a.autoplay);w.N(a.tracks.size());for(const auto& t:a.tracks){w.S(t.first);w.N(t.second.size());for(auto v:t.second)w.D(v);}}
  w.Strings({s.materialAnimations.begin(),s.materialAnimations.end()});w.N(s.materialTracks.size());for(const auto& row:s.materialTracks){const auto& t=row.second;w.S(row.first);w.S(t.material);w.S(t.property);w.S(t.kind);w.N(t.component);}
  w.N(s.effects.size());for(const auto& row:s.effects){const auto& e=row.second;w.S(e.id);w.S(e.label);w.S(e.part);w.S(e.bone);w.S(e.variable);w.N(e.autoplay);for(auto v:e.position)w.D(v);for(auto v:e.rotation)w.D(v);for(auto v:e.scale)w.D(v);w.S(e.targetPrefab);w.S(e.rendererPath);w.S(e.targetPath);w.S(e.targetAsset);w.Strings(e.targetBones);
    const auto& m=*e.native;w.S(m.key);w.S(m.prefab);w.S(m.rootName);w.N(m.root);w.N(m.particles);w.N(m.bundles.size());for(const auto& b:m.bundles){w.S(b.file);w.S(b.hash);w.N(b.Size());w.N(b.dependencies.size());for(auto d:b.dependencies)w.N(d);}}
}
template<class R>State Read(R& r){
  State s;s.keySwitchEnabled=r.B();auto n=r.Count();for(size_t i=0;i<n;++i){auto name=r.S();auto value=r.D();Require(s.defaults.emplace(name,value).second,"duplicate author default");}
  auto set=[&](auto& values){for(auto name:r.Strings())Require(values.insert(std::move(name)).second,"duplicate author control set entry");};set(s.persistent);
  n=r.Count();for(size_t i=0;i<n;++i){auto name=r.S();Control c;c.label=r.S();c.minimum=r.D();c.maximum=r.D();Require(s.controls.emplace(name,std::move(c)).second,"duplicate author control");}
  n=r.Count();for(size_t i=0;i<n;++i){Binding b;b.id=r.S();b.variable=r.S();b.type=r.S();b.key.vk=r.U();b.key.modifiers=r.U();b.speed=r.D();b.label=r.S();auto count=r.Count();for(size_t j=0;j<count;++j)b.values.push_back(r.D());s.bindings.push_back(std::move(b));}
  n=r.Count();for(size_t i=0;i<n;++i){auto name=r.S();PartState p;p.visible=r.Expr();auto count=r.Count();for(size_t j=0;j<count;++j){auto shape=r.S(),variable=r.S();Require(p.shapes.emplace(shape,variable).second,"duplicate author shape binding");}Require(s.parts.emplace(name,std::move(p)).second,"duplicate author Part controls");}
  n=r.Count();for(size_t i=0;i<n;++i){ShapeAnimation a;a.id=r.S();a.label=r.S();a.speedVariable=r.S();a.key.vk=r.U();a.key.modifiers=r.U();a.duration=r.D();a.speed=r.D();a.loop=r.B();a.autoplay=r.B();auto count=r.Count();for(size_t j=0;j<count;++j){auto variable=r.S();std::vector<float> values;auto samples=r.Count();for(size_t k=0;k<samples;++k){auto v=r.D();Require(std::abs(v)<=FLT_MAX,"author sampled value exceeds float");values.push_back(float(v));}Require(a.tracks.emplace(variable,std::move(values)).second,"duplicate author animation channel");}Require(s.animations.emplace(a.id,std::move(a)).second,"duplicate author animation");}
  set(s.materialAnimations);n=r.Count();for(size_t i=0;i<n;++i){auto variable=r.S();MaterialTrack t;t.material=r.S();t.property=r.S();t.kind=r.S();t.component=r.U();Require(s.materialTracks.emplace(variable,std::move(t)).second,"duplicate author material channel");}
  n=r.Count();for(size_t i=0;i<n;++i){EffectAttachment e;e.id=r.S();e.label=r.S();e.part=r.S();e.bone=r.S();e.variable=r.S();e.autoplay=r.B();auto scalar=[&](){auto v=r.D();Require(std::abs(v)<=FLT_MAX,"author effect transform exceeds float");return float(v);};for(auto& v:e.position)v=scalar();for(auto& v:e.rotation)v=scalar();for(auto& v:e.scale)v=scalar();e.targetPrefab=r.S();e.rendererPath=r.S();e.targetPath=r.S();e.targetAsset=r.S();e.targetBones=r.Strings();
    auto m=std::make_shared<EiemEffectNative::Manifest>();m->key=r.S();m->prefab=r.S();m->rootName=r.S();m->root=size_t(r.N());m->particles=size_t(r.N());auto count=r.Count();for(size_t j=0;j<count;++j){EiemEffectNative::Bundle b;b.file=r.S();b.hash=r.S();b.size=size_t(r.N());auto deps=r.Count();for(size_t k=0;k<deps;++k)b.dependencies.push_back(size_t(r.N()));m->bundles.push_back(std::move(b));}e.native=std::move(m);Require(s.effects.emplace(e.id,std::move(e)).second,"duplicate author effect");}
  Validate(s);return s;
}
} // namespace EiemAuthorControlMetadata

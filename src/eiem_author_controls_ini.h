#pragma once
#include "eiem_author_resource_metadata.h"

// INI owns playback, bindings and attachment relationships. Sample files own
// only numbered numeric channels, so the same samples can be rebound in INI.
namespace EiemAuthorControlsIni {
using namespace EiemDirectory;
namespace C=EiemAuthorControlMetadata;
namespace D=EiemRuntimeAuthorDirectory;
struct Samples {
  std::vector<std::vector<float>> scalar;
  std::vector<std::vector<std::array<float,10>>> bone;
};
inline Samples DecodeSamples(const Bytes& bytes){
  D::Reader r{bytes};Require(r.S()=="EFF_ANIMATION_SAMPLES/1","animation samples structure differs");Samples result;
  auto scalar=[&](){auto v=r.D();Require(std::abs(v)<=FLT_MAX,"animation sample exceeds float");return float(v);};
  auto n=r.Count();for(size_t i=0;i<n;++i){auto count=r.Count();Require(count>=2&&count<=(bytes.size()-r.p)/8,"animation sample extent differs");
    std::vector<float> values;values.reserve(count);for(size_t j=0;j<count;++j)values.push_back(scalar());result.scalar.push_back(std::move(values));}
  n=r.Count();for(size_t i=0;i<n;++i){auto count=r.Count();Require(count>=2&&count<=(bytes.size()-r.p)/80,"bone sample extent differs");
    std::vector<std::array<float,10>> values(count);for(auto& sample:values)for(auto& value:sample)value=scalar();result.bone.push_back(std::move(values));}
  Require(r.p==bytes.size(),"animation samples trailing bytes");return result;
}
using ReadSamples=std::function<Samples(const std::string&)>;
using ReadEffect=std::function<EiemAuthorResourceMetadata::Effect(const std::string&)>;
inline C::State Read(const std::vector<Section>& sections,const ReadSamples& readSamples,const ReadEffect& readEffect){
  C::State state;std::map<std::string,const Section*> index;
  for(const auto& section:sections)Require(index.emplace(section.id,&section).second,"duplicate control section");
  auto optional=[](const Settings& v,const char* key,const std::string& fallback=std::string{}){
    auto found=v.find(key);return found==v.end()?fallback:found->second;};
  auto boolean=[&](const Settings& v,const char* key,bool fallback){return v.count(key)?KeySwitchEnabled(v.at(key)):fallback;};
  for(const auto& section:sections){const auto& v=section.values;
    if(section.id=="Mod")state.keySwitchEnabled=boolean(v,"key_switch_enabled",true);
    else if(section.id=="Constants")for(const auto& row:v){bool persist=Begins(row.first,"persist ");auto name=persist?row.first.substr(8):row.first;
      Require(EiemVariableName(name)&&state.defaults.emplace(name,Number(row.second)).second,"invalid/duplicate INI state");if(persist)state.persistent.insert(name);}
  }
  auto program=EiemControlProgram::Declare(sections,state.defaults,state.persistent);
  for(const auto& section:sections){const auto& v=section.values;
    if(Begins(section.id,"Control")||Begins(section.id,"ShapeControl")){Keys(section,{"variable","label","min","max"});
      Require(state.controls.emplace(v.at("variable"),Control{v.at("label"),Number(v.at("min")),Number(v.at("max"))}).second,"duplicate INI control");
    }else if(Begins(section.id,"Key")){Keys(section,{"key","type"});Binding b;b.id=section.id;b.type=v.at("type");
      Require(EiemParseKeyChord(v.at("key"),&b.key),"unknown INI key chord");b.label=optional(v,"label");if(v.count("speed"))b.speed=Number(v.at("speed"));
      size_t assignments=0;for(const auto& row:v)if(Begins(row.first,"$")){++assignments;b.variable=row.first;for(const auto& value:Split(row.second))b.values.push_back(Number(value));}
      Require(assignments==1,"INI key requires one state assignment");
      if(b.type=="cycle"){std::set<double> values(b.values.begin(),b.values.end());Require(values.size()>=2&&values.size()==b.values.size()&&state.defaults.count(b.variable)&&values.count(state.defaults.at(b.variable)),"INI cycle values/default differ");}
      else Require(b.values.size()==1,"INI press/hold requires one target");state.bindings.push_back(std::move(b));
    }else if(Begins(section.id,"Part")){Keys(section,{"visible"});C::PartState part;std::string error;
      part.visible=EiemExpressionParser(v.at("visible"),error).Parse();Require(part.visible&&part.visible->Validate(state.defaults,error),"invalid INI visibility: "+section.id+": "+error);
      for(const auto& row:v)if(Begins(row.first,"shape.")){Require(row.first.size()>6,"empty shape binding");part.shapes.emplace(row.first.substr(6),row.second);}state.parts.emplace(section.id,std::move(part));
    }else if(Begins(section.id,"MaterialTrack")){Keys(section,{"variable","material","property","kind","component"});
      Require(state.materialTracks.emplace(v.at("variable"),MaterialTrack{v.at("material"),v.at("property"),v.at("kind"),Integer(v.at("component"))}).second,"duplicate INI material track");
    }
  }
  // Bindings can be copied to any section position; INI order is not a
  // dependency order for channel declarations and animation clips.
  for(const auto& section:sections)if(Begins(section.id,"Animation")){const auto& v=section.values;
      Keys(section,{"samples","duration","speed_variable","speed","loop","autoplay"});
      ShapeAnimation clip;clip.id=section.id.substr(strlen("Animation"));Require(Identifier(clip.id),"invalid INI animation id");
      clip.label=optional(v,"label",clip.id);clip.duration=Number(v.at("duration"));clip.speed=Number(v.at("speed"));clip.speedVariable=v.at("speed_variable");
      clip.loop=boolean(v,"loop",true);clip.autoplay=boolean(v,"autoplay",false);if(v.count("key"))Require(EiemParseKeyChord(v.at("key"),&clip.key),"invalid INI animation key");
      auto samples=readSamples(v.at("samples"));
      for(size_t i=0;i<samples.scalar.size();++i){auto key="track."+std::to_string(i);Require(v.count(key),"animation scalar binding absent: "+key);
        Require(clip.tracks.emplace(v.at(key),std::move(samples.scalar[i])).second,"animation scalar binding duplicated");if(state.materialTracks.count(v.at(key)))state.materialAnimations.insert(clip.id);}
      for(size_t i=0;i<samples.bone.size();++i){auto prefix="bone."+std::to_string(i)+".";Require(v.count(prefix+"skeleton")&&v.count(prefix+"path"),"animation bone binding absent");
        Require(index.count("Skeleton"+v.at(prefix+"skeleton"))&&!v.at(prefix+"path").empty(),"animation bone Skeleton/path is unresolved");
        clip.boneTracks.push_back({v.at(prefix+"skeleton"),v.at(prefix+"path"),std::move(samples.bone[i])});}
      for(const auto& row:v){if(Begins(row.first,"track."))Require(Integer(row.first.substr(6))<samples.scalar.size(),"animation scalar index absent");
        if(Begins(row.first,"bone.")){auto end=row.first.find('.',5);Require(end!=row.first.npos&&Integer(row.first.substr(5,end-5))<samples.bone.size(),"animation bone index absent");}}
      Require(state.animations.emplace(clip.id,std::move(clip)).second,"duplicate INI animation");
  }
  for(const auto& section:sections)if(Begins(section.id,"EffectAttachment")){const auto& v=section.values;
    Keys(section,{"mode","variable","target.prefab","prefab","root_name","bundles","root","particles"});
    EffectAttachment effect;effect.id=section.id.substr(strlen("EffectAttachment"));effect.label=optional(v,"label",effect.id);effect.variable=v.at("variable");effect.autoplay=boolean(v,"autoplay",true);
    effect.part=optional(v,"part");effect.bone=optional(v,"bone");effect.targetPrefab=LogicalResourcePath(v.at("target.prefab"));
    effect.rendererPath=optional(v,"renderer");if(v.count("target.path"))effect.targetPath=LogicalResourcePath(v.at("target.path"));effect.targetAsset=optional(v,"target.asset");
    if(v.count("target_bones"))effect.targetBones=Split(v.at("target_bones"));
    Require((v.at("mode")=="replace_game_prefab"&&IsGamePrefabReplacement(effect))||(v.at("mode")=="attach_mesh"&&!IsGamePrefabReplacement(effect)),"INI effect mode/target mismatch");
    if(!effect.part.empty())Require(state.parts.count(effect.part),"INI effect Part is absent");
    auto vector=[&](const char* name,auto& out){if(!v.count(name))return;auto values=Split(v.at(name));Require(values.size()==out.size(),"INI effect transform dimensions differ");for(size_t i=0;i<values.size();++i){auto value=Number(values[i]);Require(std::abs(value)<=FLT_MAX,"INI effect transform overflow");out[i]=float(value);}};
    vector("position",effect.position);vector("rotation",effect.rotation);vector("scale",effect.scale);
    auto native=std::make_shared<EiemEffectNative::Manifest>();native->prefab=LogicalResourcePath(v.at("prefab"));native->rootName=v.at("root_name");native->root=Integer(v.at("root"));native->particles=Integer(v.at("particles"));
    auto resources=Split(v.at("bundles"));std::map<std::string,size_t> positions;D::Writer identity;
    identity.S(native->prefab);identity.S(native->rootName);identity.N(native->root);identity.N(native->particles);identity.N(resources.size());
    for(size_t i=0;i<resources.size();++i)Require(positions.emplace(resources[i],i).second,"duplicate effect Bundle resource");
    for(const auto& resource:resources){auto declared=index.find(resource);Require(declared!=index.end()&&optional(declared->second->values,"type")=="effect","effect Bundle resource is unresolved");
      auto data=readEffect(resource);EiemEffectNative::Bundle bundle;bundle.file=declared->second->values.at("path");bundle.hash=data.hash;bundle.size=data.bytes;identity.S(bundle.hash);
      auto dependencies=declared->second->values.find("dependencies");if(dependencies!=declared->second->values.end())for(const auto& dep:Split(dependencies->second)){
        Require(positions.count(dep),"effect Bundle dependency is unresolved");bundle.dependencies.push_back(positions.at(dep));}
      identity.N(bundle.dependencies.size());for(auto dependency:bundle.dependencies)identity.N(dependency);
      native->bundles.push_back(std::move(bundle));}
    auto digest=Sha(identity.b.data(),identity.b.size());native->key=Hex(digest.data(),digest.size());effect.native=std::move(native);
    Require(state.effects.emplace(effect.id,std::move(effect)).second,"duplicate INI effect");
  }
  for(const auto& row:state.materialTracks){auto material=index.find(row.second.material);Require(material!=index.end()&&Begins(material->first,"Material")&&!Begins(material->first,"MaterialTrack"),"INI material track Material is unresolved");}
  std::set<std::string> clips;for(const auto& clip:state.animations)clips.insert(clip.first);
  EiemControlProgram::Compile(*program,sections,state.defaults,clips);
  std::set<std::string> speeds;
  for(const auto& row:state.animations)speeds.insert(row.second.speedVariable);
  for(const auto& row:program->parameters){const auto& d=row.second;
    if(d.id.empty()||speeds.count(row.first))continue;
    auto old=state.controls.find(row.first);
    if(old==state.controls.end())state.controls.emplace(row.first,Control{d.label,d.minimum,d.maximum});
    else Require(old->second.minimum==d.minimum&&old->second.maximum==d.maximum,d.id+": legacy control range disagrees with parameter");
  }
  for(const auto& row:state.animations){const auto& clip=row.second;
    auto speed=program->parameters.find(clip.speedVariable);
    if(speed!=program->parameters.end())Require(!speed->second.computed,"animation speed parameter cannot be computed");
    for(const auto& track:clip.tracks){auto d=program->parameters.find(track.first);
      if(d!=program->parameters.end())Require(!d->second.computed&&d->second.type==EiemControlProgram::Type::Float,"animation track requires a writable float parameter");}
  }
  for(const auto& binding:program->bindings){
    state.controls.emplace(binding.variable,Control{binding.id,-DBL_MAX,DBL_MAX});
    if(binding.target=="material.float"){
      Require(index.count(binding.material)&&Begins(binding.material,"Material")&&!binding.property.empty(),binding.id+": material/property absent");
      for(const auto& existing:state.materialTracks)Require(existing.second.material!=binding.material||existing.second.property!=binding.property,binding.id+": property already has a driver");
      state.materialTracks.emplace(binding.variable,MaterialTrack{binding.material,binding.property,"FLOAT",0});
    }else{
      Require(state.parts.count(binding.part),binding.id+": Part absent");auto& part=state.parts.at(binding.part);
      if(binding.target=="part.visible"){auto gate=EiemExpression::Make();gate->op=EiemExpression::And;gate->left=part.visible;gate->right=binding.value;part.visible=gate;}
      else{Require(!binding.channel.empty()&&!part.shapes.count(binding.channel),binding.id+": duplicate/empty shape channel");part.shapes.emplace(binding.channel,binding.variable);}
    }
  }
  if(!program->parameters.empty()||!program->actions.empty()||!program->ui.empty()||!program->events.empty())state.program=std::move(program);
  C::Validate(state);return state;
}
} // namespace EiemAuthorControlsIni

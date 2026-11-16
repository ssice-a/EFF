#pragma once
#include "eiem_author_resource_metadata.h"
#include "eiem_author_transform_hierarchy.h"

// INI owns playback, bindings and attachment relationships. Sample files own
// only numbered numeric channels, so the same samples can be rebound in INI.
namespace EiemAuthorControlsIni {
using namespace EiemDirectory;
namespace C=EiemAuthorControlMetadata;
namespace D=EiemRuntimeAuthorDirectory;
using Samples=EiemAnimationSamples::Data;
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
using DeferSamples=std::function<std::shared_ptr<const EiemAnimationSamples::Source>(const std::string&)>;
using ReadEffect=std::function<EiemAuthorResourceMetadata::Effect(const std::string&)>;
inline bool EffectRelationship(const Section& s){return s.values.count("effect")&&(Begins(s.id,"AttachEffect")||Begins(s.id,"ReplaceEffect"));}
inline std::vector<Section> ExpandEffects(const std::vector<Section>& source,const ReadEffect& readEffect){
  std::map<std::string,const Section*> index;for(const auto& s:source)Require(index.emplace(s.id,&s).second,"duplicate INI section");
  std::vector<Section> result;std::set<std::string> names;
  for(const auto& s:source){if(!EffectRelationship(s)){Require(names.insert(s.id).second,"duplicate INI section");result.push_back(s);continue;}
    auto resource=s.values.at("effect");auto found=index.find(resource);Require(found!=index.end()&&found->second->values.count("type")&&found->second->values.at("type")=="effect","effect relationship resource unresolved");
    auto described=readEffect(resource);Require(bool(described.layout),"effect resource native layout absent");const auto& layout=*described.layout;Section expanded;
    bool replace=Begins(s.id,"ReplaceEffect");expanded.id="EffectAttachment"+s.id.substr(strlen(replace?"ReplaceEffect":"AttachEffect"));auto& v=expanded.values;
    for(const auto& row:found->second->values)if(row.first=="bundles"||row.first=="author"||Begins(row.first,"game.")||Begins(row.first,"author."))v.emplace(row);
    if(!v.count("bundles"))v["bundles"]=resource;
    v["prefab"]=layout.prefab;v["root_name"]=layout.rootName;v["root"]=std::to_string(layout.root);v["particles"]=std::to_string(layout.particles);
    for(size_t kind=0;kind<2;++kind){const auto& rows=kind?layout.author:layout.game;auto prefix=kind?"author.":"game.";
      std::set<size_t> declared;for(const auto& field:v)if(Begins(field.first,prefix)){auto at=field.first.find('.',strlen(prefix));Require(at!=field.first.npos,"invalid effect resource binding");declared.insert(Integer(field.first.substr(strlen(prefix),at-strlen(prefix))));}
      Require(declared.size()==rows.size(),"effect binding declarations/layout differ");
      for(size_t i=0;i<rows.size();++i){Require(declared.count(i),"effect binding indices differ");const auto& row=rows[i];if(row.cab.empty())continue;auto p=std::string(prefix)+std::to_string(i)+".";
        v[p+"relocation_cab"]=row.cab;v[p+"relocation_id"]=std::to_string(row.id);
        for(size_t j=0;j<row.patches.size();++j){auto key=p+"patch."+std::to_string(j)+".";v[key+"bundle"]=std::to_string(row.patches[j].bundle);v[key+"offset"]=std::to_string(row.patches[j].offset);v[key+"kind"]=std::to_string(row.patches[j].kind);}}
    }
    Require(!s.values.count("mode")&&!s.values.count("bundles"),"use effect relationship syntax for assembly");
    for(const auto& row:s.values)if(row.first!="effect")v[row.first]=row.second;
    v["mode"]=replace?"replace_game_prefab":"attach_mesh";
    if(replace){Require(v.count("target.path"),"effect replacement logical path absent");v["target.prefab"]=v.at("target.path");v.erase("target.path");}
    Require(names.insert(expanded.id).second,"duplicate effect relationship");result.push_back(std::move(expanded));
  }
  return result;
}
inline C::State Read(const std::vector<Section>& inputSections,const ReadSamples& readSamples,const ReadEffect& readEffect,const DeferSamples& deferSamples={}){
  const auto sections=ExpandEffects(inputSections,readEffect);
  C::State state;std::map<std::string,const Section*> index;
  for(const auto& section:sections)Require(index.emplace(section.id,&section).second,"duplicate control section");
  auto optional=[](const Settings& v,const char* key,const std::string& fallback=std::string{}){
    auto found=v.find(key);return found==v.end()?fallback:found->second;};
  auto boolean=[&](const Settings& v,const char* key,bool fallback){return v.count(key)?KeySwitchEnabled(v.at(key)):fallback;};
  auto transform=[&](const Settings& v,auto& node){
    auto vector=[&](const char* name,auto& out){if(!v.count(name))return;auto values=Split(v.at(name));Require(values.size()==out.size(),"author Transform dimensions differ");for(size_t i=0;i<values.size();++i){auto value=Number(values[i]);Require(std::abs(value)<=FLT_MAX,"author Transform overflow");out[i]=float(value);}};
    vector("position",node.position);vector("rotation",node.rotation);vector("scale",node.scale);
  };
  std::map<std::string,std::shared_ptr<const AuthorTransformHierarchy>> hierarchies;
  for(const auto& section:sections)if(Begins(section.id,"Hierarchy")){
    auto graph=std::make_shared<AuthorTransformHierarchy>();graph->id=section.id.substr(9);std::map<size_t,Settings> nodes;
    for(const auto& field:section.values)if(Begins(field.first,"node.")){auto at=field.first.find('.',5);Require(at!=field.first.npos,"invalid author Transform field");nodes[Integer(field.first.substr(5,at-5))].emplace(field.first.substr(at+1),field.second);}
    for(const auto& row:nodes){Require(row.first==graph->nodes.size(),"author Transform indices must be contiguous");AuthorTransformNode node;node.path=row.second.at("path");node.label=optional(row.second,"label",node.path);transform(row.second,node);graph->nodes.push_back(std::move(node));}
    EiemAuthorTransforms::Validate(*graph);Require(hierarchies.emplace(graph->id,std::move(graph)).second,"duplicate author Transform hierarchy");
  }
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
      Keys(section,{"duration","speed_variable","speed","loop","autoplay"});
      ShapeAnimation clip;clip.id=section.id.substr(strlen("Animation"));Require(Identifier(clip.id),"invalid INI animation id");
      clip.label=optional(v,"label",clip.id);clip.duration=Number(v.at("duration"));clip.speed=Number(v.at("speed"));clip.speedVariable=v.at("speed_variable");
      clip.loop=boolean(v,"loop",true);clip.autoplay=boolean(v,"autoplay",false);if(v.count("key"))Require(EiemParseKeyChord(v.at("key"),&clip.key),"invalid INI animation key");
      std::map<size_t,Settings> outputs;
      for(const auto& field:v)if(Begins(field.first,"native.")){auto at=field.first.find('.',7);Require(at!=field.first.npos,"invalid native Animation output field");outputs[Integer(field.first.substr(7,at-7))].emplace(field.first.substr(at+1),field.second);}
      for(const auto& row:outputs){Require(row.first==clip.nativeOutputs.size(),"native Animation output indices must be contiguous");const auto& fields=row.second;auto graph=fields.at("hierarchy"),resource=fields.at("resource");Require(hierarchies.count(graph)&&index.count(resource)&&optional(index.at(resource)->values,"type")=="animation-clip","native Animation hierarchy/resource unresolved");auto path=index.at(resource)->values.at("path");EiemAuthorTransforms::Path(path);auto asset=LogicalResourcePath(fields.at("asset"));clip.nativeOutputs.push_back({graph,path,asset});}
      {std::set<std::string> targets;for(const auto& output:clip.nativeOutputs)Require(targets.insert(output.hierarchy).second,"duplicate native Animation hierarchy output");}
      Require(v.count("samples")||!clip.nativeOutputs.empty(),"Animation has no sample resource or native outputs");
      if(v.count("samples")&&deferSamples){
        clip.sampleSource=deferSamples(v.at("samples"));Require(bool(clip.sampleSource),"animation sample source absent");
        std::map<size_t,std::map<std::string,std::string>> bone;
        for(const auto& field:v){
          if(Begins(field.first,"track.")){auto channel=Integer(field.first.substr(6));
            Require(clip.scalarChannels.emplace(field.second,channel).second&&clip.tracks.emplace(field.second,std::vector<float>{}).second,"animation scalar binding duplicated");
            if(state.materialTracks.count(field.second))state.materialAnimations.insert(clip.id);
          }else if(Begins(field.first,"bone.")){auto end=field.first.find('.',5);Require(end!=field.first.npos,"animation Bone binding field differs");
            auto name=field.first.substr(end+1);Require(name=="skeleton"||name=="path","animation Bone binding property differs");
            bone[Integer(field.first.substr(5,end-5))].emplace(name,field.second);
          }
        }
        for(const auto& row:bone){const auto& fields=row.second;Require(fields.count("skeleton")&&fields.count("path")&&index.count("Skeleton"+fields.at("skeleton"))&&!fields.at("path").empty(),"animation bone Skeleton/path is unresolved");
          ShapeAnimation::BoneTrack track;track.skeleton=fields.at("skeleton");track.path=fields.at("path");track.channel=row.first;clip.boneTracks.push_back(std::move(track));}
      }else if(v.count("samples")){
      auto samples=readSamples(v.at("samples"));
      for(size_t i=0;i<samples.scalar.size();++i){auto key="track."+std::to_string(i);Require(v.count(key),"animation scalar binding absent: "+key);
        Require(clip.tracks.emplace(v.at(key),std::move(samples.scalar[i])).second,"animation scalar binding duplicated");if(state.materialTracks.count(v.at(key)))state.materialAnimations.insert(clip.id);}
      for(size_t i=0;i<samples.bone.size();++i){auto prefix="bone."+std::to_string(i)+".";Require(v.count(prefix+"skeleton")&&v.count(prefix+"path"),"animation bone binding absent");
        Require(index.count("Skeleton"+v.at(prefix+"skeleton"))&&!v.at(prefix+"path").empty(),"animation bone Skeleton/path is unresolved");
        clip.boneTracks.push_back({v.at(prefix+"skeleton"),v.at(prefix+"path"),std::move(samples.bone[i])});}
      for(const auto& row:v){if(Begins(row.first,"track."))Require(Integer(row.first.substr(6))<samples.scalar.size(),"animation scalar index absent");
        if(Begins(row.first,"bone.")){auto end=row.first.find('.',5);Require(end!=row.first.npos&&Integer(row.first.substr(5,end-5))<samples.bone.size(),"animation bone index absent");}}
      }
      Require(state.animations.emplace(clip.id,std::move(clip)).second,"duplicate INI animation");
  }
  for(const auto& section:sections)if(Begins(section.id,"EffectAttachment")){const auto& v=section.values;
    Keys(section,{"mode","variable","target.prefab","prefab","root_name","bundles","root","particles"});
    EffectAttachment effect;effect.id=section.id.substr(strlen("EffectAttachment"));effect.label=optional(v,"label",effect.id);effect.variable=v.at("variable");effect.autoplay=boolean(v,"autoplay",true);
    effect.part=optional(v,"part");effect.bone=optional(v,"bone");effect.targetPrefab=LogicalResourcePath(v.at("target.prefab"));
    if(v.count("hierarchy")){auto found=hierarchies.find(v.at("hierarchy"));Require(found!=hierarchies.end()&&v.count("node"),"effect author Transform hierarchy/node unresolved");effect.hierarchy=found->second;effect.node=v.at("node");Require(std::any_of(effect.hierarchy->nodes.begin(),effect.hierarchy->nodes.end(),[&](const auto& n){return n.path==effect.node;}),"effect author Transform node absent");}
    else Require(!v.count("node"),"effect author Transform node needs a hierarchy");
    effect.rendererPath=optional(v,"renderer");if(v.count("target.path"))effect.targetPath=LogicalResourcePath(v.at("target.path"));effect.targetAsset=optional(v,"target.asset");
    if(v.count("target_bones"))effect.targetBones=Split(v.at("target_bones"));
    std::map<size_t,Settings> lods;for(const auto& field:v)if(Begins(field.first,"lod.")){auto at=field.first.find('.',4);Require(at!=field.first.npos,"invalid effect LOD field");lods[Integer(field.first.substr(4,at-4))].emplace(field.first.substr(at+1),field.second);}
    std::set<std::string> rendererPaths{effect.rendererPath};
    for(const auto& row:lods){Require(row.first==effect.lodTargets.size()+1&&v.at("mode")=="attach_mesh","effect LOD indices/mode differ");const auto& fields=row.second;EffectLodTarget target;target.renderer=fields.at("renderer");target.path=LogicalResourcePath(fields.at("path"));target.asset=fields.at("asset");if(fields.count("bones"))target.bones=Split(fields.at("bones"));Require(!target.renderer.empty()&&target.renderer.front()!='/'&&target.renderer.find('\\')==target.renderer.npos&&target.renderer.find("..") ==target.renderer.npos&&target.renderer.find("//")==target.renderer.npos&&!target.asset.empty()&&rendererPaths.insert(target.renderer).second&&(effect.bone.empty()||!target.bones.empty()),"effect LOD target/source bones differ");effect.lodTargets.push_back(std::move(target));}
    auto coverage=optional(v,"lod_coverage","all");Require((coverage=="all"||coverage=="source")&&(coverage=="all"||effect.lodTargets.empty()),"effect LOD coverage differs");
    Require((v.at("mode")=="replace_game_prefab"&&IsGamePrefabReplacement(effect))||(v.at("mode")=="attach_mesh"&&!IsGamePrefabReplacement(effect)),"INI effect mode/target mismatch");
    Require(!IsGamePrefabReplacement(effect)||!effect.hierarchy,"game Prefab replacement cannot declare an author hierarchy");
    if(!effect.part.empty())Require(state.parts.count(effect.part),"INI effect Part is absent");
    auto vector=[&](const char* name,auto& out){if(!v.count(name))return;auto values=Split(v.at(name));Require(values.size()==out.size(),"INI effect transform dimensions differ");for(size_t i=0;i<values.size();++i){auto value=Number(values[i]);Require(std::abs(value)<=FLT_MAX,"INI effect transform overflow");out[i]=float(value);}};
    vector("position",effect.position);vector("rotation",effect.rotation);vector("scale",effect.scale);
    std::map<size_t,Settings> basis;for(const auto& field:v)if(Begins(field.first,"lod_anchor.")){auto at=field.first.find('.',11);Require(at!=field.first.npos,"invalid effect LOD anchor field");basis[Integer(field.first.substr(11,at-11))].emplace(field.first.substr(at+1),field.second);}
    if(v.count("lod_anchor")||!basis.empty()){
      Require(v.count("lod_anchor")&&effect.bone.empty()&&!basis.empty()&&basis.size()<=64,"effect renderer basis requires a static mount");
      auto anchor=std::make_shared<EffectLodAnchor>();anchor->path=v.at("lod_anchor");if(anchor->path==".")anchor->path.clear();
      if(!anchor->path.empty())EiemAuthorTransforms::Path(anchor->path);
      for(const auto& path:rendererPaths)Require(anchor->path.empty()||path.rfind(anchor->path+"/",0)==0,"effect LOD anchor is outside renderer ancestry");
      for(const auto& row:basis){Require(row.first==anchor->nodes.size(),"effect LOD anchor indices differ");AuthorTransformNode node;
        auto read=[&](const char* name,auto& out){auto items=Split(row.second.at(name));Require(items.size()==out.size(),"effect LOD anchor dimensions differ");for(size_t i=0;i<items.size();++i){auto n=Number(items[i]);Require(std::abs(n)<=FLT_MAX,"effect LOD anchor overflow");out[i]=float(n);}};
        read("position",node.position);read("rotation",node.rotation);read("scale",node.scale);double norm=0;for(auto value:node.rotation)norm+=double(value)*value;Require(std::abs(norm-1)<.001,"effect LOD anchor quaternion differs");for(auto value:node.scale)Require(value!=0,"effect LOD anchor scale differs");anchor->nodes.push_back(node);}
      effect.lodAnchor=std::move(anchor);
    }
    Require(effect.lodTargets.empty()||!effect.bone.empty()||effect.lodAnchor,"static effect LOD targets require a stable anchor");
    auto native=std::make_shared<EiemEffectNative::Manifest>();native->unifiedResources=std::any_of(inputSections.begin(),inputSections.end(),[&](const auto& s){return EffectRelationship(s)&&s.id.substr(strlen(Begins(s.id,"ReplaceEffect")?"ReplaceEffect":"AttachEffect"))==effect.id;});native->prefab=LogicalResourcePath(v.at("prefab"));native->rootName=v.at("root_name");native->root=Integer(v.at("root"));native->particles=Integer(v.at("particles"));
    auto resources=Split(v.at("bundles"));std::map<std::string,size_t> positions;D::Writer identity;
    identity.S(native->prefab);identity.S(native->rootName);identity.N(native->root);identity.N(native->particles);identity.N(resources.size());
    for(size_t i=0;i<resources.size();++i)Require(positions.emplace(resources[i],i).second,"duplicate effect Bundle resource");
    for(const auto& resource:resources){auto declared=index.find(resource);Require(declared!=index.end()&&optional(declared->second->values,"type")=="effect","effect Bundle resource is unresolved");
      auto data=readEffect(resource);EiemEffectNative::Bundle bundle;bundle.file=declared->second->values.at("path");bundle.hash=data.hash;bundle.size=data.bytes;identity.S(bundle.hash);
      auto dependencies=declared->second->values.find("dependencies");if(dependencies!=declared->second->values.end())for(const auto& dep:Split(dependencies->second)){
        Require(positions.count(dep),"effect Bundle dependency is unresolved");bundle.dependencies.push_back(positions.at(dep));}
      identity.N(bundle.dependencies.size());for(auto dependency:bundle.dependencies)identity.N(dependency);
      native->bundles.push_back(std::move(bundle));}
    std::set<size_t> gameIndices;for(const auto& field:v)if(field.first.rfind("game.",0)==0){auto end=field.first.find('.',5);Require(end!=std::string::npos,"invalid game dependency field");gameIndices.insert(Integer(field.first.substr(5,end-5)));}
    Require(gameIndices.size()<=4096,"too many effect game dependencies");
    for(size_t i=0;i<gameIndices.size();++i){Require(gameIndices.count(i),"effect game dependency indices are not contiguous");auto prefix="game."+std::to_string(i)+".";EiemEffectNative::GameResource game;game.path=LogicalResourcePath(v.at(prefix+"path"));game.type=v.at(prefix+"type");auto asset=v.find(prefix+"asset");if(asset!=v.end())game.asset=asset->second;Require(!game.type.empty(),"effect game dependency identity incomplete");
      auto source=v.find(prefix+"source_prefab"),root=v.find(prefix+"source_root");
      Require(root==v.end()||source!=v.end(),"effect game dependency root has no source Prefab");
      if(source!=v.end()){game.sourcePrefab=LogicalResourcePath(source->second);if(root!=v.end())game.sourceRoot=root->second;Require(game.sourcePrefab.size()>7&&game.sourcePrefab.compare(game.sourcePrefab.size()-7,7,".prefab")==0,"effect game dependency source is not a logical Prefab");}
      auto relocation=v.find(prefix+"relocation_cab");if(relocation!=v.end()){
        game.relocationCab=relocation->second;Require(game.relocationCab.size()==36&&game.relocationCab.rfind("CAB-",0)==0&&game.relocationCab.find_first_not_of("0123456789abcdefABCDEF",4)==std::string::npos,"effect relocation CAB differs");
        game.relocationId=Integer(v.at(prefix+"relocation_id"));Require(game.relocationId,"effect relocation PathID absent");
        std::set<size_t> patches;for(const auto& field:v)if(field.first.rfind(prefix+"patch.",0)==0){auto at=prefix.size()+6,end=field.first.find('.',at);Require(end!=std::string::npos,"effect relocation patch field differs");patches.insert(Integer(field.first.substr(at,end-at)));}
        Require(!patches.empty()&&patches.size()<=65536,"effect relocation patch count differs");
        identity.S(game.relocationCab);identity.N(game.relocationId);identity.N(patches.size());
        for(size_t pi=0;pi<patches.size();++pi){Require(patches.count(pi),"effect relocation patches are not contiguous");auto p=prefix+"patch."+std::to_string(pi)+".";EiemEffectNative::GameResource::Patch patch{Integer(v.at(p+"bundle")),Integer(v.at(p+"offset")),Integer(v.at(p+"kind"))};
          Require(patch.bundle<native->bundles.size()&&patch.kind<=1,"effect relocation patch target differs");auto bytes=native->bundles[patch.bundle].Size();auto size=patch.kind?8u:36u;Require(patch.offset<=bytes&&size<=bytes-patch.offset,"effect relocation patch exceeds Bundle");
          identity.N(patch.bundle);identity.N(patch.offset);identity.N(patch.kind);game.patches.push_back(patch);}
      }else Require(!game.asset.empty(),"effect legacy game dependency asset absent");
      identity.S(game.path);identity.S(game.type);identity.S(game.asset);if(!game.sourcePrefab.empty()){identity.S(game.sourcePrefab);identity.S(game.sourceRoot);}native->gameResources.push_back(std::move(game));}
    std::set<size_t> authorIndices;for(const auto& field:v)if(Begins(field.first,"author.")){auto end=field.first.find('.',7);Require(end!=std::string::npos,"invalid author effect binding");authorIndices.insert(Integer(field.first.substr(7,end-7)));}
    Require(authorIndices.size()<=4096,"too many author effect bindings");
    for(size_t i=0;i<authorIndices.size();++i){Require(authorIndices.count(i),"author effect binding indices differ");auto prefix="author."+std::to_string(i)+".";EiemEffectNative::AuthorResource author;
      author.resource=v.at(prefix+"resource");author.type=v.at(prefix+"type");Require(index.count(author.resource),"author effect resource unresolved");
      const auto& declaration=index.at(author.resource)->values;
      Require((author.type=="Material"&&Begins(author.resource,"Material"))||(author.type=="Mesh"&&Begins(author.resource,"Mesh"))||(author.type=="Texture2D"&&Begins(author.resource,"Texture")),"author effect resource type differs");
      author.relocationCab=v.at(prefix+"relocation_cab");author.relocationId=Integer(v.at(prefix+"relocation_id"));
      Require(author.relocationCab.size()==36&&author.relocationCab.rfind("CAB-",0)==0&&author.relocationCab.find_first_not_of("0123456789abcdefABCDEF",4)==std::string::npos&&author.relocationId,"author effect relocation marker differs");
      std::set<size_t> patches;for(const auto& field:v)if(field.first.rfind(prefix+"patch.",0)==0){auto at=prefix.size()+6,end=field.first.find('.',at);Require(end!=field.first.npos,"author effect patch differs");patches.insert(Integer(field.first.substr(at,end-at)));}
      Require(!patches.empty()&&patches.size()<=65536,"author effect relocation sites absent");identity.S(author.resource);identity.S(author.type);identity.S(author.relocationCab);identity.N(author.relocationId);
      for(size_t j=0;j<patches.size();++j){Require(patches.count(j),"author effect patches not contiguous");auto p=prefix+"patch."+std::to_string(j)+".";EiemEffectNative::GameResource::Patch patch{Integer(v.at(p+"bundle")),Integer(v.at(p+"offset")),Integer(v.at(p+"kind"))};
        Require(patch.bundle<native->bundles.size()&&patch.kind<=1,"author effect relocation target differs");auto bytes=native->bundles[patch.bundle].Size(),size=patch.kind?size_t(8):size_t(36);Require(patch.offset<=bytes&&size<=bytes-patch.offset,"author effect patch exceeds Bundle");
        identity.N(patch.bundle);identity.N(patch.offset);identity.N(patch.kind);author.patches.push_back(patch);}
      native->authorResources.push_back(std::move(author));
    }
    auto digest=Sha(identity.b.data(),identity.b.size());native->key=Hex(digest.data(),digest.size());effect.native=std::move(native);
    Require(state.effects.emplace(effect.id,std::move(effect)).second,"duplicate INI effect");
  }
  for(const auto& row:state.materialTracks){auto material=index.find(row.second.material);Require(material!=index.end()&&Begins(material->first,"Material")&&!Begins(material->first,"MaterialTrack"),"INI material track Material is unresolved");}
  for(const auto& row:state.animations)for(const auto& output:row.second.nativeOutputs)Require(std::any_of(state.effects.begin(),state.effects.end(),[&](const auto& e){return e.second.hierarchy&&e.second.hierarchy->id==output.hierarchy;}),"native Animation hierarchy has no owner attachment");
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

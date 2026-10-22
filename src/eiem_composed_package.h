#pragma once
#include "eiem_loading_input_substitution.h"
#include "eiem_runtime_shape_identity.h"

// Compose declarations BEFORE writing any native input. Shared CABs, Prefabs,
// preload lists and namespaces are consequently built exactly once.
namespace EiemComposedPackage {
using namespace EiemNativeInput;
inline std::string Prefix(const std::string& id) {
  auto hash=Sha(reinterpret_cast<const unsigned char*>(id.data()),id.size());
  return "M"+Hex(hash.data(),hash.size())+"_";
}
inline std::string Name(const std::string& id,const std::string& section) {return Prefix(id)+section;}
inline bool IsEffectRoot(const std::filesystem::path& root) {
  for(const auto& section:ParseIni(Read(root/"mod.ini")))
    if(section.id=="Mod")return section.values.count("type")&&section.values.at("type")=="effect";
  return false;
}
inline std::vector<std::filesystem::path> Roots(const std::filesystem::path& directory,bool includeEffects=false) {
  auto result=EiemModDirectory::Roots(directory);
  if(!includeEffects)result.erase(std::remove_if(result.begin(),result.end(),[](const auto& root){return IsEffectRoot(root);}),result.end());
  return result;
}
inline std::string ResourceDigest(const Resource& resource) {
  return ResourceContentHash(resource);
}
inline std::string TextureDigest(const Package& p,const std::string& section,const Texture& t) {
  std::ostringstream out;out.imbue(std::locale::classic());out<<std::setprecision(17);
  out<<t.source.path.size()<<':'<<t.source.path<<t.source.asset.size()<<':'<<t.source.asset
     <<':'<<t.width<<':'<<t.height<<':'<<t.mips<<':'<<t.filter<<':'<<t.wrap<<':'<<t.aniso<<':'<<t.linear<<':'<<t.bias
     <<':'<<ResourceDigest(*p.resources.at(t.resource));
  if(t.source.path.empty()) {
    // Inferred templates must remain in their existing dependency CAB. Equal
    // pixels alone cannot move a texture to another material's template.
    std::set<std::string> origins;
    for(const auto& m:p.materials) for(const auto& slot:m.second.textures) if(slot.second==section)
      origins.insert(m.second.source.path+"\n"+m.second.source.asset+"\n"+slot.first);
    for(const auto& origin:origins) out<<':'<<origin.size()<<':'<<origin;
  }
  auto text=out.str();auto hash=Sha(reinterpret_cast<const unsigned char*>(text.data()),text.size());return Hex(hash.data(),hash.size());
}
inline std::shared_ptr<EiemExpression> Expression(const std::shared_ptr<EiemExpression>& old,const std::string& prefix) {
  std::shared_ptr<EiemExpression> result;
  using Slot=std::pair<const EiemExpression*,std::shared_ptr<EiemExpression>*>;
  std::vector<Slot> pending{{old.get(),&result}};
  while(!pending.empty()) {
    auto slot=pending.back();pending.pop_back();if(!slot.first)continue;
    auto next=EiemExpression::Make();next->op=slot.first->op;next->number=slot.first->number;next->variable=slot.first->variable;
    if(next->op==EiemExpression::Variable)next->variable="$"+prefix+next->variable.substr(1);
    *slot.second=next;pending.push_back({slot.first->right.get(),&next->right});pending.push_back({slot.first->left.get(),&next->left});
  }
  return result;
}
inline Package Compose(const std::vector<Package>& authors,bool forceSet=false,bool reuseCopies=true) {
  if(authors.size()==1 && !forceSet) return authors.front();
  Package result;result.id="EFF_COMPOSED_1";result.composed=true;
  std::vector<const Package*> ordered;std::set<std::string> ids,folders;
  for(const auto& p:authors) {
    Require(!p.composed && ids.insert(p.id).second,"duplicate/composed Mod id: "+p.id);
    auto folder=InputPath(std::filesystem::weakly_canonical(p.root).u8string());
    Require(folders.insert(folder).second,"duplicate Mod directory: "+folder);
    ordered.push_back(&p);
  }
  std::sort(ordered.begin(),ordered.end(),[](auto a,auto b){return a->id<b->id;});
  std::string identity="EFF_COMPOSED_AUTHOR_1\n";
  std::map<std::string,std::string> textureAliases;
  std::map<std::pair<Identity,std::string>,std::string> materialCopies;
  for(const auto* source:ordered) {
    const auto& p=*source;const auto prefix=Prefix(p.id);
    identity+=p.id+":"+p.root.filename().u8string()+":"+PackageDigest(p)+"\n";
    auto resourceName=[&](const std::string& path){auto split=path.find('/');Require(split!=path.npos,"composed resource folder absent");return path.substr(0,split+1)+prefix+path.substr(split+1);};
    for(const auto& r:p.resources) Require(result.resources.emplace(resourceName(r.first),r.second).second,"composed resource collision");
    for(const auto& m:p.meshes) {auto copy=m.second;copy.resource=resourceName(copy.resource);result.meshes.emplace(prefix+m.first,std::move(copy));}
    std::map<std::string,std::string> textures,materials;
    for(const auto& t:p.textures) {
      auto fingerprint=reuseCopies?TextureDigest(p,t.first,t.second):prefix+t.first;auto candidate=prefix+t.first;
      auto alias=textureAliases.emplace(fingerprint,candidate);textures[t.first]=alias.first->second;
      if(alias.second) {auto copy=t.second;copy.resource=resourceName(copy.resource);result.textures.emplace(candidate,std::move(copy));}
    }
    for(const auto& m:p.materials) {
      auto copy=m.second;copy.resource=resourceName(copy.resource);
      std::string fingerprint=ResourceDigest(*p.resources.at(m.second.resource));
      for(auto& slot:copy.textures) {slot.second=textures.at(slot.second);fingerprint+='\n'+slot.first+":"+slot.second;}
      // A source Material is a clone template. Only identical authored
      // copies may share an object; different part bindings stay independent.
      if(!reuseCopies)fingerprint+=prefix+m.first;
      auto candidate=prefix+m.first;auto alias=materialCopies.emplace(std::make_pair(copy.source,fingerprint),candidate);
      materials[m.first]=alias.first->second;
      if(alias.second) result.materials.emplace(candidate,std::move(copy));
    }
    for(const auto& part:p.parts) {
      auto copy=part.second;copy.mesh=prefix+copy.mesh;copy.material=materials.at(copy.material);
      copy.visible=Expression(copy.visible,prefix);
      for(auto& shape:copy.shapes) shape.second="$"+prefix+shape.second.substr(1);
      result.parts.emplace(prefix+part.first,std::move(copy));result.partOwners.emplace(prefix+part.first,std::make_pair(p.id,part.first));
    }
    for(const auto& rule:p.rules) {
      auto copy=rule.second;for(auto& part:copy.parts) part=prefix+part;
      for(const auto& target:copy.targets)
        Require(result.targetIndex.emplace(target.second,prefix+rule.first).second,
                "conflicting Mesh path+asset: "+target.second.path+" / "+target.second.asset+" (Mod "+p.id+")");
      result.rules.emplace(prefix+rule.first,std::move(copy));
    }
    for(const auto& v:p.defaults) result.defaults.emplace("$"+prefix+v.first.substr(1),v.second);
    for(const auto& v:p.persistent) result.persistent.insert("$"+prefix+v.substr(1));
    for(const auto& c:p.controls) result.controls.emplace("$"+prefix+c.first.substr(1),c.second);
    for(auto binding:p.bindings) {binding.id=prefix+binding.id;binding.variable="$"+prefix+binding.variable.substr(1);result.bindings.push_back(std::move(binding));}
  }
  result.iniHash=Sha(reinterpret_cast<const unsigned char*>(identity.data()),identity.size());return result;
}
inline std::vector<Package> LoadSet(const std::filesystem::path& directory) {
  std::vector<Package> result;for(const auto& root:Roots(directory)) result.push_back(Load(root));
  return result;
}
inline bool Matches(const std::filesystem::path& directory,const std::vector<Package>& authors) {
  auto roots=Roots(directory);if(roots.size()!=authors.size()) return false;
  std::set<std::string> expected;
  for(const auto& p:authors) expected.insert(InputPath(std::filesystem::weakly_canonical(p.root).u8string()));
  for(const auto& r:roots) if(!expected.count(InputPath(r.u8string()))) return false;
  return expected.size()==authors.size();
}
} // namespace EiemComposedPackage

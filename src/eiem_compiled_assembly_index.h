#pragma once

namespace EiemCompiledMod {
constexpr const char* assemblyIndex="prefab-assemblies/string+bone-palette-dictionary/le64";
struct AssemblyIds {std::vector<size_t> roots,pristine;};
inline AssemblyIds WritePreparedAssemblies(Writer& output,const Artifact& a){
 using namespace EiemRenderAssemblyDescription;
 std::map<std::string,size_t> strings;std::vector<std::string> stringValues;
 std::map<std::vector<std::string>,size_t> palettes;std::vector<const std::vector<std::string>*> paletteValues;
 std::map<Bytes,size_t> assemblies;AssemblyIds ids;
 auto text=[&](const std::string& value){auto found=strings.emplace(value,stringValues.size());if(found.second)stringValues.push_back(value);return found.first->second;};
 auto palette=[&](const std::vector<std::string>& value){auto found=palettes.emplace(value,paletteValues.size());if(found.second){paletteValues.push_back(&value);for(const auto& bone:value)text(bone);}return found.first->second;};
 // Dictionary IDs are injective within this invocation. Use the actual compact
 // body as the exact deduplication key instead of first serializing and hashing
 // every repeated long bone path. A duplicate adds no dictionary entries, so
 // first-use string/palette/assembly order and the output bytes stay identical.
 Writer body;auto add=[&](const Description& d){Writer encoded;
  auto address=[&](const Address& value){encoded.N(text(value.path));encoded.N(value.component);};
  encoded.N(d.sources.size());for(const auto& source:d.sources){encoded.N(text(source.cab));encoded.N(uint64_t(source.id));address(source.renderer);encoded.N(source.targets.size());
   for(const auto& target:source.targets){address(target.renderer);encoded.N(text(target.marker));encoded.N(palette(target.Bones()));for(const auto* path:{&target.root,&target.skin,&target.probe,&target.lighting})encoded.N(text(*path));for(bool flag:{target.hasRoot,target.hasSkin,target.hasProbe,target.hasLighting,target.extension})encoded.B(flag);}
  }
  encoded.N(d.lods.size());for(const auto& lod:d.lods){address(lod.group);encoded.N(lod.levels.size());for(const auto& level:lod.levels){encoded.D(level.height);encoded.D(level.fade);encoded.N(level.renderers.size());for(const auto& renderer:level.renderers)address(renderer);}}
  encoded.N(d.helpers.size());for(const auto& helper:d.helpers){address(helper.component);encoded.B(helper.custom);encoded.N(helper.renderers.size());for(const auto& renderer:helper.renderers)address(renderer);}
  auto found=assemblies.emplace(std::move(encoded.b),assemblies.size());if(found.second)body.Raw(found.first->first.data(),found.first->first.size());return found.first->second;
 };
 for(const auto& root:a.manifest.plan.roots)ids.roots.push_back(add(root.Assembly()));for(const auto& root:a.pristine)ids.pristine.push_back(add(root.Assembly()));
 output.S(assemblyIndex);output.Strings(stringValues);output.N(paletteValues.size());for(const auto* bones:paletteValues){output.N(bones->size());for(const auto& bone:*bones)output.N(strings.at(bone));}
 output.N(assemblies.size());output.Blob(body.b);output.N(ids.roots.size());for(auto id:ids.roots)output.N(id);return ids;
}
inline std::vector<std::shared_ptr<const EiemRenderAssemblyDescription::Description>> ReadPreparedAssemblies(EiemOfflinePackage::Reader& reader,Plan& plan){
 using namespace EiemRenderAssemblyDescription;Require(reader.S()==assemblyIndex,"compiled assembly index structure differs");auto strings=reader.Strings();
 std::vector<std::shared_ptr<const std::vector<std::string>>> palettes;auto n=reader.Count();for(size_t i=0;i<n;++i){auto bones=std::make_shared<std::vector<std::string>>();auto count=reader.Count();bones->reserve(count);for(size_t j=0;j<count;++j)bones->push_back(strings.at(reader.Size()));palettes.push_back(std::move(bones));}
 n=reader.Count();auto bytes=reader.Blob();EiemOfflinePackage::Reader r{bytes};auto text=[&](){return strings.at(r.Size());};auto address=[&](){Address value;value.path=text();value.component=r.U();return value;};
 std::vector<std::shared_ptr<const Description>> assemblies;for(size_t i=0;i<n;++i){auto d=std::make_shared<Description>();std::set<std::pair<std::string,int64_t>> sources;std::set<Address> anchors,targets;
  auto count=r.Count();d->sources.reserve(count);for(size_t j=0;j<count;++j){EiemRenderAssemblyDescription::Source source;source.cab=text();source.id=int64_t(r.N());source.renderer=address();Require(EiemLoadingGeneration::Cab(source.cab)&&source.id&&sources.emplace(source.cab,source.id).second&&anchors.insert(source.renderer).second,"compiled assembly source identity differs");auto entries=r.Count();source.targets.reserve(entries);
   for(size_t k=0;k<entries;++k){Target target;target.renderer=address();target.marker=text();target.preparedBones=palettes.at(r.Size());target.root=text();target.skin=text();target.probe=text();target.lighting=text();target.hasRoot=r.B();target.hasSkin=r.B();target.hasProbe=r.B();target.hasLighting=r.B();target.extension=r.B();
    Require(targets.insert(target.renderer).second&&((k==0)==!target.extension)&&(!k?target.renderer==source.renderer:!(target.renderer==source.renderer))&&(!target.extension||!target.marker.empty()),"compiled assembly target ownership differs");source.targets.push_back(std::move(target));}
   d->sources.push_back(std::move(source));}
  count=r.Count();for(size_t j=0;j<count;++j){Lod lod;lod.group=address();auto entries=r.Count();for(size_t k=0;k<entries;++k){LodLevel level;level.height=float(r.D());level.fade=float(r.D());auto size=r.Count();for(size_t q=0;q<size;++q){auto value=address();Require(targets.count(value),"compiled LOD target absent");level.renderers.push_back(std::move(value));}lod.levels.push_back(std::move(level));}d->lods.push_back(std::move(lod));}
  count=r.Count();for(size_t j=0;j<count;++j){Helper helper;helper.component=address();helper.custom=r.B();auto entries=r.Count();for(size_t k=0;k<entries;++k){auto value=address();Require(targets.count(value),"compiled helper target absent");helper.renderers.push_back(std::move(value));}d->helpers.push_back(std::move(helper));}assemblies.push_back(std::move(d));
 }
 Require(r.p==bytes.size(),"compiled assembly dictionary suffix differs");n=reader.Count();Require(n==plan.roots.size(),"compiled root assembly coverage differs");for(auto& root:plan.roots){root.preparedAssembly=assemblies.at(reader.Size());std::map<std::string,unsigned> markers;for(const auto& source:root.Assembly().sources){Require(source.cab==root.cab,"compiled root has foreign Renderer");for(const auto& target:source.targets)if(!target.marker.empty()){Require(plan.parts.count(target.marker),"compiled assembly part absent");++markers[target.marker];}}Require(markers==root.markers,"compiled assembly marker coverage differs");}return assemblies;
}
inline void ExpandPreparedAssembly(Root& root){
 if(!root.preparedAssembly)return;root.assembly=*root.preparedAssembly;root.preparedAssembly.reset();for(auto& source:root.assembly.sources)for(auto& target:source.targets)if(target.preparedBones){target.bones=target.Bones();target.preparedBones.reset();}
}
} // namespace EiemCompiledMod

// Read-only survey of compiled consumers and original selected game inputs.
// This never materializes a replacement or loads a Unity asset.
#include "../../src/eiem_compiled_mod.h"
#include "../../src/eiem_static_resource_sources.h"
#include <iostream>

using namespace EiemCompiledMod;
std::string Json(const std::string& value){
 std::string out="\"";for(unsigned char c:value){if(c=='"'||c=='\\'){out+='\\';out+=char(c);}else if(c<32){static const char hex[]="0123456789ABCDEF";out+="\\u00";out+=hex[c>>4];out+=hex[c&15];}else out+=char(c);}return out+'"';
}
std::string Category(const std::string& path){
 const std::string prefix="assets/beyond/dynamicassets/gameplay/";auto relative=path.rfind(prefix,0)==0?path.substr(prefix.size()):path;
 if(relative.rfind("actors/postmodels/",0)==0){auto end=relative.find('/',strlen("actors/postmodels/"));return relative.substr(0,end);}
 return relative.substr(0,relative.find('/'));
}
void StringCounts(const std::map<std::string,size_t>& counts){
 std::cout<<'{';bool first=true;for(const auto& row:counts){if(!first)std::cout<<',';first=false;std::cout<<Json(row.first)<<':'<<row.second;}std::cout<<'}';
}
using SurveyClock=std::chrono::steady_clock;
double Ms(SurveyClock::time_point start){return std::chrono::duration<double,std::milli>(SurveyClock::now()-start).count();}
struct SourceData {Bytes raw;EndfieldInput::Bundle bundle;double readHashMs=0,decodeMs=0;};
int wmain(int argc,wchar_t** argv){try{
 Require(argc>=3,"profile_prefab_consumers <game> <Mod> [Mod...]");auto game=std::filesystem::canonical(argv[1]);
 std::map<std::string,SourceData> sources;
 auto get=[&](const EiemStaticSources::Entry& entry)->const SourceData&{
  auto key=entry.logical+":"+entry.revision;auto found=sources.find(key);if(found!=sources.end())return found->second;
  SourceData data;auto start=SurveyClock::now();data.raw=EiemStaticSources::Payload(entry);data.readHashMs=Ms(start);start=SurveyClock::now();data.bundle=EndfieldInput::Decode(data.raw);data.decodeMs=Ms(start);return sources.emplace(key,std::move(data)).first->second;
 };
 std::cout<<"{\"originalGameInputs\":true,\"gameFilesModified\":false,\"mods\":[";
 for(int arg=2;arg<argc;++arg){
  if(arg!=2)std::cout<<',';auto started=std::chrono::steady_clock::now();auto mod=std::filesystem::canonical(argv[arg]);auto package=EiemDirectory::Load(mod);
  auto selection=EiemStaticSources::LoadMod(mod,game);auto artifact=DecodeRuntime(EiemDirectory::Read(mod/"compiled.bin"),mod,game);
  std::set<Identity> wanted;std::set<std::string> targetSources,targetCabs;std::map<NativeReference,Identity> identities;
  std::map<std::string,std::pair<size_t,size_t>> ownerMeshCounts;
  for(const auto& rule:package.targetIndex){wanted.insert(rule.first);targetSources.insert(selection.assets.at(EiemStaticSources::Logical(rule.first.path)));}
  for(const auto& name:targetSources){const auto& source=get(selection.inputs.at(name));for(const auto& member:source.bundle.members)if(SerializedHeader(member.bytes)){
   auto layout=ParseSerialized(member.bytes);for(const auto& object:layout.objects)if(layout.types.at(object.second.type).classId==43)++ownerMeshCounts[name].first;
   for(const auto& asset:SerializedAssets(member.bytes,member.name,layout)){
    Identity id{EiemStaticSources::Logical(asset.path),asset.asset};if(!wanted.count(id))continue;Require(asset.classId==43,"declared target is not an original Mesh");
    NativeReference reference{EiemStaticSources::Cab(asset.cab),asset.pathId};auto inserted=identities.emplace(reference,id);Require(inserted.second||(inserted.first->second.path==id.path&&inserted.first->second.asset==id.asset),"target native reference aliases another logical identity");targetCabs.insert(reference.first);if(inserted.second)++ownerMeshCounts[name].second;
   }
  }}
  Require(identities.size()==wanted.size(),"not every declared target resolved in original game inputs");std::set<NativeReference> selected;for(const auto& row:identities)selected.insert(row.first);
  std::set<std::string> rawRoots,compiledRoots;for(const auto& root:artifact.manifest.plan.roots)compiledRoots.insert(root.path);
  size_t candidatePrefabs=0,prefabCandidateInputs=0,matchedInputs=0,matchedRenderers=0,externalHits=0,localHits=0,directOwnerInputs=0,mixedRoots=0;
  size_t totalCandidateBytes=0,maxRootsPerInput=0,multiRootInputs=0;std::map<std::string,size_t> categories,sourceStemMentions,rootBoneSignatures,meshConsumers;
  std::cout<<"{\"mod\":"<<Json(package.id)<<",\"targetMeshes\":"<<wanted.size()<<",\"targetOwnerInputs\":"<<targetSources.size()<<",\"candidateInputs\":"<<selection.candidates.size()<<",\"meshOwnerSources\":[";
  bool firstOwner=true;for(const auto& owner:ownerMeshCounts){if(!firstOwner)std::cout<<',';firstOwner=false;std::cout<<"{\"source\":"<<Json(owner.first)<<",\"allNativeMeshes\":"<<owner.second.first<<",\"targetMeshes\":"<<owner.second.second<<'}';}std::cout<<"],\"inputs\":[";
  bool firstInput=true;
  for(const auto& name:selection.candidates){
   const auto& entry=selection.inputs.at(name);const auto& source=get(entry);totalCandidateBytes+=entry.size;
   size_t rootCount=0,inputMatches=0,inputExternal=0,inputLocal=0;bool directOwner=false;std::set<std::string> inputPrefabPaths;
   double metadataMs=0,meshCompareMs=0,rootOwnershipMs=0;
   for(const auto& member:source.bundle.members)if(SerializedHeader(member.bytes)){
    auto stage=SurveyClock::now();auto layout=ParseSerialized(member.bytes);for(const auto& external:layout.externals)if(targetCabs.count(EiemStaticSources::Cab(external)))directOwner=true;
    auto assets=SerializedAssets(member.bytes,member.name,layout);std::vector<Asset> roots;
    for(const auto& asset:assets)if(asset.classId==1&&EiemStaticSources::Ends(EiemStaticSources::Logical(asset.path),".prefab")){roots.push_back(asset);inputPrefabPaths.insert(asset.path);++rootCount;}
    metadataMs+=Ms(stage);stage=SurveyClock::now();auto matches=MeshConsumers(member.bytes,layout,member.name,selected,EiemStaticSources::Cab);meshCompareMs+=Ms(stage);
    for(const auto& match:matches){auto ref=match.mesh;ref.first=EiemStaticSources::Cab(ref.first);++meshConsumers[identities.at(ref).asset];++inputMatches;
     if(ref.first==EiemStaticSources::Cab(member.name))++inputLocal;else ++inputExternal;
    }
    if(matches.empty())continue;
    for(const auto& root:roots){stage=SurveyClock::now();EiemRenderAssemblyDescription::Graph graph(member.bytes,member.name,root.pathId);std::vector<const MeshConsumer*> owned;
     for(const auto& match:matches)if(graph.Contains(match.renderer))owned.push_back(&match);rootOwnershipMs+=Ms(stage);auto hitCount=owned.size();
     for(const auto* use:owned){const auto& match=*use;
      auto target=graph.ReadTarget(match.renderer,false);std::string signature,common;
      // Compare paths relative to the common skeleton ancestor, so differing
      // story wrappers do not create artificial palette variants.
      if(!target.Bones().empty()){common=target.Bones().front();auto include=[&](const std::string& path){size_t n=0;while(n<common.size()&&n<path.size()&&common[n]==path[n])++n;common.resize(n);};
       for(const auto& bone:target.Bones())include(bone);if(target.hasRoot)include(target.root);
       auto end=common.find_last_of('/');common=end==std::string::npos?std::string{}:common.substr(0,end+1);
      }
      for(const auto& bone:target.Bones())signature+=bone.substr(common.size())+'\n';
      auto ref=match.mesh;ref.first=EiemStaticSources::Cab(ref.first);auto key=identities.at(ref).asset+":"+Hash(Bytes(signature.begin(),signature.end()));++rootBoneSignatures[key];
     }
     if(!hitCount)continue;rawRoots.insert(EiemStaticSources::Logical(root.path));++categories[Category(root.path)];if(hitCount<graph.Renderers().size())++mixedRoots;
     auto basename=root.path.substr(root.path.find_last_of('/')+1);std::set<std::string> stems;
     for(const auto* use:owned){auto ref=use->mesh;ref.first=EiemStaticSources::Cab(ref.first);auto asset=identities.at(ref).asset;auto begin=asset.find("actor_");if(begin!=asset.npos){begin+=6;auto end=asset.find('_',begin);stems.insert(EiemStaticSources::Logical(asset.substr(begin,end-begin)));}}
     for(const auto& stem:stems)if(EiemStaticSources::Logical(basename).find(stem)!=std::string::npos)++sourceStemMentions[stem];
    }
   }
   candidatePrefabs+=rootCount;maxRootsPerInput=std::max(maxRootsPerInput,rootCount);if(rootCount)++prefabCandidateInputs;if(rootCount>1)++multiRootInputs;
   if(inputMatches){++matchedInputs;matchedRenderers+=inputMatches;externalHits+=inputExternal;localHits+=inputLocal;if(directOwner)++directOwnerInputs;}
   if(!firstInput)std::cout<<',';firstInput=false;
   std::cout<<"{\"source\":"<<Json(name)<<",\"bytes\":"<<entry.size<<",\"prefabRoots\":"<<rootCount<<",\"meshMatches\":"<<inputMatches<<",\"externalMatches\":"<<inputExternal<<",\"localMatches\":"<<inputLocal<<",\"directTargetCabDependency\":"<<(directOwner?"true":"false")<<",\"prefabPaths\":[";
   bool firstPath=true;for(const auto& path:inputPrefabPaths){if(!firstPath)std::cout<<',';firstPath=false;std::cout<<Json(path);}std::cout<<"],\"readHashMs\":"<<source.readHashMs<<",\"sourceDecodeMs\":"<<source.decodeMs<<",\"metadataInspectMs\":"<<metadataMs<<",\"meshCompareMs\":"<<meshCompareMs<<",\"rootOwnershipMs\":"<<rootOwnershipMs<<",\"contentDecisionMs\":"<<(source.readHashMs+source.decodeMs+metadataMs+meshCompareMs+rootOwnershipMs)<<'}';
  }
  std::cout<<"],\"candidatePrefabRoots\":"<<candidatePrefabs<<",\"prefabCandidateInputs\":"<<prefabCandidateInputs<<",\"matchedInputs\":"<<matchedInputs<<",\"matchedPrefabRoots\":"<<rawRoots.size()<<",\"matchedRenderers\":"<<matchedRenderers<<",\"externalMeshReferences\":"<<externalHits<<",\"localMeshReferences\":"<<localHits<<",\"matchedInputsWithDirectTargetCabDependency\":"<<directOwnerInputs<<",\"maxPrefabRootsPerInput\":"<<maxRootsPerInput<<",\"multiPrefabInputs\":"<<multiRootInputs<<",\"mixedMatchedAndOtherRenderers\":"<<mixedRoots<<",\"candidateSourceBytes\":"<<totalCandidateBytes<<",\"compiledRootSetMatchesOriginalDiscovery\":"<<(rawRoots==compiledRoots?"true":"false")<<",\"pathCategories\":";StringCounts(categories);
  std::cout<<",\"prefabBasenamesContainingTargetCharacter\":";StringCounts(sourceStemMentions);std::cout<<",\"meshConsumers\":";StringCounts(meshConsumers);std::cout<<",\"normalizedBonePaletteSignatures\":";StringCounts(rootBoneSignatures);
  auto elapsed=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count();std::cout<<",\"milliseconds\":"<<elapsed<<'}';
 }
 std::cout<<"],\"uniqueSourceInputsRead\":"<<sources.size()<<"}\n";return 0;
}catch(const std::exception& error){std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}}

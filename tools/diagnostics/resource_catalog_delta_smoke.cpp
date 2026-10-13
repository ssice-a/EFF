#include "../../src/eiem_resource_catalog_delta.h"
#include "../../src/eiem_mounted_source_identity.h"
#include "../../src/eiem_game_update_filter.h"
#include <iostream>
using namespace EiemResourceCatalogDelta;
template<class F> void Reject(F action){bool rejected=false;try{action();}catch(const std::exception&){rejected=true;}Require(rejected,"invalid catalog accepted");}
int main(){try{
 // Use the fields actually retained by the mounted VFS. A stable CHK name,
 // path, offset and length must never hide changed content in that CHK.
 EiemMountedSourceIdentity::Entry range{"D:/game/StreamingAssets/a.chk",std::string(32,'A'),std::string(32,'B'),{},17,100};
 auto asCatalog=[](const EiemMountedSourceIdentity::Entry& row){Catalog c;c.domain="mounted-range";c.entries["pfb"]={EiemMountedSourceIdentity::ContentKey(row),EiemMountedSourceIdentity::LocationKey(row),{}};return c;};
 auto changedChunk=range;changedChunk.chunkContentMd5=std::string(32,'C');
 Require(Compare(asCatalog(range),asCatalog(changedChunk)).changed==std::set<std::string>{"pfb"},"stable CHK name hid changed PFB bytes");
 auto movedChunk=range;movedChunk.physical="D:/game/Persistent/a.chk";movedChunk.chunkName=std::string(32,'D');
 Require(Compare(asCatalog(range),asCatalog(movedChunk)).relocated==std::set<std::string>{"pfb"}&&Compare(asCatalog(range),asCatalog(movedChunk)).affected.empty(),"identical CHK content in another mount forced content parsing");
 auto otherRange=range;++otherRange.offset;Require(Compare(asCatalog(range),asCatalog(otherRange)).changed.count("pfb"),"different range reused CHK content decision");
 auto exactFile=range;exactFile.fileContentMd5=std::string(32,'E');auto repacked=exactFile;++repacked.offset;repacked.chunkContentMd5=std::string(32,'F');
 Require(Compare(asCatalog(exactFile),asCatalog(repacked)).affected.empty(),"verified per-file digest could not skip repacking");
 auto encrypted=range;encrypted.encrypted=true;auto newIv=encrypted;++newIv.ivSeed;
 Require(Compare(asCatalog(encrypted),asCatalog(newIv)).changed.count("pfb"),"changed encryption IV retained source decision");
 auto placeholder=range;placeholder.fileContentMd5="MD5 not implemented";Reject([&]{asCatalog(placeholder);});
 auto overflow=range;overflow.offset=INT64_MAX;Reject([&]{asCatalog(overflow);});
 Catalog baseline;baseline.domain="game-mounted";baseline.entries={
  {"mesh",{"md5:mesh-a","chunk-a:0+40",{}}},{"material",{"md5:material-a","chunk-a:40+40",{}}},
  {"positive",{"md5:pfb-a","chunk-b:0+100",{"mesh","material"}}},
  {"negative",{"md5:pfb-b","chunk-c:0+100",{"mesh"}}},
  {"unrelated",{"md5:pfb-c","chunk-d:0+100",{}}}};
 auto persisted=Decode(Encode(baseline));auto same=Compare(persisted,baseline);Require(same.affected.empty()&&same.relocated.empty(),"unchanged directory invalidated sources");
 auto hash=[](unsigned char v){auto h=EiemDirectory::Sha(&v,1);return EiemDirectory::Hex(h.data(),h.size());};EiemConsumerUpdateRecords::Stamp stamp{hash(1),hash(2),hash(3),"exact-mesh-owner"};
 {
  using namespace EiemGameUpdateFilter;GameState current{true,hash(11),hash(12)};Coverage offline{current,hash(13),true};size_t queries=0;
  auto provider=[&]{++queries;Relevant r;r.previousCandidates={"known-positive","known-negative","changed-known","gone"};
   r.completed={{"known-positive",{stamp,{}}},{"known-negative",{stamp,{}}},{"changed-known",{stamp,{}}}};
   auto updated=stamp;updated.source=hash(19);r.candidates={{"known-positive",stamp},{"known-negative",stamp},{"changed-known",updated},{"new",stamp}};return r;};
  auto unchanged=Select(current,offline,offline.targets,provider);Require(unchanged.usedOfflineCoverage&&queries==0&&unchanged.added.empty()&&unchanged.recheck.empty(),"same-game offline export queried PFB candidates");
  auto newMesh=offline;newMesh.targets=hash(14);Require(Select(current,newMesh,newMesh.targets,provider).usedOfflineCoverage&&queries==0,"new target with current-game offline export revalidated PFBs");
  auto unknown=current;unknown.ready=false;Require(Select(unknown,offline,offline.targets,provider).waitForGame&&queries==0,"unready game catalog treated as unchanged or enumerated candidates");
  auto next=current;next.dependencies=hash(15);auto selected=Select(next,offline,offline.targets,provider);
  Require(queries==1&&selected.added==std::set<std::string>{"new"}&&selected.recheck==std::set<std::string>{"changed-known"}&&selected.retired==std::set<std::string>{"gone"},"three-layer update filter missed new edge/changed known source or repeated valid decisions");
  Reject([&]{Select(current,offline,hash(20),provider);});Require(queries==1,"unexported target edit reached discovery");
 }
 EiemConsumerUpdateRecords::Decisions completed{{"positive",{stamp,{{"a.prefab","CAB-a",10,{13}}}}},{"negative",{stamp,{}}}};
 auto candidates=[&](std::initializer_list<const char*> names){std::map<std::string,EiemConsumerUpdateRecords::Stamp> result;for(auto name:names)result.emplace(name,stamp);return result;};
 Require(Pending(candidates({"positive","negative"}),baseline,baseline,completed).empty(),"completed positive/negative repeated content check");
 Require(Pending(candidates({"positive","negative","unrelated"}),baseline,baseline,completed)==std::set<std::string>{"unrelated"},"unchanged never-examined source treated as unrelated");
 auto changedRules=candidates({"positive","negative"});changedRules.at("negative").rules=hash(8);
 Require(Pending(changedRules,baseline,baseline,completed)==std::set<std::string>{"negative"},"changed rules retained a stale negative");
 auto moved=baseline;moved.entries.at("positive").location="chunk-z:200+100";auto relocation=Compare(baseline,moved);
 Require(relocation.relocated==std::set<std::string>{"positive"}&&relocation.affected.empty()&&Pending(candidates({"positive"}),baseline,moved,completed).empty(),"location-only move forced recompilation");
 auto added=baseline;added.entries["new-pfb"]={"md5:new","chunk-n:0+100",{"mesh"}};
 Require(Pending(candidates({"positive","negative","new-pfb"}),baseline,added,completed)==std::set<std::string>{"new-pfb"},"new matching candidate was skipped");
 auto sameBundle=baseline;sameBundle.entries.at("positive").content="md5:pfb-a-with-new-root";
 Require(Pending(candidates({"positive","negative"}),baseline,sameBundle,completed)==std::set<std::string>{"positive"},"new root inside existing Bundle skipped");
 auto meshUpdate=baseline;meshUpdate.entries.at("mesh").content="md5:mesh-b";
 Require(Pending(candidates({"positive","negative"}),baseline,meshUpdate,completed)==std::set<std::string>{"positive","negative"},"changed target dependency retained a stale negative");
 auto rebound=baseline;rebound.entries.at("unrelated").dependencies.insert("mesh");Require(Compare(baseline,rebound).affected==std::set<std::string>{"unrelated"},"new dependency edge not detected");
 auto removed=baseline;removed.entries.erase("material");removed.entries.at("positive").dependencies.erase("material");auto removal=Compare(baseline,removed);
 Require(removal.removed.count("material")&&removal.affected.count("positive"),"removed dependency did not invalidate old consumers");
 auto mounted=baseline;mounted.domain="new-resource-domain";Require(Compare(baseline,mounted).changed.size()==baseline.entries.size(),"changed resource domain reused old identities");
 auto bad=baseline;bad.entries.at("positive").dependencies.insert("missing");Reject([&]{Compare(baseline,bad);});
 std::cout<<"PASS unchanged mounted catalog, completed/unknown decisions, location-only relocation, new source, same-Bundle new root, changed/removed dependencies, new edges and resource-domain invalidation; no PFB reads\n";return 0;
}catch(const std::exception& error){std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}}

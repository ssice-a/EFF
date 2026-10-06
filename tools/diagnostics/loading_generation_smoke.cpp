#include <functional>
#include <iostream>
#include "eiem_loading_generation.h"
using namespace EiemLoadingGeneration;
static void Rejects(const std::function<void()>& call, const char* reason) {
  bool rejected=false; try { call(); } catch (const std::exception&) { rejected=true; }
  Require(rejected, reason);
}
int main() {
  try {
    Names names;
    std::string old="CAB-11111111111111111111111111111111", next="CAB-22222222222222222222222222222222";
    names.members={{old,next},{old+".resS",next+".resS"}};
    auto path="archive:/"+old+"/"+old+".resS";
    auto mapped="archive:/"+next+"/"+next+".resS";
    Require(RemapPath(path,names)==mapped,"archive identity map failed");
    std::string builtin="unity default resources";
    Require(RemapPath(builtin,names)==builtin,"shared builtin reference rewritten");
    Rejects([&]{RemapPath("arbitrary:/"+old+".resS",names);},"unverified path grammar accepted");
    Bytes bytes{1,2,3,4}; bytes.insert(bytes.end(),path.begin(),path.end()); bytes.push_back(5);
    std::vector<Patch> patches; AddPatch(patches,bytes,4,path,mapped);
    auto out=Apply(bytes,patches,names);
    auto digest=Sha(bytes.data(),bytes.size());auto hash=Hex(digest.data(),digest.size());
    Require(ApplyProven(bytes,hash,patches)==out,"offline-proven recipe differs from full schema-audited namespace output");
    Rejects([&]{auto changed=bytes;changed[0]^=1;ApplyProven(changed,hash,patches);},"changed source reused offline namespace proof");
    Rejects([&]{auto changed=patches;changed.front().offset=SIZE_MAX;ApplyProven(bytes,hash,changed);},"namespace proof offset overflow accepted");
    Rejects([&]{auto changed=patches;changed.front().before[0]^=1;ApplyProven(bytes,hash,changed);},"namespace proof wrong original field accepted");
    Rejects([&]{auto changed=patches;changed.push_back(changed.front());ApplyProven(bytes,hash,changed);},"overlapping namespace proof accepted");
    Require(out.size()==bytes.size() && std::equal(bytes.begin(),bytes.begin()+4,out.begin()) && out.back()==5,
            "namespace patch changed bytes outside field");
    Require(std::string(out.begin()+4,out.end()-1)==mapped,"namespace field not remapped");
    Rejects([&]{Apply(bytes,{},names);},"hidden candidate CAB occurrence accepted");
    // A covered first occurrence cannot hide an unsupported later occurrence.
    auto repeated=bytes;repeated.insert(repeated.end(),old.begin(),old.end());
    Rejects([&]{Apply(repeated,patches,names);},"later hidden candidate CAB occurrence accepted");
    std::string oldBundle="main/333333333333333333333333.ab", newBundle="main/444444444444444444444444.ab";
    names.bundles={{oldBundle,newBundle}};
    Bytes bundleBytes(oldBundle.begin(),oldBundle.end());std::vector<Patch> bundlePatches;
    AddPatch(bundlePatches,bundleBytes,0,oldBundle,newBundle);
    Require(Apply(bundleBytes,bundlePatches,names)==Bytes(newBundle.begin(),newBundle.end()),"current Bundle name scan failed");
    bundleBytes.insert(bundleBytes.end(),oldBundle.begin(),oldBundle.end());
    Rejects([&]{Apply(bundleBytes,bundlePatches,names);},"later hidden Bundle occurrence accepted");
    names.bundles={{"fixture-name","fixture-next"}};
    Bytes fixture{'f','i','x','t','u','r','e','-','n','a','m','e'};std::vector<Patch> fixturePatches;
    AddPatch(fixturePatches,fixture,0,"fixture-name","fixture-next");
    Require(Apply(fixture,fixturePatches,names)==Bytes({'f','i','x','t','u','r','e','-','n','e','x','t'}),"generic Bundle fallback changed");
    Rejects([&]{Apply(fixture,{},names);},"generic hidden Bundle identity accepted");
    names.bundles.clear();
    Rejects([&]{auto copy=patches;copy.push_back(copy.front());Apply(bytes,copy,names);},"overlapping patches accepted");
    Rejects([&]{std::vector<Patch> p;AddPatch(p,bytes,4,"wrong",next);},"wrong source/width accepted");
    auto first=Token("key-A",old,32), same=Token("key-A",old,32), second=Token("key-B",old,32);
    Require(first==same && first!=second && Cab("CAB-"+first),"generation token scope differs");
    for (const auto& identity : {oldBundle, std::string("cutscene/variant.shaderbundle"), std::string(96, 'x')}) {
      auto renamed=BundleIdentity("key-A",identity);
      Require(renamed.size()==identity.size()&&renamed!=identity&&renamed==BundleIdentity("key-A",identity),"Bundle field namespace depends on main path grammar");
      Names generic;generic.bundles.emplace(identity,renamed);Bytes source(identity.begin(),identity.end());std::vector<Patch> edits;
      AddPatch(edits,source,0,identity,renamed);
      Require(Apply(source,edits,generic)==Bytes(renamed.begin(),renamed.end()),"generic Bundle field patch differs");
    }
    std::cout << "LOADING-GENERATION-SMOKE archivePathRemap=1 sharedBuiltinsPreserved=1 outsideFieldBytesPreserved=1 hiddenIdentityRejected=1 laterHiddenCABAndBundleRejected=1 genericFallbackPreserved=1 overlapRejected=1 independentKeys=1\n";
    return 0;
  } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}

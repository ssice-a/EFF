// Read the installed compiled Mod's exact Prefab and Renderer identities.
// Does not rebuild, materialize game inputs, or modify any Mod/game file.
#include "../../src/eiem_compiled_mod.h"
#include <iostream>
using namespace EiemCompiledMod;
std::string Quote(const std::string& value){
 std::string out="\"";for(unsigned char c:value){if(c=='"'||c=='\\'){out+='\\';out+=char(c);}else if(c<32){static const char hex[]="0123456789ABCDEF";out+="\\u00";out+=hex[c>>4];out+=hex[c&15];}else out+=char(c);}return out+'"';
}
int wmain(int argc,wchar_t** argv){try{
 Require(argc==3,"inspect_compiled_prefabs <Mod> <game>");
 auto mod=std::filesystem::canonical(argv[1]),game=std::filesystem::canonical(argv[2]);
 auto artifact=DecodeRuntime(EiemDirectory::Read(mod/"compiled.bin"),mod,game);const auto& plan=artifact.manifest.plan;
 std::cout<<"{\"mod\":"<<Quote(artifact.manifest.package.id)<<",\"compiledSHA256\":"<<Quote(artifact.digest)<<",\"inputCount\":"<<plan.inputs.size()<<",\"prefabs\":[";
 bool firstRoot=true;for(const auto& root:plan.roots){
  if(!firstRoot)std::cout<<',';firstRoot=false;const auto& input=plan.inputs.at(root.input);const auto& original=plan.originals.at(root.input);
  std::cout<<"{\"path\":"<<Quote(root.path)<<",\"cab\":"<<Quote(root.cab)<<",\"pathId\":"<<Quote(std::to_string(root.source))<<",\"input\":"<<root.input<<",\"inputFile\":"<<Quote(input.file)<<",\"sourceFile\":"<<Quote(original.file)<<",\"sourceHash\":"<<Quote(original.hash)<<",\"bundle\":"<<Quote(input.bundle)<<",\"gameFile\":"<<Quote(original.physical.u8string())<<",\"offset\":"<<original.offset<<",\"bytes\":"<<original.size<<",\"renderers\":[";
  bool firstSource=true;for(const auto& source:root.Assembly().sources){
   if(!firstSource)std::cout<<',';firstSource=false;
   auto rule=std::find_if(plan.rendererRules.at(root.input).begin(),plan.rendererRules.at(root.input).end(),[&](const auto& r){return r.cab==source.cab&&r.id==source.id;});
   auto declared=rule!=plan.rendererRules.at(root.input).end();
   std::cout<<"{\"pathId\":"<<Quote(std::to_string(source.id))<<",\"address\":"<<Quote(source.renderer.path)<<",\"component\":"<<source.renderer.component<<",\"declaredReplacement\":"<<(declared?"true":"false")<<",\"meshPath\":"<<Quote(declared?rule->mesh.path:std::string{})<<",\"meshAsset\":"<<Quote(declared?rule->mesh.asset:std::string{})<<",\"targets\":[";
   bool firstTarget=true;for(const auto& target:source.targets){if(!firstTarget)std::cout<<',';firstTarget=false;std::cout<<"{\"address\":"<<Quote(target.renderer.path)<<",\"component\":"<<target.renderer.component<<",\"marker\":"<<Quote(target.marker)<<",\"bones\":"<<target.Bones().size()<<'}';}
   std::cout<<"]}";
  }
  std::cout<<"]}";
 }
 std::cout<<"],\"gameFilesModified\":false,\"offlinePrefabIdentitiesOnly\":true}\n";return 0;
}catch(const std::exception& error){std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}}

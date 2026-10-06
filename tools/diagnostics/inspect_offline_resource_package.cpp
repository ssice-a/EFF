#include "../../src/eiem_prefab_resource_rules.h"
#include <iostream>
#include <chrono>
using namespace EiemOfflinePackage;
int wmain(int argc,wchar_t** argv){try{
 Require(argc>=3&&argc<=6,"inspect_offline_resource_package <cache> <author or mods> [--replay] [--compare-audit] [--compression]");bool replay=false,compare=false,compression=false;
 for(int i=3;i<argc;++i){auto option=std::wstring(argv[i]);Require(option==L"--replay"||option==L"--compare-audit"||option==L"--compression","unknown inspection option");if(option==L"--compare-audit")compare=true;if(option==L"--compression")compression=true;replay=true;}
 auto start=std::chrono::steady_clock::now();auto elapsed=[](auto clock){return std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-clock).count();};auto cache=std::filesystem::path(argv[1]),author=std::filesystem::path(argv[2]);
 auto pointer=ReadCurrent(cache);auto manifest=ReadManifest(cache,pointer,std::filesystem::exists(author/"mod.ini")?author.parent_path():author);auto readMs=elapsed(start);
 auto package=manifest.composed?EiemComposedPackage::Compose(EiemComposedPackage::LoadSet(author),true):Load(author);Require(PackageDigest(package)==manifest.plan.author,"compiled source author differs");
 Writer original,compiled;WriteAuthor(original,package);WriteAuthor(compiled,manifest.package);Require(original.b==compiled.b,"compiled author metadata/payload differs from strict author parser");
 auto mods=manifest.composed?author:author.parent_path();auto canonical=Encode(manifest);Require(Encode(Decode(canonical,manifest.plan.directory,mods))==canonical,"compiled descriptor canonical roundtrip differs");
 size_t inputBytes=0;if(!manifest.plan.rulesOnly)for(const auto& input:manifest.plan.inputs){auto bytes=Read(input.path);Require(Hash(bytes)==input.hash,"compiled input SHA differs");inputBytes+=bytes.size();}
 if(replay){Require(manifest.plan.rulesOnly,"replay requires reference rules");auto plan=std::make_shared<Plan>(manifest.plan);start=std::chrono::steady_clock::now();EiemPrefabRules::Context context(plan,manifest.package);auto indexMs=elapsed(start);
  std::unique_ptr<EiemPrefabRules::Context> audited;auto auditPlan=std::make_shared<Plan>(*plan);auditPlan->namespaceRecipes.clear();auditPlan->spanRecipes=false;auditPlan->memberRecipes.clear();auditPlan->frameRecipes.clear();if(compare)audited=std::make_unique<EiemPrefabRules::Context>(auditPlan,manifest.package);
  double replayMs=0,auditMs=0;size_t recipes=0,patches=0;for(const auto& input:plan->namespaceRecipes)for(const auto& recipe:input){++recipes;patches+=recipe.patches.size();}
  uint64_t spanBytes[4]{},spanCount[4]{},compressedBytes=0;std::vector<std::pair<size_t,std::string>> literals;
  for(const auto& input:plan->memberRecipes)for(const auto& member:input){size_t literal=0;for(const auto& span:member.spans){spanBytes[span.kind]+=span.size;++spanCount[span.kind];if(span.kind==EiemResourceRecipe::Literal)literal+=span.size;}literals.emplace_back(literal,member.name);}
  for(size_t i=0;i<plan->inputs.size();++i){start=std::chrono::steady_clock::now();auto bytes=context.Materialize(i,true);replayMs+=elapsed(start);inputBytes+=bytes.size();if(audited){start=std::chrono::steady_clock::now();auto reference=audited->Materialize(i,true);auditMs+=elapsed(start);Require(bytes==reference,"proven namespace recipe differs from audited builder");}
   if(compression){COMPRESSOR_HANDLE compressor=nullptr;Require(CreateCompressor(COMPRESS_ALGORITHM_XPRESS_HUFF,nullptr,&compressor)!=0,"input compression unavailable");struct Close{COMPRESSOR_HANDLE handle;~Close(){CloseCompressor(handle);}}close{compressor};SIZE_T size=0;Compress(compressor,bytes.data(),bytes.size(),nullptr,0,&size);Require(size&&GetLastError()==ERROR_INSUFFICIENT_BUFFER,"input compressed extent absent");Bytes packed(size);Require(Compress(compressor,bytes.data(),bytes.size(),packed.data(),packed.size(),&size)!=0,"input compression failed");compressedBytes+=size;}
   if((i+1)%16==0||i+1==plan->inputs.size())std::cout<<"REPLAY inputs="<<i+1<<'/'<<plan->inputs.size()<<" elapsedMs="<<replayMs<<std::endl;
  }
  std::cout<<"PASS in-memory rules replay: indexMs="<<indexMs<<" materializeMs="<<replayMs<<" auditedMaterializeMs="<<auditMs<<" serializedRecipes="<<recipes<<" namespacePatches="<<patches<<" auditedByteEquality="<<int(compare)<<" persistedNativeInputs=0\n";
  for(unsigned kind=0;kind<4;++kind)std::cout<<"RECIPE kind="<<kind<<" spans="<<spanCount[kind]<<" bytes="<<spanBytes[kind]<<'\n';
  std::sort(literals.rbegin(),literals.rend());for(size_t i=0;i<std::min<size_t>(5,literals.size());++i)std::cout<<"RECIPE-LITERALS member="<<literals[i].second<<" bytes="<<literals[i].first<<'\n';
  if(compression)std::cout<<"COMPRESSION originalBytes="<<inputBytes<<" xpressHuffmanBytes="<<compressedBytes<<" nativeLoaderFormat=0 persistedFiles=0\n";
 }
 uint64_t authorBytes=0;for(const auto& resource:package.resources)authorBytes+=resource.second->bytes.size();std::cout<<"PASS compiled manifest: inputs="<<manifest.plan.inputs.size()<<" bytes="<<inputBytes<<" authorBytes="<<authorBytes<<" descriptorBytes="<<canonical.size()<<" descriptorReadMs="<<readMs<<" meshes="<<package.meshes.size()<<" rendererParts="<<manifest.plan.rendererParts<<" sourceAuthorBytesAndMetadataEqual=1 nativeAcceptance=0\n";
 }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}

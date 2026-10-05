#include "../../src/eiem_offline_resource_package.h"
#include <iostream>
#include <chrono>
using namespace EiemOfflinePackage;
int wmain(int argc,wchar_t** argv){try{
 Require(argc==3,"inspect_offline_resource_package <cache> <author>");auto start=std::chrono::steady_clock::now();auto cache=std::filesystem::path(argv[1]),author=std::filesystem::path(argv[2]);
 auto pointer=ReadCurrent(cache);auto manifest=ReadManifest(cache,pointer,author.parent_path());auto readMs=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
 auto package=Load(author);Require(PackageDigest(package)==manifest.plan.author,"compiled source author differs");
 Writer original,compiled;WriteAuthor(original,package);WriteAuthor(compiled,manifest.package);Require(original.b==compiled.b,"compiled author metadata/payload differs from strict author parser");
 Require(Encode(manifest)==Read(manifest.plan.directory/"manifest.bin",manifestLimit),"compiled descriptor roundtrip differs");
 size_t inputBytes=0;for(const auto& input:manifest.plan.inputs){auto bytes=Read(input.path,inputLimit);Require(Hash(bytes)==input.hash,"compiled input SHA differs");inputBytes+=bytes.size();}
 std::cout<<"PASS compiled manifest: inputs="<<manifest.plan.inputs.size()<<" bytes="<<inputBytes<<" descriptorReadMs="<<readMs<<" meshes="<<package.meshes.size()<<" rendererParts="<<manifest.plan.rendererParts<<" sourceAuthorBytesAndMetadataEqual=1 offlineTypeTreeParsedAtLoad=0 nativeAcceptance=0\n";
 }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}

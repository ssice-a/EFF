#define NOMINMAX
#include "../../src/eiem_native_random_access.h"
#include "../../src/eiem_renderer_input_builder.h"
#include "../../src/eiem_vfs_catalog.h"
#include "../../src/eiem_demand_native_assembly.h"
#include "../eff_author_sources.h"
#include <iostream>

using namespace EiemNativeInput;
namespace R=EiemNativeRandomAccess;
int wmain(int argc,wchar_t** argv){try{
  Require(argc==3||argc==5,"story_prefab_read_smoke <game> <logical Bundle> [asset-index Mod]");
  const auto logical=std::filesystem::path(argv[2]).generic_u8string();
  const auto entries=EiemVfsCatalog::ReadEffectiveVfs(argv[1],{logical});
  const auto& entry=entries.at(logical);Require(!entry.encrypted,"fixture requires native unencrypted VFS input");
  auto file=std::make_shared<R::File>(entry.physical,entry.offset);
  R::Limits limits;limits.blockBytes=8*1024*1024;limits.metadataBytes=8*1024*1024;
  limits.fieldBytes=4*1024*1024;limits.cacheBytes=1024*1024;limits.readBytes=32*1024*1024;
  R::Reader reader(size_t(entry.size),[file](size_t at,unsigned char* out,size_t n){file->ReadAt(at,out,n);},limits);
  try{for(const auto& member:reader.directory.members){if(!(member.flags&4))continue;
    auto layout=reader.Layout(member);Package package;package.id="story-read-fixture";AssetIndex assets;
    for(auto id:layout.order){const auto& type=layout.types.at(layout.objects.at(id).type);
      if(type.classId!=137&&type.classId!=33)continue;
      auto value=reader.Project(member,layout,id,{"m_Mesh","m_GameObject"});
      auto mesh=NullableReference(type,Child(type,value,"m_Mesh"),layout,member.name);if(!mesh.second)continue;
      Identity identity{"fixture/"+std::to_string(id),"mesh"};const auto rule=std::to_string(id);
      bool duplicate=false;for(const auto& row:assets)if(CanonicalSerializedAssetReference({row.second.cab,row.second.pathId})==CanonicalSerializedAssetReference(mesh))duplicate=true;
      if(duplicate)continue;package.targetIndex[identity]=rule;package.rules[rule].skip=true;
      auto& asset=assets[identity];asset.cab=mesh.first;asset.pathId=mesh.second;asset.classId=43;
    }
    std::cout<<"FIXTURE source="<<logical<<" physical="<<entry.physical.u8string()<<" offset="<<entry.offset<<" inputBytes="<<entry.size
      <<" decodedBytes="<<reader.directory.decoded<<" cab="<<member.name<<" objects="<<layout.objects.size()<<" meshReferences="<<assets.size()<<" beforeReadBytes="<<reader.stats.encodedBytes<<std::endl;
    if(argc==5){
      namespace N=EiemDemandNativeAssembly;
      std::vector<std::shared_ptr<N::Author>> authors;std::filesystem::path modRoot=argv[4];
      if(std::filesystem::is_regular_file(modRoot/L"mod.ini"))authors.push_back(std::make_shared<N::Author>(std::make_shared<EiemDemandAuthorResources::Catalog>(modRoot)));
      else for(const auto& mod:std::filesystem::directory_iterator(modRoot))if(mod.is_directory()&&std::filesystem::is_regular_file(mod.path()/L"mod.ini"))
        authors.push_back(std::make_shared<N::Author>(std::make_shared<EiemDemandAuthorResources::Catalog>(mod.path())));
      auto composed=N::Author::Compose(authors);auto& author=*composed;
      auto index=EiemAuthorSources::ReadIndex(argv[3],EiemAuthorSources::Collect(author.package));
      std::set<NativeReference> wanted;for(const auto& row:assets)wanted.insert(CanonicalSerializedAssetReference({row.second.cab,row.second.pathId}));
      std::set<std::string> sourceNames;
      for(const auto& row:index.assets)if(row.kind=="Mesh"&&author.package.targetIndex.count({row.path,row.name}))sourceNames.insert(row.source);
      auto sources=EiemVfsCatalog::ReadEffectiveVfs(argv[1],sourceNames);AssetIndex targets;std::vector<BuiltMeshPart> parts;
      for(const auto& source:sources){auto carrierFile=std::make_shared<R::File>(source.second.physical,source.second.offset);
        R::Reader carrier(size_t(source.second.size),[carrierFile](size_t at,unsigned char* out,size_t n){carrierFile->ReadAt(at,out,n);},limits);
        for(size_t mi=0;mi<carrier.directory.members.size();++mi){const auto& m=carrier.directory.members[mi];if(!(m.flags&4))continue;
          // CAB filtering is fixture setup only. The actual assembly resolves
          // current serialized assets and uses the production sparse builder.
          bool used=false;for(const auto& ref:wanted)used|=ref.first==SerializedAssetCabKey(m.name);if(!used)continue;
          auto nativeLayout=carrier.Layout(m);AssetIndex selected;
          for(const auto& asset:carrier.Assets(m,nativeLayout))if(asset.classId==43&&author.package.targetIndex.count({asset.path,asset.asset})&&
              wanted.count(CanonicalSerializedAssetReference({asset.cab,asset.pathId})))selected.emplace(Identity{asset.path,asset.asset},asset);
          if(selected.empty())continue;targets.insert(selected.begin(),selected.end());
          auto prepared=N::PrepareCarrier(carrier,mi,nativeLayout,author,selected);
          parts.insert(parts.end(),prepared.parts.begin(),prepared.parts.end());
        }
      }
      Require(!targets.empty()&&!parts.empty(),"installed Mod has no exact target in this story fixture");
      N::EnsureStagedMeshMetadata(author,parts);auto started=std::chrono::steady_clock::now();
      auto prepared=N::PrepareConsumer(reader,size_t(&member-reader.directory.members.data()),layout,author,targets,parts);
      Require(prepared.result.matches,"installed Mod consumer assembly did not hit this story");
      auto output=EiemNativeSpliceView::BuildRanges(reader,{prepared.plan.rewrite},32*1024*1024);
      std::cout<<"PASS installed Mod story consumer mod="<<author.package.id<<" targets="<<targets.size()<<" parts="<<parts.size()
        <<" matches="<<prepared.result.matches<<" changedObjects="<<prepared.result.changedObjects<<" addedObjects="<<prepared.result.addedObjects
        <<" readBytes="<<reader.stats.encodedBytes<<" inflatedBytes="<<reader.stats.inflatedBytes<<" cachePeak="<<reader.stats.cachePeak
        <<" prepareMs="<<std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count()<<std::endl;
      continue;
    }
    const std::vector<BuiltMeshPart> parts;
    RendererMemberBuilder builder(layout,member.name,package,assets,parts,
      [&](int64_t id,const std::set<std::string>& fields){return reader.Project(member,layout,id,fields);},
      [&](int64_t id){return reader.Object(member,layout,id);});
    builder.SetCost([&](const char* stage,double ms){if(std::string(stage)=="renderer-object-decode")return;
      std::cout<<"STAGE name="<<stage<<" ms="<<ms<<" readBytes="<<reader.stats.encodedBytes<<" inflatedBytes="<<reader.stats.inflatedBytes<<std::endl;});
    auto result=builder.Build();Require(result.matches,"story fixture has no native renderer consumers");
    std::cout<<"PASS story native renderer assembly matches="<<result.matches<<" changedObjects="<<result.changedObjects
      <<" readBytes="<<reader.stats.encodedBytes<<" inflatedBytes="<<reader.stats.inflatedBytes
      <<" cachePeak="<<reader.stats.cachePeak<<" pinnedBytes="<<reader.stats.pinnedBytes<<std::endl;
  }}catch(...){std::cerr<<"READ-STATS bytes="<<reader.stats.encodedBytes<<" reads="<<reader.stats.reads
    <<" inflatedBytes="<<reader.stats.inflatedBytes<<" inflatedBlocks="<<reader.stats.inflatedBlocks<<" cachePeak="<<reader.stats.cachePeak<<std::endl;throw;}
  file->VerifyStable();return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<std::endl;return 1;}}

#pragma once
#include "eiem_compiled_mod_store.h"

// A published Mod can be an older immutable packet or the current block store.
// Keep both entry points behind the same validated reader; author-side updates
// additionally retain the store generation and prepared-source metadata.
namespace EiemPublishedCompiledMod {
using namespace EiemCompiledMod;
struct Loaded : EiemCompiledModStore::Loaded {bool store=false;};
inline Loaded Read(const std::filesystem::path& root,const std::filesystem::path& game,
                   const std::function<void(const char*,double)>& cost={},bool runtime=true,bool consumerSnapshot=false){
 Loaded result;result.store=EiemCompiledModStore::IsStore(root/"compiled.bin");
 if(!result.store){result.artifact=runtime?EiemCompiledMod::ReadRuntime(root,game,cost):EiemCompiledMod::Read(root,game,cost);return result;}
 auto begin=std::chrono::steady_clock::now();
 static_cast<EiemCompiledModStore::Loaded&>(result)=EiemCompiledModStore::Load(root/"compiled.bin",root,game,runtime,cost,consumerSnapshot);
 // Unsupported hard links/read-only cache paths retain the existing direct,
 // delete-shared reader; never copy a potentially very large compiled file.
 if(cost&&!result.externalSnapshot)cost("artifact-store-direct-pin",0);
 if(cost)cost("artifact-store",std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count());
 return result;
}
inline Artifact ReadArtifact(const std::filesystem::path& root,const std::filesystem::path& game,
                             const std::function<void(const char*,double)>& cost={},bool runtime=true,bool consumerSnapshot=false){
 auto loaded=Read(root,game,cost,runtime,consumerSnapshot);return std::move(loaded.artifact);
}
inline Artifact ReadFullArtifact(const std::filesystem::path& root,const std::filesystem::path& game,
                                 const std::function<void(const char*,double)>& cost={}){
 return ReadArtifact(root,game,cost,false);
}
} // namespace EiemPublishedCompiledMod

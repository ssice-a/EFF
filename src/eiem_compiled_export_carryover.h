#pragma once
#include "eiem_compiled_mod_store.h"

// Previous published discoveries are build inputs, not an optional cache.
// Export never infers a logical source from a Bundle label or a hashed file ID.
namespace EiemCompiledExportCarryover {
using namespace EiemCompiledMod;
inline bool RevisionsChanged(const EiemStaticSources::Selection& current,const EiemStaticSources::Selection& previous){
 for(const auto& row:previous.inputs){auto found=current.inputs.find(row.first);if(found==current.inputs.end()||found->second.revision!=row.second.revision)return true;}return false;
}
inline EiemStaticSources::Selection Merge(const EiemStaticSources::Selection& current,const EiemStaticSources::Selection& previous,const Artifact& published,bool currentSelectionComplete=false){
 EiemStaticSources::Validate(current);EiemStaticSources::Validate(previous);EiemCompiledModStore::VerifySourceSelection(published,previous);
 // Only a complete index verified against today's installed VFS can retire
 // or supersede old discoveries. A partial selection still cannot resolve it.
 if(currentSelectionComplete&&RevisionsChanged(current,previous))return current;
 auto result=current;
 for(const auto& row:previous.inputs){auto found=result.inputs.find(row.first);
  if(found==result.inputs.end())result.inputs.emplace(row);
  else if(found->second.revision!=row.second.revision){
   // A directory seal adapter must resolve which mount/version is current.
   // Neither two readable ranges nor an old eidx proves mount precedence.
   Require(false,"export carryover has conflicting game revisions; resolve current mounted source: "+row.first);
  }
 }
 for(const auto& row:previous.assets)result.assets.emplace(row);
 for(const auto& row:previous.dependencies)if(!current.inputs.count(row.first))result.dependencies[row.first]=row.second;
 result.candidates.insert(previous.candidates.begin(),previous.candidates.end());
 // Original compiled sources include dependencies as well as positive PFBs.
 // Verify inherited ranges before accepting a previous artifact fast path.
 std::set<std::string> oldSources;for(const auto& source:published.manifest.plan.originals)oldSources.insert(source.file);
 for(const auto& row:previous.inputs){auto identity=row.first+'\n'+row.second.revision;auto file=Hash(Bytes(identity.begin(),identity.end()))+".vfs";
  if(oldSources.count(file)&&!current.inputs.count(row.first))EiemStaticSources::Payload(row.second);
 }
 EiemStaticSources::Validate(result);return result;
}
inline EiemResourceCatalogDelta::Catalog Catalog(const EiemStaticSources::Selection& selection){
 EiemResourceCatalogDelta::Catalog catalog;catalog.domain="verified-export-source-selection";
 for(const auto& row:selection.inputs){auto dependencies=selection.dependencies.find(row.first);catalog.entries.emplace(row.first,EiemResourceCatalogDelta::Entry{row.second.revision,row.second.physical.generic_u8string()+":"+std::to_string(row.second.offset)+":"+std::to_string(row.second.size),dependencies==selection.dependencies.end()?std::set<std::string>{}:dependencies->second});}return catalog;
}
inline void RebindSources(Artifact& artifact,const EiemStaticSources::Selection& selection,bool allowRetired=false){
 std::map<std::string,const EiemStaticSources::Entry*> identities;for(const auto& row:selection.inputs){auto identity=row.first+'\n'+row.second.revision;identities.emplace(Hash(Bytes(identity.begin(),identity.end()))+".vfs",&row.second);}
 bool retained=artifact.verifiedSources.size()==artifact.manifest.plan.originals.size();
 for(auto& source:artifact.manifest.plan.originals){auto found=identities.find(source.file);if(found==identities.end()){Require(allowRetired,"inherited source missing from current selection");retained=false;continue;}const auto& entry=*found->second;Require(source.hash==entry.revision&&source.size==entry.size,"inherited source byte identity differs");
  retained=retained&&source.physical==entry.physical&&source.offset==entry.offset;source.physical=entry.physical;source.offset=entry.offset;
 }
 // Binding an identical range must retain its verified stamp. Clearing every
 // stamp disabled the parameter-only compiler on every ordinary author export.
 // Selective builders still compare these stamps against the current files.
 if(!retained)artifact.verifiedSources.clear();
}
inline void CopySnapshot(const std::filesystem::path& source,const std::filesystem::path& target){
 static std::atomic<uint64_t> serial{0};auto temporary=target;temporary+=L".carryover-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(serial.fetch_add(1));
 struct Cleanup{std::filesystem::path path;~Cleanup(){std::error_code error;std::filesystem::remove(path,error);}}cleanup{temporary};
 Require(std::filesystem::copy_file(source,temporary),"export snapshot copy failed");auto replaced=std::filesystem::exists(target)?ReplaceFileW(target.c_str(),temporary.c_str(),nullptr,0,nullptr,nullptr):MoveFileExW(temporary.c_str(),target.c_str(),MOVEFILE_WRITE_THROUGH);
 Require(replaced!=0,"export snapshot copy publication failed");
}
} // namespace EiemCompiledExportCarryover

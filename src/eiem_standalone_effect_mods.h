#pragma once
// Included by the host after full model lifecycle bindings. No Unity on worker.
namespace StandaloneEffects {
struct PublishReadLock {
 HANDLE handle=nullptr;
 explicit PublishReadLock(const std::filesystem::path& directory,bool character=false){
  auto path=EiemNativeInput::InputPath(std::filesystem::weakly_canonical(std::filesystem::absolute(directory)).u8string());while(!path.empty()&&path.back()=='/')path.pop_back();if(character)std::transform(path.begin(),path.end(),path.begin(),[](unsigned char c){return char(std::tolower(c));});auto hash=Sha(reinterpret_cast<const unsigned char*>(path.data()),path.size());auto token=Hex(hash.data(),hash.size());auto name=std::wstring(character?L"Local\\EFFModBlocks-":L"Local\\EFFSource-")+std::wstring(token.begin(),token.end());
  handle=CreateMutexW(nullptr,FALSE,name.c_str());Require(handle,"cannot open effect publish lock");auto result=WaitForSingleObject(handle,0);if(result!=WAIT_OBJECT_0&&result!=WAIT_ABANDONED){CloseHandle(handle);handle=nullptr;throw std::runtime_error("effect publication is in progress; retry F9 after export");}
 }
 ~PublishReadLock(){if(handle){ReleaseMutex(handle);CloseHandle(handle);}}
 PublishReadLock(const PublishReadLock&)=delete;PublishReadLock& operator=(const PublishReadLock&)=delete;
};
inline bool IsStandalone(const std::filesystem::path& root){
 return EiemComposedPackage::IsEffectRoot(root);
}
// F9 reads only effect containers and their editable control declarations.
// Mesh/material/texture payloads stay in the already published model graph.
inline std::map<std::string,EffectAttachment> ReadAttached(const Package& published){
 PublishReadLock publication(published.root/"compiled.bin",true);
 auto ini=EiemDirectory::Read(published.root/"mod.ini");auto sections=ParseIni(ini);Package next;next.root=published.root;next.parts=published.parts;bool identity=false;
 for(const auto& s:sections){
  if(s.id=="Mod"){Keys(s,{"format","id"},{"key_switch_enabled","type"});Require(s.values.at("id")==published.id&&!s.values.count("type"),"effect reload Mod identity/type changed");next.id=published.id;identity=true;}
  else if(s.id=="Constants")for(const auto& row:s.values){auto variable=Begins(row.first,"persist ")?row.first.substr(8):row.first;Require(EiemVariableName(variable)&&next.defaults.emplace(variable,Number(row.second)).second,"effect reload invalid/duplicate state");}
 }
 Require(identity,"effect reload Mod identity absent");
 for(const auto& s:sections){
  if(Begins(s.id,"ShapeControl")){Keys(s,{"variable","label","min","max"});auto variable=s.values.at("variable");Control control{s.values.at("label"),Number(s.values.at("min")),Number(s.values.at("max"))};Require(next.controls.emplace(variable,control).second,"effect reload duplicate control");}
  else if(Begins(s.id,"EffectAttachment")){Keys(s,{"path"});auto path=s.values.at("path");auto resource=std::make_shared<Resource>(Decode(EiemDirectory::Read(DiskPath(published.root,path,10)),10));auto decoded=DecodeEffectAttachment(*resource,resource);Require(decoded.first==next.id&&s.id=="EffectAttachment"+decoded.second.id&&next.resources.emplace(path,std::move(resource)).second,"effect reload declaration identity/alias differs");}
 }
 RestoreRuntimeControls(next);Require(EiemDirectory::Read(published.root/"mod.ini")==ini,"effect declarations changed during reload; retry F9");return std::move(next.effects);
}
inline std::vector<Package> ReadAll(const std::filesystem::path& game){
 std::vector<Package> result;std::set<std::string> ids;for(const auto& root:EiemComposedPackage::Roots(game/"plugin/mods",true)){
  auto declarations=EiemDirectory::ParseIni(EiemDirectory::Read(root/"mod.ini"));for(const auto& s:declarations)if(s.id=="Mod")Require(ids.insert(s.values.at("id")).second,"duplicate Mod id across character/effect Mods");
  if(!IsStandalone(root))continue;
  auto p=Load(root);Require(p.parts.empty()&&p.rules.empty()&&!p.effects.empty(),"standalone effect Mod contains character edits");
  for(const auto& e:p.effects)Require(!e.second.targetPrefab.empty()&&!e.second.bone.empty(),"standalone effect lacks source target/bone");result.push_back(std::move(p));
 }return result;
}
}

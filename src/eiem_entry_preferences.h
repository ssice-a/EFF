#pragma once
#include "eiem_resource_directory.h"
#include <mutex>
#include <iomanip>
#include <locale>

// Numeric preferences only. Unity callbacks enqueue values; the Win32 poller
// atomically flushes them to the existing state.ini contract without Unity I/O.
class EiemEntryPreferences {
  struct Entry { EiemVariables values; bool dirty=false; ULONGLONG attempted=0; std::string error,owner; };
  std::mutex mutex;
  std::map<std::filesystem::path,Entry> entries;
  static std::filesystem::path Path(const EiemDirectory::Package& p) { return p.root/L"state.ini"; }
 public:
  static EiemVariables Read(const std::filesystem::path& path,const std::string& owner={},bool requireOwner=false) {
    if (!std::filesystem::exists(path)) return {};
    std::ifstream input(path,std::ios::binary);
    EiemDirectory::Require(bool(input),"cannot read state file");
    EiemVariables values;std::string line;bool section=false,tagged=false;
    while (std::getline(input,line)) {
      auto text=EiemDirectory::Trim(line);
      const std::string tag="; EFF Mod ID: ";
      if(text.rfind(tag,0)==0) {
        EiemDirectory::Require(!tagged && (owner.empty() || text.substr(tag.size())==owner),"state belongs to another Mod ID");
        tagged=true;continue;
      }
      if (text.empty() || text[0]==';' || text[0]=='#') continue;
      if (text=="[Values]" && !section) { section=true;continue; }
      auto equals=text.find('=');
      EiemDirectory::Require(section && equals!=std::string::npos,"invalid state section/assignment");
      auto key=EiemDirectory::Trim(text.substr(0,equals)),value=EiemDirectory::Trim(text.substr(equals+1));
      EiemDirectory::Require(EiemVariableName(key) && values.emplace(key,EiemDirectory::Number(value)).second,"invalid/duplicate state value");
    }
    EiemDirectory::Require(!input.bad(),"state read failed");
    EiemDirectory::Require(!requireOwner||tagged,"untagged state cannot cross a changed Mod ID");return values;
  }
  static void Validate(const EiemDirectory::Package& p,const EiemVariables& values) {
    for (const auto& v:values) {
      EiemDirectory::Require(p.defaults.count(v.first) && std::isfinite(v.second),"undeclared/nonfinite state value");
      auto control=p.controls.find(v.first);
      if (control!=p.controls.end()) EiemDirectory::Require(v.second>=control->second.minimum && v.second<=control->second.maximum,"state exceeds shape range");
      for (const auto& key:p.bindings) if (key.variable==v.first && key.type=="cycle")
        EiemDirectory::Require(std::find(key.values.begin(),key.values.end(),v.second)!=key.values.end(),"state exceeds cycle domain");
    }
  }
  static void WriteText(const std::filesystem::path& path,const std::string& text) {
    auto tmp=path;tmp+=L".tmp-"+std::to_wstring(GetCurrentProcessId());
    HANDLE file=CreateFileW(tmp.c_str(),GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    EiemDirectory::Require(file!=INVALID_HANDLE_VALUE,"cannot create temporary settings file");
    size_t offset=0;bool ok=true;
    while(ok&&offset<text.size()){DWORD written=0,count=DWORD(std::min(text.size()-offset,size_t(1024*1024)));ok=WriteFile(file,text.data()+offset,count,&written,nullptr)&&written==count;offset+=written;}
    if(ok)ok=FlushFileBuffers(file)!=FALSE;CloseHandle(file);
    if(ok)ok=MoveFileExW(tmp.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=FALSE;
    if(!ok)DeleteFileW(tmp.c_str());EiemDirectory::Require(ok,"atomic settings save failed; previous file retained");
  }
  static void Write(const std::filesystem::path& path,const EiemVariables& values,const std::string& owner={}) {
    std::ostringstream out;out.imbue(std::locale::classic());
    out<<"; EFF runtime preferences, not Mod actions\n";
    if(!owner.empty()) out<<"; EFF Mod ID: "<<owner<<'\n';
    out<<"[Values]\n";
    for (const auto& v:std::map<std::string,double>(values.begin(),values.end())) out<<v.first<<'='<<std::setprecision(17)<<v.second<<'\n';
    WriteText(path,out.str());
  }
  // Candidate preparation must not mutate the live preference cache. A
  // failed F10 can read disk/queued numbers but cannot replace their owner.
  EiemVariables Preview(const EiemDirectory::Package& p,bool requireOwner=false) {
    auto values=p.defaults;
    if (p.persistent.empty()) return values;
    EiemVariables saved;bool queued=false;
    {std::lock_guard<std::mutex> lock(mutex);auto found=entries.find(Path(p));
      if(found!=entries.end() && found->second.owner!=p.id) requireOwner=true;
      if(found!=entries.end() && found->second.owner==p.id && found->second.dirty) {saved=found->second.values;queued=true;}}
    if(!queued) saved=Read(Path(p),p.id,requireOwner);
    EiemVariables selected;
    for (const auto& key:p.persistent) if (saved.count(key)) selected[key]=saved.at(key);
    Validate(p,selected);
    for (const auto& v:selected) values[v.first]=v.second;
    return values;
  }
  EiemVariables Load(const EiemDirectory::Package& p) {
    auto values=Preview(p);if(p.persistent.empty()) return values;
    std::lock_guard<std::mutex> lock(mutex);auto& entry=entries[Path(p)];
    if(entry.owner!=p.id) {entry=Entry{};entry.owner=p.id;}
    entry.values.clear();for(const auto& key:p.persistent) entry.values[key]=values.at(key);
    return values;
  }
  void Capture(const EiemDirectory::Package& p,const EiemVariables& values) {
    EiemVariables selected;for (const auto& key:p.persistent) selected[key]=values.at(key);
    Validate(p,selected);
    std::lock_guard<std::mutex> lock(mutex);
    if (p.persistent.empty() && !entries.count(Path(p))) return;
    auto& entry=entries[Path(p)];
    if(entry.owner!=p.id) {entry=Entry{};entry.owner=p.id;}
    if (entry.values!=selected) {entry.values=std::move(selected);entry.dirty=true;}
  }
  template<class Log> void Flush(Log log,bool force=false) {
    struct Snapshot {std::filesystem::path path;EiemVariables values;std::string owner;};
    std::vector<Snapshot> batch;
    {
      std::lock_guard<std::mutex> lock(mutex);auto now=GetTickCount64();
      for (auto& pair:entries) {
        auto& entry=pair.second;if (!entry.dirty || (!force && now-entry.attempted<250)) continue;
        entry.attempted=now;batch.push_back({pair.first,entry.values,entry.owner});
      }
    }
    // File flushes may be slow: never hold the input queue lock during I/O.
    for (const auto& snapshot:batch) {
      try {
        bool current=false;
        for(const auto& section:EiemDirectory::ParseIni(EiemDirectory::Read(snapshot.path.parent_path()/"mod.ini")))
          if(section.id=="Mod") current=section.values.at("id")==snapshot.owner;
        EiemDirectory::Require(current,"state save owner changed or Mod removed");
        {std::lock_guard<std::mutex> lock(mutex);if(entries.at(snapshot.path).owner!=snapshot.owner) continue;}
        Write(snapshot.path,snapshot.values,snapshot.owner);
        {std::lock_guard<std::mutex> lock(mutex);auto& entry=entries.at(snapshot.path);
          if(entry.owner==snapshot.owner) {if(entry.values==snapshot.values) entry.dirty=false;entry.error.clear();}}
        log("[CONTROL-STATE-SAVED] file=%s values=%zu",snapshot.path.u8string().c_str(),snapshot.values.size());
      } catch(const std::exception& e) {
        bool changed=false;
        {std::lock_guard<std::mutex> lock(mutex);auto& entry=entries.at(snapshot.path);
          if(entry.owner!=snapshot.owner) continue;
          changed=entry.error!=e.what();entry.error=e.what();}
        if (changed) log("[CONTROL-STATE-GAP] file=%s reason=%s",snapshot.path.u8string().c_str(),e.what());
      }
    }
  }
};

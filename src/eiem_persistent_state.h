#pragma once
#include "eiem_mod_document.h"
#include <filesystem>
#include <mutex>
#include <map>
#include <iomanip>

// Pure numeric preferences. Never stores an action, resource or Unity pointer.
// Existing hotkey worker flushes batches; this lock never encloses a Mod lock
// or engine callback. Reload/normal exit can force a serialized flush.
class EiemPersistentStore {
  struct Entry { EiemVariables values;bool dirty=false;ULONGLONG attempted=0;std::string error; };
  std::mutex mutex;
  std::map<std::filesystem::path,Entry> entries;
  static std::filesystem::path Path(const std::string &mod) {
    return std::filesystem::absolute(std::filesystem::u8path(mod)).lexically_normal().parent_path()/L"state.ini";
  }
  static EiemVariables Read(const std::filesystem::path &path) {
    if(!std::filesystem::exists(path))return {};
    if(std::filesystem::file_size(path)>1024*1024)throw std::runtime_error("State file exceeds 1 MiB");
    std::ifstream input(path,std::ios::binary);
    if(!input)throw std::runtime_error("Cannot read state file");
    EiemVariables values;std::string line;bool section=false;
    while(std::getline(input,line)) {
      EiemModTrim(line);if(line.empty() || line[0]==';' || line[0]=='#')continue;
      if(line=="[Values]" && !section){section=true;continue;}
      auto equals=line.find('=');std::string key=line.substr(0,equals);
      std::string value=equals==std::string::npos?"":line.substr(equals+1);
      EiemModTrim(key);EiemModTrim(value);double number=0;
      if(!section || !EiemVariableName(key) || !EiemNumber(value,&number) || !values.emplace(key,number).second)
        throw std::runtime_error("Invalid/duplicate persisted value");
    }
    if(input.bad())throw std::runtime_error("State read failed");
    return values;
  }
  static void Write(const std::filesystem::path &path,const EiemVariables &values) {
    std::ostringstream stream;stream.imbue(std::locale::classic());stream<<"; EFF runtime preferences, not Mod actions\n[Values]\n";
    std::map<std::string,double> ordered(values.begin(),values.end());
    for(const auto &v:ordered)stream<<v.first<<'='<<std::setprecision(17)<<v.second<<'\n';
    const auto text=stream.str();auto tmp=path;tmp+=L".tmp-"+std::to_wstring(GetCurrentProcessId());
    HANDLE file=CreateFileW(tmp.c_str(),GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE)throw std::runtime_error("Cannot create temporary state file");
    DWORD written=0;bool ok=WriteFile(file,text.data(),(DWORD)text.size(),&written,nullptr) && written==text.size();
    if(ok)ok=FlushFileBuffers(file)!=FALSE;CloseHandle(file);
    if(ok)ok=MoveFileExW(tmp.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=FALSE;
    if(!ok){DeleteFileW(tmp.c_str());throw std::runtime_error("Atomic state save failed; previous file retained");}
  }
 public:
  void Load(EiemModProgram &program) {
    std::lock_guard<std::mutex> lock(mutex);
    for(size_t i=0;i<program.states.size();++i) {
      auto &state=program.states[i];if(state.persistent.empty())continue;
      try {
        const auto path=Path(state.path);auto &entry=entries[path];
        // A failed flush must not replace newer accepted values with older disk data.
        if(!entry.dirty)entry.values=Read(path);
        EiemVariables selected;
        for(const auto &name:state.persistent)if(entry.values.count(name))selected[name]=entry.values.at(name);
        std::string error;
        if(!EiemApplyUiValues(program,i,selected,error))throw std::runtime_error(error);
      } catch(const std::exception &e) {Log("[STATE] Load %s: %s; using declared defaults",state.path.c_str(),e.what());}
    }
  }
  void Queue(const EiemModProgram &program) {
    std::lock_guard<std::mutex> lock(mutex);
    for(const auto &state:program.states) {
      if(state.persistent.empty())continue;
      auto &entry=entries[Path(state.path)];
      for(const auto &name:state.persistent) {
        const double value=state.variables.at(name);
        auto old=entry.values.find(name);
        if(old==entry.values.end() || old->second!=value){entry.values[name]=value;entry.dirty=true;}
      }
    }
  }
  void Flush(bool force=false) {
    std::lock_guard<std::mutex> lock(mutex);const auto now=GetTickCount64();
    for(auto &pair:entries) {
      auto &entry=pair.second;if(!entry.dirty || (!force && now-entry.attempted<250))continue;
      entry.attempted=now;
      try {Write(pair.first,entry.values);entry.dirty=false;entry.error.clear();}
      catch(const std::exception &e) {
        if(entry.error!=e.what())Log("[STATE] Save %s: %s",pair.first.u8string().c_str(),e.what());
        entry.error=e.what();
      }
    }
  }
};

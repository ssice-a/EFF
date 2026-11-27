#pragma once
#include "eiem_entry_preferences.h"

// Ins changes the same author INI field that users can edit. Only the flag
// line is patched; values, declarations, comments and line endings survive.
class EiemModKeySettings {
  struct Pending {std::string owner;bool enabled=true;uint64_t serial=0;ULONGLONG attempted=0;std::string error;};
  std::mutex mutex;std::map<std::pair<std::filesystem::path,std::string>,Pending> pending;uint64_t serial=0;
public:
  static std::string Rewrite(const EiemDirectory::Bytes& bytes,const std::string& owner,bool enabled,const std::string& field="key_switch_enabled"){
    using namespace EiemDirectory;auto sections=ParseIni(bytes);const Section* mod=nullptr;
    for(const auto& section:sections)if(section.id=="Mod")mod=&section;
    Require(mod&&mod->values.count("id")&&mod->values.at("id")==owner,"key settings INI belongs to another Mod");
    if(field=="key_switch_enabled"&&mod->values.count(field))KeySwitchEnabled(mod->values.at(field));
    auto text=Text(bytes.data(),bytes.size());size_t insert=std::string::npos;bool inMod=false;
    std::string newline=text.find("\r\n")!=std::string::npos?"\r\n":"\n";
    for(size_t start=0;start<text.size();){auto end=text.find('\n',start);auto stop=end==std::string::npos?text.size():end+1;auto line=Trim(text.substr(start,(end==std::string::npos?text.size():end)-start));
      if(start==0&&Begins(line,"\xEF\xBB\xBF"))line.erase(0,3);
      if(line=="[Mod]"){inMod=true;insert=stop;}
      else if(inMod&&!line.empty()&&line.front()=='[')inMod=false;
      else if(inMod&&!line.empty()&&line.front()!=';'&&line.front()!='#'){
        auto equals=line.find('=');if(equals!=std::string::npos&&Trim(line.substr(0,equals))==field){
          auto valueStart=text.find('=',start)+1;auto valueEnd=end==std::string::npos?text.size():end;
          while(valueEnd>valueStart&&(text[valueEnd-1]=='\r'||text[valueEnd-1]==' '||text[valueEnd-1]=='\t'))--valueEnd;
          while(valueStart<valueEnd&&(text[valueStart]==' '||text[valueStart]=='\t'))++valueStart;
          text.replace(valueStart,valueEnd-valueStart,enabled?"1":"0");return text;
        }
      }start=stop;
    }
    Require(insert!=std::string::npos,"key settings Mod section absent");
    text.insert(insert,std::string(insert&&text[insert-1]!='\n'?newline:"")+field+"="+(enabled?"1":"0")+newline);return text;
  }
  bool Preview(const EiemDirectory::Package& p,bool gui=false){
    std::lock_guard<std::mutex> lock(mutex);auto found=pending.find({p.root/"mod.ini",gui?"gui_enabled":"key_switch_enabled"});
    return found!=pending.end()&&found->second.owner==EiemDirectory::DeclaredId(p)?found->second.enabled:gui?p.customGui.enabled:p.keySwitchEnabled;
  }
  void Capture(const EiemDirectory::Package& p,bool enabled,bool gui=false){
    std::lock_guard<std::mutex> lock(mutex);pending[{p.root/"mod.ini",gui?"gui_enabled":"key_switch_enabled"}]={EiemDirectory::DeclaredId(p),enabled,++serial,0,{}};
  }
  template<class Log> void Flush(Log log,bool force=false){
    std::vector<std::pair<std::pair<std::filesystem::path,std::string>,Pending>> batch;
    {std::lock_guard<std::mutex> lock(mutex);auto now=GetTickCount64();for(auto& row:pending)if(force||now-row.second.attempted>=250){row.second.attempted=now;batch.push_back(row);}}
    for(const auto& row:batch){std::string error;
      try{const auto& [path,field]=row.first;if(std::filesystem::is_regular_file(path)){auto bytes=EiemDirectory::Read(path);auto text=Rewrite(bytes,row.second.owner,row.second.enabled,field);
        if(text!=std::string(bytes.begin(),bytes.end()))EiemEntryPreferences::WriteText(path,text);
      }}catch(const std::exception& e){error=e.what();}
      std::lock_guard<std::mutex> lock(mutex);auto found=pending.find(row.first);if(found==pending.end()||found->second.serial!=row.second.serial)continue;
      if(error.empty())pending.erase(found);else if(error!=found->second.error){found->second.error=error;log("[CONTROL-INI-SAVE-GAP] package=%s reason=%s",row.second.owner.c_str(),error.c_str());}
    }
  }
};

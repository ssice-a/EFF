// Production discovery and identity contracts with temporary author directories.
// No game, resource compilation, Blender export or managed API is invoked.
#include "../../src/eiem_resource_entry.cpp"
#include "../../src/eiem_file_io.h"
#include <iostream>
#include <functional>

using namespace Entry;
void LoggingChecks(const std::filesystem::path& root) {
  std::filesystem::create_directories(root);
  auto main=OpenLog(root/"session.log",true),mirror=OpenLog(root/"mirror.log",true);
  Require(main&&mirror,"buffered log fixture open failed");
  auto oldLog=logFile,oldMirror=logMirror;logFile=main;logMirror=mirror;
  auto bytes=[](FILE* file){LARGE_INTEGER size{};Require(GetFileSizeEx((HANDLE)_get_osfhandle(_fileno(file)),&size)!=FALSE,"log file size unavailable");return size.QuadPart;};
  auto cleanup=[&]{logFile=oldLog;logMirror=oldMirror;fclose(main);fclose(mirror);};
  try {
    Log("[ENTRY-FIXTURE] ordinary message mentions ERROR without an error tag");
    Require(bytes(main)==0&&bytes(mirror)==0,"ordinary event forced synchronous writes instead of bounded buffering");
    Log("[DEMAND-LIFETIME-GAP] critical fixture evidence");
    Require(bytes(main)>0&&bytes(main)==bytes(mirror),"critical log did not immediately flush both outputs");
    auto flushed=bytes(main);
    Log("[ENTRY-FIXTURE] another ordinary event");
    Require(bytes(main)==flushed&&bytes(mirror)==flushed,"normal logging stopped buffering after a critical flush");
    FlushLog();
    Require(bytes(main)==flushed,"periodic logger flushed before its interval");
    {std::lock_guard<std::mutex> lock(logMutex);logFlushClock=GetTickCount64()-logFlushIntervalMs;}
    FlushLog();
    Require(bytes(main)>flushed&&bytes(main)==bytes(mirror)&&!logDirty.load(),"periodic logger did not flush its pending batch");
    flushed=bytes(main);
    Log("[ENTRY-FIXTURE] pending before build identity");
    Require(bytes(main)==flushed,"ordinary event wrote through before startup evidence");
    Log("[BUILD] fixture identity");
    Require(bytes(main)>flushed&&bytes(main)==bytes(mirror),"startup build identity was left buffered");
    for(const auto* marker:{"[CONTROL-PANEL-REQUEST] requestedOpen=1", "[CONTROL-PANEL-GRAPHICS] stage=first-present"}){
      flushed=bytes(main);
      Log("%s",marker);
      Require(bytes(main)>flushed&&bytes(main)==bytes(mirror),"panel crash breadcrumb was left buffered");
    }
    std::string bounded(2048,'x');
    for(size_t i=0;i<1000;++i)Log("[ENTRY-FIXTURE] %zu %s",i,bounded.c_str());
    Require(bytes(main)>1000000,"bounded log buffering grew without flushing full buffers");
    FlushLog(true);
    Require(!logDirty.load()&&bytes(main)==bytes(mirror),"explicit flush lost a buffered output");
  } catch(...) {cleanup();throw;}
  cleanup();
  auto read=[](const std::filesystem::path& path){std::ifstream file(path,std::ios::binary);return std::string(std::istreambuf_iterator<char>(file),{});};
  auto contents=read(root/"session.log");
  Require(contents==read(root/"mirror.log")&&contents.find("critical fixture evidence")!=std::string::npos&&contents.find("[pid=")!=std::string::npos,
          "session/mirror logging lost lines or process identity");
}
void Rejects(const std::function<void()>& action, const std::string& expected) {
  try { action(); }
  catch (const std::exception& error) {
    Require(std::string(error.what()).find(expected) != std::string::npos,
            "unexpected contract rejection: " + std::string(error.what()));
    return;
  }
  throw std::runtime_error("contract accepted: " + expected);
}
int main() {
  const auto root = std::filesystem::temp_directory_path() /
      ("eff-entry-contract-" + std::to_string(GetCurrentProcessId()) + "-" + std::to_string(GetTickCount64()));
  try {
    Require(!std::filesystem::exists(root), "fixture directory already exists");
    gameDirectory = root;
    LoggingChecks(root/"logging");
    Require(EiemGlobalConfig{}.reload.vk == VK_F10, "default reload shortcut regressed");
    EiemGlobalConfig defaultConfig;std::istringstream defaultText(kEiemDefaultGlobalConfig);std::string configError;
    Require(EiemParseGlobalConfig(defaultText, &defaultConfig, configError) && defaultConfig.reload.vk == VK_F10,
            "generated configuration disagrees with default reload shortcut");
    Require(std::string(kEiemDefaultGlobalConfig).find("effects_reload") == std::string::npos,
            "generated configuration still advertises a separate effects reload shortcut");
    std::istringstream legacyText("[Hotkeys]\nreload=F10\ngui=INSERT\neffects_reload=F9\n");
    EiemGlobalConfig legacyConfig;
    Require(EiemParseGlobalConfig(legacyText,&legacyConfig,configError)&&legacyConfig.reload.vk==VK_F10,
            "legacy effects_reload setting changed or invalidated the unified F10 binding");
    Require(!defaultConfig.detailedDiagnostics&&!legacyConfig.detailedDiagnostics,
            "detailed native diagnostics enabled without an explicit opt-in");
    std::ifstream distributionText("config/eff.ini");EiemGlobalConfig distributionConfig;
    Require(distributionText&&EiemParseGlobalConfig(distributionText,&distributionConfig,configError)&&
            !distributionConfig.detailedDiagnostics&&distributionConfig.reload.vk==defaultConfig.reload.vk&&
            distributionConfig.gui.vk==defaultConfig.gui.vk&&distributionConfig.disableCameraFade==defaultConfig.disableCameraFade,
            "distributed eff.ini disagrees with generated defaults");
    for(bool enabled : {true,false}) {
      std::istringstream text(std::string("[Diagnostics]\ndetailed=")+(enabled?"true\n":"false\n"));
      EiemGlobalConfig parsed;
      Require(EiemParseGlobalConfig(text,&parsed,configError)&&parsed.detailedDiagnostics==enabled,
              "explicit detailed diagnostics setting was not respected");
      settings.Publish(parsed);
      Require(settings.Get().detailedDiagnostics==enabled,"diagnostics publication lost the new configuration");
    }
    for(const char* text : {"[Diagnostics]\ndetailed=1\n","[Diagnostics]\nunknown=true\n","[Diagnostics]\ndetailed=false\ndetailed=true\n"}) {
      std::istringstream input(text);EiemGlobalConfig parsed;
      Require(!EiemParseGlobalConfig(input,&parsed,configError),"invalid diagnostics setting accepted");
    }
    AP_HotkeyInfo hotkeys[3]{};
    Require(AP_GetHotkeys(hotkeys,3)==2&&std::string(hotkeys[0].configKey)=="reload"&&
            std::string(hotkeys[1].configKey)=="gui",
            "plugin API still advertises a separate effects hotkey");
    auto mods = root / "plugin/mods";
    Require(DiscoverModRoots().empty()&&InstalledAuthorsMatch({}),"deleted/missing mods directory is not an empty set");
    std::filesystem::create_directories(mods);
    { std::ofstream file(root / "plugin/eff.ini",std::ios::binary);file << ';' << std::string(70000,'x') << '\n' << kEiemDefaultGlobalConfig; }
    Require(EiemEntrySettings::Read(root).reload.vk==VK_F10,"valid configuration rejected by a file size ceiling");
    Require(DiscoverModRoots().empty(),"empty Mod directory rejected");
    auto first = mods / "First";
    std::filesystem::create_directory(first);
    { std::ofstream file(first / "mod.ini"); file << "[Mod]\nformat=2\nid=First\n"; }
    Require(DiscoverModRoots().size() == 1 && DiscoverModRoots().front() == first,
            "single installed author directory rejected");
    auto foreign = root / "Foreign";
    std::filesystem::create_directory(foreign);
    Package package;package.id="First";package.root=first;
    Require(InstalledAuthorsMatch({package}),"installed author membership rejected");
    auto foreignPackage=package;foreignPackage.root=foreign;
    Require(!InstalledAuthorsMatch({foreignPackage}),"foreign author directory matched");
    auto second = mods / "Second";
    std::filesystem::create_directory(second);
    { std::ofstream file(second / "mod.ini"); file << "[Mod]\nformat=2\nid=Second\n"; }
    Package other;other.id="Second";other.root=second;
    Require(DiscoverModRoots().size()==2&&InstalledAuthorsMatch({package,other})&&!InstalledAuthorsMatch({package}),"added character Mod membership was ignored");
    std::filesystem::remove(second/"mod.ini");
    Require(InstalledAuthorsMatch({package})&&!InstalledAuthorsMatch({package,other}),"deleted character Mod membership was ignored");
    std::filesystem::remove(first/"mod.ini");Require(InstalledAuthorsMatch({}),"last Mod deletion rejected empty set");
    Require(EiemRelativeFilePath("ui/panel.lua") && EiemRelativeFilePath("ui\\panel.lua"), "valid relative script path rejected");
    for (const auto* path : {"", "/ui.lua", "C:/ui.lua", "../ui.lua", "ui/../panel.lua", "ui//panel.lua", "ui/./panel.lua", "ui/"})
      Require(!EiemRelativeFilePath(path), "invalid relative script path accepted");
    Require(std::filesystem::canonical(root).parent_path() == std::filesystem::canonical(std::filesystem::temp_directory_path()),
            "fixture cleanup escaped temporary directory");
    std::filesystem::remove_all(root);
    std::cout << "PASS current package contract, zero/multiple character Mod discovery, add/remove membership, default F10, config beyond 64 KiB, strict Lua file paths\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "FAIL " << error.what() << '\n';
    return 1;
  }
}

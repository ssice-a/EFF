// Production discovery and identity contracts with temporary author directories.
// No game, resource compilation, Blender export or managed API is invoked.
#include "../../src/eiem_resource_entry.cpp"
#include "../../src/eiem_file_io.h"
#include <iostream>
#include <functional>

using namespace Entry;
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
    Require(EiemGlobalConfig{}.reload.vk == VK_F10, "default reload shortcut regressed");
    EiemGlobalConfig defaultConfig;std::istringstream defaultText(kEiemDefaultGlobalConfig);std::string configError;
    Require(EiemParseGlobalConfig(defaultText, &defaultConfig, configError) && defaultConfig.reload.vk == VK_F10,
            "generated configuration disagrees with default reload shortcut");
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
    std::cout << "PASS abi=" << EiemPackageProtocol::abi << " zero/multiple character Mod discovery, add/remove membership, default F10, config beyond 64 KiB, strict Lua file paths\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "FAIL " << error.what() << '\n';
    return 1;
  }
}

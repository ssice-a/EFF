#pragma once
#include "eiem_file_io.h"
#include "eiem_keys.h"
#include <filesystem>
#include <stdexcept>
#include <vector>

namespace EiemCustomGui {
struct Declaration {
  std::string script, error;
  EiemKeyChord key;
  bool enabled = true;
  static bool Field(const std::string& name) {
    return name == "gui_script" || name == "gui_key" || name == "gui_enabled";
  }
  template<class Fields> static Declaration Read(const Fields& values) {
    Declaration gui;
    auto script = values.find("gui_script");
    if (script == values.end()) return gui;
    gui.script = script->second;
    if (!EiemRelativeFilePath(gui.script) || gui.script.size()>512 ||
        gui.script.size()<5 || gui.script.compare(gui.script.size()-4,4,".lua")!=0)
      gui.error = "gui_script must be a Mod-relative .lua path";
    if (auto key = values.find("gui_key"); key != values.end() && !key->second.empty() &&
        !EiemParseKeyChord(key->second, &gui.key)) gui.error = "Invalid gui_key";
    if (auto flag = values.find("gui_enabled"); flag != values.end()) {
      if (flag->second == "0" || flag->second == "false") gui.enabled = false;
      else if (flag->second != "1" && flag->second != "true") gui.error = "gui_enabled expects 0 or 1";
    }
    if (gui.script.empty()) gui.script = script->first;
    return gui;
  }
};
enum class Operation { Set, Invoke, Trigger, Play, Pause, Stop };
struct Request {
  Operation operation;
  std::string name;
  double value = 0;
};
using Requests = std::vector<Request>;

inline std::vector<unsigned char> ReadFile(const std::filesystem::path& root,
                                           const std::string& relative, size_t limit) {
  namespace fs = std::filesystem;
  if (!EiemRelativeFilePath(relative)) throw std::runtime_error("Invalid GUI relative path");
  auto base = fs::canonical(root);
  auto path = fs::canonical(base / fs::u8path(relative));
  auto inside = path.lexically_relative(base);
  if (inside.empty() || inside.is_absolute() || *inside.begin() == ".." || !fs::is_regular_file(path))
    throw std::runtime_error("GUI file must be inside the Mod directory");
  std::ifstream input(path, std::ios::binary | std::ios::ate);
  auto size = input.tellg();
  if (size <= 0 || size > std::streamoff(limit)) throw std::runtime_error("Invalid GUI file size");
  std::vector<unsigned char> bytes(static_cast<size_t>(size));
  input.seekg(0);
  if (!input.read(reinterpret_cast<char*>(bytes.data()), size) || input.peek() != EOF)
    throw std::runtime_error("GUI file changed while reading");
  return bytes;
}
}

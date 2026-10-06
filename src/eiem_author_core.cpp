// Shared serialized parser ABI for Blender author tools. No Unity runtime.
#include "eiem_native_input.h"
#include <cstdlib>
#include <sstream>
namespace {
std::string Quote(const std::string& value) {
  std::ostringstream out; out << '"';
  const char hex[] = "0123456789abcdef";
  for (unsigned char c : value) {
    if (c == '"' || c == '\\') out << '\\' << c;
    else if (c < 32) out << "\\u00" << hex[c >> 4] << hex[c & 15];
    else out << c;
  }
  out << '"'; return out.str();
}
char* Result(const std::string& value) {
  auto result = static_cast<char*>(std::malloc(value.size() + 1));
  if (result) std::memcpy(result, value.c_str(), value.size() + 1);
  return result;
}
}
extern "C" __declspec(dllexport) unsigned EFF_AuthorAbi() { return 1; }
extern "C" __declspec(dllexport) void EFF_FreeAuthorResult(void* value) { std::free(value); }
extern "C" __declspec(dllexport) char* EFF_SourceSchema(const unsigned char* data, size_t size) {
  try {
    EiemDirectory::Require(data && size >= 48, "author source input extent");
    EiemDirectory::Bytes input(data, data + size);
    const auto layout = EiemNativeInput::ParseSerialized(input);
    std::ostringstream out; out << "{\"abi\":1,\"types\":[";
    for (size_t i = 0; i < layout.types.size(); ++i) {
      const auto& type = layout.types[i];
      if (i) out << ',';
      out << "{\"index\":" << i << ",\"classId\":" << type.classId << ",\"nodes\":[";
      std::vector<size_t> levels(type.nodes.size());
      for (size_t n = 0; n < type.nodes.size(); ++n) {
        const auto& node = type.nodes[n];
        for (auto child : node.children) levels.at(child) = levels[n] + 1;
        if (n) out << ',';
        out << "{\"level\":" << levels[n] << ",\"type\":" << Quote(node.type)
            << ",\"name\":" << Quote(node.name) << ",\"metaFlag\":" << (node.align ? 0x4000 : 0) << '}';
      }
      out << "]}";
    }
    out << "]}"; return Result(out.str());
  } catch (const std::exception& error) { return Result("{\"error\":" + Quote(error.what()) + "}"); }
  catch (...) { return Result("{\"error\":\"author core failed\"}"); }
}

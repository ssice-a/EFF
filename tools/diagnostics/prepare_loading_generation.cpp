#include <fstream>
#include <iostream>
#include "eiem_loading_generation.h"
using namespace EiemLoadingGeneration;
static std::string Q(const std::string& text) {
  std::string result = "\"";
  for (unsigned char c : text) {
    if (c == '"' || c == '\\') { result += '\\'; result += c; }
    else { Require(c >= 32, "unexpected control in generation manifest"); result += c; }
  }
  return result + "\"";
}
static bool Serialized(const Bytes& bytes) {
  return bytes.size() >= 48 && !bytes[8] && !bytes[9] && !bytes[10] && bytes[11] == 22;
}
int main(int argc, char** argv) {
  try {
    Require(argc == 4, "usage: prepare_loading_generation <prepared directory> <generation key> <new output directory>");
    auto source = std::filesystem::u8path(argv[1]), output = std::filesystem::u8path(argv[3]);
    std::string key = argv[2]; Require(key.size() == 64, "requires full SHA256 generation key");
    for (auto c : key) Require((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F'), "generation key must be uppercase hexadecimal");
    Require(!std::filesystem::exists(output), "refusing to overwrite loading generation");
    Names names; std::vector<std::filesystem::path> files;
    std::set<std::string> sourceNames, generatedNames;
    for (const auto& entry : std::filesystem::directory_iterator(source))
      if (entry.is_regular_file() && entry.path().extension() == ".vfs") files.push_back(entry.path());
    std::sort(files.begin(), files.end()); Require(!files.empty(), "empty prepared loading graph");
    for (const auto& path : files) {
      auto bundle = EndfieldInput::Decode(Read(path), 512ull * 1024 * 1024);
      for (const auto& member : bundle.members) {
        Require(sourceNames.insert(member.name).second, "duplicate source member identity");
        if (!Serialized(member.bytes)) continue;
        Require(Cab(member.name), "candidate source CAB grammar differs");
        auto name = "CAB-" + Token(key, member.name, 32);
        Require(generatedNames.insert(name).second, "candidate CAB collision");
        names.members.emplace(member.name, name);
        auto layout = ParseSerialized(member.bytes);
        size_t assetBundles = 0;
        for (const auto& row : layout.objects) {
          const auto& type = layout.types.at(row.second.type); if (type.classId != 142) continue;
          auto fields = Fields(type, 0, member.bytes, row.second.offset, row.second.offset + row.second.size, row.second.offset);
          auto old = FieldString(member.bytes, fields.at("m_Name"));
          Require(old.size() == 32 && old.substr(0,5) == "main/" && old.substr(29) == ".ab",
                  "candidate AssetBundle name grammar differs");
          auto replacement = "main/" + Token(key, "bundle:" + old, 24) + ".ab";
          Require(names.bundles.emplace(old,replacement).second && generatedNames.insert(replacement).second,
                  "candidate Bundle name collision"); ++assetBundles;
        }
        Require(assetBundles == 1, "candidate source must contain one AssetBundle object");
      }
    }
    // All streams in this graph use a local CAB resource member. Unknown stream
    // naming is rejected rather than silently publishing an incomplete map.
    for (const auto& member : sourceNames) if (!names.members.count(member)) {
      auto cab = member.substr(0,36); auto found = names.members.find(cab);
      Require(found != names.members.end() && member == cab + ".resS", "candidate stream member grammar differs");
      auto mapped = found->second + ".resS";
      Require(generatedNames.insert(mapped).second, "candidate resource member collision");
      names.members.emplace(member,mapped);
    }
    for (const auto& name : generatedNames) Require(!sourceNames.count(name), "candidate identity collides with original graph");
    std::filesystem::create_directories(output / "inputs");
    std::ofstream report(output / "manifest.json.tmp",std::ios::binary);
    Require(bool(report), "cannot write generation manifest");
    report << "{\"format\":\"EFF_OFFLINE_LOADING_GENERATION\",\"version\":1,\"generationKey\":" << Q(key)
           << ",\"nativeSubmission\":false,\"productionTablePublished\":false,\"memberMap\":{";
    bool first = true;
    for (const auto& name : names.members) { if (!first) report << ','; first = false; report << Q(name.first) << ':' << Q(name.second); }
    report << "},\"bundleMap\":{"; first = true;
    for (const auto& name : names.bundles) { if (!first) report << ','; first = false; report << Q(name.first) << ':' << Q(name.second); }
    report << "},\"files\":["; first = true;
    size_t externalPaths = 0, streamPaths = 0, bundleNames = 0, opaque = 0;
    for (const auto& path : files) {
      auto sourceBytes = Read(path); auto bundle = EndfieldInput::Decode(sourceBytes, 512ull * 1024 * 1024);
      size_t fileExternals = 0, fileStreams = 0, fileOpaque = 0;
      for (auto& member : bundle.members) {
        if (Serialized(member.bytes)) {
          auto prepared = PrepareSerialized(member.bytes, names);
          externalPaths += prepared.externalPaths; fileExternals += prepared.externalPaths;
          streamPaths += prepared.streamPaths; fileStreams += prepared.streamPaths;
          bundleNames += prepared.bundleNames; opaque += prepared.opaqueObjectsPreserved;
          fileOpaque += prepared.opaqueObjectsPreserved; member.bytes = std::move(prepared.bytes);
        }
        member.name = names.members.at(member.name);
      }
      auto bytes = EndfieldInput::Encode(sourceBytes, bundle.members, 512ull * 1024 * 1024);
      auto decoded = EndfieldInput::Decode(bytes,512ull * 1024 * 1024);
      Require(decoded.members.size() == bundle.members.size(), "candidate encoded member coverage differs");
      for (size_t i = 0; i < bundle.members.size(); ++i)
        Require(decoded.members[i].name == bundle.members[i].name && decoded.members[i].flags == bundle.members[i].flags &&
                    decoded.members[i].bytes == bundle.members[i].bytes, "candidate input encoding changed member bytes/flags");
      std::ofstream file(output / "inputs" / path.filename(),std::ios::binary);
      file.write((const char*)bytes.data(), bytes.size()); Require(bool(file), "candidate input write failed");
      auto before = Sha(sourceBytes.data(),sourceBytes.size()), after = Sha(bytes.data(),bytes.size());
      if (!first) report << ','; first = false;
      report << "{\"file\":" << Q(path.filename().u8string()) << ",\"sourceSha256\":" << Q(Hex(before.data(),before.size()))
             << ",\"candidateSha256\":" << Q(Hex(after.data(),after.size())) << ",\"bytes\":" << bytes.size()
             << ",\"remappedExternals\":" << fileExternals << ",\"remappedStreams\":" << fileStreams
             << ",\"opaqueObjectsPreserved\":" << fileOpaque << '}';
    }
    report << "],\"externalPathsRemapped\":" << externalPaths << ",\"streamPathsRemapped\":" << streamPaths
           << ",\"bundleNamesRemapped\":" << bundleNames << ",\"opaqueObjectsPreserved\":" << opaque
           << ",\"numericPPtrsPreserved\":true,\"externalGuidsPreserved\":true,\"objectTableAndLayoutPreserved\":true"
           << ",\"outsideGraphDependenciesSharedWithOriginal\":true,\"schemaNamespacePrepared\":true"
           << ",\"generationCacheIsolationVerified\":false,\"existingInstanceResourceMigrationVerified\":false"
           << ",\"readyForNativeCandidateLoading\":false,\"completeResourceHotReloadImplemented\":false}\n";
    report.flush(); Require(bool(report), "generation manifest write failed"); report.close();
    std::filesystem::rename(output / "manifest.json.tmp", output / "manifest.json");
    std::cout << "OFFLINE-LOADING-GENERATION inputs=" << files.size() << " members=" << names.members.size()
              << " externalPaths=" << externalPaths << " streamPaths=" << streamPaths << " bundleNames=" << bundleNames
              << " opaqueObjectsPreserved=" << opaque << " nativeSubmission=0\n";
    return 0;
  } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}

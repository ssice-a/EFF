#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include "eiem_loading_graph_audit.h"
using namespace EiemLoadingGraphAudit;

static std::string Q(const std::string& text) {
  std::string out = "\"";
  for (unsigned char c : text) {
    if (c == '"' || c == '\\') { out += '\\'; out += (char)c; }
    else if (c < 32) {
      static const char digits[] = "0123456789ABCDEF";
      out += "\\u00"; out += digits[c >> 4]; out += digits[c & 15];
    } else out += (char)c;
  }
  return out + "\"";
}
struct Audit {
  std::string input, member, hash, bundleName;
  uint32_t flags = 0;
  std::map<int64_t, int> objects;
  std::vector<External> externals;
  std::vector<Asset> assets;
  References refs;
};
int main(int argc, char** argv) {
  try {
    Require(argc == 3, "usage: loading_graph_audit <input directory> <new report.json>");
    auto root = std::filesystem::u8path(argv[1]), output = std::filesystem::u8path(argv[2]);
    Require(!std::filesystem::exists(output), "refusing to overwrite graph audit");
    std::vector<std::filesystem::path> paths;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(root))
      if (entry.is_regular_file() && entry.path().extension() == ".vfs") paths.push_back(entry.path());
    std::sort(paths.begin(), paths.end()); Require(!paths.empty(), "empty input graph");
    std::vector<Audit> audits;
    std::map<std::string, std::pair<size_t, size_t>> members;
    std::map<std::string, size_t> serials;
    std::set<std::string> filenames;
    for (const auto& path : paths) {
      Require(filenames.insert(path.filename().u8string()).second, "duplicate input filename");
      auto bundle = EndfieldInput::Decode(Read(path));
      for (const auto& member : bundle.members) {
        Require(members.emplace(member.name, std::make_pair(audits.size(), member.bytes.size())).second,
                "duplicate input member identity");
        if (member.bytes.size() < 48 || member.bytes[8] || member.bytes[9] ||
            member.bytes[10] || member.bytes[11] != 22) continue;
        auto layout = ParseSerialized(member.bytes);
        Audit audit; audit.input = path.filename().u8string(); audit.member = member.name;
        audit.flags = member.flags;
        auto hash = Sha(member.bytes.data(), member.bytes.size()); audit.hash = Hex(hash.data(), hash.size());
        for (const auto& row : layout.objects) {
          const auto& type = layout.types.at(row.second.type);
          audit.objects.emplace(row.first, type.classId);
          if (type.classId == 142) {
            auto fields = Fields(type, 0, member.bytes, row.second.offset,
                                 row.second.offset + row.second.size, row.second.offset);
            auto name = fields.find("m_Name"); Require(name != fields.end(), "AssetBundle name absent");
            audit.bundleName = FieldString(member.bytes, name->second);
          }
        }
        audit.externals = ReadExternals(member.bytes, layout);
        audit.assets = SerializedAssets(member.bytes, member.name, layout);
        audit.refs = ReadReferences(member.bytes, layout);
        serials.emplace(member.name, audits.size()); audits.push_back(std::move(audit));
      }
    }
    std::ofstream report(output, std::ios::binary);
    Require(bool(report), "cannot write graph audit");
    size_t internalReferences = 0, boundaryReferences = 0, brokenReferences = 0, badStreams = 0, unparsedObjects = 0, boundaryStreams = 0;
    std::set<std::string> boundaryCabs;
    report << "{\"format\":\"EFF_LOADING_GRAPH_AUDIT\",\"version\":1,\"nativeSubmission\":false,\"inputs\":"
           << paths.size() << ",\"members\":" << members.size() << ",\"serialized\":[";
    bool firstAudit = true;
    for (const auto& audit : audits) {
      if (!firstAudit) report << ','; firstAudit = false;
      report << "{\"input\":" << Q(audit.input) << ",\"member\":" << Q(audit.member)
             << ",\"bundleName\":" << Q(audit.bundleName)
             << ",\"flags\":" << audit.flags << ",\"sha256\":" << Q(audit.hash)
             << ",\"objectCount\":" << audit.objects.size() << ",\"classCounts\":{";
      std::map<int, size_t> classes;
      for (const auto& object : audit.objects) ++classes[object.second];
      bool first = true;
      for (const auto& type : classes) { if (!first) report << ','; first = false; report << Q(std::to_string(type.first)) << ':' << type.second; }
      report << "},\"externals\":["; first = true;
      for (const auto& ext : audit.externals) {
        if (!first) report << ','; first = false;
        report << "{\"fileId\":" << ext.fileId << ",\"recordOffset\":" << ext.recordOffset
               << ",\"guidOffset\":" << ext.guidOffset << ",\"pathOffset\":" << ext.pathOffset
               << ",\"endOffset\":" << ext.endOffset << ",\"assetPath\":" << Q(ext.assetPath)
               << ",\"guid\":" << Q(ext.guid) << ",\"type\":" << ext.type
               << ",\"path\":" << Q(ext.path) << ",\"member\":" << Q(ext.member)
               << ",\"insideGraph\":" << (serials.count(ext.member) ? "true" : "false") << '}';
        if (!serials.count(ext.member)) boundaryCabs.insert(ext.member);
      }
      report << "],\"assets\":["; first = true;
      for (const auto& asset : audit.assets) {
        if (!first) report << ','; first = false;
        report << "{\"path\":" << Q(asset.path) << ",\"asset\":" << Q(asset.asset)
               << ",\"member\":" << Q(asset.cab) << ",\"pathId\":" << Q(std::to_string(asset.pathId))
               << ",\"classId\":" << asset.classId << '}';
      }
      report << "],\"referenceTargets\":["; first = true;
      std::map<std::pair<int32_t, int64_t>, size_t> references;
      for (const auto& ptr : audit.refs.pointers) ++references[{ptr.fileId, ptr.pathId}];
      for (const auto& ref : references) {
        auto fileId = ref.first.first; auto pathId = ref.first.second;
        bool validFile = fileId >= 0 && (size_t)fileId <= audit.externals.size();
        std::string target = validFile ? (fileId ? audit.externals[fileId - 1].member : audit.member) : "<invalid-file-id>";
        auto targetIndex = serials.find(target);
        bool inside = targetIndex != serials.end();
        bool resolved = inside && audits.at(targetIndex->second).objects.count(pathId);
        bool broken = !validFile || (inside && !resolved);
        if (broken) brokenReferences += ref.second;
        else if (inside) internalReferences += ref.second;
        else boundaryReferences += ref.second;
        if (!first) report << ','; first = false;
        report << "{\"fileId\":" << fileId << ",\"pathId\":" << Q(std::to_string(pathId))
               << ",\"member\":" << Q(target) << ",\"uses\":" << ref.second
               << ",\"insideGraph\":" << (inside ? "true" : "false")
               << ",\"resolved\":" << (resolved ? "true" : "false")
               << ",\"broken\":" << (broken ? "true" : "false") << '}';
      }
      report << "],\"unparsedObjects\":["; first = true;
      for (const auto& item : audit.refs.unparsed) {
        if (!first) report << ','; first = false; ++unparsedObjects;
        report << "{\"objectId\":" << Q(std::to_string(item.objectId))
               << ",\"classId\":" << item.classId << ",\"error\":" << Q(item.error) << '}';
      }
      report << "],\"streams\":["; first = true;
      for (const auto& stream : audit.refs.streams) {
        auto name = Basename(stream.path); auto local = members.find(name);
        bool present = local != members.end();
        bool valid = present && stream.offset <= local->second.second && stream.size <= local->second.second - stream.offset;
        bool invalid = stream.path.empty() || (present && !valid);
        if (invalid) ++badStreams;
        if (!present) ++boundaryStreams;
        if (!first) report << ','; first = false;
        report << "{\"objectId\":" << Q(std::to_string(stream.objectId)) << ",\"classId\":" << stream.classId
               << ",\"field\":" << Q(stream.field) << ",\"path\":" << Q(stream.path)
               << ",\"member\":" << Q(name) << ",\"pathDataOffset\":" << stream.pathDataOffset << ",\"offset\":" << stream.offset << ",\"size\":" << stream.size
               << ",\"insideGraph\":" << (present ? "true" : "false")
               << ",\"rangeVerified\":" << (valid ? "true" : "false")
               << ",\"broken\":" << (invalid ? "true" : "false") << '}';
      }
      report << "]}";
    }
    report << "],\"externalBoundaryMembers\":["; bool first = true;
    for (const auto& member : boundaryCabs) { if (!first) report << ','; first = false; report << Q(member); }
    report << "],\"internalReferenceUses\":" << internalReferences
           << ",\"boundaryReferenceUses\":" << boundaryReferences << ",\"brokenReferenceUses\":" << brokenReferences
           << ",\"brokenStreams\":" << badStreams
           << ",\"unparsedObjects\":" << unparsedObjects << ",\"boundaryStreams\":" << boundaryStreams
           << ",\"outgoingClosureVerified\":" << (boundaryCabs.empty() && !badStreams && !brokenReferences && !unparsedObjects && !boundaryStreams ? "true" : "false")
           << ",\"generationCacheIsolationVerified\":false}\n";
    Require(bool(report), "short graph audit write");
    std::cout << "LOADING-GRAPH-AUDIT inputs=" << paths.size() << " serialized=" << audits.size()
              << " boundaryMembers=" << boundaryCabs.size() << " internalUses=" << internalReferences
              << " boundaryUses=" << boundaryReferences << " brokenUses=" << brokenReferences
              << " brokenStreams=" << badStreams << " unparsedObjects=" << unparsedObjects << '\n';
    return brokenReferences || badStreams ? 2 : 0;
  } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}

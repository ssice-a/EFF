#include <algorithm>
#include <fstream>
#include <iostream>
#include <map>
#include <set>

#include "eiem_mesh_input_builder.h"
#include "eiem_texture_input_builder.h"
#include "eiem_renderer_input_builder.h"
#include "eiem_loading_input_substitution.h"
#include "eiem_resource_directory.h"
#include "eiem_packed_uv_probe.h"
using namespace EiemDirectory;

static std::string Key(const EiemNativeInput::Asset& asset) {
  return asset.cab + ":" + std::to_string(asset.pathId);
}

static std::string JsonEscape(const std::string& text) {
  std::string out;
  out.reserve(text.size() + 8);
  for (unsigned char c : text) {
    switch (c) {
      case '"':
        out += "\\\"";
        break;
      case '\\':
        out += "\\\\";
        break;
      case '\n':
        out += "\\n";
        break;
      case '\r':
        out += "\\r";
        break;
      case '\t':
        out += "\\t";
        break;
      default:
        if (c < 0x20) {
          char buffer[8] = {};
          _snprintf_s(buffer, sizeof(buffer), _TRUNCATE, "\\u%04X", c);
          out += buffer;
        } else {
          out.push_back((char)c);
        }
    }
  }
  return out;
}

static EiemNativeInput::Bytes SortedObjectRowsProbe(const EiemNativeInput::Bytes& source) {
  using namespace EiemNativeInput;
  auto layout = ParseSerialized(source);
  auto order = layout.order;
  std::sort(order.begin(), order.end());
  Bytes output = source;
  std::map<int64_t, size_t> rows;
  for (size_t i = 0; i < layout.order.size(); ++i) rows.emplace(layout.order[i], layout.tableStart + i * 24);
  for (size_t i = 0; i < order.size(); ++i)
    std::copy_n(source.begin() + rows.at(order[i]), 24, output.begin() + layout.tableStart + i * 24);
  Require(std::equal(source.begin(), source.begin() + layout.tableStart, output.begin()) &&
              std::equal(source.begin() + layout.tableEnd, source.end(), output.begin() + layout.tableEnd),
          "object-order probe changed bytes outside object table");
  auto checked = ParseSerialized(output);
  Require(checked.order == order && checked.objects.size() == layout.objects.size(), "object-order probe identity mismatch");
  for (const auto& entry : layout.objects) {
    const auto& before = entry.second;
    const auto& after = checked.objects.at(entry.first);
    Require(before.offset == after.offset && before.size == after.size && before.type == after.type,
            "object-order probe changed payload range/type");
  }
  return output;
}

static int MatchPackage(const std::filesystem::path& packagePath,
                        const std::vector<std::filesystem::path>& nativePaths,
                        const std::filesystem::path& reportPath) {
  auto package = Load(packagePath);
  std::map<std::string, EiemNativeInput::Asset> assets;
  size_t parsedInputs = 0;
  for (const auto& path : nativePaths) {
    auto discovered = EiemNativeInput::Inspect(Read(path), path.stem().u8string());
    ++parsedInputs;
    for (const auto& asset : discovered) {
      if (asset.classId != 43 || asset.asset.empty()) continue;
      assets.emplace(Key(asset), asset);
    }
  }

  struct Result {
    EiemDirectory::Identity target;
    std::string rule;
    std::vector<std::string> candidates;
  };
  std::vector<Result> results;
  size_t missing = 0, ambiguous = 0;
  for (const auto& entry : package.targetIndex) {
    Result result{entry.first, entry.second, {}};
    for (const auto& asset : assets) {
      if (asset.second.path == entry.first.path && asset.second.asset == entry.first.asset)
        result.candidates.push_back(asset.first);
    }
    if (result.candidates.empty()) ++missing;
    if (result.candidates.size() > 1) ++ambiguous;
    results.push_back(std::move(result));
  }
  std::sort(results.begin(), results.end(), [](const Result& left, const Result& right) {
    return std::tie(left.target.path, left.target.asset) <
           std::tie(right.target.path, right.target.asset);
  });

  Require(!std::filesystem::exists(reportPath),
          "refusing to overwrite match report " + reportPath.u8string());
  if (!reportPath.parent_path().empty())
    std::filesystem::create_directories(reportPath.parent_path());
  std::ofstream output(reportPath, std::ios::binary | std::ios::trunc);
  Require(bool(output), "cannot create match report " + reportPath.u8string());
  output << "{\n  \"format\":\"EFF_NATIVE_INPUT_MATCH\",\n"
         << "  \"version\":1,\n  \"package\":\"" << JsonEscape(packagePath.u8string()) << "\",\n"
         << "  \"nativeInputs\":" << parsedInputs << ",\n"
         << "  \"targets\":" << results.size() << ",\n"
         << "  \"missing\":" << missing << ",\n"
         << "  \"ambiguous\":" << ambiguous << ",\n"
         << "  \"verified\":" << ((missing == 0 && ambiguous == 0) ? "true" : "false")
         << ",\n  \"matches\":[\n";
  for (size_t index = 0; index < results.size(); ++index) {
    const auto& result = results[index];
    output << "    {\"path\":\"" << JsonEscape(result.target.path) << "\",\"asset\":\""
           << JsonEscape(result.target.asset) << "\",\"rule\":\"" << JsonEscape(result.rule)
           << "\",\"candidates\":[";
    for (size_t candidate = 0; candidate < result.candidates.size(); ++candidate) {
      if (candidate) output << ',';
      output << "\"" << JsonEscape(result.candidates[candidate]) << "\"";
    }
    output << "]}" << (index + 1 == results.size() ? "\n" : ",\n");
  }
  output << "  ]\n}\n";
  Require(bool(output), "failed writing match report");
  std::cout << "MATCH\t" << results.size() << "\t" << missing << "\t" << ambiguous << "\t"
            << parsedInputs << "\t" << reportPath.u8string() << "\n";
  return missing == 0 && ambiguous == 0 ? 0 : 1;
}

static int MatchPackageDirectory(const std::filesystem::path& packagePath,
                                 const std::filesystem::path& nativeDirectory,
                                 const std::filesystem::path& reportPath) {
  std::vector<std::filesystem::path> nativePaths;
  std::error_code error;
  for (const auto& entry : std::filesystem::recursive_directory_iterator(nativeDirectory, error)) {
    Require(!error, "cannot enumerate native input directory " + nativeDirectory.u8string());
    if (entry.is_regular_file(error) && entry.path().extension() == ".bytes")
      nativePaths.push_back(entry.path());
  }
  Require(!nativePaths.empty(), "native input directory has no .bytes files");
  std::sort(nativePaths.begin(), nativePaths.end());
  return MatchPackage(packagePath, nativePaths, reportPath);
}

int main(int argc, char** argv) {
  try {
    if (argc == 5 && std::string(argv[1]) == "--packed-uv-probe") {
      using namespace EiemNativeInput;
      auto package = Load(std::filesystem::u8path(argv[2]));
      auto source = Read(std::filesystem::u8path(argv[3]));
      auto path = std::filesystem::u8path(argv[4]);
      Require(!std::filesystem::exists(path), "refusing to overwrite packed-UV probe");
      std::set<std::string> names;
      for (const auto &entry : package.rules) {
        const auto &rule = entry.second;
        if (rule.skip) continue;
        for (const auto &target : rule.targets) for (size_t i = 0; i < rule.parts.size(); ++i)
          names.insert(target.second.asset + (i ? "__EFF_" + package.id + "_" + rule.parts[i] : ""));
      }
      auto bundle = EndfieldInput::Decode(source);
      size_t meshes = 0, members = 0;
      for (auto &member : bundle.members) {
        if (member.bytes.size() < 48 || member.bytes[8] || member.bytes[9] ||
            member.bytes[10] || member.bytes[11] != 22) continue;
        auto layout = ParseSerialized(member.bytes);
        std::map<int64_t, Bytes> changes;
        for (auto id : layout.order) {
          const auto &type = layout.types.at(layout.objects.at(id).type);
          if (type.classId != 43) continue;
          auto value = ObjectValue(member.bytes, layout, id);
          const auto &name = Child(type, value, "m_Name");
          if (!names.count(Text(name.raw.data(), name.raw.size()))) continue;
          if (!PackedUvProbe::Repack(type, value)) continue;
          changes.emplace(id, EncodeValue(type, value));
          ++meshes;
        }
        if (!changes.empty()) {
          member.bytes = BuildSerialized(member.bytes, layout, changes, {});
          ++members;
        }
      }
      auto output = members ? EndfieldInput::Encode(source, bundle.members) : source;
      auto checked = EndfieldInput::Decode(output);
      Require(checked.members.size() == bundle.members.size(), "packed-UV member count changed");
      for (size_t i = 0; i < checked.members.size(); ++i)
        Require(checked.members[i].name == bundle.members[i].name && checked.members[i].flags == bundle.members[i].flags &&
                    checked.members[i].bytes == bundle.members[i].bytes, "packed-UV encoded member changed");
      std::ofstream file(path, std::ios::binary);
      file.write((const char *)output.data(), output.size());
      Require(bool(file), "packed-UV write failed");
      std::cout << "PACKED-UV-PROBE\t" << members << "\t" << meshes << "\t" << output.size() << "\n";
      return 0;
    }
    if (argc == 5 && std::string(argv[1]) == "--geometry-readback-probe") {
      // Temporary serialized-input instrumentation. Production writer flags
      // and author resources stay unchanged; no runtime Mesh setter is used.
      using namespace EiemNativeInput;
      auto package = Load(std::filesystem::u8path(argv[2]));
      auto source = Read(std::filesystem::u8path(argv[3]));
      auto path = std::filesystem::u8path(argv[4]);
      Require(!std::filesystem::exists(path), "refusing to overwrite geometry-readback probe");
      std::set<std::string> names;
      for (const auto &entry : package.rules) {
        const auto &rule = entry.second;
        if (rule.skip) continue;
        for (const auto &target : rule.targets) for (size_t i = 0; i < rule.parts.size(); ++i)
          names.insert(target.second.asset + (i ? "__EFF_" + package.id + "_" + rule.parts[i] : ""));
      }
      auto bundle = EndfieldInput::Decode(source);
      size_t meshes = 0, changedMembers = 0;
      for (auto &member : bundle.members) {
        if (member.bytes.size() < 48 || member.bytes[8] || member.bytes[9] ||
            member.bytes[10] || member.bytes[11] != 22) continue;
        auto layout = ParseSerialized(member.bytes);
        std::map<int64_t, Bytes> changes;
        for (auto id : layout.order) {
          const auto &type = layout.types.at(layout.objects.at(id).type);
          if (type.classId != 43) continue;
          auto value = ObjectValue(member.bytes, layout, id);
          const auto &name = Child(type, value, "m_Name");
          if (!names.count(Text(name.raw.data(), name.raw.size()))) continue;
          for (const char *field : {"m_IsReadable", "m_KeepVertices", "m_KeepIndices"})
            SetInteger(type, Child(type, value, field), 1);
          changes.emplace(id, EncodeValue(type, value));
          ++meshes;
        }
        if (!changes.empty()) {
          member.bytes = BuildSerialized(member.bytes, layout, changes, {});
          ++changedMembers;
        }
      }
      auto output = changedMembers ? EndfieldInput::Encode(source, bundle.members) : source;
      auto checked = EndfieldInput::Decode(output);
      Require(checked.members.size() == bundle.members.size(), "geometry-readback member count changed");
      for (size_t i = 0; i < checked.members.size(); ++i)
        Require(checked.members[i].name == bundle.members[i].name && checked.members[i].flags == bundle.members[i].flags &&
                    checked.members[i].bytes == bundle.members[i].bytes, "geometry-readback encoded member changed");
      std::ofstream file(path, std::ios::binary);
      file.write((const char *)output.data(), output.size());
      Require(bool(file), "geometry-readback write failed");
      std::cout << "GEOMETRY-READBACK-PROBE\t" << changedMembers << "\t" << meshes << "\t" << output.size() << "\n";
      return 0;
    }
    if (argc == 4 && std::string(argv[1]) == "--object-order-probe") {
      using namespace EiemNativeInput;
      auto source = Read(std::filesystem::u8path(argv[2]));
      auto path = std::filesystem::u8path(argv[3]);
      Require(!std::filesystem::exists(path), "refusing to overwrite object-order probe");
      auto bundle = EndfieldInput::Decode(source);
      size_t changed = 0, objects = 0;
      for (auto& member : bundle.members) {
        if (member.bytes.size() < 48 || member.bytes[8] || member.bytes[9] ||
            member.bytes[10] || member.bytes[11] != 22) continue;
        auto sorted = SortedObjectRowsProbe(member.bytes);
        changed += sorted != member.bytes;
        objects += ParseSerialized(sorted).order.size();
        member.bytes = std::move(sorted);
      }
      auto output = changed ? EndfieldInput::Encode(source, bundle.members) : source;
      auto checked = EndfieldInput::Decode(output);
      Require(checked.members.size() == bundle.members.size(), "object-order probe member count changed");
      for (size_t i = 0; i < bundle.members.size(); ++i)
        Require(checked.members[i].name == bundle.members[i].name &&
                    checked.members[i].flags == bundle.members[i].flags &&
                    checked.members[i].bytes == bundle.members[i].bytes, "object-order probe member changed");
      std::ofstream file(path, std::ios::binary);
      file.write((const char*)output.data(), output.size());
      Require(bool(file), "object-order probe write failed");
      std::cout << "OBJECT-ORDER-PROBE\t" << changed << "\t" << objects << "\t" << output.size() << "\n";
      return 0;
    }
    if (argc == 7 && std::string(argv[1]) == "--renderer-bones-probe") {
      using namespace EiemNativeInput;
      auto package = Load(std::filesystem::u8path(argv[2]));
      auto graphPath = std::filesystem::u8path(argv[3]);
      auto meshSource = Read(std::filesystem::u8path(argv[4]));
      auto source = Read(std::filesystem::u8path(argv[5]));
      auto path = std::filesystem::u8path(argv[6]);
      Require(!std::filesystem::exists(path) && !std::filesystem::exists(path.u8string() + ".json"),
              "refusing to overwrite Renderer bones probe");
      std::map<Identity, std::string> selected;
      for (const auto& asset : Inspect(meshSource, "probe-source")) {
        Identity identity{asset.path, asset.asset};
        auto target = package.targetIndex.find(identity);
        if (target != package.targetIndex.end()) {
          Require(asset.classId == 43 && !package.rules.at(target->second).skip,
                  "Renderer bones probe requires a non-skip Mesh");
          selected.emplace(identity, target->second);
        }
      }
      Require(selected.size() == 1, "Renderer bones probe needs one logical Mesh target");
      package.targetIndex = selected;
      auto& rule = package.rules.at(selected.begin()->second);
      rule.parts.resize(1);
      const auto& partId = rule.parts[0];
      const auto& authorMesh = package.meshes.at(package.parts.at(partId).mesh);
      AssetIndex assets;
      MeshSchemaIndex schemas;
      for (const auto& entry : std::filesystem::directory_iterator(graphPath)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".vfs") continue;
        auto bytes = Read(entry.path());
        AddAssets(assets, bytes); AddMeshSchemas(schemas, bytes);
      }
      auto mesh = BuildMeshInput(meshSource, package);
      Require(mesh.parts.size() == 1 && mesh.addedObjects == 0,
              "Renderer bones probe Mesh is not the first existing-object part");
      auto bundle = EndfieldInput::Decode(source);
      size_t matches = 0;
      struct ProbePart { std::string cab; int64_t id; size_t originalBones; std::vector<int64_t> bones; };
      std::vector<ProbePart> parts;
      for (auto& member : bundle.members) {
        if (member.bytes.size() < 48 || member.bytes[8] || member.bytes[9] ||
            member.bytes[10] || member.bytes[11] != 22) continue;
        auto layout = ParseSerialized(member.bytes);
        RendererMemberBuilder builder(member.bytes, member.name, package, assets, mesh.parts);
        builder.SetMeshSchemas(schemas);
        auto full = builder.Build();
        if (full.parts.empty()) continue;
        Require(full.addedObjects == 0, "Renderer bones probe generated extra objects");
        auto fullLayout = ParseSerialized(full.bytes);
        std::map<int64_t, Bytes> changes;
        for (const auto& part : full.parts) {
          Require(part.renderer == part.sourceRenderer && part.part == partId &&
                      part.bones.size() == authorMesh.bones.size(), "Renderer bones probe palette mismatch");
          const auto& type = layout.types.at(layout.objects.at(part.renderer).type);
          auto original = ObjectValue(member.bytes, layout, part.renderer);
          auto rebound = ObjectValue(full.bytes, fullLayout, part.renderer);
          auto originalCount = ConstArray(type, Child(type, original, "m_Bones")).count;
          std::set<NativeReference> dependencies;
          for (auto id : part.bones) {
            Require(layout.objects.count(id) && layout.types.at(layout.objects.at(id).type).classId == 4,
                    "Renderer probe bone is not an existing local Transform");
            dependencies.insert({member.name, id});
          }
          Require(dependencies.size() == part.bones.size(), "Renderer probe has duplicate bones");
          size_t covered = 0;
          for (auto id : layout.order) {
            const auto& bundleType = layout.types.at(layout.objects.at(id).type);
            if (bundleType.classId != 142) continue;
            auto tree = ObjectValue(member.bytes, layout, id);
            const auto& preloads = ConstArray(bundleType, Child(bundleType, tree, "m_PreloadTable"));
            auto check = [&](const Value& info) {
              size_t first = (size_t)Integer(Child(bundleType, info, "preloadIndex"));
              size_t count = (size_t)Integer(Child(bundleType, info, "preloadSize"));
              Require(first <= preloads.count && count <= preloads.count - first,
                      "Renderer probe source preload interval invalid");
              std::set<NativeReference> references;
              for (size_t i = first; i < first + count; ++i)
                references.insert(NullableReference(bundleType, preloads.children[i], layout, member.name));
              if (!references.count({member.name, part.renderer})) return;
              for (const auto& bone : dependencies)
                Require(references.count(bone), "Renderer probe bone requires new preload registration");
              ++covered;
            };
            for (const char* name : {"m_Container", "m_HashContainer"})
              for (const auto& pair : ConstArray(bundleType, Child(bundleType, tree, name)).children)
                check(Child(bundleType, pair, "second"));
            check(Child(bundleType, tree, "m_MainAsset"));
          }
          Require(covered, "Renderer probe absent from current AssetBundle preload intervals");
          Child(type, original, "m_Bones") = Child(type, rebound, "m_Bones");
          changes.emplace(part.renderer, EncodeValue(type, original));
          parts.push_back({member.name, part.renderer, originalCount, part.bones});
          ++matches;
        }
        auto rebuilt = BuildSerialized(member.bytes, layout, changes);
        auto checked = ParseSerialized(rebuilt);
        Require(checked.order == layout.order, "Renderer bones probe changed object identity/order");
        for (auto id : layout.order) {
          const auto& before = layout.objects.at(id);
          const auto& after = checked.objects.at(id);
          auto changed = changes.find(id);
          Bytes expected = changed == changes.end()
              ? Bytes(member.bytes.begin() + before.offset, member.bytes.begin() + before.offset + before.size)
              : changed->second;
          Require(before.type == after.type && expected ==
                      Bytes(rebuilt.begin() + after.offset, rebuilt.begin() + after.offset + after.size),
                  "Renderer bones probe changed unintended object bytes");
        }
        member.bytes = std::move(rebuilt);
      }
      Require(matches, "Renderer bones probe found no current consumer of selected Mesh");
      auto output = EndfieldInput::Encode(source, bundle.members);
      auto checked = EndfieldInput::Decode(output);
      Require(checked.members.size() == bundle.members.size(), "Renderer probe member count changed");
      for (size_t i = 0; i < bundle.members.size(); ++i)
        Require(checked.members[i].name == bundle.members[i].name &&
                    checked.members[i].flags == bundle.members[i].flags &&
                    checked.members[i].bytes == bundle.members[i].bytes, "Renderer probe container roundtrip differs");
      std::ofstream file(path, std::ios::binary);
      file.write((const char*)output.data(), output.size());
      Require(bool(file), "Renderer bones probe write failed");
      std::ofstream report(path.u8string() + ".json", std::ios::binary);
      report << "{\"format\":\"EFF_RENDERER_BONES_PROBE\",\"version\":1,\"part\":\""
             << JsonEscape(partId) << "\",\"changedObjects\":" << matches
             << ",\"addedObjects\":0,\"preloadChanged\":false,\"skeletonChanged\":false,\"parts\":[";
      for (size_t i = 0; i < parts.size(); ++i) {
        const auto& part = parts[i];
        if (i) report << ',';
        report << "{\"cab\":\"" << JsonEscape(part.cab) << "\",\"renderer\":" << part.id
               << ",\"originalBones\":" << part.originalBones << ",\"bones\":[";
        for (size_t j = 0; j < part.bones.size(); ++j) { if (j) report << ','; report << part.bones[j]; }
        report << "]}";
      }
      report << "],\"nativeAcceptanceVerified\":false}\n";
      Require(bool(report), "Renderer bones probe report write failed");
      std::cout << "RENDERER-BONES-PROBE\t" << matches << "\t" << authorMesh.bones.size()
                << "\t" << output.size() << "\n";
      return 0;
    }
    if (argc == 6 && std::string(argv[1]) == "--mesh-probe") {
      // Diagnostic minimisation of the existing serialized-input writer.
      // This command never changes a production package or loading table.
      using namespace EiemNativeInput;
      std::string mode = argv[2];
      Require(mode == "table-roundtrip" || mode == "first-part-only", "unknown Mesh probe mode");
      auto package = Load(std::filesystem::u8path(argv[3]));
      auto source = Read(std::filesystem::u8path(argv[4]));
      auto path = std::filesystem::u8path(argv[5]);
      Require(!std::filesystem::exists(path), "refusing to overwrite Mesh probe");
      auto bundle = EndfieldInput::Decode(source);
      size_t selected = 0, changedMembers = 0;
      int64_t selectedId = 0;
      std::string selectedPart;
      for (auto& member : bundle.members) {
        if (member.bytes.size() < 48 || member.bytes[8] || member.bytes[9] ||
            member.bytes[10] || member.bytes[11] != 22) continue;
        auto layout = ParseSerialized(member.bytes);
        auto assets = SerializedAssets(member.bytes, member.name, layout);
        for (const auto& asset : assets) {
          auto target = package.targetIndex.find({asset.path, asset.asset});
          if (target == package.targetIndex.end()) continue;
          Require(asset.classId == 43 && !package.rules.at(target->second).skip &&
                      ++selected == 1, "Mesh probe requires one non-skip logical target");
          selectedId = asset.pathId;
          selectedPart = package.rules.at(target->second).parts.at(0);
          const auto& object = layout.objects.at(selectedId);
          Bytes payload(member.bytes.begin() + object.offset,
                        member.bytes.begin() + object.offset + object.size);
          if (mode == "first-part-only") {
            auto full = BuildMeshMember(member.bytes, member.name, package);
            auto fullLayout = ParseSerialized(full.bytes);
            const auto& rebuilt = fullLayout.objects.at(selectedId);
            payload.assign(full.bytes.begin() + rebuilt.offset,
                           full.bytes.begin() + rebuilt.offset + rebuilt.size);
          }
          auto rebuilt = BuildSerialized(member.bytes, layout, {{selectedId, payload}});
          auto checked = ParseSerialized(rebuilt);
          Require(checked.order == layout.order, "probe changed object identity/order");
          for (auto id : layout.order) {
            const auto& before = layout.objects.at(id);
            const auto& after = checked.objects.at(id);
            Bytes actual(rebuilt.begin() + after.offset, rebuilt.begin() + after.offset + after.size);
            Bytes expected(member.bytes.begin() + before.offset,
                           member.bytes.begin() + before.offset + before.size);
            if (id == selectedId) expected = payload;
            Require(before.type == after.type && actual == expected,
                    "probe changed an unintended object or type");
          }
          changedMembers += rebuilt != member.bytes;
          member.bytes = std::move(rebuilt);
        }
      }
      Require(selected == 1, "Mesh probe logical target absent");
      auto output = EndfieldInput::Encode(source, bundle.members);
      auto checked = EndfieldInput::Decode(output);
      Require(checked.members.size() == bundle.members.size(), "probe member count changed");
      for (size_t i = 0; i < bundle.members.size(); ++i)
        Require(checked.members[i].name == bundle.members[i].name &&
                    checked.members[i].flags == bundle.members[i].flags &&
                    checked.members[i].bytes == bundle.members[i].bytes,
                "probe container roundtrip differs");
      std::ofstream file(path, std::ios::binary);
      file.write((const char*)output.data(), output.size());
      Require(bool(file), "Mesh probe write failed");
      std::cout << "{\"mode\":\"" << mode << "\",\"selectedPathId\":" << selectedId
                << ",\"firstPart\":\"" << JsonEscape(selectedPart)
                << "\",\"changedMembers\":" << changedMembers
                << ",\"addedObjects\":0,\"preloadChanged\":false,\"bytes\":" << output.size() << "}\n";
      return 0;
    }
    if (argc == 3 && std::string(argv[1]) == "--package-digest") {
      auto package = Load(std::filesystem::u8path(argv[2]));
      std::cout << "{\"packageDigest\":\"" << EiemNativeInput::PackageDigest(package) << "\"}\n";
      return 0;
    }
    if (argc == 5 && std::string(argv[1]) == "--check-loading-table") {
      auto package = Load(std::filesystem::u8path(argv[2]));
      auto table = EiemNativeInput::ReadInputSubstitutions(std::filesystem::u8path(argv[3]),
          std::filesystem::u8path(argv[4]), package);
      std::cout << "INPUT-TABLE\t" << table.entries.size() << "\t" << table.packageDigest << "\t" << table.inputContractDigest << "\n";
      return 0;
    }
    if (argc == 6 && std::string(argv[1]) == "--loading-rebuild") {
      auto package = Load(std::filesystem::u8path(argv[2]));
      auto sources = std::filesystem::u8path(argv[3]), consumers = std::filesystem::u8path(argv[4]);
      auto destination = std::filesystem::u8path(argv[5]);
      Require(!std::filesystem::exists(destination), "refusing to overwrite loading input graph");
      std::vector<std::filesystem::path> inputs;
      for (const auto& directory : {sources, consumers})
        for (const auto& entry : std::filesystem::directory_iterator(directory))
          if (entry.is_regular_file() && entry.path().extension() == ".vfs") inputs.push_back(entry.path());
      std::sort(inputs.begin(), inputs.end());
      Require(!inputs.empty(), "loading input graph is empty");
      EiemNativeInput::AssetIndex assets;
      EiemNativeInput::MeshSchemaIndex schemas;
      std::vector<EiemNativeInput::BuiltMeshPart> meshes;
      for (const auto& input : inputs) {
        auto bytes = Read(input);
        EiemNativeInput::AddAssets(assets, bytes); EiemNativeInput::AddMeshSchemas(schemas, bytes);
        auto built = EiemNativeInput::BuildMeshInput(bytes, package);
        meshes.insert(meshes.end(), built.parts.begin(), built.parts.end());
      }
      for (const auto& target : package.targetIndex) Require(assets.count(target.first), "loading graph Mesh identity missing");
      for (const auto& mat : package.materials) Require(assets.count(mat.second.source), "loading graph Material identity missing");
      for (const auto& tex : package.textures) Require(assets.count(tex.second.source), "loading graph Texture identity missing");
      std::filesystem::create_directories(destination);
      size_t changed = 0, materials = 0, textures = 0, rendererParts = 0, meshParts = 0;
      for (const auto& input : inputs) {
        auto original = Read(input);
        auto mesh = EiemNativeInput::BuildMeshInput(original, package);
        auto renderer = EiemNativeInput::BuildRendererInput(mesh.bytes, package, assets, meshes, schemas);
        auto texture = EiemNativeInput::BuildTextureInput(renderer.bytes, package, assets);
        auto path = destination / input.filename();
        std::ofstream output(path, std::ios::binary);
        output.write((const char*)texture.bytes.data(), texture.bytes.size());
        Require(bool(output), "loading input graph write failed");
        materials += texture.materialMatches; textures += texture.textures.size();
        rendererParts += renderer.parts.size(); meshParts += mesh.parts.size();
        changed += original != texture.bytes;
      }
      Require(materials == package.materials.size() && textures == package.textures.size(), "loading Material/Texture graph coverage differs");
      std::cout << "LOADING-GRAPH\t" << inputs.size() << "\t" << changed << "\t" << meshParts << "\t"
                << rendererParts << "\t" << materials << "\t" << textures << "\n";
      return 0;
    }
    if (argc == 6 && std::string(argv[1]) == "--renderer-rebuild") {
      auto package = Load(std::filesystem::u8path(argv[2]));
      auto graphDirectory = std::filesystem::u8path(argv[3]);
      auto inputsDirectory = std::filesystem::u8path(argv[4]);
      auto destination = std::filesystem::u8path(argv[5]);
      Require(!std::filesystem::exists(destination), "refusing to overwrite Renderer input results");
      EiemNativeInput::AssetIndex assets;
      EiemNativeInput::MeshSchemaIndex schemas;
      std::vector<EiemNativeInput::BuiltMeshPart> meshes;
      std::vector<std::filesystem::path> graph, inputs;
      for (const auto& entry : std::filesystem::directory_iterator(graphDirectory))
        if (entry.is_regular_file() && entry.path().extension() == ".vfs") graph.push_back(entry.path());
      for (const auto& entry : std::filesystem::directory_iterator(inputsDirectory))
        if (entry.is_regular_file() && entry.path().extension() == ".vfs") inputs.push_back(entry.path());
      Require(!graph.empty() && !inputs.empty(), "Renderer graph/input directory has no VFS files");
      std::sort(graph.begin(), graph.end()); std::sort(inputs.begin(), inputs.end());
      for (const auto& input : graph) {
        auto bytes = Read(input);
        EiemNativeInput::AddAssets(assets, bytes);
        EiemNativeInput::AddMeshSchemas(schemas, bytes);
        auto rebuilt = EiemNativeInput::BuildMeshInput(bytes, package);
        meshes.insert(meshes.end(), rebuilt.parts.begin(), rebuilt.parts.end());
      }
      for (const auto& target : package.targetIndex)
        Require(assets.count(target.first), "Renderer graph is missing selected Mesh identity");
      for (const auto& material : package.materials)
        Require(assets.count(material.second.source), "Renderer graph is missing selected Material identity");
      std::filesystem::create_directories(destination);
      size_t matches = 0, skipped = 0, parts = 0, added = 0;
      for (const auto& input : inputs) {
        auto rebuilt = EiemNativeInput::BuildRendererInput(Read(input), package, assets, meshes, schemas);
        auto outputPath = destination / input.filename();
        std::ofstream output(outputPath, std::ios::binary);
        output.write((const char*)rebuilt.bytes.data(), rebuilt.bytes.size());
        Require(bool(output), "Renderer rebuilt input write failed");
        std::ofstream report(outputPath.u8string() + ".json", std::ios::binary);
        report << "{\n  \"format\":\"EFF_RENDERER_INPUT_REBUILD\",\n  \"version\":1,\n"
               << "  \"nativeSubmission\":false,\n  \"gameWorkerObserved\":false,\n"
               << "  \"dependencyClosureVerified\":false,\n  \"matches\":" << rebuilt.matches
               << ",\n  \"skipped\":" << rebuilt.skipped << ",\n  \"changedObjects\":" << rebuilt.changedObjects
               << ",\n  \"addedObjects\":" << rebuilt.addedObjects << ",\n  \"lodGroups\":" << rebuilt.lodGroups
               << ",\n  \"preloadReferences\":" << rebuilt.preloadReferences << ",\n  \"parts\":[\n";
        for (size_t i = 0; i < rebuilt.parts.size(); ++i) {
          const auto& part = rebuilt.parts[i];
          report << "    {\"cab\":\"" << JsonEscape(part.cab) << "\",\"rule\":\"" << JsonEscape(part.rule)
                 << "\",\"part\":\"" << JsonEscape(part.part) << "\",\"sourceRenderer\":" << part.sourceRenderer
                 << ",\"renderer\":" << part.renderer << ",\"owner\":" << part.owner << ",\"transform\":" << part.transform
                 << ",\"parent\":" << part.parent << ",\"meshCab\":\"" << JsonEscape(part.meshCab)
                 << "\",\"mesh\":" << part.mesh << ",\"materialCab\":\"" << JsonEscape(part.materialCab)
                 << "\",\"material\":" << part.material << ",\"bones\":[";
          for (size_t j = 0; j < part.bones.size(); ++j) { if (j) report << ','; report << part.bones[j]; }
          report << "]}" << (i + 1 == rebuilt.parts.size() ? "\n" : ",\n");
        }
        report << "  ]\n}\n";
        Require(bool(report), "Renderer input report write failed");
        std::cout << "RENDERER-REBUILT\t" << input.filename().u8string() << "\t" << rebuilt.matches << "\t"
                  << rebuilt.skipped << "\t" << rebuilt.parts.size() << "\t" << rebuilt.addedObjects << "\t" << rebuilt.bytes.size() << "\n";
        matches += rebuilt.matches; skipped += rebuilt.skipped; parts += rebuilt.parts.size(); added += rebuilt.addedObjects;
      }
      std::cout << "RENDERER-GRAPH\t" << matches << "\t" << skipped << "\t" << parts << "\t" << added << "\n";
      return 0;
    }
    if (argc == 5 && std::string(argv[1]) == "--texture-rebuild") {
      auto package = Load(std::filesystem::u8path(argv[2]));
      auto directory = std::filesystem::u8path(argv[3]);
      auto destination = std::filesystem::u8path(argv[4]);
      Require(!std::filesystem::exists(destination), "refusing to overwrite Texture input results");
      std::vector<std::filesystem::path> inputs;
      for (const auto& entry : std::filesystem::directory_iterator(directory))
        if (entry.is_regular_file() && entry.path().extension() == ".vfs")
          inputs.push_back(entry.path());
      Require(!inputs.empty(), "Texture input directory has no .vfs files");
      std::sort(inputs.begin(), inputs.end());
      EiemNativeInput::AssetIndex index;
      for (const auto& input : inputs) EiemNativeInput::AddAssets(index, Read(input));
      for (const auto& material : package.materials) {
        auto found = index.find(material.second.source);
        Require(found != index.end() && found->second.classId == 21,
                "selected Material absent from current input graph");
      }
      for (const auto& texture : package.textures) {
        auto found = index.find(texture.second.source);
        Require(found != index.end() && found->second.classId == 28,
                "selected Texture absent from current input graph");
      }
      std::filesystem::create_directories(destination);
      size_t materials = 0, textures = 0, bindings = 0;
      for (const auto& input : inputs) {
        auto result = EiemNativeInput::BuildTextureInput(Read(input), package, index);
        auto outputPath = destination / input.filename();
        std::ofstream output(outputPath, std::ios::binary);
        output.write((const char*)result.bytes.data(), result.bytes.size());
        Require(bool(output), "rebuilt Texture input write failed");
        std::ofstream report(outputPath.u8string() + ".json", std::ios::binary);
        report << "{\n  \"format\":\"EFF_TEXTURE_INPUT_REBUILD\",\n  \"version\":1,\n"
               << "  \"nativeSubmission\":false,\n  \"dependencyClosureVerified\":false,\n"
               << "  \"materialMatches\":" << result.materialMatches << ",\n"
               << "  \"textureBindings\":" << result.textureBindings << ",\n"
               << "  \"changedMaterials\":" << result.changedMaterials << ",\n  \"textures\":[\n";
        for (size_t i = 0; i < result.textures.size(); ++i) {
          const auto& texture = result.textures[i];
          report << "    {\"path\":\"" << JsonEscape(texture.target.path)
                 << "\",\"asset\":\"" << JsonEscape(texture.target.asset)
                 << "\",\"cab\":\"" << JsonEscape(texture.cab)
                 << "\",\"stream\":\"" << JsonEscape(texture.stream)
                 << "\",\"id\":" << texture.id << ",\"sourceId\":" << texture.sourceId << ",\"offset\":" << texture.offset
                 << ",\"bytes\":" << texture.bytes << "}"
                 << (i + 1 == result.textures.size() ? "\n" : ",\n");
        }
        report << "  ]\n}\n";
        Require(bool(report), "Texture input report write failed");
        materials += result.materialMatches;
        textures += result.textures.size();
        bindings += result.textureBindings;
        std::cout << "TEXTURE-REBUILT\t" << input.filename().u8string() << "\t"
                  << result.materialMatches << "\t" << result.textures.size() << "\t"
                  << result.textureBindings << "\t" << result.bytes.size() << "\n";
      }
      Require(materials == package.materials.size() && textures == package.textures.size(),
              "Texture/Material current input graph coverage differs");
      std::cout << "TEXTURE-GRAPH\t" << materials << "\t" << textures << "\t" << bindings << "\n";
      return 0;
    }
    if (argc == 5 && std::string(argv[1]) == "--mesh-rebuild") {
      auto package = Load(std::filesystem::u8path(argv[2]));
      auto source = Read(std::filesystem::u8path(argv[3]));
      auto path = std::filesystem::u8path(argv[4]);
      Require(!std::filesystem::exists(path) &&
                  !std::filesystem::exists(path.u8string() + ".json"),
              "refusing to overwrite Mesh input validation result");
      auto rebuilt = EiemNativeInput::BuildMeshInput(source, package);
      std::ofstream output(path, std::ios::binary);
      output.write((const char*)rebuilt.bytes.data(), rebuilt.bytes.size());
      Require(bool(output), "rebuilt Mesh input write failed");
      output.close();
      std::ofstream report(path.u8string() + ".json", std::ios::binary);
      report << "{\n  \"format\":\"EFF_MESH_INPUT_REBUILD\",\n  \"version\":1,\n"
             << "  \"nativeSubmission\":false,\n  \"rendererExpansion\":false,\n"
             << "  \"dependencyClosureVerified\":false,\n"
             << "  \"matches\":" << rebuilt.matches << ",\n"
             << "  \"skipped\":" << rebuilt.skipped << ",\n"
             << "  \"changedObjects\":" << rebuilt.changedObjects << ",\n"
             << "  \"addedObjects\":" << rebuilt.addedObjects << ",\n  \"parts\":[\n";
      for (size_t i = 0; i < rebuilt.parts.size(); ++i) {
        const auto& part = rebuilt.parts[i];
        report << "    {\"path\":\"" << JsonEscape(part.target.path)
               << "\",\"asset\":\"" << JsonEscape(part.target.asset)
               << "\",\"part\":\"" << JsonEscape(part.part)
               << "\",\"cab\":\"" << JsonEscape(part.cab)
               << "\",\"sourceId\":" << part.sourceId << ",\"id\":" << part.id
               << ",\"vertices\":" << part.vertices << ",\"indices\":" << part.indices
               << "}" << (i + 1 == rebuilt.parts.size() ? "\n" : ",\n");
      }
      report << "  ]\n}\n";
      Require(bool(report), "Mesh input report write failed");
      std::cout << "MESH-REBUILT\t" << rebuilt.matches << "\t" << rebuilt.skipped
                << "\t" << rebuilt.parts.size() << "\t" << rebuilt.addedObjects
                << "\t" << rebuilt.bytes.size() << "\n";
      return 0;
    }
    if (argc == 5 && std::string(argv[1]) == "--native-range") {
      auto source = EiemNativeInput::EndfieldInput::Snapshot(
          std::filesystem::u8path(argv[2]), std::stoull(argv[3]));
      auto outputPath = std::filesystem::u8path(argv[4]);
      Require(!std::filesystem::exists(outputPath), "refusing to overwrite native range snapshot");
      auto checked = EiemNativeInput::EndfieldInput::Decode(source);
      std::ofstream output(outputPath,std::ios::binary);
      output.write((const char*)source.data(),source.size());
      Require(bool(output), "native range snapshot write failed");
      auto hash = Sha(source.data(),source.size());
      std::cout << "SNAPSHOT\t" << source.size() << "\t" << Hex(hash.data(),hash.size()) << "\n";
      return 0;
    }
    if (argc == 4 && std::string(argv[1]) == "--endfield-rebuild") {
      auto source = Read(std::filesystem::u8path(argv[2]));
      auto outputPath = std::filesystem::u8path(argv[3]);
      Require(!std::filesystem::exists(outputPath), "refusing to overwrite rebuilt Endfield input");
      auto bundle = EiemNativeInput::EndfieldInput::Decode(source);
      auto rebuilt = EiemNativeInput::EndfieldInput::Encode(source,bundle.members);
      auto checked = EiemNativeInput::EndfieldInput::Decode(rebuilt);
      Require(checked.members.size() == bundle.members.size(), "Endfield rebuild member count changed");
      for (size_t i = 0; i < bundle.members.size(); ++i)
        Require(checked.members[i].name == bundle.members[i].name &&
                    checked.members[i].flags == bundle.members[i].flags &&
                    checked.members[i].bytes == bundle.members[i].bytes,
                "Endfield rebuild changed member bytes/identity");
      std::ofstream output(outputPath,std::ios::binary);
      output.write((const char*)rebuilt.data(),rebuilt.size());
      Require(bool(output), "Endfield rebuilt input write failed");
      std::cout << "REBUILT\t" << rebuilt.size() << "\t" << bundle.members.size() << "\n";
      return 0;
    }
    if (argc == 4 && std::string(argv[1]) == "--endfield-members") {
      auto inputPath = std::filesystem::u8path(argv[2]);
      auto outputPath = std::filesystem::u8path(argv[3]);
      Require(!std::filesystem::exists(outputPath), "refusing to overwrite Endfield member report");
      auto bundle = EiemNativeInput::EndfieldInput::Decode(Read(inputPath));
      std::filesystem::create_directories(outputPath);
      for (const auto& member : bundle.members) {
        auto name = std::filesystem::u8path(member.name);
        Require(name == name.filename(), "unexpected Endfield member path");
        auto path = outputPath / (member.name + ".bytes");
        std::ofstream file(path, std::ios::binary);
        file.write((const char*)member.bytes.data(), member.bytes.size());
        Require(bool(file), "Endfield member write failed");
        auto hash = Sha(member.bytes.data(), member.bytes.size());
        std::cout << "MEMBER\t" << member.name << "\t" << member.bytes.size() << "\t"
                  << Hex(hash.data(), hash.size()) << "\n";
      }
      return 0;
    }
    if (argc == 5 && std::string(argv[1]) == "--match-dir") {
      return MatchPackageDirectory(std::filesystem::u8path(argv[2]),
                                   std::filesystem::u8path(argv[3]),
                                   std::filesystem::u8path(argv[4]));
    }
    if (argc >= 5 && std::string(argv[1]) == "--match") {
      std::vector<std::filesystem::path> native;
      for (int i = 3; i < argc - 1; ++i) native.emplace_back(std::filesystem::u8path(argv[i]));
      return MatchPackage(std::filesystem::u8path(argv[2]), native,
                          std::filesystem::u8path(argv[argc - 1]));
    }
    if (argc >= 3 && std::string(argv[1]) == "--native") {
      auto assets = EiemNativeInput::Inspect(Read(std::filesystem::u8path(argv[2])));
      for (const auto& a : assets)
        std::cout << "ASSET\t" << a.path << "\t" << a.asset << "\t" << a.cab << "\t" << a.pathId
                  << "\t" << a.classId << "\t" << Hex(a.type.data(), a.type.size()) << "\n";
      std::cout << "NATIVE\t" << assets.size() << "\n";
      return 0;
    }
    Require(argc == 2,
            "usage: resource_directory_check <Mod folder> | --native <bundle/SerializedFile>");
    auto p = Load(std::filesystem::u8path(argv[1]));
    size_t vertices = 0, indices = 0, bones = 0, bytes = 0;
    for (const auto& r : p.resources) bytes += r.second->bytes.size();
    for (const auto& m : p.meshes) {
      vertices += m.second.vertices;
      indices += m.second.indices;
      bones += m.second.bones.size();
    }
    std::cout << "PACKAGE\t" << p.id << "\t" << p.meshes.size() << "\t" << p.parts.size() << "\t"
              << p.materials.size() << "\t" << p.textures.size() << "\t" << p.rules.size() << "\t"
              << p.targetIndex.size() << "\t" << p.resources.size() << "\t" << bytes << "\t"
              << vertices << "\t" << indices << "\t" << bones << "\n";
    for (const auto& r : p.resources) {
      std::cout << "RESOURCE\t" << r.first << "\t" << r.second->kind << "\t"
                << r.second->bytes.size() << "\t" << Hex(r.second->type.data(), 16) << "\n";
      for (const auto& b : r.second->blocks)
        std::cout << "BLOCK\t" << r.first << "\t" << b.first << "\t" << b.second.offset << "\t"
                  << b.second.size << "\t" << Hex(b.second.digest.data(), 32) << "\n";
    }
    for (const auto& target : p.targetIndex)
      std::cout << "TARGET\t" << target.first.path << "\t" << target.first.asset << "\t"
                << target.second << "\n";
    for (const auto& part : p.parts)
      std::cout << "PART\t" << part.first << "\t" << part.second.mesh << "\t"
                << part.second.material << "\t" << bool(part.second.visible->Evaluate(p.defaults))
                << "\n";
    return 0;
  } catch (const std::exception& e) {
    std::cerr << "REJECT\t" << e.what() << "\n";
    return 1;
  }
}

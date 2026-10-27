#include "../eff_author_sources.h"
#include "../../src/eiem_compiled_mod_set.h"
#include <iostream>

void CollectChangedFields(const EiemNativeInput::Type& type, const EiemNativeInput::Value& before,
                          const EiemNativeInput::Value& after, const std::string& prefix,
                          std::set<std::string>& fields) {
  if (EiemCompiledMod::EqualCompiledValue(type, before, after)) return;
  const auto& node = type.nodes.at(before.node);
  const auto path = prefix.empty() ? node.name : prefix + "." + node.name;
  if (node.type == "Array" || node.type == "string" || node.type == "TypelessData" ||
      EiemNativeInput::Primitive(node.type) || before.children.size() != after.children.size()) {
    fields.insert(path);
    return;
  }
  for (size_t i = 0; i < before.children.size(); ++i)
    CollectChangedFields(type, before.children[i], after.children[i], path, fields);
}

struct OverlayStats {
  size_t applied = 0, missing = 0, incompatible = 0;
  std::map<std::string, size_t> missingByPath, incompatibleByPath;
};

void MarkMissing(OverlayStats& stats, const std::string& path) {
  ++stats.missing;
  ++stats.missingByPath[path];
}

void MarkIncompatible(OverlayStats& stats, const std::string& path) {
  ++stats.incompatible;
  ++stats.incompatibleByPath[path];
}

size_t ChildIndex(const EiemNativeInput::Type& type, const EiemNativeInput::Value& value,
                  const std::string& name) {
  size_t found = SIZE_MAX;
  for (size_t i = 0; i < value.children.size(); ++i) {
    if (type.nodes.at(value.children[i].node).name != name) continue;
    if (found != SIZE_MAX) return SIZE_MAX;
    found = i;
  }
  return found;
}

bool AggregateNode(const EiemNativeInput::Type& type, size_t node) {
  const auto& value = type.nodes.at(node);
  return !EiemNativeInput::Primitive(value.type) && value.type != "Array" &&
         value.type != "string" && value.type != "TypelessData" && !value.children.empty();
}

EiemNativeInput::Value CopyMappedValue(const EiemNativeInput::Type& sourceType,
                                       const EiemNativeInput::Value& source,
                                       const EiemNativeInput::Type& targetType,
                                       const EiemNativeInput::Value& target,
                                       OverlayStats& stats, const std::string& path) {
  using namespace EiemNativeInput;
  const auto& from = sourceType.nodes.at(source.node);
  const auto& to = targetType.nodes.at(target.node);
  if (AggregateNode(sourceType, source.node) && AggregateNode(targetType, target.node)) {
    auto result = target;
    for (const auto& child : source.children) {
      const auto& name = sourceType.nodes.at(child.node).name;
      const auto sourceIndex = ChildIndex(sourceType, source, name);
      const auto targetIndex = ChildIndex(targetType, target, name);
      const auto childPath = path.empty() ? name : path + "." + name;
      if (sourceIndex == SIZE_MAX || targetIndex == SIZE_MAX) {
        if (targetIndex == SIZE_MAX) MarkMissing(stats, childPath);
        else MarkIncompatible(stats, childPath);
        continue;
      }
      result.children[targetIndex] = CopyMappedValue(sourceType, child, targetType,
                                                     target.children[targetIndex], stats, childPath);
    }
    return result;
  }
  if (from.type == "Array" && to.type == "Array" && from.children.size() == 2 &&
      to.children.size() == 2 && source.count == target.count) {
    const auto fromElement = from.children[1], toElement = to.children[1];
    if (sourceType.nodes.at(fromElement).type != targetType.nodes.at(toElement).type) {
      MarkIncompatible(stats, path);
      return target;
    }
    auto result = target;
    if (Primitive(sourceType.nodes.at(fromElement).type) && !sourceType.nodes.at(fromElement).align &&
        Primitive(targetType.nodes.at(toElement).type) && !targetType.nodes.at(toElement).align) {
      if (source.raw.size() != target.raw.size()) {
        MarkIncompatible(stats, path);
        return target;
      }
      result.raw = source.raw;
      result.count = source.count;
      ++stats.applied;
      return result;
    }
    if (source.children.size() != target.children.size()) {
      MarkIncompatible(stats, path);
      return target;
    }
    for (size_t i = 0; i < source.children.size(); ++i)
      result.children[i] = CopyMappedValue(sourceType, source.children[i], targetType,
                                           target.children[i], stats, path + "[" + std::to_string(i) + "]");
    result.count = source.count;
    return result;
  }
  if (from.type != to.type || source.raw.size() != target.raw.size() || !source.children.empty()) {
    MarkIncompatible(stats, path);
    return target;
  }
  auto result = target;
  result.raw = source.raw;
  result.padding = source.padding;
  ++stats.applied;
  return result;
}

EiemNativeInput::Value OverlayDelta(const EiemNativeInput::Type& beforeType,
                                    const EiemNativeInput::Value& before,
                                    const EiemNativeInput::Type& editedType,
                                    const EiemNativeInput::Value& edited,
                                    const EiemNativeInput::Type& currentType,
                                    const EiemNativeInput::Value& current,
                                    OverlayStats& stats, const std::string& prefix = {}) {
  using namespace EiemNativeInput;
  if (EiemCompiledMod::EqualCompiledValue(beforeType, before, edited)) return current;
  if (AggregateNode(beforeType, before.node) && AggregateNode(editedType, edited.node) &&
      AggregateNode(currentType, current.node)) {
    auto result = current;
    for (const auto& oldChild : before.children) {
      const auto& name = beforeType.nodes.at(oldChild.node).name;
      const auto editedIndex = ChildIndex(editedType, edited, name);
      const auto currentIndex = ChildIndex(currentType, current, name);
      const auto path = prefix.empty() ? name : prefix + "." + name;
      if (editedIndex == SIZE_MAX) {
        MarkIncompatible(stats, path);
        continue;
      }
      if (currentIndex == SIZE_MAX) {
        if (!EiemCompiledMod::EqualCompiledValue(beforeType, oldChild, edited.children[editedIndex]))
          MarkMissing(stats, path);
        continue;
      }
      result.children[currentIndex] = OverlayDelta(beforeType, oldChild, editedType,
                                                   edited.children[editedIndex], currentType,
                                                   current.children[currentIndex], stats, path);
    }
    return result;
  }
  return CopyMappedValue(editedType, edited, currentType, current, stats, prefix);
}

EiemNativeInput::Type OverlayFixtureType(const std::vector<std::pair<std::string, std::string>>& fields) {
  EiemNativeInput::Type type;
  type.nodes.push_back({"Renderer", "Renderer", false, {}});
  for (size_t i = 0; i < fields.size(); ++i) {
    type.nodes.push_back({fields[i].second, fields[i].first, false, {}});
    type.nodes[0].children.push_back(i + 1);
  }
  return type;
}

EiemNativeInput::Value OverlayFixtureValue(const EiemNativeInput::Type& type,
                                          const std::map<std::string, EiemNativeInput::Bytes>& fields) {
  EiemNativeInput::Value root;
  root.node = 0;
  for (size_t i = 0; i < type.nodes[0].children.size(); ++i) {
    EiemNativeInput::Value value;
    value.node = type.nodes[0].children[i];
    value.raw = fields.at(type.nodes[value.node].name);
    root.children.push_back(std::move(value));
  }
  return root;
}

EiemNativeInput::Bytes OverlayFloat(float value) {
  EiemNativeInput::Bytes bytes(sizeof(value));
  std::memcpy(bytes.data(), &value, sizeof(value));
  return bytes;
}

void CheckOverlayContract() {
  using namespace EiemNativeInput;
  auto beforeType = OverlayFixtureType({{"bounds", "float"}, {"enabled", "bool"}});
  auto editedType = OverlayFixtureType({{"bounds", "float"}, {"enabled", "bool"}});
  auto currentType = OverlayFixtureType({{"enabled", "bool"}, {"gameField", "int"}, {"bounds", "float"}});
  auto before = OverlayFixtureValue(beforeType, {{"bounds", OverlayFloat(1.0f)}, {"enabled", {1}}});
  auto edited = OverlayFixtureValue(editedType, {{"bounds", OverlayFloat(2.0f)}, {"enabled", {1}}});
  auto current = OverlayFixtureValue(currentType, {{"enabled", {0}}, {"gameField", {9, 0, 0, 0}},
                                                   {"bounds", OverlayFloat(3.0f)}});
  OverlayStats stats;
  auto rebased = OverlayDelta(beforeType, before, editedType, edited, currentType, current, stats);
  Require(stats.applied == 1 && stats.missing == 0 && stats.incompatible == 0 &&
              Child(currentType, rebased, "enabled").raw == Bytes{0} &&
              Child(currentType, rebased, "gameField").raw == Bytes({9, 0, 0, 0}) &&
              Child(currentType, rebased, "bounds").raw == OverlayFloat(2.0f),
          "field-path overlay rejected reordered schema or overwrote current-only fields");

  auto missingType = OverlayFixtureType({{"enabled", "bool"}});
  auto missing = OverlayFixtureValue(missingType, {{"enabled", {0}}});
  stats = {};
  auto withMissing = OverlayDelta(beforeType, before, editedType, edited, missingType, missing, stats);
  Require(stats.applied == 0 && stats.missing == 1 && stats.incompatible == 0 &&
              Child(missingType, withMissing, "enabled").raw == Bytes{0},
          "missing changed field invalidated the whole logical target");

  auto incompatibleType = OverlayFixtureType({{"bounds", "int"}, {"enabled", "bool"}});
  auto incompatible = OverlayFixtureValue(incompatibleType, {{"bounds", {4, 0, 0, 0}}, {"enabled", {0}}});
  stats = {};
  auto withIncompatible = OverlayDelta(beforeType, before, editedType, edited,
                                       incompatibleType, incompatible, stats);
  Require(stats.applied == 0 && stats.missing == 0 && stats.incompatible == 1 &&
              Child(incompatibleType, withIncompatible, "bounds").raw == Bytes({4, 0, 0, 0}),
          "field type change was treated as a resource identity change");
}

void CheckProductionSemanticOverlayContract() {
  using namespace EiemNativeInput;
  auto makeType = [](const std::vector<std::pair<std::string, std::string>>& fields) {
    Type type; type.classId = 23; type.nodes.push_back({"Renderer", "Renderer", false, {}});
    for (const auto& field : fields) {
      type.nodes.push_back({field.second, field.first, false, {}});
      type.nodes[0].children.push_back(type.nodes.size() - 1);
    }
    return type;
  };
  auto makeValue = [](const Type& type, const std::map<std::string, Bytes>& fields) {
    Value value; value.node = 0;
    for (auto child : type.nodes[0].children) {
      Value field; field.node = child; field.raw = fields.at(type.nodes[child].name);
      value.children.push_back(std::move(field));
    }
    return value;
  };
  auto readField = [](const Type& type, const Value& value, const std::string& name) -> const Value& {
    for (size_t i = 0; i < value.children.size(); ++i)
      if (type.nodes.at(value.children[i].node).name == name) return value.children[i];
    throw std::runtime_error("semantic replay fixture field absent: " + name);
  };
  auto floating = [](float input) { Bytes value(sizeof(input)); std::memcpy(value.data(), &input, sizeof(input)); return value; };

  auto beforeType = makeType({{"bounds", "float"}, {"enabled", "bool"}});
  auto editedType = makeType({{"bounds", "float"}, {"enabled", "bool"}});
  auto currentType = makeType({{"enabled", "bool"}, {"gameField", "int"}, {"bounds", "float"}});
  auto before = makeValue(beforeType, {{"bounds", floating(1.0f)}, {"enabled", {1}}});
  auto edited = makeValue(editedType, {{"bounds", floating(2.0f)}, {"enabled", {1}}});
  auto current = makeValue(currentType, {{"enabled", {0}}, {"gameField", {9, 0, 0, 0}},
                                         {"bounds", floating(3.0f)}});
  EiemSemanticRendererReplay::Stats stats;std::vector<std::string> editedExternals,currentExternals;
  auto rebased = EiemSemanticRendererReplay::Overlay(beforeType, before, editedType, edited,
                                                       currentType, current, editedExternals, currentExternals, stats);
  Require(stats.changed == 1 && stats.missing == 0 && stats.incompatible == 0 &&
          readField(currentType, rebased, "enabled").raw == Bytes{0} &&
          readField(currentType, rebased, "gameField").raw == Bytes({9, 0, 0, 0}) &&
          readField(currentType, rebased, "bounds").raw == floating(2.0f),
          "production semantic replay failed reordered fields or overwrote current-only data");

  auto missingType = makeType({{"enabled", "bool"}});
  auto missing = makeValue(missingType, {{"enabled", {0}}});
  stats = {};currentExternals.clear();
  auto withMissing = EiemSemanticRendererReplay::Overlay(beforeType, before, editedType, edited,
                                                          missingType, missing, editedExternals, currentExternals, stats);
  Require(stats.changed == 0 && stats.missing == 1 && stats.incompatible == 0 &&
          readField(missingType, withMissing, "enabled").raw == Bytes{0},
          "production semantic replay did not report a missing changed field locally");

  auto incompatibleType = makeType({{"bounds", "int"}, {"enabled", "bool"}});
  auto incompatible = makeValue(incompatibleType, {{"bounds", {4, 0, 0, 0}}, {"enabled", {0}}});
  stats = {};currentExternals.clear();
  auto withIncompatible = EiemSemanticRendererReplay::Overlay(beforeType, before, editedType, edited,
                                                               incompatibleType, incompatible, editedExternals, currentExternals, stats);
  Require(stats.changed == 0 && stats.missing == 0 && stats.incompatible == 1 &&
          readField(incompatibleType, withIncompatible, "bounds").raw == Bytes({4, 0, 0, 0}),
          "production semantic replay did not preserve a type-incompatible current value");

  auto arrayType = [](int classId) {
    Type type; type.classId = classId;
    type.nodes.push_back({"Mesh", "Mesh", false, {1}});
    type.nodes.push_back({"Array", "m_Data", false, {2, 3}});
    type.nodes.push_back({"int", "size", false, {}});
    type.nodes.push_back({"float", "data", false, {}});
    return type;
  };
  auto arrayValue = [](const std::vector<float>& values) {
    Value root; root.node = 0;
    Value array; array.node = 1; array.count = values.size(); array.raw.resize(values.size() * sizeof(float));
    if (!values.empty()) std::memcpy(array.raw.data(), values.data(), array.raw.size());
    root.children.push_back(std::move(array)); return root;
  };
  auto meshBeforeType = arrayType(43), meshEditedType = arrayType(43), meshCurrentType = arrayType(43);
  auto meshBefore = arrayValue({1.0f, 2.0f});
  auto meshEdited = arrayValue({3.0f, 4.0f, 5.0f});
  auto meshCurrent = arrayValue({7.0f});
  stats = {}; currentExternals.clear();
  auto resizedMesh = EiemSemanticRendererReplay::Overlay(meshBeforeType, meshBefore, meshEditedType, meshEdited,
      meshCurrentType, meshCurrent, editedExternals, currentExternals, stats, {}, true);
  Require(stats.changed == 1 && stats.incompatible == 0 &&
          readField(meshCurrentType, resizedMesh, "m_Data").count == 3 &&
          readField(meshCurrentType, resizedMesh, "m_Data").raw == readField(meshEditedType, meshEdited, "m_Data").raw,
          "Mesh semantic replay did not resize an edited native data array");

  auto rendererBeforeType = arrayType(23), rendererEditedType = arrayType(23), rendererCurrentType = arrayType(23);
  auto rendererBefore = arrayValue({1.0f, 2.0f});
  auto rendererEdited = arrayValue({3.0f, 4.0f, 5.0f});
  auto rendererCurrent = arrayValue({7.0f});
  stats = {}; currentExternals.clear();
  auto preservedRenderer = EiemSemanticRendererReplay::Overlay(rendererBeforeType, rendererBefore,
      rendererEditedType, rendererEdited, rendererCurrentType, rendererCurrent,
      editedExternals, currentExternals, stats);
  Require(stats.changed == 0 && stats.incompatible == 1 &&
          readField(rendererCurrentType, preservedRenderer, "m_Data").count == 1 &&
          readField(rendererCurrentType, preservedRenderer, "m_Data").raw == readField(rendererCurrentType, rendererCurrent, "m_Data").raw,
          "Renderer semantic replay overwrote a native array whose length changed");
}

EiemNativeInput::Bytes ReadCurrentLogicalBundle(const std::filesystem::path& game, const std::string& logical) {
  auto entry = EiemVfsCatalog::ReadEffectiveVfs(game, {logical}).at(logical);
  EiemNativeInput::Require(!entry.encrypted, "current logical source uses unsupported outer encryption");
  std::ifstream input(entry.physical, std::ios::binary);
  EiemNativeInput::Require(bool(input), "current logical source cannot open");
  input.seekg(std::streamoff(entry.offset));
  EiemNativeInput::Bytes bytes(size_t(entry.size));
  input.read(reinterpret_cast<char*>(bytes.data()), std::streamsize(bytes.size()));
  EiemNativeInput::Require(size_t(input.gcount()) == bytes.size(), "current logical source short read");
  EiemNativeInput::Require(EiemVfsCatalog::Md5(bytes) == entry.md5, "current logical source MD5 differs");
  return bytes;
}

EiemNativeInput::NativeReference ResolveCurrentMesh(const std::filesystem::path& game,
                                                     const std::string& bundle,
                                                     const EiemNativeInput::Identity& identity) {
  using namespace EiemNativeInput;
  auto archive = EndfieldInput::Decode(ReadCurrentLogicalBundle(game, bundle));
  std::vector<NativeReference> matches;
  for (const auto& member : archive.members) if (SerializedHeader(member.bytes)) {
    for (const auto& asset : SerializedAssets(member.bytes, member.name, ParseSerialized(member.bytes))) {
      if (EiemStaticSources::Logical(asset.path) == EiemStaticSources::Logical(identity.path) &&
          asset.asset == identity.asset && asset.classId == 43)
        matches.push_back({EiemStaticSources::Cab(asset.cab), asset.pathId});
    }
  }
  Require(matches.size() == 1, "current logical Mesh path+asset is missing or ambiguous");
  return matches.front();
}

int wmain(int argc, wchar_t** argv) {
  try {
    CheckOverlayContract();
    CheckProductionSemanticOverlayContract();
    if (argc == 2 && std::wstring(argv[1]) == L"--contract-smoke") {
      std::cout << "PASS production semantic replay: reordered fields, current-only fields, missing fields and type mismatch\n";
      return 0;
    }
    using namespace EiemAuthorSources;
    Require(argc == 4, "game_update_compat_probe <game> <mods> <mod-id>");
    const std::filesystem::path game = argv[1], mods = argv[2];
    const auto idBytes = WideCharToMultiByte(CP_UTF8, 0, argv[3], -1, nullptr, 0, nullptr, nullptr);
    Require(idBytes > 1, "Mod id conversion failed");
    std::string modId(size_t(idBytes), '\0');
    WideCharToMultiByte(CP_UTF8, 0, argv[3], -1, modId.data(), idBytes, nullptr, nullptr);
    modId.resize(size_t(idBytes - 1));
    std::filesystem::path mod;
    for (const auto& root : std::filesystem::directory_iterator(mods)) {
      if (!root.is_directory()) continue;
      auto sections = EiemDirectory::ParseIni(EiemDirectory::Read(root.path() / "mod.ini"));
      for (const auto& section : sections) if (section.id == "Mod" && section.values.at("id") == modId) mod = root.path();
    }
    Require(!mod.empty(), "requested Mod not found");
    auto artifact = EiemCompiledModSet::ReadPublishedArtifact(mod, game);
    auto selection = EiemStaticSources::LoadMod(mod, game);
    for (size_t i = 0; i < artifact.manifest.plan.originals.size(); ++i) {
      auto& source = artifact.manifest.plan.originals[i];
      std::string logical;
      for (const auto& row : selection.inputs)
        if (EiemStaticSources::SourceFile(row.first, row.second.revision) == source.file) { logical = row.first; break; }
      if (logical != "bundles/windows/main/68c216df3439cc0d11c16c01.ab") continue;
      auto oldBytes = EiemResourceReloadPlan::ReadSource({}, source);
      auto currentVfsEntry = EiemVfsCatalog::ReadEffectiveVfs(game, {logical}).at(logical);
      Require(!currentVfsEntry.encrypted, "effective source uses unsupported outer encryption");
      std::ifstream input(currentVfsEntry.physical, std::ios::binary); Require(bool(input), "current source cannot open");
      input.seekg(std::streamoff(currentVfsEntry.offset)); EiemNativeInput::Bytes currentBytes(size_t(currentVfsEntry.size));
      input.read(reinterpret_cast<char*>(currentBytes.data()), std::streamsize(currentBytes.size()));
      Require(size_t(input.gcount()) == currentBytes.size(), "current source short read");
      auto oldBundle = EndfieldInput::Decode(oldBytes), currentBundle = EndfieldInput::Decode(currentBytes);
      std::map<std::string, const EndfieldInput::Member*> oldMembers, currentMembers;
      for (const auto& member : oldBundle.members) oldMembers.emplace(member.name, &member);
      for (const auto& member : currentBundle.members) currentMembers.emplace(member.name, &member);
      std::map<Identity, NativeReference> currentMeshReferences;
      std::map<NativeReference, Identity> currentMeshIdentities;
      for (const auto& rule : artifact.manifest.plan.rendererRules.at(i)) {
        auto meshLogical = EiemStaticSources::Logical(rule.mesh.path);
        auto bundle = selection.assets.at(meshLogical);
        auto reference = ResolveCurrentMesh(game, bundle, rule.mesh);
        currentMeshReferences.emplace(rule.mesh, reference);
        auto inserted = currentMeshIdentities.emplace(reference, rule.mesh);
        Require(inserted.second || (inserted.first->second.path == rule.mesh.path &&
                                    inserted.first->second.asset == rule.mesh.asset),
                "current logical Mesh identities alias one native reference");
      }
      std::set<NativeReference> selectedCurrentMeshes;
      for (const auto& row : currentMeshIdentities) selectedCurrentMeshes.insert(row.first);
      size_t pathBasedConsumers = 0;
      for (const auto& member : currentBundle.members) if (SerializedHeader(member.bytes)) {
        auto layout = ParseSerialized(member.bytes);
        for (const auto& consumer : MeshConsumers(member.bytes, layout, member.name, selectedCurrentMeshes,
                                                   EiemStaticSources::Cab)) {
          auto reference = consumer.mesh;
          reference.first = EiemStaticSources::Cab(reference.first);
          Require(currentMeshIdentities.count(reference), "current PFB consumer has no logical Mesh identity");
          ++pathBasedConsumers;
        }
      }
      std::cout << "LOGICAL_MESH_REBIND identities=" << currentMeshReferences.size()
                << " currentPathBasedConsumers=" << pathBasedConsumers << '\n';
      size_t sameSize = 0, changedBundleSize = 0, added = 0, removed = 0, sameLayout = 0, changedLayout = 0;
      for (const auto& row : currentMembers) {
        auto old = oldMembers.find(row.first); if (old == oldMembers.end()) { ++added; continue; }
        if (old->second->bytes.size() == row.second->bytes.size()) ++sameSize; else ++changedBundleSize;
        if (SerializedHeader(row.second->bytes) && SerializedHeader(old->second->bytes)) {
          auto a = ParseSerialized(old->second->bytes), b = ParseSerialized(row.second->bytes);
          bool layout = a.order == b.order && a.types.size() == b.types.size() && a.objects.size() == b.objects.size();
          for (size_t t = 0; layout && t < a.types.size(); ++t)
            layout = a.types[t].classId == b.types[t].classId && a.types[t].hash == b.types[t].hash;
          for (const auto& object : a.objects) {
            auto next = b.objects.find(object.first);
            layout = layout && next != b.objects.end() && object.second.type == next->second.type;
          }
          if (layout) ++sameLayout; else ++changedLayout;
        }
      }
      for (const auto& row : oldMembers) if (!currentMembers.count(row.first)) ++removed;
      std::cout << "SOURCE mod=" << modId << " logical=" << logical << " oldBytes=" << oldBytes.size()
                << " currentBytes=" << currentBytes.size() << " oldMembers=" << oldMembers.size()
                << " currentMembers=" << currentMembers.size() << " sameMemberSize=" << sameSize
                << " changedMemberSize=" << changedBundleSize << " addedMembers=" << added
                << " removedMembers=" << removed << " sameSerializedLayout=" << sameLayout
                << " changedSerializedLayout=" << changedLayout << " explicitResources=" << artifact.explicitResources << '\n';
      struct ObjectDesc { int classId = 0; std::array<unsigned char, 16> type{}; size_t size = 0; };
      std::map<int64_t, ObjectDesc> oldObjects, currentObjects;
      auto describeObjects = [](const auto& members, auto& output) {
        for (const auto& member : members) if (SerializedHeader(member.second->bytes)) {
          auto layout = ParseSerialized(member.second->bytes);
          for (const auto& object : layout.objects) {
            const auto& type = layout.types.at(object.second.type);
            output.emplace(object.first, ObjectDesc{type.classId, type.hash, object.second.size});
          }
        }
      };
      describeObjects(oldMembers, oldObjects); describeObjects(currentMembers, currentObjects);
      size_t targetCount = 0, targetFound = 0, targetSchemaSame = 0, targetSizeSame = 0;
      for (const auto& rule : artifact.manifest.plan.rendererRules.at(i)) {
        ++targetCount; auto next = currentObjects.find(rule.id);
        if (next == currentObjects.end()) continue; ++targetFound;
        auto old = oldObjects.find(rule.id); if (old == oldObjects.end()) continue;
        targetSchemaSame += old->second.classId == next->second.classId && old->second.type == next->second.type;
        targetSizeSame += old->second.size == next->second.size;
      }
      std::cout << "TARGET_SUMMARY total=" << targetCount << " foundInCurrent=" << targetFound
                << " sameObjectSchema=" << targetSchemaSame << " sameObjectSize=" << targetSizeSame << '\n';
      auto oldEditedBundle = EiemCompiledModSet::Replay(artifact, i, oldBundle, artifact.preparedFrames);
      std::map<std::string, size_t> changedFields; size_t changedTargetObjects = 0;
      for (const auto& member : oldBundle.members) {
        auto edited = std::find_if(oldEditedBundle.members.begin(), oldEditedBundle.members.end(),
                                   [&](const auto& value) { return value.name == member.name; });
        if (edited == oldEditedBundle.members.end() || !SerializedHeader(member.bytes)) continue;
        auto beforeLayout = ParseSerialized(member.bytes), afterLayout = ParseSerialized(edited->bytes);
        for (const auto& rule : artifact.manifest.plan.rendererRules.at(i)) {
          auto before = beforeLayout.objects.find(rule.id), after = afterLayout.objects.find(rule.id);
          if (before == beforeLayout.objects.end() || after == afterLayout.objects.end()) continue;
          if (EiemCompiledModSet::ObjectBytes(member.bytes, before->second) ==
              EiemCompiledModSet::ObjectBytes(edited->bytes, after->second)) continue;
          ++changedTargetObjects;
          auto typeIndex = before->second.type;
          auto oldValue = DecodeValue(beforeLayout.types.at(typeIndex), 0,
                                      EiemCompiledModSet::ObjectBytes(member.bytes, before->second));
          auto newValue = DecodeValue(afterLayout.types.at(after->second.type), 0,
                                      EiemCompiledModSet::ObjectBytes(edited->bytes, after->second));
          std::set<std::string> fields;
          CollectChangedFields(beforeLayout.types.at(typeIndex), oldValue, newValue, {}, fields);
          for (const auto& field : fields) ++changedFields[field];
        }
      }
      std::cout << "EDITED_TARGETS changedObjects=" << changedTargetObjects;
      for (const auto& field : changedFields) std::cout << " field=" << field.first << ':' << field.second;
      std::cout << '\n';
      for (const auto& member : currentBundle.members) if (SerializedHeader(member.bytes)) {
        auto old = oldMembers.find(member.name); if (old == oldMembers.end()) continue;
        auto a = ParseSerialized(old->second->bytes), b = ParseSerialized(member.bytes);
        size_t retained = 0, changedSchema = 0, changedObjectSize = 0, addedObjects = 0, removedObjects = 0;
        for (const auto& object : a.objects) {
          auto next = b.objects.find(object.first);
          if (next == b.objects.end()) { ++removedObjects; continue; }
          ++retained;
          if (a.types.at(object.second.type).classId != b.types.at(next->second.type).classId ||
              a.types.at(object.second.type).hash != b.types.at(next->second.type).hash) ++changedSchema;
          if (object.second.size != next->second.size) ++changedObjectSize;
        }
        for (const auto& object : b.objects) if (!a.objects.count(object.first)) ++addedObjects;
        std::cout << "LAYOUT member=" << member.name << " oldTypes=" << a.types.size()
                  << " currentTypes=" << b.types.size() << " retainedObjects=" << retained
                  << " changedObjectSchema=" << changedSchema << " changedObjectSize=" << changedObjectSize
                  << " addedObjects=" << addedObjects << " removedObjects=" << removedObjects << '\n';
      }
      for (const auto& rule : artifact.manifest.plan.rendererRules.at(i)) {
        std::cout << "TARGET cab=" << rule.cab << " rendererPathID=" << rule.id
                  << " mesh=" << rule.mesh.path << '#' << rule.mesh.asset << '\n';
      }
      try {
        auto replay = EiemCompiledModSet::Replay(artifact, i, currentBundle, artifact.preparedFrames);
        std::cout << "REPLAY compatibleRecipeApplication=1 outputMembers=" << replay.members.size() << '\n';
      } catch (const std::exception& error) {
        std::cout << "REPLAY compatibleRecipeApplication=0 reason=" << error.what() << '\n';
      }
      try {
        auto unchecked = artifact;
        unchecked.explicitResources = true;
        auto replay = EiemCompiledModSet::Replay(unchecked, i, currentBundle, unchecked.preparedFrames);
        std::map<std::string, const EndfieldInput::Member*> replayMembers;
        for (const auto& member : replay.members) replayMembers.emplace(member.name, &member);
        std::set<int64_t> replayObjects;
        size_t replayTypes = 0;
        for (const auto& member : replay.members) if (SerializedHeader(member.bytes)) {
          auto layout = ParseSerialized(member.bytes);
          replayTypes += layout.types.size();
          for (const auto& object : layout.objects) replayObjects.insert(object.first);
        }
        size_t targetsPresent = 0, targetsChanged = 0, updatedObjectsRetained = 0;
        size_t currentAddedObjects = 0, currentRemovedObjects = 0;
        std::map<std::string, size_t> changedFieldsAfterRebase;
        for (const auto& rule : artifact.manifest.plan.rendererRules.at(i)) {
          auto currentMember = currentMembers.find(rule.cab);
          auto editedMember = replayMembers.find(rule.cab);
          if (currentMember == currentMembers.end() || editedMember == replayMembers.end()) continue;
          auto before = ParseSerialized(currentMember->second->bytes);
          auto after = ParseSerialized(editedMember->second->bytes);
          auto a = before.objects.find(rule.id), b = after.objects.find(rule.id);
          if (a == before.objects.end() || b == after.objects.end()) continue;
          ++targetsPresent;
          if (EiemCompiledModSet::ObjectBytes(currentMember->second->bytes, a->second) !=
              EiemCompiledModSet::ObjectBytes(editedMember->second->bytes, b->second)) {
            ++targetsChanged;
            auto oldValue = DecodeValue(before.types.at(a->second.type), 0,
                                        EiemCompiledModSet::ObjectBytes(currentMember->second->bytes, a->second));
            auto newValue = DecodeValue(after.types.at(b->second.type), 0,
                                        EiemCompiledModSet::ObjectBytes(editedMember->second->bytes, b->second));
            std::set<std::string> fields;
            CollectChangedFields(before.types.at(a->second.type), oldValue, newValue, {}, fields);
            for (const auto& field : fields) ++changedFieldsAfterRebase[field];
          }
        }
        for (const auto& row : currentObjects) if (!oldObjects.count(row.first)) {
          ++currentAddedObjects;
          if (replayObjects.count(row.first)) ++updatedObjectsRetained;
        }
        std::cout << "UNVERIFIED_REPLAY decoded=1 outputMembers=" << replay.members.size()
                  << " serializedObjects=" << replayObjects.size() << " serializedTypes=" << replayTypes
                  << " targetRenderersPresent=" << targetsPresent << " targetRenderersChanged=" << targetsChanged
                  << " currentAddedObjectsRetained=" << updatedObjectsRetained << '/' << currentAddedObjects
                  << " currentRemovedObjectsReintroduced=";
        size_t reintroduced = 0;
        for (const auto& row : oldObjects) if (!currentObjects.count(row.first)) {
          ++currentRemovedObjects;
          if (replayObjects.count(row.first)) ++reintroduced;
        }
        std::cout << reintroduced << '/' << currentRemovedObjects;
        for (const auto& field : changedFieldsAfterRebase) std::cout << " field=" << field.first << ':' << field.second;
        std::cout << '\n';
      } catch (const std::exception& error) {
        std::cout << "UNVERIFIED_REPLAY decoded=0 reason=" << error.what() << '\n';
      }
      try {
        auto semanticOldEditedBundle = EiemCompiledModSet::Replay(artifact, i, oldBundle, artifact.preparedFrames);
        auto currentEdited = currentBundle;
        std::map<std::string, const EndfieldInput::Member*> oldEditedMembers;
        for (const auto& member : semanticOldEditedBundle.members) oldEditedMembers.emplace(member.name, &member);
        std::map<NativeReference, Identity> currentMeshes;
        std::set<Identity> resolvedMeshes;
        for (const auto& rule : artifact.manifest.plan.rendererRules.at(i)) {
          if (!resolvedMeshes.insert(rule.mesh).second) continue;
          const auto meshBundle = selection.assets.at(EiemStaticSources::Logical(rule.mesh.path));
          auto reference = ResolveCurrentMesh(game, meshBundle, rule.mesh);
          reference.first = EiemStaticSources::Cab(reference.first);
          Require(currentMeshes.emplace(reference, rule.mesh).second,
                  "current logical Mesh reference aliases another identity");
        }
        std::set<NativeReference> selectedMeshes;
        for (const auto& row : currentMeshes) selectedMeshes.insert(row.first);
        size_t semanticTargets = 0, semanticChangedTargets = 0, semanticCurrentAddedObjects = 0;
        size_t semanticMissingFields = 0, semanticIncompatibleFields = 0;
        std::map<std::string, size_t> semanticFields;
        std::map<std::string, size_t> semanticMissingByPath, semanticIncompatibleByPath;
        for (auto& member : currentEdited.members) {
          if (!SerializedHeader(member.bytes)) continue;
          auto currentLayout = ParseSerialized(member.bytes); std::map<int64_t, Bytes> changes;
          for (const auto& object : currentLayout.objects) if (!oldObjects.count(object.first)) ++semanticCurrentAddedObjects;
          for (const auto& consumer : MeshConsumers(member.bytes, currentLayout, member.name,
                                                     selectedMeshes, EiemStaticSources::Cab)) {
            auto logicalMesh = currentMeshes.find({EiemStaticSources::Cab(consumer.mesh.first), consumer.mesh.second});
            if (logicalMesh == currentMeshes.end()) continue;
            const EiemResourceReloadPlan::RendererRule* templateRule = nullptr;
            for (const auto& rule : artifact.manifest.plan.rendererRules.at(i)) {
              if (rule.mesh.path != logicalMesh->second.path || rule.mesh.asset != logicalMesh->second.asset) continue;
              if (!templateRule) templateRule = &rule;
              if (rule.cab == member.name && rule.id == consumer.renderer) { templateRule = &rule; break; }
            }
            if (!templateRule) continue;
            auto oldBase = oldMembers.find(templateRule->cab), oldEdited = oldEditedMembers.find(templateRule->cab);
            if (oldBase == oldMembers.end() || oldEdited == oldEditedMembers.end()) continue;
            auto before = ParseSerialized(oldBase->second->bytes), edited = ParseSerialized(oldEdited->second->bytes);
            auto b = before.objects.find(templateRule->id), e = edited.objects.find(templateRule->id);
            auto c = currentLayout.objects.find(consumer.renderer);
            if (b == before.objects.end() || e == edited.objects.end() || c == currentLayout.objects.end()) continue;
            const auto& beforeType = before.types.at(b->second.type);
            const auto& editedType = edited.types.at(e->second.type);
            const auto& currentType = currentLayout.types.at(c->second.type);
            auto beforeBytes = EiemCompiledModSet::ObjectBytes(oldBase->second->bytes, b->second);
            auto editedBytes = EiemCompiledModSet::ObjectBytes(oldEdited->second->bytes, e->second);
            if (beforeBytes == editedBytes) continue;
            auto beforeValue = DecodeValue(beforeType, 0, beforeBytes);
            auto editedValue = DecodeValue(editedType, 0, editedBytes);
            auto currentValue = DecodeValue(currentType, 0, EiemCompiledModSet::ObjectBytes(member.bytes, c->second));
            OverlayStats overlay;
            auto rebased = OverlayDelta(beforeType, beforeValue, editedType, editedValue,
                                        currentType, currentValue, overlay);
            auto rebasedBytes = EncodeValue(currentType, rebased);
            if (rebasedBytes != EiemCompiledModSet::ObjectBytes(member.bytes, c->second)) {
              changes.emplace(consumer.renderer, std::move(rebasedBytes)); ++semanticChangedTargets;
            }
            ++semanticTargets;
            semanticMissingFields += overlay.missing;
            semanticIncompatibleFields += overlay.incompatible;
            for (const auto& field : overlay.missingByPath) semanticMissingByPath[field.first] += field.second;
            for (const auto& field : overlay.incompatibleByPath) semanticIncompatibleByPath[field.first] += field.second;
            std::set<std::string> fields;
            CollectChangedFields(beforeType, beforeValue, editedValue, {}, fields);
            for (const auto& field : fields) ++semanticFields[field];
          }
          if (!changes.empty()) {
            member.bytes = BuildSerialized(member.bytes, currentLayout, changes);
            EiemCompiledModSet::VerifyObjectReferences(member.bytes, currentLayout, ParseSerialized(member.bytes),
                                                       currentMembers.at(member.name)->bytes);
          }
        }
        std::set<int64_t> semanticObjects;
        size_t semanticNewRetained = 0;
        for (const auto& member : currentEdited.members) if (SerializedHeader(member.bytes)) {
          auto parsed = ParseSerialized(member.bytes);
          for (const auto& object : parsed.objects) semanticObjects.insert(object.first);
        }
        for (const auto& row : currentObjects) if (!oldObjects.count(row.first) && semanticObjects.count(row.first)) ++semanticNewRetained;
        std::cout << "SEMANTIC_RENDERER_REBASE targetsLocated=" << semanticTargets
                  << " changed=" << semanticChangedTargets << " newObjectsRetained=" << semanticNewRetained
                  << '/' << semanticCurrentAddedObjects << " missingFields=" << semanticMissingFields
                  << " incompatibleFields=" << semanticIncompatibleFields << " objectReferencesValid=1";
        for (const auto& field : semanticFields) std::cout << " field=" << field.first << ':' << field.second;
        for (const auto& field : semanticMissingByPath) std::cout << " missing=" << field.first << ':' << field.second;
        for (const auto& field : semanticIncompatibleByPath) std::cout << " incompatible=" << field.first << ':' << field.second;
        std::cout << '\n';
      } catch (const std::exception& error) {
        std::cout << "SEMANTIC_RENDERER_REBASE decoded=0 reason=" << error.what() << '\n';
      }
      return 0;
    }
    throw std::runtime_error("requested logical source absent from compiled Mod");
  } catch (const std::exception& error) {
    std::cerr << "FAIL game_update_compat_probe: " << error.what() << '\n';
    return 1;
  }
}

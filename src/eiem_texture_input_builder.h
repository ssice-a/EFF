#pragma once
#include "eiem_serialized_writer.h"

namespace EiemNativeInput {
// Pixel conversion is an export-time task. This writer consumes RGBA32/mips
// described by TEX metadata and preserves existing stream prefixes.
using AssetIndex = std::map<Identity, Asset>;
inline void AddAsset(AssetIndex& index, Asset asset) {
    if (asset.asset.empty()) return;
    Identity identity{asset.path, asset.asset};
    auto found = index.find(identity);
    Require(found == index.end() ||
                (found->second.cab == asset.cab && found->second.pathId == asset.pathId &&
                 found->second.classId == asset.classId && found->second.type == asset.type),
            "ambiguous current loading-input identity: " + identity.path + " / " + identity.asset);
    index.emplace(std::move(identity), std::move(asset));
}
inline void AddAssets(AssetIndex& index, std::vector<Asset> assets) {
  for(auto& asset:assets)AddAsset(index,std::move(asset));
}
inline void AddAssets(AssetIndex& index, const Bytes& input) {AddAssets(index,Inspect(input));}
inline void SetFloat(const Type& type, Value& value, double number) {
  Require(type.nodes.at(value.node).type == "float" && std::isfinite(number) &&
              std::abs(number) <= 3.402823466e38, "unsupported/nonfinite serialized float");
  float data = (float)number;
  uint32_t bits;
  memcpy(&bits, &data, 4);
  value.raw.resize(4);
  WriteInteger(value.raw, 0, bits, 4);
}
inline std::pair<std::string, int64_t> PointerIdentity(const Type& type, const Value& pointer,
                                                      const SerializedLayout& layout,
                                                      const std::string& cab) {
  auto fileId = Integer(Child(type, pointer, "m_FileID"));
  auto pathId = (int64_t)Integer(Child(type, pointer, "m_PathID"));
  Require(fileId <= layout.externals.size() && pathId, "invalid/null serialized dependency pointer");
  return {fileId ? layout.externals[(size_t)fileId - 1] : cab, pathId};
}
struct TextureWrite {
  Identity target;
  std::string cab, stream;
  int64_t id = 0;
  size_t offset = 0, bytes = 0;
  int64_t sourceId = 0;
};
struct TextureInputBuild {
  Bytes bytes;
  size_t materialMatches = 0, textureBindings = 0, changedMaterials = 0;
  std::vector<TextureWrite> textures;
  bool changedPreloads = false;
};
inline int64_t AllocateTextureId(const std::string& cab, const Package& package,
                                 const std::string& section, const Texture& texture) {
  (void)texture;
  auto label = cab + ":" + package.id + ":texture:" + section;
  auto hash = Sha((const unsigned char*)label.data(), label.size());
  uint64_t id = 0;
  for (size_t i = 0; i < 8; ++i) id |= uint64_t(hash[i]) << (8 * i);
  Require(id != 0, "allocated Texture PathID is zero");
  return (int64_t)id;
}
using TextureReference = std::pair<std::string, int64_t>;
using TextureExpansion = std::map<TextureReference, std::vector<int64_t>>;
inline int64_t AllocateMaterialId(const std::string& cab,const Package& package,const std::string& section){
 auto label=cab+":"+package.id+":material:"+section;auto hash=Sha((const unsigned char*)label.data(),label.size());uint64_t id=0;for(size_t i=0;i<8;++i)id|=uint64_t(hash[i])<<(8*i);Require(id,"allocated Material PathID is zero");return int64_t(id);
}
inline bool ExpandTexturePreloads(const Type& type, Value& bundle,
    const SerializedLayout& layout, const std::string& cab,
    const TextureExpansion& expansion, std::set<TextureReference>& referenced) {
  auto& preloads = ArrayValue(type, Child(type, bundle, "m_PreloadTable"));
  std::vector<Value> output;
  std::vector<size_t> offsets{0};
  for (const auto& pointer : preloads.children) {
    output.push_back(pointer);
    if (Integer(Child(type, pointer, "m_PathID"))) {
      auto original = PointerIdentity(type, pointer, layout, cab);
      auto extras = expansion.find(original);
      if (extras != expansion.end()) {
        referenced.insert(original);
        for (auto id : extras->second) {
          auto copy = pointer; // Same CAB and existing FileID; only the object changes.
          SetInteger(type, Child(type, copy, "m_PathID"), (uint64_t)id);
          output.push_back(std::move(copy));
        }
      }
    }
    offsets.push_back(output.size());
  }
  if (output.size() == preloads.count) return false;
  auto update = [&](Value& info) {
    auto& start = Child(type, info, "preloadIndex");
    auto& size = Child(type, info, "preloadSize");
    auto first = Integer(start), count = Integer(size);
    Require(first < offsets.size() && count <= offsets.size() - 1 - first,
            "Texture source preload interval");
    SetInteger(type, start, offsets[(size_t)first]);
    SetInteger(type, size, offsets[(size_t)(first + count)] - offsets[(size_t)first]);
  };
  for (const char* name : {"m_Container", "m_HashContainer"})
    for (auto& pair : ArrayValue(type, Child(type, bundle, name)).children)
      update(Child(type, pair, "second"));
  update(Child(type, bundle, "m_MainAsset"));
  preloads.children = std::move(output); preloads.count = preloads.children.size();
  return true;
}
inline Value ApplyMaterial(const Type& type, Value value, const Resource& resource,
                             const Material& material, const Package& package,
                             const AssetIndex& assets, const SerializedLayout& layout,
                             const std::string& cab, size_t& bindings) {
  Require(type.classId == 21 && resource.kind == 2 && type.hash == resource.type,
          "Material serialized class/schema mismatch");
  for (const auto& block : resource.blocks) {
    Require(block.first != "m_Name" && !Begins(block.first, "@"),
            "Material delta cannot change its loading identity");
    auto& field = Child(type, value, block.first);
    Bytes bytes(resource.bytes.begin() + block.second.offset,
                resource.bytes.begin() + block.second.offset + block.second.size);
    field = DecodeValue(type, field.node, bytes);
    Require(EncodeValue(type, field) == bytes, "Material field representation changed on decode");
  }
  auto& saved = Child(type, value, "m_SavedProperties");
  auto& environments = ArrayValue(type, Child(type, saved, "m_TexEnvs"));
  for (const auto& edit : material.textures) {
    const auto& texture = package.textures.at(edit.second);
    auto target = assets.find(texture.source);
    Require(target != assets.end() && target->second.classId == 28,
            "Material texture identity absent from current input graph");
    Value* matched = nullptr;
    for (auto& pair : environments.children) {
      const auto& key = Child(type, pair, "first");
      if (Text(key.raw.data(), key.raw.size()) != edit.first) continue;
      Require(!matched, "ambiguous Material texture property: " + edit.first);
      matched = &Child(type, Child(type, pair, "second"), "m_Texture");
    }
    Require(matched, "Material texture property absent: " + edit.first);
    auto current = Integer(Child(type,*matched,"m_PathID")) ? PointerIdentity(type, *matched, layout, cab) : TextureReference{cab,0};
    // The compiler resolves the serialization template from the material slot.
    // The clone lives in the same CAB, so its FileID is already registered.
    Require(!current.second || (current.first == target->second.cab && current.second == target->second.pathId),
            "new Material texture dependency requires preload registration");
    auto external=std::find(layout.externals.begin(),layout.externals.end(),target->second.cab);
    Require(target->second.cab==cab||external!=layout.externals.end(),"null Material texture schema dependency is not registered");
    SetInteger(type,Child(type,*matched,"m_FileID"),target->second.cab==cab?0:uint64_t(external-layout.externals.begin()+1));
    SetInteger(type,Child(type,*matched,"m_PathID"),uint64_t(AllocateTextureId(target->second.cab,package,edit.second,texture)));
    ++bindings;
  }
  return value;
}
inline Value ApplyTexture(const Type& type, Value value, const Texture& texture,
                            const Resource& resource, const std::string& cab,
                            std::vector<EndfieldInput::Member>& members,
                            const std::map<std::string, size_t>& sourceStreams,
                            TextureWrite& record) {
  Require(type.classId == 28 && resource.kind == 4 &&
              (resource.blocks.size() == 1 || (resource.blocks.size() == 2 && resource.blocks.count("@texture"))) &&
              resource.blocks.count("pixels"), "Texture serialized class/resource mismatch");
  auto platform = Child(type, value, "m_PlatformBlob");
  Require(ArrayValue(type, platform).count == 0 && Child(type, value, "image data").raw.empty() &&
              Integer(Child(type, value, "m_ImageCount")) == 1 &&
              Integer(Child(type, value, "m_TextureDimension")) == 2,
          "Texture requires streamed native Texture2D without platform blob");
  auto& stream = Child(type, value, "m_StreamData");
  const auto& path = Child(type, stream, "path");
  auto streamPath = Text(path.raw.data(), path.raw.size());
  auto slash = streamPath.find_last_of("/\\");
  auto name = slash == streamPath.npos ? streamPath : streamPath.substr(slash + 1);
  Require(name == cab + ".resS", "Texture stream is not the current local CAB member");
  EndfieldInput::Member* member = nullptr;
  for (auto& candidate : members) {
    if (candidate.name != name) continue;
    Require(!member, "ambiguous current Texture stream member");
    member = &candidate;
  }
  Require(member, "current Texture stream member absent");
  auto sourceOffset = Integer(Child(type, stream, "offset"));
  auto sourceSize = Integer(Child(type, stream, "size"));
  auto originalSize = sourceStreams.at(name);
  Require(sourceOffset <= originalSize && sourceSize &&
              sourceSize <= originalSize - sourceOffset, "current Texture stream range");
  const auto& pixels = resource.blocks.at("pixels");
  size_t expected = 0;
  unsigned width = texture.width, height = texture.height;
  for (unsigned i = 0; i < texture.mips; ++i) {
    expected = AddSize(expected,MultiplySize(MultiplySize(width,height),4));
    width = std::max(1u, width / 2);
    height = std::max(1u, height / 2);
  }
  Require(expected == pixels.size, "Texture pixels differ from INI mip size");
  size_t offset = AlignSize(member->bytes.size(),16);
  AddSize(offset,pixels.size);
  member->bytes.resize(offset);
  member->bytes.insert(member->bytes.end(), resource.bytes.begin() + pixels.offset,
                       resource.bytes.begin() + pixels.offset + pixels.size);
  const std::pair<const char*, uint64_t> fields[] = {
      {"m_Width", texture.width}, {"m_Height", texture.height}, {"m_CompleteImageSize", pixels.size},
      {"m_MipsStripped", 0}, {"m_TextureFormat", 4}, {"m_MipCount", texture.mips},
      {"m_IsPreProcessed", 0}, {"m_StreamingMipmaps", 0}, {"m_ColorSpace", texture.linear ? 0u : 1u}};
  for (const auto& field : fields) SetInteger(type, Child(type, value, field.first), field.second);
  auto& settings = Child(type, value, "m_TextureSettings");
  SetInteger(type, Child(type, settings, "m_FilterMode"), texture.filter);
  SetInteger(type, Child(type, settings, "m_Aniso"), texture.aniso);
  SetFloat(type, Child(type, settings, "m_MipBias"), texture.bias);
  for (const char* field : {"m_WrapU", "m_WrapV", "m_WrapW"})
    SetInteger(type, Child(type, settings, field), texture.wrap);
  SetInteger(type, Child(type, stream, "offset"), offset);
  SetInteger(type, Child(type, stream, "size"), pixels.size);
  record.stream = name;
  record.offset = offset;
  record.bytes = pixels.size;
  return value;
}
inline void ResolveTextureTemplates(Package& package, const std::vector<Bytes>& inputs,
                                    const AssetIndex& index,
                                    const std::vector<EndfieldInput::Bundle>* decoded = nullptr) {
  std::set<std::string> authored;
  for (const auto& texture : package.textures)
    if (texture.second.source.path.empty()) authored.insert(texture.first);
  if (authored.empty()) return;
  std::map<TextureReference, Identity> identities;
  std::map<Identity, std::vector<const Material*>> materialCopies;
  std::set<std::string> materialCabs;
  for (const auto& material : package.materials) {
    materialCabs.insert(index.at(material.second.source).cab);
    materialCopies[material.second.source].push_back(&material.second);
  }
  for (const auto& asset : index)
    if (asset.second.classId == 28) identities.emplace(TextureReference{asset.second.cab, asset.second.pathId}, asset.first);
  std::map<std::string, Identity> templates;
  Require(!decoded || decoded->size() == inputs.size(), "decoded template source coverage differs");
  for (size_t i = 0; i < inputs.size(); ++i) {
    auto local = decoded ? EndfieldInput::Bundle{} : EndfieldInput::Decode(inputs[i]);
    const auto& bundle = decoded ? decoded->at(i) : local;
    for (const auto& member : bundle.members) {
      if (!materialCabs.count(member.name)) continue;
      auto layout = ParseSerialized(member.bytes);
      for (const auto& asset : SerializedAssets(member.bytes, member.name, layout)) {
        auto copies = materialCopies.find(Identity{asset.path, asset.asset});
        if (copies == materialCopies.end()) continue;
        const auto& type = layout.types.at(layout.objects.at(asset.pathId).type);
        auto value = ObjectValue(member.bytes, layout, asset.pathId);
        TextureReference fallback;
        for (const auto& pair : ArrayValue(type, Child(type, Child(type, value, "m_SavedProperties"), "m_TexEnvs")).children) {
          const auto& slot=Child(type,Child(type,pair,"second"),"m_Texture");if(!Integer(Child(type,slot,"m_PathID")))continue;
          auto pointer=PointerIdentity(type,slot,layout,member.name);
          if(pointer.second&&identities.count(pointer)){fallback=pointer;break;}
        }
        for (const auto& pair : ArrayValue(type, Child(type, Child(type, value, "m_SavedProperties"), "m_TexEnvs")).children) {
          const auto& name = Child(type, pair, "first");
          auto property = Text(name.raw.data(), name.raw.size());
          for (const auto* material : copies->second) {
          auto edit = material->textures.find(property);
          if (edit == material->textures.end() || !authored.count(edit->second)) continue;
          const auto& slot=Child(type,Child(type,pair,"second"),"m_Texture");
          auto pointer = Integer(Child(type,slot,"m_PathID")) ? PointerIdentity(type,slot,layout,member.name) : TextureReference{member.name,0};
          if(!pointer.second)pointer=fallback;
          auto target = identities.find(pointer);
          Require(target != identities.end(), "Texture2D schema template absent from material slot");
          auto found = templates.find(edit->second);
          Require(found == templates.end() || (found->second.path == target->second.path &&
                                               found->second.asset == target->second.asset),
                  "one Texture declaration uses different native slot schemas; declare separate Texture IDs sharing the TEX file");
          templates[edit->second] = target->second;
          }
        }
      }
    }
  }
  for (const auto& section : authored) {
    Require(templates.count(section), "authored Texture has no material slot: " + section);
    package.textures.at(section).source = templates.at(section);
  }
}
inline TextureInputBuild BuildTextureInput(EndfieldInput::Bundle& bundle, const Package& package,
                                            const AssetIndex& index) {
  TextureInputBuild result;
  std::map<std::string, size_t> sourceStreams;
  for (const auto& member : bundle.members)
    Require(sourceStreams.emplace(member.name, member.bytes.size()).second,
            "duplicate current input member");
  std::map<Identity, std::vector<std::pair<std::string, const Material*>>> materials;
  std::map<Identity, std::vector<std::pair<std::string, const Texture*>>> textures;
  TextureExpansion expansion;
  for (const auto& material : package.materials)
    materials[material.second.source].push_back({material.first, &material.second});
  for (const auto& texture : package.textures) {
    textures[texture.second.source].push_back({texture.first, &texture.second});
    auto asset = index.find(texture.second.source);
    Require(asset != index.end() && asset->second.classId == 28,
            "Texture identity absent from current input graph");
    expansion[{asset->second.cab, asset->second.pathId}].push_back(
        AllocateTextureId(asset->second.cab, package, texture.first, texture.second));
  }
  for(const auto& material:package.materials){const auto& asset=index.at(material.second.source);expansion[{asset.cab,asset.pathId}].push_back(AllocateMaterialId(asset.cab,package,material.first));}
  for (auto& member : bundle.members) {
    if (!SerializedHeader(member.bytes)) continue;
    auto layout = ParseSerialized(member.bytes);
    auto assets = SerializedAssets(member.bytes, member.name, layout);
    std::map<int64_t, Bytes> changes;
    std::vector<AddedObject> added;
    std::set<TextureReference> requiredPreloads, referencedPreloads;
    std::set<int64_t> matched;
    for (const auto& asset : assets) {
      Identity identity{asset.path, asset.asset};
      auto material = materials.find(identity);
      auto texture = textures.find(identity);
      if (material == materials.end() && texture == textures.end()) continue;
      Require(asset.cab == member.name && matched.insert(asset.pathId).second,
              "ambiguous/nonlocal Material/Texture target in current input");
      const auto& object = layout.objects.at(asset.pathId);
      const auto& type = layout.types.at(object.type);
      auto value = ObjectValue(member.bytes, layout, asset.pathId);
      if (material != materials.end()) {
        for (const auto& authored : material->second) {
        ++result.materialMatches;
        const auto& selected = *authored.second;
        for (const auto& edit : selected.textures) {
          const auto& target = index.at(package.textures.at(edit.second).source);
          requiredPreloads.insert({target.cab, target.pathId});
        }
        auto bytes = EncodeValue(type, ApplyMaterial(type, value,
            *package.resources.at(selected.resource), selected, package, index,
            layout, member.name, result.textureBindings));
        auto id=AllocateMaterialId(member.name,package,authored.first);
        Require(!layout.objects.count(id)&&std::none_of(added.begin(),added.end(),[&](const AddedObject& row){return row.id==id;}),"additional Material PathID collision");
        added.push_back({id,object.type,std::move(bytes)});++result.changedMaterials;
        requiredPreloads.insert({member.name,asset.pathId});
        }
      } else {
        // Preserve the original object and stream bytes for unedited users.
        // Distinct declarations can use one native template independently.
        for (const auto& authored : texture->second) {
          const auto& selected = *authored.second;
          auto id = AllocateTextureId(member.name, package, authored.first, selected);
          Require(!layout.objects.count(id) && std::none_of(added.begin(), added.end(),
              [&](const AddedObject& row) { return row.id == id; }), "additional Texture PathID collision");
          TextureWrite record{identity, member.name, "", id}; record.sourceId = asset.pathId;
          auto clone = ApplyTexture(type, value, selected,
              *package.resources.at(selected.resource), member.name, bundle.members,
              sourceStreams, record);
          SetString(type, Child(type, clone, "m_Name"),
              selected.source.asset + "__EFF_" + package.id + "_" + authored.first);
          auto bytes = EncodeValue(type, clone);
          Require(EncodeValue(type, DecodeValue(type, 0, bytes)) == bytes,
                  "rebuilt Texture TypeTree roundtrip differs");
          added.push_back({id, object.type, std::move(bytes)});
          requiredPreloads.insert({member.name, asset.pathId});
          result.textures.push_back(std::move(record));
        }
      }
    }
    for (const auto& row : layout.objects) if (layout.types.at(row.second.type).classId == 142) {
      const auto& type = layout.types.at(row.second.type);
      auto value = ObjectValue(member.bytes, layout, row.first);
      if (ExpandTexturePreloads(type, value, layout, member.name, expansion, referencedPreloads)) {
        changes.emplace(row.first, EncodeValue(type, value)); result.changedPreloads = true;
      }
    }
    for (const auto& required : requiredPreloads)
      Require(referencedPreloads.count(required), "authored Texture absent from AssetBundle preload closure");
    member.bytes = BuildSerialized(member.bytes, layout, changes, added);
  }
  return result;
}
inline TextureInputBuild BuildTextureInput(const Bytes& source, const Package& package,
                                            const AssetIndex& index) {
  auto bundle = EndfieldInput::Decode(source);
  auto result = BuildTextureInput(bundle, package, index);
  result.bytes = result.textures.empty() && !result.changedMaterials && !result.changedPreloads
      ? source : EndfieldInput::Encode(source, bundle.members);
  auto checked = EndfieldInput::Decode(result.bytes);
  Require(checked.members.size() == bundle.members.size(), "rebuilt Texture input member count");
  for (size_t i = 0; i < bundle.members.size(); ++i)
    Require(checked.members[i].name == bundle.members[i].name &&
                checked.members[i].flags == bundle.members[i].flags &&
                checked.members[i].bytes == bundle.members[i].bytes,
            "rebuilt Texture input changed member identity/content");
  return result;
}
}  // namespace EiemNativeInput

#pragma once
#include <string_view>
#include <fstream>
#include <sstream>
#include "eiem_loading_graph_audit.h"

// Offline preparation only. This contract is separate from production v3 input
// substitutions, which intentionally require original member identities.
namespace EiemLoadingGeneration {
using namespace EiemLoadingGraphAudit;
struct Names {
  std::map<std::string, std::string> members, bundles;
};
inline std::string Token(const std::string& key, const std::string& identity, size_t size) {
  Require(!key.empty(), "candidate namespace key absent");
  auto label = key + "\n" + identity;
  std::string token;
  for (size_t block = 0; token.size() < size; ++block) {
    auto input = block ? label + "\n" + std::to_string(block) : label;
    auto digest = Sha((const unsigned char*)input.data(), input.size());
    token += Hex(digest.data(), digest.size()).substr(0, size - token.size());
  }
  for (auto& c : token) if (c >= 'A' && c <= 'F') c += 'a' - 'A';
  return token;
}
inline std::string BundleIdentity(const std::string& key, const std::string& original) {
  Require(!original.empty(), "candidate Bundle identity absent");
  auto slash = original.find_last_of('/');
  auto start = slash == original.npos ? size_t(0) : slash + 1;
  auto dot = original.find_last_of('.');
  auto end = dot != original.npos && dot > start ? dot : original.size();
  Require(end > start, "candidate Bundle identity has no file stem");
  return original.substr(0, start) + Token(key, "bundle:" + original, end - start) + original.substr(end);
}
inline bool Cab(const std::string& name) {
  if (name.size() != 36 || name.substr(0,4) != "CAB-") return false;
  for (size_t i = 4; i < name.size(); ++i)
    if (!((name[i] >= '0' && name[i] <= '9') || (name[i] >= 'a' && name[i] <= 'f'))) return false;
  return true;
}
inline std::string RemapPath(const std::string& path, const Names& names) {
  auto member = Basename(path); auto found = names.members.find(member);
  if (found == names.members.end()) return path;
  auto cab = member.substr(0,36), replacement = found->second.substr(0,36);
  Require(Cab(cab) && Cab(replacement), "candidate resource path is not CAB-based");
  auto expected = "archive:/" + cab + "/" + member;
  Require(path == expected, "candidate resource path needs a verified archive grammar");
  return "archive:/" + replacement + "/" + found->second;
}
struct Patch { size_t offset = 0; std::string before, after; };
inline Bytes ApplySites(const Bytes& bytes,std::vector<Patch> patches){
  std::sort(patches.begin(),patches.end(),[](const auto& a,const auto& b){return a.offset<b.offset;});size_t end=0;
  for(const auto& patch:patches){
    Require(patch.before.size()==patch.after.size()&&patch.offset>=end&&patch.offset<=bytes.size()&&patch.before.size()<=bytes.size()-patch.offset,"namespace recipe patch range/width/overlap differs");
    Require(std::equal(patch.before.begin(),patch.before.end(),bytes.begin()+patch.offset,[](char expected,unsigned char actual){return (unsigned char)expected==actual;}),"namespace recipe original field differs at byte "+std::to_string(patch.offset));end=patch.offset+patch.before.size();
  }
  Bytes result=bytes;for(const auto& patch:patches)std::copy(patch.after.begin(),patch.after.end(),result.begin()+patch.offset);return result;
}
inline Bytes ApplyProven(const Bytes& bytes,const std::string& hash,std::vector<Patch> patches){
  auto digest=Sha(bytes.data(),bytes.size());Require(Hex(digest.data(),digest.size())==hash,"namespace recipe source digest differs");
  return ApplySites(bytes,std::move(patches));
}
inline void AddPatch(std::vector<Patch>& patches, const Bytes& bytes, size_t offset,
                     const std::string& before, const std::string& after) {
  if (before == after) return;
  Require(before.size() == after.size() && offset <= bytes.size() && before.size() <= bytes.size() - offset,
          "candidate patch width/range differs");
  Require(std::equal(before.begin(), before.end(), bytes.begin() + offset), "candidate patch source differs");
  patches.push_back({offset, before, after});
}
inline Bytes Apply(const Bytes& bytes, std::vector<Patch> patches, const Names& names) {
  std::sort(patches.begin(), patches.end(), [](const auto& a, const auto& b) { return a.offset < b.offset; });
  size_t end = 0;
  for (const auto& patch : patches) {
    Require(patch.offset >= end, "candidate patches overlap");
    end = patch.offset + patch.before.size();
  }
  // Detect names hidden in unsupported managed data or unfamiliar string fields.
  // No arbitrary byte replacement is allowed to make such a file appear valid.
  std::string_view view((const char*)bytes.data(), bytes.size());
  auto requireCovered = [&](size_t pos, const std::string& old) {
    auto next = std::upper_bound(patches.begin(), patches.end(), pos,
        [](size_t offset, const Patch& patch) { return offset < patch.offset; });
    bool found = false;
    if (next != patches.begin()) {
      const auto& patch = *std::prev(next);
      found = pos >= patch.offset && pos - patch.offset <= patch.before.size() &&
          old.size() <= patch.before.size() - (pos - patch.offset);
    }
    Require(found, "candidate identity occurs outside verified schema/metadata patch: " + old + " at byte " + std::to_string(pos));
  };
  // Current CAB and Bundle names have fixed grammar. Scan each prefix once,
  // rather than rereading the entire resource for every name in the graph.
  auto scan = [&](const char* prefix, size_t width, const auto& index) {
    for (size_t pos = view.find(prefix); pos != view.npos; pos = view.find(prefix, pos + 4)) {
      if (width > view.size() - pos) continue;
      auto found = index.find(std::string(view.substr(pos, width)));
      if (found != index.end()) requireCovered(pos, found->first);
    }
  };
  std::map<std::string, bool> cabs, bundles;
  for (const auto& name : names.members) if (Cab(name.first)) cabs.emplace(name.first, true);
  auto currentBundle = [](const std::string& name) {
    return name.size() == 32 && name.substr(0,5) == "main/" && name.substr(29) == ".ab" &&
        name.substr(5,24).find_first_not_of("0123456789abcdef") == std::string::npos;
  };
  for (const auto& name : names.bundles) if (currentBundle(name.first)) bundles.emplace(name.first, true);
  scan("CAB-", 36, cabs); scan("main/", 32, bundles);
  // Preserve the generic contract for non-current Bundle fixtures/callers.
  for (const auto& name : names.bundles) if (!currentBundle(name.first))
    for (size_t pos = view.find(name.first); pos != view.npos; pos = view.find(name.first, pos + name.first.size()))
      requireCovered(pos, name.first);
  Bytes result = bytes;
  for (const auto& patch : patches) std::copy(patch.after.begin(), patch.after.end(), result.begin() + patch.offset);
  return result;
}
struct Result {
  Bytes bytes;
  size_t externalPaths = 0, streamPaths = 0, bundleNames = 0, opaqueObjectsPreserved = 0;
};
struct Recipe {std::vector<Patch> patches;size_t externalPaths=0,streamPaths=0,bundleNames=0,opaque=0;};
inline Result PrepareSerialized(const Bytes& bytes, const Names& names,Recipe* recipe=nullptr) {
  auto layout = ParseSerialized(bytes);
  auto externals = ReadExternals(bytes, layout); auto refs = ReadReferences(bytes, layout);
  std::vector<Patch> patches;
  Result result;
  for (const auto& ext : externals) {
    auto path = RemapPath(ext.path, names);
    if (path != ext.path) {
      Require(ext.type == 0 && ext.guid == std::string(32, '0') && ext.assetPath.empty(),
              "candidate external GUID/type needs a verified resolver contract");
      AddPatch(patches, bytes, ext.pathOffset, ext.path, path); ++result.externalPaths;
    }
  }
  for (const auto& stream : refs.streams) {
    auto path = RemapPath(stream.path, names);
    if (path != stream.path) {
      AddPatch(patches, bytes, stream.pathDataOffset, stream.path, path); ++result.streamPaths;
    }
  }
  for (const auto& row : layout.objects) {
    const auto& object = row.second; const auto& type = layout.types.at(object.type);
    if (type.classId != 142) continue;
    auto fields = Fields(type, 0, bytes, object.offset, object.offset + object.size, object.offset);
    size_t matchedNames = 0;
    for (const auto& field : fields) {
      if (type.nodes.at(field.second.node).type != "string") continue;
      auto old = FieldString(bytes, field.second); auto name = names.bundles.find(old);
      if (name == names.bundles.end()) continue;
      Require(field.first == "m_Name" || field.first == "m_AssetBundleName",
              "candidate AssetBundle identity field needs a verified contract");
      AddPatch(patches, bytes, field.second.offset + 4, old, name->second); ++matchedNames;
    }
    Require(matchedNames, "candidate Bundle name absent from namespace"); ++result.bundleNames;
  }
  result.bytes = Apply(bytes, patches, names);
  // Every site above is a parsed string payload. AddPatch proves equal width
  // and original bytes; Apply rejects overlaps and identities outside those
  // sites. Length prefixes, layout records, PPtrs, GUIDs and stream offsets
  // therefore cannot change. Check the actual byte boundaries directly;
  // reparsing the whole graph would only repeat the same schema traversal.
  Require(result.bytes.size() == bytes.size(), "candidate namespace changed native extent");
  auto ordered = patches;
  std::sort(ordered.begin(), ordered.end(), [](const auto& a,const auto& b){return a.offset < b.offset;});
  size_t preserved = 0;
  for (const auto& patch : ordered) {
    Require(std::equal(bytes.begin()+preserved,bytes.begin()+patch.offset,result.bytes.begin()+preserved) &&
                std::equal(patch.after.begin(),patch.after.end(),result.bytes.begin()+patch.offset),
            "candidate namespace changed bytes outside proven string fields");
    preserved = patch.offset + patch.after.size();
  }
  Require(std::equal(bytes.begin()+preserved,bytes.end(),result.bytes.begin()+preserved),
          "candidate namespace changed bytes outside proven string fields");
  for (const auto& opaque : refs.unparsed) {
    const auto& object = layout.objects.at(opaque.objectId);
    Require(std::equal(bytes.begin() + object.offset, bytes.begin() + object.offset + object.size,
                       result.bytes.begin() + object.offset), "candidate rewrote opaque managed payload");
    ++result.opaqueObjectsPreserved;
  }
  if(recipe)*recipe={std::move(patches),result.externalPaths,result.streamPaths,result.bundleNames,result.opaqueObjectsPreserved};
  return result;
}

// A recipe records only positions already proven by the full schema audit.
// The input SHA and the complete set of source identities bind that proof to
// exact bytes. A different generation changes replacement strings, not their
// widths or the proven PPtr/stream/layout invariants.
inline Result PrepareSerializedCached(const Bytes& bytes,const Names& names,
                                      const std::filesystem::path& cache,bool& reused){
  reused=false;if(cache.empty())return PrepareSerialized(bytes,names);
  auto digest=[](const Bytes& data){auto h=Sha(data.data(),data.size());return Hex(h.data(),h.size());};
  std::string identity="EFF_NAMESPACE_RECIPE_1\n"+digest(bytes);
  for(const auto& row:names.members)identity+='\n'+row.first;
  identity+="\nbundles";for(const auto& row:names.bundles)identity+='\n'+row.first;
  auto key=digest(Bytes(identity.begin(),identity.end()));auto file=cache/(key+".tsv");
  if(std::filesystem::exists(file)){
    auto data=Read(file);std::string text(data.begin(),data.end());auto end=text.rfind("sha256\t");
    Require(end!=text.npos&&end&&text[end-1]=='\n',"namespace recipe checksum absent");
    auto hash=Trim(text.substr(end+7));Require(hash==digest(Bytes(data.begin(),data.begin()+end)),"namespace recipe descriptor changed");
    std::istringstream rows(text.substr(0,end));std::string row;
    Require(bool(std::getline(rows,row))&&TableIdentity(Trim(row),"EFF_NAMESPACE_RECIPE",key),"namespace recipe identity differs");
    Require(bool(std::getline(rows,row)),"namespace recipe counts absent");std::istringstream counts(row);std::string tag;Result result;
    Require(bool(counts>>tag>>result.externalPaths>>result.streamPaths>>result.bundleNames>>result.opaqueObjectsPreserved)&&tag=="counts","namespace recipe counts invalid");
    std::vector<Patch> patches;
    while(std::getline(rows,row)){
      auto first=row.find('\t'),second=row.find('\t',first+1),third=row.find('\t',second+1);
      Require(first!=row.npos&&second!=row.npos&&third!=row.npos&&row.substr(0,first)=="patch","namespace recipe row invalid");
      auto number=row.substr(first+1,second-first-1);Require(!number.empty()&&number.find_first_not_of("0123456789")==number.npos,"namespace recipe offset invalid");
      auto kind=row.substr(second+1,third-second-1),before=row.substr(third+1);std::string after;
      if(kind=="path")after=RemapPath(before,names);else{Require(kind=="bundle"&&names.bundles.count(before),"namespace recipe unknown identity");after=names.bundles.at(before);}
      AddPatch(patches,bytes,size_t(std::stoull(number)),before,after);
    }
    result.bytes=Apply(bytes,std::move(patches),names);reused=true;return result;
  }
  Recipe recipe;auto result=PrepareSerialized(bytes,names,&recipe);
  std::ostringstream rows;rows<<"EFF_NAMESPACE_RECIPE\t1\t"<<key<<'\n'<<"counts\t"<<recipe.externalPaths<<'\t'<<recipe.streamPaths<<'\t'<<recipe.bundleNames<<'\t'<<recipe.opaque<<'\n';
  for(const auto& patch:recipe.patches){
    Require(patch.before.find_first_of("\t\r\n")==patch.before.npos,"namespace recipe identity contains delimiter");
    rows<<"patch\t"<<patch.offset<<'\t'<<(names.bundles.count(patch.before)?"bundle":"path")<<'\t'<<patch.before<<'\n';
  }
  auto text=rows.str();text+="sha256\t"+digest(Bytes(text.begin(),text.end()))+"\n";
  auto temporary=file;temporary+=L".tmp-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(GetTickCount64());
  {std::ofstream output(temporary,std::ios::binary);Require(bool(output),"namespace recipe create failed");output.write(text.data(),text.size());output.flush();Require(bool(output),"namespace recipe write failed");}
  Require(MoveFileExW(temporary.c_str(),file.c_str(),MOVEFILE_REPLACE_EXISTING)!=0,"namespace recipe publication failed");return result;
}
}  // namespace EiemLoadingGeneration

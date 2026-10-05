// Offline single-feature control. Keeps the validated floating vertex layout.
#include "../../src/eiem_renderer_input_builder.h"
#include <fstream>
#include <iostream>
using namespace EiemNativeInput;
void Save(const std::filesystem::path& path, const Bytes& bytes) {
  std::ofstream out(path,std::ios::binary); out.write((const char*)bytes.data(),bytes.size());
  Require(bool(out), "capture control output failed");
}
bool SerializedMember(const Bytes& b) { return b.size()>=48 && Le(b,8,4)==0x16000000; }
void ClearArray(const Type& t, Value& value) {
  auto& a=ArrayValue(t,value); a.children.clear(); a.raw.clear(); a.count=0;
}
int main(int argc,char** argv) {
  try {
    Require(argc==4,"usage: afterimage_shape_control <verified input dir> <Mod dir> <new output dir>");
    auto source=std::filesystem::u8path(argv[1]), destination=std::filesystem::u8path(argv[3]);
    auto package=EiemDirectory::Load(std::filesystem::u8path(argv[2]));
    Require(!std::filesystem::exists(destination),"capture control output exists");
    std::vector<std::filesystem::path> paths;
    for(auto& entry:std::filesystem::directory_iterator(source))
      if(entry.is_regular_file() && entry.path().extension()==".vfs") paths.push_back(entry.path());
    std::sort(paths.begin(),paths.end());
    std::set<NativeReference> shaped, verified;
    for(auto& path:paths) {
      auto bundle=EndfieldInput::Decode(Read(path),512ull*1024*1024);
      for(auto& member:bundle.members) if(SerializedMember(member.bytes)) {
        auto layout=ParseSerialized(member.bytes);
        // Scope this control to shaped authored parts selected by native asset
        // identity. Retain unrelated native shape channels in the same graph.
        for(const auto& asset:SerializedAssets(member.bytes,member.name,layout)) {
          Identity identity{asset.path,asset.asset};
          auto selected=package.targetIndex.find(identity);
          if(selected==package.targetIndex.end()) continue;
          const auto& rule=package.rules.at(selected->second);
          for(size_t i=0;i<rule.parts.size();++i) {
            const auto& partId=rule.parts[i];
            const auto& mesh=package.meshes.at(package.parts.at(partId).mesh);
            if(mesh.shapes.empty()) continue;
            auto id=i?AllocateMeshId(member.name,package,identity,partId):asset.pathId;
            shaped.insert({member.name,id});
          }
        }
        for(auto& row:layout.objects) {
          const auto& type=layout.types.at(row.second.type);
          if(type.classId!=43 || !shaped.count({member.name,row.first})) continue;
          auto mesh=ObjectValue(member.bytes,layout,row.first);
          Require(ConstArray(type,Child(type,Child(type,mesh,"m_Shapes"),"channels")).count,
                  "selected authored shape channels absent");
          verified.insert({member.name,row.first});
        }
      }
    }
    Require(!shaped.empty(),"no shaped Mesh in control graph");
    Require(shaped==verified,"selected authored Mesh missing from control graph");
    std::filesystem::create_directories(destination);
    size_t meshes=0,renderers=0,changedInputs=0;
    for(auto& path:paths) {
      auto sourceBytes=Read(path); auto bundle=EndfieldInput::Decode(sourceBytes,512ull*1024*1024);
      bool changed=false;
      for(auto& member:bundle.members) if(SerializedMember(member.bytes)) {
        auto layout=ParseSerialized(member.bytes); std::map<int64_t,Bytes> changes;
        for(auto& row:layout.objects) {
          const auto& type=layout.types.at(row.second.type);
          if(type.classId==43 && shaped.count({member.name,row.first})) {
            auto mesh=ObjectValue(member.bytes,layout,row.first);
            auto vertices=EncodeValue(type,Child(type,mesh,"m_VertexData"));
            auto& shape=Child(type,mesh,"m_Shapes");
            for(auto name:{"vertices","shapes","channels","fullWeights"}) ClearArray(type,Child(type,shape,name));
            Require(EncodeValue(type,Child(type,mesh,"m_VertexData"))==vertices,"control changed vertex layout");
            changes[row.first]=EncodeValue(type,mesh); ++meshes;
          } else if(type.classId==137) {
            auto renderer=ObjectValue(member.bytes,layout,row.first);
            if(!shaped.count(NullableReference(type,Child(type,renderer,"m_Mesh"),layout,member.name))) continue;
            ClearArray(type,Child(type,renderer,"m_BlendShapeWeights"));
            changes[row.first]=EncodeValue(type,renderer); ++renderers;
          }
        }
        if(!changes.empty()) { member.bytes=BuildSerialized(member.bytes,layout,changes,{}); changed=true; }
      }
      auto output=changed?EndfieldInput::Encode(sourceBytes,bundle.members,512ull*1024*1024):sourceBytes;
      Save(destination/path.filename(),output); changedInputs+=changed;
    }
    std::cout << "SHAPE-CONTROL inputs=" << paths.size() << " changed=" << changedInputs
              << " meshes=" << meshes << " renderers=" << renderers << " vertexLayoutPreserved=1\n";
  } catch(const std::exception& error) { std::cerr<<error.what()<<'\n'; return 1; }
}

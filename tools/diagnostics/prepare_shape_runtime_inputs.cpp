// Prepare readable, provenance-marked author Meshes for the capture candidate.
// Source Meshes keep every native blend-shape field and Renderer binding.
#include "../../src/eiem_renderer_input_builder.h"
#include "../../src/eiem_loading_input_substitution.h"
#include <fstream>
#include <iostream>
using namespace EiemNativeInput;
void Save(const std::filesystem::path& path,const Bytes& bytes) {
  std::ofstream out(path,std::ios::binary); out.write((const char*)bytes.data(),bytes.size());
  Require(bool(out),"shape runtime input write failed");
}
bool SerializedMember(const Bytes& b) { return b.size()>=48 && Le(b,8,4)==0x16000000; }
int main(int argc,char** argv) {
  try {
    Require(argc==4 || (argc==5 && std::string(argv[4])=="--verify"),"usage: prepare_shape_runtime_inputs <formal input dir> <Mod dir> <output dir> [--verify]");
    bool verify=argc==5;
    auto source=std::filesystem::u8path(argv[1]),destination=std::filesystem::u8path(argv[3]);
    auto package=Load(std::filesystem::u8path(argv[2]));
    std::cout<<"InputContractDigest="<<InputContractDigest(package)<<"\n";
    Require(verify?std::filesystem::exists(destination):!std::filesystem::exists(destination),"controlled runtime output directory state mismatch");
    struct Marker {std::string name;bool shaped;};
    std::map<NativeReference,Marker> markers;
    std::vector<std::filesystem::path> paths;
    for (const auto& path:std::filesystem::directory_iterator(source)) if(path.is_regular_file() && path.path().extension()==".vfs") paths.push_back(path.path());
    std::sort(paths.begin(),paths.end());
    for(const auto& path:paths) {
      auto bundle=EndfieldInput::Decode(Read(path),512ull*1024*1024);
      for(const auto& member:bundle.members) if(SerializedMember(member.bytes)) {
        auto layout=ParseSerialized(member.bytes);
        for(const auto& asset:SerializedAssets(member.bytes,member.name,layout)) {
          Identity target{asset.path,asset.asset}; auto selected=package.targetIndex.find(target);
          if(selected==package.targetIndex.end()) {
            // Accepted inputs may already carry author provenance. Resolve the
            // first-part name back to the exact source path+asset identity.
            for(auto candidate=package.targetIndex.begin();candidate!=package.targetIndex.end();++candidate) {
              const auto& rule=package.rules.at(candidate->second);
              if(candidate->first.path==asset.path && !rule.skip && !rule.parts.empty() &&
                 RuntimePartName(package,rule.parts.front(),candidate->first)==asset.asset) {
                Require(selected==package.targetIndex.end(),"ambiguous prepared source provenance");selected=candidate;
              }
            }
          }
          if(selected==package.targetIndex.end()) continue;
          target=selected->first;
          const auto& rule=package.rules.at(selected->second);
          for(size_t i=0;i<rule.parts.size();++i) {
            const auto& partId=rule.parts[i]; const auto& part=package.parts.at(partId);
            int64_t id=i?AllocateMeshId(member.name,package,target,partId):asset.pathId;
            Require(markers.emplace(NativeReference{member.name,id},Marker{RuntimePartName(package,partId,target),!package.meshes.at(part.mesh).shapes.empty()}).second,
                    "duplicate runtime shape Mesh identity");
          }
        }
      }
    }
    Require(!markers.empty(),"runtime controlled Meshes absent");
    std::filesystem::create_directories(destination);
    size_t meshCount=0,changedInputs=0;
    for(const auto& path:paths) {
      auto sourceBytes=Read(path);auto bundle=EndfieldInput::Decode(sourceBytes,512ull*1024*1024);bool changed=false;
      for(auto& member:bundle.members) if(SerializedMember(member.bytes)) {
        auto layout=ParseSerialized(member.bytes);std::map<int64_t,Bytes> changes;
        for(const auto& row:layout.objects) {
          auto marker=markers.find({member.name,row.first});if(marker==markers.end()) continue;
          const auto& type=layout.types.at(row.second.type);Require(type.classId==43,"runtime shaped object is not a Mesh");
          auto mesh=ObjectValue(member.bytes,layout,row.first);
          std::map<std::string,Bytes> fields;
          for (const auto& field:mesh.children) {
            const auto& name=type.nodes[field.node].name;
            if (name!="m_Name" && (name!="m_IsReadable" || !marker->second.shaped)) fields.emplace(name,EncodeValue(type,field));
          }
          auto shapes=EncodeValue(type,Child(type,mesh,"m_Shapes"));
          if (marker->second.shaped) Require(ArrayValue(type,Child(type,Child(type,mesh,"m_Shapes"),"channels")).count>0,"runtime source shape channels missing");
          auto vertices=EncodeValue(type,Child(type,mesh,"m_VertexData"));
          if (marker->second.shaped) Require(DescribeMeshVertexStorage(type,Child(type,mesh,"m_VertexData")).stride[0]==40,"runtime source requires validated floating frame storage");
          SetString(type,Child(type,mesh,"m_Name"),marker->second.name);
          if (marker->second.shaped) SetInteger(type,Child(type,mesh,"m_IsReadable"),1);
          Require(EncodeValue(type,Child(type,mesh,"m_Shapes"))==shapes && EncodeValue(type,Child(type,mesh,"m_VertexData"))==vertices,
                  "runtime preparation changed shape/frame bytes");
          for(const auto& field:fields) Require(EncodeValue(type,Child(type,mesh,field.first))==field.second,"controlled provenance changed another Mesh field");
          changes[row.first]=EncodeValue(type,mesh);++meshCount;
        }
        if(!changes.empty()) { member.bytes=BuildSerialized(member.bytes,layout,changes,{});changed=true; }
      }
      auto output=changed?EndfieldInput::Encode(sourceBytes,bundle.members,512ull*1024*1024):sourceBytes;
      if (verify) Require(Read(destination/path.filename())==output,"prepared control input differs from verified rebuild");
      else Save(destination/path.filename(),output);changedInputs+=changed;
    }
    Require(meshCount==markers.size(),"runtime shape graph coverage");
    std::cout<<"SHAPE-RUNTIME-INPUT inputs="<<paths.size()<<" changedInputs="<<changedInputs<<" meshes="<<meshCount
             <<" sourceShapeChannelsPreserved=1 vertexLayoutPreserved=1 rendererFieldsPreserved=1 allOtherMeshFieldsPreserved=1 verifiedExisting="<<verify<<"\n";
  } catch(const std::exception& e) { std::cerr<<e.what()<<'\n';return 1; }
}

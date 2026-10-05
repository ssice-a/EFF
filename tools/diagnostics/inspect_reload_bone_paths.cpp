// Read-only audit of a real prepared generation, independent of Unity getters.
#include "eiem_resource_reload_plan.h"
#include <iostream>
using namespace EiemResourceReloadPlan;
namespace {
std::string Quote(const std::string& value) {
 std::string out="\"";
 for(unsigned char c:value){if(c=='"'||c=='\\'){out+='\\';out+=char(c);}else if(c<32){static const char hex[]="0123456789ABCDEF";out+="\\u00";out+=hex[c>>4];out+=hex[c&15];}else out+=char(c);}
 return out+'"';
}
struct AuditNode {int kind=0;std::string name;NativeReference owner,parent,mesh,root;std::vector<NativeReference> bones;};
}
int main(int argc,char** argv) {
 try {
  Require(argc>=6,"inspect_reload_bone_paths <author> <generation> <new-report> <input files...>");
  auto package=Load(std::filesystem::u8path(argv[1]));auto directory=std::filesystem::u8path(argv[2]),output=std::filesystem::u8path(argv[3]);
  Require(!std::filesystem::exists(output),"bone path report exists");
  auto readyBytes=Read(directory/"ready.tsv",256*1024);std::istringstream ready(Text(readyBytes.data(),readyBytes.size()));std::string row;
  Require(bool(std::getline(ready,row)),"ready header missing");auto header=TabFields(row);Require(header.size()==5&&header[0]=="EFF_RESOURCE_RELOAD_GENERATION"&&header[1]=="1"&&header[3]==PackageDigest(package),"ready author/header differs");
  std::map<std::string,std::string> hashes;while(std::getline(ready,row)){auto fields=TabFields(row);Require(fields.size()==3&&fields[0]=="input"&&hashes.emplace(fields[1],fields[2]).second,"ready row differs");}
  std::map<std::string,std::pair<std::string,std::vector<std::string>>> authors;
  for(const auto& rule:package.rules)if(!rule.second.skip)for(const auto& target:rule.second.targets)for(const auto& id:rule.second.parts){
   const auto& mesh=package.meshes.at(package.parts.at(id).mesh);Require(authors.emplace(RuntimePartName(package,id,target.second),std::make_pair(id,mesh.bones)).second,"duplicate author marker");}
  std::map<NativeReference,AuditNode> nodes;std::map<NativeReference,NativeReference> transforms;std::ostringstream inputs;
  for(int i=4;i<argc;++i){auto leaf=std::filesystem::u8path(argv[i]);Require(leaf==leaf.filename()&&leaf.extension()==".vfs","non-flat input name");auto bytes=Read(directory/leaf,inputLimit);auto hash=Hash(bytes);Require(hashes.at(leaf.u8string())==hash,"candidate SHA differs");
   if(i!=4)inputs<<',';inputs<<"{\"file\":"<<Quote(leaf.u8string())<<",\"sha256\":"<<Quote(hash)<<'}';
   auto bundle=EndfieldInput::Decode(bytes,inputLimit);for(const auto& member:bundle.members){if(!Serialized(member.bytes))continue;auto layout=ParseSerialized(member.bytes);
    for(const auto& object:layout.objects){const auto& type=layout.types.at(object.second.type);auto kind=type.classId;if(kind!=1&&kind!=4&&kind!=43&&kind!=137)continue;
     auto tree=ObjectValue(member.bytes,layout,object.first);AuditNode node;node.kind=kind;auto ref=[&](const char* field){return Reference(type,Child(type,tree,field),layout,member.name);};
     if(kind==1||kind==43)node.name=Name(type,tree);
     if(kind==4){node.owner=ref("m_GameObject");node.parent=ref("m_Father");Require(transforms.emplace(node.owner,NativeReference{member.name,object.first}).second,"multiple GameObject Transforms");}
     if(kind==137){node.owner=ref("m_GameObject");node.mesh=ref("m_Mesh");node.root=ref("m_RootBone");for(const auto& bone:ConstArray(type,Child(type,tree,"m_Bones")).children)node.bones.push_back(Reference(type,bone,layout,member.name));}
     Require(nodes.emplace(NativeReference{member.name,object.first},std::move(node)).second,"duplicate serialized object");
    }
   }
  }
  auto chain=[&](NativeReference node){std::vector<NativeReference> result;std::set<NativeReference> seen;while(node.second){Require(result.size()<512&&seen.insert(node).second,"hierarchy cycle/depth");const auto& current=nodes.at(node);Require(current.kind==4,"hierarchy not Transform");result.push_back(node);node=current.parent;}Require(!result.empty(),"empty hierarchy");return result;};
  auto relative=[&](const std::vector<NativeReference>& path,const NativeReference& anchor){auto found=std::find(path.begin(),path.end(),anchor);Require(found!=path.end(),"bone outside anchor");std::string result;while(found!=path.begin()){--found;const auto& owner=nodes.at(nodes.at(*found).owner);Require(owner.kind==1&&!owner.name.empty()&&owner.name.find('/')==std::string::npos,"ambiguous path name");if(!result.empty())result+='/';result+=owner.name;}return result;};
  size_t renderers=0,slots=0,legacyRejects=0;std::set<std::string> markers;std::ostringstream entries;
  for(const auto& object:nodes){const auto& renderer=object.second;if(renderer.kind!=137)continue;auto mesh=nodes.find(renderer.mesh);if(mesh==nodes.end())continue;auto authored=authors.find(mesh->second.name);if(authored==authors.end())continue;
   const auto& expected=authored->second.second;Require(renderer.bones.size()==expected.size()&&!expected.empty()&&std::set<NativeReference>(renderer.bones.begin(),renderer.bones.end()).size()==expected.size(),"palette count/duplicate differs");Require(std::count(renderer.bones.begin(),renderer.bones.end(),renderer.root)==1,"rootBone not unique palette member");
   auto ownerChain=chain(transforms.at(renderer.owner)),rootChain=chain(renderer.root);std::vector<std::vector<NativeReference>> boneChains;for(const auto& bone:renderer.bones){auto path=chain(bone);Require(path.back()==ownerChain.back(),"foreign skeleton");boneChains.push_back(std::move(path));}
   size_t matches=0;NativeReference selected;for(const auto& anchor:rootChain){if(std::find(ownerChain.begin(),ownerChain.end(),anchor)==ownerChain.end())continue;bool correct=true;for(size_t slot=0;slot<expected.size();++slot){if(std::find(boneChains[slot].begin(),boneChains[slot].end(),anchor)==boneChains[slot].end()||relative(boneChains[slot],anchor)!=expected[slot]){correct=false;break;}}if(correct){++matches;selected=anchor;}}
   Require(matches==1,"palette full paths lack unique common anchor");
   for(size_t slot=0;slot<expected.size();++slot)if(nodes.at(nodes.at(renderer.bones[slot]).owner).name!=expected[slot])++legacyRejects;
   if(renderers++)entries<<',';slots+=expected.size();markers.insert(mesh->second.name);entries<<"{\"part\":"<<Quote(authored->second.first)<<",\"marker\":"<<Quote(mesh->second.name)<<",\"member\":"<<Quote(object.first.first)<<",\"rendererPathId\":"<<Quote(std::to_string(object.first.second))<<",\"slots\":"<<expected.size()<<",\"anchorPathId\":"<<Quote(std::to_string(selected.second))<<",\"firstExpected\":"<<Quote(expected.front())<<",\"firstActual\":"<<Quote(relative(boneChains.front(),selected))<<'}';
  }
  Require(renderers&&slots&&legacyRejects,"no authored palettes or legacy rejection reproduced");
  std::ofstream report(output,std::ios::binary);Require(bool(report),"cannot create bone report");report<<"{\"format\":\"EFF_PREPARED_GENERATION_BONE_PATH_AUDIT\",\"version\":1,\"generationKey\":"<<Quote(header[2])<<",\"verified\":true,\"resourceMutation\":false,\"nativeAcceptanceVerified\":false,\"renderers\":"<<renderers<<",\"distinctMarkers\":"<<markers.size()<<",\"orderedSlots\":"<<slots<<",\"legacyLeafComparisonsRejected\":"<<legacyRejects<<",\"inputs\":["<<inputs.str()<<"],\"palettes\":["<<entries.str()<<"]}\n";report.flush();Require(bool(report),"bone report write failed");
  std::cout<<"PASS actual prepared generation: renderers="<<renderers<<" distinctMarkers="<<markers.size()<<" orderedSlots="<<slots<<" legacyLeafComparisonsRejected="<<legacyRejects<<" fullPathsAndUniqueLocalAnchorVerified=1 nativeAcceptanceVerified=0\n";
 }catch(const std::exception& error){std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}
}

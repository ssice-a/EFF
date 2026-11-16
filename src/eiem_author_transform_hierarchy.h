#pragma once
namespace EiemAuthorTransforms {
using namespace EiemDirectory;
inline void Path(const std::string& path){Require(!path.empty()&&path.front()!='/'&&path.back()!='/'&&path.find_first_of("\\:\0",0,3)==path.npos,"invalid author Transform path");size_t start=0;while(start<path.size()){auto end=path.find('/',start);auto part=path.substr(start,end==path.npos?path.size()-start:end-start);Require(!part.empty()&&part!="."&&part!="..","invalid author Transform path segment");if(end==path.npos)break;start=end+1;}}
inline void Validate(const AuthorTransformHierarchy& graph){
 Require(Identifier(graph.id)&&!graph.nodes.empty()&&graph.nodes.size()<=1024,"invalid author Transform hierarchy");std::set<std::string> paths;
 for(const auto& node:graph.nodes){Path(node.path);auto at=node.path.rfind('/');Require((at==node.path.npos||paths.count(node.path.substr(0,at)))&&paths.insert(node.path).second,"author Transform parent absent or duplicate path");
  double norm=0;for(auto v:node.position)Require(std::isfinite(v),"author Transform position nonfinite");for(auto v:node.scale)Require(std::isfinite(v),"author Transform scale nonfinite");for(auto v:node.rotation){Require(std::isfinite(v),"author Transform rotation nonfinite");norm+=double(v)*v;}Require(std::abs(norm-1)<.001,"author Transform rotation not normalized");}
}
inline std::string Identity(const AuthorTransformHierarchy& graph){std::string bytes=graph.id;for(const auto& n:graph.nodes){bytes+='\0';bytes+=n.path;bytes.append(reinterpret_cast<const char*>(n.position.data()),sizeof(n.position));bytes.append(reinterpret_cast<const char*>(n.rotation.data()),sizeof(n.rotation));bytes.append(reinterpret_cast<const char*>(n.scale.data()),sizeof(n.scale));}auto h=Sha(reinterpret_cast<const unsigned char*>(bytes.data()),bytes.size());return Hex(h.data(),h.size());}
}

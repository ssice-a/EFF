#pragma once
#include "eiem_material_operations.h"

// A source-independent author directory. The game reader does not import a
// TypeTree, consumer list, source file range, old CAB ID or compiler here.
namespace EiemRuntimeAuthorDirectory {
using namespace EiemDirectory;
inline constexpr const char* structure="EFF_AUTHOR_ASSETS/native-mesh+texture+material-operations+replace-rules/utf8+le64+sha256";
struct MeshAsset {
 std::string part,file,path,marker,hash,material;
 unsigned vertices=0,indices=0;
 std::vector<std::string> bones,shapes;
};
struct TextureAsset {std::string id,file,path,hash;unsigned width=0,height=0,mips=0;};
struct MaterialEntry {std::string id;Identity source;EiemMaterialOperations::Edits edits;std::map<std::string,std::string> textures;};
struct ReplaceRule {std::string id;std::map<unsigned,Identity> targets;std::vector<std::string> parts;bool skip=false;};
struct Directory {std::string mod;std::vector<MeshAsset> meshes;std::vector<MaterialEntry> materials;std::vector<TextureAsset> textures;std::vector<ReplaceRule> rules;};
struct Writer {
 Bytes b;
 void N(uint64_t n){for(unsigned i=0;i<8;++i)b.push_back((unsigned char)(n>>(8*i)));}
 void S(const std::string& s){Text((const unsigned char*)s.data(),s.size());N(s.size());b.insert(b.end(),s.begin(),s.end());}
 void D(double n){Require(std::isfinite(n),"author directory has nonfinite number");uint64_t bits;memcpy(&bits,&n,8);N(bits);}
 void Strings(const std::vector<std::string>& v){N(v.size());for(const auto& s:v)S(s);}
};
struct Reader {
 const Bytes& b;size_t p=0;
 uint64_t N(){auto n=Le(b,p,8);p+=8;return n;}
 size_t Count(){auto n=N();Require(n<=(b.size()-p)/8,"author directory count exceeds remaining bytes");return size_t(n);}
 unsigned U(){auto n=N();Require(n<=UINT_MAX,"author directory unsigned field overflow");return unsigned(n);}
 bool B(){auto n=N();Require(n<=1,"author directory bool field differs");return n!=0;}
 std::string S(){auto n=N();Require(n<=b.size()-p,"author directory string truncated");auto s=Text(b.data()+p,size_t(n));p+=size_t(n);return s;}
 double D(){auto bits=N();double n;memcpy(&n,&bits,8);Require(std::isfinite(n),"author directory nonfinite number");return n;}
 std::vector<std::string> Strings(){std::vector<std::string> v;auto count=Count();v.reserve(count);for(size_t i=0;i<count;++i)v.push_back(S());return v;}
};
inline void Relative(const std::string& s){
 auto p=std::filesystem::u8path(s);Require(!s.empty()&&!p.is_absolute()&&s.find_first_of("\\:\r\n\t")==std::string::npos,"author asset path must be relative");
 for(const auto& part:p)Require(part!=".."&&part!="."&&!part.empty(),"author asset path escapes its Mod");
}
inline void Validate(const Directory& d){
 Require(!d.mod.empty(),"author asset directory Mod identity absent");std::set<std::string> parts,files,materials;
 for(const auto& m:d.meshes){Relative(m.file);Require(!m.part.empty()&&!m.marker.empty()&&m.path.rfind("assets/",0)==0&&m.vertices&&m.indices&&m.indices%3==0&&m.hash.size()==64&&m.hash.find_first_not_of("0123456789ABCDEF")==std::string::npos,"author Mesh directory fields differ");
  Require(parts.insert(m.part).second&&files.insert(m.file).second,"author Mesh Part/file duplicated");}
 std::set<std::string> textures;std::map<std::string,std::string> textureFiles;
 for(const auto& t:d.textures){Relative(t.file);auto prior=textureFiles.emplace(t.file,t.hash);Require(!files.count(t.file)&&(prior.second||prior.first->second==t.hash)&&!t.id.empty()&&textures.insert(t.id).second&&t.width&&t.height&&t.mips&&t.path.rfind("assets/",0)==0&&t.hash.size()==64&&t.hash.find_first_not_of("0123456789ABCDEF")==std::string::npos,"author Texture identity/layout duplicated or absent");}
 for(const auto& m:d.materials){Require(!m.id.empty()&&materials.insert(m.id).second&&m.source.path.rfind("assets/",0)==0&&!m.source.asset.empty(),"author Material identity duplicated/absent");for(const auto& texture:m.textures)Require(!texture.first.empty()&&textures.count(texture.second),"author Material references absent texture");}
 for(const auto& mesh:d.meshes)Require(materials.count(mesh.material),"author Mesh Part references absent Material");
 std::set<std::string> ruleNames,covered;std::set<Identity> targets;
 for(const auto& rule:d.rules){Require(!rule.id.empty()&&ruleNames.insert(rule.id).second&&!rule.targets.empty()&&(rule.skip?rule.parts.empty():!rule.parts.empty()),"author Replace rule identity/handling differs");
  for(const auto& target:rule.targets)Require(target.second.path.rfind("assets/",0)==0&&!target.second.asset.empty()&&targets.insert(target.second).second,"author Mesh target duplicated or absent");
  std::set<std::string> listed;for(const auto& part:rule.parts){Require(parts.count(part)&&listed.insert(part).second&&covered.insert(part).second,"author Part absent/duplicated in Replace rules");}
 }
 Require(covered==parts,"author asset directory contains an unowned Part");
}
inline void Operations(Writer& w,const EiemMaterialOperations::Edits& e){
 w.N(e.floats.size());for(const auto& x:e.floats){w.S(x.first);w.D(x.second);}
 w.N(e.integers.size());for(const auto& x:e.integers){w.S(x.first);w.N(uint64_t(x.second));}
 auto vectors=[&](const auto& map){w.N(map.size());for(const auto& x:map){w.S(x.first);for(auto v:x.second)w.D(v);}};
 vectors(e.colors);vectors(e.scales);vectors(e.offsets);
 w.N(e.keywords.size());for(const auto& x:e.keywords){w.S(x.first);w.N(x.second);}
 w.N(e.tags.size());for(const auto& x:e.tags){w.S(x.first);w.S(x.second);}
 w.N(bool(e.queue));if(e.queue)w.N(uint64_t(int64_t(*e.queue)));
}
inline EiemMaterialOperations::Edits Operations(Reader& r){
 EiemMaterialOperations::Edits e;
 auto scalar=[&](auto& map,const auto& read){auto n=r.Count();for(size_t i=0;i<n;++i){auto key=r.S();auto value=read();Require(map.emplace(std::move(key),std::move(value)).second,"duplicate author Material operation");}};
 scalar(e.floats,[&]{return r.D();});scalar(e.integers,[&]{return int64_t(r.N());});
 scalar(e.colors,[&]{std::array<double,4> v;for(auto& x:v)x=r.D();return v;});
 scalar(e.scales,[&]{std::array<double,2> v;for(auto& x:v)x=r.D();return v;});
 scalar(e.offsets,[&]{std::array<double,2> v;for(auto& x:v)x=r.D();return v;});
 scalar(e.keywords,[&]{return r.B();});scalar(e.tags,[&]{return r.S();});
 if(r.B()){auto value=int64_t(r.N());Require(value>=-1&&value<=INT32_MAX,"author Material queue cannot fit its native field");e.queue=int32_t(value);}return e;
}
inline Bytes Encode(const Directory& d){
 Validate(d);Writer w;w.S(structure);w.S(d.mod);w.N(d.meshes.size());
 for(const auto& m:d.meshes){w.S(m.part);w.S(m.file);w.S(m.path);w.S(m.marker);w.S(m.hash);w.S(m.material);w.N(m.vertices);w.N(m.indices);w.Strings(m.bones);w.Strings(m.shapes);}
 w.N(d.textures.size());for(const auto& t:d.textures){w.S(t.id);w.S(t.file);w.S(t.path);w.S(t.hash);w.N(t.width);w.N(t.height);w.N(t.mips);}
 w.N(d.materials.size());for(const auto& m:d.materials){w.S(m.id);w.S(m.source.path);w.S(m.source.asset);Operations(w,m.edits);w.N(m.textures.size());for(const auto& t:m.textures){w.S(t.first);w.S(t.second);}}
 w.N(d.rules.size());for(const auto& rule:d.rules){w.S(rule.id);w.N(rule.skip);w.N(rule.targets.size());for(const auto& target:rule.targets){w.N(target.first);w.S(target.second.path);w.S(target.second.asset);}w.Strings(rule.parts);}
 auto hash=Sha(w.b.data(),w.b.size());w.b.insert(w.b.end(),hash.begin(),hash.end());return w.b;
}
inline Directory Decode(const Bytes& bytes){
 Require(bytes.size()>=32,"author asset directory truncated");auto hash=Sha(bytes.data(),bytes.size()-32);
 Require(std::equal(hash.begin(),hash.end(),bytes.end()-32),"author asset directory checksum differs");Bytes payload(bytes.begin(),bytes.end()-32);Reader r{payload};
 Require(r.S()==structure,"author asset directory structure differs");Directory d;d.mod=r.S();auto n=r.Count();d.meshes.reserve(n);
 for(size_t i=0;i<n;++i){MeshAsset m;m.part=r.S();m.file=r.S();m.path=r.S();m.marker=r.S();m.hash=r.S();m.material=r.S();m.vertices=r.U();m.indices=r.U();m.bones=r.Strings();m.shapes=r.Strings();d.meshes.push_back(std::move(m));}
 n=r.Count();d.textures.reserve(n);for(size_t i=0;i<n;++i){TextureAsset t;t.id=r.S();t.file=r.S();t.path=r.S();t.hash=r.S();t.width=r.U();t.height=r.U();t.mips=r.U();d.textures.push_back(std::move(t));}
 n=r.Count();d.materials.reserve(n);for(size_t i=0;i<n;++i){MaterialEntry m;m.id=r.S();m.source.path=r.S();m.source.asset=r.S();m.edits=Operations(r);auto count=r.Count();for(size_t j=0;j<count;++j){auto key=r.S(),value=r.S();Require(m.textures.emplace(std::move(key),std::move(value)).second,"duplicate Material texture binding");}d.materials.push_back(std::move(m));}
 n=r.Count();d.rules.reserve(n);for(size_t i=0;i<n;++i){ReplaceRule rule;rule.id=r.S();rule.skip=r.B();auto count=r.Count();for(size_t j=0;j<count;++j){auto level=r.U();Identity identity{r.S(),r.S()};Require(rule.targets.emplace(level,std::move(identity)).second,"duplicate author LOD target");}rule.parts=r.Strings();d.rules.push_back(std::move(rule));}
 Require(r.p==payload.size(),"author asset directory trailing data");Validate(d);return d;
}
} // namespace EiemRuntimeAuthorDirectory

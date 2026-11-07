#pragma once
#include "eiem_mesh_binding_metadata.h"
#include "eiem_author_control_metadata.h"

// A source-independent author directory. The game reader does not import a
// TypeTree, consumer list, source file range, old CAB ID or compiler here.
namespace EiemRuntimeAuthorDirectory {
using namespace EiemDirectory;
inline constexpr const char* structure="EFF_AUTHOR_ASSETS/native-mesh+editable-mesh-author-data+optional-material+source-material-slot+skeleton+material-edits+logical-material-source+standard-dds+replace-rules+binding-bounds+field-selection+auxiliary+controls+mixed-sampled-animation+lazy-effects/utf8+le64+sha256/7";
struct MeshAsset {
 std::string part,file,path,marker,hash,material;
 std::string skeleton;
 unsigned materialSlot=0;
 unsigned vertices=0,indices=0;
 size_t nativeBytes=0,authorBytes=0;
 std::string authorHash;
 std::vector<std::string> bones,shapes;
 EiemMeshBindingMetadata::Mesh binding;
 std::vector<std::string> fields;
};
struct TextureAsset {std::string id,file,ddsFile;};
struct SkeletonAsset {std::string id,file;};
struct NativeDependency {std::string cab;int64_t pathId=0;Identity logical;unsigned classId=0;
 bool operator<(const NativeDependency& other)const{return std::tie(cab,pathId)<std::tie(other.cab,other.pathId);}
 bool operator==(const NativeDependency& other)const{return cab==other.cab&&pathId==other.pathId&&logical.path==other.logical.path&&logical.asset==other.logical.asset&&classId==other.classId;}};
struct MaterialEntry {std::string id,file,hash;Identity source;size_t authorBytes=0;std::map<std::string,std::string> textures;};
struct ReplaceRule {std::string id;std::map<unsigned,Identity> targets;std::vector<std::string> parts;bool skip=false;};
struct Auxiliary {std::string id,file,hash,authorHash;unsigned kind=0;size_t bytes=0,authorBytes=0;std::set<Identity> targets;};
struct Directory {std::string mod;std::vector<MeshAsset> meshes;std::vector<MaterialEntry> materials;std::vector<TextureAsset> textures;std::vector<SkeletonAsset> skeletons;std::vector<ReplaceRule> rules;std::vector<Auxiliary> auxiliary;EiemAuthorControlMetadata::State runtime;};
struct Writer {
 Bytes b;
 void N(uint64_t n){for(unsigned i=0;i<8;++i)b.push_back((unsigned char)(n>>(8*i)));}
 void S(const std::string& s){Text((const unsigned char*)s.data(),s.size());N(s.size());b.insert(b.end(),s.begin(),s.end());}
 void D(double n){Require(std::isfinite(n),"author directory has nonfinite number");uint64_t bits;memcpy(&bits,&n,8);N(bits);}
 void Strings(const std::vector<std::string>& v){N(v.size());for(const auto& s:v)S(s);}
 void Expr(const std::shared_ptr<EiemExpression>& value){
  std::vector<const EiemExpression*> pending{value.get()};
  while(!pending.empty()){auto node=pending.back();pending.pop_back();N(node!=nullptr);if(!node)continue;
   N(node->op);D(node->number);S(node->variable);pending.push_back(node->right.get());pending.push_back(node->left.get());}
 }
};
struct Reader {
 const Bytes& b;size_t p=0;bool mixedAnimationTracks=false;
 uint64_t N(){auto n=Le(b,p,8);p+=8;return n;}
 size_t Count(){auto n=N();Require(n<=(b.size()-p)/8,"author directory count exceeds remaining bytes");return size_t(n);}
 unsigned U(){auto n=N();Require(n<=UINT_MAX,"author directory unsigned field overflow");return unsigned(n);}
 bool B(){auto n=N();Require(n<=1,"author directory bool field differs");return n!=0;}
 std::string S(){auto n=N();Require(n<=b.size()-p,"author directory string truncated");auto s=Text(b.data()+p,size_t(n));p+=size_t(n);return s;}
 double D(){auto bits=N();double n;memcpy(&n,&bits,8);Require(std::isfinite(n),"author directory nonfinite number");return n;}
 std::vector<std::string> Strings(){std::vector<std::string> v;auto count=Count();v.reserve(count);for(size_t i=0;i<count;++i)v.push_back(S());return v;}
 std::shared_ptr<EiemExpression> Expr(){
  std::shared_ptr<EiemExpression> root;std::vector<std::shared_ptr<EiemExpression>*> pending{&root};std::vector<EiemExpression*> nodes;
  while(!pending.empty()){auto slot=pending.back();pending.pop_back();if(!B())continue;auto node=EiemExpression::Make();auto op=U();Require(op<=EiemExpression::Or,"author expression opcode differs");node->op=EiemExpression::Op(op);node->number=D();node->variable=S();*slot=node;nodes.push_back(node.get());pending.push_back(&node->right);pending.push_back(&node->left);}
  for(auto node:nodes){auto op=node->op;bool leaf=op==EiemExpression::Number||op==EiemExpression::Variable,unary=op==EiemExpression::Not||op==EiemExpression::Negative;Require(leaf?(!node->left&&!node->right):(unary?(bool(node->left)&&!node->right):(bool(node->left)&&bool(node->right))),"author expression arity differs");}return root;
 }
};
inline void Relative(const std::string& s){
 auto p=std::filesystem::u8path(s);Require(!s.empty()&&!p.is_absolute()&&s.find_first_of("\\:\r\n\t")==std::string::npos,"author asset path must be relative");
 for(const auto& part:p)Require(part!=".."&&part!="."&&!part.empty(),"author asset path escapes its Mod");
}
inline void Validate(const Directory& d){
 EiemAuthorControlMetadata::Validate(d.runtime);
 Require(!d.mod.empty(),"author asset directory Mod identity absent");std::set<std::string> parts,files,materials,skeletonIds,meshSkeletonIds;
 for(const auto& skeleton:d.skeletons){Relative(skeleton.file);Require(!skeleton.id.empty()&&skeletonIds.insert(skeleton.id).second&&files.insert(skeleton.file).second,"author Skeleton identity/file differs");}
 for(const auto& m:d.meshes){Relative(m.file);Require(!m.part.empty()&&!m.marker.empty()&&m.path.rfind("assets/",0)==0&&m.vertices&&m.indices&&m.indices%3==0&&m.hash.size()==64&&m.hash.find_first_not_of("0123456789ABCDEF")==std::string::npos,"author Mesh directory identity/geometry/hash differs");
  Require(m.nativeBytes>0,"author Mesh native payload extent is absent");
  Require(m.authorBytes>0,"author Mesh editable payload extent is absent");
  Require(m.nativeBytes<=SIZE_MAX-m.authorBytes,"author Mesh payload extent overflows");
  Require(m.authorHash.size()==64&&m.authorHash.find_first_not_of("0123456789ABCDEF")==std::string::npos,"author Mesh editable snapshot hash differs");
  Require(parts.insert(m.part).second&&files.insert(m.file).second&&
    (m.skeleton.empty()||skeletonIds.count(m.skeleton)),"author Mesh Part/file/Skeleton duplicated or unresolved");
  if(!m.skeleton.empty())meshSkeletonIds.insert(m.skeleton);
  EiemMeshBindingMetadata::Validate(m.binding,m.bones.size());
  std::set<std::string> fields;for(const auto& field:m.fields)Require(!field.empty()&&fields.insert(field).second,"author Mesh field selection empty/duplicated");
  Require(fields.count("m_Name")&&fields.count("m_VertexData")&&fields.count("m_IndexBuffer"),"author Mesh required field selection absent");}
 std::set<std::string> textures,texturePaths;std::map<std::string,std::string> textureFiles,ddsFiles;
 for(const auto& t:d.textures){Relative(t.file);Relative(t.ddsFile);auto prior=textureFiles.emplace(t.file,t.ddsFile),ddsPrior=ddsFiles.emplace(t.ddsFile,t.file);
  Require(t.file!=t.ddsFile&&!t.id.empty()&&textures.insert(t.id).second&&
   (prior.second||prior.first->second==t.ddsFile)&&
   (ddsPrior.second||ddsPrior.first->second==t.file),
   "author Texture ID/file paths are absent or ambiguous");
  if(prior.second)Require(!files.count(t.file)&&texturePaths.insert(t.file).second,
                          "author Texture scaffold path overlaps another resource");
  if(ddsPrior.second)Require(!files.count(t.ddsFile)&&texturePaths.insert(t.ddsFile).second,
                             "author DDS path overlaps another resource");
 }
 files.insert(texturePaths.begin(),texturePaths.end());
 for(const auto& m:d.materials){Relative(m.file);Require(!m.id.empty()&&materials.insert(m.id).second&&!m.source.path.empty()&&!m.source.asset.empty()&&m.source.path.rfind("assets/",0)==0&&m.authorBytes&&m.hash.size()==64&&m.hash.find_first_not_of("0123456789ABCDEF")==std::string::npos&&files.insert(m.file).second,"author Material edit identity/file/hash absent or duplicated");
  for(const auto& binding:m.textures)Require(!binding.first.empty()&&textures.count(binding.second),"author Material binding references an absent Texture: "+m.id+"/"+binding.first);}
 for(const auto& mesh:d.meshes)Require(mesh.material.empty()||materials.count(mesh.material),"author Mesh Part references absent Material");
 std::set<std::string> ruleNames,covered;std::set<Identity> targets;
 for(const auto& rule:d.rules){Require(!rule.id.empty()&&ruleNames.insert(rule.id).second&&!rule.targets.empty()&&(rule.skip?rule.parts.empty():!rule.parts.empty()),"author Replace rule identity/handling differs");
  for(const auto& target:rule.targets)Require(target.second.path.rfind("assets/",0)==0&&!target.second.asset.empty()&&targets.insert(target.second).second,"author Mesh target duplicated or absent");
  std::set<std::string> listed;for(const auto& part:rule.parts){Require(parts.count(part)&&listed.insert(part).second,"author Part absent/duplicated within Replace rule");covered.insert(part);}
 }
 Require(covered==parts,"author asset directory contains an unowned Part");
 for(const auto& state:d.runtime.parts){Require(parts.count(state.first),"author controlled Part absent");auto mesh=std::find_if(d.meshes.begin(),d.meshes.end(),[&](const auto& row){return row.part==state.first;});for(const auto& shape:state.second.shapes)Require(std::find(mesh->shapes.begin(),mesh->shapes.end(),shape.first)!=mesh->shapes.end(),"author controlled shape absent");}
 for(const auto& channel:d.runtime.materialTracks)Require(materials.count(channel.second.material),"author animated Material absent");
 for(const auto& animation:d.runtime.animations)for(const auto& track:animation.second.boneTracks){
  Require(skeletonIds.count(track.skeleton)&&meshSkeletonIds.count(track.skeleton)&&!track.path.empty()&&track.samples.size()>=2,
    "author animation Bone track Skeleton/path/sample declaration differs");
  for(const auto& sample:track.samples){double rotation=0;for(size_t i=0;i<sample.size();++i){
    Require(std::isfinite(sample[i]),"author animation Bone sample is nonfinite");
    if(i>=3&&i<7)rotation+=double(sample[i])*sample[i];
    if(i>=7)Require(sample[i]>0,"author animation Bone scale is not positive");}
   Require(std::abs(rotation-1.0)<=.001,"author animation Bone rotation is not normalized");}
 }
 std::map<std::string,std::string> effectFiles;
 for(const auto& row:d.runtime.effects){const auto& effect=row.second;Require(effect.part.empty()||parts.count(effect.part),"author effect Part absent");
  for(const auto& input:effect.native->bundles){Relative(input.file);auto prior=effectFiles.emplace(input.file,input.hash);Require(!files.count(input.file)&&!textureFiles.count(input.file)&&(prior.second||prior.first->second==input.hash),"author effect input file collision");}}
 std::set<std::string> auxiliaryIds;
 for(const auto& resource:d.auxiliary){Relative(resource.file);
  Require(!resource.id.empty()&&auxiliaryIds.insert(resource.id).second&&files.insert(resource.file).second&&!textureFiles.count(resource.file)&&!effectFiles.count(resource.file)&&resource.kind==5&&resource.bytes>=80&&resource.hash.size()==64&&resource.hash.find_first_not_of("0123456789ABCDEF")==std::string::npos&&resource.authorBytes>0&&resource.authorHash.size()==64&&resource.authorHash.find_first_not_of("0123456789ABCDEF")==std::string::npos&&!resource.targets.empty(),"author auxiliary identity/file/layout duplicated or absent");
  for(const auto& target:resource.targets)Require(targets.count(target),"author auxiliary target is not a declared Mesh");
 }
}
inline Bytes Encode(const Directory& d){
 Validate(d);Writer w;w.S(structure);w.S(d.mod);w.N(d.meshes.size());
 for(const auto& m:d.meshes){w.S(m.part);w.S(m.file);w.S(m.path);w.S(m.marker);w.S(m.hash);w.S(m.material);w.N(m.materialSlot);w.S(m.skeleton);w.N(m.vertices);w.N(m.indices);w.Strings(m.bones);w.Strings(m.shapes);
  w.N(m.binding.hashes.size());for(auto hash:m.binding.hashes)w.N(hash);
  w.N(m.binding.boneBounds.size());for(const auto& bounds:m.binding.boneBounds){for(auto x:bounds.first)w.D(x);for(auto x:bounds.second)w.D(x);}w.Strings(m.fields);w.N(m.nativeBytes);w.N(m.authorBytes);w.S(m.authorHash);}
 w.N(d.textures.size());for(const auto& t:d.textures){w.S(t.id);w.S(t.file);w.S(t.ddsFile);}
 w.N(d.skeletons.size());for(const auto& s:d.skeletons){w.S(s.id);w.S(s.file);}
 w.N(d.materials.size());for(const auto& m:d.materials){w.S(m.id);w.S(m.file);w.S(m.source.path);w.S(m.source.asset);w.N(m.authorBytes);w.S(m.hash);w.N(m.textures.size());for(const auto& binding:m.textures){w.S(binding.first);w.S(binding.second);}}
 w.N(d.rules.size());for(const auto& rule:d.rules){w.S(rule.id);w.N(rule.skip);w.N(rule.targets.size());for(const auto& target:rule.targets){w.N(target.first);w.S(target.second.path);w.S(target.second.asset);}w.Strings(rule.parts);}
 w.N(d.auxiliary.size());for(const auto& resource:d.auxiliary){w.S(resource.id);w.S(resource.file);w.S(resource.hash);w.N(resource.kind);w.N(resource.bytes);w.N(resource.targets.size());for(const auto& target:resource.targets){w.S(target.path);w.S(target.asset);}w.N(resource.authorBytes);w.S(resource.authorHash);}
 EiemAuthorControlMetadata::Write(w,d.runtime);
 auto hash=Sha(w.b.data(),w.b.size());w.b.insert(w.b.end(),hash.begin(),hash.end());return w.b;
}
inline Directory Decode(const Bytes& bytes){
 Require(bytes.size()>=32,"author asset directory truncated");auto hash=Sha(bytes.data(),bytes.size()-32);
 Require(std::equal(hash.begin(),hash.end(),bytes.end()-32),"author asset directory checksum differs");Bytes payload(bytes.begin(),bytes.end()-32);Reader r{payload};
 auto version=r.S();const bool hasSkeletons=version==structure;const bool hasMaterialSlot=hasSkeletons||version=="EFF_AUTHOR_ASSETS/native-mesh+editable-mesh-author-data+optional-material+source-material-slot+material-edits+logical-material-source+standard-dds+replace-rules+binding-bounds+field-selection+auxiliary+controls+sampled-animation+lazy-effects/utf8+le64+sha256/6";
 const bool hasBoneTracks=hasSkeletons;
 Require(hasMaterialSlot||version=="EFF_AUTHOR_ASSETS/native-mesh+editable-mesh-author-data+material-edits+logical-material-source+standard-dds+replace-rules+binding-bounds+field-selection+auxiliary+controls+sampled-animation+lazy-effects/utf8+le64+sha256/5","author asset directory structure differs");
 Directory d;d.mod=r.S();auto n=r.Count();d.meshes.reserve(n);
 for(size_t i=0;i<n;++i){MeshAsset m;m.part=r.S();m.file=r.S();m.path=r.S();m.marker=r.S();m.hash=r.S();m.material=r.S();if(hasMaterialSlot)m.materialSlot=r.U();if(hasSkeletons)m.skeleton=r.S();m.vertices=r.U();m.indices=r.U();m.bones=r.Strings();m.shapes=r.Strings();
  auto count=r.Count();for(size_t j=0;j<count;++j)m.binding.hashes.push_back(r.U());
  count=r.Count();for(size_t j=0;j<count;++j){EiemMeshBindingMetadata::Bounds bounds;for(auto& x:bounds.first)x=r.D();for(auto& x:bounds.second)x=r.D();m.binding.boneBounds.push_back(bounds);}m.fields=r.Strings();auto native=r.N(),author=r.N();Require(native<=SIZE_MAX&&author<=SIZE_MAX,"author Mesh payload extent overflow");m.nativeBytes=size_t(native);m.authorBytes=size_t(author);m.authorHash=r.S();d.meshes.push_back(std::move(m));}
 n=r.Count();d.textures.reserve(n);for(size_t i=0;i<n;++i){TextureAsset t;t.id=r.S();t.file=r.S();t.ddsFile=r.S();d.textures.push_back(std::move(t));}
 if(hasSkeletons){n=r.Count();d.skeletons.reserve(n);for(size_t i=0;i<n;++i){SkeletonAsset s;s.id=r.S();s.file=r.S();d.skeletons.push_back(std::move(s));}}
 n=r.Count();d.materials.reserve(n);for(size_t i=0;i<n;++i){MaterialEntry m;m.id=r.S();m.file=r.S();m.source={r.S(),r.S()};auto author=r.N();Require(author<=SIZE_MAX,"author Material extent overflow");m.authorBytes=size_t(author);m.hash=r.S();auto count=r.Count();for(size_t j=0;j<count;++j){auto slot=r.S(),texture=r.S();Require(m.textures.emplace(std::move(slot),std::move(texture)).second,"duplicate author Material texture slot");}d.materials.push_back(std::move(m));}
 n=r.Count();d.rules.reserve(n);for(size_t i=0;i<n;++i){ReplaceRule rule;rule.id=r.S();rule.skip=r.B();auto count=r.Count();for(size_t j=0;j<count;++j){auto level=r.U();Identity identity{r.S(),r.S()};Require(rule.targets.emplace(level,std::move(identity)).second,"duplicate author LOD target");}rule.parts=r.Strings();d.rules.push_back(std::move(rule));}
 n=r.Count();for(size_t i=0;i<n;++i){Auxiliary resource;resource.id=r.S();resource.file=r.S();resource.hash=r.S();resource.kind=r.U();auto extent=r.N();Require(extent<=SIZE_MAX,"author auxiliary size overflow");resource.bytes=size_t(extent);auto count=r.Count();for(size_t j=0;j<count;++j){auto path=r.S(),asset=r.S();Require(resource.targets.emplace(Identity{std::move(path),std::move(asset)}).second,"duplicate auxiliary target");}auto author=r.N();Require(author<=SIZE_MAX,"author auxiliary snapshot size overflow");resource.authorBytes=size_t(author);resource.authorHash=r.S();d.auxiliary.push_back(std::move(resource));}
 r.mixedAnimationTracks=hasBoneTracks;d.runtime=EiemAuthorControlMetadata::Read(r,hasBoneTracks);
 Require(r.p==payload.size(),"author asset directory trailing data");Validate(d);return d;
}
} // namespace EiemRuntimeAuthorDirectory

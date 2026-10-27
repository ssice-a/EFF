#pragma once
#include "eiem_resource_reload_plan.h"
#include "eiem_readonly_file_range.h"
#include "eiem_package_protocol.h"
#include "eiem_composed_package.h"
#include <compressapi.h>
#pragma comment(lib,"cabinet.lib")

// Structurally described, immutable output of the disk-only compiler. No Unity calls.
// Rules reference author payloads; their runtime applier uses checked TypeTrees.
namespace EiemOfflinePackage {
using namespace EiemResourceReloadPlan;
constexpr const char* compiler = EiemPackageProtocol::compiler;
struct Writer {
 Bytes b;
 void N(uint64_t n){for(unsigned i=0;i<8;++i)b.push_back((unsigned char)(n>>(i*8)));}
 void B(bool n){N(n);}
 void D(double n){Require(std::isfinite(n),"offline nonfinite number");uint64_t bits;memcpy(&bits,&n,8);N(bits);}
 void Raw(const unsigned char* p,size_t n){if(n)b.insert(b.end(),p,p+n);}
 void Blob(const Bytes& v){N(v.size());Raw(v.data(),v.size());}
 void S(const std::string& s){Text((const unsigned char*)s.data(),s.size());N(s.size());Raw((const unsigned char*)s.data(),s.size());}
 void Strings(const std::vector<std::string>& v){N(v.size());for(const auto& s:v)S(s);}
 void Set(const std::set<std::string>& v){N(v.size());for(const auto& s:v)S(s);}
 void Strings(const std::map<std::string,std::string>& v){N(v.size());for(const auto& s:v){S(s.first);S(s.second);}}
 void Id(const Identity& v){S(v.path);S(v.asset);}
 void Expr(const std::shared_ptr<EiemExpression>& v){
  std::vector<const EiemExpression*> pending{v.get()};
  while(!pending.empty()){auto node=pending.back();pending.pop_back();B(node!=nullptr);if(!node)continue;
   N(node->op);D(node->number);S(node->variable);pending.push_back(node->right.get());pending.push_back(node->left.get());}
 }
};
struct Reader {
 const Bytes& b;size_t p=0;const Package* verifiedAuthor=nullptr;bool mapResourceBytes=false;
 uint64_t N(){auto n=Le(b,p,8);p+=8;return n;}
 size_t Count(){auto n=N();Require(n<=(b.size()-p)/8,"offline count exceeds encoded extent");return size_t(n);}
 size_t Size(){auto n=N();Require(n<=SIZE_MAX,"offline size cannot be represented");return size_t(n);}
 unsigned U(){auto n=N();Require(n<=UINT_MAX,"offline integer width");return unsigned(n);}
 bool B(){auto n=N();Require(n<=1,"offline boolean");return n!=0;}
 double D(){auto bits=N();double n;memcpy(&n,&bits,8);Require(std::isfinite(n),"offline nonfinite number");return n;}
 void Raw(unsigned char* dst,size_t n){Require(p<=b.size()&&n<=b.size()-p,"offline truncated bytes");if(n)memcpy(dst,b.data()+p,n);p+=n;}
 Bytes Blob(){auto n=Size();Require(n<=b.size()-p,"offline blob truncated");Bytes v(n);Raw(v.data(),n);return v;}
 std::string S(){auto n=Size();Require(n<=b.size()-p,"offline string truncated");auto s=Text(b.data()+p,n);p+=n;return s;}
 std::vector<std::string> Strings(){std::vector<std::string> v;auto n=Count();for(size_t i=0;i<n;++i)v.push_back(S());return v;}
 std::set<std::string> Set(){std::set<std::string> v;auto n=Count();for(size_t i=0;i<n;++i)Require(v.insert(S()).second,"offline duplicate set entry");return v;}
 std::map<std::string,std::string> StringsMap(){std::map<std::string,std::string> v;auto n=Count();for(size_t i=0;i<n;++i){auto k=S(),s=S();Require(v.emplace(k,s).second,"offline duplicate map key");}return v;}
 Identity Id(bool optional=false){auto path=S(),asset=S();Require((optional&&path.empty()&&asset.empty())||(path.rfind("assets/",0)==0&&path.find("..") ==path.npos&&!asset.empty()),"offline source identity");return {path,asset};}
 std::shared_ptr<EiemExpression> Expr(){
  std::shared_ptr<EiemExpression> root;std::vector<std::shared_ptr<EiemExpression>*> pending{&root};std::vector<EiemExpression*> nodes;
  while(!pending.empty()){auto slot=pending.back();pending.pop_back();if(!B())continue;
   auto v=EiemExpression::Make();auto op=U();Require(op<=EiemExpression::Or,"offline expression opcode");v->op=EiemExpression::Op(op);v->number=D();v->variable=S();*slot=v;nodes.push_back(v.get());
   pending.push_back(&v->right);pending.push_back(&v->left);
  }
  for(auto v:nodes){auto op=v->op;bool leaf=op==EiemExpression::Number||op==EiemExpression::Variable,unary=op==EiemExpression::Not||op==EiemExpression::Negative;
   Require(leaf?(!v->left&&!v->right):(unary?(bool(v->left)&&!v->right):(bool(v->left)&&bool(v->right))),"offline expression arity");
   if(op==EiemExpression::Variable)Require(EiemVariableName(v->variable),"offline expression variable");
  }return root;
 }
};
inline void WriteAuthor(Writer& w,const Package& p,bool references=false,bool bindingLabels=true){
 w.S(p.id);w.S(p.root.filename().u8string());w.Raw(p.iniHash.data(),p.iniHash.size());
 w.N(p.resources.size());for(const auto& row:p.resources){const auto& r=*row.second;w.S(row.first);w.N(r.kind);w.Raw(r.type.data(),16);if(references)w.S(ResourceContentHash(r));else{Require(r.bytes.size()==ResourceSize(r),"mapped runtime resource cannot be embedded");w.Blob(r.bytes);}w.N(r.blocks.size());for(const auto& block:r.blocks){w.S(block.first);w.N(block.second.offset);w.N(block.second.size);w.Raw(block.second.digest.data(),32);}}
 w.N(p.meshes.size());for(const auto& row:p.meshes){w.S(row.first);const auto& v=row.second;w.S(v.resource);w.N(v.vertices);w.N(v.indices);w.Strings(v.bones);w.Strings(v.shapes);}
 w.N(p.materials.size());for(const auto& row:p.materials){w.S(row.first);w.S(row.second.resource);w.Id(row.second.source);w.Strings(row.second.textures);}
 w.N(p.textures.size());for(const auto& row:p.textures){w.S(row.first);const auto& v=row.second;w.S(v.resource);w.Id(v.source);w.N(v.width);w.N(v.height);w.N(v.mips);w.N(v.filter);w.N(v.wrap);w.N(v.aniso);w.B(v.linear);w.D(v.bias);}
 w.N(p.parts.size());for(const auto& row:p.parts){w.S(row.first);w.S(row.second.mesh);w.S(row.second.material);w.Expr(row.second.visible);w.Strings(row.second.shapes);}
 w.N(p.rules.size());for(const auto& row:p.rules){w.S(row.first);w.Strings(row.second.parts);w.B(row.second.skip);w.N(row.second.targets.size());for(const auto& t:row.second.targets){w.N(t.first);w.Id(t.second);}}
 std::map<std::string,double> defaults(p.defaults.begin(),p.defaults.end());w.N(defaults.size());for(const auto& row:defaults){w.S(row.first);w.D(row.second);}w.Set(p.persistent);
 w.N(p.controls.size());for(const auto& row:p.controls){w.S(row.first);w.S(row.second.label);w.D(row.second.minimum);w.D(row.second.maximum);}
 w.N(p.bindings.size());for(const auto& v:p.bindings){w.S(v.id);w.S(v.variable);w.S(v.type);w.N(v.key.vk);w.N(v.key.modifiers);w.D(v.speed);w.N(v.values.size());for(auto n:v.values)w.D(n);if(bindingLabels)w.S(v.label);}
}
inline void ReadAuthorReference(Reader& r,const Package& package,const std::string& key,const std::string& hash,Resource& resource){
 // One compiler invocation owns an immutable, checksum-verified author
 // snapshot. Reuse its exact digest matches for previous/roundtrip manifests;
 // a changed resource still reads and verifies its previous published file.
 if(r.verifiedAuthor&&r.verifiedAuthor->id==package.id){auto found=r.verifiedAuthor->resources.find(key);
  if(found!=r.verifiedAuthor->resources.end()&&found->second->kind==resource.kind&&found->second->type==resource.type&&ResourceContentHash(*found->second)==hash){resource.bytes=found->second->bytes;resource.mapped=found->second->mapped;resource.mappedBytes=found->second->mappedBytes;resource.byteSize=ResourceSize(*found->second);resource.directoryVerified=found->second->directoryVerified;resource.blocks=found->second->blocks;resource.contentHash=hash;return;}
 }
 auto path=DiskPath(package.root,key,resource.kind);
 if(r.mapResourceBytes&&(resource.kind==1||resource.kind==2||resource.kind==4)){
  std::shared_ptr<const EiemReadonlyFileRange::Range> mapped;
  try{mapped=std::make_shared<const EiemReadonlyFileRange::Range>(path,0,0,true);}catch(const std::runtime_error&){resource=Decode(Read(path),resource.kind);}
  if(mapped){resource=DecodeView(mapped->Data(),mapped->Size(),resource.kind);resource.mappedBytes=mapped->Data();resource.mapped=std::move(mapped);}
 }else resource=Decode(Read(path),resource.kind);
 Require(ResourceContentHash(resource)==hash,"rule author resource content changed");
}
inline Package ReadAuthor(Reader& r,const std::filesystem::path& modRoot,bool references=false,const std::filesystem::path& exactRoot={},bool bindingLabels=true){
 Package p;p.id=r.S();auto folder=r.S();Require(Identifier(p.id)&&Identifier(folder)&&folder!="."&&folder!="..", "offline Mod identity/folder");p.root=exactRoot.empty()?modRoot/std::filesystem::u8path(folder):exactRoot;r.Raw(p.iniHash.data(),32);
 auto n=r.Count();for(size_t i=0;i<n;++i){
  auto key=r.S();auto v=std::make_shared<Resource>();std::shared_ptr<const Resource> shared;
  v->kind=r.U();Require(v->kind!=0,"offline resource kind");r.Raw(v->type.data(),16);
  if(references){auto hash=r.S();
   if(r.verifiedAuthor&&r.verifiedAuthor->id==p.id){auto found=r.verifiedAuthor->resources.find(key);
    if(found!=r.verifiedAuthor->resources.end()&&found->second->kind==v->kind&&found->second->type==v->type&&ResourceContentHash(*found->second)==hash)shared=found->second;}
   if(!shared)ReadAuthorReference(r,p,key,hash,*v);
  }else {v->bytes=r.Blob();v->contentHash=Hash(v->bytes);}
  auto verifiedBlocks=std::move(v->blocks);auto directoryVerified=v->directoryVerified;auto blocks=r.Count();auto size=shared?ResourceSize(*shared):ResourceSize(*v);std::map<std::string,Block> declaredBlocks;
  for(size_t j=0;j<blocks;++j){auto name=r.S();Block b;b.offset=r.Size();b.size=r.Size();r.Raw(b.digest.data(),32);
   Require(b.offset<=size&&b.size<=size-b.offset&&declaredBlocks.emplace(name,b).second,"offline resource block range/duplicate");}
  if(shared){Require(declaredBlocks.size()==shared->blocks.size(),"offline shared resource block coverage differs");
   for(const auto& block:declaredBlocks){auto found=shared->blocks.find(block.first);Require(found!=shared->blocks.end()&&block.second.offset==found->second.offset&&block.second.size==found->second.size&&block.second.digest==found->second.digest,"offline shared resource block directory differs");}}
  else if(directoryVerified){Require(declaredBlocks.size()==verifiedBlocks.size(),"offline verified resource block coverage differs");
   for(const auto& block:declaredBlocks){auto found=verifiedBlocks.find(block.first);Require(found!=verifiedBlocks.end()&&block.second.offset==found->second.offset&&block.second.size==found->second.size&&block.second.digest==found->second.digest,"offline verified resource block directory differs");}}
  v->blocks=std::move(declaredBlocks);
  Require(p.resources.emplace(key,shared?shared:std::shared_ptr<const Resource>(v)).second,"offline duplicate resource");
 }
 n=r.Count();for(size_t i=0;i<n;++i){auto k=r.S();Mesh v;v.resource=r.S();v.vertices=r.U();v.indices=r.U();v.bones=r.Strings();v.shapes=r.Strings();Require(v.vertices&&p.resources.count(v.resource)&&p.resources.at(v.resource)->kind==1&&p.meshes.emplace(k,std::move(v)).second,"offline Mesh descriptor");}
 n=r.Count();for(size_t i=0;i<n;++i){auto k=r.S();EiemDirectory::Material v;v.resource=r.S();v.source=r.Id();v.textures=r.StringsMap();Require(p.resources.count(v.resource)&&p.resources.at(v.resource)->kind==2&&p.materials.emplace(k,std::move(v)).second,"offline Material descriptor");}
 n=r.Count();for(size_t i=0;i<n;++i){auto k=r.S();EiemDirectory::Texture v;v.resource=r.S();v.source=r.Id(true);v.width=r.U();v.height=r.U();v.mips=r.U();v.filter=r.U();v.wrap=r.U();v.aniso=r.U();v.linear=r.B();v.bias=r.D();Require(v.width&&v.height&&v.mips&&p.resources.count(v.resource)&&p.resources.at(v.resource)->kind==4&&p.textures.emplace(k,std::move(v)).second,"offline Texture descriptor");}
 n=r.Count();for(size_t i=0;i<n;++i){auto k=r.S();EiemDirectory::Part v;v.mesh=r.S();v.material=r.S();v.visible=r.Expr();v.shapes=r.StringsMap();Require(p.meshes.count(v.mesh)&&p.materials.count(v.material)&&p.parts.emplace(k,std::move(v)).second,"offline Part descriptor");}
 n=r.Count();for(size_t i=0;i<n;++i){auto k=r.S();Rule v;v.parts=r.Strings();v.skip=r.B();auto count=r.Count();for(size_t j=0;j<count;++j){auto level=r.U();auto id=r.Id();Require(v.targets.emplace(level,id).second&&p.targetIndex.emplace(id,k).second,"offline duplicate target");}for(const auto& part:v.parts)Require(p.parts.count(part),"offline absent part");Require(!v.targets.empty()&&(v.skip?v.parts.empty():!v.parts.empty())&&p.rules.emplace(k,std::move(v)).second,"offline Rule descriptor");}
 n=r.Count();for(size_t i=0;i<n;++i){auto k=r.S();auto value=r.D();Require(EiemVariableName(k)&&p.defaults.emplace(k,value).second,"offline defaults");}p.persistent=r.Set();for(const auto& k:p.persistent)Require(p.defaults.count(k),"offline persistent variable");
 n=r.Count();for(size_t i=0;i<n;++i){auto k=r.S();Control v;v.label=r.S();v.minimum=r.D();v.maximum=r.D();Require(v.minimum<=v.maximum&&p.defaults.count(k)&&p.controls.emplace(k,v).second,"offline control domain");}
 n=r.Count();std::set<std::string> bindings;for(size_t i=0;i<n;++i){Binding v;v.id=r.S();v.variable=r.S();v.type=r.S();v.key.vk=r.U();v.key.modifiers=r.U();v.speed=r.D();auto count=r.Count();for(size_t j=0;j<count;++j)v.values.push_back(r.D());if(bindingLabels)v.label=r.S();Require(p.defaults.count(v.variable)&&bindings.insert(v.id).second&&v.key.vk&&v.key.vk<=255&&!(v.key.modifiers&~(MOD_SHIFT|MOD_ALT|MOD_CONTROL))&&!v.values.empty()&&(v.type=="cycle"||v.type=="press"||v.type=="hold")&&v.speed>0,"offline binding descriptor");p.bindings.push_back(std::move(v));}
 for(const auto& part:p.parts){std::string error;Require(!part.second.visible||part.second.visible->Validate(p.defaults,error),"offline undeclared expression variable");for(const auto& shape:part.second.shapes)Require(p.controls.count(shape.second)&&std::find(p.meshes.at(part.second.mesh).shapes.begin(),p.meshes.at(part.second.mesh).shapes.end(),shape.first)!=p.meshes.at(part.second.mesh).shapes.end(),"offline shape binding");}
 for(const auto& m:p.materials)for(const auto& t:m.second.textures)Require(p.textures.count(t.second),"offline absent Texture override");RestoreShapeAnimations(p);ValidateShapeAnimations(p);return p;
}
inline void WriteAddress(Writer& w,const EiemRenderAssemblyDescription::Address& a){w.S(a.path);w.N(a.component);}
inline EiemRenderAssemblyDescription::Address ReadAddress(Reader& r){auto path=r.S();auto component=r.U();return {path,unsigned(component)};}
inline void WriteAssembly(Writer& w,const EiemRenderAssemblyDescription::Description& d){
  w.N(d.sources.size());for(const auto& s:d.sources){w.S(s.cab);w.N(uint64_t(s.id));WriteAddress(w,s.renderer);w.N(s.targets.size());for(const auto& t:s.targets){WriteAddress(w,t.renderer);w.S(t.marker);w.Strings(t.Bones());
  w.S(t.root);w.S(t.skin);w.S(t.probe);w.S(t.lighting);w.B(t.hasRoot);w.B(t.hasSkin);w.B(t.hasProbe);w.B(t.hasLighting);w.B(t.extension);}}
 w.N(d.lods.size());for(const auto& l:d.lods){WriteAddress(w,l.group);w.N(l.levels.size());for(const auto& level:l.levels){w.D(level.height);w.D(level.fade);w.N(level.renderers.size());for(const auto& a:level.renderers)WriteAddress(w,a);}}
 w.N(d.helpers.size());for(const auto& h:d.helpers){WriteAddress(w,h.component);w.B(h.custom);w.N(h.renderers.size());for(const auto& a:h.renderers)WriteAddress(w,a);}
}
inline EiemRenderAssemblyDescription::Description ReadAssembly(Reader& r){
 using namespace EiemRenderAssemblyDescription;Description d;std::set<std::pair<std::string,int64_t>> sources;std::set<std::pair<std::string,unsigned>> anchors,targets;
 auto n=r.Count();for(size_t i=0;i<n;++i){EiemRenderAssemblyDescription::Source s;s.cab=r.S();s.id=int64_t(r.N());s.renderer=ReadAddress(r);
  Require(EiemLoadingGeneration::Cab(s.cab)&&s.id&&sources.emplace(s.cab,s.id).second&&anchors.emplace(s.renderer.path,s.renderer.component).second,"offline assembly source identity/anchor");
  auto count=r.Count();for(size_t j=0;j<count;++j){Target t;t.renderer=ReadAddress(r);t.marker=r.S();t.bones=r.Strings();t.root=r.S();t.skin=r.S();t.probe=r.S();t.lighting=r.S();t.hasRoot=r.B();t.hasSkin=r.B();t.hasProbe=r.B();t.hasLighting=r.B();t.extension=r.B();
   Require(targets.emplace(t.renderer.path,t.renderer.component).second&&(!t.extension||(j>0&&!t.marker.empty()))&&((j==0)==!t.extension)&&(!j?(t.renderer==s.renderer):!(t.renderer==s.renderer)),"offline assembly target ownership");s.targets.push_back(std::move(t));
  }d.sources.push_back(std::move(s));
 }
 n=r.Count();for(size_t i=0;i<n;++i){Lod l;l.group=ReadAddress(r);auto count=r.Count();for(size_t j=0;j<count;++j){LodLevel level;level.height=float(r.D());level.fade=float(r.D());auto entries=r.Count();for(size_t k=0;k<entries;++k){auto a=ReadAddress(r);Require(targets.count({a.path,a.component}),"offline LOD references unowned Renderer");level.renderers.push_back(std::move(a));}l.levels.push_back(std::move(level));}d.lods.push_back(std::move(l));}
 n=r.Count();for(size_t i=0;i<n;++i){Helper h;h.component=ReadAddress(r);h.custom=r.B();auto count=r.Count();for(size_t j=0;j<count;++j){auto a=ReadAddress(r);Require(targets.count({a.path,a.component}),"offline helper references unowned Renderer");h.renderers.push_back(std::move(a));}d.helpers.push_back(std::move(h));}return d;
}
struct Manifest {Plan plan;Package package;std::string baseline;std::vector<Package> packages;bool composed=false;bool rootAssembly=EiemPackageProtocol::rootAssembly;};
inline std::vector<Package> Authors(const Manifest& v){return v.composed?v.packages:std::vector<Package>{v.package};}
inline Bytes Encode(const Manifest& v,bool bindingLabels=true){Writer w;w.S(v.plan.rulesOnly?(bindingLabels?EiemPackageProtocol::ruleStructure:"author-references+prefab-renderer-rules+source-ranges/le64/sha256"):(bindingLabels?EiemPackageProtocol::manifestStructure:"author-graph+inputs+roots+closure+parts+materials/le64/sha256"));w.B(v.composed);w.B(v.rootAssembly);
 // Keep the former executable digest slot so existing field layouts stay readable.
 w.S(std::string(64,'0'));w.S(v.baseline);const auto& p=v.plan;w.S(p.key);w.S(p.author);w.S(p.contract);
 if(v.composed){w.N(v.packages.size());for(const auto& author:v.packages)WriteAuthor(w,author,p.rulesOnly,bindingLabels);}else WriteAuthor(w,v.package,p.rulesOnly,bindingLabels);
 w.N(p.inputs.size());for(const auto& in:p.inputs){w.S(in.file);w.S(in.hash);w.S(in.bundle);w.Set(in.externals);}
 w.N(p.roots.size());for(const auto& root:p.roots){w.S(root.path);w.N(root.input);w.N(root.markers.size());for(const auto& m:root.markers){w.S(m.first);w.N(m.second);}
 if(v.rootAssembly){w.S(root.cab);w.N(uint64_t(root.source));WriteAssembly(w,root.Assembly());}
 }
 w.N(p.closure.size());for(auto n:p.closure)w.N(n);w.Set(p.shared);w.Strings(p.names.members);w.Strings(p.names.bundles);
 w.N(p.parts.size());for(const auto& row:p.parts){w.S(row.first);const auto& part=row.second;w.S(part.id);w.S(part.marker);w.S(part.material);w.N(part.vertices);w.Strings(part.bones);w.Strings(part.shapes);}
 w.N(p.materials.size());for(const auto& row:p.materials){w.S(row.first);w.S(row.second.name);w.N(row.second.slots.size());for(const auto& slot:row.second.slots){w.S(slot.property);w.B(slot.nonnull);w.B(slot.local);w.S(slot.texture.name);w.N(slot.texture.width);w.N(slot.texture.height);w.N(slot.texture.mips);}}
 for(auto n:{p.meshParts,p.rendererParts,p.textures,p.materialMatches,p.opaque,p.rendererBoundsHits,p.rendererBoundsMisses})w.N(n);
 if(p.rulesOnly){Require(p.originals.size()==p.inputs.size()&&p.rendererRules.size()==p.inputs.size()&&p.startupHashes.size()==p.inputs.size(),"rule input coverage differs");w.N(p.originals.size());for(size_t i=0;i<p.originals.size();++i){const auto& s=p.originals[i];w.S(s.file);w.S(s.hash);w.S(s.physical.u8string());w.N(s.offset);w.N(s.size);w.S(p.startupHashes[i]);w.N(p.rendererRules[i].size());for(const auto& rule:p.rendererRules[i]){w.S(rule.cab);w.N(uint64_t(rule.id));w.Id(rule.mesh);}}}
 if(!p.rulesOnly)return w.b;
 if(!p.namespaceRecipes.empty()){
  Require(p.namespaceRecipes.size()==p.inputs.size(),"namespace recipe input coverage differs");w.S("namespace-offset-patches/le64/sha256");w.N(p.namespaceRecipes.size());
  for(const auto& input:p.namespaceRecipes){w.N(input.size());for(const auto& recipe:input){w.S(recipe.member);w.S(recipe.hash);w.N(recipe.patches.size());for(const auto& patch:recipe.patches){w.N(patch.offset);w.S(patch.before);w.S(patch.after);}}}
 }
 if(p.spanRecipes){
  Require(p.memberRecipes.size()==p.inputs.size(),"member recipe input coverage differs");w.S("member-rebuilds/source-author-spans/le64/sha256");w.N(p.frameRecipes.size());
  for(const auto& frame:p.frameRecipes){w.S(frame.resource);w.S(frame.hash);w.N(frame.offset);w.N(frame.size);w.N(frame.count);for(auto n:frame.stride)w.N(n);for(auto n:frame.start)w.N(n);}
  w.N(p.memberRecipes.size());for(const auto& input:p.memberRecipes){w.N(input.size());for(const auto& member:input){w.S(member.name);w.S(member.hash);w.N(member.flags);w.N(member.size);w.N(member.spans.size());for(const auto& span:member.spans){w.N(span.kind);w.S(span.source);w.N(span.offset);w.N(span.size);if(span.kind==EiemResourceRecipe::Literal)w.Blob(span.literal);}}}
 }
 COMPRESSOR_HANDLE handle=nullptr;Require(CreateCompressor(COMPRESS_ALGORITHM_XPRESS_HUFF,nullptr,&handle)!=0,"rule compressor unavailable");
 struct Close {COMPRESSOR_HANDLE handle;~Close(){CloseCompressor(handle);}}close{handle};SIZE_T bytes=0;
 Compress(handle,w.b.data(),w.b.size(),nullptr,0,&bytes);Require(bytes&&GetLastError()==ERROR_INSUFFICIENT_BUFFER,"rule compression extent unavailable");Bytes packed(bytes);Require(Compress(handle,w.b.data(),w.b.size(),packed.data(),packed.size(),&bytes)!=0,"rule compression failed");packed.resize(bytes);
 Writer envelope;envelope.S("compressed-rules/xpress-huffman/le64");envelope.N(w.b.size());envelope.Blob(packed);return envelope.b;
}
inline Manifest DecodeLayout(const Bytes& bytes,const std::filesystem::path& directory,const std::filesystem::path& mods,int layout=-1,const std::filesystem::path& exactRoot={},bool deferredAssembly=false,const Package* verifiedAuthor=nullptr,bool mapResourceBytes=false){
 Reader r{bytes};r.verifiedAuthor=verifiedAuthor;r.mapResourceBytes=mapResourceBytes;Manifest v;auto tag=r.S();
 const std::string legacyManifest="author-graph+inputs+roots+closure+parts+materials/le64/sha256";
 const std::string legacyRules="author-references+prefab-renderer-rules+source-ranges/le64/sha256";
 const std::string currentRules=EiemPackageProtocol::ruleStructure;
 const bool hasBindingLabels=tag==EiemPackageProtocol::manifestStructure||tag==currentRules;
 bool rules=tag==legacyRules||tag==currentRules;
 if(layout<0&&(tag==EiemPackageProtocol::manifestStructure||tag==legacyManifest||rules)){v.composed=r.B();v.rootAssembly=r.B();}
 else { // Legacy labels select a known field layout; they never gate on this build's ABI.
  auto base=tag;const std::string suffix="_MODSET_1";v.composed=base.size()>suffix.size()&&base.compare(base.size()-suffix.size(),suffix.size(),suffix)==0;
  if(v.composed)base.resize(base.size()-suffix.size());
  if(layout>=0){v.composed=(layout&1)!=0;v.rootAssembly=(layout&2)!=0;}
  else {Require(base=="EFF_NATIVE_PACK_V35_1"||base=="EFF_NATIVE_PACK_V36_ASSEMBLY_1","offline descriptor structure is unknown");v.rootAssembly=base=="EFF_NATIVE_PACK_V36_ASSEMBLY_1";}
 }
 auto reserved=r.S();v.baseline=r.S();auto& p=v.plan;p.composed=v.composed;p.rulesOnly=rules;p.directory=directory;p.key=r.S();p.author=r.S();p.contract=r.S();
 Require(DigestText(reserved)&&DigestText(v.baseline)&&DigestText(p.key)&&DigestText(p.author)&&DigestText(p.contract),"offline digest syntax");
 if(v.composed){auto count=r.Count();for(size_t i=0;i<count;++i)v.packages.push_back(ReadAuthor(r,mods,rules,exactRoot,hasBindingLabels));v.package=EiemComposedPackage::Compose(v.packages,true);}else v.package=ReadAuthor(r,mods,rules,exactRoot,hasBindingLabels);
 auto n=r.Count();std::set<std::string> files;for(size_t i=0;i<n;++i){Input in;in.file=r.S();in.hash=r.S();in.bundle=r.S();in.externals=r.Set();auto file=std::filesystem::u8path(in.file);Require(file==file.filename()&&file.extension()==".vfs"&&files.insert(in.file).second&&DigestText(in.hash),"offline input path/hash");in.path=directory/file;p.inputs.push_back(std::move(in));}
 n=r.Count();std::set<std::string> roots;for(size_t i=0;i<n;++i){Root root;root.path=r.S();root.input=r.Size();Require(root.input<p.inputs.size()&&root.path.rfind("assets/",0)==0&&root.path.find("..") ==root.path.npos&&roots.insert(root.path).second,"offline root descriptor");auto count=r.Count();for(size_t j=0;j<count;++j){auto marker=r.S();auto copies=r.U();Require(copies&&root.markers.emplace(marker,copies).second,"offline root markers");}
 if(v.rootAssembly){root.cab=r.S();root.source=int64_t(r.N());Require(EiemLoadingGeneration::Cab(root.cab)&&root.source,"offline source prefab identity");root.assembly=ReadAssembly(r);}
 else Require(v.composed || !root.markers.empty(),"offline empty root coverage");
 p.roots.push_back(std::move(root));}
 n=r.Count();std::set<size_t> closure;for(size_t i=0;i<n;++i){auto index=r.Size();Require(index<p.inputs.size()&&closure.insert(index).second,"offline closure duplicate/extent");p.closure.push_back(index);}p.shared=r.Set();p.names.members=r.StringsMap();p.names.bundles=r.StringsMap();
 n=r.Count();for(size_t i=0;i<n;++i){auto marker=r.S();EiemResourceReloadPlan::Part part;part.id=r.S();part.marker=r.S();part.material=r.S();part.vertices=r.U();part.bones=r.Strings();part.shapes=r.Strings();Require(marker==part.marker&&part.vertices&&v.package.parts.count(part.id)&&v.package.materials.count(part.material),"offline expected Mesh descriptor");
  const auto& authored=v.package.parts.at(part.id);const auto& mesh=v.package.meshes.at(authored.mesh);
  Require(part.material==authored.material&&part.vertices==mesh.vertices&&part.bones==mesh.bones&&part.shapes==mesh.shapes&&p.parts.emplace(marker,std::move(part)).second,"offline expected Mesh author/layout differs");}
 n=r.Count();for(size_t i=0;i<n;++i){auto key=r.S();EiemResourceReloadPlan::Material material;material.name=r.S();auto count=r.Count();std::set<std::string> slots;for(size_t j=0;j<count;++j){TextureSlot slot;slot.property=r.S();slot.nonnull=r.B();slot.local=r.B();slot.texture.name=r.S();slot.texture.width=int(r.U());slot.texture.height=int(r.U());slot.texture.mips=int(r.U());Require(slots.insert(slot.property).second&&(!slot.local||(slot.nonnull&&slot.texture.width>0&&slot.texture.height>0&&slot.texture.mips>0)),"offline Texture slot");material.slots.push_back(std::move(slot));}Require(v.package.materials.count(key)&&p.materials.emplace(key,std::move(material)).second,"offline expected Material");}
 p.meshParts=r.Size();p.rendererParts=r.Size();p.textures=r.Size();p.materialMatches=r.Size();p.opaque=r.Size();p.rendererBoundsHits=r.Size();p.rendererBoundsMisses=r.Size();
 if(rules){auto count=r.Count();Require(count==p.inputs.size(),"rule original input coverage differs");for(size_t i=0;i<count;++i){EiemResourceReloadPlan::Source source;source.file=r.S();source.hash=r.S();source.physical=std::filesystem::u8path(r.S());source.offset=r.N();source.size=r.N();Require(source.file==p.inputs[i].file&&DigestText(source.hash)&&(source.physical.is_absolute()||!exactRoot.empty())&&source.size,"rule original identity invalid");p.originals.push_back(std::move(source));auto hash=r.S();Require(DigestText(hash),"rule startup digest invalid");p.startupHashes.push_back(hash);auto n=r.Count();std::vector<RendererRule> rules;std::set<std::pair<std::string,int64_t>> seen;for(size_t j=0;j<n;++j){RendererRule rule;rule.cab=r.S();rule.id=int64_t(r.N());rule.mesh=r.Id();Require(rule.id&&v.package.targetIndex.count(rule.mesh)&&seen.emplace(rule.cab,rule.id).second,"rule Renderer identity invalid");rules.push_back(std::move(rule));}p.rendererRules.push_back(std::move(rules));}}
 while(rules&&r.p<bytes.size()){
  auto feature=r.S();if(feature=="namespace-offset-patches/le64/sha256"){
  Require(p.namespaceRecipes.empty(),"duplicate namespace recipe fields");auto count=r.Count();Require(count==p.inputs.size(),"namespace recipe input coverage differs");
  for(size_t i=0;i<count;++i){std::vector<MemberNamespaceRecipe> recipes;std::set<std::string> members;auto n=r.Count();
   for(size_t j=0;j<n;++j){MemberNamespaceRecipe recipe;recipe.member=r.S();recipe.hash=r.S();Require(p.names.members.count(recipe.member)&&members.insert(recipe.member).second&&DigestText(recipe.hash),"namespace recipe member/hash invalid");auto patches=r.Count();
    for(size_t k=0;k<patches;++k){EiemLoadingGeneration::Patch patch;patch.offset=r.Size();patch.before=r.S();patch.after=r.S();Require(!patch.before.empty()&&patch.before.size()==patch.after.size(),"namespace recipe patch width differs");
     auto member=p.names.members.find(patch.before),bundle=p.names.bundles.find(patch.before);
     auto after=member!=p.names.members.end()?member->second:(bundle!=p.names.bundles.end()?bundle->second:EiemLoadingGeneration::RemapPath(patch.before,p.names));
     Require(after==patch.after&&patch.before!=patch.after,"namespace recipe identity differs");recipe.patches.push_back(std::move(patch));}
    recipes.push_back(std::move(recipe));}p.namespaceRecipes.push_back(std::move(recipes));}
  }else if(feature=="member-rebuilds/source-author-spans/le64/sha256"){
   Require(!p.spanRecipes,"duplicate member recipe fields");p.spanRecipes=true;std::set<std::string> frameSources;auto count=r.Count();
   for(size_t i=0;i<count;++i){EiemResourceRecipe::Frame frame;frame.resource=r.S();frame.hash=r.S();frame.offset=r.Size();frame.size=r.Size();frame.count=r.Size();for(auto& value:frame.stride)value=r.Size();for(auto& value:frame.start)value=r.Size();
    Require(v.package.resources.count(frame.resource)&&v.package.resources.at(frame.resource)->kind==1&&DigestText(frame.hash)&&frame.count&&frame.stride[0]==16&&frame.start[0]==0&&frameSources.insert(frame.resource).second,"member recipe frame identity differs");
    const auto& source=*v.package.resources.at(frame.resource);auto sourceSize=ResourceSize(source);Require(frame.offset<=sourceSize&&frame.size<=sourceSize-frame.offset,"member recipe frame resource extent differs");
    for(size_t s=0;s<4;++s)Require(frame.start[s]<=frame.size&&MultiplySize(frame.count,frame.stride[s])<=frame.size-frame.start[s],"member recipe frame stream extent differs");p.frameRecipes.push_back(std::move(frame));
   }
   count=r.Count();Require(count==p.inputs.size(),"member recipe input coverage differs");for(size_t i=0;i<count;++i){std::vector<EiemResourceRecipe::Member> members;std::set<std::string> names;auto memberCount=r.Count();Require(memberCount,"member recipe input empty");
    for(size_t j=0;j<memberCount;++j){EiemResourceRecipe::Member member;member.name=r.S();member.hash=r.S();member.flags=r.U();member.size=r.Size();Require(p.names.members.count(member.name)&&names.insert(member.name).second&&DigestText(member.hash),"member recipe identity differs");auto spans=r.Count();size_t extent=0;
     for(size_t k=0;k<spans;++k){EiemResourceRecipe::Span span;auto kind=r.U();Require(kind<=EiemResourceRecipe::Literal,"member recipe source kind differs");span.kind=EiemResourceRecipe::Kind(kind);span.source=r.S();span.offset=r.Size();span.size=r.Size();Require(span.size&&span.size<=member.size-extent,"member recipe output extent differs");extent+=span.size;
      if(span.kind==EiemResourceRecipe::Literal){span.literal=r.Blob();Require(span.offset==0&&span.source.empty()&&span.literal.size()==span.size,"member recipe literal differs");}
      else if(span.kind==EiemResourceRecipe::Original)Require(p.names.members.count(span.source),"member recipe original source unknown");
      else if(span.kind==EiemResourceRecipe::ExpandedFrame)Require(frameSources.count(span.source),"member recipe expanded source unknown");
      else{Require(v.package.resources.count(span.source),"member recipe author source unknown");const auto& source=*v.package.resources.at(span.source);auto sourceSize=ResourceSize(source);Require(span.offset<=sourceSize&&span.size<=sourceSize-span.offset,"member recipe author span extent differs");}
      member.spans.push_back(std::move(span));
     }Require(extent==member.size,"member recipe output incomplete");members.push_back(std::move(member));
    }p.memberRecipes.push_back(std::move(members));
   }
  }else Require(false,"unknown rule descriptor fields");
 }
 Require(r.p==bytes.size()&&!p.inputs.empty()&&!p.roots.empty()&&!p.closure.empty()&&p.materialMatches==v.package.materials.size()&&p.textures==v.package.textures.size(),"offline suffix/coverage");
 for(const auto& root:p.roots){Require(closure.count(root.input),"offline root outside closure");for(const auto& marker:root.markers)Require(p.parts.count(marker.first),"offline root marker absent");
 if(v.rootAssembly&&!deferredAssembly){std::map<std::string,unsigned> expected;for(const auto& s:root.Assembly().sources){Require(s.cab==root.cab,"offline foreign source Renderer");for(const auto& t:s.targets)if(!t.marker.empty()){Require(p.parts.count(t.marker),"offline assembly part absent");++expected[t.marker];}}Require(expected==root.markers,"offline assembly/marker coverage differs");}
 }
 Require(PackageDigest(v.package)==p.author&&InputContractDigest(v.package)==p.contract,"offline compiled author digest differs");return v;
}
inline Manifest Decode(const Bytes& bytes,const std::filesystem::path& directory,const std::filesystem::path& mods,const std::filesystem::path& exactRoot={},bool deferredAssembly=false,const Package* verifiedAuthor=nullptr,bool mapResourceBytes=false) {
 Reader header{bytes};auto tag=header.S();
 if(tag=="compressed-rules/xpress-huffman/le64"){auto size=header.Size();auto packed=header.Blob();Require(header.p==bytes.size()&&size&&size<=Bytes{}.max_size(),"compressed rule extent differs");DECOMPRESSOR_HANDLE handle=nullptr;Require(CreateDecompressor(COMPRESS_ALGORITHM_XPRESS_HUFF,nullptr,&handle)!=0,"rule decompressor unavailable");struct Close{DECOMPRESSOR_HANDLE h;~Close(){CloseDecompressor(h);}}close{handle};SIZE_T actual=0;Require(Decompress(handle,packed.data(),packed.size(),nullptr,0,&actual)==0&&GetLastError()==ERROR_INSUFFICIENT_BUFFER&&actual==size,"compressed rule declared size differs");Bytes plain(size);Require(Decompress(handle,packed.data(),packed.size(),plain.data(),plain.size(),&actual)!=0&&actual==size,"rule decompression failed");Reader inner{plain};auto structure=inner.S();Require(structure=="author-references+prefab-renderer-rules+source-ranges/le64/sha256"||structure=="author-references+prefab-renderer-rules+source-ranges+key-label/le64/sha256","compressed rule payload structure differs");return DecodeLayout(plain,directory,mods,-1,exactRoot,deferredAssembly,verifiedAuthor,mapResourceBytes);}
 if(tag=="author-references+prefab-renderer-rules+source-ranges/le64/sha256"||tag=="author-references+prefab-renderer-rules+source-ranges+key-label/le64/sha256"||tag==EiemPackageProtocol::manifestStructure||tag=="author-graph+inputs+roots+closure+parts+materials/le64/sha256"||tag=="EFF_NATIVE_PACK_V35_1"||tag=="EFF_NATIVE_PACK_V36_ASSEMBLY_1"||
    tag=="EFF_NATIVE_PACK_V35_1_MODSET_1"||tag=="EFF_NATIVE_PACK_V36_ASSEMBLY_1_MODSET_1")return DecodeLayout(bytes,directory,mods,-1,exactRoot,deferredAssembly,verifiedAuthor,mapResourceBytes);
 // Unknown release labels may still use an existing field layout. Probe the
 // single/set and marker/address layouts, accepting only a complete valid graph.
 for(int layout=0;layout<4;++layout)try{return DecodeLayout(bytes,directory,mods,layout,exactRoot,deferredAssembly,verifiedAuthor,mapResourceBytes);}catch(const std::runtime_error&){}
 throw std::runtime_error("offline descriptor field structure is unsupported");
}
struct Pointer {std::string key,hash;bool operator==(const Pointer& b)const{return key==b.key&&hash==b.hash;}bool operator!=(const Pointer& b)const{return !(*this==b);}};
inline Pointer ReadCurrent(const std::filesystem::path& cache){auto bytes=Read(cache/"current.tsv");std::istringstream rows(Text(bytes.data(),bytes.size()));std::string header,row,extra;Require(bool(std::getline(rows,header))&&bool(std::getline(rows,row))&&!std::getline(rows,extra),"offline current pointer rows");if(!header.empty()&&header.back()=='\r')header.pop_back();if(!row.empty()&&row.back()=='\r')row.pop_back();auto f=TabFields(row);Require(TableHeader(header,"EFF_RESOURCE_RELOAD_CURRENT")&&f.size()==2&&DigestText(f[0])&&DigestText(f[1]),"offline current pointer format");return {f[0],f[1]};}
inline Manifest ReadManifest(const std::filesystem::path& cache,const Pointer& pointer,const std::filesystem::path& mods){auto directory=cache/"generations"/pointer.key;auto bytes=Read(directory/"manifest.bin");Require(Hash(bytes)==pointer.hash,"offline manifest SHA differs");auto result=Decode(bytes,directory,mods);Require(result.plan.key==pointer.key,"offline directory/key differs");return result;}
inline void Publish(const std::filesystem::path& cache,const Pointer& pointer){Require(DigestText(pointer.key)&&DigestText(pointer.hash),"offline publish identity");std::filesystem::create_directories(cache);auto file=cache/("current.tmp-"+std::to_string(GetCurrentProcessId()));auto text=std::string("EFF_RESOURCE_RELOAD_CURRENT\t1\n")+pointer.key+"\t"+pointer.hash+"\n";Save(file,Bytes(text.begin(),text.end()));Require(MoveFileExW(file.c_str(),(cache/"current.tsv").c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0,"offline atomic publication failed");}
} // namespace EiemOfflinePackage

#pragma once
#include "eiem_resource_reload_plan.h"

// Versioned, immutable output of the disk-only compiler. No Unity calls.
// The loader reads compiled descriptors, never TypeTrees or author INI syntax.
namespace EiemOfflinePackage {
using namespace EiemResourceReloadPlan;
#ifdef EFF_OFFLINE_PACKAGE_V35
constexpr const char* compiler = "EFF_NATIVE_PACK_V35_1";
#else
constexpr const char* compiler = "EFF_NATIVE_PACK_V36_ASSEMBLY_1";
#endif
constexpr size_t manifestLimit = 256ull*1024*1024;
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
 void Expr(const std::shared_ptr<EiemExpression>& v,unsigned depth=0){Require(depth<128,"offline expression depth");B(bool(v));if(!v)return;N(v->op);D(v->number);S(v->variable);Expr(v->left,depth+1);Expr(v->right,depth+1);}
};
struct Reader {
 const Bytes& b;size_t p=0;
 uint64_t N(){auto n=Le(b,p,8);p+=8;return n;}
 size_t Count(size_t maximum=100000){auto n=N();Require(n<=maximum&&n<=b.size()-p,"offline count/extent");return size_t(n);}
 size_t Size(size_t maximum=manifestLimit){auto n=N();Require(n<=maximum,"offline size/extent");return size_t(n);}
 unsigned U(){auto n=N();Require(n<=UINT_MAX,"offline integer width");return unsigned(n);}
 bool B(){auto n=N();Require(n<=1,"offline boolean");return n!=0;}
 double D(){auto bits=N();double n;memcpy(&n,&bits,8);Require(std::isfinite(n),"offline nonfinite number");return n;}
 void Raw(unsigned char* dst,size_t n){Require(p<=b.size()&&n<=b.size()-p,"offline truncated bytes");if(n)memcpy(dst,b.data()+p,n);p+=n;}
 Bytes Blob(){auto n=Count(manifestLimit);Bytes v(n);Raw(v.data(),n);return v;}
 std::string S(){auto n=Count(1024*1024);auto s=Text(b.data()+p,n);p+=n;return s;}
 std::vector<std::string> Strings(){std::vector<std::string> v;auto n=Count();for(size_t i=0;i<n;++i)v.push_back(S());return v;}
 std::set<std::string> Set(){std::set<std::string> v;auto n=Count();for(size_t i=0;i<n;++i)Require(v.insert(S()).second,"offline duplicate set entry");return v;}
 std::map<std::string,std::string> StringsMap(){std::map<std::string,std::string> v;auto n=Count();for(size_t i=0;i<n;++i){auto k=S(),s=S();Require(v.emplace(k,s).second,"offline duplicate map key");}return v;}
 Identity Id(bool optional=false){auto path=S(),asset=S();Require((optional&&path.empty()&&asset.empty())||(path.rfind("assets/",0)==0&&path.find("..") ==path.npos&&!asset.empty()),"offline source identity");return {path,asset};}
 std::shared_ptr<EiemExpression> Expr(unsigned depth=0,size_t* budget=nullptr){size_t local=256;if(!budget)budget=&local;Require(depth<128&&*budget,"offline expression limit");--*budget;if(!B())return {};
  auto v=std::make_shared<EiemExpression>();auto op=U();Require(op<=EiemExpression::Or,"offline expression opcode");v->op=EiemExpression::Op(op);v->number=D();v->variable=S();v->left=Expr(depth+1,budget);v->right=Expr(depth+1,budget);
  bool leaf=op==EiemExpression::Number||op==EiemExpression::Variable,unary=op==EiemExpression::Not||op==EiemExpression::Negative;
  Require(leaf?(!v->left&&!v->right):(unary?(bool(v->left)&&!v->right):(bool(v->left)&&bool(v->right))),"offline expression arity");
  if(op==EiemExpression::Variable)Require(EiemVariableName(v->variable),"offline expression variable");return v;
 }
};
inline void WriteAuthor(Writer& w,const Package& p){
 w.S(p.id);w.S(p.root.filename().u8string());w.Raw(p.iniHash.data(),p.iniHash.size());
 w.N(p.resources.size());for(const auto& row:p.resources){const auto& r=*row.second;w.S(row.first);w.N(r.kind);w.Raw(r.type.data(),16);w.Blob(r.bytes);w.N(r.blocks.size());for(const auto& block:r.blocks){w.S(block.first);w.N(block.second.offset);w.N(block.second.size);w.Raw(block.second.digest.data(),32);}}
 w.N(p.meshes.size());for(const auto& row:p.meshes){w.S(row.first);const auto& v=row.second;w.S(v.resource);w.N(v.vertices);w.N(v.indices);w.Strings(v.bones);w.Strings(v.shapes);}
 w.N(p.materials.size());for(const auto& row:p.materials){w.S(row.first);w.S(row.second.resource);w.Id(row.second.source);w.Strings(row.second.textures);}
 w.N(p.textures.size());for(const auto& row:p.textures){w.S(row.first);const auto& v=row.second;w.S(v.resource);w.Id(v.source);w.N(v.width);w.N(v.height);w.N(v.mips);w.N(v.filter);w.N(v.wrap);w.N(v.aniso);w.B(v.linear);w.D(v.bias);}
 w.N(p.parts.size());for(const auto& row:p.parts){w.S(row.first);w.S(row.second.mesh);w.S(row.second.material);w.Expr(row.second.visible);w.Strings(row.second.shapes);}
 w.N(p.rules.size());for(const auto& row:p.rules){w.S(row.first);w.Strings(row.second.parts);w.B(row.second.skip);w.N(row.second.targets.size());for(const auto& t:row.second.targets){w.N(t.first);w.Id(t.second);}}
 std::map<std::string,double> defaults(p.defaults.begin(),p.defaults.end());w.N(defaults.size());for(const auto& row:defaults){w.S(row.first);w.D(row.second);}w.Set(p.persistent);
 w.N(p.controls.size());for(const auto& row:p.controls){w.S(row.first);w.S(row.second.label);w.D(row.second.minimum);w.D(row.second.maximum);}
 w.N(p.bindings.size());for(const auto& v:p.bindings){w.S(v.id);w.S(v.variable);w.S(v.type);w.N(v.key.vk);w.N(v.key.modifiers);w.D(v.speed);w.N(v.values.size());for(auto n:v.values)w.D(n);}
}
inline Package ReadAuthor(Reader& r,const std::filesystem::path& modRoot){
 Package p;p.id=r.S();auto folder=r.S();Require(Identifier(p.id)&&Identifier(folder)&&folder!="."&&folder!="..", "offline Mod identity/folder");p.root=modRoot/std::filesystem::u8path(folder);r.Raw(p.iniHash.data(),32);
 auto n=r.Count(512);for(size_t i=0;i<n;++i){auto key=r.S();auto v=std::make_shared<Resource>();v->kind=r.U();Require((v->kind==1||v->kind==2||v->kind==4),"offline resource kind");r.Raw(v->type.data(),16);v->bytes=r.Blob();auto blocks=r.Count(256);for(size_t j=0;j<blocks;++j){auto name=r.S();Block b;b.offset=r.Size();b.size=r.Size();r.Raw(b.digest.data(),32);Require(b.offset<=v->bytes.size()&&b.size<=v->bytes.size()-b.offset&&v->blocks.emplace(name,b).second,"offline resource block range/duplicate");}Require(p.resources.emplace(key,v).second,"offline duplicate resource");}
 n=r.Count(512);for(size_t i=0;i<n;++i){auto k=r.S();Mesh v;v.resource=r.S();v.vertices=r.U();v.indices=r.U();v.bones=r.Strings();v.shapes=r.Strings();Require(v.vertices&&p.resources.count(v.resource)&&p.resources.at(v.resource)->kind==1&&p.meshes.emplace(k,std::move(v)).second,"offline Mesh descriptor");}
 n=r.Count(512);for(size_t i=0;i<n;++i){auto k=r.S();EiemDirectory::Material v;v.resource=r.S();v.source=r.Id();v.textures=r.StringsMap();Require(p.resources.count(v.resource)&&p.resources.at(v.resource)->kind==2&&p.materials.emplace(k,std::move(v)).second,"offline Material descriptor");}
 n=r.Count(512);for(size_t i=0;i<n;++i){auto k=r.S();EiemDirectory::Texture v;v.resource=r.S();v.source=r.Id(true);v.width=r.U();v.height=r.U();v.mips=r.U();v.filter=r.U();v.wrap=r.U();v.aniso=r.U();v.linear=r.B();v.bias=r.D();Require(v.width&&v.height&&v.mips&&p.resources.count(v.resource)&&p.resources.at(v.resource)->kind==4&&p.textures.emplace(k,std::move(v)).second,"offline Texture descriptor");}
 n=r.Count(4096);for(size_t i=0;i<n;++i){auto k=r.S();EiemDirectory::Part v;v.mesh=r.S();v.material=r.S();v.visible=r.Expr();v.shapes=r.StringsMap();Require(p.meshes.count(v.mesh)&&p.materials.count(v.material)&&p.parts.emplace(k,std::move(v)).second,"offline Part descriptor");}
 n=r.Count(4096);for(size_t i=0;i<n;++i){auto k=r.S();Rule v;v.parts=r.Strings();v.skip=r.B();auto count=r.Count(32);for(size_t j=0;j<count;++j){auto level=r.U();auto id=r.Id();Require(v.targets.emplace(level,id).second&&p.targetIndex.emplace(id,k).second,"offline duplicate target");}for(const auto& part:v.parts)Require(p.parts.count(part),"offline absent part");Require(!v.targets.empty()&&(v.skip?v.parts.empty():!v.parts.empty())&&p.rules.emplace(k,std::move(v)).second,"offline Rule descriptor");}
 n=r.Count(4096);for(size_t i=0;i<n;++i){auto k=r.S();auto value=r.D();Require(EiemVariableName(k)&&p.defaults.emplace(k,value).second,"offline defaults");}p.persistent=r.Set();for(const auto& k:p.persistent)Require(p.defaults.count(k),"offline persistent variable");
 n=r.Count(4096);for(size_t i=0;i<n;++i){auto k=r.S();Control v;v.label=r.S();v.minimum=r.D();v.maximum=r.D();Require(v.minimum<=v.maximum&&p.defaults.count(k)&&p.controls.emplace(k,v).second,"offline control domain");}
 n=r.Count(4096);std::set<std::string> bindings;for(size_t i=0;i<n;++i){Binding v;v.id=r.S();v.variable=r.S();v.type=r.S();v.key.vk=r.U();v.key.modifiers=r.U();v.speed=r.D();auto count=r.Count(4096);for(size_t j=0;j<count;++j)v.values.push_back(r.D());Require(p.defaults.count(v.variable)&&bindings.insert(v.id).second&&v.key.vk&&v.key.vk<=255&&!(v.key.modifiers&~(MOD_SHIFT|MOD_ALT|MOD_CONTROL))&&!v.values.empty()&&(v.type=="cycle"||v.type=="press"||v.type=="hold")&&v.speed>0,"offline binding descriptor");p.bindings.push_back(std::move(v));}
 for(const auto& part:p.parts){std::string error;Require(!part.second.visible||part.second.visible->Validate(p.defaults,error),"offline undeclared expression variable");for(const auto& shape:part.second.shapes)Require(p.controls.count(shape.second)&&std::find(p.meshes.at(part.second.mesh).shapes.begin(),p.meshes.at(part.second.mesh).shapes.end(),shape.first)!=p.meshes.at(part.second.mesh).shapes.end(),"offline shape binding");}
 for(const auto& m:p.materials)for(const auto& t:m.second.textures)Require(p.textures.count(t.second),"offline absent Texture override");return p;
}
inline void WriteAddress(Writer& w,const EiemRenderAssemblyDescription::Address& a){w.S(a.path);w.N(a.component);}
inline EiemRenderAssemblyDescription::Address ReadAddress(Reader& r){auto path=r.S();auto component=r.Count(1024);return {path,unsigned(component)};}
inline void WriteAssembly(Writer& w,const EiemRenderAssemblyDescription::Description& d){
 w.N(d.sources.size());for(const auto& s:d.sources){w.S(s.cab);w.N(uint64_t(s.id));WriteAddress(w,s.renderer);w.N(s.targets.size());for(const auto& t:s.targets){WriteAddress(w,t.renderer);w.S(t.marker);w.Strings(t.bones);
  w.S(t.root);w.S(t.skin);w.S(t.probe);w.S(t.lighting);w.B(t.hasRoot);w.B(t.hasSkin);w.B(t.hasProbe);w.B(t.hasLighting);w.B(t.extension);}}
 w.N(d.lods.size());for(const auto& l:d.lods){WriteAddress(w,l.group);w.N(l.levels.size());for(const auto& level:l.levels){w.D(level.height);w.D(level.fade);w.N(level.renderers.size());for(const auto& a:level.renderers)WriteAddress(w,a);}}
 w.N(d.helpers.size());for(const auto& h:d.helpers){WriteAddress(w,h.component);w.B(h.custom);w.N(h.renderers.size());for(const auto& a:h.renderers)WriteAddress(w,a);}
}
inline EiemRenderAssemblyDescription::Description ReadAssembly(Reader& r){
 using namespace EiemRenderAssemblyDescription;Description d;std::set<std::pair<std::string,int64_t>> sources;std::set<std::pair<std::string,unsigned>> anchors,targets;
 auto n=r.Count(4096);for(size_t i=0;i<n;++i){EiemRenderAssemblyDescription::Source s;s.cab=r.S();s.id=int64_t(r.N());s.renderer=ReadAddress(r);
  Require(EiemLoadingGeneration::Cab(s.cab)&&s.id&&sources.emplace(s.cab,s.id).second&&anchors.emplace(s.renderer.path,s.renderer.component).second,"offline assembly source identity/anchor");
  auto count=r.Count(4096);for(size_t j=0;j<count;++j){Target t;t.renderer=ReadAddress(r);t.marker=r.S();t.bones=r.Strings();t.root=r.S();t.skin=r.S();t.probe=r.S();t.lighting=r.S();t.hasRoot=r.B();t.hasSkin=r.B();t.hasProbe=r.B();t.hasLighting=r.B();t.extension=r.B();
   Require(targets.emplace(t.renderer.path,t.renderer.component).second&&t.bones.size()<=4096&&(!t.extension||(j>0&&!t.marker.empty()))&&((j==0)==!t.extension)&&(!j?(t.renderer==s.renderer):!(t.renderer==s.renderer)),"offline assembly target ownership");s.targets.push_back(std::move(t));
  }d.sources.push_back(std::move(s));
 }
 n=r.Count(128);for(size_t i=0;i<n;++i){Lod l;l.group=ReadAddress(r);auto count=r.Count(64);for(size_t j=0;j<count;++j){LodLevel level;level.height=float(r.D());level.fade=float(r.D());auto entries=r.Count(4096);for(size_t k=0;k<entries;++k){auto a=ReadAddress(r);Require(targets.count({a.path,a.component}),"offline LOD references unowned Renderer");level.renderers.push_back(std::move(a));}l.levels.push_back(std::move(level));}d.lods.push_back(std::move(l));}
 n=r.Count(4096);for(size_t i=0;i<n;++i){Helper h;h.component=ReadAddress(r);h.custom=r.B();auto count=r.Count(4096);for(size_t j=0;j<count;++j){auto a=ReadAddress(r);Require(targets.count({a.path,a.component}),"offline helper references unowned Renderer");h.renderers.push_back(std::move(a));}d.helpers.push_back(std::move(h));}Require(!d.sources.empty(),"offline assembly missing sources");return d;
}
struct Manifest {Plan plan;Package package;std::string assembly,baseline;};
inline Bytes Encode(const Manifest& v){Writer w;w.S(compiler);w.S(v.assembly);w.S(v.baseline);const auto& p=v.plan;w.S(p.key);w.S(p.author);w.S(p.contract);WriteAuthor(w,v.package);
 w.N(p.inputs.size());for(const auto& in:p.inputs){w.S(in.file);w.S(in.hash);w.S(in.bundle);w.Set(in.externals);}
 w.N(p.roots.size());for(const auto& root:p.roots){w.S(root.path);w.N(root.input);w.N(root.markers.size());for(const auto& m:root.markers){w.S(m.first);w.N(m.second);}
#ifndef EFF_OFFLINE_PACKAGE_V35
 w.S(root.cab);w.N(uint64_t(root.source));WriteAssembly(w,root.assembly);
#endif
 }
 w.N(p.closure.size());for(auto n:p.closure)w.N(n);w.Set(p.shared);w.Strings(p.names.members);w.Strings(p.names.bundles);
 w.N(p.parts.size());for(const auto& row:p.parts){w.S(row.first);const auto& part=row.second;w.S(part.id);w.S(part.marker);w.S(part.material);w.N(part.vertices);w.Strings(part.bones);w.Strings(part.shapes);}
 w.N(p.materials.size());for(const auto& row:p.materials){w.S(row.first);w.S(row.second.name);w.N(row.second.slots.size());for(const auto& slot:row.second.slots){w.S(slot.property);w.B(slot.nonnull);w.B(slot.local);w.S(slot.texture.name);w.N(slot.texture.width);w.N(slot.texture.height);w.N(slot.texture.mips);}}
 for(auto n:{p.meshParts,p.rendererParts,p.textures,p.materialMatches,p.opaque,p.rendererBoundsHits,p.rendererBoundsMisses})w.N(n);
 Require(w.b.size()<=manifestLimit,"offline manifest too large");return w.b;
}
inline Manifest Decode(const Bytes& bytes,const std::filesystem::path& directory,const std::filesystem::path& mods){
 Require(bytes.size()<=manifestLimit,"offline manifest size");Reader r{bytes};Manifest v;Require(r.S()==compiler,"offline compiler/ABI differs");v.assembly=r.S();v.baseline=r.S();auto& p=v.plan;p.directory=directory;p.key=r.S();p.author=r.S();p.contract=r.S();
 Require(DigestText(v.assembly)&&DigestText(v.baseline)&&DigestText(p.key)&&DigestText(p.author)&&DigestText(p.contract),"offline digest syntax");v.package=ReadAuthor(r,mods);
 auto n=r.Count(128);std::set<std::string> files;for(size_t i=0;i<n;++i){Input in;in.file=r.S();in.hash=r.S();in.bundle=r.S();in.externals=r.Set();auto file=std::filesystem::u8path(in.file);Require(file==file.filename()&&file.extension()==".vfs"&&files.insert(in.file).second&&DigestText(in.hash),"offline input path/hash");in.path=directory/file;p.inputs.push_back(std::move(in));}
 n=r.Count(8);std::set<std::string> roots;for(size_t i=0;i<n;++i){Root root;root.path=r.S();root.input=r.Count(127);Require(root.input<p.inputs.size()&&root.path.rfind("assets/",0)==0&&root.path.find("..") ==root.path.npos&&roots.insert(root.path).second,"offline root descriptor");auto count=r.Count(4096);for(size_t j=0;j<count;++j){auto marker=r.S();auto copies=r.U();Require(copies&&root.markers.emplace(marker,copies).second,"offline root markers");}
#ifdef EFF_OFFLINE_PACKAGE_V35
 Require(!root.markers.empty(),"offline empty root coverage");
#else
 root.cab=r.S();root.source=int64_t(r.N());Require(EiemLoadingGeneration::Cab(root.cab)&&root.source,"offline source prefab identity");root.assembly=ReadAssembly(r);
#endif
 p.roots.push_back(std::move(root));}
 n=r.Count(128);std::set<size_t> closure;for(size_t i=0;i<n;++i){auto index=r.Count(127);Require(index<p.inputs.size()&&closure.insert(index).second,"offline closure duplicate/extent");p.closure.push_back(index);}p.shared=r.Set();p.names.members=r.StringsMap();p.names.bundles=r.StringsMap();
 n=r.Count(4096);for(size_t i=0;i<n;++i){auto marker=r.S();EiemResourceReloadPlan::Part part;part.id=r.S();part.marker=r.S();part.material=r.S();part.vertices=r.U();part.bones=r.Strings();part.shapes=r.Strings();Require(marker==part.marker&&part.vertices&&v.package.parts.count(part.id)&&v.package.materials.count(part.material)&&p.parts.emplace(marker,std::move(part)).second,"offline expected Mesh descriptor");}
 n=r.Count(512);for(size_t i=0;i<n;++i){auto key=r.S();EiemResourceReloadPlan::Material material;material.name=r.S();auto count=r.Count(512);std::set<std::string> slots;for(size_t j=0;j<count;++j){TextureSlot slot;slot.property=r.S();slot.nonnull=r.B();slot.local=r.B();slot.texture.name=r.S();slot.texture.width=int(r.U());slot.texture.height=int(r.U());slot.texture.mips=int(r.U());Require(slots.insert(slot.property).second&&(!slot.local||(slot.nonnull&&slot.texture.width>0&&slot.texture.height>0&&slot.texture.mips>0)),"offline Texture slot");material.slots.push_back(std::move(slot));}Require(v.package.materials.count(key)&&p.materials.emplace(key,std::move(material)).second,"offline expected Material");}
 p.meshParts=r.Size(100000);p.rendererParts=r.Size(100000);p.textures=r.Size(100000);p.materialMatches=r.Size(100000);p.opaque=r.Size(100000);p.rendererBoundsHits=r.Size(100000);p.rendererBoundsMisses=r.Size(100000);
 Require(r.p==bytes.size()&&!p.inputs.empty()&&!p.roots.empty()&&!p.closure.empty()&&p.materialMatches==v.package.materials.size()&&p.textures==v.package.textures.size(),"offline suffix/coverage");
 for(const auto& root:p.roots){Require(closure.count(root.input),"offline root outside closure");for(const auto& marker:root.markers)Require(p.parts.count(marker.first),"offline root marker absent");
#ifndef EFF_OFFLINE_PACKAGE_V35
 std::map<std::string,unsigned> expected;for(const auto& s:root.assembly.sources){Require(s.cab==root.cab,"offline foreign source Renderer");for(const auto& t:s.targets)if(!t.marker.empty()){Require(p.parts.count(t.marker),"offline assembly part absent");++expected[t.marker];}}Require(expected==root.markers,"offline assembly/marker coverage differs");
#endif
 }
 Require(PackageDigest(v.package)==p.author&&InputContractDigest(v.package)==p.contract,"offline compiled author digest differs");return v;
}
struct Pointer {std::string key,hash;bool operator==(const Pointer& b)const{return key==b.key&&hash==b.hash;}bool operator!=(const Pointer& b)const{return !(*this==b);}};
inline Pointer ReadCurrent(const std::filesystem::path& cache){auto bytes=Read(cache/"current.tsv",512);std::istringstream rows(Text(bytes.data(),bytes.size()));std::string header,row,extra;Require(bool(std::getline(rows,header))&&bool(std::getline(rows,row))&&!std::getline(rows,extra),"offline current pointer rows");if(!header.empty()&&header.back()=='\r')header.pop_back();if(!row.empty()&&row.back()=='\r')row.pop_back();auto f=TabFields(row);Require(header=="EFF_RESOURCE_RELOAD_CURRENT\t1"&&f.size()==2&&DigestText(f[0])&&DigestText(f[1]),"offline current pointer format");return {f[0],f[1]};}
inline Manifest ReadManifest(const std::filesystem::path& cache,const Pointer& pointer,const std::filesystem::path& mods){auto directory=cache/"generations"/pointer.key;auto bytes=Read(directory/"manifest.bin",manifestLimit);Require(Hash(bytes)==pointer.hash,"offline manifest SHA differs");auto result=Decode(bytes,directory,mods);Require(result.plan.key==pointer.key,"offline directory/key differs");return result;}
inline void Publish(const std::filesystem::path& cache,const Pointer& pointer){Require(DigestText(pointer.key)&&DigestText(pointer.hash),"offline publish identity");std::filesystem::create_directories(cache);auto file=cache/("current.tmp-"+std::to_string(GetCurrentProcessId()));auto text=std::string("EFF_RESOURCE_RELOAD_CURRENT\t1\n")+pointer.key+"\t"+pointer.hash+"\n";Save(file,Bytes(text.begin(),text.end()));Require(MoveFileExW(file.c_str(),(cache/"current.tsv").c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0,"offline atomic publication failed");}
} // namespace EiemOfflinePackage

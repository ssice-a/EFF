#pragma once
#include "eiem_renderer_input_builder.h"

// Stable addresses are relative to the pristine prefab, not Mesh/part names.
// Same-name sibling ordinals are part of the address; injected children never
// change the address of a native node. All work in this file is disk-only.
namespace EiemRenderAssemblyDescription {
using namespace EiemNativeInput;
struct Address {std::string path;unsigned component=0;};
inline bool operator==(const Address& a,const Address& b){return a.path==b.path&&a.component==b.component;}
inline bool operator<(const Address& a,const Address& b){return std::tie(a.path,a.component)<std::tie(b.path,b.component);}
inline std::string Segment(const std::string& name,unsigned ordinal){return std::to_string(name.size())+":"+name+":"+std::to_string(ordinal)+"/";}
struct Target {
 Address renderer;std::string marker;std::vector<std::string> bones;
 std::string root,skin,probe,lighting;bool hasRoot=false,hasSkin=false,hasProbe=false,hasLighting=false;
 bool extension=false;
 std::shared_ptr<const std::vector<std::string>> preparedBones;
 const std::vector<std::string>& Bones()const{return preparedBones?*preparedBones:bones;}
};
struct Source {std::string cab;int64_t id=0;Address renderer;std::vector<Target> targets;};
struct LodLevel {float height=0,fade=0;std::vector<Address> renderers;};
struct Lod {Address group;std::vector<LodLevel> levels;};
struct Helper {Address component;bool custom=false;std::vector<Address> renderers;};
struct Description {std::vector<Source> sources;std::vector<Lod> lods;std::vector<Helper> helpers;};
inline const Value* Optional(const Type& t,const Value& v,const char* name){for(const auto& c:v.children)if(t.nodes.at(c.node).name==name)return &c;return nullptr;}
// One immutable member analysis can serve every Prefab root in that member.
// Root-relative paths and component ordinals remain private to each Graph.
struct GraphData {
 std::string cab;SerializedLayout layout;std::map<int64_t,std::shared_ptr<const Value>> values;
 std::map<int64_t,int64_t> transforms;
 size_t reusedObjects=0,decodedObjects=0,sourceExtent=0;
 // previousBytes is the immutable input used to construct previous, retained
 // by the calling compiler for this slice's lifetime.
 GraphData(const Bytes& bytes,const std::string& c,const GraphData* previous=nullptr,const Bytes* previousBytes=nullptr):cab(c),layout(ParseSerialized(bytes)),sourceExtent(bytes.size()){
  Require(!previous||(previousBytes&&previous->cab==cab&&previousBytes->size()==previous->sourceExtent),"assembly reuse requires matching original member");
  std::vector<bool> sameTypes(layout.types.size(),false);
  if(previous)for(size_t i=0;i<layout.types.size()&&i<previous->layout.types.size();++i){const auto& a=layout.types[i];const auto& b=previous->layout.types[i];
   sameTypes[i]=a.classId==b.classId&&a.hash==b.hash&&a.nodes.size()==b.nodes.size();
   for(size_t j=0;sameTypes[i]&&j<a.nodes.size();++j){const auto& x=a.nodes[j];const auto& y=b.nodes[j];sameTypes[i]=x.type==y.type&&x.name==y.name&&x.align==y.align&&x.children==y.children;}}
  static const std::set<std::string> fields={"m_GameObject","m_Name","m_Component","m_Father","m_Children",
   "m_Mesh","m_Bones","m_RootBone","m_SkinningRoot","m_ProbeAnchor","m_PerRendererLightingAnchor",
   "m_Enabled","m_LODs","useCustomRenderers","customRenderers",
   "m_StaticBatchInfo","m_StaticBatchRoot","m_AdditionalVertexStreams","m_EnlightenVertexStream"};
  for(auto id:layout.order){const auto& type=layout.types.at(layout.objects.at(id).type);int kind=type.classId;
   if(kind!=1&&!IsTransformClass(kind)&&kind!=137&&kind!=23&&kind!=33&&kind!=205&&kind!=114)continue;
   if(kind==114&&std::none_of(type.nodes.at(0).children.begin(),type.nodes.at(0).children.end(),[&](size_t node){return type.nodes.at(node).name=="useCustomRenderers";}))continue;
   const auto& object=layout.objects.at(id);
   if(previous&&sameTypes.at(object.type)){auto old=previous->layout.objects.find(id);auto value=previous->values.find(id);
    if(old!=previous->layout.objects.end()&&value!=previous->values.end()&&old->second.type==object.type&&old->second.size==object.size&&
       std::equal(bytes.begin()+object.offset,bytes.begin()+object.offset+object.size,previousBytes->begin()+old->second.offset)){
     values.emplace(id,value->second);++reusedObjects;continue;}}
   values.emplace(id,std::make_shared<const Value>(ObjectFields(bytes,layout,id,fields)));++decodedObjects;
  }
  for(const auto& row:values)if(IsTransformClass(layout.types.at(layout.objects.at(row.first).type).classId)){
   const auto& type=layout.types.at(layout.objects.at(row.first).type);
   auto owner=NullableReference(type,Child(type,*row.second,"m_GameObject"),layout,cab);
   Require(owner.first==cab&&transforms.emplace(owner.second,row.first).second,"assembly duplicate/nonlocal Transform");
  }
 }
};
class Graph {
 std::shared_ptr<const GraphData> data;const std::string& cab;const SerializedLayout& layout;
 const std::map<int64_t,std::shared_ptr<const Value>>& values;const std::map<int64_t,int64_t>& transforms;
 std::map<int64_t,std::string> paths;std::map<std::pair<int64_t,int>,std::vector<int64_t>> components;
 std::vector<int64_t> renderers;
 std::map<int64_t,std::vector<int64_t>> rendererChildren;
 std::set<int64_t> visiting;int64_t root=0;
 public:
 Graph(const Bytes& b,const std::string& c,int64_t go):Graph(std::make_shared<const GraphData>(b,c),go){}
 Graph(std::shared_ptr<const GraphData> prepared,int64_t go):data(std::move(prepared)),cab(data->cab),layout(data->layout),values(data->values),transforms(data->transforms){
  root=transforms.at(go);paths[root]="";Index(root);
  for(const auto& row:values)if(Kind(row.first)==1&&transforms.count(row.first)&&paths.count(transforms.at(row.first))){
   for(const auto& entry:ConstArray(Schema(row.first),Child(Schema(row.first),*row.second,"m_Component")).children){
    auto ref=NullableReference(Schema(row.first),Child(Schema(row.first),entry,"component"),layout,cab);
    if(ref.first!=cab||!ref.second||!layout.objects.count(ref.second))continue;
    auto id=ref.second;components[{row.first,Kind(id)}].push_back(id);
    if(Kind(id)==23||Kind(id)==137)components[{row.first,0}].push_back(id);
   }
  }
  // Preserve existing SMR ordinals when a native node also has a MeshRenderer.
  for(auto& row:components)if(row.first.second==0)std::stable_partition(row.second.begin(),row.second.end(),[&](int64_t id){return Kind(id)==137;});
  // Preserve SMR-then-static order once; parent lookups must not rescan the
  // entire prefab for each native renderer during assembly validation.
  renderers=Objects(137);auto statics=Objects(23);renderers.insert(renderers.end(),statics.begin(),statics.end());
  for(auto id:renderers){auto tid=transforms.at(Pointer(id,"m_GameObject"));rendererChildren[Pointer(tid,"m_Father")].push_back(id);}
 }
 int Kind(int64_t id)const{return layout.types.at(layout.objects.at(id).type).classId;}
 bool HasSchemaField(int64_t id,const char* name)const{const auto& t=Schema(id);for(auto i:t.nodes.at(0).children)if(t.nodes.at(i).name==name)return true;return false;}
 const Type& Schema(int64_t id)const{return layout.types.at(layout.objects.at(id).type);}
 const Value& Tree(int64_t id)const{return *values.at(id);}
 int64_t Local(const Type& t,const Value& v)const{auto r=NullableReference(t,v,layout,cab);Require(r.first==cab,"assembly nonlocal hierarchy/component");return r.second;}
 int64_t Pointer(int64_t id,const char* field)const{return Local(Schema(id),Child(Schema(id),Tree(id),field));}
 std::string Name(int64_t go)const{const auto& n=Child(Schema(go),Tree(go),"m_Name");return Text(n.raw.data(),n.raw.size());}
 bool Contains(int64_t id)const{return values.count(id)&&paths.count(IsTransformClass(Kind(id))?id:transforms.at(Pointer(id,"m_GameObject")));}
 std::string Path(int64_t transform)const{return paths.at(transform);}
 Address At(int64_t component)const{auto go=Pointer(component,"m_GameObject");auto kind=Kind(component);const auto& list=components.at({go,(kind==23||kind==137)?0:kind});auto it=std::find(list.begin(),list.end(),component);Require(it!=list.end(),"assembly component absent from owner");return {Path(transforms.at(go)),unsigned(it-list.begin())};}
 Address HelperAt(int64_t component)const{auto a=At(component);a.component=0;auto go=Pointer(component,"m_GameObject");for(auto id:components.at({go,114})){if(id==component)return a;if(HasSchemaField(id,"useCustomRenderers"))++a.component;}throw std::runtime_error("assembly helper component absent");}
 std::vector<int64_t> Objects(int kind)const{std::vector<int64_t> result;for(const auto& row:values)if(Kind(row.first)==kind&&Contains(row.first))result.push_back(row.first);return result;}
 const std::vector<int64_t>& Renderers()const{return renderers;}
 const std::vector<int64_t>& RendererChildren(int64_t anchor)const{
  auto tid=transforms.at(Pointer(anchor,"m_GameObject"));auto found=rendererChildren.find(tid);
  static const std::vector<int64_t> empty;return found==rendererChildren.end()?empty:found->second;
 }
 Target ReadTarget(int64_t renderer,bool extension)const{
  Target t;t.renderer=At(renderer);t.extension=extension;const auto& schema=Schema(renderer);const auto& tree=Tree(renderer);
  if(Kind(renderer)==137)for(const auto& b:ConstArray(schema,Child(schema,tree,"m_Bones")).children)t.bones.push_back(Path(Local(schema,b)));
  auto pointer=[&](const char* field,std::string& path,bool& present){if(!HasSchemaField(renderer,field))return;auto id=Pointer(renderer,field);present=id!=0;if(present)path=Path(id);};
  if(Kind(renderer)==137){pointer("m_RootBone",t.root,t.hasRoot);pointer("m_SkinningRoot",t.skin,t.hasSkin);}
  pointer("m_ProbeAnchor",t.probe,t.hasProbe);pointer("m_PerRendererLightingAnchor",t.lighting,t.hasLighting);return t;
 }
 bool Enabled(int64_t renderer)const{return Integer(Child(Schema(renderer),Tree(renderer),"m_Enabled"))!=0;}
 void RequirePure(int64_t renderer)const{auto go=Pointer(renderer,"m_GameObject"),tid=transforms.at(go);const auto& componentsList=ConstArray(Schema(go),Child(Schema(go),Tree(go),"m_Component"));
  Require(componentsList.children.size()==(Kind(renderer)==23?3:2)&&ConstArray(Schema(tid),Child(Schema(tid),Tree(tid),"m_Children")).children.empty(),"assembly extension contains native scripts/bone children");
 }
 NativeReference Mesh(int64_t renderer)const{if(Kind(renderer)==23){auto it=components.find({Pointer(renderer,"m_GameObject"),33});if(it==components.end())return {cab,0};Require(it->second.size()==1,"assembly static Renderer has multiple MeshFilters");renderer=it->second.front();}return NullableReference(Schema(renderer),Child(Schema(renderer),Tree(renderer),"m_Mesh"),layout,cab);}
 private:
 void Index(int64_t tid){Require(visiting.insert(tid).second,"assembly hierarchy cycle/depth");
  const auto& schema=Schema(tid);std::map<std::string,unsigned> ordinal;
  for(const auto& entry:ConstArray(schema,Child(schema,Tree(tid),"m_Children")).children){auto child=Local(schema,entry);Require(Pointer(child,"m_Father")==tid,"assembly child/parent differs");auto name=Name(Pointer(child,"m_GameObject"));Require(paths.emplace(child,Path(tid)+Segment(name,ordinal[name]++)).second,"assembly repeated hierarchy child");Index(child);}
  visiting.erase(tid);
 }
};
inline Description Build(const Graph& before,const Graph& after,const std::string& cab,
                         const Package& package,const AssetIndex& assets,const std::map<NativeReference,std::string>& markers){
 Description d;std::set<int64_t> native,claimed;
 for(auto id:before.Renderers())native.insert(id);
 for(auto id:native){Source source{cab,id,before.At(id),{}};Require(source.renderer==after.At(id),"assembly native anchor address changed");
 auto ref=before.Mesh(id);bool skip=false;const Rule* rule=nullptr;
  for(const auto& selected:package.targetIndex){auto a=assets.find(selected.first);if(a!=assets.end()&&ref==NativeReference{a->second.cab,a->second.pathId}){rule=&package.rules.at(selected.second);skip=rule->skip;break;}}
  if(!skip){auto target=after.ReadTarget(id,false);auto marker=markers.find(after.Mesh(id));if(marker!=markers.end())target.marker=marker->second;source.targets.push_back(std::move(target));}
  if(rule&&!skip)for(size_t i=1;i<rule->parts.size();++i){const auto& children=after.RendererChildren(id);auto extra=RendererObjectId(cab,package,"renderer",id,rule->parts[i]);
   Require(std::find(children.begin(),children.end(),extra)!=children.end()&&!native.count(extra)&&claimed.insert(extra).second,"assembly extension source ownership differs");
   after.RequirePure(extra);
   auto target=after.ReadTarget(extra,true);auto marker=markers.find(after.Mesh(extra));Require(marker!=markers.end(),"assembly unowned extra Renderer");target.marker=marker->second;
   // The compiler creates pure visual nodes only; native bones/scripts survive.
   Require(after.Name(after.Pointer(extra,"m_GameObject")).find("__EFF_"+package.id+"_")!=std::string::npos,"assembly extension provenance absent");source.targets.push_back(std::move(target));
  }d.sources.push_back(std::move(source));
 }
 for(auto id:after.Renderers())Require(native.count(id)||claimed.count(id),"assembly unowned extra Renderer");
 for(auto id:after.Objects(205)){Lod lod;lod.group=after.At(id);const auto& t=after.Schema(id);
  for(const auto& level:ConstArray(t,Child(t,after.Tree(id),"m_LODs")).children){LodLevel l;auto height=Optional(t,level,"screenRelativeHeight");if(!height)height=Optional(t,level,"screenRelativeTransitionHeight");Require(height,"assembly LOD height field absent");l.height=float(FloatValue(t,*height));
   if(auto fade=Optional(t,level,"fadeTransitionWidth"))l.fade=float(FloatValue(t,*fade));
   for(const auto& item:ConstArray(t,Child(t,level,"renderers")).children){auto rid=after.Local(t,Child(t,item,"renderer"));Require(after.Kind(rid)==137||after.Kind(rid)==23,"assembly LOD Renderer type unsupported");l.renderers.push_back(after.At(rid));}lod.levels.push_back(std::move(l));
  }d.lods.push_back(std::move(lod));
 }
 for(auto id:after.Objects(114))if(after.HasSchemaField(id,"useCustomRenderers")){Helper h;h.component=after.HelperAt(id);const auto& t=after.Schema(id);h.custom=Integer(Child(t,after.Tree(id),"useCustomRenderers"))!=0;
  for(const auto& ptr:ConstArray(t,Child(t,after.Tree(id),"customRenderers")).children){auto rid=after.Local(t,ptr);if(rid)h.renderers.push_back(after.At(rid));}d.helpers.push_back(std::move(h));
 }return d;
}
inline Description Build(const Bytes& pristine,const Bytes& candidate,const std::string& cab,int64_t root,
                         const Package& package,const AssetIndex& assets,const std::map<NativeReference,std::string>& markers){
 Graph before(pristine,cab,root);
 if(&pristine==&candidate)return Build(before,before,cab,package,assets,markers);
 Graph after(candidate,cab,root);return Build(before,after,cab,package,assets,markers);
}
} // namespace EiemRenderAssemblyDescription

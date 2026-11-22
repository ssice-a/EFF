#pragma once
#include "eiem_physics_component_identity.h"
#include "eiem_renderer_input_builder.h"
#include "eiem_author_bone_animation.h"
#include <optional>

// Assemble authored bones and cloth as native serialized input. Both the author
// compiler and demand loader use this before the game creates any components.
namespace EiemPhysicsInput {
using namespace EiemNativeInput;
constexpr const char* structure="EFF_NEW_BONE_CLOTH/path+trs+mesh-consumers+native-templates/le64";
struct Reader {
 const Bytes& b;size_t p=0;
 uint64_t N(){auto n=Le(b,p,8);p+=8;return n;}
 size_t Count(){auto n=N();Require(p<=b.size()&&n<=(b.size()-p)/8,"Physics count exceeds encoded extent");return size_t(n);}
 bool B(){auto n=N();Require(n<=1,"Physics boolean differs");return n!=0;}
 std::string S(){auto n=N();Require(n<=b.size()-p,"Physics string truncated");auto s=Text(b.data()+p,size_t(n));p+=size_t(n);return s;}
 Bytes Blob(){auto n=N();Require(n<=b.size()-p,"Physics template truncated");Bytes v(b.begin()+p,b.begin()+p+size_t(n));p+=size_t(n);return v;}
 double D(){auto bits=N();double v;memcpy(&v,&bits,8);Require(std::isfinite(v),"Physics nonfinite transform");return v;}
};
struct Node {std::string path,parent;bool source=false,hasParent=false;std::array<double,3> position,scale;std::array<double,4> rotation;};
struct Template {std::string cab;Bytes bytes;SerializedLayout layout;AuthorByteSpans origins;};
struct Collider {std::string id,name,bone,cab;int64_t object=0;Vector3 position;std::array<double,4> rotation;};
struct Group {std::string id,name,cab;int64_t object=0;std::set<Identity> targets;std::vector<std::pair<std::string,bool>> nodes;std::vector<std::string> colliders;};
struct Document {std::string id;std::map<std::string,Node> nodes;std::map<std::string,Template> templates;std::map<std::string,Collider> colliders;std::vector<Group> groups;};
struct SkeletonUse {
 std::string id,part;
 std::shared_ptr<const EiemAuthorBoneAnimation::Skeleton> skeleton;
 const std::vector<std::string>* palette;
};
using SkeletonUses=std::map<int64_t,std::vector<SkeletonUse>>;
inline Document DecodePayload(std::shared_ptr<const Bytes> source,const std::string& resourceName={}){
 Require(bool(source),"Physics input payload is absent");
 const auto& bytes=*source;Reader r{bytes};Document d;
 Require(r.S()==structure,"new Physics input structure differs");d.id=r.S();Require(!d.id.empty(),"Physics identity absent");
 auto n=r.Count();for(size_t i=0;i<n;++i){Node v;v.path=r.S();v.parent=r.S();v.source=r.B();v.hasParent=r.B();for(auto& x:v.position)x=r.D();for(auto& x:v.rotation)x=r.D();for(auto& x:v.scale)x=r.D();Require(d.nodes.emplace(v.path,v).second,"duplicate Physics bone path");}
 for(const auto& node:d.nodes){const auto& v=node.second;Require(!v.hasParent||d.nodes.count(v.parent),"Physics bone parent absent");Require(v.source||v.hasParent,"new Physics bone lacks parent");}
 n=r.Count();for(size_t i=0;i<n;++i){Template v;v.cab=r.S();auto blobOffset=AddSize(r.p,8);v.bytes=r.Blob();v.layout=ParseSerialized(v.bytes);if(!resourceName.empty())v.origins=EiemAuthorBytes::MakeRange(resourceName,source,blobOffset,v.bytes.size());auto cab=v.cab;Require(d.templates.emplace(cab,std::move(v)).second,"duplicate Physics template CAB");}
 n=r.Count();for(size_t i=0;i<n;++i){Collider v;v.id=r.S();v.name=r.S();v.bone=r.S();v.cab=r.S();v.object=int64_t(r.N());for(auto& x:v.position)x=r.D();for(auto& x:v.rotation)x=r.D();Require(d.nodes.count(v.bone)&&d.templates.count(v.cab)&&d.templates.at(v.cab).layout.objects.count(v.object),"Physics collider dependency absent");Require(d.colliders.emplace(v.id,v).second,"duplicate Physics collider");}
 n=r.Count();for(size_t i=0;i<n;++i){Group g;g.id=r.S();g.name=r.S();g.cab=r.S();g.object=int64_t(r.N());auto count=r.Count();for(size_t j=0;j<count;++j){auto path=r.S(),asset=r.S();Require(g.targets.insert({path,asset}).second,"duplicate Physics Mesh target");}
  count=r.Count();for(size_t j=0;j<count;++j){auto path=r.S();auto move=r.B();Require(d.nodes.count(path),"Physics group bone absent");g.nodes.emplace_back(path,move);}
  count=r.Count();for(size_t j=0;j<count;++j){auto id=r.S();Require(d.colliders.count(id),"Physics collider binding absent");g.colliders.push_back(id);}
  Require(!g.targets.empty()&&g.nodes.size()>1&&d.templates.count(g.cab)&&d.templates.at(g.cab).layout.objects.count(g.object),"incomplete new Physics group");d.groups.push_back(std::move(g));
 }
 Require(r.p==bytes.size(),"Physics input trailing bytes");return d;
}
inline Document DecodePayload(const Bytes& bytes,const std::string& resourceName={}){
 return DecodePayload(std::make_shared<const Bytes>(bytes),resourceName);
}
inline Document Decode(const Resource& resource,const std::string& resourceName={}){
 const auto& block=resource.blocks.at("@physics");
 auto bytes=std::make_shared<Bytes>(resource.bytes.begin()+block.offset,resource.bytes.begin()+block.offset+block.size);
 return DecodePayload(bytes,resourceName);
}
inline std::vector<Document> Documents(const Package& package){std::vector<Document> result;for(const auto& row:package.resources)if(row.second->blocks.count("@physics"))result.push_back(Decode(*row.second,row.first));return result;}
inline Value& FieldPath(const Type& t,Value& value,const std::vector<std::string>& path){auto* v=&value;for(const auto& key:path)v=&Child(t,*v,key);return *v;}

class MemberBuilder {
 const Bytes& bytes;std::string cab;const Package& package;const AssetIndex& assets;const std::vector<Document>& documents;
 const std::set<Identity>* relevantTargets;
 const SkeletonUses* skeletonUses=nullptr;
 const std::vector<MeshConsumer>* retargetConsumers=nullptr;
 const std::vector<BuiltMeshPart>* retargetParts=nullptr;
 AuthorBoneSlots authorBoneSlots;
 std::set<int64_t> nativePaletteBones;
 struct RetargetScope {
  int64_t renderer=0;
  std::set<size_t> visited;
  EiemAuthorBoneAnimation::RetargetRoutes routes;
 };
 std::map<std::pair<int64_t,std::string>,RetargetScope> retargetScopes;
 const AuthorByteSpans* sourceOrigins=nullptr;
 SerializedLayout layout;std::map<int64_t,Value> values;std::map<int64_t,size_t> typeIds;std::set<int64_t> changed;
 std::map<int64_t,int64_t> owners,transforms,parents;std::map<int64_t,std::string> paths;std::map<int64_t,std::map<std::string,int64_t>> hierarchy;
 std::vector<AddedObject> added;std::vector<Bytes> types,scripts;std::map<std::pair<std::string,size_t>,size_t> imported;
 std::set<int64_t> movingBones;
 std::map<int64_t,std::vector<NativeReference>> preloads;
 using Project=std::function<Value(int64_t,const std::set<std::string>&)>;
 Project project;
 std::function<Value(int64_t)> decode;
 std::map<int64_t,Bytes> encodedChanges;
 bool built=false;
 static const Bytes& EmptySource(){static const Bytes value;return value;}
 const Type& Schema(int64_t id)const{return layout.types.at(typeIds.at(id));}
 Value& Tree(int64_t id){auto f=values.find(id);if(f==values.end())f=values.emplace(id,decode?decode(id):ObjectValue(bytes,layout,id,sourceOrigins)).first;return f->second;}
 int64_t Pointer(int64_t id,const char* name){auto ref=NullableReference(Schema(id),Child(Schema(id),Tree(id),name),layout,cab);Require(ref.first==cab,"Physics hierarchy must be local");return ref.second;}
 int64_t Allocate(int64_t anchor,const std::string& identity,const char* kind){auto id=RendererObjectId(cab,package,kind,anchor,identity);Require(id&&!typeIds.count(id),"Physics PathID collision");return id;}
 int64_t Add(int64_t id,size_t type,Value value){Require(typeIds.emplace(id,type).second,"Physics object identity collision");values.emplace(id,std::move(value));added.push_back({id,type,{}});return id;}
 void Reference(int64_t id,const char* field,int64_t target){SetReference(Schema(id),Child(Schema(id),Tree(id),field),{cab,target},layout,cab);}
 void Append(int64_t id,const char* field,int64_t target,const char* inner=nullptr){auto& list=ArrayValue(Schema(id),Child(Schema(id),Tree(id),field));auto value=ArrayElement(Schema(id),list);SetReference(Schema(id),inner?Child(Schema(id),value,inner):value,{cab,target},layout,cab);list.children.push_back(std::move(value));list.count=list.children.size();if(layout.objects.count(id))changed.insert(id);}
 void Hierarchy(){
  for(auto id:layout.order)if(Schema(id).classId==4){auto go=Pointer(id,"m_GameObject");Require(transforms.emplace(go,id).second,"Physics multiple GameObject Transforms");owners[id]=go;parents[id]=Pointer(id,"m_Father");}
  std::set<int64_t> visiting;std::function<int64_t(int64_t)> visit=[&](int64_t id){if(paths.count(id)){auto root=id;while(parents.at(root))root=parents.at(root);return root;}Require(visiting.insert(id).second,"Physics hierarchy cycle");auto parent=parents.at(id);int64_t root=id;
   if(parent){root=visit(parent);const auto& name=Child(Schema(owners.at(id)),Tree(owners.at(id)),"m_Name");auto text=Text(name.raw.data(),name.raw.size());Require(!text.empty()&&text.find('/')==text.npos,"Physics ambiguous bone segment");paths[id]=paths[parent].empty()?text:paths[parent]+"/"+text;}else paths[id]="";
   auto found=hierarchy[root].emplace(paths[id],id);if(!found.second)found.first->second=0;visiting.erase(id);return root;};
  for(const auto& row:parents)visit(row.first);
 }
 int64_t Root(int64_t id)const{while(parents.at(id))id=parents.at(id);return id;}
 std::optional<int64_t> Lookup(int64_t anchor,const std::string& path)const{
  const auto& index=hierarchy.at(Root(anchor));auto full=paths.at(anchor);
  if(!path.empty())full=full.empty()?path:full+"/"+path;
  auto found=index.find(full);if(found==index.end())return std::nullopt;
  return found->second;
 }
 int64_t Resolve(int64_t anchor,const std::string& path)const{return Lookup(anchor,path).value_or(0);}
 using ColliderMounts=std::map<std::string,std::string>;
 static ColliderMounts OptionalColliderMounts(const Document& d){
  ColliderMounts mounts;
  for(const auto& row:d.colliders){
   const auto& node=d.nodes.at(row.second.bone);
   if(node.source&&node.hasParent&&d.nodes.at(node.parent).source)mounts.emplace(node.path,node.parent);
  }
  for(const auto& row:d.nodes)if(row.second.hasParent)mounts.erase(row.second.parent);
  for(const auto& group:d.groups)for(const auto& node:group.nodes)mounts.erase(node.first);
  return mounts;
 }
 int64_t Anchor(int64_t renderer,const Document& d,const std::set<std::string>& required,const std::string& context,
                const ColliderMounts& mounts={}){
  Require(Schema(renderer).classId==137,"bone cloth cannot bind a static MeshRenderer");auto rootBone=Pointer(renderer,"m_RootBone"),tid=transforms.at(Pointer(renderer,"m_GameObject"));std::set<int64_t> ownerAncestors;
  for(auto id=tid;id;id=parents.at(id))ownerAncestors.insert(id);
  if(required.empty()){
   for(auto id=rootBone;id;id=parents.at(id))if(ownerAncestors.count(id))return id;
   throw std::runtime_error("author Skeleton Renderer root differs");
  }
  std::vector<int64_t> candidates;size_t bestMatches=0,bestFallbacks=SIZE_MAX;std::string firstMissing;
  for(auto id=rootBone;id;id=parents.at(id)){
   if(!ownerAncestors.count(id))continue;
   size_t matches=0,fallbacks=0;std::string missing;
   for(const auto& path:required){
    auto found=Lookup(id,path);
    if(found&&*found){++matches;continue;}
    auto parent=mounts.find(path);
    if(!found&&parent!=mounts.end()&&Resolve(id,parent->second)){++matches;++fallbacks;continue;}
    if(missing.empty())missing=path;
   }
   if(matches>bestMatches||firstMissing.empty()){bestMatches=matches;firstMissing=missing;}
   if(matches!=required.size())continue;
   if(!std::any_of(d.nodes.begin(),d.nodes.end(),[&](const auto& node){
      return node.second.source&&Resolve(id,node.first)==rootBone;
   }))continue;
   if(fallbacks<bestFallbacks){candidates.clear();bestFallbacks=fallbacks;}
   if(fallbacks==bestFallbacks)candidates.push_back(id);
  }
  Require(candidates.size()==1,"no unique local skeleton anchor: CAB="+cab+" "+context+
    " owner="+paths.at(tid)+" rootBone="+(rootBone?paths.at(rootBone):"<none>")+
    " candidates="+std::to_string(candidates.size())+" matched="+std::to_string(bestMatches)+"/"+
    std::to_string(required.size())+" firstMissing="+firstMissing);
  return candidates.front();
 }
 int64_t Anchor(int64_t renderer,const Document& d,const Group& group,const ColliderMounts& mounts){
  std::set<std::string> required;
  auto requirePath=[&](std::string path){for(;;){const auto& node=d.nodes.at(path);if(node.source){required.insert(path);break;}path=node.parent;}};
  for(const auto& node:group.nodes)requirePath(node.first);for(const auto& id:group.colliders)requirePath(d.colliders.at(id).bone);
  return Anchor(renderer,d,required,"Physics group="+group.name,mounts);
 }
 void Rotation(int64_t transform,const std::array<double,4>& rotation){auto& v=Child(Schema(transform),Tree(transform),"m_LocalRotation");for(size_t i=0;i<4;++i)SetFloat(Schema(transform),Child(Schema(transform),v,std::string(1,"xyzw"[i])),rotation[i]);}
 std::pair<int64_t,int64_t> NewObject(int64_t anchor,int64_t parent,const std::string& identity,const std::string& name,const Vector3& position,const std::array<double,4>& rotation,const Vector3& scale){
  auto transform=Allocate(anchor,identity,"physics-transform"),go=Allocate(anchor,identity,"physics-owner");auto donorGo=owners.at(parent);
  auto g=Tree(donorGo),t=Tree(parent);const auto gt=typeIds.at(donorGo),tt=typeIds.at(parent);
  SetString(layout.types.at(gt),Child(layout.types.at(gt),g,"m_Name"),name);auto& components=ArrayValue(layout.types.at(gt),Child(layout.types.at(gt),g,"m_Component"));components.children.clear();components.count=0;
  auto& children=ArrayValue(layout.types.at(tt),Child(layout.types.at(tt),t,"m_Children"));children.children.clear();children.count=0;
  Add(go,gt,std::move(g));Add(transform,tt,std::move(t));Reference(transform,"m_GameObject",go);Reference(transform,"m_Father",parent);Append(go,"m_Component",transform,"component");Append(parent,"m_Children",transform);
  SetVector(Schema(transform),Child(Schema(transform),Tree(transform),"m_LocalPosition"),position);SetVector(Schema(transform),Child(Schema(transform),Tree(transform),"m_LocalScale"),scale);Rotation(transform,rotation);
  transforms[go]=transform;owners[transform]=go;parents[transform]=parent;paths[transform]=paths.at(parent).empty()?name:paths.at(parent)+"/"+name;auto f=hierarchy[Root(parent)].emplace(paths[transform],transform);Require(f.second,"new Physics bone/object path collides with original");return {go,transform};
 }
 int64_t Bone(int64_t anchor,const Document& d,const std::string& path){const auto& node=d.nodes.at(path);auto existing=Resolve(anchor,path);if(existing){Require(node.source||!layout.objects.count(existing),"new Physics bone already exists in source");if(!node.source){auto p=ReadVector(Schema(existing),Child(Schema(existing),Tree(existing),"m_LocalPosition")),s=ReadVector(Schema(existing),Child(Schema(existing),Tree(existing),"m_LocalScale"));for(size_t i=0;i<3;++i)Require(std::abs(p[i]-node.position[i])<1e-6&&std::abs(s[i]-node.scale[i])<1e-6,"shared new Physics bone TRS differs");Require(parents.at(existing)==Resolve(anchor,node.parent),"shared new Physics bone parent differs");auto& rotation=Child(Schema(existing),Tree(existing),"m_LocalRotation");for(size_t i=0;i<4;++i)Require(std::abs(FloatValue(Schema(existing),Child(Schema(existing),rotation,std::string(1,"xyzw"[i])))-node.rotation[i])<1e-6,"shared new Physics bone rotation differs");}return existing;}Require(!node.source,"source Physics bone absent: "+path);
  auto parent=node.hasParent?Bone(anchor,d,node.parent):anchor;return NewObject(anchor,parent,d.id+":bone:"+path,path.substr(path.find_last_of('/')+1),node.position,node.rotation,node.scale).second;
 }
 int64_t ColliderParent(int64_t anchor,const Document& d,const Collider& collider,
                        const ColliderMounts& mounts,std::map<std::string,int64_t>& created){
  auto mount=mounts.find(collider.bone);
  if(mount==mounts.end()||Lookup(anchor,collider.bone))return Bone(anchor,d,collider.bone);
  auto found=created.find(collider.bone);if(found!=created.end())return found->second;
  // A prefab may omit a native collider's leaf Transform. Preserve its local
  // TRS without exposing the proxy as a source bone or copying its component.
  const auto& node=d.nodes.at(collider.bone);
  auto parent=Bone(anchor,d,mount->second);
  auto identity=d.id+":collider-mount:"+node.path;
  auto name="__EFF_ColliderMount_"+std::to_string(RendererObjectId(cab,package,"physics-transform",anchor,identity));
  auto transform=NewObject(anchor,parent,identity,name,node.position,node.rotation,node.scale).second;
  return created.emplace(node.path,transform).first->second;
 }
 void CollectNativePalettes(){
  if(!retargetConsumers||!retargetParts||!skeletonUses)return;
  bool needed=false;
  for(const auto& row:*skeletonUses)for(const auto& use:row.second)needed|=!use.skeleton->retargets.empty();
  if(!needed)return;
  std::map<NativeReference,size_t> counts;
  for(const auto& part:*retargetParts)if(!part.sourceBoneHashes.empty())
   counts.emplace(CanonicalSerializedAssetReference({part.cab,part.sourceId}),part.sourceBoneHashes.size());
  for(const auto& consumer:*retargetConsumers){
   if(consumer.filter)continue;
   auto count=counts.find(CanonicalSerializedAssetReference(consumer.mesh));
   if(count==counts.end())continue;
   const auto& t=Schema(consumer.renderer);
   const auto& palette=ConstArray(t,Child(t,Tree(consumer.renderer),"m_Bones")).children;
   if(palette.size()!=count->second)continue;
   auto rendererRoot=Pointer(consumer.renderer,"m_RootBone");
   if(!rendererRoot||!parents.count(rendererRoot))continue;
   for(const auto& slot:palette){
    auto bone=NullableReference(t,slot,layout,cab);
    if(bone.first==cab&&parents.count(bone.second)&&Root(bone.second)==Root(rendererRoot))
     nativePaletteBones.insert(bone.second);
   }
  }
 }
 bool SameBindPose(int64_t transform,const EiemAuthorBoneAnimation::Node& node){
  const auto& type=Schema(transform);const auto& value=Tree(transform);
  const auto position=ReadVector(type,Child(type,value,"m_LocalPosition"));
  const auto scale=ReadVector(type,Child(type,value,"m_LocalScale"));
  for(size_t i=0;i<position.size();++i)
   if(std::abs(node.position[i]-position[i])>1e-5||std::abs(node.scale[i]-scale[i])>1e-5)return false;
  const auto& rotation=Child(type,value,"m_LocalRotation");double dot=0;
  for(size_t i=0;i<node.rotation.size();++i)
   dot+=node.rotation[i]*FloatValue(type,Child(type,rotation,std::string(1,"xyzw"[i])));
  return std::abs(std::abs(dot)-1)<1e-5;
 }
 std::optional<EiemAuthorBoneAnimation::LocalBoneRoute> NativeRoute(int64_t owner,int64_t target,
     const EiemAuthorBoneAnimation::Skeleton& skeleton,size_t index,const EiemAuthorBoneAnimation::RotationRetarget& mapping){
  auto ancestor=target;
  for(auto at=index;at!=mapping.ancestor;at=size_t(skeleton.nodes.at(at).parent))ancestor=parents.at(ancestor);
  std::vector<size_t> sourceChain;
  for(auto at=mapping.source;at!=mapping.ancestor;at=size_t(skeleton.nodes.at(at).parent))sourceChain.push_back(at);
  // The authored bind pose identifies the same native palette reference even
  // if its GameObject has a different display name in this prefab variant.
  auto source=ancestor;
  for(auto at=sourceChain.rbegin();at!=sourceChain.rend();++at){
   const auto& type=Schema(source);int64_t found=0;
   for(const auto& child:ConstArray(type,Child(type,Tree(source),"m_Children")).children){
    auto ref=NullableReference(type,child,layout,cab);
    if(ref.first!=cab||!layout.objects.count(ref.second)||!parents.count(ref.second)||parents.at(ref.second)!=source)continue;
    if(at+1==sourceChain.rend()&&!nativePaletteBones.count(ref.second))continue;
    if(!SameBindPose(ref.second,skeleton.nodes.at(*at)))continue;
    if(found)return std::nullopt;
    found=ref.second;
   }
   if(!found)return std::nullopt;
   source=found;
  }
  if(!nativePaletteBones.count(source))return std::nullopt;
  EiemAuthorBoneAnimation::LocalBoneRoute route;
  for(auto at=source;at!=owner;){
   auto parent=parents.at(at);Require(parent,"retarget source outside its local skeleton");
   const auto& type=Schema(parent);const auto& children=ConstArray(type,Child(type,Tree(parent),"m_Children")).children;
   auto child=std::find_if(children.begin(),children.end(),[&](const Value& value){return NullableReference(type,value,layout,cab)==NativeReference{cab,at};});
   Require(child!=children.end()&&child-children.begin()<=INT_MAX,"retarget source child ordinal absent");
   route.push_back(int(child-children.begin()));at=parent;
  }
  std::reverse(route.begin(),route.end());return route;
 }
 void PublishRetargetRoutes(){
  for(const auto& [key,scope]:retargetScopes){
   if(scope.routes.empty())continue;
   const auto& [anchor,id]=key;
   const auto prefix=EiemAuthorBoneAnimation::RetargetRoutePrefix(id);
   auto owner=NewObject(anchor,anchor,"retarget-routes:"+id,
       EiemAuthorBoneAnimation::EncodeRetargetRoutes(prefix,scope.routes),{0,0,0},{0,0,0,1},{1,1,1});
   SetInteger(Schema(owner.first),Child(Schema(owner.first),Tree(owner.first),"m_IsActive"),0);
   auto& dependencies=preloads[scope.renderer];dependencies.emplace_back(cab,owner.first);dependencies.emplace_back(cab,owner.second);
  }
 }
 void PopulateSkeleton(int64_t renderer,const SkeletonUse& use){
  const auto& skeleton=*use.skeleton;
  const auto used=EiemAuthorBoneAnimation::AuthorBoneClosure(skeleton,{use.palette->begin(),use.palette->end()});
  if(std::none_of(used.begin(),used.end(),[](bool value){return value;}))return;
  Document document;document.id=use.id;
  std::set<std::string> required;
  for(size_t i=0;i<skeleton.nodes.size();++i){
   const auto& source=skeleton.nodes[i];Node node;
   node.path=source.path;node.source=source.source;node.hasParent=source.parent>=0;
   if(node.hasParent)node.parent=skeleton.nodes[size_t(source.parent)].path;
   std::copy(source.position.begin(),source.position.end(),node.position.begin());
   std::copy(source.rotation.begin(),source.rotation.end(),node.rotation.begin());
   std::copy(source.scale.begin(),source.scale.end(),node.scale.begin());
   if(used[i]&&node.hasParent&&skeleton.nodes[size_t(source.parent)].source)required.insert(node.parent);
   document.nodes.emplace(node.path,std::move(node));
  }
  const auto anchor=Anchor(renderer,document,required,"Skeleton="+use.id);
  for(size_t i=0;i<used.size();++i)if(used[i]){
   auto bone=Bone(anchor,document,skeleton.nodes[i].path);
   Require(!skeleton.retargets.count(i)||!movingBones.count(bone),
           "author Physics and retarget both drive bone: "+skeleton.nodes[i].path);
   if(auto mapping=skeleton.retargets.find(i);mapping!=skeleton.retargets.end()){
    auto& scope=retargetScopes[{anchor,use.id}];
    if(!scope.renderer)scope.renderer=renderer;
    if(scope.visited.insert(i).second)
     if(auto route=NativeRoute(anchor,bone,skeleton,i,mapping->second))scope.routes.emplace(i,std::move(*route));
   }
   auto& dependencies=preloads[renderer];dependencies.emplace_back(cab,bone);dependencies.emplace_back(cab,owners.at(bone));
  }
  auto& slots=authorBoneSlots[{renderer,use.part}];
  for(size_t slot=0;slot<use.palette->size();++slot){
   const auto& path=use.palette->at(slot);
   auto node=document.nodes.find(path);
   if(node!=document.nodes.end()&&!node->second.source)slots.emplace(slot,Resolve(anchor,path));
  }
 }
 size_t Import(const Template& donor,size_t index){auto key=std::make_pair(donor.cab,index);auto prior=imported.find(key);if(prior!=imported.end())return prior->second;
  auto type=donor.layout.types.at(index);auto record=type.nativeRecord;auto script=int16_t(Le(record,5,2));
  if(script>=0){EiemNativeInput::Reader reader(donor.bytes,donor.layout.tableEnd,false,48+donor.layout.metadataSize);auto count=reader.Count();Require(size_t(script)<count,"Physics native script registration absent");reader.Skip(size_t(script)*12);auto file=reader.Get(4);auto id=int64_t(reader.Get(8));Require(file&&file<=donor.layout.externals.size(),"Physics donor script must resolve to a registered external MonoScript");
   Value pointer;auto ptrType=Type{};ptrType.nodes={{"PPtr<Object>","script",false,{1,2}},{"int","m_FileID",false,{}},{"SInt64","m_PathID",false,{}}};pointer=EmptyValue(ptrType,0);SetReference(ptrType,pointer,{donor.layout.externals.at(size_t(file)-1),id},layout,cab);
   auto mapped=AddSize(layout.scriptCount,scripts.size());Require(mapped<=INT16_MAX,"Physics script index cannot fit native field");scripts.push_back(EncodeValue(ptrType,pointer));WriteInteger(record,5,mapped,2);
  }
  auto mapped=layout.types.size();imported.emplace(key,mapped);layout.types.push_back(type);types.push_back({});
  for(size_t p=type.dependencyOffset;p<record.size();p+=4){auto dep=Le(record,p,4);Require(dep<donor.layout.types.size(),"Physics TypeTree dependency absent");auto next=Import(donor,size_t(dep));WriteInteger(record,p,next,4);}
  types.at(mapped-(layout.types.size()-types.size()))=record;layout.types.at(mapped).nativeRecord=record;return mapped;
 }
 void RemapScript(const Template& donor,const Type& type,Value& value){auto ref=NullableReference(type,Child(type,value,"m_Script"),donor.layout,donor.cab);Require(ref.second&&ref.first!=donor.cab,"Physics native script source unresolved");SetReference(type,Child(type,value,"m_Script"),ref,layout,cab);}
 int64_t Component(int64_t anchor,const std::string& identity,int64_t go,const Template& donor,int64_t donorId){auto object=donor.layout.objects.at(donorId);auto tree=ObjectValue(donor.bytes,donor.layout,donorId,&donor.origins);const auto& originalType=donor.layout.types.at(object.type);RemapScript(donor,originalType,tree);auto type=Import(donor,object.type);auto id=Allocate(anchor,identity,"physics-component");Add(id,type,std::move(tree));Reference(id,"m_GameObject",go);Append(go,"m_Component",id,"component");return id;}
 Vector3 TransformPoint(int64_t id,Vector3 point,int64_t stop){for(auto current=id;current&&current!=stop;current=parents.at(current)){const auto& t=Schema(current);auto& tree=Tree(current);auto s=ReadVector(t,Child(t,tree,"m_LocalScale"));for(size_t i=0;i<3;++i)point[i]*=s[i];auto& q=Child(t,tree,"m_LocalRotation");double x=FloatValue(t,Child(t,q,"x")),y=FloatValue(t,Child(t,q,"y")),z=FloatValue(t,Child(t,q,"z")),w=FloatValue(t,Child(t,q,"w"));auto tx=2*(y*point[2]-z*point[1]),ty=2*(z*point[0]-x*point[2]),tz=2*(x*point[1]-y*point[0]);Vector3 next{point[0]+w*tx+y*tz-z*ty,point[1]+w*ty+z*tx-x*tz,point[2]+w*tz+x*ty-y*tx};auto p=ReadVector(t,Child(t,tree,"m_LocalPosition"));for(size_t i=0;i<3;++i)point[i]=next[i]+p[i];}return point;}
 void Populate(int64_t anchor,int64_t preloadRenderer,const Document& d,const std::vector<const Group*>& groups,const ColliderMounts& mounts){
  const auto addedStart=added.size();std::map<std::string,int64_t> colliders,createdMounts;std::vector<NativeReference> refs;
  for(const auto* group:groups)for(const auto& id:group->colliders)if(!colliders.count(id)){const auto& c=d.colliders.at(id);auto parent=ColliderParent(anchor,d,c,mounts,createdMounts);auto owner=NewObject(anchor,parent,d.id+":collider:"+id,EiemPhysicsComponentIdentity::Name(package.id,id),c.position,c.rotation,{1,1,1});
   auto component=Component(anchor,d.id+":collider:"+id,owner.first,d.templates.at(c.cab),c.object);colliders[id]=component;refs.emplace_back(cab,owner.first);refs.emplace_back(cab,owner.second);refs.emplace_back(cab,component);
  }
  for(const auto* group:groups){std::map<std::string,int64_t> nodes;for(const auto& node:group->nodes){auto bone=Bone(anchor,d,node.first);if(node.second)Require(movingBones.insert(bone).second,"two authored Physics groups drive the same bone: "+node.first);nodes[node.first]=bone;}
   auto owner=NewObject(anchor,anchor,d.id+":group:"+group->id,EiemPhysicsComponentIdentity::Name(package.id,group->id),{0,0,0},{0,0,0,1},{1,1,1});auto component=Component(anchor,d.id+":group:"+group->id,owner.first,d.templates.at(group->cab),group->object);const auto& t=Schema(component);auto& v=Tree(component);
   auto& roots=ArrayValue(t,FieldPath(t,v,{"serializeData","rootBones"}));std::set<std::string> nodePaths;for(const auto& node:group->nodes)nodePaths.insert(node.first);
   for(const auto& node:group->nodes)if(!nodePaths.count(d.nodes.at(node.first).parent)){auto p=ArrayElement(t,roots);SetReference(t,p,{cab,nodes.at(node.first)},layout,cab);roots.children.push_back(std::move(p));}roots.count=roots.children.size();Require(roots.count,"Physics chain has no root");
   auto& list=ArrayValue(t,FieldPath(t,v,{"serializeData","colliderCollisionConstraint","colliderList"}));for(const auto& id:group->colliders){auto p=ArrayElement(t,list);SetReference(t,p,{cab,colliders.at(id)},layout,cab);list.children.push_back(std::move(p));}list.count=list.children.size();
   auto& positions=ArrayValue(t,FieldPath(t,v,{"serializeData2","selectionData","positions"}));auto& attributes=ArrayValue(t,FieldPath(t,v,{"serializeData2","selectionData","attributes"}));double maxDistance=0;
   for(const auto& node:group->nodes){auto point=ArrayElement(t,positions);auto position=TransformPoint(nodes.at(node.first),{0,0,0},anchor);SetVector(t,point,position);positions.children.push_back(std::move(point));auto attribute=ArrayElement(t,attributes);SetInteger(t,Child(t,attribute,"Value"),node.second?2:1);attributes.children.push_back(std::move(attribute));auto parent=d.nodes.at(node.first).parent;if(nodes.count(parent)){auto other=TransformPoint(nodes.at(parent),{0,0,0},anchor);double dist=0;for(size_t i=0;i<3;++i)dist+=(position[i]-other[i])*(position[i]-other[i]);maxDistance=std::max(maxDistance,sqrt(dist));}}
   positions.count=positions.children.size();attributes.count=attributes.children.size();SetFloat(t,FieldPath(t,v,{"serializeData2","selectionData","maxConnectionDistance"}),maxDistance);SetInteger(t,FieldPath(t,v,{"serializeData2","selectionData","userEdit"}),1);
   refs.emplace_back(cab,owner.first);refs.emplace_back(cab,owner.second);refs.emplace_back(cab,component);
  }
  for(size_t i=addedStart;i<added.size();++i)refs.emplace_back(cab,added[i].id);
  std::sort(refs.begin(),refs.end());refs.erase(std::unique(refs.begin(),refs.end()),refs.end());
  preloads[preloadRenderer].insert(preloads[preloadRenderer].end(),refs.begin(),refs.end());
 }
 void Preloads(){for(auto id:layout.order)if(Schema(id).classId==142){const auto& t=Schema(id);auto& bundle=Tree(id);auto& list=ArrayValue(t,Child(t,bundle,"m_PreloadTable"));std::vector<Value> next;std::vector<size_t> offsets{0};std::set<int64_t> covered;
   for(const auto& item:list.children){next.push_back(item);auto ref=NullableReference(t,item,layout,cab);auto f=ref.first==cab?preloads.find(ref.second):preloads.end();if(f!=preloads.end()){covered.insert(ref.second);for(const auto& extra:f->second){auto p=ArrayElement(t,list);SetReference(t,p,extra,layout,cab);next.push_back(std::move(p));}}offsets.push_back(next.size());}
   Require(covered.size()==preloads.size(),"Physics target Renderer missing from preload closure");auto update=[&](Value& info){auto& start=Child(t,info,"preloadIndex");auto& count=Child(t,info,"preloadSize");auto a=Integer(start),b=Integer(count);Require(a<offsets.size()&&b<offsets.size()-a,"Physics preload interval differs");SetInteger(t,start,offsets[size_t(a)]);SetInteger(t,count,offsets[size_t(a+b)]-offsets[size_t(a)]);};
   for(const char* field:{"m_Container","m_HashContainer"})for(auto& row:ArrayValue(t,Child(t,bundle,field)).children)update(Child(t,row,"second"));update(Child(t,bundle,"m_MainAsset"));list.children=std::move(next);list.count=list.children.size();changed.insert(id);
  }}
 public:
 MemberBuilder(const Bytes& b,const std::string& c,const Package& p,const AssetIndex& a,const std::vector<Document>& d,const std::set<Identity>* selectedTargets=nullptr):bytes(b),cab(c),package(p),assets(a),documents(d),relevantTargets(selectedTargets),layout(ParseSerialized(b)){for(const auto& o:layout.objects)typeIds[o.first]=o.second.type;}
 MemberBuilder(SerializedLayout metadata,const std::string& c,const Package& p,const AssetIndex& a,const std::vector<Document>& d,Project fields,std::function<Value(int64_t)> object,const std::set<Identity>* selectedTargets=nullptr):bytes(EmptySource()),cab(c),package(p),assets(a),documents(d),relevantTargets(selectedTargets),layout(std::move(metadata)),project(std::move(fields)),decode(std::move(object)){Require(project&&decode,"projected Physics source absent");for(const auto& o:layout.objects)typeIds[o.first]=o.second.type;}
 void SetSkeletonUses(const SkeletonUses& uses){skeletonUses=&uses;}
 void SetRetargetPalettes(const std::vector<MeshConsumer>& consumers,const std::vector<BuiltMeshPart>& parts){
  retargetConsumers=&consumers;retargetParts=&parts;
 }
 Bytes Build(const AuthorByteSpans* inputOrigins=nullptr,AuthorByteSpans* outputOrigins=nullptr,
     const std::vector<MeshConsumer>* matchedConsumers=nullptr){Require(!built,"Physics member builder reused");built=true;Require(!project||(!inputOrigins&&!outputOrigins),"projected Physics cannot use full input provenance");sourceOrigins=inputOrigins;std::set<NativeReference> refs;std::map<NativeReference,Identity> targets;for(const auto& d:documents)for(const auto& g:d.groups)for(const auto& target:g.targets){if(relevantTargets&&!relevantTargets->count(target))continue;Require(package.targetIndex.count(target)&&assets.count(target),"Physics Mesh is not a current INI replacement target");const auto& asset=assets.at(target);auto reference=CanonicalSerializedAssetReference({asset.cab,asset.pathId});targets[reference]=target;refs.insert(reference);}
  std::vector<MeshConsumer> consumers;
  if(matchedConsumers){
   for(const auto& consumer:*matchedConsumers)
    if(refs.count(CanonicalSerializedAssetReference(consumer.mesh)))consumers.push_back(consumer);
  }else consumers=ProjectedMeshConsumers(layout,cab,refs,[&](int64_t id,const std::set<std::string>& fields){return project?project(id,fields):ObjectFields(bytes,layout,id,fields);},SerializedAssetCabKey);
  if(consumers.empty()&&(!skeletonUses||skeletonUses->empty())){if(outputOrigins)*outputOrigins=inputOrigins?*inputOrigins:AuthorByteSpans{};return bytes;}Hierarchy();CollectNativePalettes();
  for(const auto& d:documents){std::map<int64_t,std::pair<int64_t,std::vector<const Group*>>> scopes;
   auto mounts=OptionalColliderMounts(d);
   for(const auto& consumer:consumers)for(const auto& g:d.groups)if(g.targets.count(targets.at(CanonicalSerializedAssetReference(consumer.mesh)))){auto anchor=Anchor(consumer.renderer,d,g,mounts);auto& scope=scopes[anchor];if(!scope.first)scope.first=consumer.renderer;if(std::find(scope.second.begin(),scope.second.end(),&g)==scope.second.end())scope.second.push_back(&g);}
   for(const auto& scope:scopes)Populate(scope.first,scope.second.first,d,scope.second.second,mounts);
  }
  if(skeletonUses)for(const auto& row:*skeletonUses)for(const auto& use:row.second)PopulateSkeleton(row.first,use);
  PublishRetargetRoutes();
  for(auto& row:preloads){auto& dependencies=row.second;std::sort(dependencies.begin(),dependencies.end());dependencies.erase(std::unique(dependencies.begin(),dependencies.end()),dependencies.end());}
  Preloads();std::map<int64_t,AuthorByteSpans> objectOrigins;for(auto id:changed)encodedChanges[id]=EncodeValue(Schema(id),Tree(id),outputOrigins?&objectOrigins[id]:nullptr);for(auto& object:added)object.bytes=EncodeValue(Schema(object.id),Tree(object.id),outputOrigins?&objectOrigins[object.id]:nullptr);if(project)return {};return BuildSerialized(bytes,layout,encodedChanges,added,{},types,scripts,inputOrigins,&objectOrigins,outputOrigins);
 }
 const SerializedLayout& OutputLayout()const{Require(built,"Physics metadata requested before Build");return layout;}
 AuthorBoneSlots TakeAuthorBoneSlots(){Require(built,"author Bone slots requested before Build");return std::move(authorBoneSlots);}
 const std::vector<Bytes>& AddedTypes()const{return types;}
 const std::vector<Bytes>& AddedScripts()const{return scripts;}
 std::map<int64_t,Bytes> TakeChanges(){Require(built,"Physics changes requested before Build");return std::move(encodedChanges);}
 std::vector<AddedObject> TakeAdded(){Require(built,"Physics additions requested before Build");return std::move(added);}
};
inline bool Build(EndfieldInput::Bundle& bundle,const Package& package,const AssetIndex& assets,const std::vector<Document>* prepared=nullptr,const std::set<Identity>* relevantTargets=nullptr){auto local=prepared?std::vector<Document>{}:Documents(package);const auto& documents=prepared?*prepared:local;if(documents.empty())return false;bool changed=false;for(auto& member:bundle.members)if(SerializedHeader(member.bytes)){auto built=MemberBuilder(member.bytes,member.name,package,assets,documents,relevantTargets).Build(&member.origins,&member.origins);if(built!=member.bytes){member.bytes=std::move(built);changed=true;}}return changed;}
inline Bytes Build(const Bytes& input,const Package& package,const AssetIndex& assets,const std::vector<Document>* prepared=nullptr,const std::set<Identity>* relevantTargets=nullptr){auto local=prepared?std::vector<Document>{}:Documents(package);const auto& documents=prepared?*prepared:local;if(documents.empty())return input;auto bundle=EndfieldInput::Decode(input);return Build(bundle,package,assets,&documents,relevantTargets)?EndfieldInput::Encode(input,bundle.members):input;}
} // namespace EiemPhysicsInput

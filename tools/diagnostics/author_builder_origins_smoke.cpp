#define main StaticFixtureMain
#include "static_mesh_assembly_smoke.cpp"
#undef main
#include "../../src/eiem_physics_input_builder.h"

namespace {
void S(Bytes& bytes,const std::string& value){N(bytes,value.size(),8);bytes.insert(bytes.end(),value.begin(),value.end());}
void D(Bytes& bytes,double value){uint64_t bits;memcpy(&bits,&value,8);N(bytes,bits,8);}
void Blob(Bytes& bytes,const Bytes& value){N(bytes,value.size(),8);bytes.insert(bytes.end(),value.begin(),value.end());}
void PhysicsNode(Bytes& bytes,const std::string& path,bool source){
 S(bytes,path);S(bytes,source?"":"spine");N(bytes,source?1:0,8);N(bytes,source?0:1,8);
 for(double value:Vector3{0,0,source?0.0:-0.2})D(bytes,value);for(double value:std::array<double,4>{0,0,0,1})D(bytes,value);for(double value:Vector3{1,1,1})D(bytes,value);
}
std::shared_ptr<Resource> PhysicsResource(const Identity& target){
 Fixture donor;donor.cab="CAB-physics-template";donor.types={Schema(114,{Ref("m_GameObject"),Ref("m_Script"),
  {"SerializeData","serializeData",{Arr("rootBones",Ref("data")),{"Collisions","colliderCollisionConstraint",{Arr("colliderList",Ref("data"))}}}},
  {"SerializeData2","serializeData2",{{"SelectionData","selectionData",{Arr("positions",Vec("data")),Arr("attributes",{"Attribute","data",{{"int","Value",{}}}}),{"float","maxConnectionDistance",{}},{"bool","userEdit",{}}}}}},
  {"float","damping",{}}})};donor.objects.clear();donor.Add(900,0);
 SetReference(donor.T(900),Child(donor.T(900),donor.V(900),"m_Script"),{"CAB-MonoScript",123},donor.refs,donor.cab);
 SetFloat(donor.T(900),Child(donor.T(900),donor.V(900),"damping"),0.125);
 auto templateBytes=donor.BytesOf();auto templateLayout=ParseSerialized(templateBytes);templateLayout.externals=donor.refs.externals;
 templateBytes=BuildSerialized(templateBytes,templateLayout,{});
 Bytes block;S(block,EiemPhysicsInput::structure);S(block,"ribbon-physics");N(block,2,8);PhysicsNode(block,"spine",true);PhysicsNode(block,"spine/ribbon",false);
 N(block,1,8);S(block,donor.cab);Blob(block,templateBytes);N(block,0,8);
 N(block,1,8);S(block,"ribbon-group");S(block,"Ribbon");S(block,donor.cab);N(block,900,8);
 N(block,1,8);S(block,target.path);S(block,target.asset);N(block,2,8);S(block,"spine");N(block,0,8);S(block,"spine/ribbon");N(block,1,8);N(block,0,8);
 auto resource=std::make_shared<Resource>();resource->bytes.resize(13,0xcc);resource->blocks["@physics"]={13,block.size()};resource->bytes.insert(resource->bytes.end(),block.begin(),block.end());return resource;
}
void PhysicsBuilderOrigins(){
 Fixture source;source.types[3]=Schema(137,{Ref("m_GameObject"),Ref("m_Mesh"),Ref("m_RootBone"),Arr("m_Bones",Ref("data"))});
 for(int64_t id:{13,23}){auto bone=id+28;source.Add(bone-1,0);SetString(source.T(bone-1),Child(source.T(bone-1),source.V(bone-1),"m_Name"),"spine");source.Add(bone,1);source.Pointer(bone,"m_GameObject",bone-1);source.Pointer(bone,"m_Father",id-2);source.List(id-2,"m_Children",{bone});SetVector(source.T(bone),Child(source.T(bone),source.V(bone),"m_LocalScale"),{1,1,1});SetFloat(source.T(bone),Child(source.T(bone),Child(source.T(bone),source.V(bone),"m_LocalRotation"),"w"),1);source.List(bone-1,"m_Component",{bone},"component");
  source.objects.at(id)={3,EmptyValue(source.types[3],0)};source.Pointer(id,"m_GameObject",id-3);source.Pointer(id,"m_Mesh",100);source.Pointer(id,"m_RootBone",bone);source.List(id,"m_Bones",{bone});}
 auto before=source.BytesOf();auto beforeLayout=ParseSerialized(before);Identity target{"assets/static.mesh","Static"};Package package;package.id="physics-origins";package.targetIndex[target]="Rule";
 package.resources["resources/physics.bin"]=PhysicsResource(target);AssetIndex assets;assets[target].cab=source.cab;assets[target].pathId=100;assets[target].classId=43;
 auto documents=EiemPhysicsInput::Documents(package);Require(documents.size()==1&&!documents.front().templates.begin()->second.origins.empty(),"Physics document lost author source");
 const auto& resource=*package.resources.begin()->second;const auto& donor=documents.front().templates.begin()->second;EiemAuthorBytes::Validate(donor.bytes,donor.origins);
 Require(EiemPhysicsInput::Decode(resource).templates.begin()->second.origins.empty(),"legacy Physics decoder invented a resource name");
 auto sourceValue=ObjectValue(before,beforeLayout,100);auto sourceObject=EncodeValue(source.T(100),sourceValue);auto sourceResource=sourceObject;
 auto sourceTree=DecodeAuthorValue(source.T(100),0,sourceObject,"resources/source-mesh.bin",sourceResource,0);AuthorByteSpans sourceOrigins,relative;EncodeValue(source.T(100),sourceTree,&relative);
 EiemAuthorBytes::Append(sourceOrigins,EiemAuthorBytes::Slice(relative,0,sourceObject.size(),beforeLayout.objects.at(100).offset));
 AuthorByteSpans outputOrigins;auto output=EiemPhysicsInput::MemberBuilder(before,source.cab,package,assets,documents).Build(&sourceOrigins,&outputOrigins);
 Require(output==EiemPhysicsInput::MemberBuilder(before,source.cab,package,assets,documents).Build(),"Physics provenance changed output bytes");EiemAuthorBytes::Validate(output,outputOrigins);
 auto layout=ParseSerialized(output);size_t components=0;std::set<size_t> dampingOffsets;
 std::set<NativeReference> fixedRoots;
 for(const auto& object:layout.objects)if(layout.types.at(object.second.type).classId==114&&object.first!=14&&object.first!=24){
  const auto& type=layout.types.at(object.second.type);bool physics=false;for(const auto& node:type.nodes)physics|=node.name=="damping";if(!physics)continue;
  auto value=ObjectValue(output,layout,object.first,&outputOrigins);const auto& damping=Child(type,value,"damping");
  const auto& settings=Child(type,value,"serializeData");
  const auto& roots=ConstArray(type,Child(type,settings,"rootBones")).children;
  Require(roots.size()==1,"Physics group does not have one root Transform");
  fixedRoots.insert(NullableReference(type,roots.front(),layout,source.cab));
  const auto& selection=Child(type,Child(type,value,"serializeData2"),"selectionData");
  Require(ConstArray(type,Child(type,selection,"positions")).children.empty()&&
          ConstArray(type,Child(type,selection,"attributes")).children.empty()&&
          Integer(Child(type,selection,"userEdit"))==0,
          "complete root-fixed chain relies on bind-pose positions during native build");
  Require(damping.rawOrigins.size()==1&&damping.rawOrigins.front().resource=="resources/physics.bin"&&
          damping.rawOrigins.front().owned&&damping.rawOrigins.front().bytes==damping.rawOrigins.front().owned.get(),
          "Physics parameter did not bind to its retained author payload");
  Require(Child(type,value,"m_GameObject").children.front().rawOrigins.empty()&&Child(type,value,"m_Script").children.front().rawOrigins.empty(),"remapped Physics reference retained template source");
  auto encoded=EncodeValue(type,value);auto raw=EiemAuthorBytes::Slice(outputOrigins,object.second.offset,object.second.size);for(const auto& span:raw)if(span.resource=="resources/physics.bin")dampingOffsets.insert(span.offset);
  Require(encoded.size()==object.second.size,"Physics component re-encode changed extent");++components;
 }
 Require(components==2&&!dampingOffsets.empty(),"Physics fixture did not populate both independent actor roots");
 Require(fixedRoots==std::set<NativeReference>{{source.cab,41},{source.cab,51}},
         "Physics fixed roots did not bind to their own actor Transforms");
 auto meshOrigins=EiemAuthorBytes::Slice(outputOrigins,layout.objects.at(100).offset,layout.objects.at(100).size);Require(!meshOrigins.empty()&&meshOrigins.front().resource=="resources/source-mesh.bin","Physics compile dropped existing Mesh provenance");
 // Rebinding explicitly recorded spans after a parameter edit equals a full
 // compile; no byte search or resource-specific runtime code is needed.
 auto changedResource=std::make_shared<Resource>(resource);auto changedPackage=package;changedPackage.resources["resources/physics.bin"]=changedResource;
 const auto& templateType=donor.layout.types.at(donor.layout.objects.at(900).type);auto templateValue=ObjectValue(donor.bytes,donor.layout,900,&donor.origins);
 auto parameter=Child(templateType,templateValue,"damping").rawOrigins.front();float damping=0.5f;auto physicsBlock=changedResource->blocks.at("@physics");memcpy(changedResource->bytes.data()+physicsBlock.offset+parameter.offset,&damping,4);
 auto rebound=output;for(const auto& span:outputOrigins)if(span.resource=="resources/physics.bin")std::copy_n(changedResource->bytes.begin()+physicsBlock.offset+span.offset,span.size,rebound.begin()+span.output);
 auto changedDocuments=EiemPhysicsInput::Documents(changedPackage);auto rebuilt=EiemPhysicsInput::MemberBuilder(before,source.cab,changedPackage,assets,changedDocuments).Build();
 Require(rebound==rebuilt,"Physics explicit payload replay differs from full compile");
 std::cout<<"PASS Physics author provenance: template decoding, both actor roots, remapped references and parameter replay/full-compile equality\n";

 // The world actor has the native collider mount; the UI actor omits it.
 auto& document=documents.front();
 const EiemPhysicsInput::Node mount{"spine/native collider","spine",true,true,
     {0.25,0.5,0.75},{1,2,1},{0,1,0,0}};
 document.nodes.emplace(mount.path,mount);
 document.colliders.emplace("collider",EiemPhysicsInput::Collider{
     "collider","Collider",mount.path,donor.cab,900,{0,0,0},{0,0,0,1}});
 document.groups.front().colliders.push_back("collider");
 source.Add(60,0);source.Add(61,1);
 SetString(source.T(60),Child(source.T(60),source.V(60),"m_Name"),"native collider");
 source.List(60,"m_Component",{61},"component");
 source.Pointer(61,"m_GameObject",60);source.Pointer(61,"m_Father",41);
 source.List(41,"m_Children",{61});
 auto variant=source.BytesOf();
 auto assembled=EiemPhysicsInput::MemberBuilder(variant,source.cab,package,assets,documents).Build();
 auto result=ParseSerialized(assembled);
 auto ref=[&](int64_t id,const char* name){
   const auto& type=result.types.at(result.objects.at(id).type);
   auto value=ObjectValue(assembled,result,id);
   return NullableReference(type,Child(type,value,name),result,source.cab);
 };
 auto generated=[&](int64_t anchor,const char* purpose){
   return RendererObjectId(source.cab,package,"physics-transform",anchor,document.id+purpose);
 };
 const auto proxy=generated(21,":collider-mount:spine/native collider");
 Require(!result.objects.count(generated(11,":collider-mount:spine/native collider"))&&
         ref(generated(11,":collider:collider"),"m_Father")==NativeReference{source.cab,61}&&
         ref(generated(21,":collider:collider"),"m_Father")==NativeReference{source.cab,proxy}&&
         ref(proxy,"m_Father")==NativeReference{source.cab,51},
         "Physics collider mount left its actor skeleton");
 const auto& proxyType=result.types.at(result.objects.at(proxy).type);
 auto proxyValue=ObjectValue(assembled,result,proxy);
 Require(ReadVector(proxyType,Child(proxyType,proxyValue,"m_LocalPosition"))==mount.position&&
         ReadVector(proxyType,Child(proxyType,proxyValue,"m_LocalScale"))==mount.scale&&
         FloatValue(proxyType,Child(proxyType,Child(proxyType,proxyValue,"m_LocalRotation"),"y"))==1,
         "Physics collider mount lost its authored transform");
 for(auto anchor:{11,21}){
  auto component=RendererObjectId(source.cab,package,"physics-component",anchor,document.id+":group:ribbon-group");
  const auto& type=result.types.at(result.objects.at(component).type);
  auto value=ObjectValue(assembled,result,component);
  const auto& selection=Child(type,Child(type,value,"serializeData2"),"selectionData");
  const auto& attributes=ConstArray(type,Child(type,selection,"attributes")).children;
  Require(attributes.size()==2&&Integer(Child(type,attributes[0],"Value"))==1&&
          Integer(Child(type,attributes[1],"Value"))==2,
          "partial chain must retain its explicit selection attributes");
 }
 document.groups.front().nodes.emplace_back(mount.path,false);
 bool rejected=false;
 try{EiemPhysicsInput::MemberBuilder(variant,source.cab,package,assets,documents).Build();}
 catch(const std::runtime_error& error){rejected=std::string(error.what()).find("no unique local skeleton anchor")!=std::string::npos;}
 Require(rejected,"Physics synthesized a missing native chain bone");
 std::cout<<"PASS collider mount fallback: world reference kept, UI proxy local with authored TRS, missing chain bone rejected\n";
}
void BoundsOrigins(){
 auto matrix=AffineIdentity();matrix[3]=0.123;matrix[7]=-2;auto name=BoundsOriginName(17,matrix,"resources/mesh:variant.bin");
 size_t slot=0;AffineMatrix decoded{};std::string resource;Require(DecodeBoundsOrigin(name,slot,decoded,resource)&&slot==17&&decoded==matrix&&resource=="resources/mesh:variant.bin","bounds context identity did not roundtrip");
 Require(!DecodeBoundsOrigin("@bounds::invalid",slot,decoded,resource)&&!DecodeBoundsOrigin(name.substr(0,name.size()-resource.size()),slot,decoded,resource),"invalid bounds context accepted");
 auto type=Schema(137,{{"AABB","m_AABB",{Vec("m_Center"),Vec("m_Extent")}}});auto value=EmptyValue(type,0);auto& bounds=Child(type,value,"m_AABB");
 SetVector(type,Child(type,bounds,"m_Center"),{1,2,3});SetVector(type,Child(type,bounds,"m_Extent"),{4,5,6});BindBoundsOrigins(type,bounds,name);
 AuthorByteSpans origins;auto bytes=EncodeValue(type,value,&origins);Require(origins.size()==1&&origins.front().resource==name&&origins.front().size==24&&origins.front().owned&&origins.front().owned->size()==24,"bounds leaves did not share an owned 24-byte derived buffer");
 EiemAuthorBytes::Validate(bytes,origins);auto copied=DecodeValue(type,0,bytes,&origins);SetFloat(type,Child(type,Child(type,Child(type,copied,"m_AABB"),"m_Center"),"x"),1);
 EncodeValue(type,copied,&origins);Require(origins.front().output==4&&origins.front().size==20,"generated bounds float retained stale derived dependency");
 std::cout<<"PASS Renderer bounds: context-name roundtrip, six float owned origins and setter invalidation\n";
}
void RendererAndMarkerOrigins(){
 Fixture fixture;
 // Mark needs native shape/vertex fields to assert that renaming is lossless.
 fixture.types[5]=Schema(43,{{"string","m_Name",{}},{"TypelessData","m_Shapes",{}},{"TypelessData","m_VertexData",{}}});fixture.objects.at(100)={5,EmptyValue(fixture.types[5],0)};
 SetString(fixture.T(100),Child(fixture.T(100),fixture.V(100),"m_Name"),"Static");Child(fixture.T(100),fixture.V(100),"m_Shapes").raw={1,2,3,4};Child(fixture.T(100),fixture.V(100),"m_VertexData").raw={5,6,7,8};
 auto before=fixture.BytesOf();auto layout=ParseSerialized(before);auto resource=EncodeValue(fixture.T(100),fixture.V(100));auto value=DecodeAuthorValue(fixture.T(100),0,resource,"resources/mesh.bin",resource,0);AuthorByteSpans relative,origins;EncodeValue(fixture.T(100),value,&relative);EiemAuthorBytes::Append(origins,EiemAuthorBytes::Slice(relative,0,resource.size(),layout.objects.at(100).offset));
 Identity target{"assets/static.mesh","Static"},material{"assets/material.mat","Mat"};Package package;package.id="renderer-origins";package.targetIndex[target]="R";package.rules["R"].parts={"P","Q"};
 package.parts["P"].mesh=package.parts["Q"].mesh="M";package.parts["P"].material=package.parts["Q"].material="A";package.materials["A"].source=material;
 package.meshes["M"].resource="mesh";auto meshResource=std::make_shared<Resource>();meshResource->type=fixture.types[5].hash;package.resources["mesh"]=meshResource;
 auto stableMarker=RuntimePartName(package,"P",target),legacyMarker=LegacyRuntimePartName(package,"P",target);meshResource->bytes.push_back(17);
 Require(RuntimePartName(package,"P",target)==stableMarker&&LegacyRuntimePartName(package,"P",target)!=legacyMarker,"resource update changed stable marker or lost legacy version identity");
 auto composed=package;composed.id="merged-owner";composed.partOwners["P"]={package.id,"P"};Require(RuntimePartName(composed,"P",target)==stableMarker,"multi-Mod composition changed original part identity");
 Require(RuntimePartName(package,"Q",target)!=stableMarker,"different parts share a runtime marker");
 AssetIndex assets;assets[target].cab=assets[material].cab=fixture.cab;assets[target].classId=43;assets[target].pathId=100;assets[target].type=fixture.types[5].hash;assets[material].classId=21;assets[material].pathId=200;
 std::vector<BuiltMeshPart> parts{{target,"R","P",fixture.cab,100,100,3,3,{}},{target,"R","Q",fixture.cab,100,101,3,3,{}}};
 AuthorByteSpans rebuiltOrigins;auto rebuilt=RendererMemberBuilder(before,fixture.cab,package,assets,parts).Build(&origins,&rebuiltOrigins);
 Require(rebuilt.bytes==RendererMemberBuilder(before,fixture.cab,package,assets,parts).Build().bytes,"Renderer provenance changed output bytes");EiemAuthorBytes::Validate(rebuilt.bytes,rebuiltOrigins);
 auto after=ParseSerialized(rebuilt.bytes);Require(!EiemAuthorBytes::Slice(rebuiltOrigins,after.objects.at(100).offset,after.objects.at(100).size).empty(),"Renderer dropped unmodified Mesh origins");
 EndfieldInput::Bundle bundle;bundle.members.push_back({fixture.cab,0,std::move(rebuilt.bytes),std::move(rebuiltOrigins)});size_t count=0;
 Require(EiemResourceReloadPlan::Mark(bundle,{{{fixture.cab,100},"new-marker"}},count)&&count==1,"native Mesh marker not applied");
 auto marked=ParseSerialized(bundle.members.front().bytes);auto markedValue=ObjectValue(bundle.members.front().bytes,marked,100,&bundle.members.front().origins);
 Require(Child(fixture.T(100),markedValue,"m_Name").rawOrigins.empty()&&!Child(fixture.T(100),markedValue,"m_Shapes").rawOrigins.empty()&&!Child(fixture.T(100),markedValue,"m_VertexData").rawOrigins.empty(),"marker name/source data provenance differs");
 EiemAuthorBytes::Validate(bundle.members.front().bytes,bundle.members.front().origins);
 std::cout<<"PASS Renderer/marker chain: byte equality, original Mesh origins and generated-name invalidation\n";
}
void RendererBoundsIntegration(){
 Fixture fixture;fixture.types[3]=Schema(137,{Ref("m_GameObject"),Ref("m_Mesh"),{"bool","m_Enabled",{}},Arr("m_Materials",Ref("data")),Arr("m_Bones",Ref("data")),Ref("m_RootBone"),Ref("m_SkinningRoot"),Arr("m_BlendShapeWeights",{"float","data",{}}),{"bool","m_DirtyAABB",{}},{"AABB","m_AABB",{Vec("m_Center"),Vec("m_Extent")}}});
 for(int64_t id:{13,23}){fixture.objects.at(id)={3,EmptyValue(fixture.types[3],0)};fixture.Pointer(id,"m_GameObject",id-3);fixture.Pointer(id,"m_Mesh",100);fixture.Pointer(id,"m_RootBone",id-2);fixture.List(id,"m_Bones",{id-2});fixture.List(id,"m_Materials",{200});}
 auto before=fixture.BytesOf();Identity target{"assets/static.mesh","Static"},material{"assets/material.mat","Mat"};Package package;package.id="bounds-origins";package.targetIndex[target]="R";package.rules["R"].parts={"P"};package.parts["P"].mesh="M";package.parts["P"].material="A";package.materials["A"].source=material;
 package.meshes["M"].resource="mesh";package.meshes["M"].bones={""};auto resource=std::make_shared<Resource>();resource->type=fixture.types[5].hash;package.resources["mesh"]=resource;
 AssetIndex assets;assets[target].cab=assets[material].cab=fixture.cab;assets[target].classId=43;assets[target].pathId=100;assets[target].type=resource->type;assets[material].classId=21;assets[material].pathId=200;
 std::vector<BuiltMeshPart> parts{{target,"R","P",fixture.cab,100,100,3,3,{}}};RendererBoundsCache cache;cache.values.emplace(std::make_tuple(std::string("mesh"),size_t(0),AffineIdentity()),std::make_pair(Vector3{1,2,3},Vector3{4,5,6}));
 RendererMemberBuilder builder(before,fixture.cab,package,assets,parts);builder.SetBoundsCache(cache);AuthorByteSpans origins;auto result=builder.Build(nullptr,&origins);EiemAuthorBytes::Validate(result.bytes,origins);
 auto layout=ParseSerialized(result.bytes);Require(origins.size()==2,"skinned Renderer bounds origin count differs");
 for(int64_t id:{13,23}){auto value=ObjectValue(result.bytes,layout,id,&origins);const auto& type=layout.types.at(layout.objects.at(id).type);auto& bounds=Child(type,value,"m_AABB");const auto& leaf=Child(type,Child(type,bounds,"m_Center"),"x");
  Require(leaf.rawOrigins.size()==1&&leaf.rawOrigins.front().resource==BoundsOriginName(0,AffineIdentity(),"mesh")&&leaf.rawOrigins.front().owned&&leaf.rawOrigins.front().owned->size()==24,"skinned Renderer did not bind derived bounds context");
 }
 std::cout<<"PASS skinned Renderer bounds integration: both native consumers retain derived payload origins\n";

 // The unweighted Pivot drives Tip. Neither needs a Physics group.
 auto skeleton=std::make_shared<EiemAuthorBoneAnimation::Skeleton>();
 using BoneNode=EiemAuthorBoneAnimation::Node;
 BoneNode root,pivot,tip;
 pivot.path="Pivot";pivot.parent=0;pivot.source=false;pivot.position[1]=1;
 tip.path="Pivot/Tip";tip.parent=1;tip.source=false;tip.position[1]=2;
 skeleton->nodes={root,pivot,tip};
 package.meshes["M"].bones={"","Pivot/Tip"};
 EiemPhysicsInput::SkeletonUses uses;
 for(int64_t id:{13,23})uses[id].push_back({"rig","P",skeleton,&package.meshes.at("M").bones});
 uses[13].push_back({"rig","SiblingPart",skeleton,&package.meshes.at("M").bones});
 const std::vector<EiemPhysicsInput::Document> noPhysics;
 EiemPhysicsInput::MemberBuilder skeletonBuilder(before,fixture.cab,package,assets,noPhysics);
 skeletonBuilder.SetSkeletonUses(uses);
 auto withBones=skeletonBuilder.Build();
 auto slots=skeletonBuilder.TakeAuthorBoneSlots();
 Require(slots.size()==3&&slots.at({13,"P"})==slots.at({13,"SiblingPart"})&&
         slots.at({13,"P"}).at(1)!=slots.at({23,"P"}).at(1),"author skeleton crossed instances or duplicated a shared bone");
 auto skeletonLayout=ParseSerialized(withBones);
 Require(skeletonLayout.objects.size()==ParseSerialized(before).objects.size()+8,"author skeleton added objects other than two bone pairs per instance");
 auto pointer=[&](int64_t id,const char* field){
  auto value=ObjectValue(withBones,skeletonLayout,id);const auto& type=skeletonLayout.types.at(skeletonLayout.objects.at(id).type);
  return NullableReference(type,Child(type,value,field),skeletonLayout,fixture.cab).second;
 };
 RendererMemberBuilder skinned(withBones,fixture.cab,package,assets,parts);
 skinned.SetBoundsCache(cache);skinned.SetAuthorBoneSlots(slots);
 auto authored=skinned.Build();
 Require(authored.parts.size()==2,"ordinary author bones did not assemble both skinned Renderers");
 for(const auto& part:authored.parts){
   auto tipId=slots.at({part.sourceRenderer,"P"}).at(1);
   auto rootId=part.sourceRenderer-2;
   auto pivotId=pointer(tipId,"m_Father");
   Require(pointer(pivotId,"m_Father")==rootId&&part.bones==std::vector<int64_t>{rootId,tipId},
           "ordinary author parent or weighted Renderer bone slot differs");
 }
 std::cout<<"PASS ordinary author bones: native slot binding, unweighted parent, shared Parts, isolated roots, no Physics\n";
}
} // namespace
#ifndef EIEM_AUTHOR_BUILDER_FIXTURE_ONLY
int main(){try{PhysicsBuilderOrigins();BoundsOrigins();RendererAndMarkerOrigins();RendererBoundsIntegration();return 0;}catch(const std::exception& e){std::cerr<<"FAIL author builder provenance: "<<e.what()<<'\n';return 1;}}
#endif

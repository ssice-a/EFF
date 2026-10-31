#define NOMINMAX
#include "../../src/eiem_static_resource_sources.h"
#include <iostream>
using namespace EiemNativeInput;
struct FieldSpec {std::string type,name;std::vector<FieldSpec> children;};
FieldSpec Ref(const char* name){return {"PPtr<Object>",name,{{"int","m_FileID",{}},{"SInt64","m_PathID",{}}}};}
FieldSpec Arr(const char* name,FieldSpec item){item.name="data";return {"vector",name,{{"Array","Array",{{"int","size",{}},item}}}};}
FieldSpec Vec(const char* name,bool rotation=false){FieldSpec f{"Vector3f",name,{{"float","x",{}},{"float","y",{}},{"float","z",{}}}};if(rotation)f.children.push_back({"float","w",{}});return f;}
Type Schema(int kind,std::initializer_list<FieldSpec> fields){Type t;t.classId=kind;t.hash[0]=(unsigned char)kind;
 auto add=[&](auto&& self,const FieldSpec& f)->size_t{auto n=t.nodes.size();t.nodes.push_back({f.type,f.name,false,{}});for(const auto& c:f.children){auto child=self(self,c);t.nodes[n].children.push_back(child);}return n;};
 add(add,{"Object","Base",fields});return t;}
void N(Bytes& b,uint64_t v,size_t n){for(size_t i=0;i<n;++i)b.push_back((unsigned char)(v>>(i*8)));}
void Z(Bytes& b,const std::string& s){b.insert(b.end(),s.begin(),s.end());b.push_back(0);}
void Be(Bytes& b,size_t at,uint64_t v,size_t n){for(size_t i=0;i<n;++i)b[at+i]=(unsigned char)(v>>((n-1-i)*8));}
Bytes Bundle(const Bytes& member,const std::string& name){Bytes source(49);auto a=uint32_t(123);EndfieldInput::Put(source,0,a^0x4A92F0CD,4);EndfieldInput::Put(source,4,((4*a&0xFFFF0000)^EndfieldInput::Rol32(a,18)^0xD8B1E637),4);
 auto info=EndfieldInput::Encode32(1);auto size=EndfieldInput::Encode64(source.size());EndfieldInput::Put(source,8,info.second,2);EndfieldInput::Put(source,14,8,4);EndfieldInput::Put(source,18,size.second,4);EndfieldInput::Put(source,22,0xA7F49310,4);EndfieldInput::Put(source,26,info.first,2);EndfieldInput::Put(source,32,info.second,2);EndfieldInput::Put(source,34,size.first,4);EndfieldInput::Put(source,38,info.first,2);
 return EndfieldInput::Encode(source,{{name,0,member}});
}
struct Fixture {
 std::vector<Type> types;std::map<int64_t,std::pair<size_t,Value>> objects;SerializedLayout refs;std::string cab="CAB-static";
 Value& Add(int64_t id,size_t type){return objects.emplace(id,std::make_pair(type,EmptyValue(types.at(type),0))).first->second.second;}
 const Type& T(int64_t id){return types.at(objects.at(id).first);}
 Value& V(int64_t id){return objects.at(id).second;}
 void Pointer(int64_t id,const char* field,int64_t target){SetReference(T(id),Child(T(id),V(id),field),{cab,target},refs,cab);}
  void List(int64_t id,const char* name,std::initializer_list<int64_t> targets,const char* field=nullptr){auto& a=ArrayValue(T(id),Child(T(id),V(id),name));for(auto target:targets){auto v=ArrayElement(T(id),a);SetReference(T(id),field?Child(T(id),v,field):v,{cab,target},refs,cab);a.children.push_back(v);}a.count=a.children.size();}
  void SetList(int64_t id,const char* name,std::initializer_list<int64_t> targets,const char* field=nullptr){auto& a=ArrayValue(T(id),Child(T(id),V(id),name));a.children.clear();a.raw.clear();a.rawOrigins.clear();a.count=0;List(id,name,targets,field);}
 Bytes BytesOf(){Bytes m;Z(m,"fixture");N(m,0,4);N(m,1,1);N(m,types.size(),4);
  for(const auto& t:types){N(m,t.classId,4);N(m,0,1);N(m,65535,2);if(t.classId==114)N(m,0,8),N(m,0,8);m.insert(m.end(),t.hash.begin(),t.hash.end());Bytes strings,nodes;
   auto emit=[&](auto&& self,size_t n,unsigned depth)->void{const auto& f=t.nodes[n];auto offType=strings.size();Z(strings,f.type);auto offName=strings.size();Z(strings,f.name);N(nodes,1,2);N(nodes,depth,1);N(nodes,0,1);N(nodes,offType,4);N(nodes,offName,4);N(nodes,0,8);N(nodes,f.align?0x4000:0,4);N(nodes,0,8);for(auto c:f.children)self(self,c,depth+1);};emit(emit,0,0);
   N(m,t.nodes.size(),4);N(m,strings.size(),4);m.insert(m.end(),nodes.begin(),nodes.end());m.insert(m.end(),strings.begin(),strings.end());N(m,0,4);
  }N(m,objects.size(),4);while(m.size()%4)m.push_back(0);Bytes data;
  for(const auto& o:objects){while(data.size()%8)data.push_back(0);auto bytes=EncodeValue(types[o.second.first],o.second.second);N(m,o.first,8);N(m,data.size(),8);N(m,bytes.size(),4);N(m,o.second.first,4);data.insert(data.end(),bytes.begin(),bytes.end());}
  N(m,0,4);N(m,refs.externals.size(),4);for(const auto& external:refs.externals){Z(m,"");m.insert(m.end(),20,0);Z(m,"archive:/"+external+"/"+external);}
  Bytes b(48);b.insert(b.end(),m.begin(),m.end());while(b.size()%16)b.push_back(0);Be(b,20,m.size(),4);Be(b,32,b.size(),8);b.insert(b.end(),data.begin(),data.end());Be(b,24,b.size(),8);ParseSerialized(b);return b;
 }
 Fixture(){auto info=FieldSpec{"AssetInfo","second",{{"int","preloadIndex",{}},{"int","preloadSize",{}},Ref("asset")}};
  types={Schema(1,{{"string","m_Name",{}},Arr("m_Component",{"ComponentPair","data",{Ref("component")}})}),
   Schema(4,{Ref("m_GameObject"),Ref("m_Father"),Arr("m_Children",Ref("data")),Vec("m_LocalPosition"),Vec("m_LocalScale"),Vec("m_LocalRotation",true)}),
   Schema(33,{Ref("m_GameObject"),Ref("m_Mesh")}),Schema(23,{Ref("m_GameObject"),{"bool","m_Enabled",{}},Arr("m_Materials",Ref("data")),Ref("m_ProbeAnchor"),Ref("m_PerRendererLightingAnchor"),{"StaticBatchInfo","m_StaticBatchInfo",{{"UInt16","firstSubMesh",{}},{"UInt16","subMeshCount",{}}}},Ref("m_StaticBatchRoot"),Ref("m_AdditionalVertexStreams")}),
   Schema(142,{Arr("m_PreloadTable",Ref("data")),Arr("m_Container",{"pair","data",{{"string","first",{}},info}}),Arr("m_HashContainer",{"pair","data",{{"int","first",{}},info}}),{"AssetInfo","m_MainAsset",info.children}}),
   Schema(43,{{"string","m_Name",{}}}),Schema(21,{{"string","m_Name",{}}}),
   Schema(205,{Ref("m_GameObject"),Arr("m_LODs",{"LOD","data",{{"float","screenRelativeHeight",{}},Arr("renderers",{"RendererData","data",{Ref("renderer")}})}})}),
   Schema(114,{Ref("m_GameObject"),{"bool","useCustomRenderers",{}},Arr("customRenderers",Ref("data"))})};
  for(int64_t base:{10,20}){Add(base,0);SetString(T(base),Child(T(base),V(base),"m_Name"),base==10?"A":"B");Add(base+1,1);Pointer(base+1,"m_GameObject",base);SetVector(T(base+1),Child(T(base+1),V(base+1),"m_LocalScale"),{1,1,1});Add(base+2,2);Pointer(base+2,"m_GameObject",base);Pointer(base+2,"m_Mesh",100);Add(base+3,3);Pointer(base+3,"m_GameObject",base);SetInteger(T(base+3),Child(T(base+3),V(base+3),"m_Enabled"),1);List(base+3,"m_Materials",{200});List(base,"m_Component",{base+1,base+2,base+3},"component");}
  Add(100,5);SetString(T(100),Child(T(100),V(100),"m_Name"),"Static");Add(200,6);Add(300,4);List(300,"m_PreloadTable",{10,11,12,13,20,21,22,23,100,200});SetInteger(T(300),Child(T(300),Child(T(300),V(300),"m_MainAsset"),"preloadSize"),10);
  auto& container=ArrayValue(T(300),Child(T(300),V(300),"m_Container"));for(auto row:std::vector<std::pair<std::string,int64_t>>{{"assets/a.prefab",10},{"assets/b.prefab",20},{"assets/static.mesh",100}}){auto entry=ArrayElement(T(300),container);SetString(T(300),Child(T(300),entry,"first"),row.first);auto& infoValue=Child(T(300),entry,"second");SetReference(T(300),Child(T(300),infoValue,"asset"),{cab,row.second},refs,cab);SetInteger(T(300),Child(T(300),infoValue,"preloadSize"),10);container.children.push_back(entry);}container.count=container.children.size();
  SetInteger(T(13),Child(T(13),Child(T(13),V(13),"m_StaticBatchInfo"),"subMeshCount"),1);Pointer(13,"m_StaticBatchRoot",11);Pointer(13,"m_AdditionalVertexStreams",100);
  Add(14,7);Pointer(14,"m_GameObject",10);auto& lods=ArrayValue(T(14),Child(T(14),V(14),"m_LODs"));auto level=ArrayElement(T(14),lods);auto& rs=ArrayValue(T(14),Child(T(14),level,"renderers"));auto r=ArrayElement(T(14),rs);SetReference(T(14),Child(T(14),r,"renderer"),{cab,13},refs,cab);rs.children={r};rs.count=1;lods.children={level};lods.count=1;
  Add(15,8);Pointer(15,"m_GameObject",10);List(15,"customRenderers",{13});SetInteger(T(15),Child(T(15),V(15),"useCustomRenderers"),1);List(10,"m_Component",{14,15,0},"component");
  Add(30,0);SetString(T(30),Child(T(30),V(30),"m_Name"),"UnusedFilter");Add(31,1);Pointer(31,"m_GameObject",30);Add(32,2);Pointer(32,"m_GameObject",30);Pointer(32,"m_Mesh",100);List(30,"m_Component",{31,32},"component");
 }
};
template<class F> void Rejected(F f){bool rejected=false;try{f();}catch(const std::exception&){rejected=true;}Require(rejected,"invalid static assembly accepted");}
void PerRendererMaterialDonorRegression(){
 Fixture f;f.Add(201,6);f.SetList(13,"m_Materials",{200,201});f.SetList(23,"m_Materials",{});
 Package p;p.id="per-renderer-material";Identity target{"assets/static.mesh","Static"};p.targetIndex[target]="R";p.rules["R"].parts={"P","Q"};
 p.parts["P"].mesh=p.parts["Q"].mesh="M";p.parts["P"].material=p.parts["Q"].material="A";p.meshes["M"]={"mesh",3,3,{},{}};
 auto meshSchema=std::make_shared<Resource>();meshSchema->type=f.types[5].hash;p.resources["mesh"]=meshSchema;
 AssetIndex assets;assets[target].cab=f.cab;assets[target].pathId=100;assets[target].classId=43;assets[target].type=meshSchema->type;
 std::vector<BuiltMeshPart> parts{{target,"R","P",f.cab,100,100,3,3},{target,"R","Q",f.cab,100,101,3,3}};
 RendererMaterialReferences references;for(auto renderer:{int64_t(13),int64_t(23)})for(const auto& part:std::vector<std::string>{"P","Q"})
  references[{renderer,part}]={f.cab,renderer==13?201:200};
 auto build=[&](){auto input=f.BytesOf();RendererMemberBuilder builder(input,f.cab,p,assets,parts);builder.SetMaterialReferences(references);return builder.Build();};
 auto result=build();Require(result.matches==2&&result.parts.size()==4,"one Mesh hit did not expand multiple source Renderers/author Parts");
 for(const auto& part:result.parts){const auto expected=part.sourceRenderer==13?201:200;Require(part.sourceRenderer==13||part.sourceRenderer==23,"unexpected source Renderer in Material mapping regression");
  auto layout=ParseSerialized(result.bytes);auto value=ObjectValue(result.bytes,layout,part.renderer);const auto& type=layout.types.at(layout.objects.at(part.renderer).type);const auto& materials=ConstArray(type,Child(type,value,"m_Materials"));
  Require(materials.children.size()==1&&NullableReference(type,materials.children.front(),layout,f.cab)==NativeReference{f.cab,expected},"author Renderer did not use its explicit Material input independent of source slot count");}
 auto preserved=f.BytesOf();RendererMemberBuilder sourceBuilder(preserved,f.cab,p,assets,parts);sourceBuilder.UseSourceMaterials();auto sourceResult=sourceBuilder.Build();
 auto sourceLayout=ParseSerialized(sourceResult.bytes);auto sourceMaterials=[&](int64_t renderer){auto value=ObjectValue(sourceResult.bytes,sourceLayout,renderer);const auto& type=sourceLayout.types.at(sourceLayout.objects.at(renderer).type);
  std::vector<NativeReference> refs;for(const auto& item:ConstArray(type,Child(type,value,"m_Materials")).children)refs.push_back(NullableReference(type,item,sourceLayout,f.cab));return refs;};
  const auto source13=sourceMaterials(13),source23=sourceMaterials(23);
  if(source13!=std::vector<NativeReference>{{f.cab,200},{f.cab,201}}||!source23.empty()){
   auto describe=[&](const std::vector<NativeReference>& refs){std::string text="[";for(const auto& ref:refs)text+=ref.first+":"+std::to_string(ref.second)+",";return text+"]";};
   throw std::runtime_error("unchanged source Renderer material arrays were not preserved: renderer13="+describe(source13)+" renderer23="+describe(source23));
  }
 std::cout<<"PASS Mesh hit/material mapping: explicit full Material input ignores source Renderer slot count; unchanged Renderer arrays preserve 0/1/N slots\n";
}
void CrossCabSkipCaseRegression(){
 Fixture f;const std::string external="cab-external";
 SetReference(f.T(12),Child(f.T(12),f.V(12),"m_Mesh"),{external,100},f.refs,f.cab);
 Package p;p.id="cross-cab-skip";Identity target{"assets/hidden.mesh","Static"};p.targetIndex[target]="R";p.rules["R"].skip=true;
 AssetIndex assets;assets[target].cab="CAB-External";assets[target].pathId=100;assets[target].classId=43;
 auto input=f.BytesOf();RendererMemberBuilder builder(input,f.cab,p,assets,{});auto hidden=builder.Build();
 Require(hidden.matches==1&&hidden.skipped==1,"cross-CAB handling=skip did not match the external Mesh identity");
 auto layout=ParseSerialized(hidden.bytes);auto renderer=ObjectValue(hidden.bytes,layout,13);
 Require(Integer(Child(layout.types.at(layout.objects.at(13).type),renderer,"m_Enabled"))==0,
         "cross-CAB handling=skip did not serialize m_Enabled=0");
 Fixture skinned;skinned.types.push_back(Schema(137,{Ref("m_GameObject"),Ref("m_Mesh"),{"bool","m_Enabled",{}}}));
 skinned.objects.at(13)={skinned.types.size()-1,EmptyValue(skinned.types.back(),0)};
 skinned.Pointer(13,"m_GameObject",10);skinned.Pointer(12,"m_Mesh",0);
 SetReference(skinned.T(13),Child(skinned.T(13),skinned.V(13),"m_Mesh"),{external,100},skinned.refs,skinned.cab);
 auto skinnedInput=skinned.BytesOf();RendererMemberBuilder skinnedBuilder(skinnedInput,skinned.cab,p,assets,{});auto skinnedHidden=skinnedBuilder.Build();
 Require(skinnedHidden.matches==1&&skinnedHidden.skipped==1,"cross-CAB handling=skip did not match a SkinnedMeshRenderer Mesh PPtr");
 auto skinnedLayout=ParseSerialized(skinnedHidden.bytes);auto skinnedRenderer=ObjectValue(skinnedHidden.bytes,skinnedLayout,13);
 Require(Integer(Child(skinnedLayout.types.at(skinnedLayout.objects.at(13).type),skinnedRenderer,"m_Enabled"))==0,
         "cross-CAB handling=skip did not disable SkinnedMeshRenderer");
 std::cout<<"PASS handling=skip matches cross-CAB static and skinned Mesh PPtrs independent of CAB case\n";
}
void NativePaletteFallbackRegression(){
 // Real failure pattern: source Mesh has [Head,Lip,LeftEye,Neck,RightEye],
 // while the serialized Renderer retains only [Head,Neck]. It is not a prefix.
 Fixture f;
 f.types[5]=Schema(43,{{"string","m_Name",{}},Arr("m_BoneNameHashes",{"UInt32","data",{}})});
 f.objects.at(100)={5,EmptyValue(f.types[5],0)};f.Add(102,5);
 for(auto filter:{12,22,32})f.Pointer(filter,"m_Mesh",102);
 f.types.push_back(Schema(137,{Ref("m_GameObject"),Ref("m_Mesh"),
  {"bool","m_Enabled",{}},Arr("m_Materials",Ref("data")),Arr("m_Bones",Ref("data")),
  Ref("m_RootBone"),Ref("m_SkinningRoot"),Arr("m_BlendShapeWeights",{"float","data",{}}),
  {"bool","m_DirtyAABB",{}},{"AABB","m_AABB",{Vec("m_Center"),Vec("m_Extent")}}}));
 f.objects.at(13)={f.types.size()-1,EmptyValue(f.types.back(),0)};
 f.Pointer(13,"m_GameObject",10);f.Pointer(13,"m_Mesh",100);f.Pointer(13,"m_RootBone",71);
 f.List(13,"m_Bones",{71,51});f.List(13,"m_Materials",{200});
 auto node=[&](int64_t go,int64_t tr,int64_t parent,const char* name){
  f.Add(go,0);SetString(f.T(go),Child(f.T(go),f.V(go),"m_Name"),name);
  f.Add(tr,1);f.Pointer(tr,"m_GameObject",go);f.Pointer(tr,"m_Father",parent);
  SetVector(f.T(tr),Child(f.T(tr),f.V(tr),"m_LocalScale"),{1,1,1});
  SetFloat(f.T(tr),Child(f.T(tr),Child(f.T(tr),f.V(tr),"m_LocalRotation"),"w"),1);
  f.List(go,"m_Component",{tr},"component");
  if(parent)f.List(parent,"m_Children",{tr});
 };
 node(40,41,0,"Timeline");f.Pointer(11,"m_Father",41);f.Pointer(21,"m_Father",41);f.List(41,"m_Children",{11,21});
 node(50,51,11,"Neck");node(70,71,51,"Head");node(80,81,71,"Lip");
 node(110,111,71,"LeftEye");node(120,121,71,"RightEye");
 // The second actor has the same bone paths under the same timeline root.
 node(150,151,21,"Neck");node(170,171,151,"Head");node(180,181,171,"Lip");
 node(210,211,171,"LeftEye");node(220,221,171,"RightEye");
 Package p;p.id="palette";Identity target{"assets/face.mesh","Face"},mat{"assets/material.mat","Mat"};
 p.targetIndex[target]="R";p.rules["R"].parts={"P"};p.parts["P"].mesh="M";p.parts["P"].material="A";p.materials["A"].source=mat;
 p.meshes["M"]={"mesh",3,3,{"Neck/Head/Lip","Neck","Neck/Head","Neck/Head/RightEye","Neck/Head/LeftEye"},{}};
 auto resource=std::make_shared<Resource>();resource->type=f.types[5].hash;
 auto hashes=EmptyValue(f.types[5],0);auto& array=ArrayValue(f.types[5],Child(f.types[5],hashes,"m_BoneNameHashes"));
 for(auto hash:{22,44,11,55,33})AppendInteger(array.raw,hash,4);array.count=5;
 resource->bytes=EncodeValue(f.types[5],Child(f.types[5],hashes,"m_BoneNameHashes"));
 resource->blocks["m_BoneNameHashes"]={0,resource->bytes.size()};p.resources["mesh"]=resource;
 AssetIndex assets;assets[target].cab=assets[mat].cab=f.cab;assets[target].pathId=100;assets[target].classId=43;
 assets[target].type=resource->type;assets[mat].pathId=200;assets[mat].classId=21;
 std::vector<BuiltMeshPart> parts{{target,"R","P",f.cab,100,100,3,3,{11,22,33,44,55}}};
 MeshSchemaIndex schemas{{resource->type,f.types[5]}};
 RendererBoundsCache bounds;bounds.values.emplace(std::make_tuple(std::string("mesh"),size_t(2),AffineIdentity()),
  std::make_pair(Vector3{0,0,0},Vector3{1,1,1}));
 auto build=[&](){auto input=f.BytesOf();RendererMemberBuilder builder(input,f.cab,p,assets,parts);
  builder.SetMeshSchemas(schemas);builder.SetBoundsCache(bounds);return builder.Build();};
 auto check=[&](){auto result=build();Require(result.parts.size()==1&&result.parts[0].bones==std::vector<int64_t>({81,51,71,121,111}),
  "incomplete native palette used prefix slots or another actor's skeleton");
  auto layout=ParseSerialized(result.bytes);auto renderer=ObjectValue(result.bytes,layout,13);const auto& type=layout.types.at(layout.objects.at(13).type);
  std::vector<int64_t> actual;for(const auto& bone:ConstArray(type,Child(type,renderer,"m_Bones")).children)
   actual.push_back(NullableReference(type,bone,layout,f.cab).second);
  Require(actual==result.parts[0].bones,"resolved fallback palette was not written into the candidate");};
 check();
 // A missing local path must fail even if an identical path exists on actor B.
 SetString(f.T(80),Child(f.T(80),f.V(80),"m_Name"),"OtherLip");Rejected(build);
 SetString(f.T(80),Child(f.T(80),f.V(80),"m_Name"),"Lip");
 node(230,231,71,"Lip");Rejected(build);
 SetString(f.T(230),Child(f.T(230),f.V(230),"m_Name"),"OtherLip");check();
 // Longer and empty source palettes also offer no reliable slot mapping.
 auto& palette=ArrayValue(f.T(13),Child(f.T(13),f.V(13),"m_Bones"));palette.children.clear();palette.count=0;check();
 f.List(13,"m_Bones",{71,51,81,111,121,71});check();
 palette.children.clear();palette.count=0;f.List(13,"m_Bones",{71,81,111,51,121});check();
 std::cout<<"PASS incomplete native palette: exact paths, reordered author slots, written candidate, empty/longer palettes, missing/ambiguous paths and sibling actor isolation\n";
}
int main(){try{
 PerRendererMaterialDonorRegression();
 NativePaletteFallbackRegression();
 Fixture f;auto before=f.BytesOf();Package p;p.id="static";Identity target{"assets/static.mesh","Static"},mat{"assets/material.mat","Mat"};p.targetIndex[target]="R";p.rules["R"].parts={"P","Q"};
 // Shared member parsing must keep root ownership and component ordinals local.
 auto shared=std::make_shared<const EiemRenderAssemblyDescription::GraphData>(before,f.cab);
 EiemRenderAssemblyDescription::Graph firstRoot(shared,10),secondRoot(shared,20);
 Require(firstRoot.Renderers()==std::vector<int64_t>{13}&&secondRoot.Renderers()==std::vector<int64_t>{23},"shared member analysis mixed Prefab roots");
 auto ownershipLayout=ParseSerialized(before);EiemPrefabConsumers::Ownership ownership(before,ownershipLayout,f.cab);
 Require(ownership.GameObjects(10)==std::set<int64_t>{10}&&ownership.Owner(13)==10&&ownership.Owner(23)==20,"lightweight ownership differs from exact roots");
 Fixture repeatedChild;repeatedChild.List(11,"m_Children",{21,21});repeatedChild.Pointer(21,"m_Father",11);
 auto repeatedBytes=repeatedChild.BytesOf();auto repeatedLayout=ParseSerialized(repeatedBytes);
 Rejected([&]{EiemPrefabConsumers::Ownership index(repeatedBytes,repeatedLayout,f.cab);index.GameObjects(10);});
 Fixture wrongParent;wrongParent.List(11,"m_Children",{21});auto wrongBytes=wrongParent.BytesOf();auto wrongLayout=ParseSerialized(wrongBytes);
 Rejected([&]{EiemPrefabConsumers::Ownership index(wrongBytes,wrongLayout,f.cab);index.GameObjects(10);});
 auto truncated=before;truncated.resize(ownershipLayout.objects.at(13).offset+ownershipLayout.objects.at(13).size-1);
 Rejected([&]{ObjectFields(truncated,ownershipLayout,13,{"m_GameObject"});});
 auto folder=std::filesystem::temp_directory_path()/("eff-static-mesh-"+std::to_string(GetCurrentProcessId()));Require(!std::filesystem::exists(folder),"fixture directory exists");std::filesystem::create_directory(folder);
 struct Cleanup {std::filesystem::path path;~Cleanup(){std::filesystem::remove_all(path);}}cleanup{folder};auto bundle=Bundle(before,f.cab);auto file=folder/"input.vfs";EiemResourceReloadPlan::Save(file,bundle);
 EiemStaticSources::Selection selection;selection.manifest="fixture";selection.assets[target.path]="input";selection.inputs["input"]={"input",EiemResourceReloadPlan::Hash(bundle),file,0,bundle.size()};selection.candidates={"input"};auto found=EiemStaticSources::FindConsumers(selection,{target});Require(found.renderers==2&&found.roots==std::set<std::string>{"assets/a.prefab","assets/b.prefab"},"offline static consumers/PFB roots differ");
 Require(found.decisions.at("input").roots.size()==2,"offline discovery discarded same-Bundle root observations");
 Fixture negative;negative.Add(102,5);negative.Pointer(12,"m_Mesh",102);negative.Pointer(22,"m_Mesh",102);negative.Pointer(32,"m_Mesh",102);
 auto negativeBundle=Bundle(negative.BytesOf(),negative.cab);auto negativeFile=folder/"negative.vfs";EiemResourceReloadPlan::Save(negativeFile,negativeBundle);
 auto withNegative=selection;withNegative.inputs["negative"]={"negative",EiemResourceReloadPlan::Hash(negativeBundle),negativeFile,0,negativeBundle.size()};withNegative.candidates.insert("negative");
 auto conclusions=EiemStaticSources::FindConsumers(withNegative,{target});Require(conclusions.decisions.size()==2&&conclusions.decisions.at("negative").roots.empty(),"offline discovery discarded negative conclusion");
 auto persisted=EiemConsumerUpdateRecords::Decode(EiemConsumerUpdateRecords::Encode(conclusions.decisions));std::map<std::string,EiemConsumerUpdateRecords::Stamp> currentStamps;
 for(const auto& row:conclusions.decisions)currentStamps.emplace(row.first,row.second.stamp);
 Require(EiemConsumerUpdateRecords::Pending(currentStamps,persisted).empty(),"offline conclusions rechecked unchanged candidates");
 // Independent inputs may finish out of order. Keep their complete consumer
 // and root sets, including multiple Prefabs in one serialized member.
 auto multiple=selection;multiple.inputs["second"]=multiple.inputs.at("input");multiple.inputs["second"].logical="second";multiple.candidates.insert("second");
 auto repeated=EiemStaticSources::FindConsumers(multiple,{target});Require(repeated.roots==found.roots&&repeated.renderers==4&&repeated.sources==std::set<std::string>{"input","second"},"parallel source discovery omitted a candidate");
 auto invalid=multiple;invalid.inputs["second"].revision=std::string(64,'0');Rejected([&]{EiemStaticSources::FindConsumers(invalid,{target});});
 auto resource=std::make_shared<Resource>();resource->type=f.types[5].hash;p.resources["mesh"]=resource;p.meshes["M"]={"mesh",3,3,{},{}};p.materials["A"].source=mat;p.parts["P"].mesh=p.parts["Q"].mesh="M";p.parts["P"].material=p.parts["Q"].material="A";
 AssetIndex assets;assets[target].cab=assets[mat].cab=f.cab;assets[target].pathId=100;assets[target].classId=43;assets[target].type=resource->type;assets[mat].pathId=200;assets[mat].classId=21;
 std::vector<BuiltMeshPart> parts{{target,"R","P",f.cab,100,100,3,3},{target,"R","Q",f.cab,100,101,3,3}};
 auto direct=EndfieldInput::Decode(bundle);auto directResult=BuildRendererInput(direct,p,assets,parts,{});
 auto encodedResult=BuildRendererInput(bundle,p,assets,parts,{});
 Require(EndfieldInput::Encode(bundle,direct.members)==encodedResult.bytes&&directResult.parts.size()==encodedResult.parts.size()&&directResult.changedObjects==encodedResult.changedObjects,"in-memory Renderer stage differs from encoded stage");
 auto built=RendererMemberBuilder(before,f.cab,p,assets,parts).Build();Require(built.matches==2&&built.parts.size()==4&&built.addedObjects==8,"static Mesh consumers/one-to-many assembly missing");
 auto adapted=std::make_shared<const EiemRenderAssemblyDescription::GraphData>(built.bytes,f.cab,shared.get(),&before);
 auto reparsed=std::make_shared<const EiemRenderAssemblyDescription::GraphData>(built.bytes,f.cab);
 Require(adapted->reusedObjects&&adapted->decodedObjects&&adapted->decodedObjects<reparsed->decodedObjects,"modified member did not reuse unchanged assembly objects");
 EiemRenderAssemblyDescription::Graph sharedCandidate(adapted,10),coldCandidate(reparsed,10);
 std::map<NativeReference,std::string> reuseMarkers{{{f.cab,100},"first"},{{f.cab,101},"second"}};
 auto sharedDescription=EiemRenderAssemblyDescription::Build(firstRoot,sharedCandidate,f.cab,p,assets,reuseMarkers);
 auto coldDescription=EiemRenderAssemblyDescription::Build(firstRoot,coldCandidate,f.cab,p,assets,reuseMarkers);
 EiemOfflinePackage::Writer sharedAssembly,coldAssembly;EiemOfflinePackage::WriteAssembly(sharedAssembly,sharedDescription);EiemOfflinePackage::WriteAssembly(coldAssembly,coldDescription);
 Require(sharedAssembly.b==coldAssembly.b,"reused assembly analysis changed Renderer/LOD/helper output");
 auto changedSchema=f;for(auto& node:changedSchema.types[0].nodes)if(node.name=="m_Name")node.name="differentName";
 auto schemaBytes=changedSchema.BytesOf();auto schemaAnalysis=std::make_shared<const EiemRenderAssemblyDescription::GraphData>(schemaBytes,f.cab,shared.get(),&before);
 Require(schemaAnalysis->values.at(10)!=shared->values.at(10),"same schema hash hid changed field structure during assembly reuse");
 // A model Prefab can contain UI children. RectTransform inherits the same
 // hierarchy fields; preserve these nodes while discovering/rebuilding Meshes.
 Fixture rect;auto rectType=rect.types[1];rectType.classId=224;rectType.hash[0]=224;rect.types.push_back(rectType);
 rect.Add(40,0);SetString(rect.T(40),Child(rect.T(40),rect.V(40),"m_Name"),"UI");
 rect.Add(41,rect.types.size()-1);rect.Pointer(41,"m_GameObject",40);rect.Pointer(41,"m_Father",11);
 rect.List(40,"m_Component",{41},"component");rect.List(11,"m_Children",{41});
 auto rectBytes=rect.BytesOf();EiemRenderAssemblyDescription::Graph rectGraph(rectBytes,rect.cab,10);
 Require(rectGraph.Contains(41)&&rectGraph.Path(41)==EiemRenderAssemblyDescription::Segment("UI",0),"RectTransform child omitted from Prefab addresses");
 auto rectBuilt=RendererMemberBuilder(rectBytes,rect.cab,p,assets,parts).Build();
 Require(rectBuilt.matches==2&&rectBuilt.parts.size()==4,"RectTransform child prevented Renderer assembly");
 EiemRenderAssemblyDescription::Graph rectAfter(rectBuilt.bytes,rect.cab,10);
 Require(rectAfter.Path(41)==rectGraph.Path(41),"Renderer assembly changed an unrelated RectTransform address");
 // Some native skinned consumers leave m_SkinningRoot empty. Their explicit
 // root/bone palette remains valid; preserve the optional native pointer.
 Fixture skin;skin.types.push_back(Schema(137,{Ref("m_GameObject"),Ref("m_Mesh"),
  {"bool","m_Enabled",{}},Arr("m_Materials",Ref("data")),Arr("m_Bones",Ref("data")),
  Ref("m_RootBone"),Ref("m_SkinningRoot"),Arr("m_BlendShapeWeights",{"float","data",{}}),
  {"bool","m_DirtyAABB",{}},{"AABB","m_AABB",{Vec("m_Center"),Vec("m_Extent")}}}));
 skin.objects.at(13)={skin.types.size()-1,EmptyValue(skin.types.back(),0)};
 skin.Pointer(13,"m_GameObject",10);skin.Pointer(13,"m_Mesh",100);skin.Pointer(13,"m_RootBone",11);
 skin.List(13,"m_Bones",{11});skin.List(13,"m_Materials",{200});
 skin.Pointer(12,"m_Mesh",102);skin.Pointer(22,"m_Mesh",102);skin.Add(102,5);
 auto skinnedPackage=p;skinnedPackage.meshes["M"].bones={""};
 RendererBoundsCache skinBounds;skinBounds.values.emplace(std::make_tuple(std::string("mesh"),size_t(0),AffineIdentity()),
  std::make_pair(Vector3{0,0,0},Vector3{1,1,1}));
 auto buildSkin=[&](){auto input=skin.BytesOf();RendererMemberBuilder builder(input,skin.cab,skinnedPackage,assets,parts);
  builder.SetBoundsCache(skinBounds);return builder.Build();};
 auto emptySkin=buildSkin();Require(emptySkin.matches==1&&emptySkin.parts.size()==2,"null optional skinning root prevented skinned assembly");
 auto emptyLayout=ParseSerialized(emptySkin.bytes);
 for(const auto& part:emptySkin.parts){const auto& t=emptyLayout.types.at(emptyLayout.objects.at(part.renderer).type);
  auto value=ObjectValue(emptySkin.bytes,emptyLayout,part.renderer);
  Require(NullableReference(t,Child(t,value,"m_SkinningRoot"),emptyLayout,skin.cab).second==0,"null native skinning root was synthesized");}
 skin.Pointer(13,"m_SkinningRoot",11);auto explicitSkin=buildSkin();auto explicitLayout=ParseSerialized(explicitSkin.bytes);
 for(const auto& part:explicitSkin.parts){const auto& t=explicitLayout.types.at(explicitLayout.objects.at(part.renderer).type);
  auto value=ObjectValue(explicitSkin.bytes,explicitLayout,part.renderer);
  Require(NullableReference(t,Child(t,value,"m_SkinningRoot"),explicitLayout,skin.cab)==NativeReference{skin.cab,11},"explicit native skinning root changed");}
 skin.Pointer(13,"m_SkinningRoot",10);Rejected(buildSkin);
 skin.Pointer(13,"m_SkinningRoot",0);skin.Pointer(13,"m_RootBone",0);Rejected(buildSkin);
 auto crossAssets=assets;const std::string materialCab="CAB-"+std::string(32,'c');crossAssets.at(mat).cab=materialCab;
 auto cross=RendererMemberBuilder(before,f.cab,p,crossAssets,parts).Build();auto crossLayout=ParseSerialized(cross.bytes);
 Require(crossLayout.externals==std::vector<std::string>{materialCab},"cross-CAB Material was not registered exactly once");
 const NativeReference inheritedTexture{"CAB-"+std::string(32,'d'),91}, inheritedShader{"CAB-"+std::string(32,'e'),92};
 MaterialPreloadIndex borrowed{{{materialCab,200},{inheritedTexture,inheritedShader}}};
 RendererMemberBuilder borrowedBuilder(before,f.cab,p,crossAssets,parts);borrowedBuilder.SetMaterialPreloads(borrowed);
 auto withDependencies=borrowedBuilder.Build();auto dependencyLayout=ParseSerialized(withDependencies.bytes);
 auto dependencyBundle=ObjectValue(withDependencies.bytes,dependencyLayout,300);const auto& dependencyType=dependencyLayout.types.at(dependencyLayout.objects.at(300).type);
 std::set<NativeReference> dependencyRefs;for(const auto& ref:ConstArray(dependencyType,Child(dependencyType,dependencyBundle,"m_PreloadTable")).children)
  dependencyRefs.insert(NullableReference(dependencyType,ref,dependencyLayout,f.cab));
 Require(dependencyRefs.count(inheritedTexture)&&dependencyRefs.count(inheritedShader),"borrowed Material lost inherited Texture/Shader preloads");
 for(const auto& part:cross.parts){auto value=ObjectValue(cross.bytes,crossLayout,part.renderer);const auto& type=crossLayout.types.at(crossLayout.objects.at(part.renderer).type);
  Require(NullableReference(type,ConstArray(type,Child(type,value,"m_Materials")).children.front(),crossLayout,f.cab)==NativeReference{materialCab,part.material},"cross-CAB Material FileID/PathID differs");}
 EiemRenderAssemblyDescription::Graph g(built.bytes,f.cab,10);Require(g.Mesh(13)==NativeReference{f.cab,100},"native static MeshFilter changed identity");auto extra=g.RendererChildren(13);Require(extra.size()==1&&g.Mesh(extra[0])==NativeReference{f.cab,101},"static extension MeshFilter missing");g.RequirePure(extra[0]);
 Require(g.Pointer(13,"m_AdditionalVertexStreams")==0&&g.Pointer(13,"m_StaticBatchRoot")==0&&Integer(Child(g.Schema(13),Child(g.Schema(13),g.Tree(13),"m_StaticBatchInfo"),"subMeshCount"))==0,"static donor batch/vertex stream survived geometry replacement");
 auto next=ParseSerialized(built.bytes);for(const auto& part:built.parts){auto renderer=ObjectValue(built.bytes,next,part.renderer);auto ref=NullableReference(next.types[next.objects.at(part.renderer).type],ConstArray(next.types[next.objects.at(part.renderer).type],Child(next.types[next.objects.at(part.renderer).type],renderer,"m_Materials")).children[0],next,f.cab);Require(ref==NativeReference{f.cab,part.material},"static material clone binding differs");}
 std::map<NativeReference,std::string> markers{{{f.cab,100},"first"},{{f.cab,101},"second"}};auto d=EiemRenderAssemblyDescription::Build(before,built.bytes,f.cab,10,p,assets,markers);Require(d.sources.size()==1&&d.sources[0].targets.size()==2&&d.lods[0].levels[0].renderers.size()==2&&d.helpers[0].renderers.size()==2,"static descriptor/helper/LOD expansion missing");
 p.rules["R"].skip=true;auto hidden=RendererMemberBuilder(before,f.cab,p,assets,parts).Build();Require(hidden.skipped==2&&hidden.addedObjects==0,"static skip did not preserve native objects");
 EiemRenderAssemblyDescription::Graph hiddenGraph(hidden.bytes,f.cab,10);Require(!hiddenGraph.Enabled(13),"handling=skip did not serialize m_Enabled=0");
 auto skip=EiemRenderAssemblyDescription::Build(before,hidden.bytes,f.cab,10,p,assets,{});Require(skip.sources[0].targets.empty()&&skip.lods[0].levels[0].renderers.empty(),"static zero-part list mapping wrong");
 p.rules["R"].skip=false;p.meshes["M"].bones={"fake"};Rejected([&]{RendererMemberBuilder(before,f.cab,p,assets,parts).Build();});p.meshes["M"].bones.clear();p.meshes["M"].shapes={"shape"};Rejected([&]{RendererMemberBuilder(before,f.cab,p,assets,parts).Build();});p.meshes["M"].shapes.clear();
 p.rules["R"].parts={"P"};auto one=RendererMemberBuilder(before,f.cab,p,assets,parts).Build();Require(one.matches==2&&one.addedObjects==0&&one.parts.size()==2,"static one-to-one added unnecessary objects");
 auto untouched=p;untouched.targetIndex.clear();Require(RendererMemberBuilder(before,f.cab,untouched,assets,parts).Build().bytes==before,"unspecified static Mesh changed");
 f.types.push_back(Schema(137,{Ref("m_GameObject"),Ref("m_Mesh"),Arr("m_Bones",Ref("data")),Ref("m_RootBone"),Ref("m_SkinningRoot")}));f.Add(16,f.types.size()-1);f.Pointer(16,"m_GameObject",10);f.Pointer(16,"m_Mesh",102);f.Add(102,5);f.List(10,"m_Component",{16},"component");auto mixedBefore=f.BytesOf();EiemRenderAssemblyDescription::Graph mixed(mixedBefore,f.cab,10);Require(mixed.At(16).component==0&&mixed.At(13).component==1,"mixed Renderer node changed legacy SMR address");
 p.rules["R"].parts={"P","Q"};auto mixedBuilt=RendererMemberBuilder(mixedBefore,f.cab,p,assets,parts).Build();auto mixedDescription=EiemRenderAssemblyDescription::Build(mixedBefore,mixedBuilt.bytes,f.cab,10,p,assets,markers);Require(mixedDescription.sources.size()==2,"mixed native Renderer missing");for(const auto& source:mixedDescription.sources)Require(source.targets.size()==(source.id==13?2:1),"mixed node assigned a static extension to another native Renderer");
 f.Pointer(13,"m_GameObject",20);Rejected([&]{RendererMemberBuilder(f.BytesOf(),f.cab,p,assets,parts).Build();});
 CrossCabSkipCaseRegression();
 std::cout<<"PASS static MeshFilter consumers, 0/1/N assembly, material clones, hierarchy, LOD/helper mapping, no fake skinning and invalid ownership rejection\n";return 0;
 }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}

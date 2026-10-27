#define main StaticFixtureMain
#include "static_mesh_assembly_smoke.cpp"
#undef main

int main(){try{
 Fixture f;
 auto environment=FieldSpec{"UnityTexEnv","second",{Ref("m_Texture")}};
 f.types[6]=Schema(21,{{"string","m_Name",{}},{"UnityPropertySheet","m_SavedProperties",{
  Arr("m_TexEnvs",{"pair","data",{{"string","first",{}},environment}})}}});
 f.objects.at(200)={6,EmptyValue(f.types[6],0)};
 SetString(f.T(200),Child(f.T(200),f.V(200),"m_Name"),"Mat");
 auto original=f.BytesOf();auto input=Bundle(original,f.cab);
 auto bundle=EndfieldInput::Decode(input);auto layout=ParseSerialized(original);
 AssetIndex assets;AddAssets(assets,SerializedReferencedAssets(original,f.cab,layout));
 Identity logical{"assets/a.prefab","Mat"};
 Require(assets.count(logical)&&assets.at(logical).pathId==200,"fixture must resolve a preload-only Material through its Prefab path");
 Package author;author.id="preload";
 auto resource=std::make_shared<Resource>();resource->kind=2;resource->type=f.types[6].hash;author.resources["material"]=resource;
 for(const char* section:{"A","B"}){author.materials[section].resource="material";author.materials[section].source=logical;}
 MaterialPreloadIndex closures;AddMaterialPreloads(closures,original,f.cab,layout,SerializedReferencedAssets(original,f.cab,layout),author);
 Require(closures.count({f.cab,200})&&closures.at({f.cab,200}).count({f.cab,100}),"preload-only Material did not inherit its Prefab dependency interval");
 auto result=BuildTextureInput(bundle,author,assets);
 auto after=ParseSerialized(bundle.members.at(0).bytes);
 Require(result.changedMaterials==2&&result.materialMatches==2,"preload-only Material aliases did not create one clone per author section");
 for(const char* section:{"A","B"}){
  auto clone=AllocateMaterialId(f.cab,author,section);
  Require(after.objects.count(clone)&&after.types.at(after.objects.at(clone).type).classId==21,"resolved Material clone absent from owning CAB");
 }
 const auto& beforeObject=layout.objects.at(200);const auto& afterObject=after.objects.at(200);
 Require(beforeObject.size==afterObject.size&&std::equal(original.begin()+beforeObject.offset,original.begin()+beforeObject.offset+beforeObject.size,bundle.members.at(0).bytes.begin()+afterObject.offset),"Material template was modified");
 auto current=ObjectValue(bundle.members.at(0).bytes,after,300);const auto& type=after.types.at(after.objects.at(300).type);
 std::set<int64_t> preloads;for(const auto& ref:ConstArray(type,Child(type,current,"m_PreloadTable")).children)preloads.insert(NullableReference(type,ref,after,f.cab).second);
 for(const char* section:{"A","B"})Require(preloads.count(AllocateMaterialId(f.cab,author,section)),"Material clone absent from Prefab preload closure");
 // Texture template inference must use the same resolved Material object,
 // even though its authored identity names a Prefab instead of a .mat root.
 f.types.push_back(Schema(28,{{"string","m_Name",{}}}));f.Add(201,f.types.size()-1);
 SetString(f.T(201),Child(f.T(201),f.V(201),"m_Name"),"Tex");f.List(300,"m_PreloadTable",{201});
 auto& entries=ArrayValue(f.T(300),Child(f.T(300),f.V(300),"m_Container"));
 for(auto& entry:entries.children)SetInteger(f.T(300),Child(f.T(300),Child(f.T(300),entry,"second"),"preloadSize"),11);
 auto& environments=ArrayValue(f.T(200),Child(f.T(200),Child(f.T(200),f.V(200),"m_SavedProperties"),"m_TexEnvs"));
 auto slot=ArrayElement(f.T(200),environments);SetString(f.T(200),Child(f.T(200),slot,"first"),"_MainTex");
 SetReference(f.T(200),Child(f.T(200),Child(f.T(200),slot,"second"),"m_Texture"),{f.cab,201},f.refs,f.cab);environments.children={slot};environments.count=1;
 auto templateBytes=f.BytesOf();AssetIndex templateIndex;AddAssets(templateIndex,SerializedReferencedAssets(templateBytes,f.cab,ParseSerialized(templateBytes)));
 auto templateAuthor=author;templateAuthor.materials.at("A").textures["_MainTex"]="T";templateAuthor.textures["T"]={};
 ResolveTextureTemplates(templateAuthor,{Bundle(templateBytes,f.cab)},templateIndex);
 const auto& texture=templateIndex.at(templateAuthor.textures.at("T").source);
 Require(texture.cab==f.cab&&texture.pathId==201,"preload-only Material failed to resolve its native Texture slot");
 std::cout<<"PASS preload-only Material: resolved CAB/PathID, two author clones, unchanged template, Prefab preload closure, Texture template inference\n";return 0;
 }catch(const std::exception& error){std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}}

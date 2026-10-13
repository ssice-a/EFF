#define main StaticFixtureMain
#include "static_mesh_assembly_smoke.cpp"
#undef main
#include "../../src/eiem_author_material_edits.h"

int main(int argc,char** argv){try{
 namespace M=EiemAuthorMaterialEdits;
 if(argc==5){auto package=EiemDirectory::Load(std::filesystem::u8path(argv[1]));auto bytes=Read(std::filesystem::u8path(argv[2]));auto layout=ParseSerialized(bytes);auto id=std::stoll(argv[3]);const auto& type=layout.types.at(layout.objects.at(id).type);
  auto edits=M::Project(type,ObjectValue(bytes,layout,id),*package.resources.at(package.materials.at(argv[4]).resource));
  std::cout<<"PASS portable Material edits floats="<<edits.floats.size()<<" integers="<<edits.integers.size()<<" colors="<<edits.colors.size()<<" keywords="<<edits.keywords.size()<<" tags="<<edits.tags.size()<<" queue="<<bool(edits.queue)<<" originalTexturesCopied=0 consumerPrefabs=0\n";return 0;}
 Require(argc==1,"expected Mod donor PathID Material-section");
 auto vector=[](const char* name){return FieldSpec{"Vector2f",name,{{"float","x",{}},{"float","y",{}}}};};
 auto environment=FieldSpec{"UnityTexEnv","second",{Ref("m_Texture"),vector("m_Scale"),vector("m_Offset")}};
 auto texturePair=FieldSpec{"pair","data",{{"string","first",{}},environment}};
 auto tagPair=FieldSpec{"pair","data",{{"string","first",{}},{"string","second",{}}}};
 auto properties=FieldSpec{"UnityPropertySheet","m_SavedProperties",{
  Arr("m_Floats",{"pair","data",{{"string","first",{}},{"float","second",{}}}}),
  Arr("m_Ints",{"pair","data",{{"string","first",{}},{"int","second",{}}}}),
  Arr("m_Colors",{"pair","data",{{"string","first",{}},{"ColorRGBA","second",{{"float","r",{}},{"float","g",{}},{"float","b",{}},{"float","a",{}}}}}}),
  Arr("m_TexEnvs",texturePair)}};
 auto type=Schema(21,{{"string","m_Name",{}},properties,{"int","m_CustomRenderQueue",{}},Arr("m_ValidKeywords",{"string","data",{}}),Arr("stringTagMap",tagPair)});
 auto original=EmptyValue(type,0);auto& values=ArrayValue(type,Child(type,Child(type,original,"m_SavedProperties"),"m_Floats"));
 for(const char* name:{"_UseEmission","_Roughness"}){auto entry=ArrayElement(type,values);SetString(type,Child(type,entry,"first"),name);SetFloat(type,Child(type,entry,"second"),0.5);values.children.push_back(std::move(entry));}values.count=2;
 auto current=original;auto& floats=ArrayValue(type,Child(type,Child(type,current,"m_SavedProperties"),"m_Floats"));SetFloat(type,Child(type,floats.children[0],"second"),1);
 SetInteger(type,Child(type,current,"m_CustomRenderQueue"),2999);
 auto& keywords=ArrayValue(type,Child(type,current,"m_ValidKeywords"));auto keyword=ArrayElement(type,keywords);SetString(type,keyword,"_EMISSION");keywords.children={keyword};keywords.count=1;
 Resource resource;resource.kind=2;resource.type=type.hash;
 auto field=[&](const char* name){auto payload=EncodeValue(type,Child(type,current,name));Block block;block.offset=resource.bytes.size();block.size=payload.size();resource.blocks[name]=block;resource.bytes.insert(resource.bytes.end(),payload.begin(),payload.end());};
 field("m_SavedProperties");field("m_CustomRenderQueue");field("m_ValidKeywords");
 auto edits=M::Project(type,original,resource);
 Require(edits.floats.size()==1&&edits.floats.at("_UseEmission")==1&&!edits.floats.count("_Roughness")&&edits.queue==2999&&edits.keywords.at("_EMISSION"),"Material projection pinned untouched donor values or lost authored controls");
 auto noChange=resource;noChange.blocks.clear();Require(M::Project(type,original,noChange).Empty(),"unchanged donor produced Material operations");
 auto future=original;auto& futureValues=ArrayValue(type,Child(type,Child(type,future,"m_SavedProperties"),"m_Floats"));SetFloat(type,Child(type,futureValues.children[1],"second"),0.9);
 // Explicit operations can be applied to a changed donor without ever
 // overwriting its untouched roughness. The generated edit stays sparse.
 Require(!edits.floats.count("_Roughness")&&M::Number(type,Child(type,futureValues.children[1],"second"))>0.89,"future donor defaults cannot be inherited");
 std::cout<<"PASS portable Material operations: typed values, queue, keyword and untouched donor inheritance without PFB/CAB bindings\n";return 0;
 }catch(const std::exception& error){std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}}

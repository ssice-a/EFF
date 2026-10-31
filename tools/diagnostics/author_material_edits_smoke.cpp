#define main StaticFixtureMain
#include "static_mesh_assembly_smoke.cpp"
#undef main
#include "../../src/eiem_demand_material_build.h"
#include <iostream>

using namespace EiemNativeInput;
namespace R=EiemNativeRandomAccess;
namespace M=EiemDemandMaterialBuild;

static void AddBundleStringField(Fixture& fixture,const char* name){
  auto& type=fixture.types[4];auto node=type.nodes.size();type.nodes.push_back({"string",name,false,{}});
  type.nodes[0].children.push_back(node);fixture.V(300).children.push_back(EmptyValue(type,node));
}

static Fixture MaterialFixture(){
  Fixture fixture;
  AddBundleStringField(fixture,"m_Name");AddBundleStringField(fixture,"m_AssetBundleName");
  auto& bundleType=fixture.types[4];auto dependency=bundleType.nodes.size();
  bundleType.nodes.push_back({"vector","m_Dependencies",false,{dependency+1}});
  bundleType.nodes.push_back({"Array","Array",false,{dependency+2,dependency+3}});
  bundleType.nodes.push_back({"int","size",false,{}});
  bundleType.nodes.push_back({"string","data",false,{}});
  bundleType.nodes[0].children.push_back(dependency);
  fixture.V(300).children.push_back(EmptyValue(bundleType,dependency));
  FieldSpec vector2{"Vector2f","m_Scale",{{"float","x",{}},{"float","y",{}}}};
  FieldSpec offset{"Vector2f","m_Offset",{{"float","x",{}},{"float","y",{}}}};
  FieldSpec texEnv{"UnityTexEnv","second",{Ref("m_Texture"),vector2,offset}};
  FieldSpec textureMap{"vector","m_TexEnvs",{{"Array","Array",{{"int","size",{}},
    {"pair","data",{{"string","first",{}},texEnv}}}}}};
  auto type=Schema(21,{{"string","m_Name",{}},Ref("m_Shader"),
    {"UnityPropertySheet","m_SavedProperties",{textureMap}},
    {"int","m_CustomRenderQueue",{}},{"string","m_EngineSpecificField",{}}});
  fixture.types[6]=type;
  fixture.objects.at(200)={6,EmptyValue(type,0)};
  SetString(type,Child(type,fixture.V(200),"m_Name"),"Metal");
  SetReference(type,Child(type,fixture.V(200),"m_Shader"),{"CAB-current-shader",555},fixture.refs,fixture.cab);
  SetInteger(type,Child(type,fixture.V(200),"m_CustomRenderQueue"),2000);
  SetString(type,Child(type,fixture.V(200),"m_EngineSpecificField"),"updated-game-default");
  auto& saved=Child(type,fixture.V(200),"m_SavedProperties");
  auto& entries=ArrayValue(type,Child(type,saved,"m_TexEnvs"));
  auto entry=ArrayElement(type,entries);SetString(type,Child(type,entry,"first"),"_BaseMap");
  auto& value=Child(type,entry,"second");
  SetReference(type,Child(type,value,"m_Texture"),{"CAB-old-texture",777},fixture.refs,fixture.cab);
  SetFloat(type,Child(type,Child(type,value,"m_Scale"),"x"),1.0);
  SetFloat(type,Child(type,Child(type,value,"m_Scale"),"y"),1.0);
  SetFloat(type,Child(type,Child(type,value,"m_Offset"),"x"),0.0);
  SetFloat(type,Child(type,Child(type,value,"m_Offset"),"y"),0.0);
  entries.children.push_back(std::move(entry));entries.count=1;
  return fixture;
}

static NativeReference Reference(const Type& type,const Value& pointer,const SerializedLayout& layout,
                                 const std::string& cab){
  auto file=Integer(Child(type,pointer,"m_FileID"));auto id=int64_t(Integer(Child(type,pointer,"m_PathID")));
  Require(file<=layout.externals.size(),"Material fixture reference external index differs");
  return {file?layout.externals[size_t(file)-1]:cab,id};
}

static void CheckCurrentSourceMaterialIsClonedAndEdited(){
  auto fixture=MaterialFixture();auto encoded=Bundle(fixture.BytesOf(),fixture.cab);
  auto read=[&](size_t at,unsigned char* output,size_t size){
    Require(at<=encoded.size()&&size<=encoded.size()-at,"current Material fixture read exceeds Bundle");
    memcpy(output,encoded.data()+at,size);
  };
  R::Reader reader(encoded.size(),read);const auto& member=reader.directory.members.front();auto layout=reader.Layout(member);
  const std::string authorText="format=EFFMAT\nversion=1\noverrides=true\n"
    "source=assets/material.mat\nname=Metal\nrender_queue=3100\ntexture._BaseMap=TextureBody\n";
  Bytes authorBytes(authorText.begin(),authorText.end());auto author=EiemAuthorMaterialEdits::Parse(authorBytes);
  const std::map<std::string,NativeReference> textures{{"_BaseMap",{"CAB-author-texture",901}}};
  Identity logical{"assets/material.mat","Metal"};
  auto input=M::Build(reader,0,layout,200,logical,author,textures,read);
  Require(input.dependencies==std::set<NativeReference>{{"CAB-current-shader",555},{"CAB-author-texture",901}},
    "Material variant did not retain the current shader and bind the authored Texture");
  Bytes candidate(input.view->Size());Require(input.view->ReadAt(0,candidate.data(),candidate.size())==candidate.size(),
    "demand Material variant view is truncated");
  R::Reader output(candidate.size(),[&](size_t at,unsigned char* destination,size_t size){
    Require(at<=candidate.size()&&size<=candidate.size()-at,"Material candidate read exceeds view");
    memcpy(destination,candidate.data()+at,size);
  });
  const auto& outputMember=output.directory.members.front();auto outputLayout=output.Layout(outputMember);
  const auto& outputType=outputLayout.types.at(outputLayout.objects.at(200).type);
  auto material=output.Object(outputMember,outputLayout,200);
  Require(Integer(Child(outputType,material,"m_CustomRenderQueue"))==3100&&
    EiemAuthorMaterialEdits::String(outputType,Child(outputType,material,"m_EngineSpecificField"))=="updated-game-default",
    "sparse Material edits did not preserve current game fields absent from EFFMAT");
  const auto& saved=Child(outputType,material,"m_SavedProperties");
  const auto& texturesArray=ConstArray(outputType,Child(outputType,saved,"m_TexEnvs"));
  Require(texturesArray.children.size()==1,"current Material texture slots changed during variant assembly");
  const auto& texEnv=Child(outputType,texturesArray.children.front(),"second");
  Require(Reference(outputType,Child(outputType,texEnv,"m_Texture"),outputLayout,outputMember.name)==textures.at("_BaseMap"),
    "authored Material Texture PPtr was not applied to the current game Material");
  const auto& shader=Child(outputType,material,"m_Shader");
  Require(Reference(outputType,shader,outputLayout,outputMember.name)==NativeReference{"CAB-current-shader",555},
    "current game Shader PPtr was not preserved");
}

int main(){try{
  CheckCurrentSourceMaterialIsClonedAndEdited();
  auto malformed=Bytes{'f','o','r','m','a','t','=','E','F','F','M','A','T','\n'};
  bool rejected=false;try{(void)EiemAuthorMaterialEdits::Parse(malformed);}catch(const std::exception&){rejected=true;}
  Require(rejected,"incomplete EFFMAT document was accepted");
  std::cout<<"PASS sparse EFFMAT replay: current game fields and Shader are inherited, explicit Texture PPtr and queue edits are applied before serialization\n";
  return 0;
}catch(const std::exception& error){std::cerr<<"FAIL: "<<error.what()<<'\n';return 1;}}

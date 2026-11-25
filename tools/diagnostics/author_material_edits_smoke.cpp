#define main StaticFixtureMain
#include "static_mesh_assembly_smoke.cpp"
#undef main
#include "../../src/eiem_demand_material_build.h"
#include "../../src/eiem_demand_material_assembly.h"
#include "../../src/eiem_offline_resource_package.h"
#include <iostream>

using namespace EiemNativeInput;
namespace R=EiemNativeRandomAccess;
namespace M=EiemDemandMaterialBuild;
namespace MA=EiemDemandMaterialAssembly;
namespace Offline=EiemOfflinePackage;

static void PutLe(Bytes& bytes,size_t offset,uint64_t value,size_t width){
  Require(offset<=bytes.size()&&width<=bytes.size()-offset,"deferred resource fixture field extent differs");
  for(size_t i=0;i<width;++i)bytes[offset+i]=static_cast<unsigned char>(value>>(i*8));
}
static Bytes ResourceEnvelope(const Bytes& block){
  const std::string name="x";const size_t directory=2+name.size()+48;
  const size_t payload=(80+directory+15)&~size_t(15);Bytes bytes(payload+block.size(),0);
  std::memcpy(bytes.data(),"EFFRES",6);bytes[6]='2';
  PutLe(bytes,12,1,4);PutLe(bytes,16,1,4);PutLe(bytes,20,directory,4);
  PutLe(bytes,40,bytes.size(),8);PutLe(bytes,80,name.size(),2);
  std::memcpy(bytes.data()+82,name.data(),name.size());PutLe(bytes,83,payload,8);
  PutLe(bytes,91,block.size(),8);if(!block.empty())std::memcpy(bytes.data()+payload,block.data(),block.size());
  auto blockHash=Sha(bytes.data()+payload,block.size());std::memcpy(bytes.data()+99,blockHash.data(),blockHash.size());
  auto bodyHash=Sha(bytes.data()+80,bytes.size()-80);std::memcpy(bytes.data()+48,bodyHash.data(),bodyHash.size());
  return bytes;
}
static void CheckDeferredResourcePayloadVerification(){
  Bytes block(1024*1024,0x35);auto valid=ResourceEnvelope(block);auto expected=Sha(valid.data(),valid.size());
  auto expectedText=Hex(expected.data(),expected.size());auto damaged=valid;damaged.back()^=1;
  auto decoded=DecodeView(damaged.data(),damaged.size(),1,false);decoded.contentHash=expectedText;
  auto verification=std::make_shared<DeferredVerification>();verification->expected=expectedText;
  verification->data=damaged.data();verification->size=damaged.size();decoded.deferredVerification=verification;
  Require(!verification->complete.load(std::memory_order_acquire),"runtime resource was verified before it was used");
  bool rejected=false;try{VerifyDeferredResource(decoded);}catch(const std::exception&){rejected=true;}
  Require(rejected&&!verification->complete.load(std::memory_order_acquire),
    "damaged runtime author resource did not fail at first-use verification");
  auto good=DecodeView(valid.data(),valid.size(),1,false);good.contentHash=expectedText;
  auto goodVerification=std::make_shared<DeferredVerification>();goodVerification->expected=expectedText;
  goodVerification->data=valid.data();goodVerification->size=valid.size();good.deferredVerification=goodVerification;
  Require(!goodVerification->complete.load(std::memory_order_acquire),"valid resource was verified during lazy decode");
  VerifyDeferredResource(good);
  Require(goodVerification->complete.load(std::memory_order_acquire),"valid resource was not verified when used");
  std::cout<<"PASS author payload integrity is deferred until first resource read bytes="<<block.size()<<"\n";
}

static Fixture MaterialFixture(){
  Fixture fixture;
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

static void CheckMaterialPropertyRemoval(){
  FieldSpec floatMap{"vector","m_Floats",{{"Array","Array",{{"int","size",{}},
    {"pair","data",{{"string","first",{}},{"float","second",{}}}}}}}};
  auto type=Schema(21,{{"UnityPropertySheet","m_SavedProperties",{floatMap}}});
  auto material=EmptyValue(type,0);auto& saved=Child(type,material,"m_SavedProperties");
  auto& values=ArrayValue(type,Child(type,saved,"m_Floats"));auto old=ArrayElement(type,values);
  SetString(type,Child(type,old,"first"),"_LegacyOnly");SetFloat(type,Child(type,old,"second"),0.5);
  values.children.push_back(std::move(old));values.count=1;
  const std::string authorText="format=EFFMAT\nversion=1\noverrides=true\n"
    "source=assets/material.mat\nremove.float._LegacyOnly=true\n";
  Bytes authorBytes(authorText.begin(),authorText.end());auto author=EiemAuthorMaterialEdits::Parse(authorBytes);
  SerializedLayout layout;auto result=EiemNativeMaterialOperations::Apply(type,std::move(material),
    author.edits,layout,"CAB-current");
  Require(ConstArray(type,Child(type,Child(type,result,"m_SavedProperties"),"m_Floats")).children.empty(),
    "EFFMAT Material property removal was not applied");
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

static void CheckMatchedPrefabKeepsSharedMaterialReferences(){
  auto source=MaterialFixture();auto sourceBytes=Bundle(source.BytesOf(),source.cab);
  R::Reader sourceReader(sourceBytes.size(),[&](size_t at,unsigned char* out,size_t size){
    Require(at<=sourceBytes.size()&&size<=sourceBytes.size()-at,"material source fixture truncated");
    memcpy(out,sourceBytes.data()+at,size);
  });
  const auto& sourceMember=sourceReader.directory.members.front();auto sourceLayout=sourceReader.Layout(sourceMember);
  const std::string authorText="format=EFFMAT\nversion=1\noverrides=true\n"
    "source=assets/material.mat\nname=Metal\ntexture._BaseMap=TextureBody\n";
  Bytes document(authorText.begin(),authorText.end());auto author=EiemAuthorMaterialEdits::Parse(document);
  const NativeReference authoredTexture{"CAB-author-texture",901};
  const std::map<std::string,NativeReference> textures{{"_BaseMap",authoredTexture}};
  auto material=M::Build(sourceReader,0,sourceLayout,200,{"assets/material.mat","Metal"},author,textures,
    [&](size_t at,unsigned char* out,size_t size){
      Require(at<=sourceBytes.size()&&size<=sourceBytes.size()-at,"material source range truncated");
      memcpy(out,sourceBytes.data()+at,size);
    });

  MA::Shared resources;resources.inputs.emplace(material.cab,material);
  resources.references.emplace(M::RendererMaterialKey{13,"P"},material.asset);
  resources.references.emplace(M::RendererMaterialKey{23,"P"},material.asset);
  auto closure=material.dependencies;closure.insert(material.asset);
  resources.closures.emplace(material.asset,std::move(closure));

  Fixture prefab;auto prefabBytes=Bundle(prefab.BytesOf(),prefab.cab);
  R::Reader prefabReader(prefabBytes.size(),[&](size_t at,unsigned char* out,size_t size){
    Require(at<=prefabBytes.size()&&size<=prefabBytes.size()-at,"matched Prefab fixture truncated");
    memcpy(out,prefabBytes.data()+at,size);
  });
  const auto& prefabMember=prefabReader.directory.members.front();
  EiemNativeEditTransaction::Member transaction(prefabReader,0,prefabReader.Layout(prefabMember));
  Package package;package.id="material-pfb-assembly";Identity target{"assets/static.mesh","Static"};
  package.targetIndex[target]="R";package.rules["R"].parts={"P"};package.parts["P"].mesh="M";
  package.parts["P"].material="A";package.meshes["M"]={"mesh",3,3,{},{}};
  auto meshSchema=std::make_shared<Resource>();meshSchema->type=prefab.types[5].hash;package.resources["mesh"]=meshSchema;
  AssetIndex assets;assets[target].cab=prefab.cab;assets[target].pathId=100;assets[target].classId=43;assets[target].type=meshSchema->type;
  std::vector<BuiltMeshPart> parts{{target,"R","P",prefab.cab,100,100,3,3}};
  RendererMemberBuilder renderer(transaction.Layout(),prefab.cab,package,assets,parts,
    [&](int64_t id,const std::set<std::string>& fields){return transaction.Project(id,fields);},
    [&](int64_t id){return transaction.Object(id);});
  renderer.SetMaterialReferences(resources.references);renderer.SetMaterialClosures(resources.closures);
  auto built=renderer.Build();transaction.Stage(renderer.OutputLayout(),renderer.TakeChanges(),renderer.TakeAdded());
  auto plan=transaction.Commit();auto prepared=EiemNativeSpliceView::BuildRanges(prefabReader,{plan.rewrite});
  auto outputView=prepared.Bind([&](size_t at,unsigned char* out,size_t size){
    Require(at<=prefabBytes.size()&&size<=prefabBytes.size()-at,"matched Prefab source truncated");
    memcpy(out,prefabBytes.data()+at,size);
  });
  R::Reader output(outputView->Size(),[&](size_t at,unsigned char* out,size_t size){
    Require(outputView->ReadAt(at,out,size)==size,"assembled Prefab output truncated");
  });
  const auto& outputMember=output.directory.members.front();auto outputLayout=output.Layout(outputMember);
  Require(built.matches==2,"matched PFB Renderers were not rewritten");
  Require(material.asset.first!=prefab.cab,"shared author Material unexpectedly belongs to the matched PFB");
  for(const auto rendererId:{int64_t(13),int64_t(23)}){
    const auto& rendererType=outputLayout.types.at(outputLayout.objects.at(rendererId).type);
    auto rendererObject=output.Object(outputMember,outputLayout,rendererId);
    const auto& slots=ConstArray(rendererType,Child(rendererType,rendererObject,"m_Materials"));
    Require(slots.children.size()==1&&
        Reference(rendererType,slots.children.front(),outputLayout,prefab.cab)==material.asset,
      "matched Renderer did not retain its shared author Material reference");
  }
  std::set<NativeReference> prefabPreloads;
  for(auto id:outputLayout.order){const auto& type=outputLayout.types.at(outputLayout.objects.at(id).type);
    if(type.classId!=142)continue;auto bundle=output.Object(outputMember,outputLayout,id);
    for(const auto& pointer:ConstArray(type,Child(type,bundle,"m_PreloadTable")).children)
      prefabPreloads.insert(Reference(type,pointer,outputLayout,prefab.cab));}
  Require(prefabPreloads.count(material.asset)&&prefabPreloads.count(authoredTexture)&&
      prefabPreloads.count({"CAB-current-shader",555}),
    "matched PFB preload closure omits the shared Material, authored Texture, or current Shader");
  Require(outputLayout.externals.end()!=std::find(outputLayout.externals.begin(),outputLayout.externals.end(),material.asset.first),
    "matched PFB omitted its shared author Material CAB reference");
  std::cout<<"PASS matched PFB references the shared author Material and preloads only its declared dependency closure\n";
}

int main(int argc,char** argv){try{
  if(argc>1&&std::string(argv[1])=="--removal-only"){
    CheckMaterialPropertyRemoval();
    std::cout<<"PASS EFFMAT removal operations are parsed and applied to the native Material copy\n";
    return 0;
  }
  CheckCurrentSourceMaterialIsClonedAndEdited();
  CheckMatchedPrefabKeepsSharedMaterialReferences();
  CheckMaterialPropertyRemoval();
  CheckDeferredResourcePayloadVerification();
  auto malformed=Bytes{'f','o','r','m','a','t','=','E','F','F','M','A','T','\n'};
  bool rejected=false;try{(void)EiemAuthorMaterialEdits::Parse(malformed);}catch(const std::exception&){rejected=true;}
  Require(rejected,"incomplete EFFMAT document was accepted");
  std::cout<<"PASS sparse EFFMAT replay: current game fields and Shader are inherited, explicit Texture PPtr and queue edits are applied before serialization\n";
  return 0;
}catch(const std::exception& error){std::cerr<<"FAIL: "<<error.what()<<'\n';return 1;}}

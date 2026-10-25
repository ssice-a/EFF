#define EIEM_AUTHOR_BUILDER_FIXTURE_ONLY
#include "author_builder_origins_smoke.cpp"
#include "../../src/eiem_addressed_resource_update.h"

namespace AddressedTest {
using namespace EiemAddressedResources;
std::shared_ptr<Resource> ResourceOf(unsigned kind,const std::array<unsigned char,16>& type,const std::map<std::string,Bytes>& blocks){
 Bytes bytes(80);memcpy(bytes.data(),"EFFRES",6);WriteInteger(bytes,12,kind,4);WriteInteger(bytes,16,blocks.size(),4);std::copy(type.begin(),type.end(),bytes.begin()+24);
 size_t directory=0;for(const auto& block:blocks)directory+=50+block.first.size();WriteInteger(bytes,20,directory,4);bytes.resize(80+directory);size_t row=80;
 for(const auto& block:blocks){WriteInteger(bytes,row,block.first.size(),2);row+=2;std::copy(block.first.begin(),block.first.end(),bytes.begin()+row);row+=block.first.size();bytes.resize(AlignSize(bytes.size(),16));WriteInteger(bytes,row,bytes.size(),8);WriteInteger(bytes,row+8,block.second.size(),8);auto digest=Sha(block.second.data(),block.second.size());std::copy(digest.begin(),digest.end(),bytes.begin()+row+16);row+=48;bytes.insert(bytes.end(),block.second.begin(),block.second.end());}
 WriteInteger(bytes,40,bytes.size(),8);auto digest=Sha(bytes.data()+80,bytes.size()-80);std::copy(digest.begin(),digest.end(),bytes.begin()+48);return std::make_shared<Resource>(EiemDirectory::Decode(std::move(bytes),kind));
}
std::shared_ptr<Resource> Repack(const Resource& resource,const std::function<void(std::map<std::string,Bytes>&)>& edit){
 std::map<std::string,Bytes> blocks;for(const auto& row:resource.blocks)blocks[row.first]=Bytes(resource.bytes.begin()+row.second.offset,resource.bytes.begin()+row.second.offset+row.second.size);edit(blocks);return ResourceOf(resource.kind,resource.type,blocks);
}
FieldSpec Matrix(){FieldSpec result{"Matrix4x4f","data",{}};for(int row=0;row<4;++row)for(int col=0;col<4;++col)result.children.push_back({"float","e"+std::to_string(row)+std::to_string(col),{}});return result;}
Type MeshType(){return Schema(43,{{"string","m_Name",{}},
 {"VertexData","m_VertexData",{{"UInt32","m_VertexCount",{}},Arr("m_Channels",{"Channel","data",{{"UInt8","stream",{}},{"UInt8","offset",{}},{"UInt8","format",{}},{"UInt8","dimension",{}}}}),{"TypelessData","m_DataSize",{}}}},
 {"Shapes","m_Shapes",{Arr("vertices",{"ShapeVertex","data",{Vec("vertex"),{"UInt32","index",{}}}}),Arr("shapes",{"ShapeFrame","data",{{"UInt32","firstVertex",{}},{"UInt32","vertexCount",{}}}}),Arr("fullWeights",{"float","data",{}}),Arr("channels",{"ShapeChannel","data",{{"string","name",{}},{"UInt32","frameIndex",{}},{"UInt32","frameCount",{}}}})}},
 Arr("m_BindPose",Matrix()),Arr("m_BoneNameHashes",{"UInt32","data",{}}),{"UInt32","m_BonesPerVertex",{}},Arr("m_BonesAABB",{"AABB","data",{Vec("m_Center"),Vec("m_Extent")}}),
 Arr("m_SubMeshes",{"SubMesh","data",{{"UInt32","indexCount",{}},{"UInt32","vertexCount",{}},{"UInt32","firstByte",{}},{"UInt32","firstVertex",{}},{"UInt32","baseVertex",{}},{"UInt32","topology",{}}}}),
 {"int","m_IndexFormat",{}},Arr("m_IndexBuffer",{"UInt8","data",{}}),{"bool","m_IsReadable",{}},{"int","m_MeshCompression",{}},{"StreamingInfo","m_StreamData",{{"UInt64","offset",{}},{"UInt32","size",{}},{"string","path",{}}}}});}
Value MeshValue(const Type& type){
 auto mesh=EmptyValue(type,0);SetString(type,Child(type,mesh,"m_Name"),"Static");auto& vertex=Child(type,mesh,"m_VertexData");SetInteger(type,Child(type,vertex,"m_VertexCount"),3);
 auto& channels=ArrayValue(type,Child(type,vertex,"m_Channels"));for(size_t i=0;i<14;++i){auto channel=ArrayElement(type,channels);if(i==0)SetInteger(type,Child(type,channel,"dimension"),3);if(i==1){SetInteger(type,Child(type,channel,"offset"),12);SetInteger(type,Child(type,channel,"dimension"),0x31);}channels.children.push_back(std::move(channel));}channels.count=14;
 auto& data=Child(type,vertex,"m_DataSize").raw;data.resize(48);for(size_t i=0;i<3;++i){float position[3]={float(i==1),float(i==2),float(i)*0.25f};memcpy(data.data()+i*16,position,12);WriteInteger(data,i*16+12,0x40000000,4);}
 auto& shapes=Child(type,mesh,"m_Shapes");auto& frames=ArrayValue(type,Child(type,shapes,"shapes"));frames.children.push_back(ArrayElement(type,frames));frames.count=1;
 auto& weights=ArrayValue(type,Child(type,shapes,"fullWeights"));float weight=100;weights.raw.resize(4);memcpy(weights.raw.data(),&weight,4);weights.count=1;
 auto& shapeChannels=ArrayValue(type,Child(type,shapes,"channels"));auto channel=ArrayElement(type,shapeChannels);SetString(type,Child(type,channel,"name"),"Shape");SetInteger(type,Child(type,channel,"frameCount"),1);shapeChannels.children.push_back(std::move(channel));shapeChannels.count=1;
 auto& bindposes=ArrayValue(type,Child(type,mesh,"m_BindPose"));auto matrix=ArrayElement(type,bindposes);for(int i=0;i<4;++i)SetFloat(type,Child(type,matrix,"e"+std::to_string(i)+std::to_string(i)),1);bindposes.children.push_back(std::move(matrix));bindposes.count=1;
 auto& hashes=ArrayValue(type,Child(type,mesh,"m_BoneNameHashes"));hashes.count=1;N(hashes.raw,123,4);
 auto& submeshes=ArrayValue(type,Child(type,mesh,"m_SubMeshes"));auto submesh=ArrayElement(type,submeshes);SetInteger(type,Child(type,submesh,"indexCount"),3);SetInteger(type,Child(type,submesh,"vertexCount"),3);submeshes.children.push_back(std::move(submesh));submeshes.count=1;
 auto& indices=ArrayValue(type,Child(type,mesh,"m_IndexBuffer"));indices.raw={0,0,1,0,2,0};indices.count=6;return mesh;
}
Type MaterialType(){return Schema(21,{{"string","m_Name",{}},{"float","m_Custom",{}},{"SavedProperties","m_SavedProperties",{Arr("m_TexEnvs",{"TexEnvPair","data",{{"string","first",{}},{"TexEnv","second",{Ref("m_Texture")}}}})}}});}
Type TextureType(){return Schema(28,{{"string","m_Name",{}},Arr("m_PlatformBlob",{"UInt8","data",{}}),{"TypelessData","image data",{}},{"int","m_ImageCount",{}},{"int","m_TextureDimension",{}},
 {"StreamingInfo","m_StreamData",{{"UInt64","offset",{}},{"UInt32","size",{}},{"string","path",{}}}},
 {"int","m_Width",{}},{"int","m_Height",{}},{"int","m_CompleteImageSize",{}},{"int","m_MipsStripped",{}},{"int","m_TextureFormat",{}},{"int","m_MipCount",{}},{"bool","m_IsPreProcessed",{}},{"bool","m_StreamingMipmaps",{}},{"int","m_ColorSpace",{}},
 {"TextureSettings","m_TextureSettings",{{"int","m_FilterMode",{}},{"int","m_Aniso",{}},{"float","m_MipBias",{}},{"int","m_WrapU",{}},{"int","m_WrapV",{}},{"int","m_WrapW",{}}}}});}
struct Setup {
 Fixture fixture;Package package;AssetIndex assets;Bytes pristine;std::vector<BuiltMeshPart> parts;MeshSchemaIndex schemas;
 const Identity meshTarget{"assets/static.mesh","Static"},materialTarget{"assets/material.mat","Mat"},textureTarget{"assets/texture.tex","Texture"};
 Setup(){
  fixture.types[5]=MeshType();fixture.objects.at(100)={5,MeshValue(fixture.types[5])};fixture.types[6]=MaterialType();fixture.objects.at(200)={6,EmptyValue(fixture.types[6],0)};
  SetString(fixture.T(200),Child(fixture.T(200),fixture.V(200),"m_Name"),"Mat");SetFloat(fixture.T(200),Child(fixture.T(200),fixture.V(200),"m_Custom"),0.125);
  auto& envs=ArrayValue(fixture.T(200),Child(fixture.T(200),Child(fixture.T(200),fixture.V(200),"m_SavedProperties"),"m_TexEnvs"));auto env=ArrayElement(fixture.T(200),envs);SetString(fixture.T(200),Child(fixture.T(200),env,"first"),"_MainTex");SetReference(fixture.T(200),Child(fixture.T(200),Child(fixture.T(200),env,"second"),"m_Texture"),{fixture.cab,400},fixture.refs,fixture.cab);envs.children.push_back(std::move(env));envs.count=1;
  fixture.types.push_back(TextureType());fixture.Add(400,fixture.types.size()-1);auto& texture=fixture.V(400);const auto& tt=fixture.T(400);SetString(tt,Child(tt,texture,"m_Name"),"Texture");SetInteger(tt,Child(tt,texture,"m_ImageCount"),1);SetInteger(tt,Child(tt,texture,"m_TextureDimension"),2);SetString(tt,Child(tt,Child(tt,texture,"m_StreamData"),"path"),fixture.cab+".resS");SetInteger(tt,Child(tt,Child(tt,texture,"m_StreamData"),"size"),4);
  fixture.types[3]=Schema(137,{Ref("m_GameObject"),Ref("m_Mesh"),{"bool","m_Enabled",{}},Arr("m_Materials",Ref("data")),Arr("m_Bones",Ref("data")),Ref("m_RootBone"),Ref("m_SkinningRoot"),Arr("m_BlendShapeWeights",{"float","data",{}}),{"bool","m_DirtyAABB",{}},{"AABB","m_AABB",{Vec("m_Center"),Vec("m_Extent")}}});
  for(int64_t id:{13,23}){auto bone=id+28;fixture.Add(bone-1,0);SetString(fixture.T(bone-1),Child(fixture.T(bone-1),fixture.V(bone-1),"m_Name"),"spine");fixture.Add(bone,1);fixture.Pointer(bone,"m_GameObject",bone-1);fixture.Pointer(bone,"m_Father",id-2);fixture.List(id-2,"m_Children",{bone});SetVector(fixture.T(bone),Child(fixture.T(bone),fixture.V(bone),"m_LocalScale"),{1,1,1});SetFloat(fixture.T(bone),Child(fixture.T(bone),Child(fixture.T(bone),fixture.V(bone),"m_LocalRotation"),"w"),1);fixture.List(bone-1,"m_Component",{bone},"component");fixture.objects.at(id)={3,EmptyValue(fixture.types[3],0)};fixture.Pointer(id,"m_GameObject",id-3);fixture.Pointer(id,"m_Mesh",100);fixture.Pointer(id,"m_RootBone",bone);fixture.List(id,"m_Bones",{bone});fixture.List(id,"m_Materials",{200});}
  pristine=fixture.BytesOf();package.id="addressed-resources";package.targetIndex[meshTarget]="R";package.rules["R"].parts={"P"};package.parts["P"].mesh="M";package.parts["P"].material="A";
  auto& mesh=package.meshes["M"];mesh.resource="resources/mesh.bin";mesh.vertices=3;mesh.indices=3;mesh.bones={"spine"};mesh.shapes={"Shape"};std::map<std::string,Bytes> meshBlocks;
  for(const auto& field:fixture.V(100).children)if(fixture.T(100).nodes[field.node].name!="m_Name")meshBlocks[fixture.T(100).nodes[field.node].name]=EncodeValue(fixture.T(100),field);package.resources[mesh.resource]=ResourceOf(1,fixture.T(100).hash,meshBlocks);
  auto& material=package.materials["A"];material.resource="resources/material.bin";material.source=materialTarget;material.textures["_MainTex"]="T";package.resources[material.resource]=ResourceOf(2,fixture.T(200).hash,{{"m_Custom",EncodeValue(fixture.T(200),Child(fixture.T(200),fixture.V(200),"m_Custom"))},{"m_SavedProperties",EncodeValue(fixture.T(200),Child(fixture.T(200),fixture.V(200),"m_SavedProperties"))}});
  auto& tex=package.textures["T"];tex.resource="resources/texture.bin";tex.source=textureTarget;tex.width=tex.height=tex.mips=1;package.resources[tex.resource]=ResourceOf(4,tt.hash,{{"pixels",{100,120,140,255}}});
  auto rawPhysics=PhysicsResource(meshTarget);auto block=rawPhysics->blocks.at("@physics");package.resources["resources/physics.bin"]=ResourceOf(5,{},{{"@physics",Bytes(rawPhysics->bytes.begin()+block.offset,rawPhysics->bytes.begin()+block.offset+block.size)}});
  package.resources["resources/effect.bin"]=ResourceOf(10,{},{{"@effect",Bytes(64,0x5a)}});
  package.resources["resources/unused.bin"]=ResourceOf(37,{},{{"payload",EncodeValue(fixture.T(200),Child(fixture.T(200),fixture.V(200),"m_Custom"))}});
  for(auto row:std::vector<std::tuple<Identity,int64_t,int>>{{meshTarget,100,43},{materialTarget,200,21},{textureTarget,400,28}}){auto& asset=assets[std::get<0>(row)];asset.cab=fixture.cab;asset.pathId=std::get<1>(row);asset.classId=std::get<2>(row);asset.type=fixture.T(asset.pathId).hash;}
  parts={{meshTarget,"R","P",fixture.cab,100,100,3,3,{}}};schemas.emplace(fixture.T(100).hash,fixture.T(100));
 }
 EndfieldInput::Bundle Full(const Package& author)const{
  auto layout=ParseSerialized(pristine);EndfieldInput::Bundle bundle;bundle.members={{fixture.cab,4,pristine},{fixture.cab+".resS",0,Bytes{1,2,3,4}}};
  std::map<int64_t,Bytes> replacements;std::map<int64_t,AuthorByteSpans> origins;std::vector<AddedObject> added;
  auto mesh=ApplyMesh(layout.types.at(layout.objects.at(100).type),ObjectValue(pristine,layout,100),*author.resources.at(author.meshes.at("M").resource),author.meshes.at("M"),"GeneratedMesh",true);replacements[100]=EncodeValue(layout.types.at(layout.objects.at(100).type),mesh,&origins[100]);
  auto materialId=AllocateMaterialId(fixture.cab,author,"A");auto& mt=layout.types.at(layout.objects.at(200).type);size_t bindings=0;auto material=ApplyMaterial(mt,ObjectValue(pristine,layout,200),*author.resources.at(author.materials.at("A").resource),author.materials.at("A"),author,assets,layout,fixture.cab,bindings);added.push_back({materialId,layout.objects.at(200).type,EncodeValue(mt,material,&origins[materialId])});
  const auto& texture=author.textures.at("T");auto textureId=AllocateTextureId(fixture.cab,author,"T",texture);const auto& tt=layout.types.at(layout.objects.at(400).type);TextureWrite record;auto value=ApplyTexture(tt,ObjectValue(pristine,layout,400),texture,*author.resources.at(texture.resource),fixture.cab,bundle.members,{{fixture.cab+".resS",4}},record);added.push_back({textureId,layout.objects.at(400).type,EncodeValue(tt,value,&origins[textureId])});
  bundle.members.front().bytes=BuildSerialized(pristine,layout,replacements,added,{},{},{},nullptr,&origins,&bundle.members.front().origins);
  EiemPhysicsInput::Build(bundle,author,assets);BuildRendererInput(bundle,author,assets,parts,schemas);
  size_t marked=0;EiemResourceReloadPlan::Mark(bundle,{{{fixture.cab,100},RuntimePartName(author,"P",meshTarget)}},marked);Require(marked==1,"addressed fixture marker not applied");return bundle;
 }
 Artifact Compiled(const EndfieldInput::Bundle& edited)const{
  Artifact artifact;artifact.manifest.package=package;artifact.members[fixture.cab].before=ParseSerialized(pristine);artifact.manifest.plan.frameRecipes=EiemResourceRecipe::DescribeFrames(package,schemas);
  for(const auto& frame:artifact.manifest.plan.frameRecipes)artifact.preparedFrames[frame.resource]=EiemResourceRecipe::ExpandFrame(frame,package.resources.at(frame.resource)->bytes);
  std::vector<Bindings> bindings(1);Capture(bindings.front(),edited);Finalize(artifact,bindings);return artifact;
 }
};
std::vector<EiemResourceRecipe::Member> Recipes(const Setup& setup,const EndfieldInput::Bundle& edited){
 Bytes stream{1,2,3,4};EiemResourceRecipe::Dictionary originals({{EiemResourceRecipe::Original,setup.fixture.cab,&setup.pristine},{EiemResourceRecipe::Original,setup.fixture.cab+".resS",&stream}}),authors({});std::vector<EiemResourceRecipe::Member> result;
 for(const auto& member:edited.members)result.push_back(EiemResourceRecipe::Describe(member,originals,authors,{},true));return result;
}
void EqualReplay(const Setup& setup,const Artifact& artifact,const std::vector<EiemResourceRecipe::Member>& recipes,const EndfieldInput::Bundle& full){
 Bytes stream{1,2,3,4};std::map<std::string,const Bytes*> originals{{setup.fixture.cab,&setup.pristine},{setup.fixture.cab+".resS",&stream}};
 Require(recipes.size()==full.members.size(),"replay member count differs");for(size_t i=0;i<recipes.size();++i)Require(EiemResourceRecipe::Apply(recipes[i],originals,artifact.manifest.package,artifact.preparedFrames,false)==full.members[i].bytes,"addressed replay differs from complete Mesh/Material/Texture/Physics/Renderer compilation");
}
void ChangePhysics(Package& package){auto name=std::string("resources/physics.bin");auto document=EiemPhysicsInput::Decode(*package.resources.at(name),name);const auto& donor=document.templates.begin()->second;auto type=donor.layout.types.at(donor.layout.objects.at(900).type);auto value=ObjectValue(donor.bytes,donor.layout,900,&donor.origins);auto span=Child(type,value,"damping").rawOrigins.front();auto original=package.resources.at(name);package.resources[name]=Repack(*original,[&](auto& blocks){float changed=0.75f;auto offset=span.offset-original->blocks.at("@physics").offset;memcpy(blocks.at("@physics").data()+offset,&changed,4);});}
void MixedReplay(){
 Setup setup;auto edited=setup.Full(setup.package);auto artifact=setup.Compiled(edited);auto recipes=Recipes(setup,edited);EqualReplay(setup,artifact,recipes,edited);
 auto bindings=Decode(artifact.resourceBindingBytes,setup.package);Require(!bindings.ranges.count("resources/unused.bin")&&!bindings.ranges.count("resources/effect.bin"),"unreferenced identical bytes acquired a guessed author dependency");
 auto next=setup.package;
 next.resources["resources/material.bin"]=Repack(*next.resources.at("resources/material.bin"),[](auto& blocks){float value=0.5f;memcpy(blocks.at("m_Custom").data(),&value,4);});
 next.resources["resources/texture.bin"]=Repack(*next.resources.at("resources/texture.bin"),[](auto& blocks){blocks.at("pixels")={200,30,80,255};});
 next.resources["resources/mesh.bin"]=Repack(*next.resources.at("resources/mesh.bin"),[&](auto& blocks){auto type=setup.fixture.T(100);auto field=Child(type,setup.fixture.V(100),"m_VertexData");field=DecodeValue(type,field.node,blocks.at("m_VertexData"));float value=3.5f;memcpy(Child(type,field,"m_DataSize").raw.data()+16,&value,4);blocks.at("m_VertexData")=EncodeValue(type,field);});
 ChangePhysics(next);next.resources["resources/effect.bin"]=ResourceOf(10,{},{{"@effect",Bytes(111,0xa7)}});
 std::map<std::string,double> costs;auto update=Rebind(artifact,next,[&](const char* stage,double value){costs[stage]+=value;});Require(update.compatible&&update.resources.size()==5&&update.generated.size()==2,"mixed update did not rebind all changed resources and both generated streams");
 EqualReplay(setup,artifact,recipes,setup.Full(next));Require(artifact.manifest.plan.author==PackageDigest(next)&&artifact.manifest.plan.contract==InputContractDigest(next),"mixed author seal was not updated");
 Require(costs["resource-rebind-resources"]==5&&costs["resource-rebind-generated"]==2,"resource update cost counts differ");
 // Equal old float bytes belong independently to Material and Physics.
 auto single=setup.Compiled(edited);auto materialOnly=setup.package;materialOnly.resources["resources/material.bin"]=next.resources.at("resources/material.bin");auto materialUpdate=Rebind(single,materialOnly);Require(materialUpdate.compatible&&materialUpdate.resources==std::set<std::string>{"resources/material.bin"}&&materialUpdate.generated.empty(),"same-byte unrelated resources were rebound");EqualReplay(setup,single,recipes,setup.Full(materialOnly));
 std::cout<<"PASS addressed mixed resources: Mesh/Material/Texture/Physics/Effects, explicit identity and full-build byte equality including bounds/frame\n";
}
void Fallbacks(){
 Setup setup;auto edited=setup.Full(setup.package);auto artifact=setup.Compiled(edited);auto reject=[&](const Package& next){auto copy=artifact;auto update=Rebind(copy,next);Require(!update.compatible&&PackageDigest(copy.manifest.package)==PackageDigest(artifact.manifest.package)&&copy.preparedFrames==artifact.preparedFrames,"incompatible update mutated its compiled snapshot");};
 auto next=setup.package;next.resources["resources/unused.bin"]=Repack(*next.resources.at("resources/unused.bin"),[](auto& blocks){blocks.at("payload")[0]^=1;});reject(next);
 next=setup.package;next.resources["resources/material.bin"]=Repack(*next.resources.at("resources/material.bin"),[](auto& blocks){WriteInteger(blocks.at("m_SavedProperties"),0,0,4);});reject(next);
 next=setup.package;next.resources["resources/physics.bin"]=Repack(*next.resources.at("resources/physics.bin"),[](auto& blocks){EiemPhysicsInput::Reader reader{blocks.at("@physics")};reader.S();reader.S();reader.Count();reader.S();reader.S();reader.B();reader.B();double value=0.9;memcpy(blocks.at("@physics").data()+reader.p,&value,8);});reject(next);
 next=setup.package;++next.meshes.at("M").vertices;reject(next);next=setup.package;++next.textures.at("T").width;reject(next);next=setup.package;next.parts.at("P").material="changed";reject(next);
 next=setup.package;next.resources["resources/material.bin"]=Repack(*next.resources.at("resources/material.bin"),[](auto& blocks){blocks["extra"]={1};});reject(next);
 auto legacy=artifact;legacy.explicitResources=false;Require(!Rebind(legacy,setup.package).compatible,"legacy artifact used unproven author bindings");
 auto prototype=artifact;Writer oldBinding;oldBinding.S("EFF_RESOURCE_BINDINGS/explicit-fields+mesh-schemas/le64");prototype.resourceBindingBytes=std::move(oldBinding.b);
 Require(!Rebind(prototype,setup.package).compatible,"old content-dependent marker template bypassed stable identity migration");
 auto untouched=artifact;auto same=Rebind(untouched,setup.package);Require(same.compatible&&same.resources.empty()&&same.generated.empty(),"unchanged package did unnecessary resource regeneration");
 std::cout<<"PASS addressed fallback: unbound bytes, array count, generated TRS, layout, geometry/texture declarations and legacy artifact\n";
}
void DuplicateGeneratedContexts(){
 Setup setup;auto edited=setup.Full(setup.package);auto artifact=setup.Compiled(edited);auto bindings=Decode(artifact.resourceBindingBytes,setup.package);Require(!bindings.meshes.empty(),"Mesh schema not persisted");
 std::vector<Bindings> inputs(2);Capture(inputs[0],edited);Capture(inputs[1],edited);auto duplicate=setup.Compiled(edited);Finalize(duplicate,inputs);
 Require(Decode(duplicate.resourceBindingBytes,setup.package).ranges==bindings.ranges,"duplicate native consumers changed resource dependency ranges");
 std::cout<<"PASS duplicate generated contexts: shared bounds across native inputs retain one deterministic payload\n";
}
} // namespace AddressedTest
int main(){try{AddressedTest::MixedReplay();AddressedTest::Fallbacks();AddressedTest::DuplicateGeneratedContexts();return 0;}catch(const std::exception& e){std::cerr<<"FAIL addressed resources: "<<e.what()<<'\n';return 1;}}

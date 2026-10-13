#include "../../src/eiem_runtime_author_directory.h"
#include <iostream>
using namespace EiemRuntimeAuthorDirectory;
template<class F>void Rejected(F f){bool rejected=false;try{f();}catch(const std::exception&){rejected=true;}Require(rejected,"invalid author directory accepted");}
int main(){try{
 Directory d;d.mod="fixture";MeshAsset m;m.part="body";m.file="meshes/body.mesh";m.path="assets/source.asset";m.marker="body__EFF_PART";m.hash=std::string(64,'A');m.material="metal";m.vertices=3;m.indices=3;m.bones={"Bip/Pelvis","Bip/Head"};m.shapes={"Smile"};d.meshes.push_back(m);
 MaterialEntry material;material.id="metal";material.source={"assets/another/metal.mat","Metal"};material.edits.floats["_UseEmission"]=1;
 material.edits.integers["_Mode"]=-2;material.edits.colors["_EmissionColor"]={1,0,0,1};material.edits.scales["_MainTex"]={2,1};material.edits.offsets["_MainTex"]={0.25,0};material.edits.keywords["_EMISSION"]=true;material.edits.keywords["_ALPHATEST_ON"]=false;material.edits.tags["RenderType"]="Transparent";material.edits.queue=-1;d.materials.push_back(material);
 d.rules.push_back({"replace-body",{{0,{"assets/source.asset","Body"}},{1,{"assets/model.fbx","Body_lod1"}}},{"body"},false});
 d.rules.push_back({"hide",{{0,{"assets/hidden.asset","Hidden"}}},{},true});
 auto bytes=Encode(d);auto back=Decode(bytes);Require(Encode(back)==bytes&&back.materials[0].edits.integers.at("_Mode")==-2&&back.materials[0].edits.queue==-1,"author operations/bindings changed on roundtrip");
 TextureAsset t;t.id="diffuse";t.file="textures/diffuse.tex";t.path="assets/texture.asset";t.hash=std::string(64,'B');t.width=t.height=1024;t.mips=11;d.textures.push_back(t);
 auto shared=t;shared.id="another-slot";d.textures.push_back(shared);d.materials[0].textures["_MainTex"]="diffuse";
 Require(Encode(Decode(Encode(d)))==Encode(d),"shared author Texture file/bindings changed on roundtrip");
 auto badTexture=d;badTexture.textures[1].hash=std::string(64,'C');Rejected([&]{Encode(badTexture);});
 auto corrupt=bytes;corrupt.back()^=1;Rejected([&]{Decode(corrupt);});corrupt=bytes;corrupt.resize(12);Rejected([&]{Decode(corrupt);});
 auto invalid=d;invalid.meshes[0].file="../outside.mesh";Rejected([&]{Encode(invalid);});invalid=d;invalid.meshes.push_back(m);Rejected([&]{Encode(invalid);});invalid=d;invalid.materials[0].edits.floats["_UseEmission"]=std::numeric_limits<double>::infinity();Rejected([&]{Encode(invalid);});
 Directory empty;empty.mod="empty";Require(Decode(Encode(empty)).meshes.empty(),"empty author asset directory rejected");
 std::cout<<"PASS source-independent author directory: native assets, exact identities, bone/shape layout, typed Material operations, checksum and malformed inputs\n";return 0;
 }catch(const std::exception& error){std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}}

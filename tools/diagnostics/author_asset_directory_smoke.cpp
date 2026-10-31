#include "../../src/eiem_runtime_author_directory.h"
#include <iostream>
using namespace EiemRuntimeAuthorDirectory;
template<class F>void Rejected(F f){bool rejected=false;try{f();}catch(const std::exception&){rejected=true;}Require(rejected,"invalid author directory accepted");}
int main(){try{
 Directory d;d.mod="fixture";MeshAsset m;m.part="body";m.file="meshes/body.mesh";m.path="assets/source.asset";m.marker="body__EFF_PART";m.hash=std::string(64,'A');m.material="metal";m.vertices=3;m.indices=3;m.nativeBytes=32;m.authorBytes=64;m.authorHash=std::string(64,'F');m.bones={"Bip/Pelvis","Bip/Head"};m.shapes={"Smile"};m.binding.hashes={123,456};m.binding.boneBounds={{{0,1,0},{1,2,1}},{{1,0,1},{2,1,2}}};m.fields={"m_Name","m_VertexData","m_IndexBuffer"};d.meshes.push_back(m);
 MaterialEntry material;material.id="metal";material.file="native/materials/metal.effres";material.hash=std::string(64,'C');material.source={"assets/game/metal.mat","MetalBase"};material.authorBytes=64;d.materials.push_back(material);
 d.rules.push_back({"replace-body",{{0,{"assets/source.asset","Body"}},{1,{"assets/model.fbx","Body_lod1"}}},{"body"},false});
 d.rules.push_back({"hide",{{0,{"assets/hidden.asset","Hidden"}}},{},true});
 auto bytes=Encode(d);auto back=Decode(bytes);Require(Encode(back)==bytes&&back.meshes[0].nativeBytes==m.nativeBytes&&back.meshes[0].authorBytes==m.authorBytes&&back.meshes[0].authorHash==m.authorHash&&back.materials[0].source.path==material.source.path&&back.materials[0].source.asset==material.source.asset&&back.materials[0].authorBytes==material.authorBytes&&back.materials[0].textures==material.textures,"author Mesh and sparse Material identities changed on roundtrip");
 auto legacy=bytes;auto structureBytes=Le(legacy,0,8);Require(structureBytes>0&&structureBytes<legacy.size()-40,"author format marker range differs");legacy[8]^=1;
 auto legacyHash=Sha(legacy.data(),legacy.size()-32);std::copy(legacyHash.begin(),legacyHash.end(),legacy.end()-32);
 Rejected([&]{Decode(legacy);});
 auto badMaterial=Decode(Encode(d));badMaterial.materials[0].hash[0]='!';Rejected([&]{Encode(badMaterial);});
 TextureAsset t;t.id="diffuse";t.file="textures/diffuse.texmeta";t.ddsFile="textures/diffuse.dds";t.path="assets/texture.asset";t.hash=std::string(64,'B');t.cab="CAB-"+std::string(32,'E');t.pathId=201;t.pixelOffset=256;t.width=t.height=1024;t.mips=11;t.pixelBytes=EiemDdsTexture::MipBytes(t.width,t.height,t.mips,t.format);d.textures.push_back(t);
 d.materials[0].textures.emplace("_BaseMap","diffuse");
 auto shared=t;shared.id="another-slot";d.textures.push_back(shared);
 Require(Encode(Decode(Encode(d)))==Encode(d),"shared author Texture file/identity changed on roundtrip");
 Auxiliary auxiliary;auxiliary.id="ribbon";auxiliary.file="resources/ribbon.bin";auxiliary.hash=std::string(64,'D');auxiliary.kind=5;auxiliary.bytes=1024;auxiliary.authorBytes=128;auxiliary.authorHash=std::string(64,'E');auxiliary.targets.insert({"assets/source.asset","Body"});d.auxiliary.push_back(auxiliary);
 Require(Encode(Decode(Encode(d)))==Encode(d),"targeted auxiliary declarations changed on roundtrip");
 auto invalidAux=d;invalidAux.auxiliary[0].targets.insert({"assets/other.asset","Other"});Rejected([&]{Encode(invalidAux);});
 invalidAux=d;invalidAux.auxiliary[0].file=d.meshes[0].file;Rejected([&]{Encode(invalidAux);});
 invalidAux=d;invalidAux.auxiliary[0].targets.clear();Rejected([&]{Encode(invalidAux);});
 // Controls are declarations, never old resource payloads or consumer proofs.
 d.runtime.defaults={{"$visible",1},{"$smile",0},{"$speed",1},{"$fx_petals",1},{"$red",0}};d.runtime.persistent={"$visible","$speed"};
 d.runtime.controls={{"$visible",{"衣服",0,1}},{"$smile",{"微笑",0,1}},{"$fx_petals",{"花瓣",0,1}},{"$red",{"红色",0,1}}};
 Binding key;key.id="toggle";key.variable="$visible";key.type="cycle";key.key.vk='K';key.label="切换衣服";key.values={0,1};d.runtime.bindings.push_back(key);
 auto expression=EiemExpression::Make();expression->op=EiemExpression::Variable;expression->variable="$visible";d.runtime.parts["body"]={expression,{{"Smile","$smile"}}};
 ShapeAnimation animation;animation.id="smile";animation.speedVariable="$speed";animation.duration=1;animation.tracks["$smile"]={0,1,0};d.runtime.animations.emplace(animation.id,animation);
 d.runtime.materialTracks.emplace("$red",MaterialTrack{"metal","_Color","COLOR",0});
 EffectAttachment effect;effect.id="petals";effect.part="body";effect.bone="Bip/Pelvis";effect.variable="$fx_petals";auto native=std::make_shared<EiemEffectNative::Manifest>();native->key=std::string(64,'E');native->prefab="assets/effect.prefab";native->rootName="Petals";native->particles=1;
 EiemEffectNative::Bundle input;input.file="effects/petals.bundle";input.hash=std::string(64,'F');input.size=256;native->bundles.push_back(input);effect.native=native;d.runtime.effects.emplace(effect.id,effect);
 auto state=Decode(Encode(d));Require(state.runtime.parts.at("body").visible->Evaluate(state.runtime.defaults)==1&&state.runtime.bindings.front().label==key.label&&state.runtime.animations.at("smile").tracks.at("$smile")==animation.tracks.at("$smile")&&state.runtime.effects.at("petals").native->bundles.front().Size()==256&&!state.runtime.effects.at("petals").native->bundles.front().Data(),"controls/samples/lazy effect declarations changed on roundtrip");
 auto badState=d;badState.runtime.defaults.erase("$visible");Rejected([&]{Encode(badState);});badState=d;badState.runtime.parts["body"].shapes["Missing"]="$smile";Rejected([&]{Encode(badState);});
 badState=d;auto badNative=std::make_shared<EiemEffectNative::Manifest>(*native);badNative->bundles.front().dependencies={0};badState.runtime.effects.at("petals").native=badNative;Rejected([&]{Encode(badState);});
 badState=d;badNative=std::make_shared<EiemEffectNative::Manifest>(*native);badNative->bundles.front().file="../outside.bundle";badState.runtime.effects.at("petals").native=badNative;Rejected([&]{Encode(badState);});
 badState=d;badState.runtime.animations.at("smile").tracks.at("$smile")[1]=2;Rejected([&]{Encode(badState);});
 auto cyclic=EiemExpression::Make();cyclic->op=EiemExpression::Not;cyclic->left=cyclic;badState=d;badState.runtime.parts["body"].visible=cyclic;Rejected([&]{Encode(badState);});cyclic->left.reset();
 auto badTexture=d;badTexture.textures[1].format=3;Rejected([&]{Encode(badTexture);});
 auto badBinding=d;badBinding.meshes[0].binding.hashes.pop_back();Rejected([&]{Encode(badBinding);});
 badBinding=d;badBinding.meshes[0].binding.boneBounds[0].second[0]=-1;Rejected([&]{Encode(badBinding);});
 badBinding=d;badBinding.meshes[0].fields.pop_back();Rejected([&]{Encode(badBinding);});
 badBinding=d;badBinding.meshes[0].fields.push_back("m_Name");Rejected([&]{Encode(badBinding);});
 auto corrupt=bytes;corrupt.back()^=1;Rejected([&]{Decode(corrupt);});corrupt=bytes;corrupt.resize(12);Rejected([&]{Decode(corrupt);});
 auto invalid=d;invalid.meshes[0].file="../outside.mesh";Rejected([&]{Encode(invalid);});invalid=d;invalid.meshes.push_back(m);Rejected([&]{Encode(invalid);});invalid=d;invalid.materials[0].textures["_MainTex"]="missing";Rejected([&]{Encode(invalid);});
 Directory empty;empty.mod="empty";Require(Decode(Encode(empty)).meshes.empty(),"empty author asset directory rejected");
 std::cout<<"PASS source-independent author directory: native Material inputs/dependencies, Texture identities, bone/shape layout, checksum and malformed inputs\n";return 0;
 }catch(const std::exception& error){std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}}

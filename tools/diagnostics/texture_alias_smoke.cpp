#define EIEM_ADDRESSED_FIXTURE_ONLY
#include "addressed_resources_smoke.cpp"
using namespace AddressedTest;
void TextureAliases(){
 Setup s;auto& f=s.fixture;f.List(300,"m_PreloadTable",{10,11,12,13,20,21,22,23,100,200,400});
 auto& bt=f.T(300);for(auto name:{"m_Container","m_HashContainer"})for(auto&row:ArrayValue(bt,Child(bt,f.V(300),name)).children)SetInteger(bt,Child(bt,Child(bt,row,"second"),"preloadSize"),11);SetInteger(bt,Child(bt,Child(bt,f.V(300),"m_MainAsset"),"preloadSize"),11);
 auto p=s.package;p.textures.emplace("T_duplicate",p.textures.at("T"));p.materials.emplace("A_duplicate",p.materials.at("A"));p.materials.at("A_duplicate").textures["_MainTex"]="T_duplicate";
 auto before=f.BytesOf();auto build=[&](const Package&author){EndfieldInput::Bundle b;b.members={{f.cab,4,before},{f.cab+".resS",0,Bytes{1,2,3,4}}};auto result=BuildTextureInput(b,author,s.assets);return std::make_pair(std::move(b),std::move(result));};
 auto built=build(p);auto layout=ParseSerialized(built.first.members.front().bytes);size_t textures=0;for(const auto&o:layout.objects)textures+=layout.types.at(o.second.type).classId==28;
 Require(textures==2&&built.second.textures.size()==2&&built.first.members[1].bytes.size()==20,"duplicate Texture declarations created extra native objects or pixel payload");
 auto id=AllocateTextureId(f.cab,p,"T",p.textures.at("T"));Require(id==AllocateTextureId(f.cab,p,"T_duplicate",p.textures.at("T_duplicate")),"same immutable Texture dependency did not alias");
 for(auto section:{"A","A_duplicate"}){auto m=AllocateMaterialId(f.cab,p,section);const auto&t=layout.types.at(layout.objects.at(m).type);auto v=ObjectValue(built.first.members[0].bytes,layout,m);const auto&env=ArrayValue(t,Child(t,Child(t,v,"m_SavedProperties"),"m_TexEnvs")).children.front();Require(NullableReference(t,Child(t,Child(t,env,"second"),"m_Texture"),layout,f.cab)==NativeReference{f.cab,id},"Material did not use shared Texture");}
 auto checkSeparate=[&](const Package&author){auto b=build(author);auto l=ParseSerialized(b.first.members[0].bytes);size_t count=0;for(const auto&o:l.objects)count+=l.types.at(o.second.type).classId==28;Require(count==3,"independently editable Texture declarations were merged");};
 auto different=p;different.textures.at("T_duplicate").linear=!different.textures.at("T_duplicate").linear;checkSeparate(different);
 different=p;different.resources.emplace("resources/separate-texture.bin",different.resources.at("resources/texture.bin"));different.textures.at("T_duplicate").resource="resources/separate-texture.bin";checkSeparate(different);
 different=p;different.id="OtherMod";Require(id!=AllocateTextureId(f.cab,different,"T",different.textures.at("T")),"independent Mods shared mutable resource identity");
 std::cout<<"PASS Texture aliases: one native object/pixel stream for the same dependency; both Materials/preloads; sampling differences, independent files and Mods isolated\n";
}
int main(){try{TextureAliases();return 0;}catch(const std::exception&e){std::cerr<<"FAIL Texture alias: "<<e.what()<<"\n";return 1;}}

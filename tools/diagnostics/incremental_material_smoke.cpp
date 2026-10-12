#include "../../src/eiem_resource_compiler.h"
#include "../../src/eiem_compiled_mod_writer.h"
#include <iostream>
using namespace EiemCompiledModWriter;
void VerifiedAuthorRegression(){
 Bytes readerBytes;EiemOfflinePackage::Reader reader{readerBytes};Package snapshot;snapshot.id="verified";
 auto native=std::make_shared<Resource>();native->kind=1;native->bytes={1,2,3,4};native->contentHash=Hash(native->bytes);snapshot.resources.emplace("meshes/one.mesh",native);reader.verifiedAuthor=&snapshot;
 Package published;published.id=snapshot.id;published.root=std::filesystem::temp_directory_path()/L"eff-nonexistent-author-snapshot";
 Resource loaded;loaded.kind=native->kind;loaded.type=native->type;ReadAuthorReference(reader,published,"meshes/one.mesh",native->contentHash,loaded);Require(loaded.bytes==native->bytes,"verified author snapshot was not reused");
 auto reject=[&](Resource mismatch,const std::string& hash){bool failed=false;try{ReadAuthorReference(reader,published,"meshes/one.mesh",hash,mismatch);}catch(const std::exception&){failed=true;}Require(failed,"unverified author resource avoided disk validation");};
 reject(loaded,std::string(64,'A'));auto mismatch=loaded;mismatch.type[0]^=1;reject(mismatch,native->contentHash);mismatch=loaded;mismatch.kind=2;reject(mismatch,native->contentHash);published.id="different";reject(loaded,native->contentHash);
}
void NullTextureSlotRegression(const std::filesystem::path& mod,const std::filesystem::path& game,const Artifact& artifact){
 std::string materialId,property,textureId;
 for(const auto& input:artifact.manifest.plan.originals){auto bundle=EndfieldInput::Decode(ReadSource({},input));
  for(const auto& member:bundle.members){if(!Serialized(member.bytes))continue;auto layout=ParseSerialized(member.bytes);auto assets=SerializedAssets(member.bytes,member.name,layout);
   for(const auto& material:artifact.manifest.package.materials){if(material.second.textures.empty())continue;
    auto asset=std::find_if(assets.begin(),assets.end(),[&](const auto& a){return a.classId==21&&a.path==material.second.source.path&&a.asset==material.second.source.asset;});if(asset==assets.end())continue;
    auto value=ObjectValue(member.bytes,layout,asset->pathId);const auto& type=layout.types.at(layout.objects.at(asset->pathId).type);
    for(const auto& pair:ArrayValue(type,Child(type,Child(type,value,"m_SavedProperties"),"m_TexEnvs")).children){const auto& key=Child(type,pair,"first");auto name=Text(key.raw.data(),key.raw.size());
     if(material.second.textures.count(name)||Integer(Child(type,Child(type,Child(type,pair,"second"),"m_Texture"),"m_PathID")))continue;
     materialId=material.first;property=name;textureId=material.second.textures.begin()->second;break;
    }if(!property.empty())break;
   }if(!property.empty())break;
  }if(!property.empty())break;
 }
 Require(!property.empty(),"real Mod has no native null texture slot for regression");auto bytes=Read(mod/L"mod.ini");std::string ini(bytes.begin(),bytes.end());auto section=ini.find("["+materialId+"]");Require(section!=std::string::npos,"null slot regression material section absent");auto end=ini.find("\n[",section);if(end==std::string::npos)end=ini.size();ini.insert(end,"\ntexture."+property+"="+textureId+"\n");EiemOfflineCompile::AtomicSave(mod/L"mod.ini",Bytes(ini.begin(),ini.end()));
 Export(mod,game);auto compiled=EiemCompiledMod::Read(mod,game);const auto& slots=compiled.manifest.plan.materials.at(materialId).slots;auto slot=std::find_if(slots.begin(),slots.end(),[&](const auto& s){return s.property==property;});Require(slot!=slots.end()&&slot->nonnull&&slot->local,"native null texture slot was not bound to compiled author pixels");
 std::cout<<"PASS native empty texture slot: "<<materialId<<" / "<<property<<" references authored texture\n";
}
int wmain(int argc,wchar_t** argv){try{
 VerifiedAuthorRegression();
 Require(argc==5,"incremental_material_smoke <game> <original-Mod> <edited-Mod> <export-folder>");
 auto game=std::filesystem::canonical(argv[1]),original=std::filesystem::canonical(argv[2]),edited=std::filesystem::canonical(argv[3]);
 auto before=EiemCompiledMod::Read(original,game),after=EiemCompiledMod::Read(edited,game);
 const auto& author=after.manifest.package;auto changes=MaterialParameterChanges(author,before.manifest.package);Require(!changes.empty(),"real parameter edit did not take the selective path");
 auto unsafe=author;auto geometry=unsafe.meshes.begin()->second.resource;auto resource=std::make_shared<Resource>(*unsafe.resources.at(geometry));resource->contentHash=std::string(64,'A');unsafe.resources[geometry]=resource;
 Require(MaterialParameterChanges(unsafe,before.manifest.package).empty(),"geometry edit was accepted as material-only");
 unsafe=author;auto& material=unsafe.materials.begin()->second;material.textures["_NewSlot"]="changed";Require(MaterialParameterChanges(unsafe,before.manifest.package).empty(),"texture reference edit was accepted as parameter-only");
 auto temporary=std::filesystem::temp_directory_path()/(L"eff-material-regression-"+std::to_wstring(GetCurrentProcessId()));Require(std::filesystem::create_directory(temporary),"material regression directory already exists");
 struct Cleanup{std::filesystem::path path;~Cleanup(){std::error_code error;std::filesystem::remove_all(path,error);}}cleanup{temporary};
 auto cold=temporary/L"mod";std::filesystem::copy(edited,cold,std::filesystem::copy_options::recursive);std::filesystem::remove(cold/L"compiled.bin");
 Export(cold,game);auto expected=EiemCompiledMod::Read(cold,game);Require(after.views.size()==expected.views.size(),"incremental source coverage differs from full compile");
 size_t matched=0;for(size_t i=0;i<after.views.size();++i){Require(after.manifest.plan.originals[i].hash==expected.manifest.plan.originals[i].hash,"incremental original source differs");
  Require(after.views[i].startupHash==expected.views[i].startupHash&&after.views[i].size==expected.views[i].size,"incremental native view bytes differ from full compile");++matched;
 }
 // Export compiles in an atomic staging folder before publishing. Decode
 // deliberately relocates the author root to the published Mod directory.
 // Restore only that serialized folder name when checking the original bytes.
 auto disk=EiemNativeInput::Read(edited/L"compiled.bin");auto exported=after;exported.manifest.package.root=edited.parent_path()/argv[4];
 Require(EiemCompiledMod::Encode(exported,game)==disk,"incremental compiled roundtrip differs beyond author relocation");
 auto normalized=EiemCompiledMod::Encode(after,game);auto roundtrip=EiemCompiledMod::Decode(normalized,edited,game);
 Require(EiemCompiledMod::Encode(roundtrip,game)==normalized,"relocated compiled roundtrip is not canonical");
 NullTextureSlotRegression(cold,game,expected);
 std::cout<<"PASS incremental material: "<<matched<<" native views exactly equal full compile, canonical decode, geometry and texture edits excluded\n";return 0;
 }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}

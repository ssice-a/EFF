#include "../src/eiem_author_mesh_bundle.h"
#include "../src/eiem_author_texture_bundle.h"
#include "../src/eiem_file_identity.h"
#include "../src/eiem_author_resource_file.h"
#include "../src/eiem_author_resource_metadata.h"
#include "../src/eiem_native_random_access.h"
#include "../src/eiem_runtime_author_directory.h"
#include "../src/eiem_runtime_author_manifest.h"
#include <chrono>

namespace AuthorAssets {
using namespace EiemNativeInput;
struct Template {std::string kind,cab,hash;int64_t id=0;std::filesystem::path file;};
struct Source {Bytes bytes;SerializedLayout layout;std::string hash;};
using Templates=std::map<Identity,Template>;
struct TypedReader {
 const unsigned char* data=nullptr;size_t size=0,at=0;
 void Need(size_t count){Require(at<=size&&count<=size-at,"typed author input is truncated");}
 uint64_t N(){Need(8);auto value=Le(data,size,at,8);at+=8;return value;}
 int64_t I64(){auto bits=N();int64_t value;memcpy(&value,&bits,sizeof(value));return value;}
 size_t Count(){auto field=at;auto value=N();Require(value<=100000&&value<=(size-at)/8,
  "typed author record count is invalid at byte "+std::to_string(field)+" value="+
  std::to_string(value)+" remaining="+std::to_string(size-at));return size_t(value);}
 size_t Index(){auto value=N();Require(value<=SIZE_MAX,"typed author index exceeds address space");return size_t(value);}
 std::string S(){auto count=N();Require(count<=SIZE_MAX,"typed author string extent overflow");Need(size_t(count));auto value=Text(data+at,size_t(count));at+=size_t(count);return value;}
 Bytes Blob(){auto count=N();Require(count<=SIZE_MAX,"typed author blob extent overflow");Need(size_t(count));Bytes value(data+at,data+at+size_t(count));at+=size_t(count);return value;}
 std::vector<std::string> Strings(){std::vector<std::string> result;auto count=Count();result.reserve(count);for(size_t i=0;i<count;++i)result.push_back(S());return result;}
 double D(){auto bits=N();double value;memcpy(&value,&bits,8);Require(std::isfinite(value),"typed author number is not finite");return value;}
};
struct TemplateRef {Identity identity;Template value;};
struct MeshInput {std::string part,meshId,file,material;Identity target;size_t templateIndex=0;Mesh mesh;Resource fields;Bytes author;};
struct TextureInput {std::string id,file;size_t templateIndex=0;};
inline std::vector<TemplateRef> ReadTemplates(TypedReader& reader){
 std::vector<TemplateRef> result;std::set<Identity> identities;auto count=reader.Count();result.reserve(count);
  for(size_t i=0;i<count;++i){TemplateRef row;row.value.kind=reader.S();row.identity={reader.S(),reader.S()};row.value.cab=reader.S();row.value.id=reader.I64();row.value.file=std::filesystem::u8path(reader.S());row.value.hash=reader.S();
  Require((row.value.kind=="Mesh"||row.value.kind=="Texture2D")&&!row.identity.path.empty()&&!row.identity.asset.empty()&&!row.value.cab.empty()&&row.value.id!=0&&row.value.file.is_absolute()&&DigestText(row.value.hash),"typed author template declaration differs");
  Require(identities.insert(row.identity).second,"typed author template identity duplicated");result.push_back(std::move(row));
 }return result;
}
inline Templates TemplateMap(const std::vector<TemplateRef>& rows){Templates result;for(const auto& row:rows)result.emplace(row.identity,row.value);return result;}
struct Sources {
 const Templates& templates;std::map<std::filesystem::path,Source> files;
 std::pair<const Template&,const Source&> Get(const Identity& identity,const char* kind){
  auto t=templates.find(identity);Require(t!=templates.end()&&t->second.kind==kind,"exact author template absent: "+identity.path+" / "+identity.asset);
  auto found=files.find(t->second.file);if(found==files.end()){
   auto before=FileStamp(t->second.file);Source file;file.bytes=Read(t->second.file);auto hash=Sha(file.bytes.data(),file.bytes.size());file.hash=Hex(hash.data(),hash.size());
   Require(SameStamp(before,FileStamp(t->second.file))&&file.hash==t->second.hash,"author template content changed");file.layout=ParseSerialized(file.bytes);found=files.emplace(t->second.file,std::move(file)).first;
  }Require(found->second.hash==t->second.hash,"shared template digest differs");
  auto asset=SerializedAssets(found->second.bytes,t->second.cab,found->second.layout);
  auto classId=!strcmp(kind,"Mesh")?43u:28u;
  Require(std::any_of(asset.begin(),asset.end(),[&](const auto& row){return row.path==identity.path&&row.asset==identity.asset&&row.pathId==t->second.id&&row.classId==classId;}),"author template asset is not its declared logical identity");
  return {t->second,found->second};
 }
};
inline void AddBlock(Resource& resource,const std::string& name,const Bytes& value){
 Require(!name.empty()&&resource.blocks.find(name)==resource.blocks.end(),"typed Mesh field is duplicated");
 Block block;block.offset=resource.bytes.size();block.size=value.size();resource.bytes.insert(resource.bytes.end(),value.begin(),value.end());resource.blocks.emplace(name,block);
}
inline Bytes EncodeStrings(const std::vector<std::string>& values){
 Require(values.size()<=UINT32_MAX,"typed Mesh binding count overflow");Bytes result(4);EndfieldInput::Put(result,0,values.size(),4);
 for(const auto& value:values){Require(!value.empty()&&value.size()<=UINT16_MAX,"typed Mesh binding string extent invalid");auto at=result.size();result.resize(at+2);EndfieldInput::Put(result,at,value.size(),2);result.insert(result.end(),value.begin(),value.end());}return result;
}
inline void Save(const std::filesystem::path& root,const std::string& relative,const Bytes& bytes){
 EiemRuntimeAuthorDirectory::Relative(relative);auto path=root/std::filesystem::u8path(relative);std::filesystem::create_directories(path.parent_path());
 std::ofstream out(path,std::ios::binary|std::ios::trunc);Require(bool(out),"typed author resource output open failed: "+relative);
 out.write((const char*)bytes.data(),std::streamsize(bytes.size()));Require(bool(out),"typed author resource output write failed: "+relative);
}
inline MeshInput ReadMesh(TypedReader& reader,const std::vector<TemplateRef>& templates){
 MeshInput row;row.part=reader.S();row.meshId=reader.S();row.file=reader.S();row.material=reader.S();row.target={reader.S(),reader.S()};row.templateIndex=reader.Index();
 Require(row.templateIndex<templates.size()&&templates[row.templateIndex].value.kind=="Mesh","typed Mesh template index invalid");
 row.mesh.resource=row.file;auto vertices=reader.N(),indices=reader.N();Require(vertices<=UINT_MAX&&indices<=UINT_MAX,"typed Mesh geometry extent overflow");row.mesh.vertices=unsigned(vertices);row.mesh.indices=unsigned(indices);row.mesh.bones=reader.Strings();row.mesh.shapes=reader.Strings();
 row.fields.kind=1;row.fields.type=std::array<unsigned char,16>{};auto fieldCount=reader.Count();Require(fieldCount>0,"typed Mesh has no native fields");
 for(size_t i=0;i<fieldCount;++i){auto name=reader.S();Require(name.empty()||name.front()!='@',"typed Mesh field collides with metadata");AddBlock(row.fields,name,reader.Blob());}
 Bytes geometry(8);EndfieldInput::Put(geometry,0,row.mesh.vertices,4);EndfieldInput::Put(geometry,4,row.mesh.indices,4);AddBlock(row.fields,"@geometry",geometry);
 AddBlock(row.fields,"@bones",EncodeStrings(row.mesh.bones));AddBlock(row.fields,"@shapes",EncodeStrings(row.mesh.shapes));row.author=reader.Blob();
 Require(Identifier(row.part)&&Begins(row.meshId,"Mesh")&&Identifier(row.meshId)&&Identifier(row.material)&&!row.file.empty()&&row.target.path.rfind("assets/",0)==0,"typed Mesh identity differs");
 return row;
}
inline TextureInput ReadTexture(TypedReader& reader,const std::vector<TemplateRef>& templates){
 TextureInput row;row.id=reader.S();row.file=reader.S();row.templateIndex=reader.Index();
 Require(row.templateIndex<templates.size()&&templates[row.templateIndex].value.kind=="Texture2D",
         "typed Texture template index invalid");
 Require(Identifier(row.id)&&!row.file.empty(),"typed Texture identity/path differs");return row;
}
inline std::string Export(const unsigned char* data,size_t size,const std::filesystem::path& output){
 auto started=std::chrono::steady_clock::now();TypedReader reader{data,size};
 Require(reader.S()=="EFF_AUTHOR_TYPED_INPUT/1","typed author input signature/version differs");auto mod=reader.S();Require(Identifier(mod),"typed author Mod identity invalid");
 auto templates=ReadTemplates(reader);auto templateMap=TemplateMap(templates);Sources sources{templateMap};
 auto meshCount=reader.Count();std::set<std::string> outputFiles;std::vector<MeshInput> expectedMeshes;size_t nativeBytes=0;
 auto target=std::filesystem::weakly_canonical(output);Require(std::filesystem::is_directory(target),"typed author output directory must exist");
 for(size_t i=0;i<meshCount;++i){auto row=ReadMesh(reader,templates);Require(outputFiles.insert(row.file).second,"typed Mesh output path duplicated");auto templateRow=sources.Get(row.target,"Mesh");auto& source=templateRow.second.bytes;auto meshId=templateRow.first.id;
  row.fields.type=templateRow.second.layout.types.at(templateRow.second.layout.objects.at(meshId).type).hash;
  auto asset=EiemAuthorMeshBundle::Build(mod,row.part,row.material,row.target,source,templateRow.first.cab,meshId,row.fields,row.mesh,row.file,row.author);
  EiemRuntimeAuthorDirectory::MeshAsset entry;entry.part=row.part;entry.file=row.file;entry.path=row.target.path;entry.marker=asset.meshName;entry.material=row.material;entry.vertices=asset.vertices;entry.indices=asset.indices;entry.bones=row.mesh.bones;entry.shapes=row.mesh.shapes;entry.binding=std::move(asset.binding);entry.fields=std::move(asset.fields);entry.nativeBytes=asset.bytes.size();entry.authorBytes=row.author.size();
  auto nativeHash=Sha(asset.bytes.data(),asset.bytes.size());entry.hash=Hex(nativeHash.data(),nativeHash.size());auto authorHash=Sha(row.author.data(),row.author.size());entry.authorHash=Hex(authorHash.data(),authorHash.size());
  auto metadata=EiemAuthorResourceMetadata::EncodeMesh(entry);Bytes payload=std::move(asset.bytes);payload.insert(payload.end(),row.author.begin(),row.author.end());auto envelope=EiemAuthorResourceFile::Encode(1,metadata,payload);Save(target,row.file,envelope);nativeBytes+=envelope.size();expectedMeshes.push_back(std::move(row));
 }
 auto textureCount=reader.Count();
 std::vector<TextureInput> expectedTextures;
 for(size_t i=0;i<textureCount;++i){auto row=ReadTexture(reader,templates);Require(outputFiles.insert(row.file).second,"typed Texture output path duplicated");EiemRuntimeAuthorDirectory::Relative(row.file);
  const auto& templateRow=templates[row.templateIndex];auto templateSource=sources.Get(templateRow.identity,"Texture2D");
  auto asset=EiemAuthorTextureBundle::BuildScaffold(templateSource.second.bytes,templateRow.value.cab,
      templateRow.value.id,templateRow.identity.path,mod+"/"+row.file);
  EiemNativeRandomAccess::Reader native(asset.bytes.size(),[&asset](size_t at,unsigned char* out,size_t n){Require(at<=asset.bytes.size()&&n<=asset.bytes.size()-at,"typed Texture scaffold read exceeds output");memcpy(out,asset.bytes.data()+at,n);});
  Require(native.header.bytes==asset.bytes.size()&&native.directory.members.size()==1,
      "typed Texture scaffold does not match DLL native input extent/member contract");
  const auto& member=native.directory.members.front();auto layout=native.Layout(member);
  Require((member.flags&4)&&layout.externals.empty()&&layout.objects.size()==2,
      "typed Texture scaffold is not an isolated local asset");
  auto names=native.Assets(member,layout);
  Require(names.size()==1&&names.front().classId==28&&names.front().path==templateRow.identity.path&&
      names.front().asset==mod+"/"+row.file&&names.front().cab==member.name,
      "typed Texture scaffold generated identity differs from DLL runtime contract");
  auto envelope=EiemAuthorResourceFile::Encode(3,{},asset.bytes);Save(target,row.file,envelope);nativeBytes+=envelope.size();expectedTextures.push_back(std::move(row));
 }
 Require(reader.at==reader.size,"typed author input has trailing bytes");
 auto manifest=EiemRuntimeAuthorManifest::Read(target);Require(manifest.id==mod,"typed author final Format 3 manifest identity differs");
 for(const auto& row:expectedMeshes){auto mesh=manifest.resources.find(row.meshId);Require(mesh!=manifest.resources.end()&&mesh->second.kind==1&&mesh->second.path==row.file,"typed Mesh output differs from mod.ini: "+row.meshId);
  auto part=manifest.parts.find(row.part);Require(part!=manifest.parts.end()&&part->second.mesh==row.meshId&&part->second.material==row.material,"typed Mesh Part binding differs from mod.ini: "+row.part);
  Require(std::any_of(manifest.targets.begin(),manifest.targets.end(),[&](const auto& rule){return rule.source.path==row.target.path&&rule.source.asset==row.target.asset&&std::find(rule.partIds.begin(),rule.partIds.end(),row.part)!=rule.partIds.end();}),"typed Mesh target is not declared in mod.ini: "+row.target.path+" / "+row.target.asset);
 }
 for(const auto& row:expectedTextures){auto texture=manifest.resources.find(row.id);Require(texture!=manifest.resources.end()&&texture->second.kind==3&&texture->second.path==row.file&&!texture->second.dds.empty(),"typed Texture output differs from mod.ini: "+row.id);}
 auto elapsed=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count();
 std::ostringstream report;report<<"AUTHOR-TYPED-FORMAT3 meshes="<<meshCount<<" textures="<<textureCount<<" nativeBytes="<<nativeBytes<<" templateFiles="<<sources.files.size()<<" milliseconds="<<elapsed;return report.str();
}
}

extern "C" __declspec(dllexport) unsigned __cdecl EFF_AuthorAssetsAbi(){return 3;}
extern "C" __declspec(dllexport) int __cdecl EFF_AuthorAssets_ExportTypedW(
 const unsigned char* input,size_t inputSize,const wchar_t* output,char* response,size_t responseCapacity){
 auto copy=[&](const std::string& value,int status){if(!response||!responseCapacity)return 2;auto count=std::min(value.size(),responseCapacity-1);if(count)memcpy(response,value.data(),count);response[count]=0;return count==value.size()?status:2;};
 try{EiemDirectory::Require(input&&inputSize&&output,"typed author writer received empty input or null output");EiemDirectory::Require(inputSize<=size_t(2)*1024*1024*1024,"typed author input exceeds 2 GiB");
  auto root=std::filesystem::weakly_canonical(std::filesystem::path(output));auto report=AuthorAssets::Export(input,inputSize,root);return copy(report,0);
 }catch(const std::exception& error){return copy(std::string("FAIL ")+error.what(),1);}catch(...){return copy("FAIL unknown typed author writer error",1);}
}

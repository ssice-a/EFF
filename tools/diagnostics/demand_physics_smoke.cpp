#include "../../src/eiem_physics_input_builder.h"
#include "../../src/eiem_mesh_target_directory.h"
#include "../../src/eiem_native_serialized_splice.h"
#include "../../src/eiem_native_edit_transaction.h"
#include "../../src/eiem_demand_native_assembly.h"
#include <iostream>
#include <thread>
using namespace EiemNativeInput;
namespace R=EiemNativeRandomAccess;namespace P=EiemNativeSerializedSplice;namespace S=EiemNativeSpliceView;
int wmain(int argc,wchar_t** argv){std::string phase="declarations";try{
 {
   Identity target{"assets/character/body.mesh","body"},unrelated{"assets/other.mesh","other"};
   std::vector<std::pair<std::string,EiemPhysicsInput::Document>> loaded;
   for(const auto& mod:std::vector<std::string>{"mod-a","mod-b"}){
     EiemPhysicsInput::Document document;document.id="cloth";EiemPhysicsInput::Template templ;templ.cab="template";document.templates.emplace("template",templ);
     EiemPhysicsInput::Group group;group.id="back-ribbon";group.cab="template";group.targets.insert(target);document.groups.push_back(group);
     EiemPhysicsInput::Group excluded;excluded.id="unrelated";excluded.cab="template";excluded.targets.insert(unrelated);document.groups.push_back(excluded);
     loaded.emplace_back(mod,std::move(document));
   }
   auto combined=EiemDemandNativeAssembly::ComposePhysicsDocuments(loaded,{target});
   Require(combined.size()==2&&combined[0].groups.size()==1&&combined[1].groups.size()==1&&
       combined[0].groups[0].targets.count(target)&&combined[1].groups[0].targets.count(target)&&
       combined[0].groups[0].id!=combined[1].groups[0].id&&combined[0].groups[0].cab!=combined[1].groups[0].cab,
       "overlapping Physics target discarded an independent Mod group or retained unrelated targets");
   std::cout<<"PASS overlapping target keeps independent Mod Physics groups and drops unrelated declarations\n";
 }
 if(argc==2&&std::wstring(argv[1])==L"--overlap-only")return 0;
 if(argc==3&&std::wstring(argv[1])==L"--catalog"){
   EiemDemandAuthorResources::Catalog catalog{std::filesystem::path(argv[2])};Require(!catalog.OpenMeshes()&&!catalog.OpenTextures()&&!catalog.OpenPhysics(),"catalog eagerly opened resources");
   Require(catalog.GetPhysics({}).empty()&&!catalog.OpenPhysics(),"unrelated request loaded Physics");size_t textureMetadata=0,maxMetadata=0;
   std::map<std::string,std::shared_ptr<const EiemDemandAuthorResources::Texture>> textures;
   for(const auto& entry:catalog.Directory().textures){auto texture=catalog.GetTexture(entry.id);Require(texture->metadataReadBytes<64*1024,"Texture opening read pixel payload");auto inserted=textures.emplace(entry.file,texture);Require(inserted.second||inserted.first->second==texture,"shared Texture file opened more than once");if(inserted.second)textureMetadata+=texture->metadataReadBytes;maxMetadata=std::max(maxMetadata,texture->metadataReadBytes);}
   if(!catalog.Directory().textures.empty()){auto id=catalog.Directory().textures.front().id;auto first=catalog.GetTexture(id);std::atomic<bool> same{true};std::vector<std::thread> threads;for(size_t i=0;i<8;++i)threads.emplace_back([&]{if(catalog.GetTexture(id)!=first)same=false;});for(auto& thread:threads)thread.join();Require(same,"concurrent Texture requests duplicated resource");}
   std::set<Identity> targets;for(const auto& entry:catalog.Directory().auxiliary)targets.insert(entry.targets.begin(),entry.targets.end());auto physics=catalog.GetPhysics(targets);auto again=catalog.GetPhysics(targets);Require(physics==again&&catalog.OpenTextures()==textures.size(),"lazy resource cache identity changed");
   Require(!catalog.OpenMeshes(),"Texture/Physics declarations loaded unrelated Meshes");std::cout<<"PASS lazy catalog textures="<<textures.size()<<" textureMetadataBytes="<<textureMetadata<<" maxTextureMetadataBytes="<<maxMetadata<<" physics="<<physics.size()<<" meshPayloadsOpened=0 concurrentTextureRequests=8 pixelPayloadDecoded=0\n";return 0;
 }
 Require(argc==5||argc==6,"usage: demand_physics_smoke observed-file offset Mod observed-log [native-author]");
 auto root=std::filesystem::u8path(std::filesystem::path(argv[3]).u8string());Package package;package.root=root;
 auto declared=EiemMeshTargetDirectory::Read(root);Require(!declared.empty(),"Physics fixture Mod targets absent");package.id=declared.front().mod;
 for(const auto& target:declared)package.targetIndex.emplace(target.source,target.rule);
 for(const auto& section:ParseIni(Read(root/"mod.ini")))if(Begins(section.id,"Resource")){
   const auto& path=section.values.at("path");auto resource=std::make_shared<const Resource>(Decode(Read(DiskPath(root,path,0)),0));
   if(resource->blocks.count("@physics"))package.resources.emplace(path,resource);
 }
 phase="physics-document";auto documents=EiemPhysicsInput::Documents(package);Require(!documents.empty(),"Physics fixture has no authored document");
 AssetIndex assets;auto log=Read(std::filesystem::path(argv[4]));std::istringstream lines(Text(log.data(),log.size()));std::string line;
 std::regex pattern("MESH-IDENTITY.* logical=([^ ]+) asset=([^ ]+) nativeKey=[0-9A-Fa-f]+ cab=([^ ]+) pathId=(-?[0-9]+)");
 while(std::getline(lines,line)){std::smatch match;if(!std::regex_search(line,match,pattern))continue;Identity id{match[1],match[2]};if(!package.targetIndex.count(id))continue;
   Asset asset;asset.path=id.path;asset.asset=id.asset;asset.cab=match[3];asset.pathId=std::stoll(match[4]);asset.classId=43;AddAsset(assets,std::move(asset));}
 for(const auto& document:documents)for(const auto& group:document.groups)for(const auto& target:group.targets)Require(assets.count(target),"Physics fixture target was not observed in game: "+target.asset);
 std::unique_ptr<EiemDemandNativeAssembly::Author> native;std::vector<BuiltMeshPart> meshParts;
 if(argc==6){native=std::make_unique<EiemDemandNativeAssembly::Author>(std::make_shared<EiemDemandAuthorResources::Catalog>(std::filesystem::path(argv[5])));
   for(const auto& target:assets){auto rule=native->package.targetIndex.find(target.first);if(rule==native->package.targetIndex.end())continue;bool first=true;
     for(const auto& part:native->package.rules.at(rule->second).parts){const auto& mesh=native->package.meshes.at(part);auto id=first?target.second.pathId:AllocateMeshId(target.second.cab,native->package,target.first,part);
       meshParts.push_back({target.first,rule->second,part,target.second.cab,target.second.pathId,id,mesh.vertices,mesh.indices,{}});first=false;}}
   // The same Physics declarations are assembled before either Renderer path.
   auto physicsResources=std::move(package.resources);package=native->package;package.resources.insert(physicsResources.begin(),physicsResources.end());
 }
 phase="observed-input";auto file=std::make_shared<R::File>(std::filesystem::path(argv[1]),std::stoull(argv[2]));auto read=[file](size_t at,unsigned char* out,size_t n){file->ReadAt(at,out,n);};R::Reader reader(file->Size(),read);
 size_t changedCount=0,addedCount=0,typesCount=0;
 for(size_t mi=0;mi<reader.directory.members.size();++mi){const auto& member=reader.directory.members[mi];if(!(member.flags&4))continue;auto layout=reader.Layout(member);
   auto begin=std::chrono::steady_clock::now();EiemNativeEditTransaction::Member transaction(reader,mi,layout);
   EiemPhysicsInput::MemberBuilder sparse(transaction.Layout(),member.name,package,assets,documents,
     [&](int64_t id,const std::set<std::string>& fields){return transaction.Project(id,fields);},[&](int64_t id){return transaction.Object(id);});
   phase="projected-assembly";Require(sparse.Build().empty(),"projected Physics allocated a complete member");auto changes=sparse.TakeChanges();auto additions=sparse.TakeAdded();if(changes.empty()&&additions.empty())continue;
   auto changedSize=changes.size(),addedSize=additions.size();transaction.Stage(sparse.OutputLayout(),std::move(changes),std::move(additions),sparse.AddedTypes(),sparse.AddedScripts());
   size_t renderers=0;
   if(native){phase="projected-renderer";RendererMemberBuilder renderer(transaction.Layout(),member.name,package,assets,meshParts,
       [&](int64_t id,const std::set<std::string>& fields){return transaction.Project(id,fields);},[&](int64_t id){return transaction.Object(id);});
     renderer.SetAuthorMetadata(native->bindings);renderer.SetAuthorMarkers(native->markers);renderer.UseSourceMaterials();auto result=renderer.Build();renderers=result.matches;
     transaction.Stage(renderer.OutputLayout(),renderer.TakeChanges(),renderer.TakeAdded());}
   phase="serialized-ranges";auto plan=transaction.Commit();auto prepared=S::BuildRanges(reader,{plan.rewrite});
   auto elapsed=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count();auto stats=reader.stats;
   auto original=reader.ReadAt(member.offset,member.size,reader.limits.readBytes);
   phase="full-writer-oracle";auto oracle=EiemPhysicsInput::MemberBuilder(original,member.name,package,assets,documents).Build();
   if(native){RendererMemberBuilder renderer(oracle,member.name,package,assets,meshParts);renderer.SetAuthorMetadata(native->bindings);renderer.SetAuthorMarkers(native->markers);renderer.UseSourceMaterials();oracle=renderer.Build().bytes;}
   auto view=prepared.Bind(read);Bytes encoded(view->Size());Require(view->ReadAt(0,encoded.data(),encoded.size())==encoded.size(),"Physics view truncated");auto actual=EndfieldInput::Decode(encoded);
   if(native){phase="catalog-consumer-integration";auto automatic=EiemDemandNativeAssembly::PrepareConsumer(reader,mi,layout,*native,assets,meshParts);auto automaticView=S::BuildRanges(reader,{automatic.plan.rewrite}).Bind(read);Bytes combined(automaticView->Size());Require(automaticView->ReadAt(0,combined.data(),combined.size())==combined.size()&&EndfieldInput::Decode(combined).members[mi].bytes==actual.members[mi].bytes,"automatic Physics/Renderer pipeline differs from explicit transaction");Require(automatic.physicsDocuments==documents.size()&&!native->catalog->OpenMeshes()&&!native->catalog->OpenTextures(),"consumer assembly opened geometry/pixels or missed Physics");}
   // Sequential full writers repack the physical payload order twice. A
   // single transaction may retain a different order; every object byte and
   // native dependency/type/script record must still match exactly.
   if(native){const auto& actualBytes=actual.members[mi].bytes;auto al=ParseSerialized(actualBytes),ol=ParseSerialized(oracle);
     Require(al.order==ol.order&&al.externals==ol.externals&&al.types.size()==ol.types.size()&&al.scriptCount==ol.scriptCount,"combined transaction native metadata differs");
     for(size_t ti=0;ti<al.types.size();++ti)Require(al.types[ti].nativeRecord==ol.types[ti].nativeRecord,"combined transaction native type differs");
     Require(std::equal(actualBytes.begin()+al.tableEnd,actualBytes.begin()+al.tableEnd+4+al.scriptCount*12,oracle.begin()+ol.tableEnd),"combined transaction native scripts differ");
     for(auto id:ol.order){const auto& a=al.objects.at(id);const auto& o=ol.objects.at(id);Require(a.type==o.type&&a.size==o.size&&std::equal(actualBytes.begin()+a.offset,actualBytes.begin()+a.offset+a.size,oracle.begin()+o.offset),"combined transaction native object differs: "+std::to_string(id));}
   }else Require(actual.members[mi].bytes==oracle,"projected Physics bones/components/scripts/metadata/preloads differ from complete writer");
   changedCount+=changedSize;addedCount+=addedSize;typesCount+=sparse.AddedTypes().size();
   std::cout<<"PASS demand Physics cab="<<member.name<<" changed="<<transaction.ChangedObjects()<<" added="<<transaction.AddedObjects()<<" renderers="<<renderers<<" types="<<sparse.AddedTypes().size()<<" scripts="<<sparse.AddedScripts().size()<<" literalBytes="<<prepared.literalBytes<<" reusedEncodedBytes="<<prepared.reusedEncoded<<" inputReadBytes="<<stats.encodedBytes<<" prepareMs="<<elapsed<<" nativeRecordsEqual=1 nativeObjectsCreatedByEff=0\n";
 }
 file->VerifyStable();Require(changedCount&&addedCount&&typesCount,"Physics fixture did not exercise native bone/component additions");return 0;
}catch(const std::exception& error){std::cerr<<"FAIL phase="<<phase<<" reason="<<error.what()<<'\n';return 1;}}

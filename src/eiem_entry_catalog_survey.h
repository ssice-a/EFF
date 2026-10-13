#pragma once
// Included inside StartupDependencies, after its native list/manifest API.
// Developer-only: compare mapped reverse dependencies with offline consumers.
// No compiled Mod, resource route or live Unity object is changed.
namespace CatalogSurvey {
std::string Json(const std::string& value);
std::string Logical(std::string value);
struct VfsApi {
 void *instance=nullptr,*query=nullptr,*infoClass=nullptr,*md5=nullptr,*physical=nullptr,*encrypted=nullptr;
 void *blockType=nullptr,*block=nullptr,*chunks=nullptr,*chunkName=nullptr,*chunkDigest=nullptr,*fileChunkName=nullptr;
 size_t size=0,offset=0,length=0,iv=0,offsetWidth=0,lengthWidth=0;
 size_t chunkSize=0;
} vfsApi;
bool vfsBound=false;
void BindVfs(){if(vfsBound)return;
 vfsApi={};
 auto domain=il2cpp_domain_get();size_t count=0;auto loaded=il2cpp_domain_get_assemblies(domain,&count);auto klass=FindClass("Beyond.VFS","VirtualFileSystem",(void**)loaded,count);Require(klass,"survey VFS class absent");
 vfsApi.instance=Exact(klass,"get_instance",{},"Beyond.VFS.VirtualFileSystem",true);Require(vfsApi.instance,"survey VFS singleton getter absent");
 void* iterator=nullptr;while(auto method=il2cpp_class_get_methods(klass,&iterator)){
  if(strcmp(il2cpp_method_get_name(method),"GetFileInfo")||il2cpp_method_get_param_count(method)!=2||TypeName(il2cpp_method_get_param(method,0))!="System.String")continue;
  Require(!vfsApi.query,"survey VFS file lookup ambiguous");vfsApi.query=method;vfsApi.infoClass=il2cpp_class_from_type(il2cpp_method_get_param(method,1));
 }
 Require(vfsApi.query&&vfsApi.infoClass,"survey VFS file lookup absent");vfsApi.size=ValueSize(vfsApi.infoClass);
 auto integerWidth=[&](const char* name){auto type=TypeName(il2cpp_field_get_type(Field(vfsApi.infoClass,name)));Require(type=="System.Int64"||type=="System.UInt64"||type=="System.Int32"||type=="System.UInt32","survey VFS integer field type differs");return type=="System.Int64"||type=="System.UInt64"?size_t(8):size_t(4);};
 vfsApi.offsetWidth=integerWidth("offset");vfsApi.lengthWidth=integerWidth("len");vfsApi.offset=ValueOffset(vfsApi.infoClass,"offset",vfsApi.offsetWidth);vfsApi.length=ValueOffset(vfsApi.infoClass,"len",vfsApi.lengthWidth);
 vfsApi.iv=ValueOffset(vfsApi.infoClass,"ivSeed",4);
 Require(TypeName(il2cpp_field_get_type(Field(vfsApi.infoClass,"ivSeed")))=="System.Int32","survey VFS IV type differs");
 vfsApi.md5=Exact(vfsApi.infoClass,"GetFileDataMD5Str",{},"System.String",false);
 vfsApi.physical=Exact(vfsApi.infoClass,"GetChunkFileRedirectedLoaderPath",{},"System.String",false);
 vfsApi.encrypted=Exact(vfsApi.infoClass,"get_bUseEncrypt",{},"System.Boolean",false);
 vfsApi.fileChunkName=Exact(vfsApi.infoClass,"GetFileChunkMD5NameStr",{"System.Boolean"},"System.String",false);
 auto blockEnum=FindClass("Beyond.VFS","EVFSBlockType",(void**)loaded,count);
 Require(blockEnum&&TypeName(il2cpp_field_get_type(Field(blockEnum,"value__")))=="System.Byte","survey VFS block enum ABI differs");
 vfsApi.blockType=Exact(vfsApi.infoClass,"get_blockType",{},"Beyond.VFS.EVFSBlockType",false);
 vfsApi.block=Exact(klass,"GetVFBlockByType",{"Beyond.VFS.EVFSBlockType"},"Beyond.VFS.VFBlockMainInfo",false);
 auto blockClass=FindClass("Beyond.VFS","VFBlockMainInfo",(void**)loaded,count);
 auto chunkClass=FindClass("Beyond.VFS","FVFBlockChunkInfo",(void**)loaded,count);
 Require(blockClass&&chunkClass&&vfsApi.block,"survey VFS mounted block API absent");
 vfsApi.chunks=Field(blockClass,"allChunks");
 Require(TypeName(il2cpp_field_get_type(vfsApi.chunks))=="Beyond.VFS.FVFBlockChunkInfo[]","survey VFS mounted CHK array type differs");
 vfsApi.chunkSize=ValueSize(chunkClass);
 vfsApi.chunkName=Exact(chunkClass,"GetChkMD5Name",{},"System.String",false);
 vfsApi.chunkDigest=Exact(chunkClass,"GetContentMD5Str",{},"System.String",false);
 for(auto method:{vfsApi.md5,vfsApi.physical,vfsApi.encrypted,vfsApi.fileChunkName,vfsApi.blockType,vfsApi.chunkName,vfsApi.chunkDigest})
  Require(method&&((MInfo*)method)->mp,"survey VFS content/path accessors absent");
 vfsBound=true;
 Log("[CATALOG-SURVEY-VFS-BOUND] resultType=%s bytes=%zu chunkBytes=%zu offsetWidth=%zu lengthWidth=%zu metadataOffsets=1",TypeName(il2cpp_method_get_param(vfsApi.query,1)).c_str(),vfsApi.size,vfsApi.chunkSize,vfsApi.offsetWidth,vfsApi.lengthWidth);
}
bool Digest(const std::string& value){return value.size()==32&&std::all_of(value.begin(),value.end(),[](unsigned char c){return std::isxdigit(c)!=0;});}
std::string ChunkKey(std::string value){value=Logical(std::move(value));if(EiemStaticSources::Ends(value,".chk"))value.resize(value.size()-4);Require(Digest(value),"survey CHK name is not an MD5 identity");return value;}
using StringGetter=void*(*)(void*,void*);
using BoolGetter=unsigned char(*)(void*,void*);
struct QueryContext {std::map<void*,std::map<std::string,std::string>> chunks;};
const std::map<std::string,std::string>& ChunkDigests(void* vfs,Bytes& info,QueryContext& context){
 auto type=((BoolGetter)((MInfo*)vfsApi.blockType)->mp)(info.data(),vfsApi.blockType);void* args[]={&type};
 auto block=Ref::Make(Call(vfsApi.block,vfs,args),false);Require(block.Get(),"survey mounted VFS block absent");
 auto found=context.chunks.find(block.Get());if(found!=context.chunks.end())return found->second;
 void* array=nullptr;size_t count=0;il2cpp_field_get_value(block.Get(),vfsApi.chunks,&array);
 Require(ArrayLength(array,&count)&&count<=SIZE_MAX/vfsApi.chunkSize,"survey mounted CHK array extent differs");
 SkinningObservation::ArrayElementSize(array,int(vfsApi.chunkSize));
 Bytes rows(count*vfsApi.chunkSize);Require(rows.empty()||CopyRaw((char*)array+32,rows.data(),rows.size()),"survey mounted CHK array unreadable");
 std::map<std::string,std::string> digests;
 for(size_t i=0;i<count;++i){auto row=rows.data()+i*vfsApi.chunkSize;
  auto name=ChunkKey(String(((StringGetter)((MInfo*)vfsApi.chunkName)->mp)(row,vfsApi.chunkName)));
  auto digest=String(((StringGetter)((MInfo*)vfsApi.chunkDigest)->mp)(row,vfsApi.chunkDigest));
  Require(Digest(digest)&&!name.empty(),"survey CHK content identity invalid");
  auto inserted=digests.emplace(name,digest);Require(inserted.second||inserted.first->second==digest,"survey CHK identity ambiguous");
 }
 return context.chunks.emplace(block.Get(),std::move(digests)).first->second;
}
bool FileInfo(const std::string& name,void* vfs,Bytes& info){
 auto lookup="Data/Bundles/Windows/"+name;auto label=Ref::Make(il2cpp_string_new(lookup.c_str()),false);void* args[]={label.Get(),info.data()};
 return RuntimeShapes::Scalar<unsigned char>(vfsApi.query,vfs,args)!=0;
}
std::string SourceJson(const std::string& name,QueryContext& context){BindVfs();auto vfs=RuntimeShapes::Call(vfsApi.instance);Require(vfs,"survey mounted VFS singleton absent");
 // BLC's actual lookup name is Data/Bundles/Windows/main/...; the offline
 // canonical key drops Data/ and folds case. Use the verified native spelling.
 auto logical="bundles/windows/"+name;Bytes info(vfsApi.size);
 Require(FileInfo(name,vfs,info),"survey VFS source lookup failed: "+logical);uint64_t offset=0,length=0;memcpy(&offset,info.data()+vfsApi.offset,vfsApi.offsetWidth);memcpy(&length,info.data()+vfsApi.length,vfsApi.lengthWidth);
 auto digest=String(((StringGetter)((MInfo*)vfsApi.md5)->mp)(info.data(),vfsApi.md5));auto physical=String(((StringGetter)((MInfo*)vfsApi.physical)->mp)(info.data(),vfsApi.physical));
 bool encrypted=((BoolGetter)((MInfo*)vfsApi.encrypted)->mp)(info.data(),vfsApi.encrypted)!=0;
 // This build's per-file accessor has no readable digest (String's fallback
 // is diagnostic text, never an identity). The mounted CHK's contentMD5
 // below is separate from its name and seals the bytes of this exact range.
 bool fileDigestAvailable=Digest(digest);
 Require(length&&offset<=INT64_MAX&&!physical.empty(),"survey VFS content/range invalid: "+logical+" physical="+physical+" offset="+std::to_string(offset)+" bytes="+std::to_string(length));
 using ChunkNameGetter=void*(*)(void*,unsigned char,void*);
 auto chunk=String(((ChunkNameGetter)((MInfo*)vfsApi.fileChunkName)->mp)(info.data(),1,vfsApi.fileChunkName));
 const auto& digests=ChunkDigests(vfs,info,context);auto found=digests.find(ChunkKey(chunk));Require(found!=digests.end(),"survey file's CHK absent from mounted block: "+chunk);
 uint32_t iv=0;memcpy(&iv,info.data()+vfsApi.iv,4);
 EiemMountedSourceIdentity::Entry mounted{physical,ChunkKey(chunk),found->second,fileDigestAvailable?digest:std::string{},offset,length,encrypted,iv};
 return "{\"bundle\":"+Json(name)+",\"logical\":"+Json(logical)+",\"fileContentMd5\":"+Json(mounted.fileContentMd5)+",\"nativeFileDigestValue\":"+Json(digest)+",\"chunk\":"+Json(chunk)+",\"chunkContentMd5\":"+Json(found->second)+",\"physical\":"+Json(physical)+",\"offset\":"+std::to_string(offset)+",\"bytes\":"+std::to_string(length)+",\"ivSeed\":"+std::to_string(iv)+",\"encrypted\":"+(encrypted?"true":"false")+",\"catalogContentKey\":"+Json(EiemMountedSourceIdentity::ContentKey(mounted))+"}";
}
std::string Json(const std::string& value){std::string text="\"";for(unsigned char c:value){if(c=='\"'||c=='\\'){text+='\\';text+=char(c);}else if(c<32){const char* hex="0123456789abcdef";text+="\\u00";text+=hex[c>>4];text+=hex[c&15];}else text+=char(c);}return text+'\"';}
std::string Logical(std::string value){return EiemStaticSources::Logical(std::move(value));}
void Capture(void* manifest,void* prototype,const ManifestState& state){
 auto started=std::chrono::steady_clock::now();Require(bool(preparedCompiledSet),"catalog survey requires an existing compiled Mod set");
 Require(state.data&&state.address&&state.length>0&&uint64_t(state.length)<=UINTPTR_MAX-state.address,"survey manifest range absent");
 auto range=[&](uintptr_t p,size_t bytes){return p>=state.address&&p-state.address<=uint64_t(state.length)&&bytes<=uint64_t(state.length)-(p-state.address);};
 Bytes array(api.arraySize);il2cpp_field_get_value(state.data,api.bundles,array.data());int32_t count=0;uintptr_t head=0,data=0;
 memcpy(&count,array.data()+api.arrayCount,4);memcpy(&head,array.data()+api.arrayHead,sizeof(head));il2cpp_field_get_value(state.data,api.mappedData,&data);
 Require(count>=0&&size_t(count)<=SIZE_MAX/api.bundleSize&&range(head,size_t(count)*api.bundleSize),"survey Bundle array range differs");
 Bytes rows(size_t(count)*api.bundleSize);Require(CopyRaw((void*)head,rows.data(),rows.size()),"survey Bundle array unreadable");
 std::map<uint64_t,size_t> hashes;
 for(size_t i=0;i<size_t(count);++i){uint64_t key=0;memcpy(&key,rows.data()+i*api.bundleSize+api.bundleKeyOffset,8);
  Require(hashes.emplace(key,i).second,"survey Bundle key duplicated");}
 auto name=[&](size_t i){int32_t offset=0,length=0;memcpy(&offset,rows.data()+i*api.bundleSize+api.bundleNameOffset,4);
  Require(offset>=0&&uintptr_t(offset)<=UINTPTR_MAX-data,"survey name offset differs");auto pointer=data+uintptr_t(offset);
  Require(range(pointer,4)&&CopyRaw((void*)pointer,&length,4)&&length>=0&&length%2==0&&range(pointer+4,size_t(length)),"survey name escapes mapped data");
  std::wstring wide(size_t(length)/2,L'\0');Require(!length||CopyRaw((void*)(pointer+4),wide.data(),size_t(length)),"survey name unreadable");
  auto n=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,wide.data(),int(wide.size()),nullptr,0,nullptr,nullptr);Require(n>0,"survey Bundle name is empty/invalid");
  std::string text(size_t(n),'\0');Require(WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,wide.data(),int(wide.size()),text.data(),n,nullptr,nullptr)==n,"survey Bundle name conversion failed");return text;};
 struct Mod {std::string id;std::set<std::string> seeds,expected;};std::vector<Mod> mods;std::set<std::string> wanted;
 for(const auto& artifact:preparedCompiledSet->mods){const auto& package=artifact->manifest.package;auto selection=EiemStaticSources::LoadMod(package.root,gameDirectory);Mod mod;mod.id=package.id;
  // Match the exported source record to its actual native Bundle label. The
  // VFS key includes bundles/windows/, while the manifest label is main/...;
  // never infer a consumer identity by stripping arbitrary path prefixes.
  auto label=[&](const std::string& source){const auto& entry=selection.inputs.at(source);auto identity=source+"\n"+entry.revision;
   auto file=EiemResourceReloadPlan::Hash(Bytes(identity.begin(),identity.end()))+".vfs";const auto& plan=artifact->manifest.plan;
   for(size_t i=0;i<plan.originals.size();++i)if(plan.originals[i].file==file)return Logical(plan.inputs.at(i).bundle);
   // Negative candidates have no compiled view; require the same explicit
   // platform prefix used by this exported selection, checked by live seeds.
   Require(source.rfind("bundles/windows/",0)==0,"survey uncompiled source namespace unsupported");return Logical(source.substr(strlen("bundles/windows/")));
  };
  for(const auto& target:package.targetIndex)mod.seeds.insert(label(selection.assets.at(EiemStaticSources::Logical(target.first.path))));
  for(const auto& candidate:selection.candidates)mod.expected.insert(label(candidate));wanted.insert(mod.seeds.begin(),mod.seeds.end());mods.push_back(std::move(mod));}
 std::map<std::string,size_t> selected;for(size_t i=0;i<size_t(count);++i){auto text=Logical(name(i));if(wanted.count(text))Require(selected.emplace(text,i).second,"survey seed identity ambiguous");}
 Require(selected.size()==wanted.size(),"survey Mesh-owner Bundle missing from current manifest");
 auto scratch=NewList(prototype);auto clear=Inherited(api.listClass,"Clear",{},"System.Void");Require(clear,"survey list Clear API absent");
 auto directInfo=Exact(il2cpp_object_get_class(manifest),"TryGetBundleDirectDeps",{TypeName(il2cpp_class_get_type(api.scalarClass)).c_str(),TypeName(il2cpp_class_get_type(api.listClass)).c_str()},"System.Boolean",false);Require(directInfo,"survey direct dependency MethodInfo absent");
 auto reverseInfo=Exact(il2cpp_object_get_class(manifest),"TryGetBundleReverseDeps",{TypeName(il2cpp_class_get_type(api.scalarClass)).c_str(),TypeName(il2cpp_class_get_type(api.listClass)).c_str()},"System.Boolean",false);
 Require(reverseInfo&&((MInfo*)reverseInfo)->mp,"survey native reverse dependency query absent");auto reverseQuery=(Query)((MInfo*)reverseInfo)->mp;
 auto reverse=[&](size_t i){Call(clear,scratch.Get());uint64_t key=0;memcpy(&key,rows.data()+i*api.bundleSize+api.bundleKeyOffset,8);reverseQuery(manifest,key,scratch.Get(),reverseInfo);
  auto keys=ReadList(scratch.Get());std::vector<size_t> result;for(auto value:keys)result.push_back(hashes.at(value));return result;};
 std::set<size_t> verified;std::ostringstream report;report<<"{\"processId\":"<<GetCurrentProcessId()<<",\"manifestBundles\":"<<count<<",\"nativeReverseDependencies\":true,\"productionAdaptation\":false,\"mods\":[";bool firstMod=true;
 for(const auto& mod:mods){std::set<size_t> candidates;std::vector<size_t> queue;for(const auto& seed:mod.seeds)queue.push_back(selected.at(seed));
  while(!queue.empty()){auto i=queue.back();queue.pop_back();if(!candidates.insert(i).second)continue;auto next=reverse(i);queue.insert(queue.end(),next.begin(),next.end());}
  std::set<std::string> names;for(auto i:candidates){names.insert(Logical(name(i)));if(!verified.insert(i).second)continue;
   Call(clear,scratch.Get());uint64_t key=0;memcpy(&key,rows.data()+i*api.bundleSize+api.bundleKeyOffset,8);originalDirect(manifest,key,scratch.Get(),directInfo);
   for(auto value:ReadList(scratch.Get()))Require(hashes.count(value),"survey native direct dependency absent from current manifest");
  }
  std::vector<std::string> missing;std::set_difference(mod.expected.begin(),mod.expected.end(),names.begin(),names.end(),std::back_inserter(missing));
  if(!firstMod)report<<',';firstMod=false;report<<"{\"mod\":"<<Json(mod.id)<<",\"meshOwners\":"<<mod.seeds.size()<<",\"offlineCandidates\":"<<mod.expected.size()<<",\"runtimeCandidates\":"<<names.size()<<",\"missingOfflineCandidates\":[";
  bool first=true;for(const auto& value:missing){if(!first)report<<',';first=false;report<<Json(value);}report<<"],\"candidates\":[";first=true;for(const auto& value:names){if(!first)report<<',';first=false;report<<Json(value);}report<<"]}";
  Log("[CATALOG-SURVEY-MOD] mod=%s meshOwners=%zu offlineCandidates=%zu runtimeCandidates=%zu missing=%zu",mod.id.c_str(),mod.seeds.size(),mod.expected.size(),names.size(),missing.size());
 }
 BindVfs();QueryContext context;size_t queryFailures=0;
 report<<"],\"mountedSources\":[";bool firstSource=true;for(auto i:verified){if(!firstSource)report<<',';firstSource=false;
  auto label=name(i);try{report<<SourceJson(label,context);}catch(const std::exception& error){++queryFailures;report<<"{\"bundle\":"<<Json(label)<<",\"queryError\":"<<Json(error.what())<<"}";
   if(queryFailures==1)Log("[CATALOG-SURVEY-SOURCE-REJECT] bundle=%s reason=%s",label.c_str(),error.what());}
 }
 auto elapsed=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count();report<<"],\"mountedQueryFailures\":"<<queryFailures<<",\"nativeDirectQueriesVerified\":"<<verified.size()<<",\"surveyMs\":"<<elapsed<<"}";
 auto text=report.str();auto output=gameDirectory/"plugin/catalog-survey.json";EiemOfflineCompile::AtomicSave(output,Bytes(text.begin(),text.end()));
 Log("[CATALOG-SURVEY-READY] bundles=%d selectedDirectQueries=%zu mountedQueryFailures=%zu totalMs=%.3f report=%s pfbReads=0 modFilesModified=0",count,verified.size(),queryFailures,elapsed,output.u8string().c_str());
}
void Observe(void* manifest,void* prototype){
 static std::mutex guard;static std::set<std::tuple<void*,uintptr_t,int64_t>> attempted;
 try{auto state=State(manifest);{std::lock_guard<std::mutex> lock(guard);if(!attempted.emplace(state.data,state.address,state.length).second)return;}
  Capture(manifest,prototype,state);
 }catch(const std::exception& error){Log("[CATALOG-SURVEY-REJECT] reason=%s productionRoutesPreserved=1",error.what());}
}
} // namespace CatalogSurvey

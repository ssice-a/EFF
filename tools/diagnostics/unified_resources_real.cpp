#include "../../src/eiem_compiled_mod_writer.h"
#include "../../src/eiem_published_compiled_mod.h"
#include <psapi.h>
#include <condition_variable>
#include <iostream>
#include <thread>
#include <iomanip>

namespace UnifiedReal {
using namespace EiemCompiledModWriter;
struct Mutation {std::string category,resource,description;size_t offset=0;Bytes bytes;};
bool Covered(const std::vector<EiemAddressedResources::Range>& ranges,size_t offset,size_t size){
 for(const auto& range:ranges)if(offset>=range.first&&offset-range.first<=range.second&&size<=range.second-(offset-range.first))return true;return false;
}
bool FloatMutation(const Type& type,const Value& value,const std::string& resource,const std::vector<EiemAddressedResources::Range>& ranges,Mutation& result){
 if(type.nodes.at(value.node).type=="float"&&value.raw.size()==4)for(const auto& span:value.rawOrigins)if(span.resource==resource&&span.size==4&&Covered(ranges,span.offset,4)){
  float number;memcpy(&number,value.raw.data(),4);if(!std::isfinite(number))continue;number+=0.125f;if(!std::isfinite(number))continue;result.resource=resource;result.offset=span.offset;result.bytes.resize(4);memcpy(result.bytes.data(),&number,4);result.description=type.nodes.at(value.node).name;return true;
 }
 for(const auto& child:value.children)if(FloatMutation(type,child,resource,ranges,result))return true;return false;
}
const Type* Schema(Artifact& artifact,const Resource& resource,int kind){
 for(const auto& type:artifact.sharedSchemas)if(type.classId==kind&&type.hash==resource.type)return &type;
 auto search=[&]()->const Type*{for(const auto& member:artifact.members)for(const auto* layout:{&member.second.before,&member.second.after})for(const auto& type:layout->types)if(type.classId==kind&&type.hash==resource.type)return &type;return nullptr;};
 if(auto found=search())return found;for(size_t i=0;i<artifact.views.size();++i){HydrateInput(artifact,i);if(auto found=search())return found;}return nullptr;
}
std::vector<Mutation> Mutations(Artifact& artifact){
 auto bindings=EiemAddressedResources::Decode(artifact.resourceBindingBytes,artifact.manifest.package);std::map<std::string,Mutation> found;
 for(const auto& row:artifact.manifest.package.resources){const auto& resource=*row.second;auto covered=bindings.ranges.find(row.first);const std::vector<EiemAddressedResources::Range> empty;const auto& ranges=covered==bindings.ranges.end()?empty:covered->second;
  if(resource.kind==1&&!found.count("mesh")&&resource.blocks.count("m_VertexData")&&bindings.meshes.count(row.first)){
   const auto& type=bindings.meshes.at(row.first);auto base=EmptyValue(type,0);auto node=Child(type,base,"m_VertexData").node;const auto& block=resource.blocks.at("m_VertexData");Bytes bytes(resource.bytes.begin()+block.offset,resource.bytes.begin()+block.offset+block.size);auto value=DecodeAuthorValue(type,node,bytes,row.first,resource.bytes,block.offset);auto storage=DescribeMeshVertexStorage(type,value);
   const auto& channels=ArrayValue(type,Child(type,value,"m_Channels")).children;const auto& position=channels.at(0);auto stream=size_t(Integer(Child(type,position,"stream"))),offset=size_t(Integer(Child(type,position,"offset")));const auto& data=Child(type,value,"m_DataSize");
   if(Integer(Child(type,position,"format"))==0&&Integer(Child(type,position,"dimension"))==3&&!data.rawOrigins.empty()&&stream<4){auto at=storage.start[stream]+offset;auto source=data.rawOrigins.front().offset+at;
    if(at+4<=data.raw.size()&&Covered(ranges,source,4)){float number;memcpy(&number,data.raw.data()+at,4);number+=0.001f;if(std::isfinite(number)){Mutation mutation{"mesh",row.first,"first vertex position.x",source,Bytes(4)};memcpy(mutation.bytes.data(),&number,4);found.emplace("mesh",std::move(mutation));}}
   }
  }else if(resource.kind==2&&!found.count("material")){
   auto schema=Schema(artifact,resource,21);if(!schema)continue;auto base=EmptyValue(*schema,0);Mutation mutation;mutation.category="material";
   for(const auto& block:resource.blocks){if(block.first.empty()||block.first[0]=='@')continue;auto node=Child(*schema,base,block.first).node;Bytes bytes(resource.bytes.begin()+block.second.offset,resource.bytes.begin()+block.second.offset+block.second.size);auto value=DecodeAuthorValue(*schema,node,bytes,row.first,resource.bytes,block.second.offset);if(FloatMutation(*schema,value,row.first,ranges,mutation)){mutation.description=block.first+"/"+mutation.description;found.emplace("material",std::move(mutation));break;}}
  }else if(resource.kind==4&&!found.count("texture")&&resource.blocks.count("pixels")){
   const auto& block=resource.blocks.at("pixels");if(block.size>=4&&Covered(ranges,block.offset,4)){Bytes pixel(resource.bytes.begin()+block.offset,resource.bytes.begin()+block.offset+4);pixel[0]^=0x11;found.emplace("texture",Mutation{"texture",row.first,"first RGBA pixel",block.offset,std::move(pixel)});}
  }else if(resource.blocks.count("@physics")&&!found.count("physics")){
   auto document=EiemPhysicsInput::Decode(resource,row.first);Mutation mutation;mutation.category="physics";
   for(const auto& donor:document.templates){for(const auto& object:donor.second.layout.objects){const auto& type=donor.second.layout.types.at(object.second.type);if(type.classId!=114)continue;auto value=ObjectValue(donor.second.bytes,donor.second.layout,object.first,&donor.second.origins);if(FloatMutation(type,value,row.first,ranges,mutation)){mutation.description=donor.first+":"+std::to_string(object.first)+"/"+mutation.description;found.emplace("physics",std::move(mutation));break;}}if(found.count("physics"))break;}
  }else if(resource.kind==10&&!found.count("effects")&&resource.blocks.count("@effect-attachment")){
   const auto& block=resource.blocks.at("@effect-attachment");ControlReader reader{resource,block.offset,block.offset+block.size};reader.Strings();reader.N(1);float number;memcpy(&number,resource.bytes.data()+reader.p,4);number+=0.01f;Mutation mutation{"effects",row.first,"attachment position.x",reader.p,Bytes(4)};memcpy(mutation.bytes.data(),&number,4);found.emplace("effects",std::move(mutation));
  }
 }
 std::vector<Mutation> result;for(const char* category:{"mesh","material","texture","physics","effects"}){auto row=found.find(category);if(row==found.end())std::cout<<"SKIP category="<<category<<" reason=no covered editable resource\n";else{std::cout<<"MUTATION category="<<category<<" resource="<<row->second.resource<<" offset="<<row->second.offset<<" field="<<row->second.description<<'\n';result.push_back(std::move(row->second));}}return result;
}
void Seal(Resource& resource){
 size_t cursor=80;auto count=size_t(Le(resource.bytes,16,4));for(size_t i=0;i<count;++i){auto length=size_t(Le(resource.bytes,cursor,2));cursor+=2;auto name=Text(resource.bytes.data()+cursor,length);cursor+=length;auto offset=size_t(Le(resource.bytes,cursor,8)),size=size_t(Le(resource.bytes,cursor+8,8));auto digest=Sha(resource.bytes.data()+offset,size);std::copy(digest.begin(),digest.end(),resource.bytes.begin()+cursor+16);resource.blocks.at(name).digest=digest;cursor+=48;}
 auto body=Sha(resource.bytes.data()+80,resource.bytes.size()-80);std::copy(body.begin(),body.end(),resource.bytes.begin()+48);resource.contentHash=Hash(resource.bytes);EiemDirectory::Decode(resource.bytes,resource.kind);
}
void Apply(const std::filesystem::path& mod,const std::vector<Mutation>& changes){
 auto package=Load(mod);std::map<std::string,Resource> resources;for(const auto& mutation:changes){auto inserted=resources.emplace(mutation.resource,*package.resources.at(mutation.resource));auto& resource=inserted.first->second;Require(mutation.offset<=resource.bytes.size()&&mutation.bytes.size()<=resource.bytes.size()-mutation.offset,"test mutation escapes resource");std::copy(mutation.bytes.begin(),mutation.bytes.end(),resource.bytes.begin()+mutation.offset);}
 for(auto& row:resources){Seal(row.second);EiemOfflineCompile::AtomicSave(mod/std::filesystem::u8path(row.first),row.second.bytes);}
 Load(mod); // Includes validation of typed Effect controls/native bundle hashes.
}
void CopyMod(const std::filesystem::path& from,const std::filesystem::path& to){
 Require(!std::filesystem::exists(to),"test output Mod already exists");std::filesystem::create_directories(to);
 for(const auto& entry:std::filesystem::recursive_directory_iterator(from)){auto relative=entry.path().lexically_relative(from),target=to/relative;if(entry.is_directory()){std::filesystem::create_directories(target);continue;}Require(entry.is_regular_file(),"test Mod contains unsupported filesystem entry");std::filesystem::create_directories(target.parent_path());
  auto name=relative.generic_u8string();if(name.rfind("compiled.blocks/",0)==0&&CreateHardLinkW(target.c_str(),entry.path().c_str(),nullptr))continue;std::filesystem::copy_file(entry.path(),target);
 }
}
uint64_t PrivateBytes(){PROCESS_MEMORY_COUNTERS_EX usage{};usage.cb=sizeof(usage);Require(GetProcessMemoryInfo(GetCurrentProcess(),reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&usage),sizeof(usage))!=0,"process private memory query failed");return uint64_t(usage.PrivateUsage);}
struct PeakMemory {
 std::atomic<uint64_t> peak{0};std::mutex mutex;std::condition_variable changed;bool stop=false;std::thread worker;
 PeakMemory():worker([this]{std::unique_lock<std::mutex> lock(mutex);while(!stop){auto value=PrivateBytes(),previous=peak.load();while(value>previous&&!peak.compare_exchange_weak(previous,value)){}changed.wait_for(lock,std::chrono::milliseconds(10),[&]{return stop;});}}){}
 uint64_t Finish(){{std::lock_guard<std::mutex> lock(mutex);stop=true;}changed.notify_one();if(worker.joinable())worker.join();return std::max(peak.load(),PrivateBytes());}
 ~PeakMemory(){Finish();}
};
struct ExportStats {double wall=0;uint64_t startPrivate=0,peakPrivate=0;std::map<std::string,double> costs;};
ExportStats ExportTimed(const std::filesystem::path& mod,const std::filesystem::path& game,const std::filesystem::path& previous){
 ExportStats stats;std::mutex costsMutex;stats.startPrivate=PrivateBytes();PeakMemory memory;auto started=std::chrono::steady_clock::now();
 Export(mod,game,[&](const char* stage,double value){std::lock_guard<std::mutex> lock(costsMutex);stats.costs[stage]+=value;},previous);
 stats.wall=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count();stats.peakPrivate=memory.Finish();return stats;
}
EiemCompiledInputView::Buffers Buffers(const Artifact& artifact,const std::shared_ptr<const Bytes>& original){
 EiemCompiledInputView::Buffers buffers;buffers.emplace(std::make_pair(EiemResourceRecipe::Original,std::string{}),original);
 for(const auto& resource:artifact.manifest.package.resources)buffers.emplace(std::make_pair(EiemResourceRecipe::Author,resource.first),std::shared_ptr<const Bytes>(resource.second,&resource.second->bytes));
 for(const auto& frame:artifact.preparedFrames)buffers.emplace(std::make_pair(EiemResourceRecipe::ExpandedFrame,frame.first),std::shared_ptr<const Bytes>(&frame.second,[](const Bytes*){}));return buffers;
}
void EqualBytes(const Bytes& a,const Bytes& b,const std::string& context){
 Require(a.size()==b.size(),context+" extent differs: "+std::to_string(a.size())+"/"+std::to_string(b.size()));auto mismatch=std::mismatch(a.begin(),a.end(),b.begin());Require(mismatch.first==a.end(),context+" bytes differ at "+std::to_string(size_t(mismatch.first-a.begin())));
}
void DescribeDifference(const char* label,const EiemCompiledInputView::Recipe& recipe,size_t offset){
 size_t at=0;for(size_t i=0;i<recipe.spans.size();++i){const auto& span=recipe.spans[i];if(offset>=at&&offset-at<span.size){std::cout<<"VIEW-DIFF "<<label<<" span="<<i<<" kind="<<unsigned(span.kind)<<" source="<<span.source<<" spanOutput="<<at<<" sourceOffset="<<span.offset+offset-at<<" spanSize="<<span.size<<std::endl;
   for(const auto& site:recipe.names)if(site.span==i&&site.offset<=offset-at+256&&offset-at<=site.offset+site.before.size()+256)std::cout<<"NAME-SITE "<<label<<" span="<<site.span<<" local="<<site.offset<<" before="<<site.before<<" directory="<<site.directory<<std::endl;return;}at+=span.size;}
}
void NormalizeGeneratedPartNames(Bytes& bytes){
 for(const char* marker:{"__EFF_PART_","__EFF_SHAPE_"}){size_t at=0;while(at<bytes.size()){
  auto found=std::search(bytes.begin()+at,bytes.end(),marker,marker+strlen(marker));if(found==bytes.end())break;
  auto suffix=size_t(found-bytes.begin())+strlen(marker);if(bytes.size()-suffix<64){at=size_t(found-bytes.begin())+1;continue;}
  bool token=true;for(size_t i=0;i<64;++i)if(!((bytes[suffix+i]>='0'&&bytes[suffix+i]<='9')||(bytes[suffix+i]>='a'&&bytes[suffix+i]<='f')||(bytes[suffix+i]>='A'&&bytes[suffix+i]<='F'))){token=false;break;}
  if(token)std::fill(bytes.begin()+suffix,bytes.begin()+suffix+64,uint8_t('0'));at=suffix+64;
 }}
}
void ValueDifferences(const Type& type,const Value& a,const Value& b,const std::string& path,size_t& reported){
 if(reported>=6)return;if(a.raw!=b.raw){++reported;auto mismatch=std::mismatch(a.raw.begin(),a.raw.end(),b.raw.begin(),b.raw.end());auto bad=size_t(mismatch.first-a.raw.begin()),begin=bad>16?bad-16:0,count=std::min<size_t>(48,std::min(a.raw.size(),b.raw.size())-begin);std::cout<<"FIELD-DIFF path="<<path<<" type="<<type.nodes.at(a.node).type<<" rawOffset="<<bad<<" hexOffset="<<begin<<" fast="<<Hex(a.raw.data()+begin,count)<<" full="<<Hex(b.raw.data()+begin,count)<<" sizes="<<a.raw.size()<<'/'<<b.raw.size()<<std::endl;}
 if(a.count!=b.count){++reported;std::cout<<"FIELD-DIFF path="<<path<<" count="<<a.count<<'/'<<b.count<<std::endl;}
 for(size_t i=0;i<std::min(a.children.size(),b.children.size());++i)if(a.children[i].node==b.children[i].node)ValueDifferences(type,a.children[i],b.children[i],path+"/"+type.nodes.at(a.children[i].node).name+(type.nodes.at(a.node).type=="Array"?"["+std::to_string(i)+"]":""),reported);
}
void NativeDifferences(const EiemInputSpanView::View& fast,const EiemInputSpanView::View& full,size_t position){
 try{auto begin=position>16?position-16:0,count=std::min<size_t>(48,fast.Size()-begin);Bytes a(count),b(count);fast.ReadAt(begin,a.data(),count);full.ReadAt(begin,b.data(),count);std::cout<<"VIEW-HEX offset="<<begin<<" fast="<<Hex(a.data(),a.size())<<" full="<<Hex(b.data(),b.size())<<std::endl;
  a.resize(fast.Size());b.resize(full.Size());Require(fast.ReadAt(0,a.data(),a.size())==a.size()&&full.ReadAt(0,b.data(),b.size())==b.size(),"view diagnostic read truncated");auto left=EndfieldInput::Decode(a),right=EndfieldInput::Decode(b);size_t reported=0;
  for(const auto& member:left.members){auto peer=std::find_if(right.members.begin(),right.members.end(),[&](const auto& other){return other.name==member.name;});if(peer==right.members.end())continue;if(member.bytes==peer->bytes)continue;auto mismatch=std::mismatch(member.bytes.begin(),member.bytes.end(),peer->bytes.begin(),peer->bytes.end());std::cout<<"DECODED-DIFF member="<<member.name<<" offset="<<size_t(mismatch.first-member.bytes.begin())<<" sizes="<<member.bytes.size()<<'/'<<peer->bytes.size()<<std::endl;
   if(!Serialized(member.bytes)||!Serialized(peer->bytes))continue;auto al=ParseSerialized(member.bytes),bl=ParseSerialized(peer->bytes);for(const auto& object:al.objects){auto other=bl.objects.find(object.first);if(other==bl.objects.end())continue;Bytes x(member.bytes.begin()+object.second.offset,member.bytes.begin()+object.second.offset+object.second.size),y(peer->bytes.begin()+other->second.offset,peer->bytes.begin()+other->second.offset+other->second.size);if(x==y)continue;const auto& type=al.types.at(object.second.type);std::cout<<"OBJECT-DIFF member="<<member.name<<" pathId="<<object.first<<" classId="<<type.classId<<" offsets="<<object.second.offset<<'/'<<other->second.offset<<" sizes="<<x.size()<<'/'<<y.size()<<std::endl;if(type.hash==bl.types.at(other->second.type).hash)ValueDifferences(type,DecodeValue(type,0,x),DecodeValue(bl.types.at(other->second.type),0,y),"object("+std::to_string(object.first)+")",reported);if(reported>=6)break;}
  }
 }catch(const std::exception& error){std::cout<<"VIEW-DIAGNOSTIC "<<error.what()<<std::endl;}
}
uint64_t Differential(Artifact& fast,Artifact& full){
 const auto& a=fast.manifest.plan;const auto& b=full.manifest.plan;Require(a.originals.size()==b.originals.size()&&fast.views.size()==full.views.size(),"incremental/full source or view count differs");std::map<std::string,size_t> indices;for(size_t i=0;i<b.originals.size();++i)indices.emplace(b.originals[i].file,i);uint64_t compared=0;
 for(size_t i=0;i<a.originals.size();++i){auto found=indices.find(a.originals[i].file);Require(found!=indices.end(),"full rebuild lacks incremental original source");auto j=found->second;Require(a.originals[i].hash==b.originals[j].hash,"incremental/full original content differs");auto original=std::make_shared<const Bytes>(ReadSource({},a.originals[i]));
  auto fastBuffers=Buffers(fast,original),fullBuffers=Buffers(full,original);
  // Resource-only updates retain existing markers, while cold builds may
  // migrate a legacy marker to its stable form. Validate both namespaces, then
  // compare every payload byte except the generated 64-digit marker token.
  EiemCompiledInputView::Bind(fast.views.at(i),fastBuffers,a.names);EiemCompiledInputView::Bind(full.views.at(j),fullBuffers,b.names);
  auto fastView=EiemCompiledInputView::Bind(fast.views.at(i),fastBuffers,{});auto fullView=EiemCompiledInputView::Bind(full.views.at(j),fullBuffers,{});Require(fastView->Size()==fullView->Size(),"native bound view extent differs: "+a.originals[i].file);
  Bytes left(std::min(fastView->Size(),size_t(1024*1024))),right(left.size());for(size_t offset=0;offset<fastView->Size();){auto count=std::min(left.size(),fastView->Size()-offset),begin=offset>128?offset-128:0,end=std::min(fastView->Size(),offset+count+128),prefix=offset-begin,windowSize=end-begin;Bytes windowLeft(windowSize),windowRight(windowSize);
   Require(fastView->ReadAt(begin,windowLeft.data(),windowSize)==windowSize&&fullView->ReadAt(begin,windowRight.data(),windowSize)==windowSize,"native bound view read truncated");NormalizeGeneratedPartNames(windowLeft);NormalizeGeneratedPartNames(windowRight);std::copy_n(windowLeft.begin()+prefix,count,left.begin());std::copy_n(windowRight.begin()+prefix,count,right.begin());auto mismatch=std::mismatch(left.begin(),left.begin()+count,right.begin());
   if(mismatch.first!=left.begin()+count){auto position=offset+size_t(mismatch.first-left.begin());DescribeDifference("fast",fast.views.at(i),position);DescribeDifference("full",full.views.at(j),position);std::cout<<"VIEW-DIFF byte fast="<<unsigned(*mismatch.first)<<" full="<<unsigned(*(right.begin()+(mismatch.first-left.begin())))<<std::endl;NativeDifferences(*fastView,*fullView,position);
    HydrateInput(fast,i);HydrateInput(full,j);auto decoded=EndfieldInput::Decode(*original);std::map<std::string,const Bytes*> originals;for(const auto& member:decoded.members)originals.emplace(member.name,&member.bytes);for(const auto& recipe:a.memberRecipes.at(i)){auto peer=std::find_if(b.memberRecipes.at(j).begin(),b.memberRecipes.at(j).end(),[&](const auto& entry){return entry.name==recipe.name;});if(peer==b.memberRecipes.at(j).end())continue;auto x=EiemResourceRecipe::Apply(recipe,originals,fast.manifest.package,fast.preparedFrames,false),y=EiemResourceRecipe::Apply(*peer,originals,full.manifest.package,full.preparedFrames,false);if(x!=y){auto bad=std::mismatch(x.begin(),x.end(),y.begin(),y.end());std::cout<<"MEMBER-DIFF name="<<recipe.name<<" offset="<<size_t(bad.first-x.begin())<<" sizes="<<x.size()<<'/'<<y.size()<<std::endl;}else std::cout<<"MEMBER-EQUAL name="<<recipe.name<<" size="<<x.size()<<std::endl;}
    Require(false,"native bound view byte differs: source="+a.originals[i].file+" offset="+std::to_string(position));
   }offset+=count;compared+=count;}
  HydrateInput(fast,i);HydrateInput(full,j);auto decoded=EndfieldInput::Decode(*original);std::map<std::string,const Bytes*> originalMembers;for(const auto& member:decoded.members)originalMembers.emplace(member.name,&member.bytes);std::map<std::string,const EiemResourceRecipe::Member*> fullRecipes;for(const auto& recipe:b.memberRecipes.at(j))fullRecipes.emplace(recipe.name,&recipe);
  Require(a.memberRecipes.at(i).size()==fullRecipes.size(),"native member recipe count differs");for(const auto& recipe:a.memberRecipes.at(i)){auto peer=fullRecipes.find(recipe.name);Require(peer!=fullRecipes.end(),"native member recipe identity differs");auto x=EiemResourceRecipe::Apply(recipe,originalMembers,fast.manifest.package,fast.preparedFrames,!a.resourceAddressed);auto y=EiemResourceRecipe::Apply(*peer->second,originalMembers,full.manifest.package,full.preparedFrames,!b.resourceAddressed);NormalizeGeneratedPartNames(x);NormalizeGeneratedPartNames(y);EqualBytes(x,y,"member replay "+recipe.name);compared+=x.size();}
  if(i%10==0||i+1==a.originals.size())std::cout<<"DIFFERENTIAL source="<<i+1<<'/'<<a.originals.size()<<" comparedBytes="<<compared<<std::endl;
 }
 return compared;
}
struct StoreStats {uint64_t sources=0,proofs=0,newBlocks=0,newBytes=0,totalBytes=0,indexBytes=0,authorBytes=0,packageBytes=0,orphanBytes=0;};
StoreStats Store(const std::filesystem::path& mod,const EiemModUpdateStore::Snapshot& old){
 auto current=EiemModUpdateStore::Read(mod/"compiled.bin");StoreStats stats;stats.indexBytes=std::filesystem::file_size(mod/"compiled.bin");stats.totalBytes=stats.indexBytes;std::set<std::string> priorDigests,live;for(const auto& block:old.blocks)priorDigests.insert(block.second.digest);
 for(const auto& block:current.blocks){if(live.insert(block.second.digest).second)stats.totalBytes+=block.second.size;auto previous=old.blocks.find(block.first);bool changed=previous==old.blocks.end()||previous->second.digest!=block.second.digest;if(changed){stats.sources+=block.first.rfind("runtime/source/",0)==0;stats.proofs+=block.first.rfind("runtime/proof/",0)==0;}if(!priorDigests.count(block.second.digest)&&live.count(block.second.digest)){++stats.newBlocks;stats.newBytes+=block.second.size;priorDigests.insert(block.second.digest);}}
 auto author=Load(mod);stats.authorBytes=std::filesystem::file_size(mod/"mod.ini");for(const auto& row:author.resources)stats.authorBytes+=row.second->bytes.size();
 if(std::filesystem::is_regular_file(mod/"source-inputs.bin"))stats.authorBytes+=std::filesystem::file_size(mod/"source-inputs.bin");
 stats.packageBytes=stats.authorBytes+stats.totalBytes;
 if(current.external&&std::filesystem::is_directory(mod/"compiled.blocks"))for(const auto& entry:std::filesystem::directory_iterator(mod/"compiled.blocks"))if(entry.is_regular_file()&&!live.count(entry.path().stem().u8string()))stats.orphanBytes+=entry.file_size();
 return stats;
}
void Run(const std::filesystem::path& game,const std::filesystem::path& baseline,const std::filesystem::path& output,const std::string& selected){
 auto loaded=EiemPublishedCompiledMod::Read(baseline,game);Require(loaded.store&&loaded.artifact.explicitResources,"baseline must be cold-migrated to explicit resource block storage");auto mutations=Mutations(loaded.artifact);Require(!mutations.empty(),"baseline has no covered mutable resource category");auto originalStore=EiemModUpdateStore::Read(baseline/"compiled.bin");Require(!std::filesystem::exists(output),"test output directory must be new");std::filesystem::create_directories(output);
 std::ofstream report(output/"report.tsv",std::ios::binary);report<<"case\tfast_ms\tfull_ms\tresource_count\tgenerated_count\tnew_source_blocks\tnew_proof_blocks\tnew_content_blocks\tnew_bytes\tfast_live_bytes\tfull_live_bytes\tfast_index_bytes\tfull_index_bytes\tfast_private_start\tfast_private_peak\tfull_private_start\tfull_private_peak\tcompared_bytes\tfast_author_bytes\tfull_author_bytes\tfast_package_bytes\tfull_package_bytes\tfast_orphan_bytes\tfull_orphan_bytes\n";std::ofstream costs(output/"costs.tsv",std::ios::binary);costs<<"case\tmode\tstage\tvalue\n";
 std::vector<std::pair<std::string,std::vector<Mutation>>> cases;for(const auto& mutation:mutations)cases.push_back({mutation.category,{mutation}});if(mutations.size()>1)cases.push_back({"mixed",mutations});size_t completed=0;
 for(const auto& item:cases){if(!selected.empty()&&selected!=item.first)continue;auto root=output/std::filesystem::u8path(item.first),fast=root/"fast"/baseline.filename(),full=root/"full"/baseline.filename();std::filesystem::create_directories(root);CopyMod(baseline,fast);CopyMod(baseline,full);Apply(fast,item.second);Apply(full,item.second);Require(std::filesystem::remove(full/"compiled.bin"),"cold rebuild compiled index removal failed");
  std::cout<<"BEGIN case="<<item.first<<" mode=fast"<<std::endl;auto fastStats=ExportTimed(fast,game,baseline);std::cout<<"EXPORTED case="<<item.first<<" mode=fast wallMs="<<fastStats.wall<<" peakPrivate="<<fastStats.peakPrivate<<std::endl;
  std::cout<<"BEGIN case="<<item.first<<" mode=full"<<std::endl;auto fullStats=ExportTimed(full,game,{});std::cout<<"EXPORTED case="<<item.first<<" mode=full wallMs="<<fullStats.wall<<" peakPrivate="<<fullStats.peakPrivate<<std::endl;
  for(const auto* mode:{&fastStats,&fullStats})for(const auto& row:mode->costs)costs<<item.first<<'\t'<<(mode==&fastStats?"fast":"full")<<'\t'<<row.first<<'\t'<<std::setprecision(12)<<row.second<<'\n';costs.flush();
  auto fastStore=Store(fast,originalStore),fullStore=Store(full,{});auto nextFast=EiemPublishedCompiledMod::Read(fast,game),nextFull=EiemPublishedCompiledMod::Read(full,game);auto compared=Differential(nextFast.artifact,nextFull.artifact);
  auto resources=fastStats.costs["resource-rebind-resources"],generated=fastStats.costs["resource-rebind-generated"];std::set<std::string> expected;for(const auto& mutation:item.second)expected.insert(mutation.resource);
  Require(size_t(resources)==expected.size()&&fastStore.sources==0&&fastStore.proofs==0,"test did not keep source/proof blocks immutable during resource update");
  report<<item.first<<'\t'<<std::setprecision(12)<<fastStats.wall<<'\t'<<fullStats.wall<<'\t'<<resources<<'\t'<<generated<<'\t'<<fastStore.sources<<'\t'<<fastStore.proofs<<'\t'<<fastStore.newBlocks<<'\t'<<fastStore.newBytes<<'\t'<<fastStore.totalBytes<<'\t'<<fullStore.totalBytes<<'\t'<<fastStore.indexBytes<<'\t'<<fullStore.indexBytes<<'\t'<<fastStats.startPrivate<<'\t'<<fastStats.peakPrivate<<'\t'<<fullStats.startPrivate<<'\t'<<fullStats.peakPrivate<<'\t'<<compared<<'\t'<<fastStore.authorBytes<<'\t'<<fullStore.authorBytes<<'\t'<<fastStore.packageBytes<<'\t'<<fullStore.packageBytes<<'\t'<<fastStore.orphanBytes<<'\t'<<fullStore.orphanBytes<<'\n';report.flush();
  std::cout<<"PASS case="<<item.first<<" fastMs="<<fastStats.wall<<" fullMs="<<fullStats.wall<<" resources="<<resources<<" generated="<<generated<<" changedSource="<<fastStore.sources<<" changedProof="<<fastStore.proofs<<" newBlocks="<<fastStore.newBlocks<<" newBytes="<<fastStore.newBytes<<" liveBytes="<<fastStore.totalBytes<<" comparedBytes="<<compared<<std::endl;++completed;
 }
 Require(completed>0,"selected category is absent from baseline");std::cout<<"PASS unified real resources cases="<<completed<<" report="<<(output/"report.tsv").u8string()<<std::endl;
}
} // namespace UnifiedReal
int wmain(int argc,wchar_t** argv){try{
 if(argc==4&&std::wstring(argv[1])==L"--validate-bindings"){
  auto game=std::filesystem::canonical(argv[2]),mod=std::filesystem::canonical(argv[3]);auto loaded=EiemCompiledModStore::Load(mod/"compiled.bin",mod,game,true);
  auto bindings=EiemAddressedResources::Decode(loaded.artifact.resourceBindingBytes,loaded.artifact.manifest.package);size_t mapped=0;
  for(const auto& row:loaded.artifact.manifest.package.resources)if(row.second->mappedBytes&&row.second->bytes.empty())++mapped;
  EiemNativeInput::Require(mapped>0,"diagnostic did not load any mapped author resources");
  std::cout<<"PASS mapped store bindings resources="<<mapped<<" ranges="<<bindings.ranges.size()<<" meshes="<<bindings.meshes.size()<<std::endl;return 0;
 }
 if(argc==4&&std::wstring(argv[1])==L"--metadata"){
  auto game=std::filesystem::canonical(argv[2]),mod=std::filesystem::canonical(argv[3]);auto before=UnifiedReal::PrivateBytes();UnifiedReal::PeakMemory memory;std::vector<std::pair<std::string,double>> costs;auto begin=std::chrono::steady_clock::now();auto loaded=EiemPublishedCompiledMod::Read(mod,game,[&](const char* stage,double value){costs.emplace_back(stage,value);},true,true);auto wall=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count();auto peak=memory.Finish();
  std::cout<<"METADATA-READ wallMs="<<wall<<" privateStart="<<before<<" privatePeak="<<peak<<" sourceCount="<<loaded.artifact.views.size()<<" hydratedProofs="<<loaded.artifact.hydrated.size()<<" decodedMembers="<<loaded.artifact.members.size()<<" store="<<loaded.store<<" externalSnapshot="<<loaded.externalSnapshot<<'\n';
  for(const auto& row:costs)std::cout<<"METADATA-COST "<<row.first<<' '<<row.second<<'\n';
  if(loaded.store)EiemNativeInput::Require(loaded.artifact.hydrated.empty()&&loaded.artifact.members.empty(),"metadata-only read expanded per-input proofs");return 0;
 }
 if(argc==3&&std::wstring(argv[1])==L"--stats"){
  auto mod=std::filesystem::canonical(argv[2]);auto author=UnifiedReal::Load(mod);uint64_t authorBytes=std::filesystem::file_size(mod/"mod.ini");for(const auto& row:author.resources)authorBytes+=row.second->bytes.size();if(std::filesystem::is_regular_file(mod/"source-inputs.bin"))authorBytes+=std::filesystem::file_size(mod/"source-inputs.bin");
  if(EiemCompiledModStore::IsStore(mod/"compiled.bin")){auto stats=UnifiedReal::Store(mod,{});std::cout<<"STORE-SIZE indexBytes="<<stats.indexBytes<<" liveCompiledBytes="<<stats.totalBytes<<" authorBytes="<<stats.authorBytes<<" livePackageBytes="<<stats.packageBytes<<" orphanBytes="<<stats.orphanBytes<<'\n';}
  else{auto compiled=std::filesystem::file_size(mod/"compiled.bin");std::cout<<"LEGACY-SIZE compiledBytes="<<compiled<<" authorBytes="<<authorBytes<<" livePackageBytes="<<compiled+authorBytes<<'\n';}return 0;
 }
 if(argc==3&&std::wstring(argv[1])==L"--pack-schema"){
  auto mod=std::filesystem::canonical(argv[2]);auto path=mod/"compiled.bin";EiemModUpdateStore::File file(path,false);auto snapshot=EiemModUpdateStore::Read(file);auto bytes=EiemModUpdateStore::Get(file,snapshot,"runtime/schema-dictionary");
  bool packed=false;if(bytes.size()>=32){auto digest=EiemDirectory::Sha(bytes.data(),bytes.size()-32);packed=std::equal(digest.begin(),digest.end(),bytes.end()-32);}
  if(!packed){EiemCompiledModStore::PublishSnapshot(path,path,snapshot.generation,{{"runtime/schema-dictionary",EiemCompiledMod::Pack(bytes)}});std::cout<<"PACKED test baseline schema dictionary\n";}else std::cout<<"UNCHANGED schema dictionary already packed\n";return 0;
 }
 if(argc==5&&std::wstring(argv[1])==L"--compare"){
  auto game=std::filesystem::canonical(argv[2]);auto fast=EiemPublishedCompiledMod::Read(std::filesystem::canonical(argv[3]),game),full=EiemPublishedCompiledMod::Read(std::filesystem::canonical(argv[4]),game);auto compared=UnifiedReal::Differential(fast.artifact,full.artifact);std::cout<<"PASS comparedBytes="<<compared<<std::endl;return 0;
 }
 if(argc==5&&std::wstring(argv[1])==L"--fast"){
  auto game=std::filesystem::canonical(argv[2]),baseline=std::filesystem::canonical(argv[3]),fast=std::filesystem::canonical(argv[4]);EiemNativeInput::Require(!std::filesystem::equivalent(baseline,fast),"fast diagnostics must write to a private test copy");auto stats=UnifiedReal::ExportTimed(fast,game,baseline);std::cout<<"FAST-ONLY wallMs="<<stats.wall<<" startPrivate="<<stats.startPrivate<<" peakPrivate="<<stats.peakPrivate<<std::endl;for(const auto& row:stats.costs)std::cout<<"FAST-COST "<<row.first<<' '<<row.second<<std::endl;return 0;
 }
 EiemNativeInput::Require(argc==4||argc==5,"unified_resources_real <game> <cold baseline Mod> <new output directory> [mesh|material|texture|physics|effects|mixed]");
 auto game=std::filesystem::canonical(argv[1]),baseline=std::filesystem::canonical(argv[2]);auto output=std::filesystem::absolute(argv[3]).lexically_normal();auto selected=argc==5?std::filesystem::path(argv[4]).u8string():std::string{};UnifiedReal::Run(game,baseline,output,selected);return 0;
 }catch(const std::exception& error){std::cerr<<"FAIL unified real resources: "<<error.what()<<std::endl;return 1;}}

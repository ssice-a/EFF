#pragma once
// Execute the production replacement path with a Unity-like duplicate-CAB
// rejection. Native prefab lifetime is independent of container lifetime.
namespace EffectReplacementCacheTest {
using namespace Entry;
namespace D=EffectDebug;
unsigned attempts=0,assetReads=0,collisions=0,failAt=0;
bool failAsset=false;
std::map<std::string,void*> containers;
void* Invoke(void* method,void* self,void** args,void** error){
 *error=nullptr;
 if(method==(void*)911){std::vector<void*> live;for(const auto& row:containers)if(!EffectTest::dead.count(row.second))live.push_back(row.second);return Test::Array(live);}
 if(method==(void*)90){++assetReads;if(failAsset)return nullptr;EffectTest::dead.erase(Test::candidate);return Test::candidate;}
 if(method==(void*)33){Require(args&&*(unsigned char*)args[0]==0,"replacement retirement destroyed live instances");++Test::unloadCalls;EffectTest::dead.insert(self);NativeBundle::memoryBundles.erase(self);return nullptr;}
 return EffectTest::Invoke(method,self,args,error);
}
void* Load(void* stream,uint32_t,uint32_t){
 ++attempts;if(attempts==failAt)return nullptr;
 Bytes bytes;auto view=NativeBundle::StreamView(stream);
 if(view){bytes.resize(size_t(view->Length()));Require(view->Read(bytes.data(),bytes.size())==bytes.size(),"fixture view short read");}
 else{auto array=*(void**)stream;size_t size=0;Require(Entry::ArrayLength(array,&size),"fixture byte array missing");bytes.assign((unsigned char*)array+32,(unsigned char*)array+32+size);}
 std::string key(bytes.begin(),bytes.end());auto found=containers.find(key);
 if(found!=containers.end()&&!EffectTest::dead.count(found->second)){++collisions;return nullptr;}
 auto bundle=(void*)(uintptr_t(0xA000)+attempts);containers[key]=bundle;return bundle;
}
void Checks(){
 EffectTest::Setup();il2cpp_runtime_invoke=Invoke;NativeBundle::enabled=true;NativeBundle::api.streamIcall=Load;NativeBundle::viewCallbacksReady=true;
 auto manifest=std::const_pointer_cast<D::Manifest>(EffectTest::Manifest('R'));manifest->bundles.clear();manifest->root=2;
 for(unsigned i=0;i<3;++i){EiemEffectNative::Bundle b;b.hash=std::string(64,char('A'+i));b.file=std::to_string(i)+".ab";
  auto data=std::make_shared<const Bytes>(EiemNativeInput::EndfieldInput::Encode(EiemAuthorMeshBundle::EnvelopeHeader(),{{"CAB-fixture-"+std::to_string(i),4,Bytes{1,2,3,(unsigned char)i}}}));
  b.bytes=data;
  // Exercise both source.open (installed Format 3) and immutable byte routes.
  if(i!=1){b.open=[data]{return std::make_shared<const EiemInputSpanView::View>(std::vector<EiemInputSpanView::Span>{{EiemInputSpanView::literalSource,0,data->size(),data}},std::vector<size_t>{},EiemInputSpanView::ReadRange{});};}
  manifest->bundles.push_back(std::move(b));
 }
 Package package;package.id="ReplacementCacheFixture";EffectAttachment effect;effect.id="red";effect.targetPrefab="assets/effect.prefab";effect.native=manifest;package.effects[effect.id]=effect;
 D::PublishReplacements({&package});auto route=[&]{return D::RouteReplacement(effect.targetPrefab,Test::source);};
 auto check=EffectTest::Check;
 check(route()==Test::candidate&&attempts==3,"initial replacement did not load three bundles");
 auto reads=assetReads;check(route()==Test::candidate&&attempts==3&&assetReads==reads,"live prefab cache hit performed IO");
 for(unsigned i=0;i<3;++i){EffectTest::dead.insert(Test::candidate);check(route()==Test::candidate,"dead prefab with live bundles fell back to green");}
 check(attempts==3&&collisions==0,"prefab recovery submitted duplicate live containers");
 auto& cached=D::loadedReplacements.at(effect.targetPrefab);
 auto old=cached.bundles[1].Get();EffectTest::dead.insert(old);NativeBundle::memoryBundles.erase(old);EffectTest::dead.insert(Test::candidate);
 check(route()==Test::candidate&&attempts==4,"partial recovery did not load exactly the missing bundle");
 EffectTest::dead.insert(Test::candidate);failAsset=true;reads=assetReads;
 check(route()==Test::source,"asset failure did not preserve original game resource");failAsset=false;
 check(route()==Test::candidate&&attempts==4&&assetReads==reads+2,"asset failure discarded surviving bundles or blocked retry");
 EffectTest::dead.insert(Test::candidate);EffectTest::badName=true;
 check(route()==Test::source,"unverified prefab root was published");EffectTest::badName=false;reads=assetReads;
 check(route()==Test::candidate&&assetReads==reads+1,"failed prefab validation polluted the fast path cache");
 auto unloads=Test::unloadCalls;D::PublishReplacements({&package});
 check(route()==Test::candidate&&attempts==4&&Test::unloadCalls==unloads,"unchanged publication retired cache");
 D::PublishReplacements({});check(Test::unloadCalls==unloads+3&&D::loadedReplacements.empty(),"removing route did not retire owned bundles");
 // Failure after one successful native load must retain that first container.
 D::PublishReplacements({&package});failAt=attempts+2;
 check(route()==Test::source,"partial bundle failure did not preserve game resource");auto failedAttempts=attempts;failAt=0;
 check(route()==Test::candidate&&attempts==failedAttempts+2&&collisions==0,"partial failure retried an already loaded bundle");
 // A changed generation must retire, then demand-load its new container set.
 auto next=std::make_shared<D::Manifest>(*manifest);next->key=std::string(64,'S');package.effects[effect.id].native=next;
 unloads=Test::unloadCalls;D::PublishReplacements({&package});
 check(Test::unloadCalls==unloads+3&&D::loadedReplacements.empty(),"changed generation retained old ownership");
 auto before=attempts;check(route()==Test::candidate&&attempts==before+3&&collisions==0,"changed generation could not reload");
 D::PublishReplacements({});check(NativeBundle::memoryBundles.empty(),"removed replacement left native stream owners");
 std::cout<<"PASS replacement cache: initial load, warm hit, three prefab retirements, missing bundle, asset failure, invalid root, partial load retry, unchanged/changed/removed publication; duplicate submissions="<<collisions<<"\n";
 RuntimeShapes::managedAlive=false;
}
}

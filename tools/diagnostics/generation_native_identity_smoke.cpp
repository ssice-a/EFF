#define main StaticAssemblyFixtureMain
#include "static_mesh_assembly_smoke.cpp"
#undef main
#include "../../src/eiem_native_generation_identity.h"
#include "../../src/eiem_author_mesh_bundle.h"
#include "../../src/eiem_vfs_catalog.h"

namespace G=EiemNativeGenerationIdentity;
namespace R=EiemNativeRandomAccess;
G::View NativeView(const Bytes& serialized,const std::string& cab,const Bytes& pixels={}){
  std::vector<EiemNativeViewWriter::Member> members{{cab,4,0,serialized.size()}};
  if(!pixels.empty())members.push_back({cab+".resS",0,serialized.size(),pixels.size()});
  auto bytes=std::make_shared<Bytes>(EiemNativeViewWriter::Prefix(EiemAuthorMeshBundle::EnvelopeHeader(),
    {{serialized.size()+pixels.size(),serialized.size()+pixels.size(),0}},members));
  bytes->insert(bytes->end(),serialized.begin(),serialized.end());bytes->insert(bytes->end(),pixels.begin(),pixels.end());
  return std::make_shared<const EiemInputSpanView::View>(std::vector<EiemInputSpanView::Span>{{EiemInputSpanView::literalSource,0,bytes->size(),bytes}},std::vector<size_t>{},EiemInputSpanView::ReadRange{});
}
void InstalledGame(const char* game){
  const std::set<std::string> selected{
    "bundles/windows/main/2699e699f63135f3465693b7.ab",
    "bundles/windows/main/2e2c42f7f19ba91ee0a5a97d.ab",
    "bundles/windows/main/3a3f434f355f7ee2e5194686.ab",
    "bundles/windows/main/a3e567a75a3baa146889b321.ab",
    "bundles/windows/main/497656735db1ec8ba0245517.ab"};
  const auto entries=EiemVfsCatalog::ReadEffectiveVfs(std::filesystem::u8path(game),selected);
  std::vector<G::View> sources;size_t bytesRead=0,totalBytes=0,externalRemaps=0,streamFields=0,objects=0;
  for(const auto& entry:entries){
    Require(!entry.second.encrypted,"installed fixture has encrypted VFS input");
    auto file=std::make_shared<R::File>(entry.second.physical,entry.second.offset);auto size=size_t(entry.second.size);
    Require(size<=file->Size(),"installed native range exceeds backing file");totalBytes+=size;
    sources.push_back(std::make_shared<const EiemInputSpanView::View>(
      std::vector<EiemInputSpanView::Span>{{0,0,size,{}}},std::vector<size_t>{size},
      [file,&bytesRead](size_t,size_t at,unsigned char* out,size_t n){bytesRead+=n;file->ReadAt(at,out,n);}));
  }
  auto names=G::Collect(17,sources);
  for(size_t index=0;index<sources.size();++index){
    auto source=sources[index],isolated=G::Isolate(source,names,"eff/17/fixture/"+std::to_string(index)+".ab");
    auto read=[](G::View view){return [view](size_t at,unsigned char* out,size_t n){Require(view->ReadAt(at,out,n)==n,"fixture read truncated");};};
    R::Reader before(source->Size(),read(source)),after(isolated->Size(),read(isolated));
    Require(before.directory.members.size()==after.directory.members.size(),"installed input lost a member");
    for(size_t mi=0;mi<before.directory.members.size();++mi){
      auto original=before.directory.members[mi],current=after.directory.members[mi];
      Require(current.name==names.at(original.name)&&current.name!=original.name&&current.flags==original.flags,"installed native member identity was not isolated");
      if(!(original.flags&4))continue;
      auto oldLayout=before.Layout(original),newLayout=after.Layout(current);objects+=oldLayout.objects.size();
      Require(oldLayout.types.size()==newLayout.types.size()&&oldLayout.order==newLayout.order&&oldLayout.externals.size()==newLayout.externals.size(),"installed schema/object/external table changed");
      for(size_t ti=0;ti<oldLayout.types.size();++ti)Require(oldLayout.types[ti].nativeRecord==newLayout.types[ti].nativeRecord,"installed native type record changed");
      for(size_t ei=0;ei<oldLayout.externals.size();++ei){auto expected=G::Path(oldLayout.externals[ei],names);
        Require(newLayout.externals[ei]==expected,"installed native external did not follow its generation");externalRemaps+=expected!=oldLayout.externals[ei];}
      for(auto id:oldLayout.order){const auto& type=oldLayout.types.at(oldLayout.objects.at(id).type);
        Require(newLayout.objects.at(id).type==oldLayout.objects.at(id).type,"installed object type changed");
        std::set<std::string> wanted;for(auto node:type.nodes.at(0).children)if(G::StreamSchema(type,node))wanted.insert(type.nodes.at(node).name);
        if(wanted.empty())continue;
        auto fields=before.Fields(original,oldLayout,id,wanted),actual=after.Fields(current,newLayout,id,wanted);
        for(auto& field:fields){G::RemapStream(type,field.second,names);Bytes expectedBytes,actualBytes;
          WriteValue(type,field.second,expectedBytes);WriteValue(type,actual.at(field.first),actualBytes);
          Require(expectedBytes==actualBytes,"installed streaming descriptor differs");++streamFields;}
      }
    }
  }
  Require(externalRemaps>0,"installed fixture did not exercise cross-Bundle native references");
  std::cout<<"PASS installed chen world/UI native inputs="<<sources.size()<<" objects="<<objects<<" crossCabRemaps="<<externalRemaps<<" streamFields="<<streamFields<<" sourceBytes="<<totalBytes<<" bytesRead="<<bytesRead<<'\n';
}
int main(int argc,char** argv){try{
  Fixture consumer,carrier;consumer.cab="CAB-11111111111111111111111111111111";carrier.cab="CAB-22222222222222222222222222222222";
  SetReference(consumer.T(12),Child(consumer.T(12),consumer.V(12),"m_Mesh"),{carrier.cab,100},consumer.refs,consumer.cab);
  const std::string vanillaTexture="CAB-33333333333333333333333333333333";
  SetReference(consumer.T(13),Child(consumer.T(13),consumer.V(13),"m_ProbeAnchor"),{vanillaTexture,700},consumer.refs,consumer.cab);
  carrier.types.push_back(Schema(28,{{"string","m_Name",{}},{"StreamingInfo","m_StreamData",{{"UInt64","offset",{}},{"UInt32","size",{}},{"string","path",{}}}}}));carrier.Add(400,carrier.types.size()-1);
  auto& stream=Child(carrier.T(400),carrier.V(400),"m_StreamData");
  SetString(carrier.T(400),Child(carrier.T(400),stream,"path"),"archive:/"+carrier.cab+"/"+carrier.cab+".resS");
  SetInteger(carrier.T(400),Child(carrier.T(400),stream,"size"),8*1024*1024);
  SetString(carrier.T(100),Child(carrier.T(100),carrier.V(100),"m_Name"),carrier.cab);
  auto a=NativeView(consumer.BytesOf(),consumer.cab),b=NativeView(carrier.BytesOf(),carrier.cab,Bytes(8*1024*1024,91));
  size_t bytesRead=0;auto source=b;
  b=std::make_shared<const EiemInputSpanView::View>(std::vector<EiemInputSpanView::Span>{{0,0,source->Size(),{}}},std::vector<size_t>{source->Size()},
    [source,&bytesRead](size_t,size_t at,unsigned char* out,size_t n){bytesRead+=n;Require(source->ReadAt(at,out,n)==n,"test source truncated");});
  auto names=G::Collect(7,{a,b});auto next=G::Collect(8,{a,b});
  auto isolatedA=G::Isolate(a,names,"eff/7/root/consumer.ab"),isolatedB=G::Isolate(b,names,"eff/7/root/carrier.ab");
  R::Reader ar(isolatedA->Size(),[isolatedA](size_t at,unsigned char* out,size_t n){isolatedA->ReadAt(at,out,n);});
  R::Reader br(isolatedB->Size(),[isolatedB](size_t at,unsigned char* out,size_t n){isolatedB->ReadAt(at,out,n);});
  auto am=ar.directory.members.at(0),bm=br.directory.members.at(0);auto al=ar.Layout(am),bl=br.Layout(bm);
  Require(am.name!=consumer.cab&&bm.name!=carrier.cab&&am.name==names.at(consumer.cab)&&bm.name==names.at(carrier.cab),"new generation retained the live vanilla SerializedFile CAB identity");
  Require(next.at(consumer.cab)!=am.name&&G::Collect(7,{a,b})==names,"generation identity is unstable or reused across F10");
  auto bundle=ar.Fields(am,al,300,{"m_Name","m_AssetBundleName"});
  for(const auto& field:bundle)Require(Text(field.second.raw.data(),field.second.raw.size())=="eff/7/root/consumer.ab","native AssetBundle name retained its vanilla identity");
  auto mesh=ar.Object(am,al,12);auto meshRef=NullableReference(al.types.at(al.objects.at(12).type),Child(al.types.at(al.objects.at(12).type),mesh,"m_Mesh"),al,am.name);
  Require(meshRef==NativeReference{bm.name,100},"new Prefab still references the retained vanilla Mesh CAB");
  auto renderer=ar.Object(am,al,13);const auto& rt=al.types.at(al.objects.at(13).type);
  Require(NullableReference(rt,Child(rt,renderer,"m_ProbeAnchor"),al,am.name)==NativeReference{vanillaTexture,700},"unchanged game resource was copied or redirected");
  auto texture=br.Fields(bm,bl,400,{"m_StreamData"});const auto& tt=bl.types.at(bl.objects.at(400).type);
  auto path=Child(tt,texture.at("m_StreamData"),"path");
  Require(Text(path.raw.data(),path.raw.size())=="archive:/"+bm.name+"/"+br.directory.members.at(1).name,"resource stream path was not remapped with its native member");
  auto label=br.Fields(bm,bl,100,{"m_Name"}).at("m_Name");
  Require(Text(label.raw.data(),label.raw.size())==carrier.cab,"identity rewriting changed an author asset name");
  Require(bytesRead<1024*1024,"generation preparation materialized pixel payloads");
  auto pixel=br.ReadAt(br.directory.members.at(1).offset+7,1,1);Require(pixel==Bytes{91},"stream bytes changed");
  std::cout<<"PASS native 0-to-1 and 1-to-0-to-1 identities, cross-CAB references, unchanged game dependencies, lazy texture stream; readBytes="<<bytesRead<<'\n';
  if(argc==3&&std::string(argv[1])=="--game")InstalledGame(argv[2]);
  else Require(argc==1,"usage: smoke.exe [--game GAME_DIRECTORY]");return 0;
}catch(const std::exception& error){std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}}

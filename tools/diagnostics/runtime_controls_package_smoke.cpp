#include "../../src/eiem_offline_resource_package.h"
#include <iostream>
int main(int argc,char** argv){try{
 using namespace EiemDirectory;Require(argc==2,"runtime_controls_package_smoke <mod>");auto p=Load(std::filesystem::u8path(argv[1]));
 Require(!p.effects.empty(),"runtime effects not reconstructed from declarations");
 if(!p.parts.empty()&&(!p.materialTracks.empty()||!p.materialAnimationIds.empty()))Require(!p.materialTracks.empty()&&!p.materialAnimationIds.empty(),"runtime material controls not reconstructed from declarations");
 EiemOfflinePackage::Writer writer;EiemOfflinePackage::WriteAuthor(writer,p);EiemOfflinePackage::Reader reader{writer.b};auto copy=EiemOfflinePackage::ReadAuthor(reader,p.root.parent_path());
 Require(reader.p==writer.b.size()&&copy.materialTracks.size()==p.materialTracks.size()&&copy.effects.size()==p.effects.size()&&copy.shapeAnimations.size()==p.shapeAnimations.size(),"runtime controls lost in compiled author roundtrip");
 auto& effect=copy.effects.begin()->second;Require(effect.native&&effect.native->bundles.front().Data()&&effect.part==p.effects.begin()->second.part,"compiled effect bytes/target lost");
 auto resource=std::find_if(copy.resources.begin(),copy.resources.end(),[](const auto& row){return row.second->kind==10;});Require(resource!=copy.resources.end(),"effect resource backing absent");
 const auto& bundle=effect.native->bundles.front();auto owner=bundle.Owner();Require(owner&&!owner.owner_before(resource->second)&&!resource->second.owner_before(owner),"effect archive does not share its Resource owner");
 auto start=reinterpret_cast<uintptr_t>(resource->second->bytes.data()),address=reinterpret_cast<uintptr_t>(bundle.Data());Require(address>=start&&address-start<=resource->second->bytes.size()&&bundle.Size()<=resource->second->bytes.size()-(address-start),"effect archive is not a view into the package resource");
 std::cout<<"PASS Python author -> C++ declaration -> compiled author roundtrip: materialTracks="<<copy.materialTracks.size()<<" effects="<<copy.effects.size()<<" bundles="<<effect.native->bundles.size()<<" copiedArchiveBytes=0\n";return 0;
 }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}

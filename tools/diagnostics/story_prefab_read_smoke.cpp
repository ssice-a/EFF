#define NOMINMAX
#include "../../src/eiem_native_random_access.h"
#include "../../src/eiem_renderer_input_builder.h"
#include "../../src/eiem_vfs_catalog.h"
#include "../../src/eiem_demand_native_assembly.h"
#include "../eff_author_sources.h"
#include <iostream>

using namespace EiemNativeInput;
namespace R=EiemNativeRandomAccess;
namespace {
using NativeSlots=std::map<int64_t,std::set<std::pair<NativeReference,size_t>>>;
NativeSlots NativePalette(R::Reader& reader,const EiemNativeViewWriter::Member& member,const SerializedLayout& layout,
    const std::vector<BuiltMeshPart>& parts) {
  std::map<NativeReference,const BuiltMeshPart*> meshes;
  for(const auto& part:parts)meshes.emplace(CanonicalSerializedAssetReference({part.cab,part.sourceId}),&part);
  NativeSlots result;
  std::set<NativeReference> reported;
  for(auto id:layout.order){
    const auto& type=layout.types.at(layout.objects.at(id).type);
    if(type.classId!=137)continue;
    auto value=reader.Project(member,layout,id,{"m_Mesh","m_Bones"});
    auto ref=CanonicalSerializedAssetReference(NullableReference(type,Child(type,value,"m_Mesh"),layout,member.name));
    auto found=meshes.find(ref);
    if(found==meshes.end())continue;
    const auto& part=*found->second;
    const auto& slots=ConstArray(type,Child(type,value,"m_Bones")).children;
    if(slots.size()!=part.sourceBoneHashes.size())continue;
    if(reported.insert(ref).second)
      std::cout<<"NATIVE-PALETTE mesh="<<part.target.asset<<" ref="<<ref.first<<":"<<ref.second<<std::endl;
    for(size_t slot=0;slot<slots.size();++slot){
      auto bone=NullableReference(type,slots[slot],layout,member.name);
      if(bone.first==member.name&&bone.second)result[bone.second].emplace(ref,slot);
    }
  }
  return result;
}

bool SameBindPose(const EiemAuthorBoneAnimation::Node& rest,const Type& type,const Value& transform) {
  const auto position=ReadVector(type,Child(type,transform,"m_LocalPosition"));
  const auto scale=ReadVector(type,Child(type,transform,"m_LocalScale"));
  for(size_t axis=0;axis<position.size();++axis)
    if(std::abs(rest.position[axis]-position[axis])>1e-5||std::abs(rest.scale[axis]-scale[axis])>1e-5)return false;
  const auto& rotation=Child(type,transform,"m_LocalRotation");
  double dot=0;
  for(size_t axis=0;axis<rest.rotation.size();++axis)
    dot+=rest.rotation[axis]*FloatValue(type,Child(type,rotation,std::string(1,"xyzw"[axis])));
  return std::abs(std::abs(dot)-1)<1e-5;
}

std::pair<size_t,size_t> VerifyRetargetBindings(const Bytes& bytes,const std::string& cab,
    const EiemDemandNativeAssembly::Author& author,const RendererInputBuild& output,const NativeSlots& native) {
  auto layout=ParseSerialized(bytes);
  std::map<int64_t,Value> objects;
  auto type=[&](int64_t id)->const Type&{return layout.types.at(layout.objects.at(id).type);};
  auto value=[&](int64_t id)->const Value&{
    auto it=objects.find(id);
    if(it==objects.end())it=objects.emplace(id,ObjectValue(bytes,layout,id)).first;
    return it->second;
  };
  auto local=[&](const Type& schema,const Value& field){
    auto ref=NullableReference(schema,field,layout,cab);
    Require(ref.first==cab,"retarget fixture found nonlocal transform");
    return ref.second;
  };
  auto pointer=[&](int64_t id,const char* field){return local(type(id),Child(type(id),value(id),field));};
  auto name=[&](int64_t id){
    const auto owner=pointer(id,"m_GameObject");
    const auto& field=Child(type(owner),value(owner),"m_Name");
    return Text(field.raw.data(),field.raw.size());
  };
  std::set<int64_t> verified;
  std::map<std::pair<int64_t,std::string>,EiemAuthorBoneAnimation::RetargetRoutes> bindingTables;
  size_t missing=0,paletteBindings=0;
  for(const auto& part:output.parts){
    const auto& mesh=author.package.meshes.at(author.package.parts.at(part.part).mesh);
    if(mesh.skeleton.empty())continue;
    const auto& catalog=author.meshSources.at(part.part).first;
    auto skeleton=catalog->GetSkeleton(mesh.skeleton);
    if(skeleton->retargets.empty())continue;
    const auto prefix=EiemAuthorBoneAnimation::RetargetRoutePrefix(
        EiemComposedPackage::Name(author.catalogInstances.at(catalog->Root()),mesh.skeleton));
    Require(part.bones.size()==mesh.bones.size(),"retarget fixture palette size differs");
    std::map<std::string,size_t> nodes;
    for(size_t i=0;i<skeleton->nodes.size();++i)nodes.emplace(skeleton->nodes[i].path,i);
    for(size_t slot=0;slot<part.bones.size();++slot){
      auto found=nodes.find(mesh.bones[slot]);
      if(found==nodes.end())continue;
      auto id=part.bones[slot];
      for(auto i=int32_t(found->second);i>=0&&!skeleton->nodes[size_t(i)].source;i=skeleton->nodes[size_t(i)].parent){
        const auto mapping=skeleton->retargets.find(size_t(i));
        if(mapping!=skeleton->retargets.end()&&verified.insert(id).second){
          const auto& target=skeleton->nodes[size_t(i)].path;
          Require(name(id)==target.substr(target.find_last_of('/')+1),"retarget target bone differs: "+target);
          auto ancestor=id;
          for(auto j=size_t(i);j!=mapping->second.ancestor;j=size_t(skeleton->nodes.at(j).parent))
            ancestor=pointer(ancestor,"m_Father");
          const auto& base=skeleton->nodes.at(mapping->second.ancestor).path;
          const auto& relative=mapping->second.sourceRelativePath;
          auto poseNode=ancestor;
          std::string posePath=base;
          size_t restMatches=1;
          std::istringstream segments(relative);
          for(std::string segment;std::getline(segments,segment,'/');){
            posePath=posePath.empty()?segment:posePath+"/"+segment;
            const auto& rest=skeleton->nodes.at(nodes.at(posePath));
            const auto& schema=type(poseNode);
            const auto& children=ConstArray(schema,Child(schema,value(poseNode),"m_Children")).children;
            int64_t poseMatch=0;
            restMatches=0;
            for(const auto& link:children){
              const auto candidate=local(schema,link);
              if(!candidate||!SameBindPose(rest,type(candidate),value(candidate)))continue;
              ++restMatches;
              poseMatch=candidate;
            }
            if(restMatches!=1)break;
            poseNode=poseMatch;
          }
          if(restMatches!=1){
            ++missing;
            std::cerr<<"RETARGET target="<<target<<" local bind pose candidates="<<restMatches<<std::endl;
          }else{
            auto slots=native.find(poseNode);
            Require(slots!=native.end(),"retarget fixture source absent from original Mesh palette: "+target);
            ++paletteBindings;
            auto anchor=id;
            for(size_t depth=0;depth<=size_t(std::count(target.begin(),target.end(),'/'));++depth)
              anchor=pointer(anchor,"m_Father");
            auto table=bindingTables.find({anchor,prefix});
            if(table==bindingTables.end()){
              std::string label;
              const auto& schema=type(anchor);
              for(const auto& child:ConstArray(schema,Child(schema,value(anchor),"m_Children")).children){
                auto candidate=local(schema,child);
                auto text=name(candidate);
                if(text.compare(0,prefix.size(),prefix)!=0)continue;
                Require(label.empty(),"same prefab skeleton has multiple binding tables");
                label=std::move(text);
                const auto owner=pointer(candidate,"m_GameObject");
                Require(!Integer(Child(type(owner),value(owner),"m_IsActive")),"retarget binding metadata is active");
              }
              Require(!label.empty(),"assembled prefab has no retarget binding table");
              table=bindingTables.emplace(std::make_pair(anchor,prefix),
                  EiemAuthorBoneAnimation::DecodeRetargetRoutes(std::string_view(label).substr(prefix.size()),*skeleton)).first;
            }
            auto route=table->second.find(size_t(i));
            Require(route!=table->second.end(),"assembled prefab retarget binding absent: "+target);
            auto resolved=anchor;
            for(auto step:route->second){
              const auto& schema=type(resolved);
              resolved=local(schema,ConstArray(schema,Child(schema,value(resolved),"m_Children")).children.at(size_t(step)));
            }
            Require(resolved==poseNode,"assembled retarget address differs from original palette reference: "+target);
            std::cout<<"RETARGET target="<<target.substr(target.find_last_of('/')+1)
                     <<" native="<<name(poseNode);
            for(const auto& [nativeMesh,nativeSlot]:slots->second)
              std::cout<<" slot="<<nativeMesh.first<<":"<<nativeMesh.second<<"/"<<nativeSlot;
            std::cout<<std::endl;
          }
        }
        id=pointer(id,"m_Father");
      }
    }
  }
  Require(!missing,"retarget rest bindings absent or ambiguous: "+std::to_string(missing)+" of "+std::to_string(verified.size()));
  return {verified.size(),paletteBindings};
}
}
int wmain(int argc,wchar_t** argv){try{
  Require(argc==3||argc==5,"story_prefab_read_smoke <game> <logical Bundle> [asset-index Mod]");
  const auto logical=std::filesystem::path(argv[2]).generic_u8string();
  const auto entries=EiemVfsCatalog::ReadEffectiveVfs(argv[1],{logical});
  const auto& entry=entries.at(logical);Require(!entry.encrypted,"fixture requires native unencrypted VFS input");
  auto file=std::make_shared<R::File>(entry.physical,entry.offset);
  R::Limits limits;limits.blockBytes=8*1024*1024;limits.metadataBytes=8*1024*1024;
  limits.fieldBytes=4*1024*1024;limits.cacheBytes=1024*1024;limits.readBytes=32*1024*1024;
  R::Reader reader(size_t(entry.size),[file](size_t at,unsigned char* out,size_t n){file->ReadAt(at,out,n);},limits);
  try{for(const auto& member:reader.directory.members){if(!(member.flags&4))continue;
    auto layout=reader.Layout(member);Package package;package.id="story-read-fixture";AssetIndex assets;
    for(auto id:layout.order){const auto& type=layout.types.at(layout.objects.at(id).type);
      if(type.classId!=137&&type.classId!=33)continue;
      auto value=reader.Project(member,layout,id,{"m_Mesh","m_GameObject"});
      auto mesh=NullableReference(type,Child(type,value,"m_Mesh"),layout,member.name);if(!mesh.second)continue;
      Identity identity{"fixture/"+std::to_string(id),"mesh"};const auto rule=std::to_string(id);
      bool duplicate=false;for(const auto& row:assets)if(CanonicalSerializedAssetReference({row.second.cab,row.second.pathId})==CanonicalSerializedAssetReference(mesh))duplicate=true;
      if(duplicate)continue;package.targetIndex[identity]=rule;package.rules[rule].skip=true;
      auto& asset=assets[identity];asset.cab=mesh.first;asset.pathId=mesh.second;asset.classId=43;
    }
    std::cout<<"FIXTURE source="<<logical<<" physical="<<entry.physical.u8string()<<" offset="<<entry.offset<<" inputBytes="<<entry.size
      <<" decodedBytes="<<reader.directory.decoded<<" cab="<<member.name<<" objects="<<layout.objects.size()<<" meshReferences="<<assets.size()<<" beforeReadBytes="<<reader.stats.encodedBytes<<std::endl;
    if(argc==5){
      namespace N=EiemDemandNativeAssembly;
      std::vector<std::shared_ptr<N::Author>> authors;std::filesystem::path modRoot=argv[4];
      if(std::filesystem::is_regular_file(modRoot/L"mod.ini"))authors.push_back(std::make_shared<N::Author>(std::make_shared<EiemDemandAuthorResources::Catalog>(modRoot)));
      else for(const auto& mod:std::filesystem::directory_iterator(modRoot))if(mod.is_directory()&&std::filesystem::is_regular_file(mod.path()/L"mod.ini"))
        authors.push_back(std::make_shared<N::Author>(std::make_shared<EiemDemandAuthorResources::Catalog>(mod.path())));
      auto composed=N::Author::Compose(authors);auto& author=*composed;
      auto index=EiemAuthorSources::ReadIndex(argv[3],EiemAuthorSources::Collect(author.package));
      std::set<NativeReference> wanted;for(const auto& row:assets)wanted.insert(CanonicalSerializedAssetReference({row.second.cab,row.second.pathId}));
      std::set<std::string> sourceNames;
      for(const auto& row:index.assets)if(row.kind=="Mesh"&&author.package.targetIndex.count({row.path,row.name}))sourceNames.insert(row.source);
      auto sources=EiemVfsCatalog::ReadEffectiveVfs(argv[1],sourceNames);AssetIndex targets;std::vector<BuiltMeshPart> parts;
      for(const auto& source:sources){auto carrierFile=std::make_shared<R::File>(source.second.physical,source.second.offset);
        R::Reader carrier(size_t(source.second.size),[carrierFile](size_t at,unsigned char* out,size_t n){carrierFile->ReadAt(at,out,n);},limits);
        for(size_t mi=0;mi<carrier.directory.members.size();++mi){const auto& m=carrier.directory.members[mi];if(!(m.flags&4))continue;
          // CAB filtering is fixture setup only. The actual assembly resolves
          // current serialized assets and uses the production sparse builder.
          bool used=false;for(const auto& ref:wanted)used|=ref.first==SerializedAssetCabKey(m.name);if(!used)continue;
          auto nativeLayout=carrier.Layout(m);AssetIndex selected;
          for(const auto& asset:carrier.Assets(m,nativeLayout))if(asset.classId==43&&author.package.targetIndex.count({asset.path,asset.asset})&&
              wanted.count(CanonicalSerializedAssetReference({asset.cab,asset.pathId})))selected.emplace(Identity{asset.path,asset.asset},asset);
          if(selected.empty())continue;targets.insert(selected.begin(),selected.end());
          auto prepared=N::PrepareCarrier(carrier,mi,nativeLayout,author,selected);
          parts.insert(parts.end(),prepared.parts.begin(),prepared.parts.end());
        }
      }
      Require(!targets.empty()&&!parts.empty(),"installed Mod has no exact target in this story fixture");
      auto native=NativePalette(reader,member,layout,parts);
      auto started=std::chrono::steady_clock::now();
      EiemNativeEditTransaction::Member transaction(reader,size_t(&member-reader.directory.members.data()),layout);
      auto matches=N::MatchConsumers(transaction,author,targets);
      N::EnsureStagedMeshMetadata(author,parts,matches);
      auto prepared=N::StageConsumer(transaction,author,targets,parts,nullptr,nullptr,&matches);
      prepared.plan=transaction.Commit();
      const auto prepareMs=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count();
      Require(prepared.result.matches,"installed Mod consumer assembly did not hit this story");
      auto output=EiemNativeSpliceView::BuildRanges(reader,{prepared.plan.rewrite},32*1024*1024);
      std::pair<size_t,size_t> retargets{};
      if(std::any_of(prepared.result.parts.begin(),prepared.result.parts.end(),[&](const auto& part){
           return !author.package.meshes.at(author.package.parts.at(part.part).mesh).skeleton.empty();
         })){
        auto view=output.Bind([file](size_t at,unsigned char* out,size_t n){file->ReadAt(at,out,n);});
        Bytes encoded(view->Size());
        Require(view->ReadAt(0,encoded.data(),encoded.size())==encoded.size(),"assembled test bundle truncated");
        auto bundle=EndfieldInput::Decode(encoded);
        retargets=VerifyRetargetBindings(bundle.members.at(size_t(&member-reader.directory.members.data())).bytes,
                                       member.name,author,prepared.result,native);
      }
      std::cout<<"PASS installed Mod story consumer mod="<<author.package.id<<" targets="<<targets.size()<<" parts="<<parts.size()
        <<" matches="<<prepared.result.matches<<" changedObjects="<<prepared.result.changedObjects<<" addedObjects="<<prepared.result.addedObjects
        <<" retargetBindings="<<retargets.first<<" paletteBindings="<<retargets.second
        <<" readBytes="<<reader.stats.encodedBytes<<" inflatedBytes="<<reader.stats.inflatedBytes<<" cachePeak="<<reader.stats.cachePeak
        <<" prepareMs="<<prepareMs<<std::endl;
      continue;
    }
    const std::vector<BuiltMeshPart> parts;
    RendererMemberBuilder builder(layout,member.name,package,assets,parts,
      [&](int64_t id,const std::set<std::string>& fields){return reader.Project(member,layout,id,fields);},
      [&](int64_t id){return reader.Object(member,layout,id);});
    builder.SetCost([&](const char* stage,double ms){if(std::string(stage)=="renderer-object-decode")return;
      std::cout<<"STAGE name="<<stage<<" ms="<<ms<<" readBytes="<<reader.stats.encodedBytes<<" inflatedBytes="<<reader.stats.inflatedBytes<<std::endl;});
    auto result=builder.Build();Require(result.matches,"story fixture has no native renderer consumers");
    std::cout<<"PASS story native renderer assembly matches="<<result.matches<<" changedObjects="<<result.changedObjects
      <<" readBytes="<<reader.stats.encodedBytes<<" inflatedBytes="<<reader.stats.inflatedBytes
      <<" cachePeak="<<reader.stats.cachePeak<<" pinnedBytes="<<reader.stats.pinnedBytes<<std::endl;
  }}catch(...){std::cerr<<"READ-STATS bytes="<<reader.stats.encodedBytes<<" reads="<<reader.stats.reads
    <<" inflatedBytes="<<reader.stats.inflatedBytes<<" inflatedBlocks="<<reader.stats.inflatedBlocks<<" cachePeak="<<reader.stats.cachePeak<<std::endl;throw;}
  file->VerifyStable();return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<std::endl;return 1;}}

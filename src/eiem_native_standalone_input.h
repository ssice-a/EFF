#pragma once
#include "eiem_native_edit_transaction.h"
#include "eiem_author_mesh_bundle.h"

// An independent native carrier, assembled from the CURRENT game's schema.
// Only the asset and its native AssetBundle directory survive. Dependency
// references are explicit; callers must schedule them before deserialization.
namespace EiemNativeStandaloneInput {
using namespace EiemNativeInput;
namespace R=EiemNativeRandomAccess;namespace S=EiemNativeSpliceView;namespace P=EiemNativeSerializedSplice;
struct Input {
  std::string cab,bundle,path;NativeReference asset;
  std::set<NativeReference> dependencies;
  std::shared_ptr<const EiemInputSpanView::View> view;
  size_t literalBytes=0,streamedBytes=0;
};
inline void RebaseReferences(const Type& type,Value& value,const SerializedLayout& source,const std::string& sourceCab,
    SerializedLayout& destination,const std::string& destinationCab,std::set<NativeReference>& references,size_t depth=0) {
  Require(depth<type.nodes.size(),"standalone native reference schema cycle");const auto& node=type.nodes.at(value.node);
  if(node.type.rfind("PPtr<",0)==0){
    auto file=Integer(Child(type,value,"m_FileID"));auto id=int64_t(Integer(Child(type,value,"m_PathID")));
    Require(file<=source.externals.size(),"standalone native reference external index differs");
    if(!id){SetReference(type,value,{destinationCab,0},destination,destinationCab);return;}
    NativeReference reference{file?source.externals[size_t(file)-1]:sourceCab,id};
    Require(!reference.first.empty(),"standalone native reference CAB absent");
    SetReference(type,value,reference,destination,destinationCab);references.insert(std::move(reference));return;
  }
  for(auto& child:value.children)RebaseReferences(type,child,source,sourceCab,destination,destinationCab,references,depth+1);
}
inline Input Build(R::Reader& reader,size_t memberIndex,const SerializedLayout& source,int64_t assetId,
    const SerializedLayout& references,P::RangePayload payload,const std::string& token,
    const std::set<NativeReference>& dependencies,R::ReadRange lease) {
  Require(bool(lease)&&token.size()==64&&token.find_first_not_of("0123456789abcdefABCDEF")==std::string::npos,
    "standalone native source lease/token absent");
  const auto& member=reader.directory.members.at(memberIndex);auto asset=source.objects.find(assetId);
  Require(asset!=source.objects.end(),"standalone native asset absent");
  auto canonical=token;std::transform(canonical.begin(),canonical.end(),canonical.begin(),[](unsigned char c){return char(std::tolower(c));});
  Input result;result.cab="CAB-"+canonical.substr(0,32);result.bundle="eff/"+canonical+".ab";
  result.path="assets/eff/"+canonical;result.asset={result.cab,assetId};result.dependencies=dependencies;
  Require(result.cab!=member.name,"standalone native carrier aliases its donor");
  int64_t bundleId=0;for(auto id:source.order)if(source.types.at(source.objects.at(id).type).classId==142){
    Require(!bundleId,"standalone native source has multiple AssetBundle directories");bundleId=id;}
  Require(bundleId&&bundleId!=assetId,"standalone native AssetBundle directory absent");
  auto output=source;output.externals=references.externals;Require(ExternalPrefix(source.externals,output.externals),"standalone native source external prefix differs");
  const auto& type=source.types.at(source.objects.at(bundleId).type);auto bundle=reader.Object(member,source,bundleId);
  SetString(type,Child(type,bundle,"m_Name"),result.bundle);SetString(type,Child(type,bundle,"m_AssetBundleName"),result.bundle);
  auto reset=[](Value& list){list.children.clear();list.raw.clear();list.rawOrigins.clear();list.count=0;};
  reset(ArrayValue(type,Child(type,bundle,"m_Dependencies")));
  auto& preloads=ArrayValue(type,Child(type,bundle,"m_PreloadTable"));reset(preloads);
  auto append=[&](const NativeReference& reference){auto value=ArrayElement(type,preloads);SetReference(type,value,reference,output,result.cab);preloads.children.push_back(std::move(value));};
  for(const auto& dependency:dependencies){Require(dependency.second&&!dependency.first.empty()&&dependency.first!=result.cab,"standalone native dependency invalid");append(dependency);}
  append(result.asset);preloads.count=preloads.children.size();
  auto info=[&](Value& value){SetInteger(type,Child(type,value,"preloadIndex"),0);SetInteger(type,Child(type,value,"preloadSize"),preloads.count);
    SetReference(type,Child(type,value,"asset"),result.asset,output,result.cab);};
  for(const char* name:{"m_Container","m_HashContainer"}){auto& list=ArrayValue(type,Child(type,bundle,name));reset(list);
    // String lookup is sufficient for the carrier. The game's private path
    // hash is never fabricated; consumers use CAB/PathID native references.
    if(!strcmp(name,"m_Container")){auto entry=ArrayElement(type,list);SetString(type,Child(type,entry,"first"),result.path);info(Child(type,entry,"second"));list.children.push_back(std::move(entry));list.count=1;}
  }
  auto& main=Child(type,bundle,"m_MainAsset");SetInteger(type,Child(type,main,"preloadIndex"),0);SetInteger(type,Child(type,main,"preloadSize"),0);SetReference(type,Child(type,main,"asset"),{result.cab,0},output,result.cab);
  auto bytes=std::make_shared<const Bytes>(EncodeValue(type,bundle));
  std::map<int64_t,P::RangePayload> edits;edits.emplace(assetId,std::move(payload));edits[bundleId].pieces.push_back({0,bytes->size(),bytes,{}});
  std::set<int64_t> removed;for(auto id:source.order)if(id!=assetId&&id!=bundleId)removed.insert(id);
  auto plan=P::BuildRanges(reader,memberIndex,output,edits,{},removed);
  std::vector<EiemNativeViewWriter::Block> blocks;for(size_t left=plan.size;left;){auto n=std::min(left,size_t(UINT32_MAX));blocks.push_back({n,n,0});left-=n;}
  auto prefix=std::make_shared<const Bytes>(EiemNativeViewWriter::Prefix(EiemAuthorMeshBundle::EnvelopeHeader(),blocks,{{result.cab,4,0,plan.size}}));
  auto backing=reader.UncompressedView(std::move(lease));std::vector<std::shared_ptr<const EiemInputSpanView::View>> inputs{backing};
  std::vector<EiemInputSpanView::Span> spans{{EiemInputSpanView::literalSource,0,prefix->size(),prefix}};result.literalBytes=prefix->size();
  for(const auto& piece:plan.rewrite.pieces){if(piece.bytes){spans.push_back({EiemInputSpanView::literalSource,0,piece.size,piece.bytes});result.literalBytes=AddSize(result.literalBytes,piece.size);}
    else{size_t sourceId=0;auto offset=AddSize(member.offset,piece.offset);
      if(piece.view){sourceId=inputs.size();inputs.push_back(piece.view);offset=piece.offset;}
      spans.push_back({sourceId,offset,piece.size,{}});result.streamedBytes=AddSize(result.streamedBytes,piece.size);}}
  std::vector<size_t> sizes;for(const auto& input:inputs)sizes.push_back(input->Size());
  result.view=std::make_shared<const EiemInputSpanView::View>(std::move(spans),sizes,
    [inputs=std::move(inputs)](size_t id,size_t at,unsigned char* out,size_t n){Require(inputs.at(id)->ReadAt(at,out,n)==n,"standalone native source read truncated");});
  return result;
}
} // namespace EiemNativeStandaloneInput

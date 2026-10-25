#pragma once
#include "eiem_compiled_mod.h"

// Author-side dependency metadata. The DLL only binds immutable byte ranges;
// it never evaluates these schemas, computes bounds or expands mesh frames.
namespace EiemAddressedResources {
using namespace EiemCompiledMod;
using Range=std::pair<size_t,size_t>;
struct Bindings {
 std::map<std::string,std::vector<Range>> ranges;
 std::map<std::string,Type> meshes;
 std::map<std::string,Bytes> generated;
};
inline constexpr const char* bindingStructure="EFF_RESOURCE_BINDINGS/stable-parts+explicit-fields+mesh-schemas/le64";
inline bool CurrentBindings(const Bytes& bytes){EiemOfflinePackage::Reader r{bytes};return r.S()==bindingStructure;}
inline void Normalize(std::vector<Range>& ranges){
 std::sort(ranges.begin(),ranges.end());std::vector<Range> result;
 for(const auto& range:ranges){if(!range.second)continue;auto end=AddSize(range.first,range.second);
  if(!result.empty()&&range.first<=AddSize(result.back().first,result.back().second))result.back().second=std::max(AddSize(result.back().first,result.back().second),end)-result.back().first;
  else result.push_back(range);
 }ranges=std::move(result);
}
inline void Capture(Bindings& result,const EndfieldInput::Bundle& edited){
 for(const auto& member:edited.members){EiemAuthorBytes::Validate(member.bytes,member.origins);
  for(const auto& origin:member.origins){
   if(origin.resource.rfind("@bounds:",0)==0){Require(origin.owned!=nullptr,"generated bounds lack ownership");auto inserted=result.generated.emplace(origin.resource,*origin.owned);Require(inserted.second||inserted.first->second==*origin.owned,"generated bounds disagree");}
   else if(origin.resource.rfind("@frame:",0)!=0)result.ranges[origin.resource].push_back({origin.offset,origin.size});
  }
 }
}
inline void PoolMetadata(EiemCompiledInputView::Recipe& view,const std::map<std::string,MemberProof>& proofs,const EndfieldInput::Bundle& edited,Bindings& bindings){
 Require(view.spans.size()>1&&view.spans[1].kind==EiemResourceRecipe::Literal,"native metadata literal absent");
 const auto& bytes=view.spans[1].literal;std::vector<Range> ranges;size_t base=0;
 for(const auto& member:edited.members)if(Serialized(member.bytes)){const auto& layout=proofs.at(member.name).after;base=AlignSize(base,16);
  Require(layout.countOffset>=48&&base<=bytes.size()&&layout.dataOffset<=bytes.size()-base,"pooled metadata extent differs");
  // Pool each native TypeTree independently. Different Prefabs have different
  // combinations/order and MonoScript slots; the schema payload is shared.
  size_t offset=layout.typeCountOffset+4;
  for(const auto& type:layout.types){Require(!type.nativeRecord.empty()&&type.nativeRecord.size()<=layout.countOffset-offset,"native TypeTree record absent");
   auto prefix=type.classId==114?size_t(39):size_t(23);Require(type.dependencyOffset>=prefix+4&&type.dependencyOffset<=type.nativeRecord.size(),"native TypeTree dependency extent differs");
   auto size=type.dependencyOffset-4-prefix;if(size>=1024)ranges.emplace_back(base+offset+prefix,size);offset=AddSize(offset,type.nativeRecord.size());}
  Require(offset==layout.countOffset,"native TypeTree table coverage differs");base=AddSize(base,layout.dataOffset);
 }
 struct Piece{size_t begin=0,end=0,index=0;};std::vector<Piece> pieces;std::vector<EiemResourceRecipe::Span> output;output.push_back(std::move(view.spans[0]));size_t cursor=0;
 auto literal=[&](size_t begin,size_t end){if(begin==end)return;pieces.push_back({begin,end,output.size()});output.push_back({EiemResourceRecipe::Literal,{},0,end-begin,Bytes(bytes.begin()+begin,bytes.begin()+end)});};
 for(const auto& range:ranges){auto begin=range.first,end=AddSize(begin,range.second);bool names=false;for(const auto& site:view.names)if(site.span==1&&site.offset<end&&site.offset+site.before.size()>begin){names=true;break;}if(names)continue;
  literal(cursor,begin);Bytes pooled(bytes.begin()+begin,bytes.begin()+end);auto name="@literal:"+Hash(pooled);bindings.generated.emplace(name,std::move(pooled));pieces.push_back({begin,end,output.size()});output.push_back({EiemResourceRecipe::ExpandedFrame,name,0,range.second,{}});cursor=end;
 }literal(cursor,bytes.size());auto shift=output.size()-2;
 for(auto& site:view.names){if(site.span==1){auto piece=std::find_if(pieces.begin(),pieces.end(),[&](const auto& value){return site.offset>=value.begin&&site.offset+site.before.size()<=value.end;});Require(piece!=pieces.end()&&output[piece->index].kind==EiemResourceRecipe::Literal,"pooled metadata namespace ownership differs");site.span=piece->index;site.offset-=piece->begin;}else if(site.span>1)site.span+=shift;}
 for(size_t i=2;i<view.spans.size();++i)output.push_back(std::move(view.spans[i]));view.spans=std::move(output);
}
inline Bytes Encode(const Bindings& bindings){
 Writer w;w.S(bindingStructure);w.N(bindings.ranges.size());
 for(const auto& row:bindings.ranges){w.S(row.first);auto ranges=row.second;Normalize(ranges);w.N(ranges.size());for(const auto& range:ranges){w.N(range.first);w.N(range.second);}}
 w.N(bindings.meshes.size());SchemaWriter schemas;
 for(const auto& row:bindings.meshes){w.S(row.first);SerializedLayout layout;layout.metadataSize=4;layout.dataOffset=52;layout.countOffset=48;layout.tableStart=layout.tableEnd=52;layout.types={row.second};WriteLayout(w,layout,schemas);}
 return std::move(w.b);
}
inline Bindings Decode(const Bytes& bytes,const Package& author){
 EiemOfflinePackage::Reader r{bytes};Require(r.S()==bindingStructure,"resource binding structure differs");Bindings result;
 auto count=r.Count();for(size_t i=0;i<count;++i){auto name=r.S();Require(author.resources.count(name)&&!result.ranges.count(name),"resource binding identity absent/repeated");auto& ranges=result.ranges[name];auto n=r.Count();size_t end=0;
  for(size_t j=0;j<n;++j){auto offset=r.Size(),size=r.Size();Require(size&&offset>=end&&offset<=author.resources.at(name)->bytes.size()&&size<=author.resources.at(name)->bytes.size()-offset,"resource binding extent differs");ranges.emplace_back(offset,size);end=AddSize(offset,size);}}
 count=r.Count();SchemaReader schemas;for(size_t i=0;i<count;++i){auto name=r.S();auto layout=ReadLayout(r,schemas);Require(author.resources.count(name)&&layout.types.size()==1&&layout.types.front().classId==43&&layout.types.front().hash==author.resources.at(name)->type&&result.meshes.emplace(name,std::move(layout.types.front())).second,"resource binding Mesh schema differs");}
 Require(r.p==bytes.size(),"resource binding suffix differs");return result;
}
inline void Finalize(Artifact& artifact,std::vector<Bindings>& inputs){
 Bindings result;for(auto& input:inputs){for(auto& row:input.ranges){auto& ranges=result.ranges[row.first];ranges.insert(ranges.end(),row.second.begin(),row.second.end());}
  for(auto& row:input.generated){auto found=artifact.preparedFrames.emplace(row.first,row.second);Require(found.second||found.first->second==row.second,"compiled generated resource disagrees");}}
 const auto& package=artifact.manifest.package;
 for(const auto& frame:artifact.manifest.plan.frameRecipes)result.ranges[frame.resource].emplace_back(frame.offset,frame.size);
 for(const auto& resource:package.resources)if(resource.second->kind==1)for(const auto& member:artifact.members)for(const auto& type:member.second.before.types)if(type.classId==43&&type.hash==resource.second->type){result.meshes.emplace(resource.first,type);break;}
 for(auto& row:result.ranges)Normalize(row.second);
 artifact.resourceBindingBytes=Encode(result);artifact.explicitResources=true;artifact.manifest.plan.resourceAddressed=true;
}
inline bool RecoverCompiledBindings(Artifact& artifact){
 if(artifact.explicitResources&&!artifact.resourceBindingBytes.empty()&&CurrentBindings(artifact.resourceBindingBytes))return true;
 const auto& plan=artifact.manifest.plan;const auto& package=artifact.manifest.package;
 if(!plan.rulesOnly||!plan.spanRecipes||artifact.views.size()!=plan.inputs.size()||plan.memberRecipes.size()!=plan.inputs.size())return false;
 Bindings recovered;
 auto collect=[&](const auto& spans){for(const auto& span:spans)if(span.kind==EiemResourceRecipe::Author){
  auto resource=package.resources.find(span.source);if(resource==package.resources.end()||span.offset>resource->second->bytes.size()||span.size>resource->second->bytes.size()-span.offset)return false;
  if(span.size)recovered.ranges[span.source].emplace_back(span.offset,span.size);
 }return true;};
 for(const auto& view:artifact.views)if(!collect(view.spans))return false;
 for(const auto& input:plan.memberRecipes)for(const auto& member:input)if(!collect(member.spans))return false;
 for(const auto& frame:plan.frameRecipes){auto resource=package.resources.find(frame.resource);if(resource==package.resources.end()||frame.offset>resource->second->bytes.size()||frame.size>resource->second->bytes.size()-frame.offset)return false;recovered.ranges[frame.resource].emplace_back(frame.offset,frame.size);}
 std::set<std::string> neededMeshes;for(const auto& row:package.meshes)neededMeshes.insert(row.second.resource);
 if(!neededMeshes.empty())for(size_t i=0;i<plan.inputs.size()&&!neededMeshes.empty();++i){
  HydrateInput(artifact,i);
  for(const auto& recipe:plan.memberRecipes[i]){auto proof=artifact.members.find(recipe.name);if(proof==artifact.members.end())continue;
   for(const auto& type:proof->second.before.types)if(type.classId==43){
    for(auto mesh=neededMeshes.begin();mesh!=neededMeshes.end();){const auto& resource=*package.resources.at(*mesh);
    if(type.hash==resource.type){auto inserted=recovered.meshes.emplace(*mesh,type);Require(inserted.second||EncodeSchema(inserted.first->second)==EncodeSchema(type),"legacy Mesh schema differs across source proofs");mesh=neededMeshes.erase(mesh);}else ++mesh;
    }
   }
  }
 }
 if(!neededMeshes.empty())return false;
 for(auto& row:recovered.ranges)Normalize(row.second);
 auto encoded=Encode(recovered);Decode(encoded,package);
 artifact.resourceBindingBytes=std::move(encoded);artifact.explicitResources=true;artifact.manifest.plan.resourceAddressed=true;
 return true;
}
inline bool SameLayout(const Resource& a,const Resource& b){
 if(a.kind!=b.kind||a.type!=b.type||a.bytes.size()!=b.bytes.size()||a.blocks.size()!=b.blocks.size())return false;
 for(const auto& row:a.blocks){auto found=b.blocks.find(row.first);if(found==b.blocks.end()||row.second.offset!=found->second.offset||row.second.size!=found->second.size)return false;}return true;
}
inline bool CoveredChanges(const Resource& previous,const Resource& next,const std::vector<Range>& ranges){
 size_t at=0;for(const auto& block:next.blocks){auto begin=block.second.offset,end=AddSize(begin,block.second.size);
  // Map iteration is by field name, so each block starts its own ordered search.
  at=size_t(std::lower_bound(ranges.begin(),ranges.end(),begin,[](const Range& range,size_t value){return range.first+range.second<=value;})-ranges.begin());
  for(size_t p=begin;p<end;){while(at<ranges.size()&&ranges[at].first+ranges[at].second<=p)++at;
   if(at<ranges.size()&&ranges[at].first<=p){p=std::min(end,ranges[at].first+ranges[at].second);continue;}
   auto stop=at<ranges.size()?std::min(end,ranges[at].first):end;
   if(!std::equal(previous.bytes.begin()+p,previous.bytes.begin()+stop,next.bytes.begin()+p))return false;p=stop;
  }
 }return true;
}
inline bool SameDeclarations(const Package& next,const Package& previous){
 if(next.meshes.size()!=previous.meshes.size())return false;auto compare=next,oldCore=previous;
 for(auto i=compare.resources.begin();i!=compare.resources.end();)if(i->second->kind==10)i=compare.resources.erase(i);else ++i;
 for(auto i=oldCore.resources.begin();i!=oldCore.resources.end();)if(i->second->kind==10)i=oldCore.resources.erase(i);else ++i;
 if(compare.resources.size()!=oldCore.resources.size())return false;
 for(auto& row:compare.resources){auto old=oldCore.resources.find(row.first);if(old==oldCore.resources.end())return false;row.second=old->second;}
 if(InputContractDigest(compare)!=InputContractDigest(oldCore))return false;
 for(const auto& row:next.meshes){auto old=previous.meshes.find(row.first);if(old==previous.meshes.end())return false;const auto& a=row.second;const auto& b=old->second;
  if(a.resource!=b.resource||a.vertices!=b.vertices||a.indices!=b.indices||a.bones!=b.bones||a.shapes!=b.shapes)return false;}
 return true;
}
struct Update {bool compatible=false;std::set<std::string> resources,generated;};
inline Bytes RebindPrepared(const Bytes& packed,const Artifact& before,const Artifact& after){
 auto bytes=Unpack(packed);EiemOfflinePackage::Reader r{bytes};auto structure=r.S();
 Require(structure=="EFF_PREPARED_PFB/author+core-inputs+assets+schemas+palettes+native-bundle-routes/le64","prepared resource template structure differs");
 Require(r.S()==before.manifest.plan.author&&r.S()==before.manifest.plan.contract,"prepared resource template seal differs");
 r.verifiedAuthor=&before.manifest.package;auto resolved=ReadAuthor(r,before.manifest.package.root.parent_path(),true,before.manifest.package.root);
 auto next=after.manifest.package;for(auto& row:next.textures){auto old=resolved.textures.find(row.first);Require(old!=resolved.textures.end(),"prepared Texture identity differs");row.second.source=old->second.source;}
 Writer w;w.S(structure);w.S(after.manifest.plan.author);w.S(after.manifest.plan.contract);WriteAuthor(w,next,true);w.Raw(bytes.data()+r.p,bytes.size()-r.p);return Pack(w.b);
}
inline Update Rebind(Artifact& artifact,const Package& author,const std::function<void(const char*,double)>& cost={}){
 Update update;const auto& old=artifact.manifest.package;
 if(!artifact.explicitResources||artifact.resourceBindingBytes.empty()||!CurrentBindings(artifact.resourceBindingBytes)||!SameDeclarations(author,old))return update;
 auto bindings=Decode(artifact.resourceBindingBytes,old);
 for(const auto& row:author.resources){auto prior=old.resources.find(row.first);if(prior==old.resources.end()){if(row.second->kind!=10)return update;update.resources.insert(row.first);continue;}const auto& previous=*prior->second;const auto& next=*row.second;
  if(ResourceContentHash(previous)==ResourceContentHash(next))continue;
  if(next.kind!=10){if(!SameLayout(previous,next))return update;auto found=bindings.ranges.find(row.first);const std::vector<Range> empty;
   if(!CoveredChanges(previous,next,found==bindings.ranges.end()?empty:found->second))return update;}
  update.resources.insert(row.first);
 }
 // Validate and regenerate each changed Mesh once, independently of its PFB count.
 for(const auto& row:author.meshes)if(update.resources.count(row.second.resource)){
  auto schema=bindings.meshes.find(row.second.resource);if(schema==bindings.meshes.end())return Update{};
  const auto& type=schema->second;auto mesh=EmptyValue(type,0);const auto& resource=*author.resources.at(row.second.resource);
  for(const auto& block:resource.blocks)if(block.first[0]!='@'){auto& field=Child(type,mesh,block.first);Bytes bytes(resource.bytes.begin()+block.second.offset,resource.bytes.begin()+block.second.offset+block.second.size);field=DecodeValue(type,field.node,bytes);}
  const auto& vertex=Child(type,mesh,"m_VertexData");DescribeMeshVertexStorage(type,vertex);
  auto format=Integer(Child(type,mesh,"m_IndexFormat"));Require(format<=1,"updated Mesh index format differs");const auto& indices=ArrayValue(type,Child(type,mesh,"m_IndexBuffer")).raw;size_t width=format?4:2;
  Require(indices.size()==MultiplySize(row.second.indices,width),"updated Mesh index count differs");for(size_t p=0;p<indices.size();p+=width)Require(Le(indices,p,width)<row.second.vertices,"updated Mesh index exceeds vertices");
 }
 auto frames=artifact.preparedFrames;auto recipes=artifact.manifest.plan.frameRecipes;
 for(auto& frame:recipes)if(update.resources.count(frame.resource)){frame.hash.clear();auto bytes=EiemResourceRecipe::ExpandFrame(frame,author.resources.at(frame.resource)->bytes);frame.hash=Hash(bytes);frames[frame.resource]=std::move(bytes);update.generated.insert(frame.resource);}
 for(auto& row:frames){size_t root=0;AffineMatrix matrix;std::string resource;if(!DecodeBoundsOrigin(row.first,root,matrix,resource)||!update.resources.count(resource))continue;
  auto mesh=std::find_if(author.meshes.begin(),author.meshes.end(),[&](const auto& value){return value.second.resource==resource;});Require(mesh!=author.meshes.end()&&bindings.meshes.count(resource),"generated bounds Mesh absent");
  const auto& type=bindings.meshes.at(resource);auto bounds=PartBounds(type,EmptyValue(type,0),*author.resources.at(resource),mesh->second,root,matrix);Bytes bytes;
  for(const auto& vector:{bounds.first,bounds.second})for(auto value:vector){float number=float(value);auto p=bytes.size();bytes.resize(p+4);memcpy(bytes.data()+p,&number,4);}row.second=std::move(bytes);update.generated.insert(row.first);
 }
 artifact.preparedFrames=std::move(frames);artifact.manifest.plan.frameRecipes=std::move(recipes);
 artifact.manifest.package=author;artifact.manifest.plan.author=PackageDigest(author);artifact.manifest.plan.contract=InputContractDigest(author);artifact.manifest.plan.resourceAddressed=true;
 update.compatible=true;if(cost){cost("resource-rebind-resources",double(update.resources.size()));cost("resource-rebind-generated",double(update.generated.size()));}return update;
}
} // namespace EiemAddressedResources

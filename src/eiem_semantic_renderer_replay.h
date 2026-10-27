#pragma once
#include "eiem_compiled_mod.h"
#include "eiem_compiled_object_merge.h"
#include "eiem_prefab_consumer_index.h"
#include "eiem_static_resource_sources.h"

// Versioned field-path recipes used only when a known logical source changed
// after export. The ordinary source-span view remains the fast path.
namespace EiemSemanticRendererReplay {
using namespace EiemCompiledMod;
using EiemResourceReloadPlan::SemanticOperation;
using EiemResourceReloadPlan::RendererRule;
struct Stats {size_t targets=0,changed=0,missing=0,incompatible=0;};
inline Bytes ObjectBytes(const Bytes& bytes,const Object& object){
 Require(object.offset<=bytes.size()&&object.size<=bytes.size()-object.offset,"semantic object extent differs");
 return Bytes(bytes.begin()+object.offset,bytes.begin()+object.offset+object.size);
}
inline bool Aggregate(const Type& type,size_t node){const auto& value=type.nodes.at(node);return !Primitive(value.type)&&value.type!="Array"&&value.type!="string"&&value.type!="TypelessData"&&!value.children.empty();}
inline size_t FieldIndex(const Type& type,const Value& value,const std::string& name);
inline bool EqualSemantic(const Type& leftType,const Value& left,const Type& rightType,const Value& right){
 const auto& a=leftType.nodes.at(left.node);const auto& b=rightType.nodes.at(right.node);if(a.type!=b.type)return false;
 if(a.type=="Array")return left.count==right.count&&left.raw==right.raw&&left.children.size()==right.children.size()&&
  std::equal(left.children.begin(),left.children.end(),right.children.begin(),[&](const auto& x,const auto& y){return EqualSemantic(leftType,x,rightType,y);});
 if(Aggregate(leftType,left.node)&&Aggregate(rightType,right.node)){
  if(left.children.size()!=right.children.size())return false;for(const auto& child:left.children){auto li=FieldIndex(leftType,left,leftType.nodes.at(child.node).name),ri=FieldIndex(rightType,right,rightType.nodes.at(child.node).name);if(li==SIZE_MAX||ri==SIZE_MAX||!EqualSemantic(leftType,left.children[li],rightType,right.children[ri]))return false;}return true;
 }
 return left.raw==right.raw&&left.count==right.count;
}
inline size_t FieldIndex(const Type& type,const Value& value,const std::string& name){
 size_t found=SIZE_MAX;for(size_t i=0;i<value.children.size();++i)if(type.nodes.at(value.children[i].node).name==name){if(found!=SIZE_MAX)return SIZE_MAX;found=i;}return found;
}
inline bool ContainsPointer(const Type& type,size_t node){
 const auto& value=type.nodes.at(node);if(value.type.rfind("PPtr<",0)==0)return true;
 for(auto child:value.children)if(ContainsPointer(type,child))return true;return false;
}
inline bool HasChangedPointer(const Type& beforeType,const Value& before,const Type& editedType,const Value& edited){
 if(EqualSemantic(beforeType,before,editedType,edited))return false;
 const auto& beforeNode=beforeType.nodes.at(before.node);const auto& editedNode=editedType.nodes.at(edited.node);
 if(beforeNode.type.rfind("PPtr<",0)==0||editedNode.type.rfind("PPtr<",0)==0)return true;
 if(beforeNode.type=="Array"||editedNode.type=="Array"){
  if(!ContainsPointer(beforeType,before.node)&&!ContainsPointer(editedType,edited.node))return false;
  if(beforeNode.type!="Array"||editedNode.type!="Array"||before.count!=edited.count||before.children.size()!=edited.children.size())return true;
  for(size_t i=0;i<before.children.size();++i)if(HasChangedPointer(beforeType,before.children[i],editedType,edited.children[i]))return true;return false;
 }
 if(!Aggregate(beforeType,before.node)||!Aggregate(editedType,edited.node))return false;
 for(const auto& oldChild:before.children){const auto& name=beforeType.nodes.at(oldChild.node).name;auto bi=FieldIndex(beforeType,before,name),ei=FieldIndex(editedType,edited,name);
  if(ei==SIZE_MAX){if(ContainsPointer(beforeType,oldChild.node))return true;continue;}
  if(bi==SIZE_MAX||HasChangedPointer(beforeType,oldChild,editedType,edited.children[ei]))return true;
 }
 for(const auto& newChild:edited.children){const auto& name=editedType.nodes.at(newChild.node).name;if(FieldIndex(beforeType,before,name)==SIZE_MAX&&ContainsPointer(editedType,newChild.node))return true;}
 return false;
}
inline bool HasChangedNamedArray(const Type& beforeType,const Value& before,const Type& editedType,const Value& edited,const std::string& name){
 if(EqualSemantic(beforeType,before,editedType,edited))return false;
 const auto& beforeNode=beforeType.nodes.at(before.node);const auto& editedNode=editedType.nodes.at(edited.node);
 if(beforeNode.name==name||editedNode.name==name)return beforeNode.type=="Array"||editedNode.type=="Array";
 if(!Aggregate(beforeType,before.node)||!Aggregate(editedType,edited.node))return false;
 for(const auto& child:before.children){const auto& field=beforeType.nodes.at(child.node).name;auto ei=FieldIndex(editedType,edited,field);if(ei!=SIZE_MAX&&HasChangedNamedArray(beforeType,child,editedType,edited.children[ei],name))return true;}
 return false;
}
inline std::string CabKey(std::string value){std::transform(value.begin(),value.end(),value.begin(),[](unsigned char c){return char(std::tolower(c));});return value;}
inline Value DefaultValue(const Type& type,size_t node){
 Value value;value.node=node;const auto& schema=type.nodes.at(node);auto width=Primitive(schema.type);
 if(width){value.raw.resize(width);return value;}if(schema.type=="string"||schema.type=="TypelessData")return value;
 if(schema.type=="Array"){value.count=0;return value;}for(auto child:schema.children)value.children.push_back(DefaultValue(type,child));return value;
}
inline void CopyPointer(const Type& sourceType,const Value& source,const Type& targetType,Value& target,
                        const std::vector<std::string>& sourceExternals,std::vector<std::string>& targetExternals){
 auto sf=FieldIndex(sourceType,source,"m_FileID"),sp=FieldIndex(sourceType,source,"m_PathID");
 auto tf=FieldIndex(targetType,target,"m_FileID"),tp=FieldIndex(targetType,target,"m_PathID");
 Require(sf!=SIZE_MAX&&sp!=SIZE_MAX&&tf!=SIZE_MAX&&tp!=SIZE_MAX,"semantic PPtr fields absent");
 target.children[tp].raw=source.children[sp].raw;target.children[tp].padding=source.children[sp].padding;auto file=Integer(source.children[sf]);
 Require(file<=sourceExternals.size(),"semantic PPtr source FileID is out of range");
 uint64_t mapped=0;if(file){auto key=CabKey(sourceExternals[size_t(file-1)]);size_t index=0;for(;index<targetExternals.size();++index)if(CabKey(targetExternals[index])==key)break;
  if(index==targetExternals.size())targetExternals.push_back(sourceExternals[size_t(file-1)]);mapped=index+1;}
 SetInteger(targetType,target.children[tf],mapped);
}
inline Value CopyValue(const Type& sourceType,const Value& source,const Type& targetType,const Value& target,
                       const std::vector<std::string>& sourceExternals,std::vector<std::string>& targetExternals,
                       Stats& stats,const std::string& path,bool allowArrayResize){
 const auto& from=sourceType.nodes.at(source.node);const auto& to=targetType.nodes.at(target.node);
 if(from.type.rfind("PPtr<",0)==0&&to.type.rfind("PPtr<",0)==0){auto result=target;CopyPointer(sourceType,source,targetType,result,sourceExternals,targetExternals);++stats.changed;return result;}
 if(Aggregate(sourceType,source.node)&&Aggregate(targetType,target.node)){
  auto result=target;for(const auto& oldChild:source.children){auto si=FieldIndex(sourceType,source,sourceType.nodes.at(oldChild.node).name);auto ti=FieldIndex(targetType,target,sourceType.nodes.at(oldChild.node).name);
   const auto name=sourceType.nodes.at(oldChild.node).name;const auto childPath=path.empty()?name:path+"."+name;
   if(si==SIZE_MAX||ti==SIZE_MAX){++stats.missing;continue;}result.children[ti]=CopyValue(sourceType,oldChild,targetType,target.children[ti],sourceExternals,targetExternals,stats,childPath,allowArrayResize);}
  return result;
 }
 if(from.type=="Array"&&to.type=="Array"&&from.children.size()==2&&to.children.size()==2){
  if(source.count!=target.count&&!allowArrayResize){++stats.incompatible;return target;}
  auto result=target;result.children.clear();result.raw.clear();result.count=source.count;const auto sourceElement=from.children[1],targetElement=to.children[1];
  if(sourceType.nodes.at(sourceElement).type!=targetType.nodes.at(targetElement).type){++stats.incompatible;return target;}
  auto step=Primitive(targetType.nodes.at(targetElement).type);if(step&&!targetType.nodes.at(targetElement).align){
   Require(source.children.empty()&&source.count<=SIZE_MAX/step&&source.raw.size()==source.count*step,"semantic packed array source extent differs");result.raw=source.raw;result.children.clear();
  }else{
   Require(source.raw.empty()&&source.count==source.children.size(),"semantic array source count differs");result.raw.clear();result.children.reserve(source.children.size());
   for(const auto& element:source.children){auto empty=DefaultValue(targetType,targetElement);result.children.push_back(CopyValue(sourceType,element,targetType,empty,sourceExternals,targetExternals,stats,path+"[]",allowArrayResize));}
  }
  ++stats.changed;return result;
 }
 if(from.type!=to.type||!source.children.empty()||(!Primitive(from.type)&&from.type!="string"&&from.type!="TypelessData")){
  ++stats.incompatible;return target;
 }
 if(Primitive(from.type)&&source.raw.size()!=target.raw.size()){++stats.incompatible;return target;}
 auto result=target;result.raw=source.raw;result.padding=source.padding;++stats.changed;return result;
}
inline Value Overlay(const Type& beforeType,const Value& before,const Type& editedType,const Value& edited,
                     const Type& currentType,const Value& current,const std::vector<std::string>& editedExternals,
                     std::vector<std::string>& currentExternals,Stats& stats,const std::string& prefix={},bool allowArrayResize=false){
 if(EqualSemantic(beforeType,before,editedType,edited))return current;
 if(Aggregate(beforeType,before.node)&&Aggregate(editedType,edited.node)&&Aggregate(currentType,current.node)){
  auto result=current;for(const auto& oldChild:before.children){const auto& name=beforeType.nodes.at(oldChild.node).name;auto bi=FieldIndex(beforeType,before,name),ei=FieldIndex(editedType,edited,name),ci=FieldIndex(currentType,current,name);
   auto path=prefix.empty()?name:prefix+"."+name;if(ei==SIZE_MAX){++stats.incompatible;continue;}if(ci==SIZE_MAX){if(!EqualSemantic(beforeType,oldChild,editedType,edited.children[ei]))++stats.missing;continue;}
   if(bi==SIZE_MAX){++stats.incompatible;continue;}result.children[ci]=Overlay(beforeType,oldChild,editedType,edited.children[ei],currentType,current.children[ci],editedExternals,currentExternals,stats,path,allowArrayResize);}
  return result;
 }
 return CopyValue(editedType,edited,currentType,current,editedExternals,currentExternals,stats,prefix,allowArrayResize);
}
inline std::string LogicalForSource(const EiemStaticSources::Selection& selection,const std::string& file){
 for(const auto& row:selection.inputs)if(EiemStaticSources::SourceFile(row.first,row.second.revision)==file)return EiemStaticSources::Logical(row.first);
 throw std::runtime_error("semantic export source has no logical route: "+file);
}
inline std::map<std::pair<std::string,std::string>,Asset> Assets(const EndfieldInput::Bundle& bundle){
 auto named=NamedSerializedAssets(bundle);std::map<std::pair<std::string,std::string>,Asset> result;
 for(const auto& member:bundle.members)if(SerializedHeader(member.bytes)){auto layout=ParseSerialized(member.bytes);
  for(auto asset:SerializedAssets(member.bytes,member.name,layout,&named)){if(asset.classId!=43||asset.asset.empty())continue;
   auto key=std::make_pair(EiemStaticSources::Logical(asset.path),asset.asset);auto pathId=asset.pathId;auto found=result.emplace(key,std::move(asset));Require(found.second||found.first->second.pathId==pathId,"semantic Mesh identity is ambiguous in source");}}
 return result;
}
inline void AddOperation(std::vector<SemanticOperation>& out,unsigned kind,const Identity& mesh,const std::string& meshBundle,
                         const std::string& cab,int64_t id,const Bytes& beforeBytes,const Type& beforeType,
                         const Bytes& editedBytes,const Type& editedType,const std::vector<std::string>& editedExternals){
 if(beforeBytes==editedBytes)return;Require(beforeType.classId==editedType.classId,"semantic object class changed");
 SemanticOperation operation;operation.kind=kind;operation.mesh=mesh;operation.meshBundle=meshBundle;operation.cab=cab;operation.id=id;
 operation.beforeSchema=EncodeSchema(beforeType);operation.editedSchema=EncodeSchema(editedType);operation.before=beforeBytes;operation.edited=editedBytes;operation.editedExternals=editedExternals;out.push_back(std::move(operation));
}
inline bool Capture(const EndfieldInput::Bundle& original,const EndfieldInput::Bundle& edited,const std::string& sourceLogical,
                    const std::vector<RendererRule>& rules,const std::map<Identity,std::string>& targetBundles,
                    std::vector<SemanticOperation>& output){
 output.clear();std::map<std::string,const EndfieldInput::Member*> beforeMembers,editedMembers;
 for(const auto& row:original.members)beforeMembers.emplace(row.name,&row);for(const auto& row:edited.members)editedMembers.emplace(row.name,&row);
 auto oldAssets=Assets(original),newAssets=Assets(edited);
 auto addAsset=[&](const Identity& identity,const std::string& bundle){if(EiemStaticSources::Logical(bundle)!=sourceLogical)return;
  auto old=oldAssets.find({EiemStaticSources::Logical(identity.path),identity.asset});auto next=newAssets.find({EiemStaticSources::Logical(identity.path),identity.asset});
  if(old==oldAssets.end()||next==newAssets.end())return;auto oldCab=EiemStaticSources::Cab(old->second.cab),newCab=EiemStaticSources::Cab(next->second.cab);
  const EndfieldInput::Member* oldMember=nullptr;const EndfieldInput::Member* newMember=nullptr;for(const auto& row:original.members)if(EiemStaticSources::Cab(row.name)==oldCab)oldMember=&row;for(const auto& row:edited.members)if(EiemStaticSources::Cab(row.name)==newCab)newMember=&row;
  if(!oldMember||!newMember||!SerializedHeader(oldMember->bytes)||!SerializedHeader(newMember->bytes))return;
  auto a=ParseSerialized(oldMember->bytes),b=ParseSerialized(newMember->bytes);auto oi=a.objects.find(old->second.pathId),ni=b.objects.find(next->second.pathId);if(oi==a.objects.end()||ni==b.objects.end())return;
  AddOperation(output,SemanticOperation::MeshAsset,identity,bundle,oldMember->name,oi->first,ObjectBytes(oldMember->bytes,oi->second),a.types.at(oi->second.type),ObjectBytes(newMember->bytes,ni->second),b.types.at(ni->second.type),b.externals);
 };
 for(const auto& row:targetBundles)addAsset(row.first,row.second);
 for(const auto& rule:rules){auto old=beforeMembers.find(rule.cab),next=editedMembers.find(rule.cab);if(old==beforeMembers.end()||next==editedMembers.end()||!SerializedHeader(old->second->bytes)||!SerializedHeader(next->second->bytes))continue;
  auto a=ParseSerialized(old->second->bytes),b=ParseSerialized(next->second->bytes);auto oi=a.objects.find(rule.id),ni=b.objects.find(rule.id);if(oi==a.objects.end()||ni==b.objects.end())continue;
  AddOperation(output,SemanticOperation::Renderer,rule.mesh,rule.meshBundle,rule.cab,rule.id,ObjectBytes(old->second->bytes,oi->second),a.types.at(oi->second.type),ObjectBytes(next->second->bytes,ni->second),b.types.at(ni->second.type),b.externals);
 }
 std::set<std::pair<std::string,int64_t>> supported;for(const auto& operation:output)supported.emplace(EiemStaticSources::Cab(operation.cab),operation.id);
 bool complete=true;for(const auto& operation:output){EiemOfflinePackage::Reader beforeReader{operation.beforeSchema},editedReader{operation.editedSchema};auto beforeType=DecodeSchema(beforeReader),editedType=DecodeSchema(editedReader);
  auto beforeValue=DecodeValue(beforeType,0,operation.before),editedValue=DecodeValue(editedType,0,operation.edited);
  if(HasChangedPointer(beforeType,beforeValue,editedType,editedValue)||HasChangedNamedArray(beforeType,beforeValue,editedType,editedValue,"m_BlendShapeWeights"))complete=false;}
 for(const auto& row:original.members){auto next=editedMembers.find(row.name);if(next==editedMembers.end()){complete=false;continue;}if(!SerializedHeader(row.bytes)||!SerializedHeader(next->second->bytes)){if(row.bytes!=next->second->bytes)complete=false;continue;}
  auto a=ParseSerialized(row.bytes),b=ParseSerialized(next->second->bytes);if(a.objects.size()!=b.objects.size())complete=false;
  for(const auto& object:a.objects){auto changed=b.objects.find(object.first);if(changed==b.objects.end()){complete=false;continue;}if(ObjectBytes(row.bytes,object.second)!=ObjectBytes(next->second->bytes,changed->second)&&!supported.count({EiemStaticSources::Cab(row.name),object.first}))complete=false;}
 }
 for(const auto& row:edited.members)if(!beforeMembers.count(row.name))complete=false;return complete;
}
inline NativeReference ResolveMesh(const EndfieldInput::Bundle& bundle,const Identity& identity){
 auto assets=Assets(bundle);auto found=assets.find({EiemStaticSources::Logical(identity.path),identity.asset});Require(found!=assets.end(),"current logical Mesh is absent: "+identity.path+"#"+identity.asset);
 Require(found->second.classId==43,"current logical target is not a Mesh");return {EiemStaticSources::Cab(found->second.cab),found->second.pathId};
}
inline void Apply(EndfieldInput::Bundle& current,const std::string& currentLogical,const std::vector<SemanticOperation>& operations,
                  const std::function<NativeReference(const std::string&,const Identity&)>& resolveMesh,Stats& stats){
 if(operations.empty())return;std::map<std::string,const EndfieldInput::Member*> members;for(const auto& member:current.members)members.emplace(member.name,&member);
 for(auto& member:current.members){if(!SerializedHeader(member.bytes))continue;const auto pristine=member.bytes;auto layout=ParseSerialized(member.bytes);const auto beforeLayout=layout;std::map<int64_t,Bytes> changes;
  for(const auto& operation:operations){std::vector<int64_t> targets;
   if(operation.kind==SemanticOperation::MeshAsset){if(currentLogical!=operation.meshBundle)continue;auto native=resolveMesh(operation.meshBundle,operation.mesh);if(EiemStaticSources::Cab(member.name)==native.first&&layout.objects.count(native.second))targets.push_back(native.second);}
   else if(operation.kind==SemanticOperation::Renderer){auto reference=resolveMesh(operation.meshBundle,operation.mesh);std::set<NativeReference> selected{reference};
    for(const auto& consumer:MeshConsumers(member.bytes,layout,member.name,selected,EiemStaticSources::Cab))targets.push_back(consumer.renderer);}
   if(targets.empty())continue;auto beforeSchemaReader=EiemOfflinePackage::Reader{operation.beforeSchema};auto beforeType=DecodeSchema(beforeSchemaReader);Require(beforeSchemaReader.p==operation.beforeSchema.size(),"semantic baseline schema suffix differs");
   auto editedSchemaReader=EiemOfflinePackage::Reader{operation.editedSchema};auto editedType=DecodeSchema(editedSchemaReader);Require(editedSchemaReader.p==operation.editedSchema.size(),"semantic edited schema suffix differs");
   auto beforeValue=DecodeValue(beforeType,0,operation.before),editedValue=DecodeValue(editedType,0,operation.edited);
   for(auto id:targets){auto object=layout.objects.find(id);if(object==layout.objects.end())continue;const auto& currentType=layout.types.at(object->second.type);if(beforeType.classId!=currentType.classId||editedType.classId!=currentType.classId)continue;
    auto changed=changes.find(id);auto currentBytes=changed==changes.end()?ObjectBytes(member.bytes,object->second):changed->second;auto currentValue=DecodeValue(currentType,0,currentBytes);auto externals=layout.externals;Stats local;
    auto value=Overlay(beforeType,beforeValue,editedType,editedValue,currentType,currentValue,operation.editedExternals,externals,local,{},operation.kind==SemanticOperation::MeshAsset);
    auto encoded=EncodeValue(currentType,value);if(encoded==currentBytes)continue;layout.externals=std::move(externals);changes[id]=std::move(encoded);++stats.targets;++stats.changed;stats.missing+=local.missing;stats.incompatible+=local.incompatible;
   }
  }
  if(!changes.empty()){member.bytes=BuildSerialized(pristine,layout,changes);VerifyObjectReferences(member.bytes,beforeLayout,ParseSerialized(member.bytes),pristine);}
 }
}
} // namespace EiemSemanticRendererReplay

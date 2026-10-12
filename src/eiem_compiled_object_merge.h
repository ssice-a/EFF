#pragma once
#include "eiem_compiled_mod.h"

// Merge registrations carried by proven native members. Component names and
// physics parameters have no meaning here; script identities, schemas and
// references remain the game's own serialized data.
namespace EiemCompiledMod {
inline std::vector<Bytes> ScriptRecords(const Bytes& bytes,const SerializedLayout& layout){
 EiemNativeInput::Reader r(bytes,layout.tableEnd,false,48+layout.metadataSize);auto count=r.Count();
 std::vector<Bytes> out;for(size_t i=0;i<count;++i){auto start=r.p;r.Skip(12);out.emplace_back(bytes.begin()+start,bytes.begin()+r.p);}return out;
}
struct NativeRegistrations {
 SerializedLayout& layout;size_t originalTypes,originalScripts;
 std::vector<Bytes> types,scripts;std::map<Bytes,size_t> typeIds,scriptIds;
 static Bytes TypeKey(Bytes record,size_t dependencies){
  Require(dependencies>=4&&dependencies<=record.size()&&(record.size()-dependencies)%4==0,"compiled native type dependency extent differs");
  std::fill(record.begin()+dependencies,record.end(),(unsigned char)0);return record;
 }
 NativeRegistrations(SerializedLayout& value,const Bytes& original):layout(value),originalTypes(value.types.size()){
  auto native=ScriptRecords(original,layout);originalScripts=native.size();for(size_t i=0;i<native.size();++i)scriptIds.emplace(native[i],i);
  for(size_t i=0;i<layout.types.size();++i)typeIds.emplace(TypeKey(layout.types[i].nativeRecord,layout.types[i].dependencyOffset),i);
 }
 std::vector<size_t> Merge(const Bytes& bytes,const SerializedLayout& source,const std::vector<size_t>& files){
  Require(source.types.size()>=originalTypes,"compiled removed an original native type");
  for(size_t i=0;i<originalTypes;++i)Require(source.types[i].nativeRecord==layout.types[i].nativeRecord,"compiled changed an original native type record");
  auto nativeScripts=ScriptRecords(bytes,source);std::vector<size_t> scriptMap;
  for(auto record:nativeScripts){auto file=Le(record,0,4);Require(file<files.size(),"compiled script FileID lacks source registration");WriteInteger(record,0,files[size_t(file)],4);
   auto next=scriptIds.emplace(record,originalScripts+scripts.size());if(next.second)scripts.push_back(std::move(record));scriptMap.push_back(next.first->second);
  }
  std::vector<size_t> mapping;std::vector<Bytes> records;mapping.reserve(source.types.size());
  for(size_t i=0;i<source.types.size();++i){if(i<originalTypes){mapping.push_back(i);records.push_back(source.types[i].nativeRecord);continue;}
   auto record=source.types[i].nativeRecord;auto script=int16_t(Le(record,5,2));
   if(script>=0){Require(size_t(script)<scriptMap.size()&&scriptMap[size_t(script)]<=INT16_MAX,"compiled script type index extent differs");WriteInteger(record,5,scriptMap[size_t(script)],2);}
   auto key=TypeKey(record,source.types[i].dependencyOffset);auto found=typeIds.find(key);
   if(found==typeIds.end()){auto index=layout.types.size();typeIds.emplace(std::move(key),index);layout.types.push_back(source.types[i]);types.emplace_back();mapping.push_back(index);}else mapping.push_back(found->second);
   records.push_back(std::move(record));
  }
  for(size_t i=originalTypes;i<source.types.size();++i){auto& record=records[i];for(size_t p=source.types[i].dependencyOffset;p<record.size();p+=4){auto dependency=Le(record,p,4);Require(dependency<mapping.size()&&mapping[size_t(dependency)]<=INT_MAX,"compiled native type dependency index differs");WriteInteger(record,p,mapping[size_t(dependency)],4);}
   auto target=mapping[i];if(target<originalTypes){Require(record==layout.types[target].nativeRecord,"compiled native type registration conflict");continue;}
   auto& saved=types.at(target-originalTypes);Require(saved.empty()||saved==record,"compiled native type dependency registration conflict");saved=record;layout.types[target].nativeRecord=record;
  }
  return mapping;
 }
};
inline bool GenericObjectChanges(const SerializedLayout& before,const SerializedLayout& after){
 if(after.types.size()!=before.types.size())return true;
 for(const auto& object:before.objects)if(!after.objects.count(object.first))return true;return false;
}
inline void VerifyObjectReferences(const Bytes& bytes,const SerializedLayout& before,const SerializedLayout& after,const Bytes& original){
 auto unchanged=[&](int64_t id){auto old=before.objects.find(id);if(old==before.objects.end())return false;const auto& next=after.objects.at(id);return old->second.type==next.type&&old->second.size==next.size&&std::equal(bytes.begin()+next.offset,bytes.begin()+next.offset+next.size,original.begin()+old->second.offset);};
 auto references=EiemLoadingGraphAudit::ReadReferences(bytes,after);
 for(const auto& pointer:references.pointers){Require(pointer.fileId>=0&&size_t(pointer.fileId)<=after.externals.size(),"compiled generic reference FileID absent");
  if(!pointer.fileId)Require(after.objects.count(pointer.pathId)||(!before.objects.count(pointer.pathId)&&unchanged(pointer.objectId)),"compiled generic object reference target was deleted/absent: "+std::to_string(pointer.pathId));
 }
 auto nativeScripts=ScriptRecords(original,before);
 for(const auto& record:ScriptRecords(bytes,after)){auto file=Le(record,0,4);auto id=int64_t(Le(record,4,8));Require(file<=after.externals.size(),"compiled script reference FileID absent");
  if(!file&&id&&!after.objects.count(id))Require(!before.objects.count(id)&&std::find(nativeScripts.begin(),nativeScripts.end(),record)!=nativeScripts.end(),"compiled script reference target was deleted/absent: "+std::to_string(id));
 }
 bool deleted=false;for(const auto& object:before.objects)deleted=deleted||!after.objects.count(object.first);
 for(const auto& object:references.unparsed){
  Require(!deleted&&unchanged(object.objectId),"compiled edited object reference schema could not be decoded: "+object.error);
 }
}
} // namespace EiemCompiledMod

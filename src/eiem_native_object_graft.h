#pragma once
#include "eiem_native_serialized_splice.h"

// Compose a current native object from selected pre-encoded author fields and
// retained game fields. Source content hashes and the whole-object type hash
// are deliberately not compatibility gates. Undeclared current fields survive.
namespace EiemNativeObjectGraft {
using namespace EiemNativeInput;
namespace R=EiemNativeRandomAccess;namespace P=EiemNativeSerializedSplice;namespace S=EiemNativeSpliceView;
inline bool SameField(const Type& left,size_t a,const Type& right,size_t b) {
  const auto& x=left.nodes.at(a);const auto& y=right.nodes.at(b);
  if(x.type!=y.type||x.name!=y.name||x.align!=y.align||x.children.size()!=y.children.size())return false;
  for(size_t i=0;i<x.children.size();++i)if(!SameField(left,x.children[i],right,y.children[i]))return false;
  return true;
}
inline bool HasAlignment(const Type& type,size_t node) {
  const auto& n=type.nodes.at(node);if(n.align||n.type=="string")return true;
  for(auto child:n.children)if(HasAlignment(type,child))return true;return false;
}
struct AuthorObject {
  Type type;
  std::map<std::string,Field> fields;
  std::shared_ptr<const EiemInputSpanView::View> view;
  size_t offset=0,size=0;
};
inline AuthorObject Describe(R::Reader& reader,size_t memberIndex,const SerializedLayout& layout,int64_t id,R::ReadRange lease) {
  const auto& member=reader.directory.members.at(memberIndex);const auto& object=layout.objects.at(id);
  AuthorObject result;result.type=layout.types.at(object.type);result.offset=AddSize(member.offset,object.offset);result.size=object.size;
  result.fields=reader.Ranges(member,layout,id);result.view=reader.UncompressedView(std::move(lease));return result;
}
inline P::RangePayload Compose(R::Reader& current,size_t memberIndex,const SerializedLayout& layout,int64_t id,
    const AuthorObject& author,const std::set<std::string>& selected,const std::map<std::string,Value>& overrides={}) {
  Require(bool(author.view),"native author object has no leased input");
  const auto& member=current.directory.members.at(memberIndex);const auto& object=layout.objects.at(id);const auto& type=layout.types.at(object.type);
  Require(type.classId==author.type.classId,"native author object class differs");auto fields=current.Ranges(member,layout,id);std::set<std::string> used;P::RangePayload result;size_t output=0;
  for(auto node:type.nodes.at(0).children){const auto& name=type.nodes.at(node).name;const auto& field=fields.at(name);auto edit=overrides.find(name);
    if(edit!=overrides.end()){
      Require(edit->second.node==node,"native object override belongs to another schema");
      Bytes encoded(output&3);WriteValue(type,edit->second,encoded);encoded.erase(encoded.begin(),encoded.begin()+(output&3));
      auto bytes=std::make_shared<const Bytes>(std::move(encoded));result.pieces.push_back({0,bytes->size(),bytes,{}});output=AddSize(output,bytes->size());continue;
    }
    bool authored=selected.count(name)!=0;const Field* source=&field;const Type* schema=&type;size_t base=AddSize(member.offset,object.offset);
    if(authored){auto found=author.fields.find(name);Require(found!=author.fields.end(),"native author field absent: "+name);source=&found->second;schema=&author.type;base=author.offset;
      Require(SameField(type,node,author.type,source->node),"native author field needs semantic adaptation: "+name);used.insert(name);
    }
    // Reusing encoded padding requires the same offset modulo four. For small
    // exceptional fields decode/re-encode only that field at its new position.
    if(HasAlignment(*schema,source->node)&&((source->offset-base)&3)!=(output&3)){
      auto padding=(source->offset-base)&3;Bytes bytes(AddSize(padding,source->size));
      if(authored)Require(author.view->ReadAt(source->offset,bytes.data()+padding,source->size)==source->size,"native author field truncated");
      else current.ReadAt(source->offset,bytes.data()+padding,source->size);
      Reader input(bytes,padding);auto value=ReadValue(*schema,source->node,input,0);Require(input.p==bytes.size(),"native field alignment adaptation extent differs");
      // Field schemas match, but node indices can differ in the two trees.
      std::function<void(Value&,size_t)> remap=[&](Value& v,size_t n){const auto& old=schema->nodes.at(v.node);v.node=n;const auto& target=type.nodes.at(n);
        if(old.type=="Array"){for(auto& child:v.children)remap(child,target.children.at(1));}
        else for(size_t i=0;i<v.children.size();++i)remap(v.children[i],target.children.at(i));
      };remap(value,node);Bytes encoded(output&3);WriteValue(type,value,encoded);encoded.erase(encoded.begin(),encoded.begin()+(output&3));
      auto owned=std::make_shared<const Bytes>(std::move(encoded));result.pieces.push_back({0,owned->size(),owned,{}});output=AddSize(output,owned->size());
    }else{
      result.pieces.push_back({authored?source->offset:source->offset-member.offset,source->size,{},authored?author.view:nullptr});output=AddSize(output,source->size);
    }
  }
  for(const auto& name:selected)Require(used.count(name)||overrides.count(name),"current native field absent: "+name);
  for(const auto& edit:overrides)Require(fields.count(edit.first),"current override field absent: "+edit.first);
  if(type.nodes.at(0).align&&(output&3)){auto pad=std::make_shared<const Bytes>(4-(output&3),static_cast<unsigned char>(0));result.pieces.push_back({0,pad->size(),pad,{}});}
  return result;
}
} // namespace EiemNativeObjectGraft

// Read compiled native objects without rebuilding or invoking any game API.
#include "../../src/eiem_offline_resource_package.h"
#include <iostream>
using namespace EiemOfflinePackage;
Bytes BlockBytes(const Resource& r,const std::string& name){const auto& b=r.blocks.at(name);return Bytes(r.bytes.begin()+b.offset,r.bytes.begin()+b.offset+b.size);}
int wmain(int argc,wchar_t** argv){try{
 Require(argc==3||argc==4,"inspect_compiled_resource_payloads <generation> <mods> [baseline]");auto folder=std::filesystem::path(argv[1]);auto m=Decode(Read(folder/"manifest.bin",manifestLimit),folder,argv[2]);
 const auto& p=m.package;size_t meshes=0,materials=0,textures=0,originalTextures=0;std::set<std::string> foundMaterials,foundTextures;
 for(const auto& input:m.plan.inputs){auto bundle=EndfieldInput::Decode(Read(folder/"startup"/input.file,inputLimit),inputLimit);
  if(argc==4){auto pristine=EndfieldInput::Decode(Read(std::filesystem::path(argv[3])/"inputs"/input.file,inputLimit),inputLimit);
   for(const auto& member:pristine.members)if(Serialized(member.bytes)){auto old=ParseSerialized(member.bytes);
    auto current=std::find_if(bundle.members.begin(),bundle.members.end(),[&](const auto& r){return r.name==member.name;});Require(current!=bundle.members.end(),"original CAB absent");auto now=ParseSerialized(current->bytes);
    for(const auto& row:old.objects)if(old.types.at(row.second.type).classId==28){const auto& object=now.objects.at(row.first);
     Require(object.size==row.second.size&&std::equal(member.bytes.begin()+row.second.offset,member.bytes.begin()+row.second.offset+row.second.size,current->bytes.begin()+object.offset),"original Texture2D modified");
     const auto& t=old.types.at(row.second.type);auto value=ObjectValue(member.bytes,old,row.first);const auto& stream=Child(t,value,"m_StreamData");const auto& path=Child(t,stream,"path");auto label=Text(path.raw.data(),path.raw.size());auto slash=label.find_last_of("/\\");if(slash!=label.npos)label=label.substr(slash+1);
     auto size=Integer(Child(t,stream,"size")),offset=Integer(Child(t,stream,"offset"));if(size){auto before=std::find_if(pristine.members.begin(),pristine.members.end(),[&](const auto& r){return r.name==label;});auto after=std::find_if(bundle.members.begin(),bundle.members.end(),[&](const auto& r){return r.name==label;});
      Require(before!=pristine.members.end()&&after!=bundle.members.end()&&offset<=before->bytes.size()&&size<=before->bytes.size()-offset&&offset<=after->bytes.size()&&size<=after->bytes.size()-offset&&std::equal(before->bytes.begin()+offset,before->bytes.begin()+offset+size,after->bytes.begin()+offset),"original Texture pixels modified");
     }++originalTextures;
    }
   }
  }
  for(const auto& member:bundle.members)if(Serialized(member.bytes)){auto layout=ParseSerialized(member.bytes);
   for(const auto& row:layout.objects){const auto& t=layout.types.at(row.second.type);if(t.classId!=43&&t.classId!=21&&t.classId!=28)continue;
    auto value=ObjectValue(member.bytes,layout,row.first);auto name=Name(t,value);
    if(t.classId==43){auto expected=m.plan.parts.find(name);if(expected==m.plan.parts.end())continue;
     const auto& mesh=p.meshes.at(p.parts.at(expected->second.id).mesh);const auto& resource=*p.resources.at(mesh.resource);
     for(const char* field:{"m_IndexBuffer","m_IndexFormat","m_BindPose","m_BoneNameHashes","m_Shapes"})Require(EncodeValue(t,Child(t,value,field))==BlockBytes(resource,field),"compiled Mesh author field differs: "+name+"/"+field);
     ++meshes;
    }else if(t.classId==21){
     for(const auto& material:p.materials){auto expected=m.plan.materials.at(material.first).name;if(name!=expected)continue;
      foundMaterials.insert(material.first);const auto& resource=*p.resources.at(material.second.resource);
      for(const auto& field:resource.blocks)Require(EncodeValue(t,Child(t,value,field.first))==BlockBytes(resource,field.first),"compiled Material field differs: "+field.first);
      const auto& props=ArrayValue(t,Child(t,Child(t,value,"m_SavedProperties"),"m_TexEnvs"));
      for(const auto& edit:material.second.textures){bool found=false;for(const auto& prop:props.children){const auto& key=Child(t,prop,"first");if(Text(key.raw.data(),key.raw.size())!=edit.first)continue;auto ref=Reference(t,Child(t,Child(t,prop,"second"),"m_Texture"),layout,member.name);const auto& tex=p.textures.at(edit.second);
        auto id=AllocateTextureId(ref.first,p,edit.second,tex);Require(ref.second==id,"compiled original/custom Texture pointer differs");found=true;
       }Require(found,"compiled Material property absent");
      }++materials;
     }
    }else{
     for(const auto& texture:p.textures){const auto& tex=texture.second;if(row.first!=AllocateTextureId(member.name,p,texture.first,tex))continue;
      const auto& stream=Child(t,value,"m_StreamData");const auto& path=Child(t,stream,"path");auto label=Text(path.raw.data(),path.raw.size());auto slash=label.find_last_of("/\\");if(slash!=label.npos)label=label.substr(slash+1);
      auto payload=std::find_if(bundle.members.begin(),bundle.members.end(),[&](const auto& r){return r.name==label;});Require(payload!=bundle.members.end(),"compiled Texture stream absent");
      auto offset=Integer(Child(t,stream,"offset")),size=Integer(Child(t,stream,"size"));Require(offset<=payload->bytes.size()&&size<=payload->bytes.size()-offset,"compiled Texture stream bounds");
      auto pixels=BlockBytes(*p.resources.at(tex.resource),"pixels");Require(size==pixels.size()&&std::equal(pixels.begin(),pixels.end(),payload->bytes.begin()+offset),"compiled Texture pixels/mips differ");
      Require(Integer(Child(t,value,"m_Width"))==tex.width&&Integer(Child(t,value,"m_Height"))==tex.height&&Integer(Child(t,value,"m_MipCount"))==tex.mips,"compiled Texture dimensions differ");
      foundTextures.insert(texture.first);++textures;
     }
    }
   }
  }
 }
 Require(meshes==m.plan.parts.size()&&foundMaterials.size()==p.materials.size()&&foundTextures.size()==p.textures.size(),"compiled resource coverage differs");
 std::cout<<"PASS compiled native payloads: meshes="<<meshes<<" materials="<<materials<<" textures="<<textures<<" originalsPreserved="<<originalTextures<<" exactIndicesBindposesShapes=1 materialFieldsAndTextureSelections=1 pixelMipBytes=1 nativeAcceptance=0\n";
 }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}

#include <filesystem>
#include <fstream>
#include <iostream>
#include "eiem_texture_input_builder.h"
#include "eiem_loading_generation.h"
using namespace EiemNativeInput;
namespace {
std::string Q(const std::string& s) {
  std::string r="\"";
  for(unsigned char c:s) {
    if(c=='"'||c=='\\'){r+='\\';r+=char(c);}
    else if(c<32){static const char h[]="0123456789ABCDEF";r+="\\u00";r+=h[c>>4];r+=h[c&15];}
    else r+=char(c);
  }
  return r+'"';
}
std::string Hash(const Bytes& b){auto h=Sha(b.data(),b.size());return Hex(h.data(),h.size());}
std::string Name(const Type& t,const Value& v){const auto& n=Child(t,v,"m_Name");return Text(n.raw.data(),n.raw.size());}
std::string Ref(const Type& t,const Value& v,const SerializedLayout& l,const std::string& cab){
  auto id=(int64_t)Integer(Child(t,v,"m_PathID"));auto file=Integer(Child(t,v,"m_FileID"));
  Require(file<=l.externals.size(),"descriptor PPtr file slot out of range");
  return "{\"member\":"+Q(id?(file?l.externals[size_t(file)-1]:cab):"")+",\"pathId\":"+Q(std::to_string(id))+",\"fileId\":"+std::to_string(file)+"}";
}
void Objects(std::ostream& out,const EndfieldInput::Bundle& b){
  bool first=true;out<<'[';
  for(const auto& member:b.members){
    if(member.name.size()!=36||member.name.rfind("CAB-",0)!=0)continue;
    auto l=ParseSerialized(member.bytes);auto assets=SerializedAssets(member.bytes,member.name,l);
    for(const auto& row:l.objects){
      const auto& type=l.types.at(row.second.type);if(type.classId!=21&&type.classId!=28)continue;
      auto v=ObjectValue(member.bytes,l,row.first);auto oh=Sha(member.bytes.data()+row.second.offset,row.second.size);auto objectHash=Hex(oh.data(),oh.size());
      if(!first)out<<',';first=false;
      out<<"{\"member\":"<<Q(member.name)<<",\"pathId\":"<<Q(std::to_string(row.first))<<",\"classId\":"<<type.classId<<",\"objectSha256\":"<<Q(objectHash)<<",\"name\":"<<Q(Name(type,v))<<",\"containerPaths\":[";
      bool af=true;for(const auto& a:assets)if(a.pathId==row.first){if(!af)out<<',';af=false;out<<Q(a.path);}out<<']';
      if(type.classId==21){
        out<<",\"shader\":"<<Ref(type,Child(type,v,"m_Shader"),l,member.name)<<",\"textureProperties\":[";
        bool pf=true;auto& saved=Child(type,v,"m_SavedProperties");
        for(const auto& p:ArrayValue(type,Child(type,saved,"m_TexEnvs")).children){
          const auto& key=Child(type,p,"first");auto prop=Text(key.raw.data(),key.raw.size());
          if(!pf)out<<',';pf=false;
          out<<"{\"property\":"<<Q(prop)<<",\"texture\":"<<Ref(type,Child(type,Child(type,p,"second"),"m_Texture"),l,member.name)<<'}';
        }
        out<<']';
      }else{
        for(const char* f:{"m_Width","m_Height","m_TextureFormat","m_MipCount","m_ColorSpace","m_IsReadable"})
          out<<','<<Q(f)<<':'<<Integer(Child(type,v,f));
        const auto& stream=Child(type,v,"m_StreamData");const auto& p=Child(type,stream,"path");auto path=Text(p.raw.data(),p.raw.size());
        auto off=Integer(Child(type,stream,"offset")),size=Integer(Child(type,stream,"size"));
        std::string digest;
        if(size){auto name=EiemLoadingGraphAudit::Basename(path);const EndfieldInput::Member* data=nullptr;
          for(const auto& candidate:b.members)if(candidate.name==name){Require(!data,"ambiguous texture stream member");data=&candidate;}
          Require(data&&off<=data->bytes.size()&&size<=data->bytes.size()-off,"texture stream descriptor outside local member");
          auto hash=Sha(data->bytes.data()+size_t(off),size_t(size));digest=Hex(hash.data(),hash.size());
        }
        out<<",\"stream\":{\"path\":"<<Q(path)<<",\"offset\":"<<off<<",\"bytes\":"<<size<<",\"sha256\":"<<Q(digest)<<'}';
      }
      out<<'}';
    }
  }
  out<<']';
}
}
int main(int argc,char** argv){
 try{
  Require(argc>=7,"usage: inspect_material_candidates <accepted-dir> <candidate-dir> <pristine-dir> <key> <new-report.json> <input filenames...>");
  auto oldRoot=std::filesystem::u8path(argv[1]),newRoot=std::filesystem::u8path(argv[2]),output=std::filesystem::u8path(argv[5]),pristineRoot=std::filesystem::u8path(argv[3]);
  Require(!std::filesystem::exists(output),"descriptor report already exists");
  std::string key=argv[4];Require(key.size()==64 && key.find_first_not_of("0123456789ABCDEF")==std::string::npos,"descriptor generation key invalid");
  struct Input{std::string name;Bytes oldBytes,newBytes,pristineBytes;EndfieldInput::Bundle oldBundle,newBundle,pristineBundle;};
  std::vector<Input> inputs;EiemLoadingGeneration::Names names;
  for(int i=6;i<argc;++i){
    std::filesystem::path leaf=std::filesystem::u8path(argv[i]);Require(leaf==leaf.filename()&&leaf.extension()==".vfs","input is not a flat VFS filename");
    Input input;input.name=leaf.u8string();input.oldBytes=Read(oldRoot/leaf);input.newBytes=Read(newRoot/leaf);
    std::filesystem::path pristinePath;
    for(const auto& prefix:{pristineRoot,pristineRoot/"sources",pristineRoot/"consumers"})if(std::filesystem::is_regular_file(prefix/leaf)){
      Require(pristinePath.empty(),"ambiguous pristine input");pristinePath=prefix/leaf;
    }
    Require(!pristinePath.empty(),"pristine input absent");input.pristineBytes=Read(pristinePath);
    input.pristineBundle=EndfieldInput::Decode(input.pristineBytes);
    input.oldBundle=EndfieldInput::Decode(input.oldBytes);input.newBundle=EndfieldInput::Decode(input.newBytes);
    for(const auto& member:input.oldBundle.members){
      auto cab=member.name.substr(0,36);Require(cab.size()==36&&cab.rfind("CAB-",0)==0,"unexpected candidate member grammar");
      names.members.emplace(member.name,"CAB-"+EiemLoadingGeneration::Token(key,cab,32)+member.name.substr(36));
      if(member.name.size()!=36)continue;
      auto l=ParseSerialized(member.bytes);
      for(const auto& row:l.objects)if(l.types.at(row.second.type).classId==142){
        auto value=ObjectValue(member.bytes,l,row.first);auto original=Name(l.types.at(row.second.type),value);
        names.bundles.emplace(original,"main/"+EiemLoadingGeneration::Token(key,"bundle:"+original,24)+".ab");
      }
    }
    inputs.push_back(std::move(input));
  }
  // Validate all files before publishing even an offline descriptor. The candidate
  // must differ only in parsed Bundle/CAB/stream names; numeric references and
  // original plus custom Texture payloads remain the accepted production bytes.
  for(const auto& input:inputs){
    Require(input.oldBundle.members.size()==input.newBundle.members.size(),"candidate member count differs");
    for(size_t i=0;i<input.oldBundle.members.size();++i){
      const auto& old=input.oldBundle.members[i];const auto& next=input.newBundle.members[i];
      Require(next.name==names.members.at(old.name)&&old.flags==next.flags,"candidate member identity/flags differ");
      auto expected=old.name.size()==36?EiemLoadingGeneration::PrepareSerialized(old.bytes,names).bytes:old.bytes;
      Require(expected==next.bytes,"candidate payload changed outside namespace or selected dependency closure missing");
    }
  }
  std::ofstream out(output,std::ios::binary);Require(bool(out),"cannot create descriptor output");
  out<<"{\"format\":\"EFF_MATERIAL_TEXTURE_CANDIDATE_DESCRIPTORS\",\"version\":1,\"generationKey\":"<<Q(key)<<",\"nativeSubmission\":false,\"namespaceOnlyPayloadVerified\":true,\"inputs\":[";
  bool first=true;for(const auto& input:inputs){if(!first)out<<',';first=false;
    out<<"{\"file\":"<<Q(input.name)<<",\"originalSha256\":"<<Q(Hash(input.oldBytes))<<",\"candidateSha256\":"<<Q(Hash(input.newBytes))<<",\"sourceObjects\":";Objects(out,input.oldBundle);
    out<<",\"candidateObjects\":";Objects(out,input.newBundle);out<<",\"pristineSha256\":"<<Q(Hash(input.pristineBytes))<<",\"pristineObjects\":";Objects(out,input.pristineBundle);out<<'}';
  }
  out<<"],\"sharedNativeDependenciesVerified\":false,\"nativeTexturePixelsVerified\":false,\"materialTexturePrefabCandidateVerified\":false,\"completeResourceHotReloadImplemented\":false}\n";
  Require(bool(out),"short descriptor report write");std::cout<<"PASS candidate descriptors inputs="<<inputs.size()<<" payloadsAndStreamsPreserved=1 nativeSubmission=0\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}

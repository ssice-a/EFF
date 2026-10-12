#pragma once
#include "../eff_author_sources.h"

// Exercise the author index/VFS boundary independently of the game's files.
inline void AuthorSourceRegression() {
 using namespace EiemAuthorSources;
 auto root=std::filesystem::temp_directory_path()/("eff-unified-test-"+std::to_string(GetCurrentProcessId())+"-"+std::to_string(GetTickCount64()));
 Require(std::filesystem::create_directory(root),"source fixture already exists");
 struct Cleanup{std::filesystem::path path;~Cleanup(){std::error_code error;std::filesystem::remove_all(path,error);}}cleanup{root};
 auto number=[](Bytes& b,uint64_t value,size_t width){for(size_t i=0;i<width;++i)b.push_back((unsigned char)(value>>(i*8)));};
 auto string=[&](Bytes& b,const std::string& value){auto n=value.size();do{auto byte=unsigned(n&127);n>>=7;b.push_back((unsigned char)(byte|(n?128:0)));}while(n);b.insert(b.end(),value.begin(),value.end());};
 auto asset=[&](Bytes& b,const char* path){b.push_back(1);string(b,"mesh");string(b,path);string(b,"Mesh");number(b,123,8);string(b,"Data/Bundles/Windows/A.ab");};
 auto cab=[&](Bytes& b,const char* name,const char* source,const std::vector<std::string>& deps){b.push_back(2);string(b,name);string(b,source);number(b,deps.size(),4);for(const auto& dep:deps)string(b,dep);};
 Bytes index;string(index,"EFF_END_FIELD_INDEX");number(index,9999,4);string(index,"fingerprint");
 asset(index,"ASSETS\\MODEL.PREFAB");for(size_t i=0;i<100;++i)asset(index,"assets/irrelevant.prefab");
 cab(index,"CAB-one","Bundles/Windows/A.ab",{"CAB-one","CAB-two","unity default resources","CAB-unresolved"});
 cab(index,"CAB-two","Bundles/Windows/B.ab",{});cab(index,"CAB-three","Bundles/Windows/A.ab",{"CAB-two"});
 auto prefix=index;index.push_back(0);number(index,101,8);number(index,3,8);
 auto path=root/"sources.eidx";EiemResourceReloadPlan::Save(path,index);
 Requests requests{{Identity{"assets/model.prefab","mesh"},"Mesh"}};
 auto read=ReadIndex(path,requests);
 Require(read.fingerprint=="fingerprint"&&read.assets.size()==1&&read.assets.front().id==123&&read.assets.front().source=="bundles/windows/a.ab","source query lost exact case/slash-normalized identity");
 Require(read.sources==std::vector<std::string>({"bundles/windows/a.ab","bundles/windows/b.ab"})&&read.graph==std::vector<std::vector<size_t>>({{1},{}}),"source dependencies lost edges or retained self/builtin/unresolved edges");
 auto rejected=[&](const Bytes& bytes){EiemResourceReloadPlan::Save(path,bytes);bool failed=false;try{ReadIndex(path,requests);}catch(const std::exception&){failed=true;}Require(failed,"malformed source index accepted");};
 auto invalid=index;invalid.push_back(0);rejected(invalid);invalid=index;invalid.pop_back();rejected(invalid);invalid=index;invalid[invalid.size()-16]=102;rejected(invalid);
 invalid=prefix;cab(invalid,"CAB-one","Bundles/Windows/Conflicting.ab",{});rejected(invalid);
 Bytes badString{255,255,255,255,8};bool failed=false;try{EiemAuthorSources::Reader r(badString);r.S();}catch(const std::exception&){failed=true;}Require(failed,"overflowing source string accepted");
 Require(VfsLogical("\x01 prefix Assets\\StreamingAssets\\Bundles/Windows/A.ab!suffix")=="bundles/windows/a.ab"&&VfsLogical("Data/Bundles/B.ab")=="bundles/b.ab"&&VfsLogical("/Bundles/C.ab/")=="bundles/c.ab","VFS path normalization differs");
 Require(Md5(Bytes{'a','b','c'})=="900150983CD24FB0D6963F7D28E17F72","native source MD5 differs");

 auto vfs=root/"vfs";std::filesystem::create_directory(vfs);
 auto blc=[&](const char* name,uint64_t offset){
  Bytes plain;number(plain,11,4);number(plain,0,2);number(plain,0,8);number(plain,0,4);number(plain,0,8);number(plain,0,1);number(plain,1,4);
  plain.resize(plain.size()+16+16);number(plain,0,8);number(plain,0,1);number(plain,2,4);
  for(const char* logical:{"Data/Bundles/Selected.ab","Data/Bundles/Unrelated.ab"}){number(plain,strlen(logical),2);plain.insert(plain.end(),logical,logical+strlen(logical));number(plain,0,8);plain.resize(plain.size()+16+16);number(plain,offset,8);number(plain,5,8);number(plain,0,1);number(plain,0,1);}
  number(plain,0,4);Bytes nonce(12);ChaCha(plain,nonce.data());nonce.insert(nonce.end(),plain.begin(),plain.end());EiemResourceReloadPlan::Save(vfs/name,nonce);
 };
 blc("a.blc",1);blc("b.blc",2);
 auto selected=ReadVfs(vfs,{"bundles/selected.ab"});
 Require(selected.size()==1&&selected.begin()->second.offset==2&&selected.begin()->second.size==5,"parallel VFS parsing changed last-metadata precedence or selected unrelated assets");
 auto fingerprint=Fingerprint(vfs,Files(vfs,L".blc"));Require(ReadVfs(vfs,{"bundles/selected.ab"},fingerprint).size()==1,"VFS fingerprint rejected identical files");
 failed=false;try{ReadVfs(vfs,{"bundles/selected.ab"},std::string(64,'0'));}catch(const std::exception&){failed=true;}Require(failed,"stale VFS fingerprint accepted");
 std::cout<<"PASS author exact-source discovery, filtered index, dependency/count validation, VFS routing and deterministic parallel metadata\n";
}

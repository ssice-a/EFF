#include "../eff_author_sources.h"
#include <iostream>
#include <sstream>
int wmain(int argc,wchar_t** argv){try{
 using namespace EiemAuthorSources;
 if(argc==3&&std::wstring(argv[1])==L"--synthetic"){
  auto game=std::filesystem::path(argv[2]);auto effective=ReadEffectiveVfs(game,{"bundles/same.ab","bundles/keep.ab","bundles/new.ab"});
  auto read=[](const VfsEntry& e){auto data=EiemNativeInput::Read(e.physical);return Bytes(data.begin()+e.offset,data.begin()+e.offset+e.size);};
  Require(read(effective.at("bundles/same.ab"))==Bytes({5,6})&&read(effective.at("bundles/keep.ab"))==Bytes({3})&&read(effective.at("bundles/new.ab"))==Bytes({7}),"synthetic effective payload differs");
  for(const auto& name:{"bundles/removed.ab","bundles/missing.ab"}){bool rejected=false;try{ReadEffectiveVfs(game,{name});}catch(const std::exception&){rejected=true;}Require(rejected,"removed/unavailable current source used old bytes");}
  std::cout<<"PASS native synthetic VFS: new/changed/removed source, same-content fallback, unavailable new source rejects old bytes fingerprint="<<GameFingerprint(game)<<std::endl;return 0;
 }
 if(argc!=4)throw std::runtime_error("usage: vfs_layers_native_smoke GAME EXPECTED_FINGERPRINT TSV");
 auto game=std::filesystem::path(argv[1]);std::wstring expectedWide(argv[2]);std::string expected(expectedWide.begin(),expectedWide.end());auto fingerprint=GameFingerprint(game);Require(fingerprint==expected,"C++ / AnimeStudio layered fingerprint differs");
 std::ifstream in{std::filesystem::path(argv[3])};Require(bool(in),"mounted ranges TSV absent");std::map<std::string,VfsEntry> expectedEntries;std::string line;
 while(std::getline(in,line)){std::vector<std::string> fields;std::stringstream row(line);std::string field;while(std::getline(row,field,'\t'))fields.push_back(field);Require(fields.size()==5,"range TSV width differs");expectedEntries.emplace(fields[0],VfsEntry{std::filesystem::u8path(fields[1]),std::stoull(fields[2]),std::stoull(fields[3]),fields[4]});}
 std::set<std::string> selected;for(const auto& row:expectedEntries)selected.insert(row.first);auto actual=ReadEffectiveVfs(game,selected);
 for(const auto& row:expectedEntries){const auto& got=actual.at(row.first);Require(_wcsicmp(got.physical.lexically_normal().c_str(),row.second.physical.lexically_normal().c_str())==0&&got.offset==row.second.offset&&got.size==row.second.size&&got.md5==row.second.md5,"native mounted range differs: "+row.first);}
 std::cout<<"PASS native VFS layers: mountedRanges="<<actual.size()<<" fingerprint="<<fingerprint<<" metadataOnly=1"<<std::endl;
 return 0;
 }catch(const std::exception& error){std::cerr<<"FAIL native VFS layers: "<<error.what()<<std::endl;return 1;}}

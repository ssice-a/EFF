#include "../eff_author_sources.h"
#include <iostream>
// Cross-check exactly the ranges returned by a live native VFS query. The
// input TSV comes from the survey; disk precedence is never inferred here.
int VerifyMounted(int argc,wchar_t** argv){
 using namespace EiemAuthorSources;Require(argc==5,"inspect_vfs_lookup_names --mounted <game> <native-range.tsv> <report.json>");
 auto game=std::filesystem::canonical(argv[2]);auto input=Read(argv[3]);std::istringstream lines(Text(input.data(),input.size()));std::string line;
 struct Range{std::string logical;std::filesystem::path physical;uint64_t offset,size;};std::vector<Range> ranges;std::set<std::string> selected;
 while(std::getline(lines,line)){if(!line.empty()&&line.back()=='\r')line.pop_back();auto fields=TabFields(line);Require(fields.size()==4,"native range TSV shape differs");
  Require(selected.insert(fields[0]).second,"native range repeated");ranges.push_back({fields[0],std::filesystem::u8path(fields[1]),std::stoull(fields[2]),std::stoull(fields[3])});}
 Require(!ranges.empty(),"native range list empty");auto shipped=ReadVfs(game/"Endfield_Data/StreamingAssets/VFS",selected),persistent=ReadVfs(game/"Endfield_Data/Persistent/VFS",selected);
 std::ostringstream report;report<<"{\"nativeRanges\":"<<ranges.size()<<",\"nativeChunkContentDigestVerified\":false,\"originalArchivesModified\":false,\"sources\":[";bool first=true;
 for(const auto& range:ranges){const VfsEntry* entry=nullptr;for(const auto* catalog:{&shipped,&persistent}){auto found=catalog->find(range.logical);if(found==catalog->end())continue;
   if(std::filesystem::weakly_canonical(found->second.physical)==std::filesystem::weakly_canonical(range.physical)){Require(!entry,"native range has ambiguous BLC owners");entry=&found->second;}}
  Require(entry&&entry->offset==range.offset&&entry->size==range.size,"native range differs from BLC: "+range.logical);
  Require(!entry->encrypted,"native range's outer encryption requires a different readback test");auto before=FileStamp(range.physical);
  Require(range.size<=SIZE_MAX&&range.offset<=before.bytes&&range.size<=before.bytes-range.offset,"native range escapes CHK");Bytes raw(size_t(range.size));std::ifstream stream(range.physical,std::ios::binary);Require(bool(stream),"native range cannot open CHK");stream.seekg(std::streamoff(range.offset));
  for(size_t p=0;p<raw.size();){auto n=std::min(raw.size()-p,size_t(1024*1024));stream.read((char*)raw.data()+p,n);Require(size_t(stream.gcount())==n,"native range short read");p+=n;}
  Require(SameStamp(before,FileStamp(range.physical))&&Md5(raw)==entry->md5,"native range bytes differ from BLC content digest");
  if(!first)report<<',';first=false;report<<"{\"logical\":"<<Json(range.logical)<<",\"physical\":"<<Json(range.physical.u8string())<<",\"offset\":"<<range.offset<<",\"bytes\":"<<range.size<<",\"fileContentMd5\":"<<Json(entry->md5)<<",\"chunkContentMd5\":"<<Json(entry->chunkContentMd5)<<",\"sourceSha256\":"<<Json(EiemResourceReloadPlan::Hash(raw))<<"}";
 }
 report<<"]}";auto text=report.str();EiemOfflineCompile::AtomicSave(argv[4],Bytes(text.begin(),text.end()));
 std::cout<<"PASS mounted native ranges: sources="<<ranges.size()<<" physicalAndRange=exact perFileBytesMd5=exact nativeChunkDigestVerified=0 originalArchivesModified=0\n";return 0;
}
int wmain(int argc,wchar_t** argv){try{
 if(argc>1&&!wcscmp(argv[1],L"--mounted"))return VerifyMounted(argc,argv);
 using namespace EiemAuthorSources;Require(argc==3,"inspect_vfs_lookup_names <VFS> <logical-source>");auto logical=std::filesystem::path(argv[2]).u8string();auto found=ReadVfs(argv[1],{logical});
 for(const auto& row:found)std::cout<<"logical="<<row.first<<" lookupHex="<<Hex((const unsigned char*)row.second.lookupName.data(),row.second.lookupName.size())<<" md5="<<row.second.md5<<" bytes="<<row.second.size<<" offset="<<row.second.offset<<" physical="<<row.second.physical.u8string()<<" chunkContentMd5="<<row.second.chunkContentMd5<<'\n';
 Require(!found.empty(),"source absent from BLC");return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}

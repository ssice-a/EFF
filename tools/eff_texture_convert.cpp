// Manual PNG -> format-2 Pixels conversion. No game/DLL calls or pack watcher.
#include "../src/eiem_resource_directory.h"
#include <wincodec.h>
#include <wrl/client.h>
#include <chrono>
#include <iostream>

using namespace EiemDirectory;
using Microsoft::WRL::ComPtr;
namespace fs = std::filesystem;

struct Target {
  fs::path root, output;
  std::string section;
  unsigned width = 0, height = 0, mips = 0;
  fs::path previous;
  std::vector<std::string> sections;
};
struct Image { unsigned width, height; Bytes pixels; };

std::string Lower(std::string s) {
  for (auto& c : s) if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
  return s;
}
void Check(HRESULT result, const char* message) {
  Require(SUCCEEDED(result), std::string(message) + " (HRESULT " + std::to_string(result) + ")");
}
bool Within(const fs::path& root, const fs::path& path) {
  auto base = fs::weakly_canonical(root).wstring();
  auto value = fs::weakly_canonical(path).wstring();
  return value.size() > base.size() &&
    _wcsnicmp(base.c_str(), value.c_str(), base.size()) == 0 &&
    (value[base.size()] == L'\\' || value[base.size()] == L'/');
}
size_t PixelSize(unsigned w, unsigned h) {
  Require(w && h && uint64_t(w) * h <= (512ull * 1024 * 1024) / 4,
          "image exceeds 512 MiB pixel limit");
  return size_t(w) * h * 4;
}
Image ReadPng(IWICImagingFactory* factory, const fs::path& path, const Target& target) {
  ComPtr<IWICBitmapDecoder> decoder;
  Check(factory->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ,
        WICDecodeMetadataCacheOnLoad, &decoder), "cannot decode PNG");
  GUID container{};
  Check(decoder->GetContainerFormat(&container), "cannot read image format");
  Require(IsEqualGUID(container, GUID_ContainerFormatPng), "input is not PNG");
  UINT count = 0;
  Check(decoder->GetFrameCount(&count), "cannot read PNG frame count");
  Require(count == 1, "animated/multiframe PNG is unsupported");
  ComPtr<IWICBitmapFrameDecode> frame;
  Check(decoder->GetFrame(0, &frame), "cannot read PNG frame");
  Image result{};
  Check(frame->GetSize(&result.width, &result.height), "cannot read PNG dimensions");
  Require(result.width == target.width && result.height == target.height,
          "PNG dimensions " + std::to_string(result.width) + "x" + std::to_string(result.height) +
          " differ from INI " + std::to_string(target.width) + "x" + std::to_string(target.height));
  result.pixels.resize(PixelSize(result.width, result.height));
  ComPtr<IWICFormatConverter> converter;
  Check(factory->CreateFormatConverter(&converter), "cannot create RGBA decoder");
  // Deliberately no profile/gamma transform, matching Blender's PNG RGBA input.
  Check(converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppRGBA,
        WICBitmapDitherTypeNone, nullptr, 0, WICBitmapPaletteTypeCustom), "cannot decode PNG to RGBA32");
  Check(converter->CopyPixels(nullptr, result.width * 4, UINT(result.pixels.size()),
                             result.pixels.data()), "cannot copy PNG pixels");
  return result;
}

// BOX mip generation uses separable fixed-point averages and premultiplied
// alpha, as Pillow's RGBA BOX path does in the Blender exporter.
Image BoxMip(const Image& input) {
  Image premultiplied = input;
  for (size_t p = 0; p < input.pixels.size(); p += 4) {
    for (size_t c = 0; c < 3; ++c) {
      unsigned t = unsigned(input.pixels[p + c]) * input.pixels[p + 3] + 128;
      premultiplied.pixels[p + c] = static_cast<unsigned char>(((t >> 8) + t) >> 8);
    }
  }
  auto resizeAxis = [](const Image& src, unsigned count, bool horizontal) {
    unsigned old = horizontal ? src.width : src.height;
    if (old == count) return src;
    Image dst{horizontal ? count : src.width, horizontal ? src.height : count, {}};
    dst.pixels.resize(PixelSize(dst.width, dst.height));
    const double scale = double(old) / count;
    const unsigned fixed = 1u << 22;
    for (unsigned out = 0; out < count; ++out) {
      double center = (out + .5) * scale;
      int first = std::max(0, int(center - scale * .5 + .5));
      int last = std::min(int(old), int(center + scale * .5 + .5));
      std::vector<unsigned> weights;
      unsigned active = 0;
      for (int in = first; in < last; ++in) {
        double distance = (in - center + .5) / scale;
        weights.push_back(distance > -.5 && distance <= .5 ? 1u : 0u);
        active += weights.back();
      }
      Require(active != 0, "empty BOX mip footprint");
      for (auto& weight : weights) weight = weight ? unsigned(double(fixed) / active + .5) : 0;
      unsigned rows = horizontal ? dst.height : dst.width;
      for (unsigned row = 0; row < rows; ++row) {
        size_t to = (horizontal ? size_t(row) * dst.width + out : size_t(out) * dst.width + row) * 4;
        for (unsigned c = 0; c < 4; ++c) {
          uint64_t sum = fixed / 2;
          for (int in = first; in < last; ++in) {
            size_t from = (horizontal ? size_t(row) * src.width + in : size_t(in) * src.width + row) * 4;
            sum += uint64_t(src.pixels[from + c]) * weights[size_t(in - first)];
          }
          dst.pixels[to + c] = static_cast<unsigned char>(std::min<uint64_t>(255, sum >> 22));
        }
      }
    }
    return dst;
  };
  auto horizontal = resizeAxis(premultiplied, std::max(1u, input.width / 2), true);
  auto result = resizeAxis(horizontal, std::max(1u, input.height / 2), false);
  for (size_t p = 0; p < result.pixels.size(); p += 4) {
    unsigned alpha = result.pixels[p + 3];
    for (size_t c = 0; c < 3; ++c) if (alpha && alpha != 255)
      result.pixels[p + c] = static_cast<unsigned char>(std::min(255u, unsigned(result.pixels[p + c]) * 255 / alpha));
  }
  return result;
}
void Put(Bytes& bytes, size_t offset, uint64_t value, size_t size) {
  for (size_t i = 0; i < size; ++i) bytes.at(offset + i) = static_cast<unsigned char>(value >> (8 * i));
}
Bytes EncodePixels(Image image, unsigned mips) {
  Bytes output(144, 0);
  for (unsigned i = 0; i < mips; ++i) {
    Require(output.size() + image.pixels.size() <= 512ull * 1024 * 1024,
            "mip chain exceeds 512 MiB limit");
    for (unsigned y = image.height; y > 0; --y) {
      auto start = image.pixels.begin() + size_t(y - 1) * image.width * 4;
      output.insert(output.end(), start, start + size_t(image.width) * 4);
    }
    if (i + 1 < mips) image = BoxMip(image);
  }
  memcpy(output.data(), "EFFRES2\0", 8);
  Put(output, 8, 2, 4); Put(output, 12, 4, 4); Put(output, 16, 1, 4);
  Put(output, 20, 56, 4); Put(output, 40, output.size(), 8);
  Put(output, 80, 6, 2); memcpy(output.data() + 82, "pixels", 6);
  Put(output, 88, 144, 8); Put(output, 96, output.size() - 144, 8);
  auto pixelsHash = Sha(output.data() + 144, output.size() - 144);
  std::copy(pixelsHash.begin(), pixelsHash.end(), output.begin() + 104);
  auto bodyHash = Sha(output.data() + 80, output.size() - 80);
  std::copy(bodyHash.begin(), bodyHash.end(), output.begin() + 48);
  return output;
}

std::string WithoutHash(std::string stem) {
  auto split = stem.rfind('_');
  if (split != stem.npos && stem.size() - split - 1 == 12 &&
      std::all_of(stem.begin() + split + 1, stem.end(), [](unsigned char c) { return std::isxdigit(c) != 0; }))
    stem.resize(split);
  return stem;
}
std::vector<Target> ReadTargets(const fs::path& ini) {
  auto root = fs::canonical(ini).parent_path();
  auto sections = ParseIni(Read(ini, 4 * 1024 * 1024));
  bool format2 = false;
  std::vector<Target> result;
  for (const auto& s : sections) {
    if (s.id == "Mod") format2 = s.values.at("format") == "2";
    if (!Begins(s.id, "Texture")) continue;
    Keys(s, {"path", "source.path", "source.asset", "width", "height", "format", "mip_count",
             "linear", "filter", "wrap", "aniso", "mip_bias"});
    const auto& v = s.values;
    Require(v.at("format") == "RGBA32", "only INI format=RGBA32 is currently supported: " + s.id);
    Require((v.at("linear") == "true" || v.at("linear") == "false") &&
            Integer(v.at("filter")) <= 2 && Integer(v.at("wrap")) <= 3 &&
            Integer(v.at("aniso")) <= 16 && std::abs(Number(v.at("mip_bias"))) <= 3.402823466e38,
            "invalid texture INI settings: " + s.id);
    auto output = DiskPath(root, v.at("path"), 4);
    Require(Begins(v.at("path"), "textures/") && Lower(output.extension().u8string()) == ".tex" &&
            Within(root, output), "texture output must remain inside Mod/textures");
    Target target{root, output, s.id, Integer(v.at("width")), Integer(v.at("height")), Integer(v.at("mip_count"))};
    target.sections.push_back(s.id);
    PixelSize(target.width, target.height);
    unsigned maxMips = 1;
    for (unsigned n = std::max(target.width, target.height); n > 1; n >>= 1) ++maxMips;
    Require(target.mips && target.mips <= maxMips, "invalid INI mip_count: " + s.id);
    auto duplicate = std::find_if(result.begin(), result.end(), [&](const Target& t) { return t.output == output; });
    if (duplicate != result.end()) {
      Require(duplicate->width == target.width && duplicate->height == target.height && duplicate->mips == target.mips,
              "shared .tex has conflicting INI dimensions/mips: " + v.at("path"));
      duplicate->sections.push_back(s.id);
    } else result.push_back(std::move(target));
  }
  Require(format2 && !result.empty(), "expected Mod format=2 with Texture declarations: " + ini.u8string());
  return result;
}
Target Resolve(const fs::path& png, const std::vector<Target>& targets,const std::string& selected,bool interactive) {
  auto stem = Lower(png.stem().u8string());
  std::vector<Target> exact, alias;
  for (const auto& t : targets) {
    if(!selected.empty()){if(std::find(t.sections.begin(),t.sections.end(),selected)!=t.sections.end())exact.push_back(t);continue;}
    auto outputStem = Lower(t.output.stem().u8string());
    if (stem == outputStem) exact.push_back(t);
    else if (stem == WithoutHash(outputStem)||WithoutHash(stem)==outputStem) alias.push_back(t);
  }
  auto matched = exact.empty() ? alias : exact;
  if(matched.empty()&&interactive&&selected.empty()){
    std::cout<<"Choose INI texture settings for "<<png.filename().u8string()<<":\n";
    for(size_t i=0;i<targets.size();++i)std::cout<<i+1<<". "<<targets[i].section<<"  "<<targets[i].width<<'x'<<targets[i].height<<" mips="<<targets[i].mips<<"  "<<targets[i].root.u8string()<<'\n';
    std::cout<<"Texture number (0 cancels): "<<std::flush;std::string value;std::getline(std::cin,value);
    auto index=Integer(Trim(value));Require(index&&index<=targets.size(),"no texture selected");matched.push_back(targets[index-1]);
  }
  Require(!matched.empty(), "no INI Texture.path matches PNG name; use --texture <TextureSection>: " + png.filename().u8string());
  Require(matched.size() == 1, "PNG name matches multiple textures/Mods; drag the target mod.ini with the PNG: " + png.filename().u8string());
  auto result=matched[0];result.previous=result.output;auto filename=png.filename();filename.replace_extension(".tex");result.output=result.output.parent_path()/filename;
  Require(Within(result.root,result.output),"same-name .tex escapes Mod directory");
  for(const auto& other:targets)Require(other.root!=result.root||other.output!=result.output||other.output==result.previous,"same-name output already belongs to a different Texture declaration");
  return result;
}
fs::path Executable() {
  std::wstring value(32768, L'\0');
  DWORD count = GetModuleFileNameW(nullptr, value.data(), DWORD(value.size()));
  Require(count && count < value.size(), "cannot locate converter EXE");
  value.resize(count);
  return fs::path(value);
}
Bytes RenameIni(const Bytes& original,const std::map<std::string,std::string>& names){
  std::string text((const char*)original.data(),original.size()),output,section;size_t at=0;std::set<std::string> found;
  while(at<text.size()){auto end=text.find('\n',at);if(end==text.npos)end=text.size();auto line=text.substr(at,end-at);auto trimmed=Trim(line);
    if(trimmed.size()>2&&trimmed.front()=='['&&trimmed.back()==']')section=trimmed.substr(1,trimmed.size()-2);
    if(Begins(section,"Texture")){auto equal=line.find('=');if(equal!=line.npos&&Trim(line.substr(0,equal))=="path"){
      auto name=Trim(line.substr(equal+1));auto change=names.find(name);if(change!=names.end()){
        auto suffix=!line.empty()&&line.back()=='\r'?"\r":"";line=line.substr(0,equal+1)+change->second+suffix;found.insert(name);
      }
    }}output+=line;if(end<text.size())output+='\n';at=end+1;
  }
  Require(found.size()==names.size(),"INI texture rename coverage differs");return Bytes(output.begin(),output.end());
}
struct Change {fs::path path,temporary,backup;bool existed=false,committed=false;};
void Store(const fs::path& path,const Bytes& bytes){std::ofstream file(path,std::ios::binary);Require(bool(file),"cannot create conversion staging");file.write((const char*)bytes.data(),bytes.size());file.close();Require(bool(file),"cannot finish conversion staging");}
void Replace(const fs::path& from,const fs::path& to){Require(MoveFileExW(from.c_str(),to.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0,"cannot publish conversion (Windows error "+std::to_string(GetLastError())+")");}
void WriteAtomic(const std::vector<std::pair<Target,Bytes>>& converted){
  std::vector<Change> changes;std::map<fs::path,std::map<std::string,std::string>> renames;std::map<fs::path,fs::path> stages,retired;
  for(const auto& item:converted){const auto& t=item.first;
    if(!stages.count(t.root)){auto stage=t.root.parent_path().parent_path()/"texture-convert-backups"/t.root.filename()/(std::to_string(GetCurrentProcessId())+"-"+std::to_string(GetTickCount64()));fs::create_directories(stage);stages.emplace(t.root,stage);}
    auto stage=stages.at(t.root);auto index=std::to_string(changes.size());Change change{t.output,stage/(index+".converted.tmp"),stage/(index+".previous.tex"),fs::exists(t.output),false};
    if(change.existed)fs::copy_file(change.path,change.backup);Store(change.temporary,item.second);changes.push_back(std::move(change));
    if(t.previous!=t.output){auto old=t.previous.lexically_relative(t.root).generic_u8string(),now=t.output.lexically_relative(t.root).generic_u8string();
      Require(renames[t.root].emplace(old,now).second,"two PNG files rename the same declared .tex");
      if(fs::exists(t.previous)){auto backup=stage/t.previous.filename();fs::copy_file(t.previous,backup);retired.emplace(t.previous,backup);}
    }
  }
  // Publish pixels first and INI last. Every shared Texture.path using the old
  // file is updated, then its obsolete file is archived outside the Mod.
  for(const auto& row:renames){auto ini=row.first/"mod.ini",stage=stages.at(row.first);auto original=Read(ini,4*1024*1024);Change change{ini,stage/"mod.ini.converted.tmp",stage/"mod.ini.before",true,false};
    Store(change.backup,original);Store(change.temporary,RenameIni(original,row.second));changes.push_back(std::move(change));
  }
  std::vector<fs::path> removed;
  try{
    for(auto& change:changes){fs::create_directories(change.path.parent_path());Replace(change.temporary,change.path);change.committed=true;}
    for(const auto& row:retired){Replace(row.first,row.second);removed.push_back(row.first);}
  }catch(...){
    for(const auto& path:removed){auto temporary=retired.at(path).wstring()+L".restore.tmp";fs::copy_file(retired.at(path),temporary);Replace(temporary,path);}
    for(auto it=changes.rbegin();it!=changes.rend();++it)if(it->committed){if(it->existed){auto temporary=it->backup.wstring()+L".restore.tmp";fs::copy_file(it->backup,temporary);Replace(temporary,it->path);}else fs::remove(it->path);}
    throw;
  }
  for(const auto& item:converted){const auto& t=item.first;std::cout<<"OK "<<t.output.u8string()<<"\n  "<<t.width<<'x'<<t.height<<" RGBA32 mips="<<t.mips<<" bytes="<<item.second.size()<<"\n  backups: "<<stages.at(t.root).u8string()<<'\n';}
}
void Usage() {
  std::cout << "EFF PNG texture converter\n"
    << "Drag one or more PNG files onto this EXE. Installed plugin/mods/mod.ini supplies the settings.\n"
    << "Name PNG after Texture.path: body_<hash>.png or body.png; clothes_D.png, etc.\n"
    << "Output keeps the PNG filename with .tex; INI paths update together. Dimensions must match INI.\n"
    << "For another Mod, drag its mod.ini together with the PNG files.\n"
    << "CLI: eff_texture_convert [--no-pause] [--mod <Mod folder or mod.ini>] [--texture <TextureSection>] <PNG>...\n"
    << "Only PNG -> .tex; no native bundle rebuild or DLL/F10 changes.\n";
}
int wmain(int argc, wchar_t** argv) {
  SetConsoleOutputCP(CP_UTF8);
  DWORD processes[2]{};
  bool pause = GetConsoleProcessList(processes, 2) == 1;
  int status = 0;
  bool com = false;
  try {
    fs::path explicitIni;
    std::string selected;
    std::vector<fs::path> pngs;
    for (int i = 1; i < argc; ++i) {
      std::wstring arg = argv[i];
      if (arg == L"--no-pause") { pause = false; continue; }
      if (arg == L"--help" || arg == L"-h") { Usage(); pngs.clear(); break; }
      if(arg==L"--texture"){Require(++i<argc&&selected.empty(),"--texture needs one INI Texture section");selected=fs::path(argv[i]).u8string();continue;}
      if (arg == L"--mod") {
        Require(++i < argc && explicitIni.empty(), "--mod needs one Mod folder or mod.ini");
        explicitIni = argv[i];
        if (fs::is_directory(explicitIni)) explicitIni /= "mod.ini";
        continue;
      }
      fs::path path = fs::canonical(fs::path(arg));
      if (Lower(path.filename().u8string()) == "mod.ini") {
        Require(explicitIni.empty(), "only one mod.ini can be specified"); explicitIni = path;
      } else {
        Require(Lower(path.extension().u8string()) == ".png", "drag PNG files (and optionally mod.ini)");
        pngs.push_back(std::move(path));
      }
    }
    if (pngs.empty()) {
      if (argc == 1 || !explicitIni.empty()) Usage();
    } else {
      std::vector<Target> targets;
      if (!explicitIni.empty()) targets = ReadTargets(explicitIni);
      else {
        // A copied converter inside Mod/textures uses its nearest mod.ini.
        auto parent=Executable().parent_path();for(unsigned i=0;i<8&&!parent.empty();++i){if(fs::is_regular_file(parent/"mod.ini")){explicitIni=parent/"mod.ini";break;}auto next=parent.parent_path();if(next==parent)break;parent=next;}
        if(!explicitIni.empty())targets=ReadTargets(explicitIni);
        else{auto mods=Executable().parent_path().parent_path()/"mods";Require(fs::is_directory(mods),"cannot find plugin/mods; drag mod.ini with the PNG");
          for(const auto& entry:fs::directory_iterator(mods))if(entry.is_directory()&&fs::is_regular_file(entry.path()/"mod.ini")){auto found=ReadTargets(entry.path()/"mod.ini");targets.insert(targets.end(),found.begin(),found.end());}
        }
      }
      Check(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED), "cannot initialize Windows PNG decoder");
      com = true;
      ComPtr<IWICImagingFactory> factory;
      Check(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                            IID_PPV_ARGS(&factory)), "cannot create Windows PNG decoder");
      std::vector<std::pair<Target, Bytes>> converted;
      std::set<fs::path> outputs;
      std::set<fs::path> sources;
      auto started = std::chrono::steady_clock::now();
      for (const auto& png : pngs) {
        auto target = Resolve(png, targets,selected,pause);
        Require(outputs.insert(target.output).second, "multiple PNG inputs target the same .tex");
        Require(sources.insert(target.previous).second,"multiple PNG inputs target the same Texture declaration");
        auto bytes = EncodePixels(ReadPng(factory.Get(), png, target), target.mips);
        converted.emplace_back(std::move(target), std::move(bytes));
      }
      WriteAtomic(converted);
      std::cout << "Converted " << converted.size() << " texture(s), elapsedMs="
                << std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count()
                << ". Same-name .tex published; INI paths updated; material slot identities retained.\n";
    }
  } catch (const std::exception& e) {
    std::cerr << "ERROR: " << e.what() << '\n'; status = 1;
  }
  if (com) CoUninitialize();
  if (pause) { std::cout << "Press Enter to close..." << std::flush; std::cin.get(); }
  return status;
}

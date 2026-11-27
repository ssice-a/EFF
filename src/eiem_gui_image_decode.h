#pragma once
#include "eiem_custom_gui.h"
#include "eiem_dds_texture.h"
#include <d3d11.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <algorithm>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <optional>
#include <thread>
#pragma comment(lib,"ole32.lib")
#pragma comment(lib,"windowscodecs.lib")

namespace EiemGuiImageDecode {
template<class T> using Ptr=Microsoft::WRL::ComPtr<T>;
struct Layout {
  UINT width=0,height=0;
  DXGI_FORMAT format=DXGI_FORMAT_R8G8B8A8_UNORM;
  struct Mip {size_t offset;UINT pitch,width,height;};
  std::vector<Mip> mips;
  size_t extent=0;
  size_t Bytes(size_t first=0) const {return mips.empty()?0:extent-mips[first].offset;}
};
struct Image: Layout {
  std::vector<unsigned char> pixels;
};
inline void Check(HRESULT result,const char* message) {
  if(FAILED(result))throw std::runtime_error(message);
}
inline Image Decode(const std::filesystem::path& root,const std::string& relative) {
  auto data=EiemCustomGui::ReadFile(root,relative,24*1024*1024);
  Image image;
  if(data.size()>=4&&memcmp(data.data(),"DDS ",4)==0) {
    const auto layout=EiemDdsTexture::Parse(data,data.size());
    if(layout.format!=25||layout.width>4096||layout.height>4096||
       layout.width%4||layout.height%4)
      throw std::runtime_error("GUI DDS requires BC7, 4-pixel alignment and dimensions up to 4096");
    if(const auto alpha=EiemDirectory::Le(data,144,4);alpha==2||alpha==4)
      throw std::runtime_error("GUI DDS requires straight alpha");
    image.width=layout.width;image.height=layout.height;
    // The ImGui target is UNORM. Sample authored display colors as stored,
    // just as the PNG/JPEG path does.
    image.format=DXGI_FORMAT_BC7_UNORM;
    auto offset=layout.offset;
    for(UINT w=image.width,h=image.height,level=0;level<layout.mips;++level) {
      const UINT pitch=((w+3)/4)*16;
      image.mips.push_back({offset,pitch,w,h});
      offset+=size_t(pitch)*((h+3)/4);
      w=std::max(1u,w/2);h=std::max(1u,h/2);
    }
    image.extent=data.size();image.pixels=std::move(data);
    return image;
  }
  const bool png=data.size()>=8&&memcmp(data.data(),"\x89PNG\r\n\x1a\n",8)==0;
  const bool jpeg=data.size()>=3&&data[0]==0xff&&data[1]==0xd8&&data[2]==0xff;
  if(!png&&!jpeg)throw std::runtime_error("GUI images must be PNG, JPEG or DDS BC7");
  if(data.size()>8*1024*1024)throw std::runtime_error("GUI PNG/JPEG file exceeds 8 MiB");
  struct Com {
    HRESULT result=CoInitializeEx(nullptr,COINIT_MULTITHREADED);
    ~Com(){if(SUCCEEDED(result))CoUninitialize();}
  } com;
  if(com.result!=RPC_E_CHANGED_MODE)Check(com.result,"GUI image COM initialization failed");
  Ptr<IWICImagingFactory> factory;
  Check(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory)),"WIC unavailable");
  Ptr<IWICStream> stream;
  Check(factory->CreateStream(&stream),"Cannot create GUI image stream");
  Check(stream->InitializeFromMemory(data.data(),DWORD(data.size())),"Cannot read GUI image stream");
  Ptr<IWICBitmapDecoder> decoder;
  Check(factory->CreateDecoder(png?GUID_ContainerFormatPng:GUID_ContainerFormatJpeg,nullptr,&decoder),"GUI image codec unavailable");
  Check(decoder->Initialize(stream.Get(),WICDecodeMetadataCacheOnDemand),"Cannot decode GUI image");
  Ptr<IWICBitmapFrameDecode> bitmap;
  Check(decoder->GetFrame(0,&bitmap),"GUI image has no frame");
  Check(bitmap->GetSize(&image.width,&image.height),"GUI image dimensions unavailable");
  if(!image.width||!image.height||image.width>2048||image.height>2048)
    throw std::runtime_error("GUI image dimensions exceed 2048 pixels");
  Ptr<IWICFormatConverter> converter;
  Check(factory->CreateFormatConverter(&converter),"Cannot create GUI image converter");
  Check(converter->Initialize(bitmap.Get(),GUID_WICPixelFormat32bppRGBA,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom),"Cannot convert GUI image to RGBA");
  image.pixels.resize(size_t(image.width)*image.height*4);
  image.extent=image.pixels.size();
  image.mips.push_back({0,image.width*4,image.width,image.height});
  Check(converter->CopyPixels(nullptr,image.width*4,UINT(image.pixels.size()),image.pixels.data()),"Cannot read GUI image pixels");
  return image;
}

// The worker handles disk reads and CPU decode. Only the UI thread creates SRVs.
// Tickets are owned by the visible image entries, so old work cannot migrate
// into a reopened panel or a new F10 generation.
class Queue {
  struct Ticket {};
public:
  using Handle=std::shared_ptr<Ticket>;
  struct Result {Image image;std::string error;};
private:
  struct Request {std::weak_ptr<Ticket> ticket;std::filesystem::path root;std::string path;};
  struct Completed {std::weak_ptr<Ticket> ticket;Result result;};
  std::mutex mutex;
  std::condition_variable changed;
  std::deque<Request> pending;
  std::deque<Completed> ready;
  size_t readyBytes=0;
  bool stopping=false;
  std::thread worker;
  void Prune() {
    pending.erase(std::remove_if(pending.begin(),pending.end(),
      [](const Request& request){return request.ticket.expired();}),pending.end());
    for(auto it=ready.begin();it!=ready.end();) {
      if(it->ticket.expired()){readyBytes-=it->result.image.Bytes();it=ready.erase(it);}
      else ++it;
    }
  }
  void Run() {
    for(;;) {
      Request request;
      {
        std::unique_lock<std::mutex> lock(mutex);
        changed.wait(lock,[&]{return stopping||!pending.empty();});
        if(stopping)return;
        request=std::move(pending.front());pending.pop_front();
      }
      if(request.ticket.expired())continue;
      Result result;
      try{result.image=Decode(request.root,request.path);}
      catch(const std::exception& error){result.error=error.what();}
      const auto bytes=result.image.Bytes();
      std::unique_lock<std::mutex> lock(mutex);
      changed.wait(lock,[&]{
        return stopping||request.ticket.expired()||(ready.size()<8&&bytes<=32*1024*1024-readyBytes);
      });
      if(stopping)return;
      if(request.ticket.expired())continue;
      ready.push_back({std::move(request.ticket),std::move(result)});
      readyBytes+=bytes;
    }
  }
public:
  ~Queue() {
    {std::lock_guard<std::mutex> lock(mutex);stopping=true;}
    changed.notify_one();
    if(worker.joinable())worker.join();
  }
  Handle Submit(const std::filesystem::path& root,const std::string& path) {
    std::lock_guard<std::mutex> lock(mutex);
    if(pending.size()>=64)return {};
    if(!worker.joinable())worker=std::thread([this]{Run();});
    auto ticket=std::make_shared<Ticket>();
    pending.push_back({ticket,root,path});changed.notify_one();
    return ticket;
  }
  template<class Accept> std::optional<Result> Take(const Handle& ticket,Accept accept) {
    std::lock_guard<std::mutex> lock(mutex);
    for(auto it=ready.begin();it!=ready.end();++it) {
      if(it->ticket.lock()!=ticket)continue;
      if(it->result.error.empty()&&!accept(it->result.image))return {};
      readyBytes-=it->result.image.Bytes();
      auto result=std::move(it->result);
      ready.erase(it);changed.notify_one();
      return result;
    }
    return {};
  }
  void DiscardCanceled() {
    std::lock_guard<std::mutex> lock(mutex);Prune();changed.notify_one();
  }
};
}

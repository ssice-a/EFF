#pragma once
#include "eiem_gui_image_decode.h"
#include "eiem_lua_ui.h"

// One cache per host window, with ownership scoped to each open Mod session.
class EiemGuiImages {
  static constexpr size_t MiB=1024*1024;
  static constexpr size_t panelTextures=512,panelBytes=64*MiB,panelEntries=2048;
  static constexpr size_t totalTextures=2048,totalBytes=128*MiB;
  static constexpr size_t uploadTextures=4,uploadBytes=8*MiB;
  static constexpr size_t panelQueue=8;
  struct Usage {
    size_t bytes=0,count=0;
    bool Fits(size_t length,size_t maxCount,size_t maxBytes) const {
      return count<maxCount&&bytes<=maxBytes&&length<=maxBytes-bytes;
    }
    void Add(size_t length) {bytes+=length;++count;}
    void Remove(size_t length) {bytes-=length;--count;}
    void Remove(Usage amount) {bytes-=amount.bytes;count-=amount.count;}
  };
  struct Texture {
    EiemGuiImageDecode::Ptr<ID3D11ShaderResourceView> view;
    EiemGuiImageDecode::Queue::Handle ticket;
    std::string error;
    EiemGuiImageDecode::Layout layout;
    size_t mip=0;
    int used=-1,requested=-1;
    ImVec2 nextSample{};
    size_t Bytes() const {return layout.Bytes(mip);}
    EiemLuaUi::ImageInfo Info() const {
      return {reinterpret_cast<ImTextureID>(view.Get()),float(layout.width),float(layout.height),error.c_str()};
    }
  };
  struct Panel {
    std::map<std::string,Texture> textures;
    Usage resident,usedThisFrame;
    size_t pending=0;
  };
  std::map<uint64_t,Panel> panels;
  EiemGuiImageDecode::Queue decode;
  Usage resident,uploaded;
  size_t frameBytes=panelBytes,frameTextures=panelTextures;
  int frame=-1;

  size_t SelectMip(const Panel& panel,const EiemGuiImageDecode::Layout& layout,ImVec2 size) const {
    const auto end=layout.mips.size();
    if(!end)return 0;
    const float scale=size.x>0||size.y>0?std::max(size.x/layout.width,size.y/layout.height):1.f;
    size_t best=end;
    for(size_t i=0;i<end;++i) {
      const auto& mip=layout.mips[i];
      // D3D11 requires a BC7 resource's top-level dimensions to be block-aligned.
      if(layout.format==DXGI_FORMAT_BC7_UNORM&&(mip.width%4||mip.height%4))continue;
      if(!panel.usedThisFrame.Fits(layout.Bytes(i),frameTextures,frameBytes))continue;
      if(best!=end&&mip.width<layout.width*scale&&mip.height<layout.height*scale)break;
      best=i;
    }
    return best;
  }
  Texture* OldestUnused(Panel& panel) const {
    Texture* oldest=nullptr;
    for(auto& [path,texture]:panel.textures)
      if(texture.view&&texture.used!=frame&&(!oldest||texture.used<oldest->used))
        oldest=&texture;
    return oldest;
  }
  void Evict(Panel& panel,Texture* texture) {
    if(!texture)throw std::logic_error("GUI image cache accounting");
    resident.Remove(texture->Bytes());
    panel.resident.Remove(texture->Bytes());
    texture->view.Reset();
  }
  void MakeRoom(Panel& panel,size_t bytes) {
    // Draw commands keep raw SRV pointers until RenderDrawData returns.
    while(!panel.resident.Fits(bytes,panelTextures,panelBytes))
      Evict(panel,OldestUnused(panel));
    while(!resident.Fits(bytes,totalTextures,totalBytes)) {
      Panel* owner=nullptr;
      Texture* oldest=nullptr;
      for(auto& [session,candidatePanel]:panels)
        if(auto* candidate=OldestUnused(candidatePanel);candidate&&(!oldest||candidate->used<oldest->used)) {
          owner=&candidatePanel;
          oldest=candidate;
        }
      if(!owner)throw std::logic_error("GUI image cache accounting");
      Evict(*owner,oldest);
    }
  }
  void Upload(ID3D11Device* device,Panel& panel,Texture& texture,const EiemGuiImageDecode::Image& image,size_t mip) {
    const size_t bytes=image.Bytes(mip);
    std::vector<D3D11_SUBRESOURCE_DATA> inputs;
    inputs.reserve(image.mips.size()-mip);
    for(size_t i=mip;i<image.mips.size();++i)
      inputs.push_back({image.pixels.data()+image.mips[i].offset,image.mips[i].pitch,0});
    D3D11_TEXTURE2D_DESC desc{};
    desc.Width=image.mips[mip].width;desc.Height=image.mips[mip].height;desc.MipLevels=UINT(inputs.size());desc.ArraySize=1;
    desc.Format=image.format;desc.SampleDesc.Count=1;
    desc.Usage=D3D11_USAGE_IMMUTABLE;desc.BindFlags=D3D11_BIND_SHADER_RESOURCE;
    if(texture.view)Evict(panel,&texture);
    MakeRoom(panel,bytes);
    uploaded.Add(bytes);
    EiemGuiImageDecode::Ptr<ID3D11Texture2D> gpu;
    if(FAILED(device->CreateTexture2D(&desc,inputs.data(),&gpu))||
       FAILED(device->CreateShaderResourceView(gpu.Get(),nullptr,&texture.view))) {
      texture.view.Reset();
      texture.error="Cannot create GUI texture on this graphics device";
      return;
    }
    texture.mip=mip;
    panel.resident.Add(bytes);
    resident.Add(bytes);
  }
public:
  void BeginFrame(size_t openPanels) {
    frame=ImGui::GetFrameCount();
    const auto share=std::max(size_t(1),openPanels);
    frameBytes=std::min(panelBytes,totalBytes/share);
    frameTextures=std::min(panelTextures,totalTextures/share);
    uploaded={};
    for(auto& row:panels)row.second.usedThisFrame={};
  }
  void EndFrame() {
    bool canceled=false;
    for(auto& [session,panel]:panels)if(panel.pending)
      for(auto& [path,texture]:panel.textures)if(texture.ticket&&texture.requested!=frame) {
        texture.ticket.reset();--panel.pending;canceled=true;
      }
    if(canceled)decode.DiscardCanceled();
  }
  void Release(uint64_t session) {
    const auto it=panels.find(session);
    if(it==panels.end())return;
    const bool pending=it->second.pending!=0;
    resident.Remove(it->second.resident);
    panels.erase(it);
    if(pending)decode.DiscardCanceled();
  }
  void Clear() {
    panels.clear();
    resident={};
    decode.DiscardCanceled();
  }
  EiemLuaUi::ImageInfo Load(ID3D11Device* device,uint64_t session,const std::filesystem::path& root,const char* relative,ImVec2 sampleSize={}) {
    auto& panel=panels[session];
    auto& textures=panel.textures;
    auto found=textures.find(relative);
    if(found==textures.end()) {
      if(textures.size()>=panelEntries) {
        auto oldest=textures.end();
        for(auto it=textures.begin();it!=textures.end();++it)
          if(!it->second.view&&!it->second.ticket&&it->second.requested!=frame&&
             (oldest==textures.end()||it->second.requested<oldest->second.requested))
            oldest=it;
        if(oldest==textures.end())return {};
        textures.erase(oldest);
      }
      found=textures.try_emplace(relative).first;
    }
    auto& texture=found->second;
    // Repeated draws share a single SRV. Remember a larger use for the next
    // frame, after ImGui has finished using this frame's pointer.
    bool firstRequest=texture.requested!=frame;
    const auto previous=texture.requested==frame-1?texture.nextSample:ImVec2{};
    if(sampleSize.x==0&&sampleSize.y==0)sampleSize={4096,4096};
    if(firstRequest)texture.nextSample=sampleSize;
    else {
      texture.nextSample.x=std::max(texture.nextSample.x,sampleSize.x);
      texture.nextSample.y=std::max(texture.nextSample.y,sampleSize.y);
    }
    if(firstRequest)sampleSize={std::max(previous.x,sampleSize.x),std::max(previous.y,sampleSize.y)};
    texture.requested=frame;
    if(texture.used==frame||(!texture.view&&!texture.error.empty()))return texture.Info();
    const auto desired=SelectMip(panel,texture.layout,sampleSize);
    auto currentFits=[&] {return panel.usedThisFrame.Fits(texture.Bytes(),frameTextures,frameBytes);};
    if(texture.ticket) {
      size_t preparedMip=0;
      auto ready=decode.Take(texture.ticket,[&](const EiemGuiImageDecode::Image& image){
        preparedMip=SelectMip(panel,image,sampleSize);
        return preparedMip==image.mips.size()||uploaded.count==0||
               uploaded.Fits(image.Bytes(preparedMip),uploadTextures,uploadBytes);
      });
      if(ready) {
        texture.ticket.reset();--panel.pending;
        texture.error=std::move(ready->error);
        if(texture.error.empty()) {
          if(preparedMip<ready->image.mips.size())
            Upload(device,panel,texture,ready->image,preparedMip);
          else if(texture.view)Evict(panel,&texture);
          texture.layout=std::move(ready->image);
        }
      }
    }else if(texture.error.empty()&&(texture.layout.mips.empty()||desired<texture.layout.mips.size())&&
             (!texture.view||desired!=texture.mip)&&panel.pending<panelQueue) {
      texture.ticket=decode.Submit(root,found->first);
      if(texture.ticket)++panel.pending;
    }
    if(texture.view) {
      if(!currentFits())
        return {ImTextureID_Invalid,float(texture.layout.width),float(texture.layout.height)};
      texture.used=frame;
      panel.usedThisFrame.Add(texture.Bytes());
    }
    return texture.Info();
  }
};

#pragma once
#include "../deps/imgui/imgui.h"
#include "../deps/imgui/imgui_internal.h"
#include <algorithm>
#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// The native host, Ins and all author panels share one ImGui context. Retire
// author records between frames, after DX11 has consumed their draw lists.
static_assert(IMGUI_VERSION_NUM==19282,"Review author GUI frame-boundary cleanup after upgrading ImGui");
class EiemGuiState {
  static constexpr int retainedRecords=512;
  struct Header { uint64_t session; int frame; };
  std::unordered_map<ImGuiWindow*,uint64_t> windows;
  std::unordered_map<ImGuiID,uint64_t> tables,tabBars;
  std::unordered_map<ImGuiWindow*,std::unordered_map<ImGuiID,Header>> headers;
  std::unordered_set<uint64_t> trackedSessions;

  uint64_t Owner(ImGuiWindow* window) const {
    for(;window;window=window->ParentWindow) {
      auto owner=windows.find(window);
      if(owner!=windows.end())return owner->second;
    }
    return 0;
  }
  template<class T,class Predicate>
  static void Erase(ImVector<T>& list,Predicate remove) {
    if(!list.empty())list.resize(int(std::remove_if(list.begin(),list.end(),remove)-list.begin()));
  }
  template<class T,class Predicate>
  static void RetirePool(std::unordered_map<ImGuiID,uint64_t>& owners,ImPool<T>& pool,Predicate retire) {
    bool removed=false;
    for(auto it=owners.begin();it!=owners.end();) {
      auto* item=pool.GetByKey(it->first);
      if(item&&retire(item,it->second)){removed=true;it=owners.erase(it);}
      else ++it;
    }
    // ImPool puts freed slots on a freelist, but Remove leaves the old keys.
    if(removed)Erase(pool.Map.Data,[](const auto& pair){return pair.val_i==-1;});
  }
public:
  void Window(ImGuiWindow* window,uint64_t session){
    auto [it,created]=windows.try_emplace(window,session);
    if(created||it->second!=session){it->second=session;trackedSessions.insert(session);}
  }
  void Table(ImGuiID id,uint64_t session){tables.emplace(id,session);}
  void TabBar(ImGuiID id,uint64_t session){tabBars.emplace(id,session);}
  void CollapsingHeader(ImGuiWindow* window,ImGuiID id,uint64_t session) {
    // ImGui only adds storage for a header after it has been interacted with.
    if(window->StateStorage.GetInt(id,-1)!=-1)
      headers[window].insert_or_assign(id,Header{session,ImGui::GetFrameCount()});
  }

  void Collect(const std::vector<uint64_t>& openSessions) {
    if(windows.empty()&&tables.empty()&&tabBars.empty()&&headers.empty())return;
    auto& g=*ImGui::GetCurrentContext();
    IM_ASSERT(!g.WithinFrameScope);
    const auto closed=[&](uint64_t session){
      return std::find(openSessions.begin(),openSessions.end(),session)==openSessions.end();
    };
    const auto stale=[&](int last,int poolSize){
      // Keep state for temporarily hidden controls. Under pressure, reclaim
      // every unused record before a fresh frame can allocate more.
      return last<g.FrameCount&&poolSize>retainedRecords;
    };
    bool sessionsChanged=false;
    for(auto it=trackedSessions.begin();it!=trackedSessions.end();) {
      if(closed(*it)){sessionsChanged=true;it=trackedSessions.erase(it);}
      else ++it;
    }
    std::unordered_set<ImGuiWindow*> retired;
    if(sessionsChanged||g.Windows.Size>retainedRecords)for(auto* window:g.Windows) {
      const auto owner=Owner(window);
      if(owner&&(closed(owner)||stale(window->LastFrameActive,g.Windows.Size)))retired.insert(window);
    }
    const auto gone=[&](ImGuiWindow* window){return retired.count(window)!=0;};
    for(auto it=headers.begin();it!=headers.end();) {
      if(gone(it->first)){it=headers.erase(it);continue;}
      auto& records=it->second;
      const int count=int(records.size());
      if(sessionsChanged||count>retainedRecords)Erase(it->first->StateStorage.Data,[&](const auto& pair){
        auto row=records.find(pair.key);
        if(row==records.end()||(!closed(row->second.session)&&!stale(row->second.frame,count)))return false;
        records.erase(row);
        return true;
      });
      if(records.empty())it=headers.erase(it);else ++it;
    }
    const int tableCount=g.Tables.GetAliveCount(),tabBarCount=g.TabBars.GetAliveCount();
    if(sessionsChanged||!retired.empty()||tableCount>retainedRecords)RetirePool(tables,g.Tables,[&](ImGuiTable* table,uint64_t owner){
      if(!closed(owner)&&!gone(table->OuterWindow)&&!gone(table->InnerWindow)&&
         !stale(table->LastFrameActive,tableCount))return false;
      ImGui::TableRemove(table);
      return true;
    });
    if(sessionsChanged||!retired.empty()||tabBarCount>retainedRecords)RetirePool(tabBars,g.TabBars,[&](ImGuiTabBar* bar,uint64_t owner){
      if(!closed(owner)&&!gone(bar->Window)&&!stale(bar->CurrFrameVisible,tabBarCount))return false;
      ImGui::TabBarRemove(bar);
      return true;
    });
    if(retired.empty())return;

    for(int i=0;i<g.OpenPopupStack.Size;++i) {
      auto& popup=g.OpenPopupStack[i];
      if(gone(popup.Window)||(!popup.Window&&gone(popup.RestoreNavWindow))) {
        ImGui::ClosePopupToLevel(i,false);
        break;
      }
      if(gone(popup.RestoreNavWindow))popup.RestoreNavWindow=nullptr;
    }
    if(gone(g.ActiveIdWindow)) {
      if(g.DragDropActive&&g.DragDropPayload.SourceId==g.ActiveId)ImGui::ClearDragDrop();
      if(g.InputTextState.ID==g.ActiveId) {
        g.InputTextState.ID=0;
        g.InputTextState.ClearFreeMemory();
        g.InputTextDeactivatedState.ClearFreeMemory();
        g.InputTextReactivateId=g.TempInputId=0;
      }
      ImGui::ClearActiveID();
      g.DeactivatedItemData={};
    }
    if(gone(g.HoveredWindow)) {
      g.HoveredId=g.HoveredIdPreviousFrame=0;
      g.HoverItemDelayId=g.HoverItemDelayIdPreviousFrame=0;
    }
    if(gone(g.WheelingWindow)) {
      g.WheelingWindowStartFrame=g.WheelingWindowScrolledFrame=-1;
      g.WheelingWindowReleaseTimer=0;
      g.WheelingWindowWheelRemainder={};
    }
    for(auto** pointer:{&g.HoveredWindow,&g.HoveredWindowUnderMovingWindow,&g.HoveredWindowBeforeClear,
                        &g.MovingWindow,&g.WheelingWindow,&g.NavWindowingTarget,&g.NavWindowingTargetAnim,
                        &g.NavWindowingListWindow,&g.TooltipPreviousWindow,&g.LogWindow})
      if(gone(*pointer))*pointer=nullptr;
    for(auto* result:{&g.NavInitResult,&g.NavMoveResultLocal,&g.NavMoveResultLocalVisible,
                      &g.NavMoveResultOther,&g.NavTabbingResultFirst})
      if(gone(result->Window))result->Clear();

    for(auto* window:g.Windows)if(!gone(window)) {
      Erase(window->DC.ChildWindows,gone);
      for(auto** pointer:{&window->ParentWindow,&window->ParentWindowInBeginStack,&window->ParentWindowForFocusRoute,
                          &window->NavLastChildNavWindow})
        if(gone(*pointer))*pointer=nullptr;
      for(auto** pointer:{&window->RootWindow,&window->RootWindowPopupTree,&window->RootWindowForTitleBarHighlight,
                          &window->RootWindowForNav})
        if(gone(*pointer))*pointer=window;
    }
    Erase(g.WindowsFocusOrder,gone);
    for(int i=0;i<g.WindowsFocusOrder.Size;++i)g.WindowsFocusOrder[i]->FocusOrder=short(i);
    Erase(g.Windows,gone);
    Erase(g.WindowsById.Data,[&](const auto& pair){return gone(static_cast<ImGuiWindow*>(pair.val_p));});
    g.WindowsTempSortBuffer.resize(0);
    for(auto* viewport:g.Viewports) {
      viewport->DrawDataP.Clear();
      viewport->DrawDataBuilder.LayerData1.resize(0);
    }
    if(gone(g.NavWindow)) {
      g.NavNextActivateId=0;
      g.NavMoveForwardToNextFrame=false;
      ImGui::FocusTopMostWindowUnderOne(nullptr,nullptr,nullptr,ImGuiFocusRequestFlags_RestoreFocusedChild);
    }
    for(auto* window:retired) {
      windows.erase(window);
      IM_DELETE(window);
    }
  }
};

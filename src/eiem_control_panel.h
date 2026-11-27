#pragma once
#include "eiem_resource_directory.h"
#include "eiem_shape_animation_player.h"
#include <atomic>
#include <deque>
#include <mutex>

// The panel never reads live control maps or calls Unity. Its commands are
// consumed once per Unity frame against both graph and selection revisions.
namespace EiemControlPanel {
inline std::string ModLabel(const EiemDirectory::Package& p){
 auto folder=p.root.filename().u8string();const auto& author=EiemDirectory::DeclaredId(p);
 return folder.empty()?author:folder==author?folder:folder+"  ("+author+")";
}
struct Snapshot {
  uint64_t revision=0, scope=0;
  std::string selected;
  std::map<std::string,bool> keySwitchEnabled;
  std::map<std::string,bool> customGuiEnabled;
  std::shared_ptr<const std::vector<EiemDirectory::Package>> graph;
  std::map<std::string,EiemVariables> values;
  std::map<std::string,EiemVariables> effective;
  std::map<std::string,std::string> errors;
  std::map<std::string,std::map<std::string,EiemShapeAnimation::Playback>> animations;
};
enum class Action { Select, Keyboard, Invoke, Hold, Value, AnimationPlay, AnimationPause, AnimationStop, AnimationSpeed, AnimationReset, InvokeAction, GuiEnabled, GuiRequests };
struct Command {
  uint64_t revision=0, scope=0;
  Action action=Action::Select;
  std::string package, name;
  double value=0;
  EiemCustomGui::Requests requests;
};
struct Shortcut {
  EiemKeyChord chord{};
  bool held=false, initialized=false;
  bool Press(EiemKeyChord next,bool physical,bool focused) noexcept {
    bool edge=initialized && next==chord && physical && !held && focused;
    chord=next;held=physical;initialized=true;return edge;
  }
};
class Store {
  static constexpr size_t maxCommands=128;
  static constexpr size_t maxOpenGuis=8;
  static constexpr size_t maxBatchesPerGui=12;
  static constexpr size_t maxGuiBatches=maxOpenGuis*maxBatchesPerGui;
  std::shared_ptr<const Snapshot> snapshot;
  std::mutex mutex;
  std::deque<Command> commands;
  std::map<std::string,uint64_t> customOpen;
  std::atomic_bool anyCustomOpen{false};
  uint64_t nextOpen=0;
public:
  std::atomic_bool open{false}, stopping{false}, failed{false};
  std::atomic<HWND> window{nullptr}, gameWindow{nullptr};
  // Assigned before the UI worker starts; UI diagnostics never call Unity.
  std::function<void(const std::string&)> diagnostic;
#ifdef EIEM_PANEL_TEST
  std::function<HRESULT(const char*)> graphicsFailure;
#endif
  void Report(const std::string& message) const {if(diagnostic)diagnostic(message);}
  std::shared_ptr<const Snapshot> Read() const { return std::atomic_load(&snapshot); }
  void Publish(std::shared_ptr<const Snapshot> next) noexcept { std::atomic_store(&snapshot,std::move(next)); }
  bool ToggleGui(const Snapshot& view,const std::string& id) {
    std::lock_guard<std::mutex> lock(mutex);
    auto current=Read();auto enabled=view.customGuiEnabled.find(id);
    if (!current || current->revision!=view.revision || enabled==view.customGuiEnabled.end() || !enabled->second ||
        !current->customGuiEnabled.count(id) || !current->customGuiEnabled.at(id)) return false;
    if(customOpen.erase(id)){anyCustomOpen=!customOpen.empty();return true;}
    if(customOpen.size()>=maxOpenGuis)return false;
    customOpen.emplace(id,++nextOpen);anyCustomOpen=true;
    return true;
  }
  void CloseGui(const std::string& id) {
    std::lock_guard<std::mutex> lock(mutex);customOpen.erase(id);anyCustomOpen=!customOpen.empty();
  }
  uint64_t GuiSession(const std::string& id) {
    std::lock_guard<std::mutex> lock(mutex);
    auto session=customOpen.find(id);
    return session==customOpen.end()?0:session->second;
  }
  std::map<std::string,uint64_t> OpenGuis() {
    if(!anyCustomOpen)return {};
    std::lock_guard<std::mutex> lock(mutex);return customOpen;
  }
  bool Visible() const {return open||anyCustomOpen;}
  void Close() {
    open=false;
    if(!anyCustomOpen)return;
    std::lock_guard<std::mutex> lock(mutex);customOpen.clear();anyCustomOpen=false;
  }
  bool Submit(Command command) {
    std::lock_guard<std::mutex> lock(mutex);
    // A dragged slider replaces its previous queued position, without
    // crossing a selection command or another action.
    if (!commands.empty()) {
      auto& last=commands.back();
      if ((command.action==Action::Value||command.action==Action::AnimationSpeed) && last.action==command.action &&
          last.revision==command.revision && last.scope==command.scope &&
          last.package==command.package && last.name==command.name) {
        last.value=command.value;return true;
      }
      if(command.action==Action::GuiRequests&&last.action==command.action&&last.revision==command.revision&&last.scope==command.scope&&
         last.package==command.package&&command.requests.size()==1&&last.requests.size()==1&&
         command.requests[0].operation==EiemCustomGui::Operation::Set&&last.requests[0].operation==command.requests[0].operation&&
         last.requests[0].name==command.requests[0].name){
        last.requests[0].value=command.requests[0].value;return true;
      }
    }
    if(commands.size()>=maxCommands)return false;
    if(command.action==Action::GuiRequests&&commands.size()>=maxBatchesPerGui){
      // A stalled game thread can collect commands from several author frames.
      // Preserve FIFO order while reserving space for other panels and Ins.
      size_t guiBatches=0,sessionBatches=0;
      for(const auto& queued:commands){
        if(queued.action!=Action::GuiRequests)continue;
        if(++guiBatches>=maxGuiBatches)return false;
        if(queued.scope==command.scope&&++sessionBatches>=maxBatchesPerGui)return false;
      }
    }
    commands.push_back(std::move(command));return true;
  }
  std::deque<Command> Drain() {
    std::deque<Command> result;
    std::lock_guard<std::mutex> lock(mutex);result.swap(commands);return result;
  }
};
class GuiShortcuts {
  struct Entry { std::string id; EiemKeyChord key; bool held=false; };
  std::vector<Entry> entries;
  uint64_t revision=uint64_t(-1);
public:
  void Poll(Store& store,const EiemKeyChord& ins,const EiemKeyChord& reload,bool focused,
            bool (*sample)(const EiemKeyChord&)) {
    auto view=store.Read();
    if(!view||!view->graph)return;
    if(view->revision!=revision){
      revision=view->revision;entries.clear();
      for(const auto& p:*view->graph)if(p.customGui.key.vk&&p.customGui.error.empty()&&p.loadError.empty())
        entries.push_back({p.id,p.customGui.key,sample(p.customGui.key)});
    }
    for(auto& item:entries){
      bool down=sample(item.key),pressed=down&&!item.held;item.held=down;
      auto enabled=view->keySwitchEnabled.find(item.id);
      if(pressed&&focused&&item.key.vk!=ins.vk&&item.key.vk!=reload.vk&&
         enabled!=view->keySwitchEnabled.end()&&enabled->second)store.ToggleGui(*view,item.id);
    }
  }
};
} // namespace EiemControlPanel

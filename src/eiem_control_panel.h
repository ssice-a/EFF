#pragma once
#include "eiem_resource_directory.h"
#include "eiem_shape_animation_player.h"
#include <atomic>
#include <deque>
#include <mutex>

// The panel never reads live control maps or calls Unity. Its commands are
// consumed once per Unity frame against both graph and selection revisions.
namespace EiemControlPanel {
struct Snapshot {
  uint64_t revision=0, scope=0;
  std::string selected;
  std::map<std::string,bool> keySwitchEnabled;
  std::shared_ptr<const std::vector<EiemDirectory::Package>> graph;
  std::map<std::string,EiemVariables> values;
  std::map<std::string,std::map<std::string,EiemShapeAnimation::Playback>> animations;
};
enum class Action { Select, Keyboard, Invoke, Hold, Value, AnimationPlay, AnimationPause, AnimationStop, AnimationSpeed, AnimationReset };
struct Command {
  uint64_t revision=0, scope=0;
  Action action=Action::Select;
  std::string package, name;
  double value=0;
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
  std::shared_ptr<const Snapshot> snapshot;
  std::mutex mutex;
  std::deque<Command> commands;
public:
  std::atomic_bool open{false}, stopping{false}, failed{false};
  std::atomic<HWND> window{nullptr}, gameWindow{nullptr};
  std::shared_ptr<const Snapshot> Read() const { return std::atomic_load(&snapshot); }
  void Publish(std::shared_ptr<const Snapshot> next) noexcept { std::atomic_store(&snapshot,std::move(next)); }
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
    }
    if (commands.size()>=128) return false;
    commands.push_back(std::move(command));return true;
  }
  std::deque<Command> Drain() {
    std::deque<Command> result;
    std::lock_guard<std::mutex> lock(mutex);result.swap(commands);return result;
  }
};
} // namespace EiemControlPanel

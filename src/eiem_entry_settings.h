#pragma once
#include "eiem_global_settings.h"
#include <atomic>
#include <filesystem>
#include <memory>
#include <stdexcept>
class EiemEntrySettings {
  std::shared_ptr<const EiemGlobalConfig> published=std::make_shared<const EiemGlobalConfig>();
 public:
  EiemGlobalConfig Get() const {return *std::atomic_load(&published);}
  using Snapshot=std::shared_ptr<const EiemGlobalConfig>;
  static Snapshot Prepare(const EiemGlobalConfig& value) {return std::make_shared<const EiemGlobalConfig>(value);}
  void Publish(const EiemGlobalConfig& value) {PublishPrepared(Prepare(value));}
  void PublishPrepared(const Snapshot& value) noexcept {std::atomic_store(&published,value);}
  static EiemGlobalConfig Read(const std::filesystem::path& game) {
    auto path=game/"plugin/eff.ini";
    if (!std::filesystem::exists(path)) {
      std::filesystem::create_directories(path.parent_path());
      std::ofstream out(path,std::ios::binary);out<<kEiemDefaultGlobalConfig;
      if (!out) throw std::runtime_error("cannot create default eff.ini");
    }
    std::ifstream input(path,std::ios::binary);EiemGlobalConfig next;std::string error;
    if (!input || !EiemParseGlobalConfig(input,&next,error)) throw std::runtime_error("eff.ini: "+(error.empty()?std::string("read failed"):error));
    return next;
  }
};

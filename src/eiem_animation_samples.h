#pragma once
#include <array>
#include <atomic>
#include <functional>
#include <exception>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

// Immutable sample payloads are shared by clips and installations. INI owns
// bindings; a real game owner permits sampling. Neither declaration parsing
// nor preparing an autoplay clock opens a sample file.
namespace EiemAnimationSamples {
struct Data {
  std::vector<std::vector<float>> scalar;
  std::vector<std::vector<std::array<float,10>>> bone;
};
struct Activity { std::atomic<size_t> owners{0}; };
struct Owner {
  std::shared_ptr<Activity> activity;
  explicit Owner(std::shared_ptr<Activity> value):activity(std::move(value)){++activity->owners;}
  ~Owner(){--activity->owners;}
  Owner(const Owner&)=delete;
  Owner& operator=(const Owner&)=delete;
};
class Source {
  mutable std::mutex mutex_;
  mutable std::weak_ptr<const Data> cached_;
  mutable std::exception_ptr failure_;
public:
  std::string identity;
  std::shared_ptr<Activity> activity;
  std::function<Data()> read;
  std::vector<std::function<void(const Data&)>> validate;
  bool Active()const noexcept{return !activity||activity->owners.load()!=0;}
  std::shared_ptr<const Data> Open()const{
    std::lock_guard<std::mutex> lock(mutex_);
    if(failure_)std::rethrow_exception(failure_);
    auto result=cached_.lock();
    if(!result)try{auto decoded=read();for(const auto& check:validate)check(decoded);result=std::make_shared<const Data>(std::move(decoded));cached_=result;}catch(...){failure_=std::current_exception();throw;}
    return result;
  }
  bool Loaded()const{std::lock_guard<std::mutex> lock(mutex_);return !cached_.expired();}
};
}

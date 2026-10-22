#pragma once
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <vector>

// Clock/sampling without native objects. Shapes and scalar material tracks can
// share this kernel while retaining separate binding and write ownership.
namespace EiemSampledAnimation {
struct Playback {bool playing=false,paused=false,held=false;double time=0;};
enum class Step {Unchanged,Advanced,Finished};
inline Step Advance(Playback& state,double duration,bool loop,double speed,double elapsed){
 if(!std::isfinite(elapsed)||elapsed<0||!std::isfinite(speed)||speed<=0||!std::isfinite(duration)||duration<=0)
  throw std::runtime_error("sampled animation clock domain differs");
 if(!state.playing||state.paused||elapsed==0)return Step::Unchanged;
 double step=elapsed*speed;if(!std::isfinite(step))throw std::runtime_error("animation time overflow");
 if(!loop){if(step>=duration-state.time)return Step::Finished;state.time+=step;return Step::Advanced;}
 // Reduce before addition: two individually finite times can still overflow.
 if(step>=duration)step=std::fmod(step,duration);
 if(state.time>=duration-step)state.time-=duration-step;else state.time+=step;
 return Step::Advanced;
}
inline double Sample(const std::vector<float>& samples,double phase){
 // Sample count/finiteness are checked once when author data is loaded.
 double position=std::clamp(phase,0.0,1.0)*double(samples.size()-1);auto i=size_t(position),j=std::min(i+1,samples.size()-1);
 return double(samples[i])+(double(samples[j])-double(samples[i]))*(position-double(i));
}
} // namespace EiemSampledAnimation

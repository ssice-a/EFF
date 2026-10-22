#pragma once
#include "eiem_resource_directory.h"
#include "eiem_sampled_animation.h"
namespace EiemShapeAnimation {
using Playback=EiemSampledAnimation::Playback;
struct Player {
 std::map<std::string,Playback> clips;
 std::map<std::string,double> output;
 std::map<std::string,std::set<std::string>> conflicts;
 std::map<std::string,std::set<std::string>> ownedTracks,trackOwners;
private:
 // IDs, not pointers, survive candidate copies, swaps and F10 publication.
 std::set<std::string> active;
 size_t lastVisited=0;
 void StopId(const std::string& id){auto& state=clips.at(id);if(!state.playing)return;state.playing=state.paused=false;state.time=0;active.erase(id);for(const auto& track:ownedTracks.at(id))output.erase(track);}
public:
 size_t ActiveCount()const noexcept{return active.size();}
 size_t LastVisited()const noexcept{return lastVisited;}
 void Stop(const EiemDirectory::ShapeAnimation& clip){StopId(clip.id);}
 void Pause(const EiemDirectory::ShapeAnimation& clip){auto& state=clips.at(clip.id);if(!state.playing)return;state.paused=!state.paused;if(state.paused)active.erase(clip.id);else active.insert(clip.id);}
 bool Manual(const EiemDirectory::Package&,const std::string& variable){bool changed=false;auto owners=trackOwners.find(variable);if(owners==trackOwners.end())return false;
  for(const auto& id:owners->second)if(clips.at(id).playing){StopId(id);changed=true;}return changed;}
 void Evaluate(const EiemDirectory::ShapeAnimation& clip){const auto& state=clips.at(clip.id);if(!state.playing)return;double phase=state.time/clip.duration;
  for(const auto& track:clip.tracks)output[track.first]=EiemSampledAnimation::Sample(track.second,phase);}
 void Play(const EiemDirectory::ShapeAnimation& clip){auto& state=clips.at(clip.id);if(state.playing&&state.paused){state.paused=false;active.insert(clip.id);return;}
  auto found=conflicts.find(clip.id);if(found!=conflicts.end())for(const auto& id:found->second)StopId(id);
  state.playing=true;state.paused=false;state.time=0;active.insert(clip.id);Evaluate(clip);}
 bool Advance(const EiemDirectory::Package& p,const EiemVariables& values,double elapsed){lastVisited=0;
  EiemDirectory::Require(std::isfinite(elapsed)&&elapsed>=0,"animation frame delta invalid");if(elapsed==0||active.empty())return false;
  bool changed=false;for(auto it=active.begin();it!=active.end();){const auto& clip=p.shapeAnimations.at(*it++);++lastVisited;auto& state=clips.at(clip.id);
   auto step=EiemSampledAnimation::Advance(state,clip.duration,clip.loop,values.at(clip.speedVariable),elapsed);
   if(step==EiemSampledAnimation::Step::Finished){Stop(clip);changed=true;}else if(step==EiemSampledAnimation::Step::Advanced)Evaluate(clip);
  }return changed;}
 void Prepare(const EiemDirectory::Package& p){clips.clear();output.clear();conflicts.clear();ownedTracks.clear();trackOwners.clear();active.clear();lastVisited=0;
  // Build conflicts once with the control graph, never by scanning Meshes or
  // testing group intersections in the per-frame interpolation path.
  for(const auto& row:p.shapeAnimations){clips.emplace(row.first,Playback{});ownedTracks.emplace(row.first,std::set<std::string>{});for(const auto& track:row.second.tracks){ownedTracks.at(row.first).insert(track.first);trackOwners[track.first].insert(row.first);}}
  for(const auto& row:trackOwners)for(const auto& id:row.second)for(const auto& other:row.second)if(id!=other)conflicts[id].insert(other);
  for(const auto& row:p.shapeAnimations)if(row.second.autoplay)Play(row.second);}
 double Value(const EiemVariables& manual,const std::string& variable)const{auto found=output.find(variable);return found==output.end()?manual.at(variable):found->second;}
};
}

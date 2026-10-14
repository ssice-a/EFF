#pragma once
#include "eiem_resource_directory.h"
namespace EiemShapeAnimation {
struct Playback {bool playing=false,paused=false,held=false;double time=0;};
struct Player {
 std::map<std::string,Playback> clips;
 std::map<std::string,double> output;
 std::map<std::string,std::set<std::string>> conflicts;
 std::map<std::string,std::set<std::string>> ownedTracks;
 void Stop(const EiemDirectory::ShapeAnimation& clip){auto& state=clips.at(clip.id);if(!state.playing)return;state.playing=state.paused=false;state.time=0;for(const auto& track:clip.tracks)output.erase(track.first);}
 bool Manual(const EiemDirectory::Package& p,const std::string& variable){bool changed=false;for(const auto& clip:p.shapeAnimations)if(clip.second.tracks.count(variable)&&clips.at(clip.first).playing){Stop(clip.second);changed=true;}return changed;}
 void Evaluate(const EiemDirectory::ShapeAnimation& clip){const auto& state=clips.at(clip.id);if(!state.playing)return;double phase=std::clamp(state.time/clip.duration,0.0,1.0);for(const auto& track:clip.tracks){double position=phase*double(track.second.size()-1);auto i=size_t(position);auto j=std::min(i+1,track.second.size()-1);double fraction=position-double(i);output[track.first]=double(track.second[i])+(double(track.second[j])-double(track.second[i]))*fraction;}}
 void Play(const EiemDirectory::ShapeAnimation& clip){auto& state=clips.at(clip.id);if(state.playing&&state.paused){state.paused=false;return;}
  auto found=conflicts.find(clip.id);if(found!=conflicts.end())for(const auto& id:found->second){auto& old=clips.at(id);if(!old.playing)continue;old.playing=old.paused=false;old.time=0;for(const auto& track:ownedTracks.at(id))output.erase(track);}
  state.playing=true;state.paused=false;state.time=0;Evaluate(clip);}
 bool Advance(const EiemDirectory::Package& p,const EiemVariables& values,double elapsed){bool changed=false;for(const auto& row:p.shapeAnimations){const auto& clip=row.second;auto& state=clips.at(row.first);if(!state.playing||state.paused)continue;double step=elapsed*values.at(clip.speedVariable);EiemDirectory::Require(std::isfinite(step)&&step>=0,"animation time overflow");state.time+=step;if(state.time>=clip.duration){if(clip.loop)state.time=std::fmod(state.time,clip.duration);else{Stop(clip);changed=true;continue;}}Evaluate(clip);}return changed;}
 void Prepare(const EiemDirectory::Package& p){clips.clear();output.clear();conflicts.clear();ownedTracks.clear();std::map<std::string,std::set<std::string>> owners;
  // Build conflicts once with the control graph, never by scanning Meshes or
  // testing group intersections in the per-frame interpolation path.
  for(const auto& row:p.shapeAnimations){clips.emplace(row.first,Playback{});for(const auto& track:row.second.tracks){ownedTracks[row.first].insert(track.first);owners[track.first].insert(row.first);}}
  for(const auto& row:owners)for(const auto& id:row.second)for(const auto& other:row.second)if(id!=other)conflicts[id].insert(other);
  for(const auto& row:p.shapeAnimations)if(row.second.autoplay)Play(row.second);}
 double Value(const EiemVariables& manual,const std::string& variable)const{auto found=output.find(variable);return found==output.end()?manual.at(variable):found->second;}
};
}

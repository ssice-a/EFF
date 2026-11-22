#pragma once
#include "eiem_resource_directory.h"
#include "eiem_sampled_animation.h"
#include "eiem_author_bone_animation.h"
#include <utility>
namespace EiemShapeAnimation {
using Playback=EiemSampledAnimation::Playback;
using BoneTarget=std::pair<std::string,std::string>;
struct Player {
 std::map<std::string,Playback> clips;
 std::map<std::string,double> output;
 std::map<BoneTarget,EiemAuthorBoneAnimation::TransformSample> boneOutput;
 std::map<std::string,std::set<std::string>> conflicts;
 std::map<std::string,std::set<std::string>> ownedTracks,trackOwners;
 std::map<std::string,std::set<BoneTarget>> ownedBones;
 std::map<std::string,std::set<std::string>> bonePaths;
private:
 // IDs, not pointers, survive candidate copies, swaps and F10 publication.
 std::set<std::string> active;
 size_t lastVisited=0;
 struct SampleLease {std::shared_ptr<const EiemAnimationSamples::Source> source;std::shared_ptr<const EiemAnimationSamples::Data> data;};
 std::map<std::string,SampleLease> sampleLeases;
 void StopId(const std::string& id){auto& state=clips.at(id);if(!state.playing)return;state.playing=state.paused=false;state.time=0;active.erase(id);sampleLeases.erase(id);for(const auto& track:ownedTracks.at(id))output.erase(track);for(const auto& bone:ownedBones.at(id))boneOutput.erase(bone);}
public:
 static bool Compatible(const EiemDirectory::ShapeAnimation& x,const EiemDirectory::ShapeAnimation& y){
  if(x.duration!=y.duration||x.loop!=y.loop||x.speedVariable!=y.speedVariable||x.tracks!=y.tracks||x.boneTracks.size()!=y.boneTracks.size()||x.scalarChannels!=y.scalarChannels||x.nativeOutputs.size()!=y.nativeOutputs.size())return false;
  for(size_t i=0;i<x.nativeOutputs.size();++i){const auto& u=x.nativeOutputs[i];const auto& v=y.nativeOutputs[i];if(u.hierarchy!=v.hierarchy||u.file!=v.file||u.asset!=v.asset||u.revision!=v.revision)return false;}
  if(bool(x.sampleSource)!=bool(y.sampleSource)||(x.sampleSource&&x.sampleSource->identity!=y.sampleSource->identity))return false;
  for(size_t i=0;i<x.boneTracks.size();++i){const auto& u=x.boneTracks[i];const auto& v=y.boneTracks[i];if(u.skeleton!=v.skeleton||u.path!=v.path||u.samples!=v.samples||u.channel!=v.channel)return false;}
  return true;
 }
 // The candidate has already built its new ownership/conflict indexes. Retain
 // only compatible playback, never copy the previous generation's topology or
 // sample leases. Retained user playback takes priority over new autoplay.
 void RetainCompatible(const EiemDirectory::Package& next,const EiemDirectory::Package& previous,const Player& old){
  std::map<std::string,Playback> retained;
  for(const auto& row:next.shapeAnimations){auto before=previous.shapeAnimations.find(row.first);auto state=old.clips.find(row.first);
   if(before!=previous.shapeAnimations.end()&&state!=old.clips.end()&&Compatible(before->second,row.second)){retained.emplace(row.first,state->second);StopId(row.first);}}
  for(const auto& row:retained){const auto& clip=next.shapeAnimations.at(row.first);
   if(row.second.playing){Play(clip);clips.at(row.first)=row.second;if(row.second.paused)active.erase(row.first);Evaluate(clip);}
   else clips.at(row.first)=row.second;
  }
 }
 size_t ActiveCount()const noexcept{return active.size();}
 size_t LastVisited()const noexcept{return lastVisited;}
 void Stop(const EiemDirectory::ShapeAnimation& clip){StopId(clip.id);}
 void Pause(const EiemDirectory::ShapeAnimation& clip){auto& state=clips.at(clip.id);if(!state.playing)return;state.paused=!state.paused;if(state.paused)active.erase(clip.id);else active.insert(clip.id);}
 bool Manual(const EiemDirectory::Package&,const std::string& variable){bool changed=false;auto owners=trackOwners.find(variable);if(owners==trackOwners.end())return false;
  for(const auto& id:owners->second)if(clips.at(id).playing){StopId(id);changed=true;}return changed;}
 void Evaluate(const EiemDirectory::ShapeAnimation& clip){const auto& state=clips.at(clip.id);if(!state.playing)return;
  if(!clip.SamplesActive()){sampleLeases.erase(clip.id);for(const auto& name:ownedTracks.at(clip.id))output.erase(name);for(const auto& bone:ownedBones.at(clip.id))boneOutput.erase(bone);return;}
  const EiemAnimationSamples::Data* data=nullptr;if(clip.sampleSource){auto& lease=sampleLeases[clip.id];if(lease.source!=clip.sampleSource){auto opened=clip.sampleSource->Open();lease={clip.sampleSource,std::move(opened)};}data=lease.data.get();}
  double phase=state.time/clip.duration;
  for(const auto& track:clip.tracks)output[track.first]=EiemSampledAnimation::Sample(clip.ScalarSamples(track.first,data),phase);
  for(const auto& track:clip.boneTracks)boneOutput[{track.skeleton,track.path}]=EiemAuthorBoneAnimation::Sample(clip.BoneSamples(track,data),phase);}
 void Play(const EiemDirectory::ShapeAnimation& clip){auto& state=clips.at(clip.id);if(state.playing&&state.paused){state.paused=false;active.insert(clip.id);return;}
  auto found=conflicts.find(clip.id);if(found!=conflicts.end())for(const auto& id:found->second)StopId(id);
  state.playing=true;state.paused=false;state.time=0;active.insert(clip.id);Evaluate(clip);}
 bool Advance(const EiemDirectory::Package& p,const EiemVariables& values,double elapsed){lastVisited=0;
  EiemDirectory::Require(std::isfinite(elapsed)&&elapsed>=0,"animation frame delta invalid");bool changed=false;
  for(auto it=sampleLeases.begin();it!=sampleLeases.end();){const auto id=it->first;++it;if(!p.shapeAnimations.at(id).SamplesActive()){Evaluate(p.shapeAnimations.at(id));changed=true;}}
  if(elapsed==0||active.empty())return changed;
  for(auto it=active.begin();it!=active.end();){const auto& clip=p.shapeAnimations.at(*it++);
   // Numeric sample residency cannot stop another adapter's native output.
   // Evaluate still drops inactive samples without opening their resource.
   if(!clip.SamplesActive()&&clip.nativeOutputs.empty()){bool held=sampleLeases.count(clip.id)!=0;Evaluate(clip);changed|=held;continue;}++lastVisited;auto& state=clips.at(clip.id);
   auto step=EiemSampledAnimation::Advance(state,clip.duration,clip.loop,values.at(clip.speedVariable),elapsed);
   if(step==EiemSampledAnimation::Step::Finished){Stop(clip);changed=true;}else if(step==EiemSampledAnimation::Step::Advanced)Evaluate(clip);
  }return changed;}
 void Prepare(const EiemDirectory::Package& p){clips.clear();output.clear();boneOutput.clear();conflicts.clear();ownedTracks.clear();trackOwners.clear();ownedBones.clear();bonePaths.clear();active.clear();lastVisited=0;sampleLeases.clear();
  // Build conflicts once with the control graph, never by scanning Meshes or
  // testing group intersections in the per-frame interpolation path.
  std::map<std::string,std::set<std::string>> skeletonOwners;
  for(const auto& row:p.shapeAnimations){clips.emplace(row.first,Playback{});ownedTracks.emplace(row.first,std::set<std::string>{});ownedBones.emplace(row.first,std::set<BoneTarget>{});for(const auto& track:row.second.tracks){ownedTracks.at(row.first).insert(track.first);trackOwners[track.first].insert(row.first);}for(const auto& track:row.second.boneTracks){ownedBones.at(row.first).emplace(track.skeleton,track.path);bonePaths[track.skeleton].insert(track.path);skeletonOwners[track.skeleton].insert(row.first);}}
  for(const auto& row:trackOwners)for(const auto& id:row.second)for(const auto& other:row.second)if(id!=other)conflicts[id].insert(other);
  for(const auto& row:skeletonOwners)for(const auto& id:row.second)for(const auto& other:row.second)if(id!=other)conflicts[id].insert(other);
  std::map<std::string,std::set<std::string>> nativeOwners;for(const auto& row:p.shapeAnimations)for(const auto& output:row.second.nativeOutputs)nativeOwners[output.hierarchy].insert(row.first);
  for(const auto& row:nativeOwners)for(const auto& id:row.second)for(const auto& other:row.second)if(id!=other)conflicts[id].insert(other);
  for(const auto& row:p.shapeAnimations)if(row.second.autoplay)Play(row.second);}
 double Value(const EiemVariables& manual,const std::string& variable)const{auto found=output.find(variable);return found==output.end()?manual.at(variable):found->second;}
};
}

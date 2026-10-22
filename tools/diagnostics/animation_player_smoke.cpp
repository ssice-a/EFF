#ifndef EIEM_ANIMATION_PLAYER_HEADER
#define EIEM_ANIMATION_PLAYER_HEADER "../../src/eiem_shape_animation_player.h"
#endif
#include EIEM_ANIMATION_PLAYER_HEADER
#include <chrono>
#include <iostream>
#include <limits>
using namespace EiemDirectory;
Package Fixture(size_t count){Package p;p.id="animation";for(size_t i=0;i<count;++i){ShapeAnimation c;c.id="clip_"+std::to_string(i);c.speedVariable="$speed_"+std::to_string(i);c.duration=1;c.tracks["$value_"+std::to_string(i)]={0,.5f,1};p.defaults[c.speedVariable]=1;p.shapeAnimations.emplace(c.id,c);}return p;}
int main(int argc,char** argv){
 try{
  if(argc==2&&std::string(argv[1])=="--profile"){
   for(size_t count:{size_t(1),size_t(10000)})for(bool playing:{false,true}){
    auto p=Fixture(count);EiemShapeAnimation::Player player;player.Prepare(p);if(playing)player.Play(p.shapeAnimations.begin()->second);
    auto start=std::chrono::steady_clock::now();double check=0;for(unsigned i=0;i<5000;++i){player.Advance(p,p.defaults,1./144.);if(playing)check+=player.output.begin()->second;}
    auto ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
    std::cout<<"PROFILE defined="<<count<<" active="<<playing<<" frames=5000 wallMs="<<ms<<" checksum="<<check<<'\n';
   }return 0;
  }
  auto p=Fixture(10000);EiemShapeAnimation::Player player;player.Prepare(p);auto& c=p.shapeAnimations.begin()->second;player.Play(c);player.Advance(p,p.defaults,.25);
  Require(player.output.begin()->second==.25,"linear interpolation differs");
  auto values=p.defaults;values[c.speedVariable]=2;player.Advance(p,values,.125);Require(player.output.begin()->second==.5,"speed change resets accumulated phase");
  // Validation for paused and inactive scheduling is compiled only once the
  // scheduler is present, while the profile also builds against the baseline.
#if defined(EIEM_ACTIVE_ANIMATION_TEST)
  Require(player.ActiveCount()==1&&player.LastVisited()==1,"inactive clips reached per-frame sampler");
  player.Pause(c);player.Advance(p,values,.5);Require(player.ActiveCount()==0&&player.LastVisited()==0&&player.output.begin()->second==.5,"pause sampled/reset output");
  player.Play(c);player.Advance(p,values,.125);Require(player.output.begin()->second==.75,"resume reset phase");
  auto copy=player;player.Stop(c);Require(copy.ActiveCount()==1&&player.ActiveCount()==0,"copied player shares scheduler state");
  copy.Advance(p,values,.125);Require(copy.output.begin()->second==0,"copied player retained dangling active handles");
  auto conflict=c;conflict.id="conflict";conflict.speedVariable="$conflict_speed";p.defaults[conflict.speedVariable]=1;p.shapeAnimations[conflict.id]=conflict;player.Prepare(p);player.Play(c);player.Pause(c);player.Play(conflict);
  Require(!player.clips.at(c.id).playing&&player.ActiveCount()==1,"conflicting paused clip kept ownership");
  player.Stop(c);Require(player.output.count(c.tracks.begin()->first),"inactive Stop erased another clip output");
  player.Manual(p,c.tracks.begin()->first);Require(!player.ActiveCount()&&player.output.empty(),"manual control did not stop indexed owner");
  player.Prepare(p);player.Play(c);auto time=player.clips.at(c.id).time;player.Advance(p,p.defaults,0);Require(player.LastVisited()==0&&player.clips.at(c.id).time==time,"zero delta evaluated curves");
  bool rejected=false;try{player.Advance(p,p.defaults,-1);}catch(...){rejected=true;}Require(rejected&&player.clips.at(c.id).time==time,"invalid clock mutated playback");
  auto& once=p.shapeAnimations.at(c.id);once.loop=false;player.Prepare(p);player.Play(once);Require(player.Advance(p,p.defaults,2)&&!player.ActiveCount()&&player.output.empty(),"one-shot does not restore manual output");
  once.loop=true;once.duration=1e308;player.Prepare(p);player.Play(once);player.Advance(p,p.defaults,7.5e307);player.Advance(p,p.defaults,7.5e307);
  Require(std::isfinite(player.clips.at(once.id).time)&&std::abs(player.clips.at(once.id).time/once.duration-.5)<1e-15,"large loop clock overflow");
  std::cout<<"PASS active playback: interpolation, speed continuity, pause/resume, copy ownership, conflicts/manual handoff, zero/invalid delta, once/loop and large clock\n";
#endif
  return 0;
 }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}

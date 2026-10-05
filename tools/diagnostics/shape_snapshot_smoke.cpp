#include "../../src/eiem_shape_snapshot.h"
#include <iostream>
#include <limits>
using namespace EiemShapeSnapshot;
template <class F> void Reject(F f) { bool rejected=false; try { f(); } catch(const std::exception&) { rejected=true; } Check(rejected,"invalid shape accepted"); }
int main() {
  try {
    Geometry base{{{1,2,3},{4,5,6}},{{0,1,0},{0,1,0}},{{1,0,0,-1},{1,0,0,1}}};
    Frame first{25,{{2,0,0},{0,4,0}},{{0,.2f,0},{0,.4f,0}},{{.4f,0,0},{.8f,0,0}}};
    Frame last{100,{{8,0,0},{0,8,0}},{{0,.8f,0},{0,.8f,0}},{{1,0,0},{2,0,0}}};
    std::vector<Channel> channels{{"A",{first,last}},{"B",{Frame{100,{{0,0,10},{0,0,20}},{{0,0,0},{0,0,0}},{{0,0,0},{0,0,0}}}}}};
    auto snapshot=Evaluate(base,channels,{{"A",62.5f},{"B",50}});
    Check(snapshot.positions[0]==Vec3{6,2,8} && snapshot.positions[1]==Vec3{4,11,16},"multi-frame/simultaneous shape interpolation");
    Check(std::abs(snapshot.normals[0][1]-1.5f)<1e-6f && std::abs(snapshot.tangents[0][0]-1.7f)<1e-6f,
          "normal/tangent delta interpolation");
    Check(snapshot.tangents[0][3]==-1 && snapshot.tangents[1][3]==1,"tangent handedness changed");
    auto future=Evaluate(base,channels,{{"A",100},{"B",100}});
    Check(snapshot.positions[0]==Vec3{6,2,8} && base.positions[0]==Vec3{1,2,3},"source/old snapshot mutated");
    Check(future.positions[0]==Vec3{9,2,13},"future shape snapshot");
    Check(Evaluate(base,channels,{}).positions==base.positions,"zero/default deformation");
    Check(Evaluate(base,channels,{{"A",12.5f}}).positions[0]==Vec3{2,2,3},"implicit zero frame");
    Check(Evaluate(base,channels,{{"A",-25}}).positions[0]==Vec3{-1,2,3},"negative weight extrapolation");
    Check(Evaluate(base,channels,{{"A",175}}).positions[0]==Vec3{15,2,3},"upper frame extrapolation");
    auto negative=channels; negative[0].frames={first}; negative[0].frames[0].weight=-25;
    Check(Evaluate(base,negative,{{"A",-12.5f}}).positions[0]==Vec3{2,2,3},"negative native frame");
    Reject([&]{Evaluate(base,channels,{{"missing",1}});});
    Reject([&]{Evaluate(base,channels,{{"A",std::numeric_limits<float>::infinity()}});});
    auto invalid=channels; invalid[0].frames[1].weight=25; Reject([&]{Evaluate(base,invalid,{});});
    invalid=channels; invalid[0].frames[0].positions.pop_back(); Reject([&]{Evaluate(base,invalid,{});});
    invalid=channels; invalid[0].frames[0].weight=0; Reject([&]{Evaluate(base,invalid,{});});
    Check(Advance(.2,1,1,.3)==.5 && Advance(.8,0,1,.3)==.5 && Advance(.95,1,1,.3)==1,
          "hold speed/direction/target saturation");
    Reject([&]{Advance(0,1,1,-.1);});
    std::cout<<"PASS shape snapshot: multi-frame/native-percent interpolation, signed/extrapolated weights, simultaneous channels, normal/tangent deltas, handedness, immutable source/ghosts, invalid data, hold motion\n";
    return 0;
  } catch(const std::exception& e) { std::cerr<<"FAIL "<<e.what()<<'\n'; return 1; }
}

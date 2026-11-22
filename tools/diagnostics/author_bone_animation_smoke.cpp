#include "../../src/eiem_author_bone_animation.h"
#include "../../src/eiem_shape_animation_player.h"

#include <iostream>

using namespace EiemAuthorBoneAnimation;

void U8(std::vector<unsigned char>& out, uint8_t value) { out.push_back(value); }
void U16(std::vector<unsigned char>& out, uint16_t value) {
  out.push_back(static_cast<unsigned char>(value));
  out.push_back(static_cast<unsigned char>(value >> 8));
}
void U32(std::vector<unsigned char>& out, uint32_t value) {
  for (unsigned i = 0; i != 4; ++i) out.push_back(static_cast<unsigned char>(value >> (8 * i)));
}
void F32(std::vector<unsigned char>& out, float value) {
  uint32_t bits;
  std::memcpy(&bits, &value, sizeof(bits));
  U32(out, bits);
}
void String(std::vector<unsigned char>& out, const std::string& value) {
  U16(out, static_cast<uint16_t>(value.size()));
  out.insert(out.end(), value.begin(), value.end());
}
void Sample(std::vector<unsigned char>& out, float y, float qz, float qw) {
  for (float value : {0.f, y, 0.f, 0.f, qz, 0.f, qw, 1.f, 1.f, 1.f}) F32(out, value);
}

std::vector<unsigned char> SkeletonBytes(bool withSourceFlags=false) {
  std::vector<unsigned char> out{'E','F','F','S','K','A','1',0};
  U32(out, withSourceFlags?2:1); U32(out, 2); String(out, "unity-y-up-left-handed");
  String(out, "WeaponRoot"); U32(out, 0xffffffffu);
  for (float value : {0.f,0.f,0.f,0.f,0.f,0.f,1.f,1.f,1.f,1.f}) F32(out, value);
  if(withSourceFlags) U8(out,1);
  String(out, "WeaponRoot/Tip"); U32(out, 0);
  for (float value : {0.f,0.f,1.f,0.f,0.f,0.f,1.f,1.f,1.f,1.f}) F32(out, value);
  if(withSourceFlags) U8(out,0);
  return out;
}

std::vector<unsigned char> ClipBytes() {
  std::vector<unsigned char> out{'E','F','F','B','A','N','1',0};
  U32(out, 1); String(out, "weapon_swing"); String(out, "weapon_rig");
  F32(out, 1.f); U32(out, 30); U8(out, 1); U32(out, 1);
  String(out, "WeaponRoot/Tip"); U32(out, 2);
  Sample(out, 0.f, 0.f, 1.f);
  Sample(out, .5f, .3826834f, .9238795f);
  return out;
}

void CheckRetarget() {
  const float half = std::sqrt(.5f);
  const Rotation turnX{half, 0, 0, half}, turnY{0, half, 0, half}, turnZ{0, 0, half, half};
  Skeleton rig;
  rig.nodes = {{"Root", -1}, {"Root/Short", 0, {}, turnZ},
               {"Root/Short/End", 1, {}, turnX},
               {"Root/Long", 0, {}, {0, 0, 0, 1}, {1, 1, 1}, false},
               {"Root/Long/End", 3, {0, 2, 0}, turnY, {1, 1, 1}, false}};
  SetRetargets(rig, {{"Root/Long/End", "Root/Short/End"}});
  const auto& mapping = rig.retargets.at(4);
  if (mapping.ancestor != 0 || mapping.sourceRelativePath != "Short/End")
    throw std::runtime_error("retarget did not preserve the instance-local relative source path");
  const RetargetRoutes routes{{4,{4,6,1}}};
  if (DecodeRetargetRoutes(EncodeRetargetRoutes("",routes),rig)!=routes)
    throw std::runtime_error("serialized native retarget route differs");
  for (const auto* bad : {"4=4,6,;", "4=4,-6,1,;", "0=4,6,1,;", "4=4,6,1,;4=4,6,1,;"}) {
    bool rejected=false;
    try { (void)DecodeRetargetRoutes(bad,rig); } catch (const std::exception&) { rejected=true; }
    if (!rejected) throw std::runtime_error("invalid local retarget route accepted");
  }
  const auto requireRotation = [](const Rotation& actual, const Rotation& expected) {
    for (size_t i = 0; i < 4; ++i)
      if (std::abs(actual[i] - expected[i]) > 1e-6)
        throw std::runtime_error("retarget rotation does not match the target bind basis");
  };
  requireRotation(RetargetRotation(mapping, turnX), turnY);
  // A source X rotation becomes a target Y rotation because the short
  // chain's parent is rotated +90 degrees around Z in the bind pose.
  requireRotation(RetargetRotation(mapping, Multiply(turnX, turnX)), {0, 1, 0, 0});
  for (const auto& source : {"Root/Missing", "Root/Long"}) {
    bool rejected = false;
    try { SetRetargets(rig, {{"Root/Long/End", source}}); }
    catch (const std::exception&) { rejected = true; }
    if (!rejected) throw std::runtime_error("invalid retarget source accepted");
  }
}

int main() {
  try {
    CheckRetarget();
    const auto skeletonBytes = SkeletonBytes();
    const auto skeleton = DecodeSkeleton(skeletonBytes.data(), skeletonBytes.size());
    const auto ownedBytes=SkeletonBytes(true);
    const auto owned=DecodeSkeleton(ownedBytes.data(),ownedBytes.size());
    if(!skeleton.nodes[1].source||!owned.nodes[0].source||owned.nodes[1].source)
      throw std::runtime_error("Skeleton ownership flags differ");
    const auto clipBytes = ClipBytes();
    const auto clip = DecodeClip(clipBytes.data(), clipBytes.size(), skeleton, "weapon_rig");
    const auto middle = Sample(clip.tracks.front(), .5);
    if (clip.id != "weapon_swing" || clip.tracks.size() != 1 ||
        std::abs(middle[1] - .25f) > 1e-6f ||
        std::abs(middle[4] - .1950903f) > 1e-5f ||
        std::abs(middle[6] - .9807853f) > 1e-5f)
      throw std::runtime_error("BoneAnimation interpolation differs");

    bool rejected = false;
    try { (void)DecodeClip(clipBytes.data(), clipBytes.size(), skeleton, "other_rig"); }
    catch (const std::exception&) { rejected = true; }
    if (!rejected) throw std::runtime_error("mismatched Skeleton reference accepted");

    auto broken = ClipBytes();
    const std::string path = "WeaponRoot/Tip";
    auto found = std::search(broken.begin(), broken.end(), path.begin(), path.end());
    if (found == broken.end()) throw std::runtime_error("smoke fixture path absent");
    std::copy(path.begin(), path.end(), found);
    broken[static_cast<size_t>(found - broken.begin()) + path.size() - 1] = 'X';
    rejected = false;
    try { (void)DecodeClip(broken.data(), broken.size(), skeleton, "weapon_rig"); }
    catch (const std::exception&) { rejected = true; }
    if (!rejected) throw std::runtime_error("unknown bone track accepted");

    EiemDirectory::Package package;
    EiemDirectory::ShapeAnimation mixed;
    mixed.id = "weapon_swing";
    mixed.speedVariable = "swing_speed";
    mixed.duration = 1;
    mixed.tracks.emplace("shape_channel", std::vector<float>{0.f, 1.f});
    EiemDirectory::ShapeAnimation::BoneTrack bone;
    bone.skeleton = "weapon_rig";
    bone.path = "WeaponRoot/Tip";
    bone.samples = clip.tracks.front().samples;
    mixed.boneTracks.push_back(bone);
    package.shapeAnimations.emplace(mixed.id, mixed);

    EiemShapeAnimation::Player player;
    player.Prepare(package);
    player.Play(package.shapeAnimations.at(mixed.id));
    const EiemVariables values{{"swing_speed", 1.0}};
    player.Advance(package, values, .5);
    const auto target = EiemShapeAnimation::BoneTarget{"weapon_rig", "WeaponRoot/Tip"};
    if (std::abs(player.Value(EiemVariables{{"shape_channel", 0.0}}, "shape_channel") - .5) > 1e-6 ||
        std::abs(player.boneOutput.at(target)[1] - .25f) > 1e-6f)
      throw std::runtime_error("Shape and Bone tracks did not share the clip clock");
    player.Pause(package.shapeAnimations.at(mixed.id));
    player.Advance(package, values, .25);
    if (std::abs(player.boneOutput.at(target)[1] - .25f) > 1e-6f)
      throw std::runtime_error("paused Bone track advanced");
    player.Stop(package.shapeAnimations.at(mixed.id));
    if (player.boneOutput.count(target))
      throw std::runtime_error("stopped Bone track retained an output sample");

    auto competing = mixed;
    competing.id = "weapon_idle";
    competing.tracks.clear();
    competing.tracks.emplace("other_channel", std::vector<float>{1.f, 0.f});
    competing.boneTracks.front().path="WeaponRoot";
    competing.boneTracks.front().samples = {
      TransformSample{0.f, .75f, 0.f, 0.f, 0.f, 0.f, 1.f, 1.f, 1.f, 1.f},
      TransformSample{0.f, .75f, 0.f, 0.f, 0.f, 0.f, 1.f, 1.f, 1.f, 1.f}};
    package.shapeAnimations.emplace(competing.id, competing);
    player.Prepare(package);
    player.Play(package.shapeAnimations.at(mixed.id));
    player.Play(package.shapeAnimations.at(competing.id));
    if (player.clips.at(mixed.id).playing || player.output.count("shape_channel") ||
        player.boneOutput.count(target)||
        std::abs(player.boneOutput.at({"weapon_rig","WeaponRoot"})[1] - .75f) > 1e-6f)
      throw std::runtime_error("Action on disjoint bones did not retire the same Skeleton's previous clip");

    std::cout << "AUTHOR_BONE_ANIMATION_OK nodes=" << skeleton.nodes.size()
              << " tracks=" << clip.tracks.size() << " midpointY=" << middle[1]
              << " sharedClock=1 pauseStop=1 conflictStop=1\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "FAIL " << error.what() << '\n';
    return 1;
  }
}

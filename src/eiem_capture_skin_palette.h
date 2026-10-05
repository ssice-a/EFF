#pragma once
// The public frame-matrix request payload contains a row-ordered 3x4 root
// matrix followed by one 3x4 matrix per Mesh bind-pose slot. This is caller
// memory, not Unity's private renderer state. Unity Matrix4x4 inputs are
// column ordered. Each palette is assembled from the same renderer instance. The inverse
// is the native capture parent's Transform.worldToLocalMatrix: skinningRoot,
// then rootBone, then world space. It is not the source Renderer frame.
#include "eiem_skin_math.h"
#include <array>
#include <cstring>
#include <stdexcept>

namespace EiemCaptureSkin {
using Matrix = EiemSkinProbe::Matrix;
using Rows = std::array<float,12>;
using Palette = std::vector<Rows>;

inline void RequireAffine(const Matrix& matrix) {
  for (float value:matrix.m)
    if (!std::isfinite(value)) throw std::runtime_error("capture skin matrix is nonfinite");
  if (std::fabs(matrix.m[3])>0.0001f || std::fabs(matrix.m[7])>0.0001f ||
      std::fabs(matrix.m[11])>0.0001f || std::fabs(matrix.m[15]-1.0f)>0.0001f)
    throw std::runtime_error("capture skin matrix is not affine");
}
inline Rows RowMatrix(const Matrix& matrix) {
  RequireAffine(matrix);
  Rows rows{};
  for (unsigned row=0;row<3;++row)
    for (unsigned column=0;column<4;++column) rows[row*4+column]=matrix.m[column*4+row];
  return rows;
}
inline Palette Build(const Matrix& rootWorld,const Matrix& rendererInverse,
                     const std::vector<Matrix>& boneWorld,const std::vector<Matrix>& bindPoses) {
  if (boneWorld.empty() || boneWorld.size()!=bindPoses.size() || boneWorld.size()>256)
    throw std::runtime_error("capture skin palette slot count");
  RequireAffine(rendererInverse);
  if (std::fabs(EiemSkinProbe::Determinant3x3(rendererInverse))<1.0e-12f)
    throw std::runtime_error("capture skin renderer frame is singular");
  Palette palette;palette.reserve(boneWorld.size()+1);
  palette.push_back(RowMatrix(rootWorld));
  for (size_t slot=0;slot<boneWorld.size();++slot) {
    RequireAffine(boneWorld[slot]);RequireAffine(bindPoses[slot]);
    const auto world=EiemSkinProbe::Multiply(boneWorld[slot],bindPoses[slot]);
    palette.push_back(RowMatrix(EiemSkinProbe::Multiply(rendererInverse,world)));
  }
  return palette;
}
inline bool Deliver(const Palette& palette,void* destination,int bytes) {
  static_assert(sizeof(Rows)==48,"native frame matrix row extent");
  if (!destination || palette.size()<2 || palette.size()>257 || bytes<=0 ||
      size_t(bytes)!=palette.size()*sizeof(Rows)) return false;
  // Validate everything before touching the caller's destination.
  for (const auto& rows:palette) for (float value:rows) if (!std::isfinite(value)) return false;
  std::memcpy(destination,palette.data(),size_t(bytes));
  return true;
}
}

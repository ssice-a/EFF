#pragma once

namespace EiemDemandDependencyScope {
enum class Boundary { BundleTree, AssetTree, OperationUpdate };
inline bool OpensRequestScope(Boundary boundary) noexcept {
  return boundary==Boundary::BundleTree||boundary==Boundary::AssetTree;
}
inline bool MayAugment(bool configured,unsigned requestDepth) noexcept {
  return configured&&requestDepth!=0;
}
} // namespace EiemDemandDependencyScope

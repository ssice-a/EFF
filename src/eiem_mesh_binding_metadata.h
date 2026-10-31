#pragma once
#include "eiem_resource_directory.h"

// Small author-computed data needed to bind a shared Mesh to each native
// skeleton. No vertices, pixels or per-instance state belong in this record.
namespace EiemMeshBindingMetadata {
using Bounds=std::pair<std::array<double,3>,std::array<double,3>>;
struct Mesh {
  std::vector<uint32_t> hashes;
  std::vector<Bounds> boneBounds;
};
inline void Validate(const Mesh& mesh,size_t bones) {
  EiemDirectory::Require(mesh.hashes.size()==bones&&mesh.boneBounds.size()==bones,"author Mesh binding metadata count differs");
  for(const auto& bounds:mesh.boneBounds)for(size_t i=0;i<3;++i)
    EiemDirectory::Require(std::isfinite(bounds.first[i])&&std::isfinite(bounds.second[i])&&bounds.second[i]>=0,"author Mesh binding bounds invalid");
}
} // namespace EiemMeshBindingMetadata

#pragma once

// The native assembly callbacks remain observation boundaries only.  Mesh
// binding is independent of world/UI/NPC construction order and always uses
// the canonical hierarchy table built for the current Renderer instance.
static bool EiemResolveMeshBonesFromAssembly(
    const EiemSkinIdentity &identity, void *renderer, void **out,
    char *error, size_t errorSize) {
  if (out) *out = nullptr;
  return EiemResolveMeshBonesFromIndexTable(identity, renderer, out, error,
                                             errorSize);
}
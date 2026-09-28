#pragma once

// The native assembly callbacks remain observation boundaries only. Mesh
// binding is independent of world/UI/NPC construction order and uses the
// source Renderer palettes captured for the current model instance.
static bool EiemResolveMeshBonesFromAssembly(
    const EiemSkinIdentity &identity, void *renderer, void **out,
    char *error, size_t errorSize) {
  if (out) *out = nullptr;
  return EiemResolveMeshBonesFromSourcePalettes(identity, renderer, out,
                                                error, errorSize);
}

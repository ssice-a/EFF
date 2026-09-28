# Shared skeleton binding

EIEMESH stores two different kinds of skeleton information:

- `bonePaths`, bind poses, and weights describe the exported Mesh palette.
- `boneSourceCandidates` describes where each palette slot can be obtained
  from an original game Renderer. A candidate is `(meshPath, meshAsset,
  slot)`.

## Normal Mesh replacement

The DLL snapshots every original `SkinnedMeshRenderer.bones` array in one
completed model instance before applying any replacement. For each replacement
slot it finds the matching source Mesh palette and copies the exact Transform
pointer from the recorded slot. All matching candidates must resolve to the
same pointer; a missing or conflicting candidate rejects that replacement and
leaves the source Renderer unchanged.

This is intentionally instance-local:

- World, UI, and NPC models never share Transform pointers.
- LODs can contribute candidates to one replacement palette. The active LOD
  does not determine the binding.
- Renderer root bones, Transform names, and runtime child-index paths are not
  used. PFBs may insert branches or rename nodes, so those values are not a
  stable identity.
- Hot reload first restores the original Mesh and original `bones[]` from the
  override ledger, then captures a fresh source palette. A previous replacement
  can never become a donor for the next generation.

## Explicit Mod-owned Skeleton

An optional `Skeleton` resource creates EIEM-owned Transform nodes for future
physics or truly new bones. That path is separate from normal Mesh binding and
may use the serialized `boneIndexPaths` to address nodes inside the explicit
Skeleton resource. A normal replacement Mesh must still provide source
candidates for every slot; an empty candidate list is accepted only when an
explicit Skeleton supplies that slot.

## Blender export contract

The Blender exporter builds the candidate list from the imported source Mesh
records and the shared armature catalog. It keeps the slot order identical
across weights, bind poses, and candidates. Adding a bone used by the edited
Mesh extends the palette and merges donor records from sibling source Meshes.
If an authored source bone has no donor, export fails instead of producing a
package that can only fall back to names or local LOD order.

`boneIndexPaths` remains in EIEMESH v6 for the explicit Skeleton interface and
authoring round trips. It is not consulted by the ordinary Mesh replacement
resolver.

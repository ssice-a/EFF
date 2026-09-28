# DLL architecture

The runtime is divided into three boundaries:

1. Resource readers load the EIEMESH v6 payload and build a generated Unity
   Mesh. They keep weights, bind poses, and source candidates immutable.
2. Model assembly snapshots the completed model's original Renderers. The
   skin resolver maps replacement slots to the exact `bones[]` pointers from
   that same instance.
3. The render executor commits Mesh, bones, materials, shapes, and visibility
   as one guarded transaction. The override ledger stores the original state
   for F10 and hot reload.

World, UI, and NPC construction paths all call the same executor after their
model hierarchy is complete. The executor receives a component snapshot once
per model pass, so source palette capture and replacement traversal observe the
same Renderer set.

## Skin binding rule

Ordinary Mesh replacement uses `boneSourceCandidates`; it does not walk the
runtime Transform hierarchy, compare Transform names, or guess a local LOD
slot. This is the only stable way to handle PFB-specific branch layouts and
per-instance bone palettes. Explicit Mod-owned Skeleton resources have their
own node lifecycle and are the only path that uses `boneIndexPaths`.

## Reload and failure behavior

Before a new generation is applied, the ledger restores the source Mesh,
source materials, source bones, root bone, and shape baseline. A failed source
candidate match, an ambiguous match, or a dead Transform rejects that renderer
transaction and keeps the source state. No partially bound Mesh is retained.

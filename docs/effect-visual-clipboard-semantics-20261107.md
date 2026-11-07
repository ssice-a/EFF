# Effect visual clipboard semantics

The copy unit is a visual effect tree, not an entire prefab and not a raw
serialized-file byte range.

Copying a selected emitter includes:

- the selected emitter and its child emitters;
- each child GameObject and Transform needed to preserve placement;
- the ParticleSystem, all native modules and curves;
- the ParticleSystemRenderer and its mesh/material slots;
- SubModule relationships between emitters.

EffectSetting, unrelated scene nodes, bundle tables, CAB names and source
PathIDs are excluded from the clipboard. The destination creates fresh native
identities and registers the new emitter with its existing effect lifecycle.

Mesh, Material and Texture references are represented as typed references. A
game-owned reference is resolved by logical path and asset name in the target
effect source. It is not copied as a second game asset. An authored reference
must either already exist in the target source or be transferred through the
author-resource payload; an unresolved reference aborts the paste instead of
writing a null pointer.

The object graph mentioned by the implementation is only the Unity
serialization machinery needed to encode ownership and PPtr relationships.
It is not a new user-facing editing concept.

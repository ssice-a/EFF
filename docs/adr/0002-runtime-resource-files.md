# ADR-0002: Self-Describing Runtime Resource Files

- Status: Accepted
- Date: 2026-10-30
- Scope: Mod format, DLL runtime inputs, Blender round-trip, Mod Manager migration

## Decision

1. The new Mod format uses `[Mod] format=3`. The new DLL accepts format 3 only.
   Mod Manager owns the one-time conversion from supported format 2 packages.
2. `mod.ini` is the only relationship and file-location manifest. It names resource IDs, relative
   paths, Mesh targets, Part membership, Material bindings, controls, physics,
   effects, and animation relationships. It contains no file offsets, cache
   keys, content hashes, CAB/PathID values, or generated consumer lists.
   A game resource identity uses its normalized logical path and asset name;
   the path may belong to either the `assets/` or `packages/` namespace for
   Mesh targets and Material/Texture sources. The namespace does not depend on
   resource kind, and identity paths reject traversal segments, backslashes,
   and drive prefixes.
3. Each referenced resource file is self-describing and independently
   addressable. Its file identity is its resource ID and relative path in
   `mod.ini`; runtime-native data and the author metadata required by Blender
   live in that same resource file. No `assets.bin`, compiled catalog, side
   index, or cache is required for correctness.
4. Mesh resource files contain the native serialized Mesh and an `EFFMESH`
   author snapshot so Blender can round-trip geometry, bindings, and target
   metadata. Runtime Mesh UV coordinates and tangent handedness remain in the
   source game's convention. An author snapshot marked `Blender` stores V and
   UV0 tangent handedness in Blender's convention; the native Mesh writer
   converts those fields back once. A snapshot in the Unity coordinate
   convention passes those fields through. Format 2 migration derives author
   data separately and preserves native UV values in the runtime buffer; it
   expands packed frame channels only when CPU shape jobs require Float32.
   Unchanged source DDS files are copied byte-for-byte. Authored Texture PNG
   pixels are vertically flipped from Blender's image-buffer order when encoded
   as DDS, matching the game's Texture2D row convention. Format 2 migration
   preserves the extracted Texture2D pixel order when recompressing RGBA data.
   Blender effect buffers use their own row-order contract and pass through
   unchanged.
   Material resource
   files contain only a sparse `EFFMAT` edit document plus its logical source
   path and asset name; they do not contain a native Material snapshot. When a
   declared Mesh target is prepared, the DLL
   resolves that logical Material in the current game manifest, reads its
   current serialized object, applies the sparse parameter edits and explicit
   Texture bindings to a private variant, then lets the game deserialize it.
   Unedited fields therefore follow the current game Material, and the variant
   is shared by every Renderer/Part use of the same authored Material. Source
   content hashes and old CAB/PathIDs are not compatibility identities.
   Physics resources follow the same rule: native Physics bytes come first,
   followed by an `EFFPHS3` author snapshot containing the editable `EFFPHYS`
   document and its `EFFSKEL` dependency. Metadata records both extents and
   the author snapshot SHA-256. Runtime reads only the native extent when a
   target Mesh is requested; Blender validates and restores the author data.
5. A Texture declaration names a native Texture2D scaffold (`path`) and an
   ordinary DDS (`dds`) in the same default `textures/` directory. The
   scaffold is a small native serialized Texture2D with an empty image-data
   field; it carries the game-readable object schema and CAB/PathID identity,
   not image dimensions or a pixel-length contract. The standard DDS is the
   sole source of dimensions, pixel format, mip count, and mip bytes. Runtime
   reads those values from the DDS header, updates the corresponding fields
   in the scaffold, injects its mip payload, and gives the result to the
   game's normal deserializer. The DDS file remains byte-for-byte unchanged.
   Replacing it with another valid DDS, including a different size or mip
   layout, needs no Blender re-export and no change to `mod.ini`.
6. Physics files and each native Effect bundle are separate resources named by
   `Resource` declarations in `mod.ini`. They are opened only when a matched
   owner needs them. Physics author data is co-located with its native input;
   no Skeleton sidecar is needed for Blender round-trip. `ResourceControls` names one self-describing control-state
   resource; `ShapeAnimation`, `MaterialAnimation`, and `EffectAttachment`
   sections name the authored records within that resource and point to the
   same file. The DLL verifies that these declarations match the control-state
   contents. It does not keep a second generated relationship index.
7. The default layout keeps resource types easy to browse: `meshes/`,
   `materials/`, `textures/`, `physics/`, and `effects/`, with
   `controls.effres` at the Mod root. A Texture scaffold and its `.dds` file
   live together under `textures/`; there is no parallel `native/` tree.
   Directory layout remains user-controlled: the DLL resolves each declared
   path relative to its Mod root, rejects traversal outside that root, and
   does not recursively scan or hash payload files at startup. Relative file
   locations repeated inside resource metadata are descriptive author data;
   readers use the `mod.ini` path and do not require the duplicate to match.
8. Startup reads `mod.ini` and creates transient rule lookup tables. The game
   request path performs exact logical target matching, then reads only the
   declared inputs needed for that request and applies a bounded serialized
   edit before the game's normal deserialization and object creation.
9. F10 publishes an immutable resource generation. In-flight reads keep their
   previous generation; changed DDS or resource files affect later requests
   and only the already-loaded owners whose declared inputs changed. A retired
   generation releases its native inputs after game consumers and async jobs
   release their leases.
10. Blender imports and exports each declared resource independently. A full
    scene export, selected-resource export, and selective import all use the
    same `mod.ini` relationships. Export timing separates snapshot, resource
    encoding, DDS compression, validation, and publication. Sampled animation
    metadata is encoded into the control-state resource offline; effect bundle
    files remain independent and are read on demand.

## Migration and Acceptance

Mod Manager converts format 2 to format 3 into a staging directory, validates
every declared relationship and file, then atomically publishes the result.
The original package remains available for rollback until the user removes it.
If author data is absent or cannot be converted losslessly, migration reports
the exact resource and keeps the original package intact. The DLL never
attempts migration or reads the old runtime format.

Migration does not require users to install AnimeStudio or the Blender add-on.
Mod Manager first reuses a compatible user-selected or registered index
workspace. If none is available, it downloads the portable index from the EFF
main repository into the user's selected resource/index location. If the
download is unavailable, Manager reads the selected game's VFS and builds or
refreshes the index locally.

The local fallback must work on a fresh Manager install with no network and no
AnimeStudio tools cache. Therefore, the VFS/Bundle reader and index builder
needed for fallback are part of Manager's installed functionality; the game
directory supplies data, not executable EFF tools. The user chooses the index
workspace. The full index and compiler are never placed in a fixed C: drive
cache; the portable index and shared compiler remain centrally versioned in
the EFF repository.

The EIDX and its bundle-span sidecar contain logical asset identities, native
record identities, dependencies, and index offsets; they do not contain
native asset payloads. For each required template, Manager reads only the
corresponding source Bundle from the local VFS. The index is a migration
locator, never a required file in the resulting Mod. Downloaded indexes and
Bundle ranges must be validated against the selected game before use. Users
choose where the index workspace is stored; migration does not put the full
index or compiler in a fixed C: drive cache.

Acceptance requires the same Mods to pass: static format validation, Blender
import/export round-trip by resource type, DDS replacement followed by F10,
matched Mesh/Material/Texture loading, physics/effect loading, unchanged-target
preservation, deletion of a Mod while running, and resource retirement after
world or party changes. Measurements report request-to-ready latency, bytes
read, peak private memory, and resource lease retirement separately.

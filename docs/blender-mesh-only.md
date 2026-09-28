# Mesh-only Export

Use **File > Export > EFF Mesh only** after selecting the EFF Mesh objects.
This mode writes the selected Mesh, material and texture dependencies, but it
does not infer or emit Skeleton/Physics resources and does not run the
authoring check that requires a newly added bone's shared Rig to be selected.

The Mesh binary still keeps the source skin palette and bind poses when those
are present on the selected object. This keeps the exported geometry usable by
the game's existing skeleton while leaving skeleton and physics authoring for a
separate export.

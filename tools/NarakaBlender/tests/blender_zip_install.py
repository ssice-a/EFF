"""Validate the distributable using a temporary BLENDER_USER_SCRIPTS directory."""
import importlib
import sys

import bpy

archive = sys.argv[sys.argv.index("--") + 1]
result = bpy.ops.preferences.addon_install(filepath=archive, overwrite=True)
assert result == {"FINISHED"}, result
result = bpy.ops.preferences.addon_enable(module="naraka_blender")
assert result == {"FINISHED"}, result
assert "naraka_blender" in bpy.context.preferences.addons
module = importlib.import_module("naraka_blender")
assert module.bl_info["version"] == (0, 3, 0)
result = bpy.ops.preferences.addon_disable(module="naraka_blender")
assert result == {"FINISHED"}, result
print("NARAKA_ZIP_INSTALL_PASS")

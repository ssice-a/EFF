"""Create a self-contained Blender add-on ZIP; no existing EFF add-on required."""
from pathlib import Path
from zipfile import ZipFile, ZIP_DEFLATED
import ast

root = Path(__file__).resolve().parent
module = ast.parse((root / "naraka_blender/__init__.py").read_text(encoding="utf-8"))
info = ast.literal_eval(next(n.value for n in module.body if isinstance(n, ast.Assign) and any(isinstance(t, ast.Name) and t.id == "bl_info" for t in n.targets)))
output = root / "dist" / ("naraka_blender-%s.zip" % ".".join(map(str, info["version"])))
output.parent.mkdir(exist_ok=True)
with ZipFile(output, "w", ZIP_DEFLATED) as archive:
    for source in sorted((root / "naraka_blender").glob("*.py")):
        archive.write(source, source.relative_to(root))
    archive.write(root / "README.md", "naraka_blender/README.md")
    archive.write(root / "shader-contracts.md", "naraka_blender/shader-contracts.md")
    archive.write(root / "material-porting.md", "naraka_blender/material-porting.md")
    archive.write(root / "shaders/naraka_flow.hlsl", "naraka_blender/shaders/naraka_flow.hlsl")
    archive.write(root / "LICENSE", "naraka_blender/LICENSE")
print(output)

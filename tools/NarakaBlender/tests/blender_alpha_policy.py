"""Blender 5 material regression: one RGBA texture, independent per-material opacity."""
import json
import sys
from pathlib import Path

import bpy

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from naraka_blender.materials import MaterialBuilder

# Exercise the real image loader as well: zero data alpha must keep color RGB.
import tempfile
import struct
import zlib
def png_chunk(kind, payload):
    return struct.pack('>I', len(payload)) + kind + payload + struct.pack('>I', zlib.crc32(kind + payload))
with tempfile.TemporaryDirectory(prefix='naraka-packed-diffuse-') as temporary:
    path = Path(temporary) / 'packed_D.png'
    path.write_bytes(b'\x89PNG\r\n\x1a\n' + png_chunk(b'IHDR', struct.pack('>IIBBBBB', 1, 1, 8, 6, 0, 0, 0)) +
        png_chunk(b'IDAT', zlib.compress(bytes([0, 255, 64, 32, 0]))) + png_chunk(b'IEND', b''))
    loader = MaterialBuilder(Path(temporary), {'textures': {'packed': {'path': path.name, 'name': 'packed_D', 'linear': False}}}, [])
    loaded = loader.image('packed', False)
    assert loaded.alpha_mode == 'CHANNEL_PACKED'
    assert loaded.pixels[0] > 0.9 and loaded.pixels[3] == 0, tuple(loaded.pixels)
    bpy.data.images.remove(loaded)
print('BLENDER PACKED DIFFUSE PASS RGB retained at A=0')

image = bpy.data.images.new("Same diffuse RGBA", 1, 1, alpha=True)
image.pixels = (0.8, 0.2, 0.1, 0.25)
sources = [
    ("opaque", "unknown_hair.shader", {"_Cutoff": 0.5, "_ZWrite": 0}, "", 3000),
    ("cutout", "lx22standardcharactercloth.shader", {"_Cutoff": 0.5}, "_USE_CUTOUT", -1),
    ("blend", "ch_cloth_silk_specular4_periodic2.shadergraph", {"_AlphaClipThreshold": 1}, "", -1),
    ("unknown blend source", "unknown.shader", {"_SrcBlend": 5, "_DstBlend": 10}, "", -1),
    ("silk", "ch_cloth_silk_specular4_periodic2.shadergraph", {"_AlphaClipThreshold": 0.5}, "", -1),
]
builder = MaterialBuilder(Path('.'), {"textures": {"rgba": {"wrap": 0, "filter": 1}}}, [], readable=False)
builder.image = lambda identity, linear: image
materials = {}
for label, shader, floats, keywords, queue in sources:
    material = builder.build(dict(id=label, name=label, shader=shader, floats=floats, vectors={},
        textures={"_MainTex": {"texture": "rgba"}}, keywords=keywords, renderQueue=queue))
    materials[label] = material
    tree = material.node_tree
    texture = next(n for n in tree.nodes if n.type == 'TEX_IMAGE')
    assert texture.image == image
    # Follow actual node links: cull-only alpha must never reach image Alpha.
    bsdf = next(n for n in tree.nodes if n.type == 'BSDF_PRINCIPLED')
    pending = [l.from_node for l in bsdf.inputs['Alpha'].links]
    upstream = set()
    while pending:
        node = pending.pop()
        if node in upstream: continue
        upstream.add(node)
        pending.extend(l.from_node for s in node.inputs for l in s.links)
    uses_diffuse_alpha = texture in upstream
    assert uses_diffuse_alpha == (label not in ('opaque', 'unknown blend source')), (label, uses_diffuse_alpha)
    if label == 'cutout':
        assert any(n.type == 'MATH' and n.operation == 'LESS_THAN' for n in upstream)
    print('BLENDER ALPHA PASS', label, 'diffuse alpha wired:', uses_diffuse_alpha)

if '--render' in sys.argv:
    grouped = MaterialBuilder(Path('.'), {"textures": {"rgba": {"wrap": 0, "filter": 1}}}, [])
    grouped.image = lambda identity, linear: image
    for label, shader, floats, keywords, queue in sources:
        materials[label] = grouped.build(dict(id=label, name=label, shader=shader, floats=floats, vectors={},
            textures={"_MainTex": {"texture": "rgba"}}, keywords=keywords, renderQueue=queue))
    # Actual CPU Cycles pixels, independent of the unavailable GL test context.
    # Transparent film directly measures per-material coverage of the same D.
    import tempfile
    from array import array
    scene = bpy.context.scene
    for obj in list(bpy.data.objects):
        bpy.data.objects.remove(obj, do_unlink=True)
    bpy.ops.mesh.primitive_plane_add(size=4)
    plane = bpy.context.object
    camera = bpy.data.objects.new('Alpha fixture camera', bpy.data.cameras.new('Alpha fixture camera'))
    scene.collection.objects.link(camera)
    camera.location = (0, 0, 2)
    camera.rotation_euler = (0, 0, 0)
    camera.data.type = 'ORTHO'; camera.data.ortho_scale = 1
    scene.camera = camera
    scene.render.engine = 'CYCLES'
    scene.cycles.device = 'CPU'; scene.cycles.samples = 64
    scene.cycles.use_denoising = False
    scene.render.resolution_x = 16; scene.render.resolution_y = 16
    scene.render.resolution_percentage = 100
    scene.render.film_transparent = True
    scene.render.image_settings.file_format = 'OPEN_EXR'
    scene.render.image_settings.color_mode = 'RGBA'
    with tempfile.TemporaryDirectory(prefix='naraka-alpha-pixels-') as temporary:
        for label, expected in (('opaque', 1), ('cutout', 0), ('blend', 0.25), ('silk', 0.5), ('unknown blend source', 1)):
            plane.data.materials.clear(); plane.data.materials.append(materials[label])
            # The fixture supplies independent channels, just as loaded textures.
            image.alpha_mode = 'CHANNEL_PACKED'
            scene.render.filepath = str(Path(temporary) / (label + '.exr'))
            bpy.ops.render.render(write_still=True)
            result = bpy.data.images.load(scene.render.filepath, check_existing=False)
            pixels = array('f', [0]) * len(result.pixels)
            result.pixels.foreach_get(pixels)
            # Cycles stochastically samples transparent BSDFs. Average the full
            # covered tile instead of asserting on one noisy path sample.
            alpha = sum(pixels[3::4]) / (16 * 16)
            assert abs(alpha - expected) < 0.01, (label, alpha, expected)
            bpy.data.images.remove(result)
            print('BLENDER ALPHA PIXEL PASS', label, round(alpha, 6))
print('BLENDER_ALPHA_POLICY_PASS', bpy.app.version_string)

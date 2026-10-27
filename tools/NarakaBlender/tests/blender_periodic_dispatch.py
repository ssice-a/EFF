"""Exercise lightweight emission dispatch on real keyword-enabled outfit materials."""
import copy
import json
import math
import sys
import tempfile
from pathlib import Path

import bpy

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from naraka_blender.materials import MaterialBuilder
from naraka_blender import lightweight
from naraka_blender.emission import periodic2_enabled

package = Path(sys.argv[sys.argv.index('--') + 1])
manifest = json.loads(package.read_text(encoding='utf-8-sig'))
builder = MaterialBuilder(package.parent, manifest, [])
sources = [s for s in manifest['materials'].values()
           if '_MFX_EMISSIVE_PERIODIC_2' in s.get('keywords', '').split()
           and s['textures'].get('_EmissionMap', {}).get('texture')]
assert sources, 'Fixture must contain actual periodic outfit materials'
failures = []
metrics = []
for source in sources:
    material = builder.build(source)
    waves = [n for n in material.node_tree.nodes if n.get('naraka_role') == 'flow_layer']
    if material.get('naraka_dynamic_material') != 'periodic2' or len(waves) != 2:
        failures.append(source['name'] + ': RG/BA phase texture was imported as static RGB emission')
        continue
    texture = next(n for n in material.node_tree.nodes
                   if n.type == 'TEX_IMAGE' and n.get('naraka_binding') == '_EmissionMap')
    assert texture.image.colorspace_settings.name == 'Non-Color'
    assert texture.image.alpha_mode == 'CHANNEL_PACKED'
    assert len(material.node_tree.animation_data.drivers) == 1
    split = next(l.to_node for l in material.node_tree.links
                 if l.from_node == texture and l.to_node.type == 'SEPARATE_COLOR')
    waves.sort(key=lambda n: n['naraka_flow_layer'])
    for i, wave in enumerate(waves):
        assert wave.inputs['区域蒙版'].links[0].from_socket == split.outputs[i * 2]
        assert wave.inputs['相位'].links[0].from_socket == (split.outputs[1] if i == 0 else texture.outputs[1])
        assert wave.inputs['时间 秒'].is_linked
        assert math.isclose(wave.inputs['速度 rad/s'].default_value, source['vectors']['_MatEffectParam%d' % i][1], rel_tol=1e-6)
    assert '相位' in texture['naraka_texture_usage']
    assert not any(n.type == 'TEX_IMAGE' for wave in waves for n in wave.node_tree.nodes)
    metrics.append({'name': material.name, 'shader': source['shader'], 'rootNodes': len(material.node_tree.nodes)})

assert not failures, '\n'.join(failures)

# The supported identity alone must not enable a keyword-disabled variant.
for source in sources:
    for key in ('_MFX_EMISSIVE_PERIODIC_2', '_MFX_EMISSIVE_PERIODIC_2_ON'):
        assert periodic2_enabled(source['shader'], {}, {key})
    assert periodic2_enabled(source['shader'], {'_MFX_EMISSIVE_PERIODIC_2': 1}, set())
    static = copy.deepcopy(source)
    static['keywords'] = '_USE_CUTOUT'
    static['floats']['_MFX_EMISSIVE_PERIODIC_2'] = 0
    material = builder.build(static)
    assert not material.get('naraka_dynamic_material')
    assert not lightweight.repair_periodic_emission(material)
assert not periodic2_enabled('unknown.shader', {}, {'_MFX_EMISSIVE_PERIODIC_2'})

# These vertex variants share _MainTex_ST, ignoring _EmissionMap_ST.
transformed = copy.deepcopy(sources[0])
transformed['textures']['_MainTex'].update(scale=[.5, .75], offset=[.125, .25])
transformed['textures']['_EmissionMap'].update(scale=[2, 3], offset=[.4, .6])
material = builder.build(transformed)
texture = next(n for n in material.node_tree.nodes if n.type == 'TEX_IMAGE' and n.get('naraka_binding') == '_EmissionMap')
mapping = texture.inputs[0].links[0].from_node
assert mapping.type == 'MAPPING'
assert tuple(mapping.inputs['Scale'].default_value) == (.5, .75, 1)
assert tuple(mapping.inputs['Location'].default_value) == (.125, .25, 0)

# Reproduce the old importer at the actual builder boundary; repair it without
# changing PBR links, alpha/render state, edited HDR color or texture coordinates.
enabled = lightweight.periodic2_enabled
lightweight.periodic2_enabled = lambda *args: False
try:
    legacy = builder.build(sources[0])
finally:
    lightweight.periodic2_enabled = enabled
tree = legacy.node_tree
bsdf = next(n for n in tree.nodes if n.type == 'BSDF_PRINCIPLED')
old = bsdf.inputs['Emission Color'].links[0].from_node
old.inputs[2].default_value = (2, .1, 4, 1)
unaffected = [(l.from_socket, l.to_socket) for l in tree.links
              if l.to_node != old and l.from_node != old]
render_state = (legacy.surface_render_method, legacy.use_backface_culling)
assert lightweight.repair_periodic_emission(legacy)
assert all(any(l.from_socket == a and l.to_socket == b for l in tree.links) for a, b in unaffected)
assert (legacy.surface_render_method, legacy.use_backface_culling) == render_state
wave = next(n for n in tree.nodes if n.get('naraka_flow_layer') == 0)
assert all(math.isclose(a, b, rel_tol=1e-6) for a, b in zip(wave.inputs['颜色'].default_value, (2, .1, 4, 1)))
assert not lightweight.repair_periodic_emission(legacy), 'Repair must be idempotent'

# Render a spatial phase field at two times. This distinguishes an advancing
# phase band from static RGB emission, a uniform pulse or a scrolling UV map.
output = Path(sys.argv[sys.argv.index('--') + 2]) if len(sys.argv) > sys.argv.index('--') + 2 else Path(tempfile.mkdtemp(prefix='naraka-periodic-'))
output.mkdir(parents=True, exist_ok=True)
bpy.ops.object.select_all(action='SELECT')
bpy.ops.object.delete(use_global=False)
bpy.ops.mesh.primitive_plane_add(size=2)
plane = bpy.context.object
plane.data.uv_layers.active.name = 'UV0'
bpy.ops.object.camera_add(location=(0, 0, 3))
camera = bpy.context.object
camera.data.type = 'ORTHO'
camera.data.ortho_scale = 2
scene = bpy.context.scene
scene.camera = camera
scene.render.engine = 'CYCLES'
scene.cycles.device = 'CPU'
scene.cycles.samples = 1
scene.cycles.use_denoising = False
scene.render.resolution_x, scene.render.resolution_y = 32, 16
scene.render.resolution_percentage = 100
scene.render.image_settings.file_format = 'OPEN_EXR'
scene.render.image_settings.color_mode = 'RGBA'
scene.render.film_transparent = True
scene.render.fps, scene.render.fps_base = 30, 1.001
# A center sample is deterministic even with a single Cycles sample.
scene.cycles.use_animated_seed = False
packed = [[.8, (i + .5) / 32, .5, 1 - (i + .5) / 32] for i in range(32)]
field = bpy.data.images.new('Regression packed phase field', width=32, height=1, alpha=True, float_buffer=True)
field.colorspace_settings.name = 'Non-Color'
field.alpha_mode = 'CHANNEL_PACKED'
field.pixels[:] = [v for pixel in packed for v in pixel]
pixels = []
for source in sources[:2]:
    sample = copy.deepcopy(source)
    sample['vectors'].update(_EmissionColor=[2, .1, 4, 1], _MatEffectColor0=[.3, 1, .2, 1])
    sample['vectors']['_MatEffectParam2'][1] = .25
    material = builder.build(sample)
    tree = material.node_tree
    texture = next(n for n in tree.nodes if n.type == 'TEX_IMAGE' and n.get('naraka_binding') == '_EmissionMap')
    texture.image, texture.interpolation = field, 'Closest'
    bsdf = next(n for n in tree.nodes if n.type == 'BSDF_PRINCIPLED')
    out = next(n for n in tree.nodes if n.type == 'OUTPUT_MATERIAL')
    emission = tree.nodes.new('ShaderNodeEmission')
    tree.links.new(bsdf.inputs['Emission Color'].links[0].from_socket, emission.inputs[0])
    tree.links.new(emission.outputs[0], out.inputs['Surface'])
    plane.data.materials.clear()
    plane.data.materials.append(material)
    frames = []
    for frame in (0, 37):
        scene.frame_set(frame)
        scene.render.filepath = str(output / ('%s-%d.exr' % (source['name'], frame)))
        bpy.ops.render.render(write_still=True)
        result = bpy.data.images.load(scene.render.filepath, check_existing=False)
        data = list(result.pixels)
        bpy.data.images.remove(result)
        actuals = []
        for x in (4, 12, 20, 28):
            actual = data[(8 * 32 + x) * 4:(8 * 32 + x) * 4 + 3]
            t = frame * scene.render.fps_base / scene.render.fps
            terms = []
            for i in range(2):
                p = sample['vectors']['_MatEffectParam%d' % i]
                wave = math.sin(packed[x][i * 2 + 1] * p[0] + t * p[1]) * .5 + .5
                terms.append(packed[x][i * 2] ** 2 * max(0, min(1, (wave - p[2]) / max(2 ** -24, 1 - p[2]))) ** p[3])
            want = [terms[0] * sample['vectors']['_EmissionColor'][j] + terms[1] * sample['vectors']['_MatEffectColor0'][j] * (1 + .25 * (sample['vectors']['_EmissionColor'][j] - 1)) for j in range(3)]
            assert max(abs(a - b) for a, b in zip(actual, want)) < .003, (source['name'], frame, x, actual, want)
            actuals.append(actual)
        frames.append(actuals)
    assert max(abs(a - b) for arow, brow in zip(*frames) for a, b in zip(arow, brow)) > .05
    pixels.append({'name': source['name'], 'samples': frames})
(output / 'verification.json').write_text(json.dumps({'materials': metrics, 'spatialPhasePixels': pixels, 'legacyRepair': True}, ensure_ascii=False, indent=2), encoding='utf-8')
print('PERIODIC_DISPATCH_PASS', json.dumps(metrics, ensure_ascii=False))

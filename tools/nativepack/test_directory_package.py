"""Format contracts and shared state behavior; native bytes are checked separately."""
from __future__ import annotations

from pathlib import Path
import struct
import tempfile
import unittest

from nativepack.delta_apply import GEOMETRY_FIELDS
from nativepack.ini_package import export_directory, update_directory, ini_document, save_ini
from nativepack.ini_reader import read_package
from nativepack.resource_file import (MESH, MATERIAL, encode_resource, decode_resource,
                                     encode_strings, decode_strings, resource_path)
from nativepack.state_rules import Condition, apply_key


class ResourceTests(unittest.TestCase):
    def test_compressed_texture_resource_round_trip(self):
        from nativepack.texture_fields import texture_blocks, texture_metadata, texture_byte_size
        from nativepack.resource_file import PIXELS
        for format_name in ('BC5', 'BC7'):
            texture = dict(width=5, height=7, format=format_name, mipCount=3,
                           colorSpace=0, filter=1, wrap=0, aniso=1, mipBias=0.0,
                           sourcePixelsHash='ab' * 32, pixels=bytes(96))
            self.assertEqual(texture_byte_size(texture), 96)
            resource = decode_resource(encode_resource(PIXELS, '12' * 16, texture_blocks(texture)), PIXELS)
            metadata = texture_metadata(resource, {})
            self.assertEqual(metadata['format'], format_name)
            self.assertEqual(metadata['sourcePixelsHash'], 'AB' * 32)
            self.assertEqual((metadata['width'], metadata['height'], metadata['mipCount']), (5, 7, 3))

    def test_blocks_and_empty_material(self):
        blocks = {"native": bytes(range(23)), "@bones": encode_strings(["Root/骨骼", "Root/Bone"]), "empty": b""}
        encoded = encode_resource(MESH, "12" * 16, blocks)
        decoded = decode_resource(encoded, MESH)
        self.assertEqual(decoded["blocks"], blocks)
        self.assertEqual(decode_strings(decoded["blocks"]["@bones"]), ["Root/骨骼", "Root/Bone"])
        empty = encode_resource(MATERIAL, "34" * 16, {})
        self.assertEqual(decode_resource(empty, MATERIAL)["blocks"], {})

    def test_invalid_resources_are_rejected(self):
        encoded = encode_resource(MESH, "12" * 16, {"field": b"native input"})
        with self.assertRaisesRegex(ValueError, "kind"):
            decode_resource(encoded, MATERIAL)
        changed = bytearray(encoded); changed[-1] ^= 1
        with self.assertRaisesRegex(ValueError, "checksum"):
            decode_resource(changed, MESH)
        with self.assertRaisesRegex(ValueError, "size"):
            decode_resource(encoded + b"original mesh bytes", MESH)
        with self.assertRaisesRegex(ValueError, "truncated"):
            decode_strings(struct.pack("<I", 1) + struct.pack("<H", 20) + b"x")
        with tempfile.TemporaryDirectory() as root:
            for relative in ("../x", "meshes/../../x", "C:/x", "/x", "meshes\\x", "meshes//x"):
                with self.assertRaises(ValueError): resource_path(root, relative)


class StateTests(unittest.TestCase):
    def test_many_keys_share_one_state(self):
        state = {"$mode": 0}
        forward = dict(variable="$mode", type="cycle", values=[0, 1, 2])
        backward = dict(variable="$mode", type="cycle", values=[2, 1, 0])
        condition = Condition("$mode == 1 || $mode == 2")
        self.assertFalse(condition.evaluate(state))
        apply_key(state, forward); self.assertTrue(condition.evaluate(state))
        apply_key(state, backward); self.assertFalse(condition.evaluate(state))
        self.assertEqual(len(state), 1)
        self.assertEqual(condition.variables, {"$mode"})

    def test_conditions_hold_and_no_execution(self):
        condition = Condition("($mode >= 1 && $mode != 3) || !$other")
        self.assertTrue(condition.evaluate({"$mode": 2, "$other": 1}))
        self.assertFalse(condition.evaluate({"$mode": 3, "$other": 1}))
        state = {"$shape": 0.2}
        apply_key(state, dict(variable="$shape", type="hold", values=[1.0], speed=2), 0.1)
        self.assertAlmostEqual(state["$shape"], 0.4)
        apply_key(state, dict(variable="$shape", type="hold", values=[0.0], speed=2), 2)
        self.assertEqual(state["$shape"], 0)
        for text in ("__import__('os')", "$mode = 1", "$mode ==", "($mode == 1", "$mode + 1", "true garbage"):
            with self.assertRaises(ValueError): Condition(text)


def fixture():
    part = dict(partId="author-part", object="MeshBody", material="MaterialBody", vertices=3, indices=3,
                bonePaths=["Root", "Root/Bone"], shapeNames=["TestShape"], nativeTypeHash="12" * 16,
                visibilityBindings=[dict(variable="$mode", visible=[1, 2])],
                shapeControls=[dict(shape="TestShape", identity="shape_id", enabled=True, label="Test Shape",
                                    default=0.0, minimum=0.0, maximum=1.0,
                                    hotkey_increase="UP", hotkey_decrease="DOWN", hotkey_speed=1.0)],
                fieldData={field: b"native-field" for field in GEOMETRY_FIELDS})
    texture = dict(id="tex0", sourcePath="assets/body_d", sourceAsset="Body_D", pixels=b"\x00\x20\x40\xff",
                   pixelLabel="body", format="RGBA32", width=1, height=1, mipCount=1, colorSpace=1,
                   filter=1, wrap=0, aniso=1, mipBias=0.0)
    second = dict(texture, id="tex1", sourcePath="assets/body_copy", sourceAsset="Body_Copy")
    material = dict(id="MaterialBody", sourcePath="assets/body_mat", sourceAsset="BodyMaterial",
                    nativeTypeHash="56" * 16, fieldData={},
                    textureEdits=[dict(property="_BaseMap", texture="tex0"), dict(property="_DetailMap", texture="tex1")])
    rules = [dict(sourcePath="assets/body", sourceAsset="Body", hidden=False, parts=["author-part"],
                  targets=[dict(level=0, sourcePath="assets/body", sourceAsset="Body"),
                           dict(level=1, sourcePath="assets/body.fbx", sourceAsset="Body_LOD1")])]
    return dict(mod_id="Test", parts=[part], materials=[material], textures=[texture, second], rules=rules,
                switches=[dict(variable="$mode", label="服装切换组", key="NUMPAD1", stateCount=3, default=0)])


class DirectoryTests(unittest.TestCase):
    def test_memory_only_export_emits_complete_input_without_staging_files(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder) / 'staging'
            root.mkdir()
            memory = export_directory(root, **fixture(), memory_only=True)
            self.assertEqual(list(root.iterdir()), [])
            self.assertIn('[Mod]', memory['_memory_ini'].decode('utf-8'))
            self.assertEqual(set(memory['_memory_resources']), {
                'meshes/MeshBody.mesh', 'materials/MaterialBody.mat', 'textures/body.tex',
            })
            published = Path(folder) / 'published'
            published.mkdir()
            (published / 'mod.ini').write_bytes(memory['_memory_ini'])
            for relative, data in memory['_memory_resources'].items():
                path = published / relative
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(data)
            package = read_package(published)
            self.assertEqual(package['modId'], 'Test')
            self.assertEqual(package['rules'], memory['rules'])

    def test_memory_only_export_keeps_material_animation_in_memory(self):
        clip = dict(id='matflow', label='Emission flow', key='F6', duration=1, speed=1,
                    loop=True, autoplay=True,
                    targets=[dict(variable='$matflow_0', material='MaterialBody',
                                  property='_Emission', kind='FLOAT', component=0,
                                  samples=[0, .5, 1])])
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder) / 'staging'
            root.mkdir()
            memory = export_directory(root, **fixture(), material_clips=[clip], memory_only=True)
            self.assertEqual(list(root.iterdir()), [])
            self.assertIn('resources/materialanimation_matflow.bin', memory['_memory_resources'])
            published = Path(folder) / 'published'
            published.mkdir()
            (published / 'mod.ini').write_bytes(memory['_memory_ini'])
            for relative, data in memory['_memory_resources'].items():
                path = published / relative
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(data)
            package = read_package(published)
            self.assertEqual(package['materialAnimations'][0]['tracks']['$matflow_0'], (0, .5, 1))

    def test_editable_mesh_snapshot_id_matches_manifest_part_id(self):
        from nativepack.ini_package import part_resource_id
        data = fixture()
        data['parts'][0]['partId'] = 'MeshBody.001'
        data['rules'][0]['parts'] = [data['parts'][0]['partId']]
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            export_directory(root, **data)
            manifest = ini_document(root)
            part_id = part_resource_id(data['parts'][0]['partId'])
            self.assertIn(part_id, manifest)
            self.assertEqual(part_id, 'PartBody.001')

    def test_nested_mod_has_independent_resource_scope(self):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder)/'lx'
            root.mkdir()
            export_directory(root,**fixture())
            before=read_package(root)
            child=root/'effects'/'花瓣'
            child.mkdir(parents=True)
            export_directory(child,**dict(fixture(),mod_id='Petals'))
            after=read_package(root)
            self.assertEqual(after['_files'],before['_files'])
            self.assertEqual(read_package(child)['modId'],'Petals')
            (child/'unexpected.txt').write_text('child only',encoding='utf-8')
            self.assertEqual(read_package(root)['_files'],before['_files'])
            with self.assertRaisesRegex(ValueError,'unreferenced/source'):
                read_package(child)
            (root/'unexpected.txt').write_text('parent still strict',encoding='utf-8')
            with self.assertRaisesRegex(ValueError,'unreferenced/source'):
                read_package(root)

    def test_shared_texture_bindings_encode_and_write_one_payload(self):
        from unittest.mock import patch
        from nativepack import ini_package
        data=fixture();textures=data['textures'];encoded=ini_package.encode_resource
        calls=[]
        def encode(kind, native_type, blocks):
            if kind==ini_package.PIXELS:calls.append(bytes(blocks['pixels']))
            return encoded(kind,native_type,blocks)
        with tempfile.TemporaryDirectory() as folder,patch.object(ini_package,'encode_resource',encode):
            root=Path(folder);result=export_directory(root,**data)
            self.assertEqual(len(calls),1)
            self.assertEqual(len(list((root/'textures').glob('*.tex'))),1)
            self.assertEqual(len(result['textures']),2)
            self.assertEqual(sum(bool(t['_written']) for t in textures),1)
            # Identical labels with different pixels remain distinct inputs.
            textures[1]['pixels']=b'\xff\x00\x00\xff'
            with self.assertRaisesRegex(ValueError,'same TEX filename'):
                ini_package.texture_output_paths(root,ini_package.ini_document(root),textures)

    def variant_fixture(self, same_label=False):
        data = fixture()
        first = dict(data['textures'][0], id='variant-a', section='TextureVariantA',
                     pixelLabel='shared' if same_label else 'bar')
        second = dict(first, id='variant-b', section='TextureVariantB',
                      pixels=b'\xff\x00\x00\xff', pixelLabel='shared' if same_label else 'cloth')
        material = data['materials'][0]
        data['textures'] = [first, second]
        data['materials'] = [dict(material, textureEdits=[]),
            dict(material, id='MaterialVariantA', textureEdits=[dict(property='_BaseMap', texture=first['id'])]),
            dict(material, id='MaterialVariantB', textureEdits=[dict(property='_BaseMap', texture=second['id'])])]
        part = data['parts'][0]
        data['parts'] = [dict(part, partId='part-' + name, object='Mesh' + name,
                             material=mid, shapeControls=[]) for name, mid in
                        [('Original', 'MaterialBody'), ('VariantA', 'MaterialVariantA'), ('VariantB', 'MaterialVariantB')]]
        data['rules'][0]['parts'] = [part['partId'] for part in data['parts']]
        return data

    def test_material_variants_keep_native_and_independent_texture_bindings(self):
        data = self.variant_fixture()
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            package = export_directory(root, **data)
            materials = {m['id']: m for m in package['materials']}
            self.assertEqual(materials['MaterialBody']['textureEdits'], [])
            self.assertEqual(materials['MaterialVariantA']['textureEdits'][0]['texture'], 'TextureVariantA')
            self.assertEqual(materials['MaterialVariantB']['textureEdits'][0]['texture'], 'TextureVariantB')
            self.assertEqual({t['id'] for t in package['textures']}, {'TextureVariantA', 'TextureVariantB'})
            second_before = (root / 'textures/cloth.tex').read_bytes()
            material = data['materials'][1]
            texture = dict(data['textures'][0], pixels=b'\x00\xff\x00\xff')
            updated = update_directory(root, materials=[material], textures=[texture], texture_only=True)
            self.assertEqual((root / 'textures/cloth.tex').read_bytes(), second_before)
            self.assertEqual(len(updated['textures']), 2)
            self.assertEqual(ini_document(root)['MaterialVariantB']['texture._BaseMap'], 'TextureVariantB')

    def test_variant_images_with_same_filename_do_not_overwrite(self):
        data = self.variant_fixture(same_label=True)
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            package = export_directory(root, **data)
            textures = {t['id']: t for t in package['textures']}
            first, second = textures['TextureVariantA']['file'], textures['TextureVariantB']['file']
            self.assertNotEqual(first, second)
            second_before = (root / second).read_bytes()
            updated = update_directory(root, materials=[data['materials'][1]],
                textures=[dict(data['textures'][0], pixels=b'\x00\xff\x00\xff')], texture_only=True)
            self.assertEqual((root / second).read_bytes(), second_before)
            self.assertEqual({t['id'] for t in updated['textures']}, set(textures))

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.package = export_directory(self.root, **fixture())

    def tearDown(self): self.temp.cleanup()

    def test_per_mod_key_switch_default_and_explicit_flag(self):
        self.assertTrue(self.package['keySwitchEnabled'])
        self.edit_ini(lambda text: text.replace('key_switch_enabled=1\n', ''))
        self.assertTrue(read_package(self.root)['keySwitchEnabled'])
        update_directory(self.root, textures=fixture()['textures'], texture_only=True)
        self.assertEqual(ini_document(self.root)['Mod']['key_switch_enabled'], '1')
        self.edit_ini(lambda text: text.replace('key_switch_enabled=1', 'key_switch_enabled=false'))
        self.assertFalse(read_package(self.root)['keySwitchEnabled'])
        updated = update_directory(self.root, textures=fixture()['textures'], texture_only=True)
        self.assertFalse(updated['keySwitchEnabled'])
        self.edit_ini(lambda text: text.replace('key_switch_enabled=false', 'key_switch_enabled=TRUE'))
        self.assertTrue(read_package(self.root)['keySwitchEnabled'])
        self.edit_ini(lambda text: text.replace('key_switch_enabled=TRUE', 'key_switch_enabled=2'))
        with self.assertRaises(ValueError): read_package(self.root)
        with tempfile.TemporaryDirectory() as root:
            disabled = export_directory(root, **fixture(), key_switch_enabled=False)
            self.assertFalse(disabled['keySwitchEnabled'])

    def test_different_bindings_can_share_one_key(self):
        self.edit_ini(lambda text: text + '\n[KeyOther]\nkey=NUMPAD1\ntype=cycle\n$mode=0,1,2\n')
        bindings = read_package(self.root)['keyBindings']
        self.assertEqual(sum(b['key'] == 'NUMPAD1' for b in bindings), 2)

    def test_switch_group_label_exports_and_old_packages_remain_readable(self):
        package = read_package(self.root)
        self.assertEqual(package['keyBindings'][0]['label'], '服装切换组')
        self.edit_ini(lambda text: text.replace('label=服装切换组\n', ''))
        self.assertEqual(read_package(self.root)['keyBindings'][0]['label'], '')

    def test_material_switch_variants_share_one_mesh_and_follow_key_state(self):
        data = fixture()
        data['materials'].append(dict(data['materials'][0], id='MaterialAlternate'))
        part = data['parts'][0]
        part['visibilityBindings'].append(dict(variable='$material', visible=[0]))
        alternate = dict(part, partId='author-part-alternate', material='MaterialAlternate',
                         visibilityBindings=[dict(variable='$mode', visible=[1, 2]),
                                             dict(variable='$material', visible=[1])])
        data['parts'].append(alternate)
        data['rules'][0]['parts'].append(alternate['partId'])
        data['switches'].append(dict(variable='$material', label='裙子材质', key='F7',
                                     stateCount=2, default=0))
        with tempfile.TemporaryDirectory() as folder:
            package = export_directory(Path(folder), **data)
        self.assertEqual(len(package['parts']), 2)
        self.assertEqual({part['mesh'] for part in package['parts']}, {'MeshBody'})
        states = dict(package['states'], **{'$mode': 1, '$material': 0})
        first, second = package['parts']
        self.assertTrue(first['visible'].evaluate(states))
        self.assertFalse(second['visible'].evaluate(states))
        states['$material'] = 1
        self.assertFalse(first['visible'].evaluate(states))
        self.assertTrue(second['visible'].evaluate(states))
        self.assertEqual({binding['label'] for binding in package['keyBindings']
                          if binding['type'] == 'cycle'},
                         {'服装切换组', '裙子材质'})

    def test_partial_mesh_update_accepts_shared_material_variants(self):
        data = fixture()
        data['materials'].append(dict(data['materials'][0], id='MaterialAlternate'))
        part = data['parts'][0]
        alternate = dict(part, partId='author-part-alternate', material='MaterialAlternate')
        data['parts'].append(alternate)
        data['rules'][0]['parts'].append(alternate['partId'])
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            export_directory(root, **data)
            updated = update_directory(root, parts=[part])
        self.assertEqual(len(updated['parts']), 2)
        self.assertEqual({row['material'] for row in updated['parts']},
                         {'MaterialBody', 'MaterialAlternate'})

    def edit_ini(self, transform):
        path = self.root / "mod.ini"
        path.write_text(transform(path.read_text(encoding="utf-8")), encoding="utf-8")

    def test_directory_has_one_authoritative_graph(self):
        self.assertEqual({t["file"] for t in self.package["textures"]}, {self.package["textures"][0]["file"]})
        self.assertEqual(len(self.package["states"]), 2)  # mode and shape; not one variable per key
        self.assertFalse(self.package["parts"][0]["initialVisible"])
        self.assertEqual(len(self.package["rules"][0]["targets"]), 2)
        self.assertEqual(self.package["parts"][0]["bonePaths"], ["Root", "Root/Bone"])
        self.assertFalse(any(p.suffix in (".json", ".texture", ".png") for p in self.root.rglob("*")))
        texture = self.package["textures"][0]
        self.assertEqual((texture["width"], texture["height"], texture["mipCount"], texture["format"]), (1, 1, 1, "RGBA32"))
        text = (self.root / "mod.ini").read_text(encoding="utf-8")
        self.assertNotIn("mergeContract", text)
        self.assertNotIn("boneSourceCandidates", text)
        self.assertNotIn("nativeLoadVerified", text)

    def test_add_second_key_without_adding_state(self):
        self.edit_ini(lambda text: text + "\n[KeyBackward]\nkey=NUMPAD2\ntype=cycle\n$mode=2,1,0\n")
        package = read_package(self.root)
        self.assertEqual(package["states"], self.package["states"])
        state = dict(package["states"])
        apply_key(state, package["keyBindings"][0])
        self.assertTrue(package["parts"][0]["visible"].evaluate(state))
        apply_key(state, package["keyBindings"][-1])
        self.assertFalse(package["parts"][0]["visible"].evaluate(state))

    def test_dangling_links_and_undefined_states(self):
        self.edit_ini(lambda text: text.replace("material=MaterialBody", "material=MaterialMissing"))
        with self.assertRaisesRegex(ValueError, "unresolved part"): read_package(self.root)
        self.edit_ini(lambda text: text.replace("material=MaterialMissing", "material=MaterialBody").replace("visible=", "visible=$unknown == 1 && "))
        with self.assertRaisesRegex(ValueError, "undefined state"): read_package(self.root)

    def test_unreferenced_files_and_corruption(self):
        (self.root / "original.pfb").write_bytes(b"source data")
        with self.assertRaisesRegex(ValueError, "unreferenced/source"): read_package(self.root)
        (self.root / "original.pfb").unlink()
        path = next((self.root / "meshes").iterdir())
        data = bytearray(path.read_bytes()); data[-1] ^= 1; path.write_bytes(data)
        with self.assertRaisesRegex(ValueError, "checksum"): read_package(self.root)

    def test_texture_settings_are_editable_in_ini(self):
        ini = ini_document(self.root)
        for section in ini.sections():
            if section.startswith('Texture'):
                ini[section].update(filter='2', wrap='1', linear='true', mip_bias='-0.5')
        save_ini(self.root, ini)
        updated = read_package(self.root)
        for texture in updated["textures"]:
            self.assertEqual((texture["filter"], texture["wrap"], texture["colorSpace"], texture["mipBias"]), (2, 1, 0, -0.5))
        self.assertEqual(updated["_files"], self.package["_files"])
        self.assertEqual(updated["textures"][0]["sha256"], self.package["textures"][0]["sha256"])

    def test_texture_declarations_are_checked_against_payload(self):
        original = (self.root/'mod.ini').read_bytes()
        for field, value in (('filter', '9'), ('aniso', '99'), ('linear', 'maybe'), ('mip_bias', 'nan')):
            with self.subTest(declaration=(field, value)):
                ini = ini_document(self.root)
                ini['TextureBody_D'][field] = value
                save_ini(self.root, ini)
                with self.assertRaises(ValueError): read_package(self.root)
                (self.root/'mod.ini').write_bytes(original)
        (self.root / "textures" / "old.texture").write_bytes(b"old sidecar")
        with self.assertRaisesRegex(ValueError, "unreferenced/source"): read_package(self.root)

    def test_shared_cycle_domain_and_structure_compatibility(self):
        original = (self.root/'mod.ini').read_bytes()
        self.edit_ini(lambda text: text + "\n[KeyBad]\nkey=NUMPAD2\ntype=cycle\n$mode=0,4\n")
        with self.assertRaisesRegex(ValueError, "same cycle domain"): read_package(self.root)
        (self.root/'mod.ini').write_bytes(original)
        self.edit_ini(lambda text: text.replace("format=2", "format=999"))
        self.assertEqual(read_package(self.root)['modId'], self.package['modId'])
        for path in self.root.rglob('*'):
            if path.suffix not in ('.mesh', '.mat', '.tex'): continue
            data = bytearray(path.read_bytes());data[6] = ord('9');data[8:12] = (999).to_bytes(4,'little');path.write_bytes(data)
        self.assertEqual(read_package(self.root)['modId'], self.package['modId'])

    def test_same_path_pixel_update_preserves_ini_and_other_resources(self):
        before = {p.relative_to(self.root): p.read_bytes() for p in self.root.rglob('*') if p.is_file()}
        textures = [dict(t, pixels=b'\xff\xff\xff\xff') for t in fixture()['textures']]
        updated = update_directory(self.root, textures=textures, texture_only=True)
        self.assertEqual((self.root/'mod.ini').read_bytes(), before[Path('mod.ini')])
        self.assertEqual([t['file'] for t in updated['textures']], [t['file'] for t in self.package['textures']])
        for path, data in before.items():
            if path.parts[0] != 'textures':
                self.assertEqual((self.root/path).read_bytes(), data)
        self.assertNotEqual(updated['textures'][0]['sha256'], self.package['textures'][0]['sha256'])

    def test_one_shared_texture_change_uses_new_image_name_without_overwriting_other(self):
        original = self.package['textures'][1]
        data = (self.root/original['file']).read_bytes()
        updated = update_directory(self.root, textures=[dict(fixture()['textures'][0], pixelLabel='alternate', pixels=b'\xff'*4)], texture_only=True)
        self.assertNotEqual(updated['textures'][0]['file'], updated['textures'][1]['file'])
        self.assertEqual(updated['textures'][0]['file'], 'textures/alternate.tex')
        self.assertEqual((self.root/original['file']).read_bytes(), data)

    def test_texture_export_updates_bindings_and_dimensions_without_material_binary(self):
        inputs = fixture()
        before = (self.root/'materials/MaterialBody.mat').read_bytes()
        texture = dict(inputs['textures'][0], sourceAsset='NewTexture', width=2, pixels=b'\xff'*8)
        material = dict(inputs['materials'][0], fieldData={'changed': b'ignored'},
                        textureEdits=[dict(property='_BaseMap', texture='tex0')])
        updated = update_directory(self.root, materials=[material], textures=[texture], texture_only=True)
        self.assertEqual(len(updated['textures']), 1)
        self.assertEqual(updated['textures'][0]['width'], 2)
        self.assertEqual(updated['materials'][0]['textureEdits'][0]['texture'], 'TextureNewTexture')
        self.assertEqual((self.root/'materials/MaterialBody.mat').read_bytes(), before)
        self.assertEqual(updated['textures'][0]['file'], 'textures/body.tex')
        self.assertEqual(decode_resource((self.root/'textures/body.tex').read_bytes(), 4)['blocks']['pixels'], b'\xff'*8)

    def test_mesh_update_preserves_controls_and_rejects_removed_bound_shape(self):
        before = (self.root/'mod.ini').read_bytes()
        part = dict(fixture()['parts'][0], vertices=6, indices=6)
        updated = update_directory(self.root, parts=[part])
        self.assertEqual(updated['parts'][0]['vertices'], 6)
        self.assertEqual((self.root/'mod.ini').read_bytes(), before)
        with self.assertRaisesRegex(ValueError, 'controlled shape'):
            update_directory(self.root, parts=[dict(part, shapeNames=[])])

    def test_material_update_removes_obsolete_texture_and_keeps_controls(self):
        material = dict(fixture()['materials'][0], textureEdits=[], fieldData={'m_Name': b'new'})
        updated = update_directory(self.root, materials=[material])
        self.assertEqual(updated['textures'], [])
        self.assertEqual(updated['states'], self.package['states'])
        self.assertEqual(updated['shapeControls'], self.package['shapeControls'])

    def test_format3_staging_accepts_sparse_material_without_native_fields(self):
        material = dict(fixture()['materials'][0], nativeTypeHash='00' * 16,
                        fieldData={}, textureEdits=[])
        update_directory(self.root, materials=[material])
        loaded = read_package(self.root)
        self.assertEqual(loaded['materials'][0]['nativeTypeHash'], '00' * 16)
        self.assertEqual(loaded['materials'][0]['nativeFields'], [])
        self.assertEqual(loaded['_resources']['materials/MaterialBody.mat']['blocks'], {})
        self.assertEqual(loaded['materials'][0]['textureEdits'], [])

    def test_different_images_with_same_filename_report_conflict(self):
        inputs = fixture()
        inputs['textures'][1]['pixels'] = b'\xff'*4
        with tempfile.TemporaryDirectory() as root:
            with self.assertRaisesRegex(ValueError, 'different images use the same TEX filename'):
                export_directory(root, **inputs)

    def test_same_pixels_with_different_image_names_keep_both_names(self):
        inputs = fixture()
        inputs['textures'][1]['pixelLabel'] = 'body_copy'
        with tempfile.TemporaryDirectory() as root:
            package = export_directory(root, **inputs)
            self.assertEqual([t['file'] for t in package['textures']], ['textures/body.tex', 'textures/body_copy.tex'])

    def test_partial_export_replaces_legacy_hash_names_with_image_names(self):
        ini = ini_document(self.root)
        old = 'textures/body_010de4985b31.tex'
        (self.root/'textures/body.tex').rename(self.root/old)
        for section in ('TextureBody_D', 'TextureBody_Copy'):
            ini[section]['path'] = old
        save_ini(self.root, ini)
        updated = update_directory(self.root, textures=fixture()['textures'], texture_only=True)
        self.assertEqual([t['file'] for t in updated['textures']], ['textures/body.tex', 'textures/body.tex'])
        self.assertFalse((self.root/old).exists())

    def test_partial_export_rejects_overwriting_unselected_shared_image(self):
        before = (self.root/'textures/body.tex').read_bytes()
        with self.assertRaisesRegex(ValueError, 'different images use the same TEX filename'):
            update_directory(self.root, textures=[dict(fixture()['textures'][0], pixels=b'\xff'*4)], texture_only=True)
        self.assertEqual((self.root/'textures/body.tex').read_bytes(), before)

    def test_reusable_tex_uses_embedded_dimensions_not_stale_ini(self):
        ini = ini_document(self.root)
        for section in ('TextureBody_D', 'TextureBody_Copy'):
            ini[section].update(width='2048', height='2048', mip_count='12', format='RGBA32')
        save_ini(self.root, ini)
        path = self.root/ini['TextureBody_D']['path']
        path.write_bytes(encode_resource(4, '00'*16, {
            '@texture': struct.pack('<5I', 1, 1024, 1024, 4, 1),
            'pixels': b'\xff'*(1024*1024*4)}))
        updated = read_package(self.root)
        self.assertEqual((updated['textures'][0]['width'], updated['textures'][0]['height']), (1024, 1024))

    def test_texture_ini_only_contains_path_and_nondefault_settings(self):
        ini = ini_document(self.root)
        self.assertEqual(dict(ini['TextureBody_D']), {'path': 'textures/body.tex'})
        inputs = fixture()
        inputs['textures'][0]['colorSpace'] = 0
        with tempfile.TemporaryDirectory() as root:
            export_directory(root, **inputs)
            self.assertEqual(dict(ini_document(root)['TextureBody_D']), {'path': 'textures/body.tex', 'linear': 'true'})

    def test_legacy_pixels_still_load_with_explicit_dimensions(self):
        ini = ini_document(self.root)
        for section in ('TextureBody_D', 'TextureBody_Copy'):
            ini[section].update(width='1', height='1', format='RGBA32', mip_count='1',
                                **{'source.path': 'assets/old', 'source.asset': 'Old'})
        save_ini(self.root, ini)
        path = self.root/ini['TextureBody_D']['path']
        path.write_bytes(encode_resource(4, '00'*16, {'pixels': fixture()['textures'][0]['pixels']}))
        self.assertEqual(read_package(self.root)['textures'][0]['width'], 1)
        ini['TextureBody_D']['width'] = '2'
        save_ini(self.root, ini)
        with self.assertRaisesRegex(ValueError, 'pixel size'):
            read_package(self.root)


if __name__ == "__main__": unittest.main()

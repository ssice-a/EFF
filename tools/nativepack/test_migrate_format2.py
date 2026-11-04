import hashlib
import importlib.util
import json
import sys
import unittest
from types import SimpleNamespace
from unittest.mock import patch

from tools.nativepack import author_format3
from tools.nativepack.migrate_format2 import (
    _prepare_textures,
    _srgb_property,
    _verify_effect_resource_payload,
)


class TextureColorSpaceTests(unittest.TestCase):
    def test_diffuse_and_emission_slots_are_srgb(self):
        for property_name in ("_BaseMap", "_MainTex", "_AlbedoMap", "_BaseColorMap",
                              "_DiffuseMap", "_EmissionMap"):
            with self.subTest(property_name=property_name):
                self.assertTrue(_srgb_property(property_name))

    def test_non_color_texture_slots_remain_linear(self):
        for property_name in ("_BumpMap", "_MetallicGlossMap", "_BlendTex",
                              "_MaskTex", "_VFXSpecialMainTex"):
            with self.subTest(property_name=property_name):
                self.assertFalse(_srgb_property(property_name))


class TextureReuseTests(unittest.TestCase):
    def test_identical_textures_share_an_id_only_when_sampling_matches(self):
        class Source:
            def __init__(self):
                self.materials = {}
                self._resources_by_identity = {}

            def find_all(self, kind, path, asset):
                if kind != "Material":
                    return ()
                value = self.materials.get((path, asset))
                return (value,) if value is not None else ()

        source = Source()
        pixels = bytes((10, 20, 30, 255)) * 4
        texture_rows = []
        materials = []
        resources = {}
        cases = (
            ("TextureA", "MaterialA", "_BaseMap", "source-a", 1),
            ("TextureB", "MaterialB", "_BaseMap", "source-b", 1),
            ("TextureC", "MaterialC", "_BumpMap", "source-c", 1),
            ("TextureD", "MaterialD", "_BaseMap", "source-d", 2),
        )
        for texture_id, material_id, prop, identity, filter_mode in cases:
            material_path = "assets/materials/" + material_id + ".mat"
            source.materials[(material_path, material_id)] = {
                "details": {"textures": [{
                    "property": prop,
                    "pointer": {"identity": identity, "type": "Texture2D", "resolved": True},
                }]},
            }
            source._resources_by_identity[identity] = {
                "identity": identity,
                "logicalPath": "assets/textures/" + texture_id + ".asset",
                "name": texture_id,
                "details": {"format": "RGBA32"},
            }
            relative = "textures/" + texture_id + ".tex"
            texture_rows.append({
                "id": texture_id, "file": relative, "width": 2, "height": 2,
                "mipCount": 1, "format": "RGBA32", "filter": filter_mode,
                "wrap": 0, "aniso": 1, "mipBias": 0.0,
            })
            resources[relative] = {"blocks": {"pixels": pixels}}
            materials.append({
                "id": material_id, "sourcePath": material_path,
                "sourceAsset": material_id,
                "textureEdits": [{"property": prop, "texture": texture_id}],
            })

        textures, remap = _prepare_textures({
            "textures": texture_rows, "materials": materials,
            "_resources": resources,
        }, [source], texconv=None)

        self.assertEqual(len(textures), 3)
        self.assertEqual(remap[("MaterialA", "_BaseMap")],
                         remap[("MaterialB", "_BaseMap")])
        self.assertNotEqual(remap[("MaterialA", "_BaseMap")],
                            remap[("MaterialC", "_BumpMap")])
        self.assertNotEqual(remap[("MaterialA", "_BaseMap")],
                            remap[("MaterialD", "_BaseMap")])


class TextureRowOrderTests(unittest.TestCase):
    def test_format2_texture_compression_preserves_native_row_order(self):
        class Source:
            def __init__(self):
                self.material = {
                    "details": {"textures": [{
                        "property": "_BaseMap",
                        "pointer": {"identity": "texture-source", "type": "Texture2D", "resolved": True},
                    }]}
                }
                self.texture = {
                    "identity": "texture-source",
                    "logicalPath": "assets/textures/body.asset",
                    "name": "body",
                    "details": {"format": "BC7"},
                }
                self._resources_by_identity = {"texture-source": self.texture}

            def find_all(self, kind, path, asset):
                if kind == "Material" and path == "assets/materials/body.mat" and asset == "body":
                    return (self.material,)
                return ()

        source = Source()
        pixels = bytes((1, 2, 3, 255, 4, 5, 6, 255,
                        7, 8, 9, 255, 10, 11, 12, 255))
        calls = []

        def compress_rgba(data, width, height, mipmaps, source_format,
                          color_space, texconv, *, flip_y):
            calls.append((bytes(data), flip_y))
            return {"mipCount": 1, "format": "BC7", "formatId": 25,
                    "pixels": b"compressed"}

        package = {
            "textures": [{
                "id": "TextureBody", "file": "textures/body.tex",
                "width": 2, "height": 2, "mipCount": 1, "format": "BC7",
                "filter": 1, "wrap": 0, "aniso": 1, "mipBias": 0.0,
            }],
            "materials": [{
                "id": "MaterialBody", "sourcePath": "assets/materials/body.mat",
                "sourceAsset": "body",
                "textureEdits": [{"property": "_BaseMap", "texture": "TextureBody"}],
            }],
            "_resources": {"textures/body.tex": {"blocks": {"pixels": pixels}}},
        }

        with patch.dict(sys.modules, {
            "eiem_texture_compression": SimpleNamespace(compress_rgba=compress_rgba),
        }):
            textures, _remap = _prepare_textures(package, [source], texconv="unused")

        self.assertEqual(calls, [(pixels, False)])
        self.assertEqual(textures[0]["pixels"], b"compressed")


class EffectResourceMigrationTests(unittest.TestCase):
    def test_format3_envelope_preserves_the_original_bundle_bytes(self):
        from pathlib import Path
        from tempfile import TemporaryDirectory

        relative = "effects/" + "a" * 64 + "/0.effres"
        resource_id = "ResourceEffect_" + hashlib.sha256(
            relative.encode("utf-8")).digest()[:8].hex().upper()
        native = b"UnityFS\x00legacy effect bundle"
        with TemporaryDirectory() as temporary:
            path = Path(temporary) / "effect.effres"
            path.write_bytes(author_format3.encode_effect_resource(resource_id, relative, native))
            _verify_effect_resource_payload(path, relative, native)
            with self.assertRaises(ValueError):
                _verify_effect_resource_payload(path, relative, native + b"changed")


class FrozenEffectImportTests(unittest.TestCase):
    def test_top_level_effect_document_resolves_tools_nativepack(self):
        from pathlib import Path
        from tempfile import TemporaryDirectory

        module_path = Path(__file__).resolve().parents[1] / "Blender" / "eiem_effect_document.py"
        spec = importlib.util.spec_from_file_location("eiem_effect_document", module_path)
        if spec is None or spec.loader is None:
            raise RuntimeError("Could not load the standalone effect document module.")
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)

        catalog = json.dumps({
            "format": "EFF_EFFECT_SOURCE", "version": 1, "objects": [],
        }, separators=(",", ":")).encode("utf-8")
        baseline = b"{}"
        receipt = json.dumps({
            "format": "EFF_GENERATED_FILES/1",
            "files": {
                "effect.json": hashlib.sha256(catalog).hexdigest().upper(),
                "source/manifest.json": hashlib.sha256(baseline).hexdigest().upper(),
            },
        }, separators=(",", ":")).encode("utf-8")

        with TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / "source").mkdir()
            (root / "effect.json").write_bytes(catalog)
            (root / "source" / "manifest.json").write_bytes(baseline)
            (root / "source" / "generated-files.json").write_bytes(receipt)
            with patch.dict(sys.modules, {"nativepack": None}):
                state = module.load_source(root)

        self.assertEqual(state["format"], "EFF_EFFECT_WORKSPACE")


if __name__ == "__main__":
    unittest.main()

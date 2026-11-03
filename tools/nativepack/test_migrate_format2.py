import hashlib
import unittest

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


if __name__ == "__main__":
    unittest.main()

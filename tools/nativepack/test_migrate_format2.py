import hashlib
import unittest

from tools.nativepack import author_format3
from tools.nativepack.migrate_format2 import _srgb_property, _verify_effect_resource_payload


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

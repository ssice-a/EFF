import hashlib
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

from . import effect_native


def _text(value):
    data = value.encode("utf-8")
    return len(data).to_bytes(8, "little") + data


class EffectNativeReaderTests(unittest.TestCase):
    def test_keeps_bundle_root_index_when_resolving_bundle_path(self):
        with tempfile.TemporaryDirectory() as temporary:
            build = Path(temporary)
            bundle_path = build / "runtime" / "bundles" / "one.ab"
            bundle_path.parent.mkdir(parents=True)
            bundle = b"native envelope fixture"
            bundle_path.write_bytes(bundle)

            runtime = b"".join((
                _text("EFF_EFFECT_DEBUG_NATIVE/inactive-root+le64"),
                _text("effect-key"),
                _text("assets/example.prefab"),
                _text("ExampleEffect"),
                (0).to_bytes(8, "little"),
                (7).to_bytes(8, "little"),
                (1).to_bytes(8, "little"),
                _text("runtime/bundles/one.ab"),
                _text(hashlib.sha256(bundle).hexdigest().upper()),
                (0).to_bytes(8, "little"),
            ))
            (build / "runtime.bin").write_bytes(runtime)

            with patch.object(effect_native, "native_archive", return_value=True):
                result = effect_native.read_native_build(build)

            self.assertEqual(result["root"], 0)
            self.assertIs(type(result["root"]), int)
            self.assertEqual(result["particles"], 7)
            self.assertEqual(result["bundles"][0]["data"], bundle)


if __name__ == "__main__":
    unittest.main()

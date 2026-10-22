"""Pure policy tests share their contract fixtures with the C# preview."""
import importlib.util
import json
from pathlib import Path
import unittest

root = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("naraka_alpha", root / "naraka_blender/alpha.py")
alpha = importlib.util.module_from_spec(spec)
spec.loader.exec_module(alpha)


class AlphaPolicyTests(unittest.TestCase):
    def test_shared_contracts(self):
        fixture_path = root / "tests/alpha_policy_cases.json"
        # Each repository can run independently; in the shared workspace the
        # contract fixtures must also stay byte-identical across languages.
        csharp_fixture = root.parent / "AnimeStudio/AnimeStudio.PreviewTests/alpha_policy_cases.json"
        if csharp_fixture.is_file():
            self.assertEqual(fixture_path.read_bytes(), csharp_fixture.read_bytes())
        fixtures = json.loads(fixture_path.read_text("utf-8"))
        for fixture in fixtures:
            with self.subTest(fixture=fixture["name"]):
                actual = alpha.resolve_alpha(fixture)
                for key, value in fixture["expected"].items():
                    self.assertEqual(actual[key], value, key)


if __name__ == "__main__":
    unittest.main()

"""Integration checks against the canonical Blender/Pillow EFFRES2 encoder."""
from __future__ import annotations

import argparse
import configparser
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

from PIL import Image

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from nativepack.resource_file import PIXELS, decode_resource, encode_resource


def declaration(path, width, height, mips, section="TextureTest"):
    return f"""[{section}]
path={path}
source.path=assets/test.tga
source.asset=Test
width={width}
height={height}
format=RGBA32
mip_count={mips}
linear=false
filter=1
wrap=0
aniso=1
mip_bias=0.0
"""


def reference(png, mips):
    with Image.open(png) as source:
        mip = source.convert("RGBA")
        pixels = bytearray()
        for i in range(mips):
            pixels.extend(mip.transpose(Image.Transpose.FLIP_TOP_BOTTOM).tobytes())
            if i + 1 < mips:
                mip = mip.resize((max(1, mip.width // 2), max(1, mip.height // 2)), Image.Resampling.BOX)
    return encode_resource(PIXELS, "00" * 16, {"pixels": pixels})


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--exe", type=Path, required=True)
    parser.add_argument("--installed-mod", type=Path, required=True)
    parser.add_argument("--samples", type=Path, required=True)
    parser.add_argument("--report", type=Path, required=True)
    args = parser.parse_args()
    results = []
    installed_before = {str(p): hashlib.sha256(p.read_bytes()).hexdigest()
                        for p in args.installed_mod.rglob("*") if p.is_file()}
    with tempfile.TemporaryDirectory(prefix="eff-png-", dir=args.report.parent) as temporary:
        root = Path(temporary)
        tools = root / "plugin" / "tools"
        mod = root / "plugin" / "mods" / "测试 Mod"
        inputs = root / "PNG 输入"
        tools.mkdir(parents=True)
        mod.mkdir(parents=True)
        inputs.mkdir()
        exe = tools / "eff_texture_convert.exe"
        shutil.copyfile(args.exe, exe)
        ini = mod / "mod.ini"

        def invoke(*arguments, ok=True):
            proc = subprocess.run([str(exe), "--no-pause", *map(str, arguments)], capture_output=True,
                                  encoding="utf-8", errors="replace", timeout=30)
            if (proc.returncode == 0) != ok:
                raise AssertionError(proc.stdout + proc.stderr)
            return proc

        def compare(png, output, mips, label):
            actual, expected = output.read_bytes(), reference(png, mips)
            assert actual == expected, (label, "differs from Blender/Pillow bytes")
            decode_resource(actual, PIXELS)
            results.append({"case": label, "bytes": len(actual), "sha256": hashlib.sha256(actual).hexdigest()})

        for size in [(8, 8), (5, 7), (1, 9), (9, 1), (1, 1)]:
            w, h = size
            png = inputs / "rgba.png"
            image = Image.new("RGBA", size)
            image.putdata([((i * 23) % 256, (i * 31) % 256, (i * 79) % 256, (i * 37) % 256)
                           for i in range(w * h)])
            image.save(png)
            mips = max(w.bit_length(), h.bit_length())
            ini.write_text("[Mod]\nformat=2\nid=Test\n" + declaration("textures/rgba_012345abcdef.tex", w, h, mips), encoding="utf-8")
            invoke(png)
            output = mod / "textures" / "rgba.tex"
            compare(png, output, mips, f"RGBA transparency and BOX {w}x{h}")
            assert 'path=textures/rgba.tex' in ini.read_text('utf8')
            assert not (mod / 'textures/rgba_012345abcdef.tex').exists()
            # A second successful conversion preserves the previous output outside Mod.
            invoke(ini, png)
            backups = list((root / "plugin" / "texture-convert-backups").rglob('*.previous.tex'))
            assert any(p.read_bytes() == output.read_bytes() for p in backups)

        for mode in ("RGB", "L", "P"):
            png = inputs / "other.png"
            image = Image.new(mode, (4, 4))
            if mode == "P":
                image.putpalette([v for i in range(256) for v in (i, 255 - i, (i * 17) % 256)])
                image.putdata(range(16))
                image.info["transparency"] = bytes(range(256))
            elif mode == "RGB":
                image.putdata([(i * 7, i * 11, i * 13) for i in range(16)])
            else:
                image.putdata([i * 13 for i in range(16)])
            image.save(png)
            ini.write_text("[Mod]\nformat=2\nid=Test\n" + declaration("textures/other.tex", 4, 4, 3), encoding="utf-8")
            invoke("--mod", mod, png)
            compare(png, mod / "textures/other.tex", 3, f"PNG mode {mode}")

        # Real LZY declarations, including shared pixel files, converted in a copied Mod.
        shutil.copyfile(args.installed_mod / "mod.ini", ini)
        settings = configparser.ConfigParser(interpolation=None)
        settings.read(ini, encoding="utf-8-sig")
        shared = {}
        pngs = []
        for name in settings.sections():
            if not name.startswith("Texture"):
                continue
            row = settings[name]
            relative = row["path"]
            if relative in shared:
                continue
            stem = Path(relative).stem.rsplit("_", 1)[0]
            png = inputs / (stem + ".png")
            shutil.copyfile(args.samples / png.name, png)
            shared[relative] = (png, int(row["mip_count"]))
            pngs.append(png)
        proc = invoke(*pngs)
        results.append({"case": "real LZY batch", "stdout": proc.stdout.strip()})
        for relative, (png, mips) in shared.items():
            compare(png, mod / 'textures' / (png.stem + '.tex'), mips, "LZY " + png.name)
        renamed=configparser.ConfigParser(interpolation=None);renamed.read(ini,encoding='utf8')
        for name in settings.sections():
            if name.startswith('Texture'):
                png=shared[settings[name]['path']][0]
                assert renamed[name]['path']=='textures/'+png.stem+'.tex'
        results.append({'case':'all shared Texture.path entries renamed with PNG; material IDs unchanged','passed':True})
        cloth = next(png for png in pngs if png.stem == 'clothes_D')
        invoke('--mod', mod, '--texture', 'TextureT_actor_lizhiyan_cloth_01_D', cloth)
        compare(cloth, mod / 'textures/clothes_D.tex', 12, 'shared texture alternate INI section selection')

        # Failures must preserve previously converted outputs and not alter INI.
        def snapshot():
            return {str(p.relative_to(mod)): p.read_bytes() for p in mod.rglob("*") if p.is_file()}

        before = snapshot()
        unknown = inputs / "unmatched.png"
        Image.new("RGBA", (1, 1)).save(unknown)
        invoke(unknown, ok=False)
        invoke(pngs[0], pngs[0], ok=False)
        bad = inputs / "body.png"
        Image.new("RGBA", (2, 2)).save(bad)
        invoke(bad, ok=False)
        bad.write_bytes(b"not a PNG")
        invoke(bad, ok=False)
        assert before == snapshot()
        results.append({"case": "unknown name / duplicate output / wrong size / invalid PNG preserve Mod", "passed": True})

        other = mod.parent / "Another Mod"
        other.mkdir()
        shutil.copyfile(ini, other / "mod.ini")
        Image.new("RGBA", (1024, 1024), (70, 130, 160, 255)).save(bad)
        invoke(bad, ok=False)
        invoke(ini, bad)
        compare(bad, mod / "textures/body.tex", 11, "explicit mod.ini disambiguation")
        exact = inputs / "body_010de4985b31.png"
        shutil.copyfile(bad, exact)
        invoke("--mod", mod, exact)
        compare(exact, mod / "textures/body_010de4985b31.tex", 11, "exact Texture.path stem")
        # A portable converter and editable PNG can live inside Mod/textures.
        portable=mod/'textures/eff_texture_convert.exe';shutil.copyfile(exe,portable)
        white=mod/'textures/WHITE.png';shutil.copyfile(bad,white)
        result=subprocess.run([str(portable),'--no-pause','--texture','TextureT_actor_lizhiyan_body_01_D',str(white)],capture_output=True,encoding='utf8',errors='replace',timeout=30)
        assert result.returncode==0,result.stdout+result.stderr
        compare(white,mod/'textures/WHITE.tex',11,'portable converter, in-Mod PNG, arbitrary same-name output')
        assert 'path=textures/WHITE.tex' in ini.read_text('utf8')
        assert not (mod/'textures/body_010de4985b31.tex').exists()
        assert white.exists()

    installed_after = {str(p): hashlib.sha256(p.read_bytes()).hexdigest()
                       for p in args.installed_mod.rglob("*") if p.is_file()}
    assert installed_before == installed_after, "installed Mod changed during isolated tests"
    report = {"passed": True, "exeSha256": hashlib.sha256(args.exe.read_bytes()).hexdigest(),
              "installedModUnchanged": True, "results": results}
    args.report.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    print(f"PASS {len(results)} checks; canonical EFFRES2/Pillow byte equality; installed Mod unchanged")


if __name__ == "__main__":
    main()

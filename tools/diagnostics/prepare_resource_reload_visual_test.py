"""Create an isolated cyan body-texture witness; never edit the installed Mod."""
import argparse
import configparser
import hashlib
import json
from pathlib import Path
import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from nativepack.ini_reader import read_package
from nativepack.resource_file import decode_resource, encode_resource, PIXELS

def sha(data):
    return hashlib.sha256(data).hexdigest().upper()

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("mod", type=Path)
    p.add_argument("new_test", type=Path)
    args = p.parse_args()
    if args.new_test.exists():
        raise ValueError("Refusing to overwrite an existing visual test")
    package = read_package(args.mod)
    files = {name: (args.mod / name).read_bytes() for name in package["_files"]}
    ini = configparser.ConfigParser(interpolation=None)
    ini.optionxform = str
    ini.read_string(files["mod.ini"].decode("utf-8-sig"))
    pixel_file = ini["TextureT_actor_lizhiyan_body_01_D"]["path"]
    decoded = decode_resource(files[pixel_file], PIXELS)
    original = decoded["blocks"]["pixels"]
    color = bytearray(original)
    # All mip levels are RGBA32. Preserve alpha and every declaration/layout.
    color[0::4] = bytes([16]) * (len(color) // 4)
    color[1::4] = bytes([220]) * (len(color) // 4)
    color[2::4] = bytes([160]) * (len(color) // 4)
    files[pixel_file] = encode_resource(PIXELS, decoded["nativeTypeHash"], {"pixels": color})
    author = args.new_test / "author"
    for name, data in files.items():
        path = author / name
        path.parent.mkdir(parents=True, exist_ok=True)
        with path.open("xb") as f:
            f.write(data)
    read_package(author)
    production_label = ini["Mod"]["id"] + ":" + sha((args.mod / "mod.ini").read_bytes()) + "\n"
    for name in sorted(set(files) - {"mod.ini"}):
        production_label += name + ":" + sha((args.mod / name).read_bytes()) + "\n"
    production_digest = sha(production_label.encode("utf8"))
    report = {"format": "EFF_RESOURCE_RELOAD_VISUAL_TEST", "version": 1,
              "productionFilesEdited": False, "productionDigest": production_digest,
              "firstF10": "cyan body BaseMap; unchanged alpha, dimensions, all mip levels and original/custom selections",
              "secondF10": "original installed Mod graph", "onlyChangedFile": pixel_file,
              "originalPixelSha256": sha(original), "testPixelSha256": sha(color),
              "alphaPreserved": color[3::4] == original[3::4], "nativeRuntimeVerified": False}
    with (args.new_test / "visual-test.json").open("x", encoding="utf8") as f:
        json.dump(report, f, ensure_ascii=False, indent=2)
        f.write("\n")
    print(f"PASS visual witness authorFiles={len(files)} changedPixelResource=1 alphaPreserved=1 productionDigest={production_digest}")

if __name__ == "__main__":
    main()

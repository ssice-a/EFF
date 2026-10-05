"""Compare AnimeStudio-redecoded native textures against the captured author PNGs.

Consumes the independent native verifier's successful report. Writes fresh
pixel evidence; never edits source resources, Blender scenes or the game.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import traceback

from PIL import Image


def rgba(path):
    with Image.open(path) as image:
        pixels = image.convert("RGBA")
        return pixels.size, pixels.tobytes()


def verify(report_path):
    report = json.loads(report_path.read_text(encoding="utf-8"))
    if not (report.get("format") == "EFF_NATIVE_WRITE_VERIFICATION" and
            report.get("verified") and report.get("offlineOnly") and
            report.get("nativeLoadVerified") is False):
        raise ValueError("Requires a successful offline native verification report")
    textures = []
    for row in report["textures"]:
        author_size, author_pixels = rgba(row["authorPng"])
        decoded_size, decoded_pixels = rgba(row["redecodedPng"])
        if author_size != decoded_size or author_pixels != decoded_pixels:
            raise ValueError("Native PNG pixels differ from author: " + row["texture"])
        textures.append(dict(identity=row["texture"], width=author_size[0], height=author_size[1],
                             pixelBytes=len(author_pixels),
                             rgbaTopDownSha256=hashlib.sha256(author_pixels).hexdigest().upper(),
                             authorPng=row["authorPng"], redecodedPng=row["redecodedPng"],
                             byteIdentical=True))
    if not textures:
        raise ValueError("Native texture evidence is empty")
    return dict(format="EFF_NATIVE_TEXTURE_PIXEL_VERIFICATION", version=1,
                verified=True, offlineOnly=True, nativeLoadVerified=False,
                nativeReport=str(report_path.resolve()), textures=textures)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--native-report", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if args.output.exists():
        raise FileExistsError("Refusing to overwrite evidence: " + str(args.output))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    try:
        result = verify(args.native_report)
    except Exception as error:
        result = dict(verified=False, nativeLoadVerified=False,
                      stage="independent-PNG-pixel-comparison", error=str(error),
                      traceback=traceback.format_exc())
    args.output.write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(dict(verified=result["verified"], nativeLoadVerified=False,
                         textures=len(result.get("textures", [])), report=str(args.output.resolve())), ensure_ascii=False))
    return 0 if result["verified"] else 1


if __name__ == "__main__":
    raise SystemExit(main())

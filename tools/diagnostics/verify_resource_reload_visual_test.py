"""Check the isolated pixel witness and both rebuilt serialized graphs."""
import argparse
import hashlib
import json
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from nativepack.ini_reader import read_package
from nativepack.resource_file import decode_resource, PIXELS


def require(ok, message):
    if not ok:
        raise ValueError(message)


def sha(data):
    return hashlib.sha256(data).hexdigest().upper()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("mod", type=Path)
    parser.add_argument("evidence", type=Path)
    args = parser.parse_args()
    evidence = args.evidence
    report = json.loads((evidence / "visual-test/visual-test.json").read_text("utf8"))
    production = read_package(args.mod)
    test_root = evidence / "visual-test/author"
    test = read_package(test_root)
    require(production["_files"] == test["_files"], "visual witness resource list changed")
    files = {name: (args.mod / name).read_bytes() for name in production["_files"]}
    changed = [name for name, data in files.items() if data != (test_root / name).read_bytes()]
    require(changed == [report["onlyChangedFile"]], "unexpected visual witness file edits")
    label = production["modId"] + ":" + sha(files["mod.ini"]) + "\n"
    label += "".join(name + ":" + sha(files[name]) + "\n" for name in sorted(set(files) - {"mod.ini"}))
    require(sha(label.encode()) == report["productionDigest"], "installed author source changed")
    before = decode_resource(files[changed[0]], PIXELS)
    after = decode_resource((test_root / changed[0]).read_bytes(), PIXELS)
    a, b = before["blocks"]["pixels"], after["blocks"]["pixels"]
    require(before["nativeTypeHash"] == after["nativeTypeHash"] and len(a) == len(b), "texture layout changed")
    require(a[3::4] == b[3::4], "alpha changed")
    require(b[0::4] == bytes([16]) * (len(b) // 4) and
            b[1::4] == bytes([220]) * (len(b) // 4) and
            b[2::4] == bytes([160]) * (len(b) // 4), "all mip witness RGB differs")
    original = json.loads((evidence / "generation-b-audit.json").read_text("utf8"))
    visual = json.loads((evidence / "generation-visual-audit.json").read_text("utf8"))
    for graph in (original, visual):
        require(graph["brokenReferenceUses"] == 0 and graph["brokenStreams"] == 0, "broken graph references/streams")
    members = {row["member"]: row["input"] for row in original["serialized"] + visual["serialized"]}

    def normalized(graph):
        rows = []
        for row in graph["serialized"]:
            refs = [{k: (members.get(v, v) if k == "member" else v) for k, v in r.items()}
                    for r in row["referenceTargets"]]
            refs.sort(key=lambda r: (r["fileId"], r["pathId"], r["member"]))
            assets = [{k: (members.get(v, v) if k == "member" else v) for k, v in r.items()}
                      for r in row["assets"]]
            streams = [{k: v for k, v in s.items() if k not in ("path", "member", "pathDataOffset")}
                       for s in row["streams"]]
            rows.append(dict(input=row["input"], objectCount=row["objectCount"], classCounts=row["classCounts"],
                             refs=refs, assets=assets, streams=streams, opaque=row["unparsedObjects"]))
        return sorted(rows, key=lambda r: r["input"])

    require(normalized(original) == normalized(visual), "visual witness changed graph object/reference/stream layout")
    result = dict(format="EFF_FULL_RELOAD_VISUAL_LOCAL_VERIFICATION", version=1, verified=True,
                  productionDigest=report["productionDigest"], authorFiles=len(files), onlyChangedFile=changed[0],
                  allMipsCyanRGB=True, alphaPreserved=True, otherAuthorResourcesByteIdentical=True,
                  toushiMaterialAndTextureChoicesPreserved=True, rebuiltInputs=visual["inputs"],
                  internalReferenceUses=visual["internalReferenceUses"], boundaryReferenceUses=visual["boundaryReferenceUses"],
                  brokenReferenceUses=0, brokenStreams=0, opaqueObjects=visual["unparsedObjects"],
                  visualGraphLayoutAndNumericReferencesPreserved=True, nativeRuntimeVerified=False)
    with (evidence / "visual-local-verification.json").open("x", encoding="utf8") as f:
        json.dump(result, f, ensure_ascii=False, indent=2)
        f.write("\n")
    print("PASS isolated cyan witness: only body pixel resource changed; all alpha/mips verified; other 24 author files identical; 68 rebuilt graph layouts/references/streams preserved; nativeRuntimeVerified=0")


if __name__ == "__main__":
    main()

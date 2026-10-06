"""Stage a hash-checked pristine baseline for the DLL's resource worker."""
import argparse
import hashlib
import json
from pathlib import Path

def sha(data):
    return hashlib.sha256(data).hexdigest().upper()

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("pristine", type=Path)
    parser.add_argument("assembly", type=Path)
    parser.add_argument("new_output", type=Path)
    parser.add_argument("--root", action="append", required=True,
                        help="精确的源 GameObject 容器路径；每个世界/UI/其他根分别传入")
    args = parser.parse_args()
    roots = sorted(set(args.root))
    if len(roots) != len(args.root) or not roots or any(
            not root.startswith('assets/') or '..' in root or '\\' in root or
            any(ch in root for ch in '\r\n\t') for root in roots):
        raise ValueError('Root paths must be unique nonempty exact assets/ container paths')
    if args.new_output.exists():
        raise ValueError("Refusing to overwrite an existing baseline")
    snapshots = json.loads((args.pristine / "snapshots.json").read_text("utf8"))
    if snapshots["format"] != "EFF_CURRENT_INPUT_SNAPSHOTS":
        raise ValueError("Unexpected source baseline format")
    inputs = {}
    for row in snapshots["inputs"]:
        relative = Path(row["file"])
        if relative.is_absolute() or ".." in relative.parts:
            raise ValueError("Source path escapes pristine directory")
        data = (args.pristine / relative).read_bytes()
        if len(data) != row["bytes"] or sha(data) != row["sha256"] or row["outerEncrypted"]:
            raise ValueError("Pristine source SHA/size/encryption differs: " + str(relative))
        with Path(row["physicalPath"]).open("rb") as f:
            f.seek(row["offset"])
            if f.read(row["bytes"]) != data:
                raise ValueError("Installed game's pristine physical range differs")
        if relative.name in inputs:
            raise ValueError("Duplicate flat baseline name")
        inputs[relative.name] = data
    args.new_output.mkdir(parents=True)
    (args.new_output / "inputs").mkdir()
    rows = ["EFF_RESOURCE_RELOAD_BASELINE\t1"]
    for name, data in sorted(inputs.items()):
        with (args.new_output / "inputs" / name).open("xb") as f:
            f.write(data)
        rows.append("input\t" + name + "\t" + sha(data))
    for root in roots:
        rows.append("root\t" + root + "\tGameObject")
    with args.assembly.open("rb") as f:
        assembly_hash = hashlib.file_digest(f, "sha256").hexdigest().upper()
    with (args.new_output / "assembly.sha256").open("x", encoding="ascii") as f:
        f.write(assembly_hash + "\n")
    with (args.new_output / "baseline.tsv").open("x", encoding="utf8", newline="\n") as f:
        f.write("\n".join(rows) + "\n")
    ranges = ["EFF_RESOURCE_SOURCE_RANGES\t1"]
    for row in sorted(snapshots["inputs"], key=lambda row: Path(row["file"]).name):
        ranges.append("\t".join((Path(row["file"]).name, row["physicalPath"], str(row["offset"]),
                                  str(row["bytes"]), row["sha256"])))
    (args.new_output / "source-ranges.tsv").write_text("\n".join(ranges) + "\n", encoding="utf8")
    print(f"PASS staged pristine baseline inputs={len(inputs)} physicalRangesVerified=1 assemblySha256={assembly_hash}")

if __name__ == "__main__":
    main()

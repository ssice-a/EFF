"""Compare complete cold rebuild outputs and report wall/CPU preparation costs."""
import argparse
import hashlib
import json
from pathlib import Path
import re


def ready(folder):
    lines = (folder / "ready.tsv").read_text("utf8").splitlines()
    header = lines[0].split("\t")
    assert len(header) == 5 and header[:2] == ["EFF_RESOURCE_RELOAD_GENERATION", "1"]
    rows = {}
    for line in lines[1:]:
        kind, name, digest = line.split("\t")
        assert kind == "input" and name == Path(name).name and name not in rows
        data = (folder / name).read_bytes()
        assert hashlib.sha256(data).hexdigest().upper() == digest, name + " checksum differs"
        rows[name] = (digest, len(data))
    return header, rows


def timing(log):
    text = log.read_text("utf-8-sig")
    total = re.search(r"TIMING elapsedMs=(\S+) peakWorkingSetBytes=(\d+)", text)
    assert total and "PASS resource plan" in text
    result = dict(wallMs=float(total[1]), peakWorkingSetBytes=int(total[2]), steps={})
    cpu = re.search(r"CPU userMs=(\S+) kernelMs=(\S+)", text)
    if cpu:
        result.update(userCpuMs=float(cpu[1]), kernelCpuMs=float(cpu[2]))
    for name, ms, calls in re.findall(r"COST (\S+) elapsedMs=(\S+) calls=(\d+)", text):
        result["steps"][name] = dict(wallMs=float(ms), calls=int(calls))
    bounds = re.search(r"BOUNDS hits=(\d+) misses=(\d+)", text)
    if bounds:
        result["bounds"] = dict(hits=int(bounds[1]), misses=int(bounds[2]))
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("reference", "candidate", "reference-log", "candidate-log", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    args = parser.parse_args()
    before, before_rows = ready(args.reference)
    after, after_rows = ready(args.candidate)
    assert before == after and before_rows == after_rows, "native candidate output content changed"
    baseline, candidate = timing(args.reference_log), timing(args.candidate_log)
    report = dict(format="EFF_RESOURCE_RELOAD_PREPARATION_COMPARISON", version=1, verified=True,
                  coldRebuild=True, runtimeApisUsed=False, inputs=len(after_rows),
                  nativeInputBytes=sum(row[1] for row in after_rows.values()), generationKey=after[2],
                  allOutputBytesIdentical=True, reference=baseline, candidate=candidate,
                  wallSpeedup=baseline["wallMs"] / candidate["wallMs"],
                  nativeMigrationAccepted=False, inGamePerformanceMeasured=False)
    with args.output.open("x", encoding="utf8") as file:
        json.dump(report, file, indent=2)
        file.write("\n")
    print(json.dumps(report))


if __name__ == "__main__":
    main()

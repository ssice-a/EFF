"""Summarize full-resource transactions without treating mocks as runtime evidence."""
import argparse
from collections import defaultdict
import json
from pathlib import Path
import re


def summarize(text):
    rows = defaultdict(list)
    for line in text.splitlines():
        match = re.search(r"\[(FULL-RELOAD-[A-Z-]+)\]\s*(.*)", line)
        if match:
            fields = {m.group(1): m.group(2) for m in re.finditer(r"(\w+)=(.*?)(?=\s+\w+=|$)", match.group(2))}
            rows[match.group(1)].append(fields)
    commits = rows["FULL-RELOAD-COMMIT"]
    errors = rows["FULL-RELOAD-REJECT"] + rows["FULL-RELOAD-ROLLBACK-GAP"] + rows["FULL-RELOAD-DRAIN-GAP"]
    rollbacks = rows["FULL-RELOAD-ROLLBACK"]
    migrated = rows["FULL-RELOAD-MIGRATED"]
    verified = bool(rows["FULL-RELOAD-READY"] and commits and migrated) and not errors
    return dict(format="EFF_FULL_RESOURCE_RELOAD_RUNTIME_FINDINGS", version=1,
                ready=bool(rows["FULL-RELOAD-READY"]), prepared=rows["FULL-RELOAD-PREPARED"],
                preparationProgress=rows["FULL-RELOAD-PROGRESS"], workerCosts=rows["FULL-RELOAD-COST"],
                prewarmReady=rows["FULL-RELOAD-PREWARM-READY"], requests=rows["FULL-RELOAD-REQUEST"],
                prewarmWorldWaits=rows["FULL-RELOAD-PREWARM-WAIT"], prewarmWorldStarts=rows["FULL-RELOAD-PREWARM-START"],
                latency=rows["FULL-RELOAD-LATENCY"], nativeCosts=rows["FULL-RELOAD-NATIVE-COST"], waits=rows["FULL-RELOAD-WAIT"],
                transactions=rows["FULL-RELOAD-BEGIN"], originalResourceLeases=rows["FULL-RELOAD-LEASE"],
                candidateBundles=rows["FULL-RELOAD-BUNDLE"], nativePrefabChecks=rows["FULL-RELOAD-PREFAB"],
                owners=rows["FULL-RELOAD-OWNERS"], ownerSkips=rows["FULL-RELOAD-OWNER-SKIP"],
                migrated=migrated, commits=commits, rejected=errors, rollbacks=rollbacks,
                quarantine=rows["FULL-RELOAD-QUARANTINE"], frameCostSamples=rows["FULL-RELOAD-FRAME"],
                successfulCurrentWorldResourceTransactions=len(commits),
                currentWorldMigrationLogged=verified,
                twoWayVisualWitnessTransactionsLogged=verified and len(commits) >= 2 and
                    len(rows["FULL-RELOAD-PREPARED"]) >= 2 and
                    rows["FULL-RELOAD-PREPARED"][0].get("testAuthorFirstCommit") == "1" and
                    rows["FULL-RELOAD-PREPARED"][1].get("testAuthorFirstCommit") == "0",
                bodyTextureVisualAcceptanceReported=False, GPUTexturePixelsVerified=False,
                currentDisplayMigrationSupported=False, persistentPoolMigrationSupported=False,
                exhaustiveNativeGenerationRetirementVerified=False,
                completeResourceHotReloadAccepted=False,
                scope="Current LZY world owner via game-owned lifecycle; isolated world/UI prefab roots and shared original game leases. Live displays and persistent pools are rejected. Visual acceptance must be supplied separately.")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("log", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    result = summarize(args.log.read_text("utf-8-sig", errors="replace"))
    result["log"] = str(args.log.resolve())
    with args.output.open("x", encoding="utf8") as file:
        json.dump(result, file, ensure_ascii=False, indent=2)
        file.write("\n")
    print(json.dumps({key: result[key] for key in ("ready", "successfulCurrentWorldResourceTransactions",
          "currentWorldMigrationLogged", "twoWayVisualWitnessTransactionsLogged", "completeResourceHotReloadAccepted")}, ensure_ascii=False))


if __name__ == "__main__":
    main()

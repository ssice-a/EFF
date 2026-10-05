"""Summarize bounded natural-load/assembly/F10 observations.

This stage cannot establish a safe explicit refresh entry, cache invalidation,
or full resource hot reload. Missing natural events remain inconclusive.
"""
import argparse
from collections import Counter, defaultdict
import json
from pathlib import Path
import re

PAIR = re.compile(r"([A-Za-z][A-Za-z0-9_]*)=([^\s]+)")
PREFIX = re.compile(r"\[(RELOAD-(?:PROBE|LOAD)-[A-Z-]+)\]")
FULL_PREFIX = re.compile(r"\[(FULL-RELOAD-[A-Z-]+)\]")

def summarize(text):
    rows = defaultdict(list)
    full = defaultdict(list)
    for line in text.splitlines():
        match = PREFIX.search(line)
        if match:
            rows[match[1]].append(dict(PAIR.findall(line[match.end():])))
        match = FULL_PREFIX.search(line)
        if match:
            full[match[1]].append(dict(PAIR.findall(line[match.end():])))
    events = rows["RELOAD-PROBE-EVENT"]
    entered, returned = {}, {}
    duplicates = []
    for event in events:
        target = entered if event.get("phase") == "enter" else returned
        identity = event.get("id")
        if identity in target:
            duplicates.append(identity)
        target[identity] = event
    changed = []
    for identity in entered.keys() & returned.keys():
        first, last = entered[identity], returned[identity]
        if any(first.get(key) != last.get(key) for key in ("name", "self", "methodInfo", "helper")):
            changed.append(identity)
    assets = defaultdict(list)
    for asset in rows["RELOAD-PROBE-ASSET"]:
        assets[(asset.get("bundle"), asset.get("name"))].append(asset)
    parts = rows["RELOAD-PROBE-PART"]
    mismatched_slots = [p for p in parts if p.get("boneSlots") != p.get("bindposes")]
    observer_errors = [line for line in text.splitlines() if "[ENTRY-OBSERVER]" in line and "reload probe" in line]
    kinds = Counter(e.get("name") for e in entered.values())
    ready = rows["RELOAD-PROBE-READY"]
    expected_hooks = ("helperInit", "refind", "helperRelease", "materialInit", "materialRelease", "modelChange", "bundleUnload", "proxyLoadImmediate", "proxyRelease", "proxyManagerRelease", "proxyManagerUnloadAll", "bundleManagerUnloadUnused", "modelLoaded", "animationChange", "setModelId", "loadMain", "removeMain")
    optional_lifecycle_hooks = ("proxyReleaseHandle", "proxyUnload", "proxyClear", "proxyDispose", "proxyManagerOnDispose")
    loading_ready = rows["RELOAD-LOAD-READY"]
    completions = rows["RELOAD-LOAD-COMPLETE"]
    native_assets = rows["RELOAD-LOAD-ASSET"]
    return {
        "format": "EFF_FULL_RELOAD_RUNTIME_OBSERVATION", "version": 1,
        "probeReady": bool(ready),
        "allExpectedHooksInstalled": bool(ready) and all(ready[-1].get(key) == "1" for key in expected_hooks),
        "allProxyLifecycleHooksInstalled": bool(ready) and all(ready[-1].get(key) == "1" for key in optional_lifecycle_hooks),
        "eventCounts": dict(kinds), "entered": len(entered), "returned": len(returned),
        "missingReturns": sorted(entered.keys() - returned.keys()),
        "unmatchedReturns": sorted(returned.keys() - entered.keys()),
        "duplicateEventIds": duplicates, "changedIdentityOrMethodInfo": changed,
        "originalExceptions": [e for e in returned.values() if e.get("originalThrew") == "1"],
        "assemblyRecords": rows["RELOAD-PROBE-ASSEMBLY"],
        "helperRecords": rows["RELOAD-PROBE-HELPER"],
        "partRecords": parts, "distinctAuthoredMarkers": sorted({p.get("marker") for p in parts}),
        "boneSlotBindposeCountMismatches": mismatched_slots,
        "assetObservations": rows["RELOAD-PROBE-ASSET"],
        "repeatedBundleAssetObservations": [{"bundle": k[0], "name": k[1], "returnedObjects": [r.get("object") for r in v]} for k, v in assets.items() if len(v) > 1],
        "gameInitiatedBundleUnloads": rows["RELOAD-PROBE-UNLOAD"],
        "fullReloadRequests": full["FULL-RELOAD-REQUEST"],
        "fullReloadRoutes": full["FULL-RELOAD-ROUTE"],
        "fullReloadMigrations": full["FULL-RELOAD-MIGRATED"],
        "fullReloadCommits": full["FULL-RELOAD-COMMIT"],
        "fullReloadRejects": full["FULL-RELOAD-REJECT"],
        "fullReloadRollbacks": full["FULL-RELOAD-ROLLBACK"],
        "fullReloadConsumerRebinds": full["FULL-RELOAD-CONSUMERS"],
        "loadingProbeReady": loading_ready,
        "bothNativeLoadingBindingsInstalled": bool(loading_ready) and all(loading_ready[-1].get(key) == "1" for key in ("nativeCompletion", "nativeHash")),
        "submittedInputIdentities": rows["RELOAD-LOAD-SUBMITTED"],
        "nativeBundleCompletions": completions,
        "nativeAssetObservations": native_assets,
        "nativeLoadingCounters": rows["RELOAD-LOAD-COUNTERS"],
        "allNativeAssetSurfacesInstalled": bool(loading_ready) and all(loading_ready[-1].get(key) == "1" for key in ("nativeHash", "nativeString", "nativeAsyncHash", "nativeAsyncString", "nativeAssetResult")),
        "nativeAssetRequests": rows["RELOAD-LOAD-ASSET-REQUEST"],
        "nativeAssetCompletions": rows["RELOAD-LOAD-ASSET-COMPLETE"],
        "nativeAsyncAssetCounters": rows["RELOAD-LOAD-ASYNC-COUNTERS"],
        "nativeLoadingErrors": rows["RELOAD-LOAD-ERROR"],

        "nativeUnmatchedCompletions": rows["RELOAD-LOAD-UNMATCHED"],
        "observedBundleReleases": rows["RELOAD-LOAD-RELEASE"],
        "loadingIdentityLimitEvents": rows["RELOAD-LOAD-LIMIT"],
        "conflictingLiveBundleContent": [event for event in completions if event.get("conflictingContent") == "1"],
        "nativeInputCompletionObserved": any(event.get("returnedNonNull") == "1" and event.get("weakIdentity") == "1" for event in completions),
        "nativeInputAssetReturnObserved": any(event.get("returnedNonNull") == "1" and event.get("weakIdentity") == "1" for event in native_assets),
        "loadingPublicationCounterDoesNotProveChangedContent": True,

        "configurationPrepare": rows["RELOAD-PROBE-PREPARE"],
        "configurationCommit": rows["RELOAD-PROBE-COMMIT"],
        "frameWindows": rows["RELOAD-PROBE-FRAMES"],
        "observerReadErrors": observer_errors,
        "initialAssemblyObserved": bool(kinds["EntityRenderHelper._InitRenderAndMaterial"] and rows["RELOAD-PROBE-ASSEMBLY"]),
        "naturalRefindObserved": bool(kinds["EntityRenderHelper.ReFindRenderersInChildren"]),
        "naturalModelChangeObserved": bool(kinds["ModelComponent.ChangeMainModelSync"]),
        "safeExplicitRefreshEntryVerified": False,
        "generationCacheIsolationVerified": False,
        "existingInstanceResourceMigrationVerified": False,
        "completeResourceHotReloadImplemented": False,
        "eventSampleLimitPerBoundary": 128,
        "timingScope": "instrumented normal game calls and current configuration F10; main-thread callback gaps are not GPU timing or predicted full-resource latency",
        "cacheInterpretation": "Repeated object pointers alone do not prove cache invalidation or a valid resource lifetime.",
    }

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("log", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    result = summarize(args.log.read_text(encoding="utf-8-sig", errors="replace"))
    result["log"] = str(args.log.resolve())
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("x", encoding="utf-8") as file:
        json.dump(result, file, ensure_ascii=False, indent=2)
        file.write("\n")
    print(json.dumps({key: result[key] for key in ("probeReady", "allExpectedHooksInstalled", "eventCounts", "initialAssemblyObserved", "naturalRefindObserved", "naturalModelChangeObserved", "fullReloadRequests", "fullReloadMigrations", "fullReloadCommits", "safeExplicitRefreshEntryVerified", "completeResourceHotReloadImplemented")}, ensure_ascii=False, indent=2))

if __name__ == "__main__":
    main()

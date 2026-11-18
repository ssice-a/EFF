"""Read a bounded first-binding trace without loading game code or a heap dump."""
import argparse
import collections
import json
from pathlib import Path


def analyze(path: Path):
    rows = [json.loads(line) for line in path.read_text(encoding="utf-8").splitlines()]
    if not rows or rows[0].get("format") != "EFF_SCRIPT_BINDING_PROBE/1":
        raise ValueError("Not an EFF first-binding trace")
    header, events = rows[0], rows[1:]
    if [row["event"] for row in events] != list(range(len(events))):
        raise ValueError("Event sequence is incomplete or duplicated")
    before = {r["call"]: r for r in events if r["kind"] == 1 and r["phase"] == 0}
    after = {r["call"]: r for r in events if r["kind"] == 1 and r["phase"] == 1}
    groups = collections.Counter()
    suspicious = []
    image = int(header["unityBase"], 16)
    for call, start in before.items():
        end = after.get(call)
        if end is None:
            # The finite budget can stop between the before/after events.
            # An absent post-event alone cannot prove native non-return.
            groups["post_event_missing_at_record_limit" if len(events) == header["limit"]
                   else "post_event_missing_or_native_unreturned"] += 1
            continue
        script_before = int(start["scriptClass"], 16)
        script_after = int(end["scriptClass"], 16)
        component_after = int(end["componentClass"], 16)
        if (not int(start["componentScriptId"]) and int(start["argumentClass"], 16)
                and component_after):
            status = "explicit_class_bound_without_monoscript"
        elif not int(start["script"], 16):
            status = "missing_monoscript_argument"
        elif component_after:
            status = "component_class_bound"
        elif script_before:
            status = "ready_script_left_component_without_class"
        elif script_after:
            status = "script_became_ready_during_binding_but_component_empty"
        else:
            status = "script_class_unavailable_during_binding"
        groups[status] += 1
        if status not in ("component_class_bound", "explicit_class_bound_without_monoscript"):
            suspicious.append({
                "call": call, "classification": status, "before": start, "after": end,
                "unityStackRvas": [hex(int(address, 16) - image) for address in start["stack"]
                                   if image <= int(address, 16) < image + 0x22a2000],
            })
    return {
        "source": str(path), "header": header, "eventCount": len(events),
        "limitReached": len(events) == header["limit"],
        "bindingCalls": len(before), "classifications": dict(groups),
        "suspiciousBindings": suspicious,
        "inputTimeline": [r for r in events if r["kind"] == 2],
        "limits": "A missing class identifies the first-binding state, not the dependency or initialization cause; native stacks and input timing require correlation.",
    }


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("trace", type=Path)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    result = analyze(args.trace)
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(result, indent=2), encoding="utf-8")
    print(json.dumps({k: v for k, v in result.items()
                      if k not in ("suspiciousBindings", "inputTimeline")}, indent=2))

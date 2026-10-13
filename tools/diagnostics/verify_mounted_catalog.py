"""Compare a native mounted-catalog survey against exact BLC/range readback.

The companion inspect_vfs_lookup_names --mounted command reads the ranges
chosen by the game, validates each BLC file MD5, and emits the readback JSON.
This check never infers mount precedence or hashes whole game archives.
"""
import argparse
import json
from pathlib import Path


def verify(native, readback):
    if native.get("mountedQueryFailures") != 0:
        raise ValueError("Native VFS survey is incomplete")
    expected = {row["logical"]: row for row in readback["sources"]}
    if len(expected) != len(readback["sources"]):
        raise ValueError("Readback source repeated")
    observed = {}
    for row in native["mountedSources"]:
        logical = row["logical"]
        if logical in observed:
            raise ValueError("Native source repeated")
        observed[logical] = row
        before = expected[logical]
        for field in ("offset", "bytes"):
            if row[field] != before[field]:
                raise ValueError(f"Native/BLC {field} differs: {logical}")
        if Path(row["physical"]) != Path(before["physical"]):
            raise ValueError(f"Native/BLC physical path differs: {logical}")
        if row["chunkContentMd5"].upper() != before["chunkContentMd5"].upper():
            raise ValueError(f"Native/BLC CHK content MD5 differs: {logical}")
        if row["fileContentMd5"] and row["fileContentMd5"].upper() != before["fileContentMd5"].upper():
            raise ValueError(f"Native/BLC file MD5 differs: {logical}")
        if row["encrypted"]:
            raise ValueError("Encrypted range requires separate plaintext readback")
    if observed.keys() != expected.keys() or not observed:
        raise ValueError("Native/BLC source coverage differs or empty")
    return dict(processId=native["processId"], sources=len(observed),
                chunks=len({row["chunk"] for row in observed.values()}),
                persistentSources=sum("/persistent/" in row["physical"].replace("\\", "/").lower()
                                      for row in observed.values()),
                catalogSurveyMs=native["surveyMs"],
                nativeChunkContentDigestMatchesBLC=True,
                physicalRangeMatchesBLC=True, readbackFileMD5MatchesBLC=True,
                perFileNativeDigestObservations=sorted({row["nativeFileDigestValue"] for row in observed.values()}),
                automaticProductionAdaptation=False)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("native", type=Path)
    parser.add_argument("readback", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    report = verify(json.loads(args.native.read_text(encoding="utf-8")),
                    json.loads(args.readback.read_text(encoding="utf-8")))
    args.output.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print("PASS mounted native identities:", json.dumps(report, ensure_ascii=False))

"""List selected loader method owners in the inspected Endfield v29 metadata."""
import argparse
import json
import struct
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("metadata", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    data = args.metadata.read_bytes()
    if struct.unpack_from("<II", data) != (0xFAB11BAF, 29):
        raise ValueError("Expected the inspected v29 metadata")
    string_offset, string_size = struct.unpack_from("<II", data, 24)
    type_offset, type_size = struct.unpack_from("<II", data, 8 + 19 * 8)
    method_offset, method_size = struct.unpack_from("<II", data, 8 + 5 * 8)
    if type_size % 92 or method_size % 32:
        raise ValueError("Expected inspected 92-byte types and 32-byte methods")

    def name(index):
        if not 0 <= index < string_size:
            raise ValueError("Invalid string index")
        end = data.index(b"\0", string_offset + index, string_offset + string_size)
        return data[string_offset + index:end].decode("utf-8")

    selected = {"LoadFromFile", "LoadFromFileAsync", "LoadFromFile2",
                "LoadBundleFromFile", "LoadBundleFromFileAsync"}
    rows = []
    for index in range(method_size // 32):
        values = struct.unpack_from("<6I4H", data, method_offset + index * 32)
        method_name = name(values[0])
        if method_name not in selected:
            continue
        if values[1] >= type_size // 92 or values[5] >> 24 != 6:
            raise ValueError("Invalid method owner/token")
        owner = struct.unpack_from("<2I", data, type_offset + values[1] * 92)
        row = {"namespace": name(owner[1]), "class": name(owner[0]),
               "method": method_name, "token": hex(values[5]),
               "static": bool(values[6] & 0x10), "parameters": values[9],
               "returnTypeIndex": values[2]}
        rows.append(row)
        print(json.dumps(row))
    args.output.write_text(json.dumps(rows, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()

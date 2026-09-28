#!/usr/bin/env python3
"""Extract one logical file from Endfield's current VFS (code version 4).

This is an offline helper. It never changes the game VFS and does not belong
to eff.dll. Requires Python 3 and pycryptodome.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import struct
from pathlib import Path

from Crypto.Cipher import ChaCha20


KEY = bytes.fromhex(
    "E95B317AC4F828569D23A86BF271DCB53E846FA75C924D671DBA8E38F4CA52E1"
)
PROTO_VERSION = 3
BLOCK_HEAD_LEN = 12


def args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--game-path", type=Path, required=True)
    parser.add_argument("--logical-path", required=True)
    parser.add_argument("--output", type=Path, required=True)
    return parser.parse_args()


def decrypt(data: bytes, nonce: bytes) -> bytes:
    cipher = ChaCha20.new(key=KEY, nonce=nonce)
    cipher.decrypt(b"\0" * 64)
    return cipher.decrypt(data)


def decrypt_blc(path: Path) -> bytes:
    raw = path.read_bytes()
    if len(raw) < BLOCK_HEAD_LEN:
        raise ValueError(f"BLC is too short: {path}")
    plain = decrypt(raw[BLOCK_HEAD_LEN:], raw[:BLOCK_HEAD_LEN])
    # Current BLC files end with a signed CRC. Do not include it in parsing.
    return plain[:-4] if len(plain) >= 4 else plain


def read_i32(data: bytes, offset: int) -> tuple[int, int]:
    return struct.unpack_from("<i", data, offset)[0], offset + 4


def read_i64(data: bytes, offset: int) -> tuple[int, int]:
    return struct.unpack_from("<q", data, offset)[0], offset + 8


def read_u16(data: bytes, offset: int) -> tuple[int, int]:
    return struct.unpack_from("<H", data, offset)[0], offset + 2


def read_u8(data: bytes, offset: int) -> tuple[int, int]:
    return data[offset], offset + 1


def read_bytes(data: bytes, offset: int, size: int) -> tuple[bytes, int]:
    end = offset + size
    if end > len(data):
        raise ValueError("truncated BLC entry")
    return data[offset:end], end


def read_string(data: bytes, offset: int, size: int) -> tuple[str, int]:
    value, offset = read_bytes(data, offset, size)
    return value.decode("ascii", errors="replace"), offset


def normalize(value: str) -> str:
    value = re.sub(r"[^\x20-\x7e/\\]", "", value).replace("\\", "/")
    match = re.search(r"(Data/|Assets/)[A-Za-z0-9_./-]+", value)
    if match:
        value = match.group(0)
    for prefix in ("Assets/StreamingAssets/", "Assets/", "Data/"):
        if value.startswith(prefix):
            return value[len(prefix) :]
    return value


def per_file_decrypt(data: bytes, iv_seed: int) -> bytes:
    nonce = struct.pack("<i", PROTO_VERSION) + struct.pack("<q", iv_seed)
    return decrypt(data, nonce)


def scan_blc(blc: Path, target: str) -> dict[str, object] | None:
    data = decrypt_blc(blc)
    offset = 0
    raw_version, offset = read_i32(data, offset)
    if raw_version < 11:
        code_version = raw_version
        _version, offset = read_i32(data, offset)
    else:
        code_version = 3

    name_len, offset = read_u16(data, offset)
    _name, offset = read_string(data, offset, name_len)
    _directory_hash, offset = read_i64(data, offset)
    _file_count, offset = read_i32(data, offset)
    _chunks_length, offset = read_i64(data, offset)
    _block_type, offset = read_u8(data, offset)
    chunk_count, offset = read_i32(data, offset)

    wanted = target.casefold()
    for _ in range(chunk_count):
        chunk_md5, offset = read_bytes(data, offset, 16)
        _content_md5, offset = read_bytes(data, offset, 16)
        _length, offset = read_i64(data, offset)
        _chunk_type, offset = read_u8(data, offset)
        if code_version > 3:
            _main_tag, offset = read_i32(data, offset)
        file_count, offset = read_i32(data, offset)
        for _ in range(file_count):
            name_len, offset = read_u16(data, offset)
            raw_name, offset = read_string(data, offset, name_len)
            file_name_hash, offset = read_i64(data, offset)
            _file_chunk_md5, offset = read_bytes(data, offset, 16)
            file_data_md5, offset = read_bytes(data, offset, 16)
            file_offset, offset = read_i64(data, offset)
            file_len, offset = read_i64(data, offset)
            file_type, offset = read_u8(data, offset)
            encrypted, offset = read_u8(data, offset)
            iv_seed = 0
            if encrypted:
                iv_seed, offset = read_i64(data, offset)
            if code_version > 3:
                _file_tag, offset = read_i32(data, offset)

            logical = normalize(raw_name)
            if logical.casefold() == wanted:
                return {
                    "logicalPath": logical,
                    "chunk": chunk_md5.hex().upper(),
                    "offset": file_offset,
                    "length": file_len,
                    "encrypted": bool(encrypted),
                    "ivSeed": iv_seed,
                    "fileType": file_type,
                    "fileNameHash": file_name_hash,
                    "fileDataMd5": file_data_md5.hex().upper(),
                }
    return None


def main() -> int:
    options = args()
    game = options.game_path.resolve()
    target = normalize(options.logical_path)
    vfs = game / "Endfield_Data" / "StreamingAssets" / "VFS"
    if not vfs.is_dir():
        raise FileNotFoundError(f"VFS directory not found: {vfs}")

    match: dict[str, object] | None = None
    source_blc: Path | None = None
    for blc in sorted(vfs.glob("*/*.blc")):
        result = scan_blc(blc, target)
        if result is not None:
            match = result
            source_blc = blc
            break
    if match is None or source_blc is None:
        raise FileNotFoundError(f"logical path was not found: {target}")

    chunk = str(match["chunk"])
    chunk_path = source_blc.parent / f"{chunk}.chk"
    if not chunk_path.is_file():
        raise FileNotFoundError(f"CHK for {target} was not found: {chunk_path}")
    offset = int(match["offset"])
    length = int(match["length"])
    if offset < 0 or length <= 0 or offset + length > chunk_path.stat().st_size:
        raise ValueError(f"invalid VFS range for {target}")

    with chunk_path.open("rb") as stream:
        stream.seek(offset)
        payload = stream.read(length)
    if len(payload) != length:
        raise ValueError(f"short read for {target}")
    if bool(match["encrypted"]):
        payload = per_file_decrypt(payload, int(match["ivSeed"]))

    output = options.output.resolve()
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(payload)
    metadata = dict(match)
    metadata.update({
        "sourceBLC": str(source_blc),
        "sourceCHK": str(chunk_path),
        "output": str(output),
        "size": len(payload),
        "sha256": hashlib.sha256(payload).hexdigest().upper(),
    })
    output.with_suffix(output.suffix + ".meta.json").write_text(
        json.dumps(metadata, ensure_ascii=True, indent=2) + "\n",
        encoding="utf-8",
    )
    print(f"Extracted {target} -> {output} ({len(payload)} bytes)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

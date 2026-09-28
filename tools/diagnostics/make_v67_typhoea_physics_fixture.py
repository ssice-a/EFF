"""Build the v67 Typhoea left-index Physics resource used by game diagnostics."""

import argparse
import hashlib
import importlib.util
import struct
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
MODULE = ROOT / "tools" / "Blender" / "eiem_physics_document.py"
SPEC = importlib.util.spec_from_file_location("eiem_physics_document", MODULE)
physics_document = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(physics_document)

COORDINATE = "unity-y-up-left-handed"
SKELETON_RELATIVE = "skeletons/typhoea-left-index-v67.skeleton"
PHYSICS_RELATIVE = "typhoea-left-index-v67.physics"

CHAIN_NAMES = (
    "Root", "Bip001", "Bip001_Pelvis", "Bip001_Spine", "Bip001_Spine1",
    "Bip001_Spine2", "Bip001_L_Clavicle", "Bip001_L_UpperArm",
    "Bip001_L_Forearm", "Bip001_L_Hand", "Bip001_L_Finger0",
    "Bip001_L_Finger01", "Bip001_L_Finger02",
)
CHAIN_PATHS = tuple("/".join(CHAIN_NAMES[:index + 1])
                    for index in range(len(CHAIN_NAMES)))
FINGER0 = CHAIN_PATHS[-3]
FINGER01 = FINGER0 + "/Bip001_L_Finger01"
FINGER02 = FINGER01 + "/Bip001_L_Finger02"

IDENTITY_TRANSFORM = (0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0, 1.0)
FINGER_TRANSFORMS = {
    FINGER0: (-0.00749969482421875, 0.010161439888179302,
              -0.015764109790325165, 0.49777480959892273,
              -0.28597843647003174, 0.06516677141189575,
              0.8162046074867249, 1.0, 1.0, 1.0),
    FINGER01: (-0.0268561914563179, 0.0, -7.629395071262479e-08,
               -0.002532258629798889, 0.013269119895994663,
               0.014892201870679855, 0.9997978210449219, 1.0, 1.0, 1.0),
    FINGER02: (-0.022547340020537376, 7.629395071262479e-08, 0.0,
               0.0, 0.0, 0.0, 1.0, 1.0, 1.0, 1.0),
}
NODES = tuple((path, index - 1, FINGER_TRANSFORMS.get(path, IDENTITY_TRANSFORM))
              for index, path in enumerate(CHAIN_PATHS))


def identity(label):
    return hashlib.sha256(label.encode("utf-8")).hexdigest()[:32]


def seven_bit_string(value):
    payload = value.encode("utf-8")
    size = len(payload)
    prefix = bytearray()
    while size >= 0x80:
        prefix.append((size & 0x7F) | 0x80)
        size >>= 7
    prefix.append(size)
    return bytes(prefix) + payload


def skeleton_bytes():
    data = bytearray(b"EFFSKEL\0")
    data.extend(struct.pack("<i", 2))
    data.extend(seven_bit_string(COORDINATE))
    data.extend(struct.pack("<I", len(NODES)))
    for path, parent, transform in NODES:
        data.extend(seven_bit_string(path))
        data.extend(struct.pack("<i10f", parent, *transform))
    data.extend(struct.pack("<Ii", 0, -1))
    data.extend(struct.pack("<I", len(NODES)))
    data.extend(bytes([1] * len(NODES)))
    return bytes(data)


def physics_bytes():
    payload = {
        "version": 5,
        "purpose": "authoring",
        "coordinate": COORDINATE,
        "backend": "BeyondDynamicBone",
        "id": identity("v67-typhoea-left-index-resource"),
        "skeleton": SKELETON_RELATIVE,
        "groups": [{
            "id": identity("v67-typhoea-left-index-group"),
            "name": "v67 Typhoea left index diagnostic",
            "nodes": [
                {"bone": FINGER0, "role": "FIXED"},
                {"bone": FINGER01, "role": "MOVE"},
                {"bone": FINGER02, "role": "MOVE"},
            ],
            "parameters": {
                "gravity": 10.0,
                "stablizationTimeAfterReset": 0.1,
                "gravityFalloff": 0.0,
                "blendWeight": 1.0,
                "animationPoseRatio": 1.0,
            },
            "radius": physics_document.default_radius(),
            "nativeParameters": [],
            "colliders": [],
        }],
        "colliders": [],
    }
    encoded = physics_document.encode(payload)
    decoded = physics_document.decode(encoded)
    assert decoded["groups"][0]["nodes"][2]["bone"] == FINGER02
    return encoded


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    output = args.output.resolve()
    skeleton = output / SKELETON_RELATIVE
    physics = output / PHYSICS_RELATIVE
    skeleton.parent.mkdir(parents=True, exist_ok=True)
    skeleton.write_bytes(skeleton_bytes())
    physics.write_bytes(physics_bytes())
    print(f"physics={physics} bytes={physics.stat().st_size}")
    print(f"skeleton={skeleton} bytes={skeleton.stat().st_size}")


if __name__ == "__main__":
    main()

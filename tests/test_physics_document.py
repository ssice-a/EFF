"""Authoring wire contract; native decoding is not native physics execution."""
import copy
import importlib.util
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("physics_codec_test", ROOT / "tools/Blender/eiem_physics_document.py")
codec = importlib.util.module_from_spec(spec)
spec.loader.exec_module(codec)


def fixture():
    return {"version": codec.VERSION, "purpose": "authoring", "coordinate": codec.COORDINATE,
        "backend": "BeyondDynamicBone", "id": "0"*32, "skeleton": "skeletons/rig.skeleton",
        "groups": [{"id": "1"*32, "name": "发梢", "nodes": [
            {"bone": "Rig/Root", "role": "FIXED"}, {"bone": "Rig/Root/Tip", "role": "MOVE"}],
            "parameters": dict(codec.DEFAULTS), "radius": {"value": 0.02, "useCurve": True,
                "keys": [
                    {"time": 0.0, "value": 1.0, "inSlope": 0.0, "outSlope": -0.25,
                     "weightedMode": 2, "inWeight": 1/3, "outWeight": 0.2},
                    {"time": 0.9974365234375, "value": 0.25, "inSlope": -0.5, "outSlope": 0.0,
                     "weightedMode": 1, "inWeight": 0.4, "outWeight": 1/3}],
                "preInfinity": 2, "postInfinity": 2, "rotationOrder": 4},
            "nativeParameters": [
                {"path": "serializeData.gravity", "floating": True, "value": 9.8},
                {"path": "serializeData.gravityDirection.y", "floating": True, "value": -1.0},
                {"path": "serializeData.angleLimitConstraint.useAngleLimit", "floating": False, "value": 1},
                {"path": "serializeData.radius.curve.m_Curve.0.time", "floating": True, "value": 0.0},
                {"path": "serializeData.radius.curve.m_PreInfinity", "floating": False, "value": 2}],
            "colliders": ["2"*32]}],
        "colliders": [{"id": "2"*32, "name": "胶囊", "bone": "Rig/Root", "shape": "CAPSULE",
            "position": [0.25, -0.5, 0.75], "rotation": [0,0,0,1],
            "radius": 0.03125, "endRadius": 0.046875, "span": 0.125,
            "alignedOnCenter": False}]}


SOURCE = r'''
#include "eiem_physics_document.h"
#include <fstream>
#include <cstdio>
struct Reader {
  std::ifstream file;
  Reader(const char *path):file(path,std::ios::binary) {}
  bool Bytes(void *p,size_t n) { if(!n)return true; file.read((char *)p,n); return file.good(); }
  bool End() { return file.peek()==std::char_traits<char>::eof(); }
};
int main(int argc,char **argv) {
  if(argc<3 || argc>4)return 2;
  Reader reader(argv[1]); EiemPhysicsDocument d; d.id="unchanged"; std::string error;
  bool ok=EiemReadPhysicsAuthor(reader,d,error);
  bool expected=std::string(argv[2])=="ok";
  if(ok!=expected || (!ok && (d.id!="unchanged" || error.empty()))) {puts(error.c_str());return 1;}
  if(ok) {
    if(d.version>=3 && (d.groups[0].radius.keys.size()<2 || d.groups[0].radius.value<=0)) return 1;
    bool anyV4Count=argc==4 && std::string(argv[3])=="any-v4-count";
    if(d.version>=4 && anyV4Count && d.groups[0].nativeParameters.empty()) return 1;
    if(d.version>=4 && !anyV4Count && (d.groups[0].nativeParameters.size()!=5 ||
       d.groups[0].nativeParameters[1].floatingValue!=-1.0 ||
       d.groups[0].nativeParameters[2].integerValue!=1)) return 1;
    if(d.version>=5 && !anyV4Count &&
       (d.colliders[0].endRadius!=.046875f || d.colliders[0].alignedOnCenter)) return 1;
    printf("%zu %zu\n",d.groups.size(),d.colliders.size());
  }
  return 0;
}
'''


def compile_reader(folder):
    source, exe = folder / "reader.cpp", folder / "reader.exe"
    source.write_text(SOURCE, encoding="utf-8")
    result = subprocess.run(["cl", "/nologo", "/EHsc", "/std:c++17", "/utf-8", f"/I{ROOT/'src'}",
                             str(source), f"/Fe{exe}"], cwd=folder, capture_output=True, text=True, errors="replace")
    if result.returncode:
        raise AssertionError(result.stdout + result.stderr)
    return exe


class PhysicsDocumentTests(unittest.TestCase):
    def test_roundtrip_shared_collider_and_float32_canonical_bytes(self):
        doc = fixture()
        second = copy.deepcopy(doc["groups"][0]); second["id"] = "3"*32
        doc["groups"].append(second)
        data = codec.encode(doc)
        actual = codec.decode(data)
        self.assertEqual(codec.encode(actual), data)
        self.assertEqual(len(actual["colliders"]), 1)
        self.assertEqual(actual["colliders"][0], doc["colliders"][0])
        self.assertAlmostEqual(actual["groups"][0]["radius"]["value"], .02)
        self.assertEqual(actual["groups"][0]["radius"]["keys"][0]["weightedMode"], 2)
        self.assertEqual(actual["groups"][0]["radius"]["keys"][-1]["time"], .9974365234375)
        codec.validate(actual, {"Rig/Root", "Rig/Root/Tip"})

    def test_old_authoring_versions_are_rejected(self):
        for version in (1, 3, 4):
            with self.subTest(version=version):
                doc = fixture(); doc["version"] = version
                with self.assertRaises(ValueError): codec.encode(doc)
                data = bytearray(codec.encode(fixture()))
                data[8:12] = version.to_bytes(4, "little")
                with self.assertRaises(ValueError): codec.decode(data)

    def test_invalid_schema_topology_references_and_numbers(self):
        changes = [lambda d: d.update(version=2), lambda d: d.update(runtime_handle=1),
                   lambda d: d.update(skeleton="../rig.skeleton"),
                   lambda d: d["groups"][0]["parameters"].update(gravity=float("nan")),
                   lambda d: d["groups"][0]["radius"].update(value=0),
                   lambda d: d["groups"][0]["radius"]["keys"][1].update(time=0),
                   lambda d: d["groups"][0]["radius"]["keys"][0].update(weightedMode=4),
                   lambda d: d["groups"][0]["nativeParameters"].append(
                       {"path":"serializeData.rootBones.0", "floating":False, "value":0}),
                   lambda d: d["groups"][0]["nativeParameters"].append(
                       {"path":"serializeData.0.value", "floating":True, "value":1.0}),
                   lambda d: d["groups"][0]["nativeParameters"].append(
                       copy.deepcopy(d["groups"][0]["nativeParameters"][0])),
                   lambda d: d["groups"][0]["nativeParameters"][0].update(value=float("nan")),
                   lambda d: d["groups"][0]["parameters"].update(blendWeight=2),
                   lambda d: d["groups"][0]["nodes"][1].update(bone="Other/Tip"),
                   lambda d: d["groups"][0]["nodes"][0].update(role="MOVE"),
                   lambda d: d["groups"][0].update(colliders=["a"*32]),
                   lambda d: d["groups"][0].update(colliders=["2"*32]*2),
                   lambda d: d["colliders"][0].update(id="1"*32),
                   lambda d: d["colliders"][0].update(radius=0),
                   lambda d: d["colliders"][0].update(endRadius=0),
                   lambda d: d["colliders"][0].update(alignedOnCenter=2),
                   lambda d: d["colliders"][0].update(shape="SPHERE"),
                   lambda d: d["colliders"][0].update(rotation=[0,0,0,2])]
        for change in changes:
            with self.subTest(change=changes.index(change)):
                value = fixture(); change(value)
                with self.assertRaises(ValueError): codec.encode(value)
        with self.assertRaisesRegex(ValueError, "missing Skeleton bone"):
            codec.validate(fixture(), {"Rig/Root"})

    def test_root_path_and_zero_weight_nodes_are_valid(self):
        doc = fixture()
        doc["groups"][0]["nodes"] = [{"bone": "", "role": "FIXED"}, {"bone": "Tip", "role": "MOVE"}]
        self.assertEqual(codec.decode(codec.encode(doc))["groups"][0]["nodes"], doc["groups"][0]["nodes"])

    def test_multiple_fixed_roots_share_one_group(self):
        doc = fixture()
        doc["groups"][0]["nodes"].extend([
            {"bone": "Rig/Other", "role": "FIXED"},
            {"bone": "Rig/Other/Tip", "role": "MOVE"}])
        decoded = codec.decode(codec.encode(doc))
        self.assertEqual([node["role"] for node in decoded["groups"][0]["nodes"]].count("FIXED"), 2)
        decoded["groups"][0]["nodes"][-1]["role"] = "FIXED"
        with self.assertRaisesRegex(ValueError, "root"):
            codec.encode(decoded)

    def test_reject_truncation_trailing_data_invalid_utf8_and_counts(self):
        data = codec.encode(fixture())
        for size in range(len(data)):
            with self.assertRaises(ValueError): codec.decode(data[:size])
        for bad in (data+b"x", data[:12]+struct.pack("<I", 5000)+data[16:],
                    data.replace("发梢".encode(), b"\xff"*6)):
            with self.assertRaises(ValueError): codec.decode(bad)


class NativePhysicsDocumentTests(unittest.TestCase):
    def test_real_cpp_reader_and_transactional_failures(self):
        if not shutil.which("cl"): self.skipTest("Requires MSVC")
        with tempfile.TemporaryDirectory(prefix="eiem-physics-wire-") as directory:
            folder = Path(directory); exe = compile_reader(folder)
            data = codec.encode(fixture())
            cases = [(data, "ok"), (data+b"x", "bad"), (data[:-1], "bad"),
                     (data.replace(b"authoring", b"runtime!!"), "bad"),
                     (data.replace("发梢".encode(), b"\xff"*6), "bad"),
                     (data.replace(b"Rig/Root/Tip", b"Rig/Else/Tip"), "bad"),
                     (data[:12]+struct.pack("<I", 5000)+data[16:], "bad")]
            for i, (payload, expected) in enumerate(cases):
                path = folder / f"{i}.physics"; path.write_bytes(payload)
                run = subprocess.run([str(exe), str(path), expected], capture_output=True, text=True)
                self.assertEqual(run.returncode, 0, run.stdout + run.stderr)


if __name__ == "__main__": unittest.main()

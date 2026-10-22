"""Exercise the production pipe dispatcher against the real runtime test seam."""
import hashlib
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import time

from eff_effect_debug import execute, exchange


def run(exe):
    with tempfile.TemporaryDirectory(prefix="EFF 特效 pipe ") as temporary:
        root = Path(temporary)
        output = (root / "server.log").open("wb")
        server = subprocess.Popen([str(exe), "--serve"], stdout=output, stderr=subprocess.STDOUT)
        try:
            for _ in range(100):
                try:
                    status = exchange(server.pid, "STATUS")
                    break
                except OSError:
                    if server.poll() is not None:
                        raise RuntimeError((root / "server.log").read_text(errors="replace"))
                    time.sleep(.03)
            else:
                raise TimeoutError("fixture pipe did not start")
            assert "phase=" in status
            owners = execute(server.pid, "OWNERS")
            assert "4098\t" in owners, owners
            nodes = execute(server.pid, "NODES\t4098")
            assert "\n-\t" in nodes, nodes
            assert exchange(server.pid, "PLAY\t0\t-").startswith("ERR\t")
            assert exchange(server.pid, "SPEED\t1").startswith("ERR\t"), "unsupported command falsely acknowledged"

            def n(value):
                return struct.pack("<Q", value)

            def s(text):
                raw = text.encode("utf-8")
                return n(len(raw)) + raw

            bundle = root / "runtime/bundles/test.ab"
            bundle.parent.mkdir(parents=True)
            bundle.write_bytes(b"UnityFS\0" + b"native fixture")
            packet = (s("EFF_EFFECT_DEBUG_NATIVE/inactive-root+le64") + s("A" * 64) +
                      s("assets/effect.prefab") + s("effect") + n(0) + n(1) + n(1) +
                      s("runtime/bundles/test.ab") + s(hashlib.sha256(bundle.read_bytes()).hexdigest().upper()) + n(0))
            manifest = root / "runtime.bin"
            manifest.write_bytes(packet)
            prepared = execute(server.pid, "PREPARE\t" + str(manifest))
            assert prepared.startswith("OK\tready"), prepared
            assert execute(server.pid, "PLAY\t4098\t-").startswith("OK\tplaying")
            assert "phase=playing" in exchange(server.pid, "STATUS")
            assert "reused=1" in execute(server.pid, "PREPARE\t" + str(manifest))
            # A disk-thread failure must retain the currently playing instance.
            bundle.write_bytes(b"damaged")
            assert exchange(server.pid, "PREPARE\t" + str(manifest)).startswith("ERR\t")
            assert "phase=playing" in exchange(server.pid, "STATUS")
            assert execute(server.pid, "STOP").startswith("OK\tstopped")
            assert execute(server.pid, "CLEAR").startswith("OK\tcleared")
            assert "phase=idle" in exchange(server.pid, "STATUS")
            print("PASS real named-pipe transport: UTF-8 path, async tickets, owners/nodes, prepare/play/reuse, corrupt bundle rejection, stop/clear; zero worker Unity calls")
        finally:
            server.terminate()
            server.wait(timeout=10)
            output.close()


if __name__ == "__main__":
    run(Path(sys.argv[1]).resolve())

from runtime_source import read_runtime_source
"""Execute the production async hook against a deterministic loader fixture.

This tests our native boundary, not IL2CPP or GPU rendering. A callback must
reach the game unchanged: replacing Action<GameObject> with a native-address
delegate caused the 2026-09-05 UI crash before the replacement callback ran.
Run in an MSVC developer environment (cl on PATH).
"""

from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]


def production_async_hook():
    source = read_runtime_source(ROOT)
    start = source.rindex("static int32_t TraceUIModelLoaderLoadModelAsync(")
    opening = source.index("{", start)
    depth = 1
    cursor = opening + 1
    while depth:
        depth += (source[cursor] == "{") - (source[cursor] == "}")
        cursor += 1
    return source[start:cursor]


FIXTURE = r'''
#include <cstdint>
#include <cstdio>
#include <cstring>
using TraceUIModelLoaderLoadModelAsyncFn =
    int32_t (*)(void *, void *, void *, void *, void *);
static void *s_origUIModelLoaderLoadModelAsync = nullptr;
static int wrapCalls = 0;
static int pendingCalls = 0;
static int completionCalls = 0;
static void *received[5] = {};
static int tokens[6] = {};
static int32_t loaderResult = 37;
static bool completeInline = false;
static void *deferred = nullptr;
// The fixture flags attempts to manufacture a new managed callback. It never
// calls an invalid function pointer, so the old bug gives a deterministic FAIL.
static void TraceDescribeString(void *, char *out, size_t) { out[0] = 0; }
static void Log(const char *, ...) {}
static void *EiemCreateUIModelCallback(void *, const char *, void *) {
    ++wrapCalls;
    return &tokens[5];
}
static void EiemSetPendingUIModelRequest(void *, int32_t) { ++pendingCalls; }
static void GameCompletion(void *callback) {
    if (callback == &tokens[3]) ++completionCalls;
}
static int32_t GameLoad(void *self, void *path, void *parent,
                        void *callback, void *method) {
    received[0] = self; received[1] = path; received[2] = parent;
    received[3] = callback; received[4] = method;
    if (loaderResult >= 0 && callback) {
        if (completeInline) GameCompletion(callback);
        else deferred = callback;
    }
    return loaderResult;
}
// PRODUCTION_HOOK
#define CHECK(condition) do { if (!(condition)) { \
    std::fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #condition); return 1; \
} } while (false)
int main(int argc, char **argv) {
    CHECK(argc == 2);
    const char *scenario = argv[1];
    completeInline = std::strcmp(scenario, "inline") == 0;
    const bool nullCallback = std::strcmp(scenario, "null") == 0;
    const bool canceled = std::strcmp(scenario, "cancel") == 0;
    if (std::strcmp(scenario, "failed") == 0) loaderResult = -1;
    const bool unavailable = std::strcmp(scenario, "unavailable") == 0;
    if (!unavailable) s_origUIModelLoaderLoadModelAsync = (void *)&GameLoad;
    void *callback = nullCallback ? nullptr : &tokens[3];
    int32_t result = TraceUIModelLoaderLoadModelAsync(
        &tokens[0], &tokens[1], &tokens[2], callback, &tokens[4]);
    CHECK(wrapCalls == 0);
    CHECK(pendingCalls == 0);
    if (unavailable) { CHECK(result == -1); CHECK(received[0] == nullptr); return 0; }
    CHECK(result == loaderResult);
    CHECK(received[0] == &tokens[0]); CHECK(received[1] == &tokens[1]);
    CHECK(received[2] == &tokens[2]); CHECK(received[3] == callback);
    CHECK(received[4] == &tokens[4]);
    if (canceled) deferred = nullptr; // cancellation stays game-owned
    if (deferred) GameCompletion(deferred);
    CHECK(completionCalls == (callback && loaderResult >= 0 && !canceled ? 1 : 0));
    return 0;
}
'''


class UIAsyncPassthrough(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if not shutil.which("cl"):
            raise unittest.SkipTest("Requires MSVC developer environment (cl)")
        cls.temp = tempfile.TemporaryDirectory(prefix="eiem-ui-callback-")
        cls.addClassCleanup(cls.temp.cleanup)
        folder = Path(cls.temp.name)
        source = folder / "ui_callback_fixture.cpp"
        source.write_text(
            FIXTURE.replace("// PRODUCTION_HOOK", production_async_hook()),
            encoding="utf-8",
        )
        cls.executable = folder / "ui_callback_fixture.exe"
        build = subprocess.run(
            ["cl", "/nologo", "/EHsc", "/std:c++17", str(source),
             f"/Fe{cls.executable}"],
            cwd=folder, capture_output=True, text=True, encoding="utf-8", errors="replace",
        )
        if build.returncode:
            raise AssertionError(build.stdout + build.stderr)

    def test_game_owns_callback_identity_timing_and_cancellation(self):
        for scenario in ("deferred", "inline", "null", "failed", "cancel", "unavailable"):
            with self.subTest(scenario=scenario):
                result = subprocess.run(
                    [str(self.executable), scenario], capture_output=True, text=True, encoding="utf-8", errors="replace",
                )
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)


if __name__ == "__main__":
    unittest.main()

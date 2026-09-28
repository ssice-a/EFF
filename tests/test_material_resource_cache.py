"""Run real Material/Texture builders and cache; mock only engine/OS endpoints."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

from test_mesh_resource_cache import FIXTURE, ROOT, production_cache


def fixture_source():
    source = (ROOT / "src/eiem_resource_backend.h").read_text(encoding="utf-8")
    prefix = FIXTURE.split("// PRODUCTION_BUILDER")[0]
    prefix = prefix.replace('#include <map>', '#include <map>\n#include <string>\n#include <fstream>')
    prefix = prefix.replace('char path[MAX_PATH] = "mesh.mesh"; };', '''char path[MAX_PATH] = "mesh.mesh";
        char targetAsset[96] = {}; bool textureLinear = false, textureMipmaps = true;
        int textureFilter = 1, textureWrap = 0, textureAniso = 1; float textureMipBias = 0; };''')
    prefix = prefix.replace('static int EiemNativeObjectStatus(void *object)', '''static std::map<void *, int> nativeStates;
static int EiemNativeObjectStatus(void *object)''')
    prefix = prefix.replace('if (!object || object == deadNative) return 0;', '''if (!object || object == deadNative) return 0;
    if (nativeStates.count(object)) return nativeStates[object];''')
    start = prefix.index("static bool EiemFindModResource(")
    end = prefix.index("static unsigned GetFullPathNameA", start)
    prefix = prefix[:start] + r'''
static bool EiemFindModResource(const char *mod, const char *section, const char *kind,
                                EiemModResource *out) {
    strcpy_s(out->modPath, mod); strcpy_s(out->section, section);
    strcpy_s(out->path, std::strcmp(kind, "Texture") == 0 ? "texture.bin" : "material.mat");
    return true;
}
''' + prefix[end:]
    prefix = prefix.replace("// PRODUCTION_CACHE", production_cache())
    texture_stamp = source[source.index("static uint64_t EiemTextureDependencyStamp(const EiemModResource &resource,", source.index("static bool EiemParseInt32")):source.index("static bool EiemBuildTextureResource(")]
    builders = source[source.index("static bool EiemBuildTextureResource("):source.index("static void EiemApplySubmeshMaterialMap(")]
    # This harness stubs disk resolution; remove only the later production
    # definition from the extracted builders, retaining cache helpers.
    marker = "static bool EiemResolveResourceDiskPath("
    first = prefix.index(marker)
    second = prefix.find(marker, first + 1)
    if second >= 0:
        end = prefix.index("\n}", second) + 2
        prefix = prefix[:second] + prefix[end:]
    return prefix + ENDPOINTS + texture_stamp + builders + MAIN


ENDPOINTS = r'''
#define IL2CPP_ARRAY_DATA 32
struct Color { float r, g, b, a; };
struct Vector2 { float x, y; };
struct Object { std::map<std::string, void *> textures; };
static std::vector<std::unique_ptr<Object>> objects;
static std::vector<std::unique_ptr<char[]>> buffers;
static int methodIds[20];
static void *s_eiemTexture2DClass = &methodIds[0], *s_eiemByteClass = &methodIds[1];
static void *s_eiemTexture2DCtor = &methodIds[2], *s_eiemImageLoad = &methodIds[3];
static void *s_eiemTextureSetFilterMode = &methodIds[4], *s_eiemTextureSetWrapMode = &methodIds[5];
static void *s_eiemTextureSetAnisoLevel = &methodIds[6], *s_eiemTextureSetMipMapBias = &methodIds[7];
static void *s_eiemObjectSetName = &methodIds[8], *s_eiemMaterialClass = &methodIds[9];
static void *s_eiemMaterialCtorCopy = &methodIds[10], *s_eiemMaterialSetFloat = &methodIds[11];
static void *s_eiemMaterialSetInt = &methodIds[12], *s_eiemMaterialSetColor = &methodIds[13];
static void *s_eiemMaterialSetTextureScale = &methodIds[14], *s_eiemMaterialSetTextureOffset = &methodIds[15];
static void *s_eiemMaterialSetTexture = &methodIds[16], *s_eiemMaterialGetTexture = &methodIds[17];
static void *s_eiemVector2Class = &methodIds[18];
static void *g_material_get_shader = nullptr;
static void *g_eiemShaderGetPropertyCount = nullptr;
static void *g_eiemShaderGetPropertyName = nullptr;
static void *g_eiemShaderGetPropertyType = nullptr;
static Object sourceMaterial;
static int meshAllocations = 0, textureBuilds = 0, materialBuilds = 0;
static bool invalidNewTexture = false;
static void *il2cpp_object_new(void *type) {
    if (heldLocks || !unityThread) std::abort();
    objects.emplace_back(new Object);
    Object *object = objects.back().get();
    if (type == s_eiemTexture2DClass) {
        ++textureBuilds;
        if (invalidNewTexture) nativeStates[object] = 0;
    } else ++materialBuilds;
    return object;
}
static void *il2cpp_array_new(void *, size_t size) {
    buffers.emplace_back(new char[IL2CPP_ARRAY_DATA + size]{});
    return buffers.back().get();
}
static void *il2cpp_string_new(const char *text) {
    buffers.emplace_back(new char[std::strlen(text) + 1]);
    std::strcpy(buffers.back().get(), text); return buffers.back().get();
}
static void ReadStrUtf8(void *, char *out, size_t size) {
    if (out && size) out[0] = 0;
}
static void *Invoke(void *method, void *self, void **params = nullptr) {
    if (heldLocks || !unityThread) std::abort();
    if (method == s_eiemImageLoad) {
        static char boxed[24] = {}; boxed[16] = 1; return boxed;
    }
    if (method == s_eiemMaterialCtorCopy) {
        ((Object *)self)->textures = ((Object *)params[0])->textures;
    } else if (method == s_eiemMaterialSetTexture) {
        ((Object *)self)->textures[(char *)params[0]] = params[1];
    } else if (method == s_eiemMaterialGetTexture) {
        if (EiemNativeObjectStatus(self) != 1) std::abort();
        void *texture = ((Object *)self)->textures[(char *)params[0]];
        return EiemNativeObjectStatus(texture) == 0 ? nullptr : texture;
    }
    return nullptr;
}
static bool EiemModRelativeFilePath(const std::string &path) { return !path.empty(); }
static bool EiemResolveResourceDiskPath(const EiemModResource &r, char *out, size_t size) {
    strcpy_s(out, size, r.path); return true;
}
static bool EiemReadMaterialFile(const char *, std::vector<std::pair<std::string, std::string>> *out,
                                  char *, size_t) {
    *out = {{"format", "EFFMAT"}, {"version", "1"}, {"overrides", "true"},
            {"source", "assets/material.mat"}, {"texture._BaseMap", "TextureA"},
            {"texture._OtherMap", "TextureA"}, {"float._Value", "1"}};
    return true;
}
static uint64_t EiemMaterialDependencyStamp(const EiemModRule &, uint64_t stamp,
                  const std::vector<std::pair<std::string, std::string>> &) { return stamp; }
static void *EiemLoadOriginalAsset(const char *, void *, char *, size_t) { return &sourceMaterial; }
static bool EiemParseFloat(const std::string &, float *out) { *out = 1; return true; }
static bool EiemParseInt32(const std::string &, int32_t *out) { *out = 1; return true; }
static bool EiemParseColor(const std::string &, Color *) { return true; }
static bool EiemParseVector2(const std::string &, Vector2 *) { return true; }
'''

MAIN = r'''
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL %d: %s\n", __LINE__, #x); return 1; } } while (false)
int main(int argc, char **argv) {
    CHECK(argc == 2);
    std::string scenario = argv[1];
    std::ofstream("texture.bin", std::ios::binary) << "PNG endpoint fixture";
    EiemModRule rule;
    char error[256] = {};
    void *material = nullptr;
    if (scenario == "wrong_thread" || scenario == "invalid_new_texture") {
        unityThread = scenario != "wrong_thread";
        invalidNewTexture = scenario == "invalid_new_texture";
        CHECK(!EiemBuildMaterialResource(rule, "MaterialA", &material, error, sizeof(error)));
        CHECK(!material && error[0] && materialBuilds == 0);
        CHECK(s_eiemMaterialResourceCache.empty() && s_eiemTextureResourceCache.empty());
        return 0;
    }
    CHECK(EiemBuildMaterialResource(rule, "MaterialA", &material, error, sizeof(error)));
    void *texture = ((Object *)material)->textures["_BaseMap"];
    CHECK(texture && ((Object *)material)->textures["_OtherMap"] == texture);
    CHECK(materialBuilds == 1 && textureBuilds == 1);
    void *shared = nullptr;
    CHECK(EiemBuildMaterialResource(rule, "MaterialA", &shared, error, sizeof(error)));
    CHECK(shared == material && materialBuilds == 1 && textureBuilds == 1);
    if (scenario == "live") return 0;
    if (scenario == "unknown_texture" || scenario == "unknown_material") {
        nativeStates[scenario == "unknown_texture" ? texture : material] = -1;
        CHECK(!EiemBuildMaterialResource(rule, "MaterialA", &shared, error, sizeof(error)));
        CHECK(!shared && error[0] && materialBuilds == 1 && textureBuilds == 1);
        CHECK(s_eiemMaterialResourceCache.size() == 1 && s_eiemTextureResourceCache.size() == 1);
        return 0;
    }
    const bool onlyMaterial = scenario == "material_dead";
    const bool onlyTexture = scenario == "texture_dead";
    const bool sameStamp = scenario == "same_stamp_new_texture";
    const int cycles = scenario == "cycles" ? 3 : 1;
    for (int i = 0; i < cycles; ++i) {
        void *oldMaterial = material, *oldTexture = texture;
        if (!onlyTexture && !sameStamp) nativeStates[oldMaterial] = 0;
        if (!onlyMaterial && !sameStamp) nativeStates[oldTexture] = 0;
        if (sameStamp) {
            EiemModResource resource;
            CHECK(EiemFindModResource(rule.modPath, "TextureA", "Texture", &resource));
            void *newTexture = il2cpp_object_new(s_eiemTexture2DClass);
            CHECK(EiemCacheObject(s_eiemTextureResourceCache, &s_eiemTextureResourceCacheLock,
                 rule.modPath, "TextureA", EiemTextureDependencyStamp(resource, 10),
                 newTexture, error, sizeof(error)));
            CHECK(s_eiemTextureResourceCache.size() == 1);
        }
        CHECK(EiemBuildMaterialResource(rule, "MaterialA", &material, error, sizeof(error)));
        texture = ((Object *)material)->textures["_BaseMap"];
        CHECK(material != oldMaterial && EiemNativeObjectStatus(material) == 1);
        CHECK(onlyMaterial ? texture == oldTexture : texture != oldTexture);
        CHECK(EiemNativeObjectStatus(texture) == 1 && ((Object *)material)->textures["_OtherMap"] == texture);
        // Rebuild must not mutate a previously shared Material in place.
        CHECK(((Object *)oldMaterial)->textures["_BaseMap"] == oldTexture);
        CHECK(s_eiemMaterialResourceCache.size() == 1 && s_eiemTextureResourceCache.size() == 1);
        CHECK(materialBuilds == i + 2 && textureBuilds == (onlyMaterial ? 1 : i + 2));
        CHECK(EiemBuildMaterialResource(rule, "MaterialA", &shared, error, sizeof(error)));
        CHECK(shared == material && materialBuilds == i + 2);
        CHECK(heldLocks == 0);
    }
    return 0;
}
'''


class MaterialResourceCache(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if not shutil.which("cl"):
            raise unittest.SkipTest("Requires MSVC developer environment (cl)")
        cls.temp = tempfile.TemporaryDirectory(prefix="eiem-material-cache-")
        cls.addClassCleanup(cls.temp.cleanup)
        cls.folder = Path(cls.temp.name)
        source = cls.folder / "material_cache.cpp"
        source.write_text(fixture_source(), encoding="utf-8")
        cls.executable = cls.folder / "material_cache.exe"
        build = subprocess.run(["cl", "/nologo", "/EHsc", "/std:c++17", f"/I{ROOT / 'src'}", str(source), f"/Fe{cls.executable}"],
                               cwd=cls.folder, capture_output=True, text=True,
                               encoding="utf-8", errors="replace")
        if build.returncode:
            raise AssertionError(build.stdout + build.stderr)

    def test_builders_revalidate_native_resources_and_texture_dependencies(self):
        for scenario in ("live", "material_dead", "texture_dead", "all_dead", "cycles",
                         "unknown_material", "unknown_texture", "same_stamp_new_texture",
                         "wrong_thread", "invalid_new_texture"):
            with self.subTest(scenario=scenario):
                result = subprocess.run([str(self.executable), scenario], cwd=self.folder,
                                        capture_output=True, text=True,
                                        encoding="utf-8", errors="replace")
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)


if __name__ == "__main__":
    unittest.main()

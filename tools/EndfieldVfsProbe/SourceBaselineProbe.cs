// Read-only verification: reparse the exported native bytes, never the game process.
using System.Security.Cryptography;
using System.Text.Json;
using AnimeStudio;
using AssetObject = AnimeStudio.Object;

internal static class SourceBaselineProbe
{
    public static int Run(string[] args)
    {
        if (args.Length != 3) throw new ArgumentException("--verify-source-baseline PACKAGE NEW_REPORT_JSON");
        if (File.Exists(args[2])) throw new IOException("Evidence output already exists.");
        var package = Path.GetFullPath(args[1]);
        using var json = JsonDocument.Parse(File.ReadAllText(Path.Combine(package, "source", "manifest.json")));
        var manifest = json.RootElement;
        Check(manifest.GetProperty("format").GetString() == "EFFSOURCE" && manifest.GetProperty("version").GetInt32() == 1, "Manifest version");
        var rows = manifest.GetProperty("serializedFiles").EnumerateArray().ToArray();
        var streams = manifest.GetProperty("rawStreams").EnumerateArray().ToArray();
        var fixture = Path.GetFullPath(Path.Combine(Path.GetTempPath(), "EFFSourceValidation-" + Guid.NewGuid().ToString("N")));
        var tempRoot = Path.GetFullPath(Path.GetTempPath()).TrimEnd(Path.DirectorySeparatorChar) + Path.DirectorySeparatorChar;
        Check(fixture.StartsWith(tempRoot, StringComparison.OrdinalIgnoreCase), "Fixture escapes Temp");
        Directory.CreateDirectory(fixture);
        var manager = new AssetsManager
        {
            Game = GameManager.GetGameByType(GameType.ArknightsEndfield), SpecifyUnityVersion = "2021.3.34f5",
            ResolveDependencies = false, FilterData = new() { Items = new() }
        };
        try
        {
            foreach (var row in rows) CopyVerified(package, fixture, row.GetProperty("cab").GetString(), row.GetProperty("snapshot"));
            foreach (var row in streams) CopyVerified(package, fixture, row.GetProperty("member").GetString(), row.GetProperty("snapshot"));
            TypeFlags.SetTypes(new());
            foreach (var type in new[] { ClassIDType.GameObject, ClassIDType.Transform, ClassIDType.RectTransform,
                ClassIDType.Mesh, ClassIDType.MeshFilter, ClassIDType.MeshRenderer, ClassIDType.SkinnedMeshRenderer,
                ClassIDType.Material, ClassIDType.Texture2D, ClassIDType.AssetBundle, ClassIDType.MonoBehaviour,
                ClassIDType.MonoScript, ClassIDType.Animator, ClassIDType.Avatar }) TypeFlags.SetType(type, true, false);
            Logger.Default = new ConsoleLogger();
            Logger.Flags = LoggerEvent.Error;
            manager.LoadFiles(rows.Select(row => Path.Combine(fixture, row.GetProperty("cab").GetString())).ToArray(), mergeSplitAssets: false);
            Check(manager.assetsFileList.Count == rows.Length, "Parsed SerializedFile count");
            var objects = manager.assetsFileList.SelectMany(file => file.Objects).ToDictionary(Key);
            foreach (var row in rows)
            {
                var file = manager.assetsFileList.Single(f => f.fileName == row.GetProperty("cab").GetString());
                Check(file.Objects.Count == file.m_Objects.Count, "Object parsing failed in " + file.fileName);
                var header = row.GetProperty("header");
                Check(file.header.m_FileSize == header.GetProperty("fileSize").GetInt64() &&
                    file.header.m_DataOffset == header.GetProperty("dataOffset").GetInt64() &&
                    file.header.m_MetadataSize == header.GetProperty("metadataSize").GetUInt32() &&
                    (int)file.header.m_Version == header.GetProperty("version").GetInt32(), "SerializedFile header");
                var table = row.GetProperty("objects").EnumerateArray().ToArray();
                Check(table.Length == file.m_Objects.Count, "Object table size");
                for (var i = 0; i < table.Length; i++)
                {
                    var actual = file.m_Objects[i]; var expected = table[i];
                    Check(actual.m_PathID == expected.GetProperty("pathId").GetInt64() &&
                        actual.byteStart == expected.GetProperty("offset").GetInt64() && actual.byteSize == expected.GetProperty("size").GetUInt32() &&
                        actual.classID == expected.GetProperty("classId").GetInt32() && actual.typeID == expected.GetProperty("typeIndex").GetInt32(), "Object table entry");
                }
                var types = row.GetProperty("types").EnumerateArray().ToArray();
                Check(types.Length == file.m_Types.Count, "Type table size");
                for (var i = 0; i < types.Length; i++)
                    Check(Hex(file.m_Types[i].m_OldTypeHash) == types[i].GetProperty("oldTypeHash").GetString() &&
                        (file.m_Types[i].m_Type?.m_Nodes?.Count ?? 0) == types[i].GetProperty("typeTreeNodes").GetInt32(), "Type hash/schema");
                var externals = row.GetProperty("externals").EnumerateArray().ToArray();
                Check(externals.Length == file.m_Externals.Count, "External table size");
                for (var i = 0; i < externals.Length; i++)
                    Check(file.m_Externals[i].pathName == externals[i].GetProperty("path").GetString() &&
                        file.m_Externals[i].guid.ToString() == externals[i].GetProperty("guid").GetString(), "External identity");
            }
            var uses = manifest.GetProperty("rendererUses").EnumerateArray().ToArray();
            foreach (var use in uses)
            {
                var renderer = (Renderer)objects[use.GetProperty("identity").GetString()];
                Pointer(renderer.m_GameObject, use.GetProperty("owner").GetString());
                if (renderer is SkinnedMeshRenderer skinned)
                {
                    Pointer(skinned.m_Mesh, use.GetProperty("mesh"));
                    Pointer(skinned.m_RootBone, use.GetProperty("rootBone"));
                    Pointer(skinned.m_SkinningRoot, use.GetProperty("skinningRoot"));
                    var bones = use.GetProperty("bones").EnumerateArray().ToArray();
                    Check(skinned.m_Bones.Count == bones.Length, "Bone palette length");
                    for (var i = 0; i < bones.Length; i++) Pointer(skinned.m_Bones[i], bones[i]);
                }
                else
                {
                    var filterId = use.GetProperty("meshFilter").GetString();
                    if (filterId != null) Pointer(((MeshFilter)objects[filterId]).m_Mesh, use.GetProperty("mesh"));
                }
                var materials = use.GetProperty("materials").EnumerateArray().ToArray();
                Check(renderer.m_Materials.Count == materials.Length, "Material slots");
                for (var i = 0; i < materials.Length; i++) Pointer(renderer.m_Materials[i], materials[i].GetProperty("pointer"));
            }
            foreach (var node in manifest.GetProperty("hierarchy").EnumerateArray())
            {
                var go = (GameObject)objects[node.GetProperty("identity").GetString()];
                var transform = (Transform)objects[node.GetProperty("transform").GetString()];
                Pointer(transform.m_GameObject, Key(go));
                Pointer(transform.m_Father, node.GetProperty("parent"));
                var components = node.GetProperty("components").EnumerateArray().ToArray();
                Check(components.Length == go.m_Components.Count, "Component count");
                for (var i = 0; i < components.Length; i++) Pointer(go.m_Components[i], components[i]);
            }
            foreach (var resource in manifest.GetProperty("resources").EnumerateArray())
            {
                var asset = objects[resource.GetProperty("identity").GetString()];
                Check(asset.Name == resource.GetProperty("name").GetString(), "Author resource name");
                if (asset is Material material) Pointer(material.m_Shader, resource.GetProperty("details").GetProperty("shader"));
            }
            var root = manifest.GetProperty("root").GetString();
            var entry = manifest.GetProperty("containerEntries")[0];
            var bundle = (AssetBundle)objects[entry.GetProperty("bundle").GetString()];
            Check(bundle.m_Container.Any(e => e.Key == entry.GetProperty("serializedKey").GetString() &&
                e.Value.asset.TryGet(out var target) && Key(target) == root), "Container root pointer");
            var result = new { format = 1, verified = true, offlineOnly = true, package,
                serializedFiles = rows.Length, rawStreams = streams.Length,
                objects = objects.Count, rendererUses = uses.Length,
                nodes = manifest.GetProperty("hierarchy").GetArrayLength(),
                checks = new[] { "Snapshot sizes/SHA256", "Native source reparse", "Header/object/type/external tables",
                    "Exact root entry", "Renderer Mesh/Material/bones/root/skinningRoot PPtrs", "Hierarchy components", "Material Shader PPtrs" } };
            File.WriteAllText(args[2], JsonSerializer.Serialize(result, new JsonSerializerOptions { WriteIndented = true }) + "\n");
            Console.WriteLine(JsonSerializer.Serialize(result));
            return 0;
        }
        finally
        {
            manager.Clear();
            // Resolve and verify the final absolute target immediately before recursive cleanup.
            Check(Path.GetFullPath(fixture).StartsWith(tempRoot, StringComparison.OrdinalIgnoreCase) &&
                Path.GetFileName(fixture).StartsWith("EFFSourceValidation-", StringComparison.Ordinal), "Unsafe fixture cleanup");
            Directory.Delete(fixture, recursive: true);
        }
    }

    private static void CopyVerified(string package, string fixture, string member, JsonElement snapshot)
    {
        Check(member == Path.GetFileName(member), "Invalid Bundle member name");
        var path = Path.GetFullPath(Path.Combine(package, snapshot.GetProperty("file").GetString()));
        Check(path.StartsWith(package.TrimEnd(Path.DirectorySeparatorChar) + Path.DirectorySeparatorChar, StringComparison.OrdinalIgnoreCase), "Snapshot path escape");
        using var stream = File.OpenRead(path);
        Check(stream.Length == snapshot.GetProperty("size").GetInt64(), "Snapshot size");
        Check(Convert.ToHexString(SHA256.HashData(stream)) == snapshot.GetProperty("sha256").GetString(), "Snapshot hash");
        File.Copy(path, Path.Combine(fixture, member));
    }

    private static void Pointer<T>(PPtr<T> pointer, JsonElement expected) where T : AssetObject
    {
        if (expected.ValueKind == JsonValueKind.Null) { Check(pointer == null, "Absent pointer"); return; }
        Check(pointer != null && pointer.m_FileID == expected.GetProperty("fileId").GetInt32() &&
            pointer.m_PathID == expected.GetProperty("pathId").GetInt64(), "PPtr payload");
        Pointer(pointer, expected.GetProperty("identity").GetString());
    }
    private static void Pointer<T>(PPtr<T> pointer, string identity) where T : AssetObject
    {
        if (pointer == null || pointer.IsNull) { Check(identity == null, "Null PPtr"); return; }
        Check(pointer.Cast<AssetObject>().TryGet(out var actual) && Key(actual) == identity, "PPtr target: " + identity);
    }
    private static string Hex(byte[] value) => value == null ? null : Convert.ToHexString(value);
    private static string Key(AssetObject obj) => obj.assetsFile.fileName.ToLowerInvariant() + ":" + obj.m_PathID;
    private static void Check(bool value, string message) { if (!value) throw new InvalidDataException(message); }
}

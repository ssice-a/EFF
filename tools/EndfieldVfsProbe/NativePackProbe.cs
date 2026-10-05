// Offline native resource writer evidence. Never opens the game process.
using System.Security.Cryptography;
using System.Text.Json;
using AnimeStudio;

internal static partial class NativePackProbe
{
    private static readonly JsonSerializerOptions JsonOptions = new() { WriteIndented = true };

    public static int Current(string[] args)
    {
        if (args.Length != 3) throw new ArgumentException("--export-observed-schema <decoded inputs> <new evidence JSON>");
        if (File.Exists(args[2])) throw new IOException("Current SerializedFile evidence exists");
        var manager = NewManager();
        var rows = new List<object>();
        foreach (var path in Directory.EnumerateFiles(args[1], "CAB-*.bytes", SearchOption.AllDirectories).Order())
        {
            if (path.EndsWith(".resS.bytes", StringComparison.OrdinalIgnoreCase)) continue;
            var bytes = File.ReadAllBytes(path);
            var cab = Path.GetFileNameWithoutExtension(path);
            using var reader = new FileReader(Path.Combine(Path.GetDirectoryName(path), cab), new MemoryStream(bytes));
            var file = new SerializedFile(reader, manager);
            var types = file.m_Types.Select((t, i) => new {
                index = i, classId = t.classID, scriptTypeIndex = t.m_ScriptTypeIndex,
                oldTypeHash = Convert.ToHexString(t.m_OldTypeHash),
                typeTreeNodes = t.m_Type?.m_Nodes?.Count ?? 0,
                nodes = t.m_Type?.m_Nodes?.Select(n => new {
                    type = n.m_Type, name = n.m_Name, level = n.m_Level,
                    byteSize = n.m_ByteSize, metaFlag = n.m_MetaFlag, typeFlags = n.m_TypeFlags }) }).ToArray();
            rows.Add(new {
                cab, unityVersion = file.unityVersion, types,
                snapshot = new { file = Path.GetFullPath(path), size = bytes.Length,
                    sha256 = Convert.ToHexString(SHA256.HashData(bytes)) },
                header = new { fileSize = file.header.m_FileSize, dataOffset = file.header.m_DataOffset,
                    metadataSize = file.header.m_MetadataSize, version = (int)file.header.m_Version },
                objects = file.m_Objects.Select(o => new { pathId = o.m_PathID, offset = o.byteStart,
                    size = o.byteSize, classId = o.classID, typeIndex = o.typeID }),
                externals = file.m_Externals.Select(e => new { path = e.pathName, guid = e.guid.ToString(), type = e.type }) });
        }
        File.WriteAllText(args[2], JsonSerializer.Serialize(new { format = "EFF_CURRENT_SERIALIZED_EVIDENCE",
            version = 1, nativeSubmission = false, files = rows }, JsonOptions));
        Console.WriteLine($"Current SerializedFile schemas/tables={rows.Count}");
        return 0;
    }

    public static int Run(string[] args)
    {
        if (args.Length != 3 || args[0] != "--export-native-schema")
            throw new ArgumentException("--export-native-schema SOURCE_PACKAGE NEW_SCHEMA_JSON");
        var output = Path.GetFullPath(args[2]);
        if (File.Exists(output)) throw new IOException("Evidence already exists.");
        var package = Path.GetFullPath(args[1]);
        using var doc = JsonDocument.Parse(File.ReadAllText(Path.Combine(package, "source", "manifest.json")));
        var manifest = doc.RootElement;
        Check(manifest.GetProperty("format").GetString() == "EFFSOURCE" && manifest.GetProperty("version").GetInt32() == 1, "Source version");
        var temp = Path.Combine(Path.GetTempPath(), "EFFNativeSchema-" + Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(temp);
        var manager = NewManager();
        try
        {
            foreach (var row in manifest.GetProperty("serializedFiles").EnumerateArray())
                Copy(package, temp, row.GetProperty("cab").GetString(), row.GetProperty("snapshot"));
            foreach (var row in manifest.GetProperty("rawStreams").EnumerateArray())
                Copy(package, temp, row.GetProperty("member").GetString(), row.GetProperty("snapshot"));
            var paths = manifest.GetProperty("serializedFiles").EnumerateArray()
                .Select(r => Path.Combine(temp, r.GetProperty("cab").GetString())).ToArray();
            manager.LoadFiles(paths, mergeSplitAssets: false);
            Check(manager.assetsFileList.Count == paths.Length, "Native source count");
            var result = new
            {
                format = "EFF_NATIVE_SCHEMA", version = 1, sourcePackage = package,
                vfsFingerprint = manifest.GetProperty("vfsFingerprint").GetString(),
                files = manager.assetsFileList.Select(f => new
                {
                    cab = f.fileName, unityVersion = f.unityVersion,
                    types = f.m_Types.Select((t, i) => new
                    {
                        index = i, classId = t.classID, scriptTypeIndex = t.m_ScriptTypeIndex,
                        oldTypeHash = Convert.ToHexString(t.m_OldTypeHash),
                        nodes = t.m_Type?.m_Nodes?.Select(n => new
                        {
                            type = n.m_Type, name = n.m_Name, level = n.m_Level,
                            byteSize = n.m_ByteSize, metaFlag = n.m_MetaFlag, typeFlags = n.m_TypeFlags
                        })
                    })
                })
            };
            File.WriteAllText(output, JsonSerializer.Serialize(result, JsonOptions) + "\n");
            Console.WriteLine(JsonSerializer.Serialize(new { verified = true, files = paths.Length, schema = output }));
            return 0;
        }
        finally
        {
            manager.Clear();
            var resolved = Path.GetFullPath(temp);
            Check(resolved.StartsWith(Path.GetFullPath(Path.GetTempPath()).TrimEnd(Path.DirectorySeparatorChar) + Path.DirectorySeparatorChar,
                StringComparison.OrdinalIgnoreCase) && Path.GetFileName(resolved).StartsWith("EFFNativeSchema-"), "Cleanup path");
            Directory.Delete(resolved, true);
        }
    }

    internal static AssetsManager NewManager()
    {
        TypeFlags.SetTypes(new());
        foreach (var type in new[] { ClassIDType.GameObject, ClassIDType.Transform, ClassIDType.RectTransform,
            ClassIDType.Mesh, ClassIDType.MeshFilter, ClassIDType.MeshRenderer, ClassIDType.SkinnedMeshRenderer,
            ClassIDType.Material, ClassIDType.Texture2D, ClassIDType.AssetBundle, ClassIDType.MonoBehaviour,
            ClassIDType.MonoScript, ClassIDType.Animator, ClassIDType.Avatar }) TypeFlags.SetType(type, true, false);
        Logger.Default = new ConsoleLogger();
        Logger.Flags = LoggerEvent.Error;
        return new AssetsManager { Game = GameManager.GetGameByType(GameType.ArknightsEndfield),
            SpecifyUnityVersion = "2021.3.34f5", ResolveDependencies = false, FilterData = new() { Items = new() } };
    }

    private static void Copy(string package, string temp, string member, JsonElement snapshot)
    {
        Check(member == Path.GetFileName(member), "Member path");
        var path = Path.GetFullPath(Path.Combine(package, snapshot.GetProperty("file").GetString()));
        Check(path.StartsWith(package.TrimEnd(Path.DirectorySeparatorChar) + Path.DirectorySeparatorChar, StringComparison.OrdinalIgnoreCase), "Snapshot path");
        var bytes = File.ReadAllBytes(path);
        Check(bytes.LongLength == snapshot.GetProperty("size").GetInt64() && Convert.ToHexString(SHA256.HashData(bytes)) == snapshot.GetProperty("sha256").GetString(), "Snapshot hash");
        File.WriteAllBytes(Path.Combine(temp, member), bytes);
    }

    internal static void Check(bool condition, string message)
    {
        if (!condition) throw new InvalidDataException(message);
    }
}

// Offline evidence only. A CAB dependency is not a component Mesh reference.
using System.Collections;
using System.Globalization;
using System.Text.Json;
using AnimeStudio;
using AnimeStudio.GUI;
using AssetObject = AnimeStudio.Object;

internal static class PrefabMeshReferenceProbe
{
    private static readonly JsonSerializerOptions JsonOptions = new() { WriteIndented = true };
    private static string Key(string cab, string pathId) => cab.ToLowerInvariant() + ":" + pathId;
    private static string Key(AssetObject obj) => Key(obj.assetsFile.fileName, obj.m_PathID.ToString(CultureInfo.InvariantCulture));

    public static int Run(string[] args)
    {
        if (args.Length != 5)
        {
            Console.Error.WriteLine("--inspect-mesh-consumers/--inspect-prefab-container-roots VFS_ROOT WORKSPACE CANDIDATES_JSON NEW_OUTPUT_JSON");
            return 2;
        }
        if (File.Exists(args[4])) throw new IOException("Evidence output already exists.");
        var useContainerRoots = args[0] == "--inspect-prefab-container-roots";
        using var input = JsonDocument.Parse(File.ReadAllText(args[3]));
        var manifest = input.RootElement;
        var archive = EndfieldVfsArchive.Open(args[1]);
        if (archive.Fingerprint != manifest.GetProperty("fingerprint").GetString())
            throw new InvalidDataException("Index fingerprint does not match the current VFS.");
        var targetKeys = manifest.GetProperty("targets").EnumerateArray().Select(t =>
            Key(t.GetProperty("cab").GetString(), t.GetProperty("pathID").GetString())).ToHashSet();
        var bundles = manifest.GetProperty("bundles").EnumerateArray().ToArray();
        var cabSources = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
        foreach (var bundle in bundles)
            foreach (var cab in bundle.GetProperty("cabs").EnumerateArray())
                cabSources.Add(cab.GetString(), bundle.GetProperty("source").GetString());
        var failures = new List<object>();
        var paths = new List<string>();
        foreach (var bundle in bundles)
        {
            var source = bundle.GetProperty("source").GetString();
            try { paths.Add(archive.ExtractToCache(source, args[2])); }
            catch (Exception ex) { failures.Add(new { source, stage = "extract", error = ex.Message }); }
        }
        // Parse component references without loading Mesh/GPU or texture payloads.
        TypeFlags.SetTypes(new());
        foreach (var type in new[] { ClassIDType.GameObject, ClassIDType.Transform,
                     ClassIDType.SkinnedMeshRenderer, ClassIDType.MeshRenderer, ClassIDType.MeshFilter,
                     ClassIDType.MonoBehaviour, ClassIDType.MonoScript, ClassIDType.Material, ClassIDType.AssetBundle,
                     ClassIDType.Animator })
            TypeFlags.SetType(type, true, false);
        Logger.Default = new ConsoleLogger();
        Logger.Flags = LoggerEvent.Error;
        var manager = new AssetsManager
        {
            Game = GameManager.GetGameByType(GameType.ArknightsEndfield),
            SpecifyUnityVersion = ProbeUnityVersion.Override, ResolveDependencies = false
        };
        Console.WriteLine($"Inspecting {paths.Count} source bundles, without dependency payload loading.");
        manager.LoadFiles(paths.ToArray(), mergeSplitAssets: false);
        var prefabOwners = new Dictionary<string, HashSet<string>>();
        var rootResults = new List<object>();
        // AssetInfo.asset is the logical asset's entry pointer. The preload range is
        // dependency membership, not proof that any one entry is the prefab root.
        var containers = manager.assetsFileList.SelectMany(f => f.Objects).OfType<AssetBundle>()
            .SelectMany(ab => ab.m_Container.Select(entry => new { bundle = ab, entry })).ToArray();
        var containerEntries = containers.Select(c =>
        {
            c.entry.Value.asset.TryGet(out AssetObject asset);
            return new { source = cabSources.GetValueOrDefault(c.bundle.assetsFile.fileName),
                assetBundle = Key(c.bundle), rawKey = c.entry.Key,
                resolvedKey = ResolveContainerKey(c.entry.Key),
                asset = Pointer(c.bundle.assetsFile, c.entry.Value.asset.m_FileID, c.entry.Value.asset.m_PathID),
                resolvedType = asset?.type.ToString(), resolvedName = asset?.Name,
                c.entry.Value.preloadIndex, c.entry.Value.preloadSize };
        }).ToArray();
        foreach (var candidate in manifest.GetProperty("candidates").EnumerateArray())
        {
            var source = candidate.GetProperty("source").GetString();
            var container = candidate.GetProperty("container").GetString();
            if (useContainerRoots)
            {
                var matches = containers.Where(c =>
                    cabSources.GetValueOrDefault(c.bundle.assetsFile.fileName) == source &&
                    string.Equals(ResolveContainerKey(c.entry.Key), container, StringComparison.OrdinalIgnoreCase)).ToArray();
                AssetObject asset = null;
                GameObject root = null;
                var resolvedAssets = matches.Select(m =>
                {
                    m.entry.Value.asset.TryGet(out AssetObject value);
                    return value;
                }).Where(value => value != null).ToArray();
                // A container can have additional entries for embedded subassets.
                // Select its unique GameObject entry by type and exact PPtr identity,
                // never by root name, order, or preload-table membership.
                var directRoots = resolvedAssets.OfType<GameObject>().DistinctBy(Key).ToArray();
                if (directRoots.Length == 1)
                {
                    asset = directRoots[0];
                    root = directRoots[0];
                }
                else if (matches.Length == 1 && resolvedAssets.Length == 1)
                {
                    asset = resolvedAssets[0];
                    if (asset is Component component) component.m_GameObject.TryGet(out root);
                }
                var issues = new List<string>();
                var seen = new HashSet<string>();
                if (root != null) VisitChecked(root, container, prefabOwners, seen, issues);
                rootResults.Add(new { container, source, containerMatchCount = matches.Length,
                    directGameObjectEntryCount = directRoots.Length,
                    containerAssets = matches.Select(m => Pointer(m.bundle.assetsFile,
                        m.entry.Value.asset.m_FileID, m.entry.Value.asset.m_PathID)).ToArray(),
                    rootEvidence = root == null ? "unresolved-AssetInfo.asset" : asset is GameObject
                        ? "AssetBundle.m_Container.AssetInfo.asset-GameObject"
                        : "AssetBundle.m_Container.AssetInfo.asset-Component.m_GameObject",
                    assetIdentity = asset == null ? null : Key(asset), assetType = asset?.type.ToString(),
                    root = root == null ? null : Key(root), rootName = root?.m_Name,
                    rootCount = root == null ? 0 : 1, visitedGameObjects = seen.Count, hierarchyIssues = issues });
                continue;
            }
            var mainIds = candidate.GetProperty("records").EnumerateArray()
                .Where(r => r.GetProperty("type").GetString() == "GameObject")
                .Select(r => r.GetProperty("pathID").GetString()).ToHashSet();
            var files = manager.assetsFileList.Where(f => cabSources.TryGetValue(f.fileName, out var s) && s == source).ToArray();
            var objects = files.SelectMany(f => f.Objects).OfType<GameObject>();
            var roots = objects.Where(o => mainIds.Contains(o.m_PathID.ToString(CultureInfo.InvariantCulture)) &&
                                           o.m_Transform?.m_Father.IsNull == true).ToArray();
            var rootEvidence = "indexed-GameObject-PathID";
            if (roots.Length == 0)
            {
                // This authoring aid is labelled separately from exact indexed roots.
                var stem = Path.GetFileNameWithoutExtension(container);
                roots = objects.Where(o => o.m_Name == stem && o.m_Transform?.m_Father.IsNull == true).ToArray();
                rootEvidence = "unique-name-in-exact-source-CAB";
            }
            if (roots.Length == 1) Visit(roots[0], container, prefabOwners, new HashSet<string>());
            rootResults.Add(new { container, source, rootCount = roots.Length, rootEvidence,
                                  root = roots.Length == 1 ? Key(roots[0]) : null });
        }
        var meshUses = new List<object>();
        var allMeshRefs = new List<object>();
        var materials = new List<object>();
        var unknownComponents = new List<object>();
        foreach (var file in manager.assetsFileList)
        {
            foreach (var obj in file.Objects)
            {
                if (obj is SkinnedMeshRenderer smr)
                {
                    var extra = new { materials = smr.m_Materials.Select(p => Pointer(file, p.m_FileID, p.m_PathID)).ToArray(),
                                      bones = smr.m_Bones.Count, blendShapeWeights = smr.m_BlendShapeWeights.Length };
                    AddMesh(obj, "m_Mesh", smr.m_Mesh.m_FileID, smr.m_Mesh.m_PathID, extra);
                }
                else if (obj is MeshFilter mf)
                    AddMesh(obj, "m_Mesh", mf.m_Mesh.m_FileID, mf.m_Mesh.m_PathID, null);
                else if (obj is Material material)
                    materials.Add(new { identity = Key(obj), name = obj.Name,
                        shader = Pointer(file, material.m_Shader.m_FileID, material.m_Shader.m_PathID),
                        textures = material.m_SavedProperties.m_TexEnvs.Select(p => new
                        { property = p.Key, reference = Pointer(file, p.Value.m_Texture.m_FileID, p.Value.m_Texture.m_PathID) }).ToArray() });
                else if (obj is MonoBehaviour behaviour)
                {
                    var position = obj.reader.Position;
                    try
                    {
                        var fields = obj.ToType();
                        if (fields == null) unknownComponents.Add(new { identity = Key(obj), reason = "type-tree-unavailable" });
                        else Walk(fields, "", (field, fileId, pathId) => AddMesh(obj, field, fileId, pathId, null), file, targetKeys);
                    }
                    catch (Exception ex) { unknownComponents.Add(new { identity = Key(obj), reason = ex.Message }); }
                    finally { obj.reader.Position = position; }
                }
            }
        }
        void AddMesh(AssetObject obj, string field, int fileId, long pathId, object extra)
        {
            var pointer = Pointer(obj.assetsFile, fileId, pathId);
            if (pointer.identity == null) return;
            var component = obj as Component;
            GameObject owner = null;
            component?.m_GameObject.TryGet(out owner);
            var owners = owner != null && prefabOwners.TryGetValue(Key(owner), out var p) ? p.OrderBy(s => s).ToArray() : Array.Empty<string>();
            var row = new { component = Key(obj), componentType = obj.type.ToString(), field,
                            mesh = pointer, owner = owner?.m_Name, ownerIdentity = owner == null ? null : Key(owner),
                            prefabs = owners, source = cabSources.GetValueOrDefault(obj.assetsFile.fileName),
                            exactTargetMatch = targetKeys.Contains(pointer.identity), details = extra };
            allMeshRefs.Add(row);
            if (targetKeys.Contains(pointer.identity)) meshUses.Add(row);
        }
        var loaded = manager.assetsFileList.Select(f => f.fileName).ToHashSet(StringComparer.OrdinalIgnoreCase);
        foreach (var cab in cabSources.Keys.Where(c => !loaded.Contains(c)))
            failures.Add(new { cab, stage = "parse", error = "Expected source CAB was not loaded" });
        var report = new { format = 1, offlineEvidenceOnly = true, runtimeActionUseProven = false,
            replacementRulesGenerated = false, input = Path.GetFullPath(args[3]), archive.Fingerprint,
            loadedFileCount = loaded.Count, rootMappingMode = useContainerRoots ? "AssetInfo.asset" : "indexed-or-name-heuristic",
            knownHashedPaths = AssetsHelper.Paths.Count, containerEntries, prefabRoots = rootResults, exactMeshUseCount = meshUses.Count,
            exactMeshUses = meshUses, rendererMeshReferences = allMeshRefs, materials, unknownComponents, failures,
            limits = new[] { "No action association until runtime evidence is collected.",
                "No reference is inferred from a CAB dependency or a component name.",
                "Absent/type-tree-unavailable runtime components may assign Meshes dynamically.",
                "Material/texture references are identities, not replacement filenames." } };
        Directory.CreateDirectory(Path.GetDirectoryName(Path.GetFullPath(args[4])));
        File.WriteAllText(args[4], JsonSerializer.Serialize(report, JsonOptions) + "\n");
        Console.WriteLine($"Exact target Mesh references={meshUses.Count}; unknown components={unknownComponents.Count}; failures={failures.Count}.");
        return failures.Count == 0 ? 0 : 1;
    }

    private sealed record PointerInfo(string identity, int fileId, string pathID, string cab, string failure);
    private static PointerInfo Pointer(SerializedFile file, int fileId, long pathId)
    {
        string cab = fileId == 0 ? file.fileName : fileId > 0 && fileId <= file.m_Externals.Count
            ? file.m_Externals[fileId - 1].fileName : null;
        var id = pathId.ToString(CultureInfo.InvariantCulture);
        return new(pathId != 0 && cab != null ? Key(cab, id) : null, fileId, id, cab,
                   pathId == 0 ? "null-reference" : cab == null ? "invalid-external-file-index" : null);
    }

    private static void Visit(GameObject obj, string prefab, Dictionary<string, HashSet<string>> owners, HashSet<string> seen)
    {
        var key = Key(obj);
        if (!seen.Add(key)) return;
        if (!owners.TryGetValue(key, out var paths)) owners.Add(key, paths = new());
        paths.Add(prefab);
        if (obj.m_Transform == null) return;
        foreach (var ptr in obj.m_Transform.m_Children)
            if (ptr.TryGet(out var t) && t.m_GameObject.TryGet(out var child)) Visit(child, prefab, owners, seen);
    }

    private static string ResolveContainerKey(string key) =>
        ulong.TryParse(key, out var hash) && AssetsHelper.Paths.TryGetValue(hash, out var path) ? path : key;

    private static void VisitChecked(GameObject obj, string prefab, Dictionary<string, HashSet<string>> owners,
        HashSet<string> seen, List<string> issues)
    {
        var key = Key(obj);
        if (!seen.Add(key))
        {
            issues.Add("repeated-hierarchy-node:" + key);
            return;
        }
        if (!owners.TryGetValue(key, out var paths)) owners.Add(key, paths = new());
        paths.Add(prefab);
        if (obj.m_Transform == null)
        {
            issues.Add("unresolved-Transform:" + key);
            return;
        }
        foreach (var ptr in obj.m_Transform.m_Children)
        {
            if (!ptr.TryGet(out var transform) || !transform.m_GameObject.TryGet(out var child))
                issues.Add("unresolved-child:" + key + ":" + ptr.m_FileID + ":" + ptr.m_PathID);
            else VisitChecked(child, prefab, owners, seen, issues);
        }
    }

    private static void Walk(object value, string field, Action<string, int, long> add, SerializedFile file, HashSet<string> targets)
    {
        if (value is IDictionary dict)
        {
            if (dict.Contains("m_FileID") && dict.Contains("m_PathID"))
            {
                var fileId = Convert.ToInt32(dict["m_FileID"]);
                var pathId = Convert.ToInt64(dict["m_PathID"]);
                var ptr = Pointer(file, fileId, pathId);
                if (ptr.identity != null && targets.Contains(ptr.identity)) add(field, fileId, pathId);
                return;
            }
            foreach (DictionaryEntry entry in dict) Walk(entry.Value, field + "." + entry.Key, add, file, targets);
        }
        else if (value is KeyValuePair<object, object> pair)
        {
            Walk(pair.Key, field + ".Key", add, file, targets);
            Walk(pair.Value, field + ".Value", add, file, targets);
        }
        else if (value is IEnumerable items && value is not string && value is not byte[])
        {
            var index = 0;
            foreach (var item in items) Walk(item, field + $"[{index++}]", add, file, targets);
        }
    }
}

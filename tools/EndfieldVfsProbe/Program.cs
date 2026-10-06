using System.Diagnostics;
using System.Security.Cryptography;
using AnimeStudio;
using AnimeStudio.GUI;

args = ProbeUnityVersion.ReadOption(args);
if (args.Length > 0 && args[0] == "--prepare-static-sources")
{
    try { return StaticModSourceCommand.Run(args); }
    catch (Exception error) { Console.Error.WriteLine("STATIC-SOURCES-REJECT reason=" + error.Message); return 1; }
}
if (args.Length == 1 && args[0] == "--test-package-contracts")
{
    try { return PackageContractTests.Run(); }
    catch (Exception error) { Console.Error.WriteLine("PACKAGE-CONTRACTS failed: " + error.Message); return 1; }
}

if (args.Length > 0 && args[0] == "--inspect-observed-inputs")
    return ObservedInputProbe.Run(args);
if (args.Length > 0 && args[0] == "--resolve-target-input-ranges")
    return ObservedInputProbe.Ranges(args);
if (args.Length > 0 && args[0] == "--resolve-input-source-ranges")
    return ObservedInputProbe.SourceRanges(args);

if (args.Length > 0 && args[0] == "--verify-source-baseline")
    return SourceBaselineProbe.Run(args);

if (args.Length > 0 && args[0] == "--export-native-schema")
    return NativePackProbe.Run(args);
if (args.Length > 0 && args[0] == "--export-observed-schema")
    return NativePackProbe.Current(args);

if (args.Length > 0 && args[0] == "--verify-native-pack")
    return NativePackProbe.Verify(args);

if (args.Length > 0 && (args[0] == "--inspect-mesh-consumers" || args[0] == "--inspect-prefab-container-roots"))
    return PrefabMeshReferenceProbe.Run(args);

if (args.Length > 0 && (args[0] == "--inspect-prefab-components" || args[0] == "--inspect-resource-components"))
    return PrefabComponentProbe.Run(args);

if (args.Length > 0 && args[0] == "--inspect-prefab-animation")
    return PrefabAnimationProbe.Run(args);

if (args.Length == 3 && args[0] == "--find-index")
{
    var index = EndfieldIndexStore.Load(args[1]).Assets;
    var query = args[2];
    var matches = index.Records.Where(x =>
            x.Container.Contains(query, StringComparison.OrdinalIgnoreCase) ||
            x.Name.Contains(query, StringComparison.OrdinalIgnoreCase))
        .Take(500).ToArray();
    foreach (var record in matches)
        Console.WriteLine($"{record.Type}|{record.PathId}|{record.Name}|{record.Container}|{record.Source}");
    Console.WriteLine($"matches={matches.Length}");
    return matches.Length > 0 ? 0 : 1;
}

if (args.Length == 4 && args[0] == "--find-record")
{
    var index = EndfieldIndexStore.Load(args[1]).Assets;
    var matches = index.Records.Where(x =>
            string.Equals(x.Name, args[2], StringComparison.OrdinalIgnoreCase) &&
            x.Container.Contains(args[3], StringComparison.OrdinalIgnoreCase))
        .ToArray();
    foreach (var record in matches)
        Console.WriteLine($"{record.Type}|{record.PathId}|{record.Name}|{record.Container}|{record.Source}");
    Console.WriteLine($"matches={matches.Length}");
    return matches.Length > 0 ? 0 : 1;
}

if (args.Length == 2 && args[0] == "--vfs-stats")
{
    var statsArchive = EndfieldVfsArchive.Open(args[1]);
    var bundles = statsArchive.Entries.Values.Where(x => x.LogicalPath.EndsWith(".ab", StringComparison.OrdinalIgnoreCase)).ToArray();
    Console.WriteLine($"files={statsArchive.Entries.Count:N0} bundles={bundles.Length:N0} bundleBytes={bundles.Sum(x => x.Length) / 1024d / 1024d / 1024d:N2} GiB fingerprint={statsArchive.Fingerprint}");
    return 0;
}

if (args.Length == 4 && args[0] == "--build-index")
{
    var buildArchive = EndfieldVfsArchive.Open(args[1]);
    var limit = int.Parse(args[3]);
    var progress = new Progress<EndfieldIndexProgress>(value =>
    {
        if (value.ProcessedBundles % 1024 == 0 || value.ProcessedBundles == value.TotalBundles)
            Console.WriteLine($"{value.ProcessedBundles}/{value.TotalBundles} assets={value.AssetCount} cabs={value.CabCount} failed={value.FailedBundles}");
    });
    var path = await EndfieldVfsAssetIndexBuilder.BuildAsync(
        buildArchive, args[2], GameManager.GetGameByType(GameType.ArknightsEndfield),
        progress, CancellationToken.None, limit);
    var built = EndfieldIndexStore.Load(path).Assets;
    Console.WriteLine($"index={path} assets={built.AssetCount} directories={built.DirectoryCount}");
    return built.AssetCount > 0 ? 0 : 1;
}

if (args.Length == 3 && args[0] == "--stream-map")
{
    var streamArchive = EndfieldVfsArchive.Open(args[1]);
    var bytes = streamArchive.ReadPayload(args[2]);
    using var payload = new MemoryStream(bytes, writable: false);
    var entries = AssetsHelper.BuildAssetMapFromStream(payload, args[2], GameManager.GetGameByType(GameType.ArknightsEndfield));
    Console.WriteLine($"entries={entries.Count}");
    foreach (var entry in entries.Take(10))
        Console.WriteLine($"{entry.Type}|{entry.PathID}|{entry.Name}|{entry.Container}|{entry.Source}");
    return entries.Count > 0 ? 0 : 1;
}

if (args.Length == 4 && args[0] == "--extract")
{
    var extractArchive = EndfieldVfsArchive.Open(args[1]);
    var output = extractArchive.ExtractToCache(args[2], args[3]);
    Console.WriteLine(output);
    return File.Exists(output) ? 0 : 1;
}

if (args.Length == 5 && args[0] == "--extract-closure")
{
    var closureArchive = EndfieldVfsArchive.Open(args[1]);
    var dependencyIndex = EndfieldIndexStore.Load(args[2]).Dependencies;
    var bundles = dependencyIndex.ResolveBundleClosure(args[3]);
    foreach (var bundle in bundles)
        Console.WriteLine(closureArchive.ExtractToCache(bundle, args[4]));
    Console.WriteLine($"bundles={bundles.Count}");
    return bundles.Count > 0 ? 0 : 1;
}

if (args.Length == 6 && args[0] == "--inspect-mesh")
{
    var meshArchive = EndfieldVfsArchive.Open(args[1]);
    var loadedIndex = EndfieldIndexStore.Load(args[2]);
    var logicalPath = args[3];
    var targetPathId = long.Parse(args[5]);
    var record = loadedIndex.Assets.Records.FirstOrDefault(x =>
        x.PathId == targetPathId && x.Type == "Mesh" &&
        string.Equals(x.Container, logicalPath, StringComparison.OrdinalIgnoreCase));
    if (record == null)
    {
        Console.Error.WriteLine("target Mesh index record not found");
        return 1;
    }
    var bundlePaths = loadedIndex.Dependencies.ResolveBundleClosure(record.Source);
    var cachePaths = bundlePaths.Select(path => meshArchive.ExtractToCache(path, args[4])).ToArray();
    var manager = new AssetsManager
    {
        Game = GameManager.GetGameByType(GameType.ArknightsEndfield),
        SpecifyUnityVersion = ProbeUnityVersion.Override,
        ResolveDependencies = false,
    };
    manager.LoadFiles(cachePaths, mergeSplitAssets: false);
    var sourceCabs = loadedIndex.Dependencies.GetCabNames(record.Source).ToHashSet(StringComparer.OrdinalIgnoreCase);
    var meshes = manager.assetsFileList.Where(file => sourceCabs.Contains(file.fileName))
        .SelectMany(file => file.Objects).OfType<Mesh>().Where(mesh => mesh.m_PathID == targetPathId).ToArray();
    if (meshes.Length != 1)
    {
        Console.Error.WriteLine("Expected one exact Mesh in the source Bundle; found " + meshes.Length);
        return 1;
    }
    var mesh = meshes[0];

    Console.WriteLine($"mesh={mesh.Name} pathId={mesh.m_PathID} cab={mesh.assetsFile.fileName} " +
                      $"vertices={mesh.m_VertexCount} indices={mesh.m_Indices.Count} " +
                      $"skin={mesh.m_Skin?.Count ?? 0} bindposes={mesh.m_BindPose?.Length ?? 0} " +
                      $"boneHashes={mesh.m_BoneNameHashes?.Length ?? 0}");
    if (mesh.m_Skin?.Count > 0)
    {
        var skin = mesh.m_Skin[0];
        Console.WriteLine("skin[0].weights=" + string.Join(",", skin.weight.Select(x => x.ToString("R"))));
        Console.WriteLine("skin[0].indices=" + string.Join(",", skin.boneIndex));
    }
    if (mesh.m_BindPose?.Length > 0)
        Console.WriteLine("bindpose[0]=" + string.Join(",", Enumerable.Range(0, 16)
            .Select(i => mesh.m_BindPose[0][i].ToString("R"))));

    var field = typeof(Mesh).GetField("m_VertexData",
        System.Reflection.BindingFlags.Instance | System.Reflection.BindingFlags.NonPublic);
    var vertexData = field?.GetValue(mesh) as VertexData;
    if (vertexData == null)
    {
        Console.Error.WriteLine("Mesh vertex data is unavailable");
        return 1;
    }
    Console.WriteLine($"vertexData.vertices={vertexData.m_VertexCount} bytes={vertexData.m_DataSize.Length} " +
                      $"channels={vertexData.m_Channels.Count} streams={vertexData.m_Streams.Count}");
    for (var index = 0; index < vertexData.m_Channels.Count; index++)
    {
        var channel = vertexData.m_Channels[index];
        Console.WriteLine($"channel[{index}] stream={channel.stream} offset={channel.offset} " +
                          $"format={channel.format} dimension={channel.dimension}");
    }
    for (var index = 0; index < vertexData.m_Streams.Count; index++)
    {
        var stream = vertexData.m_Streams[index];
        Console.WriteLine($"stream[{index}] mask=0x{stream.channelMask:X8} offset={stream.offset} stride={stream.stride}");
    }
    foreach (var index in new[] { 12, 13 })
    {
        if (index >= vertexData.m_Channels.Count)
            continue;
        var channel = vertexData.m_Channels[index];
        if (channel.dimension == 0 || channel.stream >= vertexData.m_Streams.Count)
            continue;
        var stream = vertexData.m_Streams[channel.stream];
        var componentSize = (int)MeshHelper.GetFormatSize(
            MeshHelper.ToVertexFormat(channel.format, mesh.version));
        var size = componentSize * channel.dimension;
        var offset = checked((int)stream.offset + channel.offset);
        Console.WriteLine($"channel[{index}].vertex[0].raw=" +
                          Convert.ToHexString(vertexData.m_DataSize.AsSpan(offset, size)));
    }
    return 0;
}

if (args.Length == 6 && args[0] == "--inspect-closure")
{
    var closureArchive = EndfieldVfsArchive.Open(args[1]);
    var dependencyIndex = EndfieldIndexStore.Load(args[2]).Dependencies;
    var logicalPath = args[3];
    var targetPathId = long.Parse(args[5]);
    var bundlePaths = dependencyIndex.ResolveBundleClosure(logicalPath);
    var cachePaths = bundlePaths.Select(path => closureArchive.ExtractToCache(path, args[4])).ToArray();
    var manager = new AssetsManager
    {
        Game = GameManager.GetGameByType(GameType.ArknightsEndfield),
        SpecifyUnityVersion = ProbeUnityVersion.Override,
        ResolveDependencies = false,
    };
    manager.LoadFiles(cachePaths, mergeSplitAssets: false);
    var sourceCabs = dependencyIndex.GetCabNames(logicalPath).ToHashSet(StringComparer.OrdinalIgnoreCase);
    foreach (var file in manager.assetsFileList.Where(x => sourceCabs.Contains(x.fileName)))
    {
        var gameObject = file.Objects.OfType<GameObject>()
            .FirstOrDefault(x => x.m_PathID == targetPathId);
        if (gameObject == null)
            continue;
        Console.WriteLine($"files={manager.assetsFileList.Count} bundles={bundlePaths.Count} object={gameObject.m_Name} hasModel={gameObject.HasModel()}");
        return gameObject.HasModel() ? 0 : 1;
    }
    Console.Error.WriteLine("target GameObject not found");
    return 1;
}

if (args.Length == 6 && args[0] == "--inspect-prefab")
{
    var prefabArchive = EndfieldVfsArchive.Open(args[1]);
    var loadedIndex = EndfieldIndexStore.Load(args[2]);
    var logicalPath = args[3];
    var prefabName = args[5];
    var bundlePaths = loadedIndex.Dependencies.ResolveBundleClosure(logicalPath);
    var cachePaths = bundlePaths.Select(path => prefabArchive.ExtractToCache(path, args[4])).ToArray();
    var manager = new AssetsManager
    {
        Game = GameManager.GetGameByType(GameType.ArknightsEndfield),
        SpecifyUnityVersion = ProbeUnityVersion.Override,
        ResolveDependencies = false,
    };
    manager.LoadFiles(cachePaths, mergeSplitAssets: false);
    var matches = manager.assetsFileList.SelectMany(x => x.Objects).OfType<GameObject>()
        .Where(x => string.Equals(x.m_Name, prefabName, StringComparison.OrdinalIgnoreCase))
        .ToArray();
    foreach (var gameObject in matches)
        Console.WriteLine($"pathId={gameObject.m_PathID} cab={gameObject.assetsFile.fileName} " +
                          $"root={gameObject.m_Transform?.m_Father.IsNull == true} hasModel={gameObject.HasModel()}");
    var records = loadedIndex.Assets.Records.Where(x =>
            string.Equals(x.Source, logicalPath, StringComparison.OrdinalIgnoreCase) &&
            x.Container.EndsWith($"/{prefabName}.prefab", StringComparison.OrdinalIgnoreCase))
        .ToArray();
    if (records.Length == 0)
        records = loadedIndex.Assets.Records.Where(x =>
                x.Container.EndsWith($"/{prefabName}.prefab", StringComparison.OrdinalIgnoreCase))
            .ToArray();
    var file = records.Length == 0 ? null : new VirtualAssetFile(records[0].Container, records);
    var root = file == null ? null : EndfieldPrefabDocument.FindRoot(
        manager.assetsFileList.SelectMany(x => x.Objects).OfType<GameObject>(), file,
        loadedIndex.Dependencies.GetCabNames(logicalPath));
    var structure = root == null ? string.Empty : EndfieldPrefabDocument.Build(file, root);
    Console.WriteLine($"files={manager.assetsFileList.Count} bundles={bundlePaths.Count} matches={matches.Length} " +
                      $"resolvedRoot={root?.m_PathID.ToString() ?? "none"} structureChars={structure.Length} " +
                      $"hasMeshRefs={structure.Contains(" Mesh=")} hasMaterialRefs={structure.Contains(" Materials=[")}");
    return root != null && structure.Contains(" Mesh=") ? 0 : 1;
}

if (args.Length == 7 && args[0] == "--export-model")
{
    var closureArchive = EndfieldVfsArchive.Open(args[1]);
    var dependencyIndex = EndfieldIndexStore.Load(args[2]).Dependencies;
    var logicalPath = args[3];
    var targetPathId = long.Parse(args[5]);
    var bundlePaths = dependencyIndex.ResolveBundleClosure(logicalPath);
    var cachePaths = bundlePaths.Select(path => closureArchive.ExtractToCache(path, args[4])).ToArray();
    var game = GameManager.GetGameByType(GameType.ArknightsEndfield);
    var manager = new AssetsManager { Game = game, SpecifyUnityVersion = ProbeUnityVersion.Override, ResolveDependencies = false };
    manager.LoadFiles(cachePaths, mergeSplitAssets: false);
    var sourceCabs = dependencyIndex.GetCabNames(logicalPath).ToHashSet(StringComparer.OrdinalIgnoreCase);
    var gameObject = manager.assetsFileList.Where(x => sourceCabs.Contains(x.fileName))
        .SelectMany(x => x.Objects).OfType<GameObject>()
        .FirstOrDefault(x => x.m_PathID == targetPathId);
    if (gameObject == null || !gameObject.HasModel())
    {
        Console.Error.WriteLine("target model was not found");
        return 1;
    }

    var uvs = Enumerable.Range(0, 8).ToDictionary(
        i => $"UV{i}", i => (i < 2, i < 2 ? i : 0));
    var convert = new ModelConverter(gameObject, new ModelConverter.Options
    {
        imageFormat = ImageFormat.Png,
        game = game,
        collectAnimations = false,
        exportMaterials = true,
        materials = new HashSet<Material>(),
        uvs = uvs,
        texs = new Dictionary<string, int>(),
    });
    var output = Path.GetFullPath(args[6]);
    ModelExporter.ExportFbx(output, convert, new Fbx.ExportOptions
    {
        eulerFilter = true,
        filterPrecision = 0.25f,
        exportAllNodes = true,
        exportSkins = true,
        exportAnimations = false,
        exportBlendShape = true,
        castToBone = false,
        boneSize = 10,
        scaleFactor = 1,
        fbxVersion = 3,
        fbxFormat = 0,
    });
    Console.WriteLine($"output={output} meshes={convert.MeshList.Count} materials={convert.MaterialList.Count} textures={convert.TextureList.Count} morphs={convert.MorphList.Count}");
    return File.Exists(output) && new FileInfo(output).Length > 0 ? 0 : 1;
}

if (args.Length == 2 && args[0] == "--assetmap")
{
    var mapStopwatch = Stopwatch.StartNew();
    var map = Path.GetExtension(args[1]).Equals(".eidx", StringComparison.OrdinalIgnoreCase)
        ? EndfieldIndexStore.Load(args[1]).Assets
        : VirtualAssetPathIndex.LoadXml(args[1]);
    GC.Collect();
    Console.WriteLine($"assets={map.AssetCount:N0} directories={map.DirectoryCount:N0} elapsed={mapStopwatch.Elapsed} managed={GC.GetTotalMemory(false) / 1024d / 1024d:N1} MiB");
    return 0;
}

if (args.Length != 4)
{
    Console.Error.WriteLine("usage: EndfieldVfsProbe --assetmap <xml> | --stream-map <VFS root> <logical path> | --extract <VFS root> <logical path> <workspace> | --extract-closure <VFS root> <index XML> <logical path> <workspace> | --inspect-closure <VFS root> <index XML> <logical path> <workspace> <PathID> | --export-model <VFS root> <index XML> <logical path> <workspace> <PathID> <output FBX> | <VFS root> <logical path> <workspace> <known plaintext file>");
    return 2;
}

var stopwatch = Stopwatch.StartNew();
var archive = EndfieldVfsArchive.Open(args[0]);
Console.WriteLine($"index entries={archive.Entries.Count:N0} elapsed={stopwatch.Elapsed}");
var extracted = archive.ExtractToCache(args[1], args[2]);
var actual = Convert.ToHexString(SHA256.HashData(File.ReadAllBytes(extracted)));
var expected = Convert.ToHexString(SHA256.HashData(File.ReadAllBytes(args[3])));
Console.WriteLine($"logical={args[1]}");
Console.WriteLine($"output={extracted}");
Console.WriteLine($"actual={actual}");
Console.WriteLine($"expected={expected}");
Console.WriteLine($"equal={actual == expected}");
return actual == expected ? 0 : 1;

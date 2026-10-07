using System.Buffers.Binary;
using System.Text;
using System.Text.Json;
using AnimeStudio;
using AnimeStudio.GUI;

/// <summary>Pure synthetic contracts; never opens the game, VFS, or an author package.</summary>
internal static class PackageContractTests
{
    public static int Run()
    {
        var testRoot = Path.Combine(Path.GetTempPath(), "EFFPackageContracts-" + Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(testRoot);
        try
        {
            Dictionary<string, string> Names() => new(StringComparer.OrdinalIgnoreCase);
            var names = Names();
            var first = EiemPackageIdentity.PayloadFileName("meshes", "same", "CAB-a", 42, ".mesh", names);
            Check(Path.GetFileName(first) == "same.mesh", "Unique payload should keep the readable original name");
            var second = EiemPackageIdentity.PayloadFileName("meshes", "same", "CAB-b", 42, ".mesh", names);
            Check(first != second, "PathID must not collide across CABs");
            Check(Path.GetFileName(second).Length == "same_12345678.mesh".Length, "Only a short suffix is needed on collision");
            var repeated = Names();
            Check(first == EiemPackageIdentity.PayloadFileName("meshes", "same", "cab-A", 42, ".mesh", repeated), "CAB case normalization");
            Check(second == EiemPackageIdentity.PayloadFileName("meshes", "same", "cab-B", 42, ".mesh", repeated), "Stable suffix for the same source and export order");
            Check(second != EiemPackageIdentity.PayloadFileName("meshes", "SAME", "CAB-c", 42, ".mesh", names), "Case-insensitive filesystem collision");
            Reject(() => EiemPackageIdentity.PayloadFileName("meshes", "same", "cab-A", 42, ".mesh", names), "Duplicate exact source payload");
            var occupied = Names();
            occupied.Add(first, "occupied original"); occupied.Add(second, "occupied suffix");
            var fallback = EiemPackageIdentity.PayloadFileName("meshes", "same", "CAB-b", 42, ".mesh", occupied);
            Check(fallback != first && fallback != second && Path.GetFileNameWithoutExtension(fallback).EndsWith("_2"), "Short-hash collision cannot overwrite another payload");
            var sanitized = Names();
            Check(EiemPackageIdentity.PayloadFileName("textures", "a:b", "CAB-a", 1, ".png", sanitized) !=
                EiemPackageIdentity.PayloadFileName("textures", "a?b", "CAB-b", 1, ".png", sanitized), "Sanitized filename collision");
            Check(Path.GetFileName(EiemPackageIdentity.PayloadFileName("textures", "CON", "CAB-a", 2, ".png", sanitized)) == "_CON.png", "Windows device filename");
            Check(EiemPackageIdentity.SnapshotFileName("a:b") != EiemPackageIdentity.SnapshotFileName("a?b"), "Snapshot sanitation collision");
            Reject(() => EiemPackageIdentity.PayloadFileName("meshes", "same", "", 42, ".mesh", Names()), "Missing source identity");

            var dependencies = new EndfieldBundleDependencyIndex();
            dependencies.AddCab("CAB-a", "Bundles/a.ab", new[] { "archive:/CAB-b" });
            dependencies.AddCab("CAB-b", "Bundles/b.ab", Array.Empty<string>());
            dependencies.AddCab("cab-A", "Bundles/a.ab", new[] { "CAB-b" });
            Check(dependencies.CabCount == 2 && dependencies.ResolveBundleClosure("Bundles/a.ab").SequenceEqual(new[] { "Bundles/a.ab", "Bundles/b.ab" }), "Dependency closure and idempotent CAB identity");
            Reject(() => dependencies.AddCab("CAB-a", "Bundles/other.ab", new[] { "CAB-b" }), "Conflicting CAB source");
            Reject(() => dependencies.AddCab("CAB-a", "Bundles/a.ab", Array.Empty<string>()), "Conflicting CAB dependencies");
            dependencies.AddCab("CAB-unrelated", "Bundles/unrelated.ab", new[] { "CAB-unavailable" });
            var graph = dependencies.DirectBundleDependencies(out var missing);
            Check(graph["bundles/a.ab"].SequenceEqual(new[] { "bundles/b.ab" }) &&
                !missing.ContainsKey("bundles/a.ab") && missing["bundles/unrelated.ab"].Single() == "CAB-unavailable",
                "Unrelated missing CAB does not invalidate selected dependency graph");
            var shipped = new EndfieldVfsEntry("bundle", "shipped.blc", Path.Combine(testRoot, "shipped.chk"), 0, 1, false, 0, 0, 0, "");
            File.WriteAllBytes(shipped.SourceChk, new byte[] { 1 });
            var cached = shipped with { SourceChk = Path.Combine(testRoot, "deleted-cache.chk") };
            Check(StaticModSourceCommand.ChooseSource(cached, shipped) == shipped, "Deleted download-cache chunk falls back to shipped source");
            File.WriteAllBytes(cached.SourceChk, new byte[] { 2 });
            Check(StaticModSourceCommand.ChooseSource(cached, shipped) == cached, "Available cached payload takes precedence");
            var noSourceRejected = false;
            try { StaticModSourceCommand.ChooseSource(null, null); }
            catch (FileNotFoundException) { noSourceRejected = true; }
            Check(noSourceRejected, "Missing physical source");

            var index = Path.Combine(testRoot, "index.eidx");
            WriteIndex(index, new[] { "Bundles/a.ab" });
            var loaded = EndfieldIndexStore.Load(index);
            Check(loaded.StoredAssetCount == 1 && loaded.StoredCabCount == 1, "Current index round trip");
            Check(EndfieldIndexStore.LoadPrefabExportIndex(index, "assets/test.prefab").Assets.Count == 1, "Compact index round trip");
            using (var stream = new FileStream(index, FileMode.Open, FileAccess.ReadWrite))
            {
                using var reader = new BinaryReader(stream, Encoding.UTF8, true);
                reader.ReadString();
                using var writer = new BinaryWriter(stream, Encoding.UTF8, true);
                writer.Write(4321); // Same fields, different informational release label.
            }
            Check(EndfieldIndexStore.Load(index).StoredAssetCount == 1 &&
                EndfieldIndexStore.LoadPrefabExportIndex(index, "assets/test.prefab").Assets.Count == 1,
                "Index compatibility follows complete fields, not the release label");
            using (var stream = new FileStream(index, FileMode.Append)) stream.WriteByte(1);
            Reject(() => EndfieldIndexStore.Load(index), "Index trailing bytes");
            WriteIndex(index, new[] { "Bundles/a.ab", "Bundles/b.ab" });
            Reject(() => EndfieldIndexStore.LoadPrefabExportIndex(index, "assets/test.prefab"), "Ambiguous Prefab source");
            using (var writer = new BinaryWriter(File.Create(index), Encoding.UTF8))
            {
                EndfieldIndexStore.WriteHeader(writer, "synthetic-vfs");
                writer.Write((byte)2); writer.Write("CAB-many"); writer.Write("Bundles/many.ab");
                writer.Write(100001);
                for (var i = 0; i < 100001; ++i) writer.Write("unity default resources");
                EndfieldIndexStore.WriteEnd(writer, 0, 1);
            }
            Check(EndfieldIndexStore.Load(index).StoredCabCount == 1, "CAB count is bounded by encoded bytes, not a resource capacity limit");

            var mesh = Path.Combine(testRoot, "minimal.mesh");
            WriteMesh(mesh, 1);
            EiemPackageValidator.ValidateMesh(mesh);
            WriteMesh(mesh, 0);
            Reject(() => EiemPackageValidator.ValidateMesh(mesh), "Missing exact donor");

            using var evidence = JsonDocument.Parse("{\"serializedFiles\":[{\"cab\":\"CAB-a\",\"unityVersion\":\"2022.3.10f1\"},{\"cab\":\"CAB-b\",\"unityVersion\":\"2021.3.34f5\"}]}");
            var manager = new AssetsManager { Game = GameManager.GetGameByType(GameType.ArknightsEndfield),
                StrippedUnityVersionResolver = ProbeUnityVersion.FromManifest(evidence.RootElement) };
            using var a = SyntheticReader("CAB-a", "0.0.0");
            var fileA = new SerializedFile(a, manager);
            manager.CheckStrippedVersion(fileA);
            Check(fileA.unityVersion == "2022.3.10f1", "Per-CAB version evidence");
            using var b = SyntheticReader("CAB-b", "2023.1.1f1");
            var fileB = new SerializedFile(b, manager);
            manager.CheckStrippedVersion(fileB);
            Check(fileB.unityVersion == "2023.1.1f1", "Embedded version takes precedence");
            using var absent = SyntheticReader("CAB-missing", "0.0.0");
            Reject(() => manager.CheckStrippedVersion(new SerializedFile(absent, manager)), "Missing stripped source evidence");
            Console.WriteLine("PACKAGE-CONTRACTS passed: CAB payload identity, snapshots, dependency conflicts, full/compact index, donor palette, per-CAB Unity version");
            return 0;
        }
        finally
        {
            var absolute = Path.GetFullPath(testRoot);
            var temp = Path.GetFullPath(Path.GetTempPath()).TrimEnd(Path.DirectorySeparatorChar) + Path.DirectorySeparatorChar;
            Check(absolute.StartsWith(temp, StringComparison.OrdinalIgnoreCase) &&
                Path.GetFileName(absolute).StartsWith("EFFPackageContracts-", StringComparison.Ordinal), "Fixture cleanup target");
            Directory.Delete(absolute, recursive: true);
        }
    }

    private static FileReader SyntheticReader(string cab, string version)
    {
        using var metadata = new MemoryStream();
        using (var writer = new BinaryWriter(metadata, Encoding.UTF8, leaveOpen: true))
        {
            writer.Write(Encoding.UTF8.GetBytes(version)); writer.Write((byte)0);
            writer.Write((int)BuildTarget.StandaloneWindows64); writer.Write(false);
            for (var i = 0; i < 5; i++) writer.Write(0); // types, objects, scripts, externals, reference types
            writer.Write((byte)0); // user information
        }
        var bytes = new byte[48 + metadata.Length];
        BinaryPrimitives.WriteUInt32BigEndian(bytes.AsSpan(8), 22);
        BinaryPrimitives.WriteUInt32BigEndian(bytes.AsSpan(20), (uint)metadata.Length);
        BinaryPrimitives.WriteInt64BigEndian(bytes.AsSpan(24), bytes.Length);
        BinaryPrimitives.WriteInt64BigEndian(bytes.AsSpan(32), bytes.Length);
        metadata.ToArray().CopyTo(bytes, 48);
        return new FileReader(cab, new MemoryStream(bytes, writable: false));
    }

    private static void WriteIndex(string path, string[] sources)
    {
        using var writer = new BinaryWriter(File.Create(path), Encoding.UTF8);
        EndfieldIndexStore.WriteHeader(writer, "synthetic-vfs");
        foreach (var source in sources)
        {
            writer.Write((byte)1); writer.Write("test"); writer.Write("assets/test.prefab");
            writer.Write("GameObject"); writer.Write(42L); writer.Write(source);
            writer.Write((byte)2); writer.Write("CAB-" + source); writer.Write(source); writer.Write(0);
        }
        EndfieldIndexStore.WriteEnd(writer, sources.Length, sources.Length);
    }

    private static void WriteMesh(string path, int donors)
    {
        using var writer = new BinaryWriter(File.Create(path), Encoding.UTF8);
        writer.Write(Encoding.ASCII.GetBytes("EFFMESH\0")); writer.Write(6);
        writer.Write("unity-y-up-left-handed"); writer.Write("assets/test.mesh"); writer.Write("test"); writer.Write(0);
        for (var i = 0; i < 12; i++) writer.Write(0); // vertex vectors
        writer.Write(0); writer.Write(0); writer.Write(0); // indices, submeshes, weights
        writer.Write(1); for (var i = 0; i < 16; i++) writer.Write(0f); // bindpose
        writer.Write(0); writer.Write(0); writer.Write(0); // hashes, bone paths, index paths
        writer.Write(1); writer.Write("assets/test.mesh"); writer.Write("test"); writer.Write(0u);
        writer.Write(1); writer.Write(donors);
        for (var i = 0; i < donors; i++) { writer.Write("assets/test.mesh"); writer.Write("test"); writer.Write(0u); }
        for (var i = 0; i < 5; i++) writer.Write(0); // blend-shape arrays
    }

    private static void Check(bool condition, string message)
    {
        if (!condition) throw new InvalidDataException("Package contract: " + message);
    }

    private static void Reject(Action action, string message)
    {
        try { action(); }
        catch (InvalidDataException) { return; }
        catch (ArgumentException) { return; }
        catch (Exception error) when (message == "Missing stripped source evidence" && error.Message.Contains("stripped", StringComparison.OrdinalIgnoreCase)) { return; }
        throw new InvalidDataException("Expected rejection: " + message);
    }
}

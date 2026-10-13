using System.Diagnostics;
using System.Security.Cryptography;
using System.Text;
using System.Text.Json;
using AnimeStudio.GUI;

// Author-side preparation only. Reads an existing index and selected ranges;
// it never queries a running game or constructs a whole-game payload index.
internal static class StaticModSourceCommand
{
    private const string Structure = "EFF_STATIC_RESOURCE_SELECTION/paths+ranges+dependencies/le64";
    private sealed record Input(string Name, string Digest, string File, ulong Offset, ulong Size);
    internal static EndfieldVfsEntry ChooseSource(EndfieldVfsEntry cached, EndfieldVfsEntry shipped)
    {
        // Download-cache metadata can outlive the cached CHK. It cannot
        // override a shipped source unless its physical payload still exists.
        if (cached != null && File.Exists(cached.SourceChk)) return cached;
        return shipped ?? throw new FileNotFoundException("No available physical source for a selected bundle.");
    }
    private static string Logical(string value) => value.Replace('\\', '/').ToLowerInvariant();
    private static void String(BinaryWriter writer, string value)
    {
        var bytes = Encoding.UTF8.GetBytes(value);
        writer.Write((ulong)bytes.Length); writer.Write(bytes);
    }
    private static string String(BinaryReader reader)
    {
        var length = reader.ReadUInt64();
        if (length > int.MaxValue || length > (ulong)(reader.BaseStream.Length - reader.BaseStream.Position))
            throw new InvalidDataException("Static source string exceeds its encoded extent.");
        return Encoding.UTF8.GetString(reader.ReadBytes((int)length));
    }
    private static void Set(BinaryWriter writer, IEnumerable<string> values)
    {
        var sorted = values.Distinct(StringComparer.Ordinal).Order(StringComparer.Ordinal).ToArray();
        writer.Write((ulong)sorted.Length); foreach (var value in sorted) String(writer, value);
    }
    private static string Request(string compiler, string mod)
    {
        var info = new ProcessStartInfo(compiler) { UseShellExecute = false, CreateNoWindow = true,
            RedirectStandardOutput = true, RedirectStandardError = true };
        info.ArgumentList.Add("--source-request"); info.ArgumentList.Add(mod);
        using var process = Process.Start(info) ?? throw new IOException("Cannot start source request reader.");
        var output = process.StandardOutput.ReadToEnd(); var error = process.StandardError.ReadToEnd();
        process.WaitForExit();
        if (process.ExitCode != 0) throw new InvalidDataException("Author source request failed: " + error);
        return output.Trim();
    }
    public static int Run(string[] args)
    {
        if (args.Length != 5 && (args.Length != 6 || args[5] != "--refresh"))
            throw new ArgumentException("--prepare-static-sources GAME INDEX MOD COMPILER [--refresh]");
        var game = Path.GetFullPath(args[1]); var indexPath = Path.GetFullPath(args[2]);
        var mod = Path.GetFullPath(args[3]); var compiler = Path.GetFullPath(args[4]);
        var archive = EndfieldVfsArchive.Open(game);
        var catalogIdentity = EndfieldIndexStore.Load(indexPath, new HashSet<string>(StringComparer.OrdinalIgnoreCase)).VfsFingerprint;
        if (catalogIdentity != archive.Fingerprint) throw new InvalidDataException("Source index must describe both installed VFS layers; reopen the game folder with current AnimeStudio.");
        var request = Request(compiler, mod);
        var requestHash = Convert.ToHexString(SHA256.HashData(Encoding.UTF8.GetBytes(request + "\nselection=reverse-mesh-consumers+declared-assets+material-dependency-closure\nresourceCatalog="+catalogIdentity)));
        var destination = Path.Combine(mod, "source-inputs.bin");
        if (args.Length == 5 && File.Exists(destination))
        {
            var old = File.ReadAllBytes(destination);
            if (old.Length >= 32 && SHA256.HashData(old.AsSpan(0, old.Length - 32)).AsSpan().SequenceEqual(old.AsSpan(old.Length - 32)))
            {
                using var stream = new MemoryStream(old, 0, old.Length - 32, false);
                using var reader = new BinaryReader(stream, Encoding.UTF8);
                if (String(reader) == Structure && String(reader) == requestHash)
                { Console.WriteLine("STATIC-SOURCES-REUSED targetDeclarationsUnchanged=1"); return 0; }
            }
        }
        using var document = JsonDocument.Parse(request);
        var sources = document.RootElement.GetProperty("sources").EnumerateArray().ToArray();
        var requestedContainers = sources.Select(s => Logical(s.GetProperty("path").GetString()!))
            .ToHashSet(StringComparer.OrdinalIgnoreCase);
        var index = EndfieldIndexStore.Load(indexPath, requestedContainers);
        if (index.VfsFingerprint != archive.Fingerprint)
            throw new InvalidDataException("The existing source index no longer describes both installed VFS layers; regenerate it with AnimeStudio.");
        var assets = new SortedDictionary<string, string>(StringComparer.Ordinal);
        var meshSources = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        var materialSources = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        foreach (var source in sources)
        {
            var path = Logical(source.GetProperty("path").GetString()!);
            var name = source.GetProperty("asset").GetString()!;
            var kind = source.GetProperty("kind").GetString()!;
            // A Prefab container may be indexed through Animator/script rows,
            // without a separate GameObject asset row. Resolve its one native
            // bundle by exact container; the compiler verifies the root and
            // retained component schemas against the actual serialized bytes.
            var matches = kind == "Prefab"
                ? index.Assets.Records.Where(r => Logical(r.Container) == path)
                    .DistinctBy(r => Logical(r.Source)).ToArray()
                : index.Assets.Records.Where(r => Logical(r.Container) == path && r.Name == name && r.Type == kind)
                    .DistinctBy(r => (Logical(r.Source), r.PathId)).ToArray();
            if (matches.Length != 1) throw new InvalidDataException($"Expected one exact {kind} source for {path}/{name}; found {matches.Length}.");
            var bundle = Logical(matches[0].Source);
            if (assets.TryGetValue(path, out var old) && old != bundle) throw new InvalidDataException("Conflicting source bundles for " + path);
            assets[path] = bundle; if (kind == "Mesh") meshSources.Add(bundle);
            if (kind == "Material") materialSources.Add(bundle);
        }
        var graph = index.Dependencies.DirectBundleDependencies(out var unresolved);
        var reverse = new Dictionary<string, HashSet<string>>(StringComparer.OrdinalIgnoreCase);
        foreach (var row in graph) foreach (var dependency in row.Value)
        {
            if (!reverse.TryGetValue(dependency, out var owners)) reverse[dependency] = owners = new(StringComparer.OrdinalIgnoreCase);
            owners.Add(row.Key);
        }
        var candidates = new HashSet<string>(meshSources, StringComparer.OrdinalIgnoreCase);
        var queue = new Queue<string>(meshSources);
        while (queue.TryDequeue(out var bundle)) if (reverse.TryGetValue(bundle, out var owners))
            foreach (var owner in owners) if (candidates.Add(owner)) queue.Enqueue(owner);
        var selected = new HashSet<string>(candidates, StringComparer.OrdinalIgnoreCase);
        selected.UnionWith(assets.Values);
        // Slot templates are direct dependencies of declared source assets.
        // Prefab dependencies remain owned by the game's normal loader.
        foreach (var bundle in assets.Values.ToArray())
            if (graph.TryGetValue(bundle, out var dependencies)) selected.UnionWith(dependencies);
        // A borrowed Material may use textures and Shader dependencies that
        // the target character never loads. Keep its native dependency closure.
        var materialQueue = new Queue<string>(materialSources);
        var materialClosure = new HashSet<string>(materialSources, StringComparer.OrdinalIgnoreCase);
        while (materialQueue.TryDequeue(out var materialBundle))
            if (graph.TryGetValue(materialBundle, out var dependencies))
                foreach (var dependency in dependencies)
                    if (materialClosure.Add(dependency)) materialQueue.Enqueue(dependency);
        selected.UnionWith(materialClosure);
        foreach (var record in index.Assets.Records.Where(r => r.Type == "GameObject" && r.Container.EndsWith(".prefab", StringComparison.OrdinalIgnoreCase) && candidates.Contains(Logical(r.Source))))
        {
            var path = Logical(record.Container); var bundle = Logical(record.Source);
            if (assets.TryGetValue(path, out var old) && old != bundle) throw new InvalidDataException("Ambiguous Prefab source: " + path);
            assets[path] = bundle;
        }
        var inputs = new List<Input>();
        foreach (var bundle in selected.Order(StringComparer.Ordinal))
        {
            if (!archive.TryGet(bundle, out var entry)) throw new InvalidDataException("Source absent from current VFS: " + bundle);
            if (entry.Encrypted) throw new InvalidDataException("Outer-encrypted static source is unsupported: " + bundle);
            var bytes = archive.ReadPayload(entry.LogicalPath);
            if (Convert.ToHexString(MD5.HashData(bytes)) != entry.FileDataMd5) throw new InvalidDataException("Source range MD5 differs: " + bundle);
            var relative = Path.GetRelativePath(game, entry.SourceChk).Replace('\\', '/');
            if (relative.StartsWith("../", StringComparison.Ordinal) || Path.IsPathRooted(relative)) throw new InvalidDataException("Source file escapes game directory.");
            inputs.Add(new(bundle, Convert.ToHexString(SHA256.HashData(bytes)), relative, (ulong)entry.Offset, (ulong)entry.Length));
        }
        using var output = new MemoryStream();
        using (var writer = new BinaryWriter(output, Encoding.UTF8, true))
        {
            String(writer, Structure); String(writer, requestHash);
            writer.Write((ulong)assets.Count); foreach (var asset in assets) { String(writer, asset.Key); String(writer, asset.Value); }
            writer.Write((ulong)inputs.Count); foreach (var input in inputs)
            { String(writer, input.Name); String(writer, input.Digest); String(writer, input.File); writer.Write(input.Offset); writer.Write(input.Size); }
            writer.Write((ulong)selected.Count); foreach (var bundle in selected.Order(StringComparer.Ordinal)) { String(writer, bundle); Set(writer, graph[bundle].Where(selected.Contains).Select(Logical)); }
            Set(writer, candidates.Select(Logical));
        }
        var payload = output.ToArray(); output.Write(SHA256.HashData(payload));
        var temporary = destination + ".tmp-" + Environment.ProcessId;
        try { File.WriteAllBytes(temporary, output.ToArray()); File.Move(temporary, destination, true); }
        finally { if (File.Exists(temporary)) File.Delete(temporary); }
        Console.WriteLine($"STATIC-SOURCES-PREPARED paths={assets.Count} candidates={candidates.Count} inputs={inputs.Count} descriptorBytes={output.Length} sourceBytes={inputs.Sum(i => (long)i.Size)}");
        return 0;
    }
}

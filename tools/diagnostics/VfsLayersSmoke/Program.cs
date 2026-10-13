using AnimeStudio.GUI;
using System.Text.Json;

try
{
if(args.Length==2&&args[0]=="--write-fixture"){LayerTests.WriteFixture(args[1]);return;}
if (args.Length == 2 && args[0] == "--audit-game")
{
    var data = Path.Combine(args[1], "Endfield_Data");
    var baseVfs = EndfieldVfsArchive.OpenLayer(Path.Combine(data, "StreamingAssets", "VFS"));
    var hotVfs = EndfieldVfsArchive.OpenLayer(Path.Combine(data, "Persistent", "VFS"));
    var cipher = typeof(EndfieldVfsArchive).GetMethod("ChaCha20Xor", System.Reflection.BindingFlags.NonPublic | System.Reflection.BindingFlags.Static)!.CreateDelegate<BlcCipher>();
    var key = Convert.FromHexString("E95B317AC4F828569D23A86BF271DCB53E846FA75C924D671DBA8E38F4CA52E1");
    foreach (var block in new[] { "7064D8E2", "19E3AE45", "07A1BB91" }) foreach (var layer in new[] { "StreamingAssets", "Persistent" }) {
        var path = Path.Combine(data, layer, "VFS", block, block + ".blc");
        if (!File.Exists(path)) continue;
        var raw = File.ReadAllBytes(path);var plain=raw.AsSpan(12).ToArray();cipher(plain,key,raw.AsSpan(0,12),1);
        using var reader = new BinaryReader(new MemoryStream(plain));
        var code=reader.ReadInt32();var version=code<11?reader.ReadInt32():code;
        Console.WriteLine($"HEADER {layer} {block} code={code} version={version}");
    }
    var missing = hotVfs.Entries.Values.Where(e => !File.Exists(e.SourceChk)).ToArray();
    var shared = hotVfs.Entries.Values.Where(e => baseVfs.TryGet(e.LogicalPath, out _)).ToArray();
    var unresolved = missing.Where(e => !baseVfs.TryGet(e.LogicalPath, out var b) || b.FileDataMd5 != e.FileDataMd5 || b.Length != e.Length).ToArray();
    Console.WriteLine(JsonSerializer.Serialize(new {
        baseFiles = baseVfs.Entries.Count, hotFiles = hotVfs.Entries.Count,
        baseOnly = baseVfs.Entries.Keys.Count(k => !hotVfs.TryGet(k, out _)),
        hotOnly = hotVfs.Entries.Keys.Count(k => !baseVfs.TryGet(k, out _)),
        hotMissingChunks = missing.Length,
        fallbackWithoutSameIdentity = unresolved.Length,
        unresolvedBundles = unresolved.Count(e => e.LogicalPath.EndsWith(".ab", StringComparison.OrdinalIgnoreCase)),
        baseOnlyBundles = baseVfs.Entries.Keys.Count(k => k.EndsWith(".ab", StringComparison.OrdinalIgnoreCase) && !hotVfs.TryGet(k, out _)),
        unresolvedBundleSamples = unresolved.Where(e => e.LogicalPath.EndsWith(".ab", StringComparison.OrdinalIgnoreCase)).Take(3),
        changedShared = shared.Count(e => baseVfs.Entries[e.LogicalPath].FileDataMd5 != e.FileDataMd5),
        unresolvedSamples = unresolved.Take(3)
    }, new JsonSerializerOptions { WriteIndented=true }));
    return;
}
if (args.Length == 3 && args[0] == "--verify-mounted")
{
    var effective = EndfieldVfsArchive.Open(args[1]);
    using var report = JsonDocument.Parse(File.ReadAllText(args[2]));
    int checkedCount = 0;
    foreach (var row in report.RootElement.GetProperty("sources").EnumerateArray())
    {
        if (!effective.TryGet(row.GetProperty("logical").GetString()!, out var entry) ||
            !string.Equals(Path.GetFullPath(entry.SourceChk), Path.GetFullPath(row.GetProperty("physical").GetString()!), StringComparison.OrdinalIgnoreCase) ||
            entry.Offset != row.GetProperty("offset").GetInt64() || entry.Length != row.GetProperty("bytes").GetInt64() ||
            entry.FileDataMd5 != row.GetProperty("fileContentMd5").GetString())
            throw new InvalidDataException("Mounted source differs: " + row.GetProperty("logical"));
        checkedCount++;
    }
    var parent = EndfieldVfsArchive.Open(Path.GetDirectoryName(args[1])!);
    if (parent.Fingerprint != effective.Fingerprint) throw new InvalidDataException("Parent selection identity differs");
    var unavailableBundles=effective.Entries.Values.Count(e=>e.LogicalPath.EndsWith(".ab",StringComparison.OrdinalIgnoreCase)&&!File.Exists(e.SourceChk));
    if(unavailableBundles!=0)throw new InvalidDataException("Installed Bundle payloads missing: "+unavailableBundles);
    Console.WriteLine($"PASS installed VFS: matchedMounted={checkedCount} layers={effective.LayerCount} files={effective.Entries.Count} bundles={effective.Entries.Keys.Count(k => k.EndsWith(".ab", StringComparison.OrdinalIgnoreCase))} unavailable={effective.MissingPayloadCount} unavailableBundles={unavailableBundles} fingerprint={effective.Fingerprint} parentSelection=1 noExtraction=1");
    return;
}
if (args.Length == 0) { LayerTests.Run(); return; }
throw new ArgumentException("--audit-game <game> / --verify-mounted <game> <report>");
}
catch (Exception error) { Console.Error.WriteLine("FAIL VFS layers: " + error); Environment.ExitCode=1; }
delegate void BlcCipher(Span<byte> data, ReadOnlySpan<byte> key, ReadOnlySpan<byte> nonce, uint counter);

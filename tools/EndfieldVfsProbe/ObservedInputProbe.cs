using System.Security.Cryptography;
using System.Text.Json;
using AnimeStudio;
using AnimeStudio.GUI;

// Independent AnimeStudio decoding of diagnostic copies only. Never loads the
// game DLL or writes to installed Mod/game input files.
internal static class ObservedInputProbe
{
    public static int SourceRanges(string[] args)
    {
        if (args.Length != 5)
            throw new ArgumentException("--resolve-input-source-ranges <candidate JSON> <persistent VFS> <streaming VFS> <new report>");
        if (File.Exists(args[4])) throw new IOException("Current source ranges report exists");
        using var selection = JsonDocument.Parse(File.ReadAllBytes(args[1]));
        var persistent = EndfieldVfsArchive.Open(args[2]);
        var streaming = EndfieldVfsArchive.Open(args[3]);
        if (streaming.Fingerprint != selection.RootElement.GetProperty("vfsFingerprint").GetString())
            throw new IOException("Locator index fingerprint differs from current StreamingAssets VFS");
        var ranges = new List<object>();
        foreach (var item in selection.RootElement.GetProperty("sources").EnumerateArray())
        {
            var bundle = item.GetString();
            if ((!persistent.TryGet(bundle, out var entry) || !File.Exists(entry.SourceChk)) &&
                (!streaming.TryGet(bundle, out entry) || !File.Exists(entry.SourceChk)))
                throw new IOException("Current candidate absent from VFS: " + bundle);
            if (entry.Encrypted) throw new IOException("Unsupported encrypted outer VFS range");
            ranges.Add(new { bundle, physicalPath = entry.SourceChk, offset = entry.Offset,
                bytes = entry.Length, outerEncrypted = false });
        }
        File.WriteAllText(args[4], JsonSerializer.Serialize(new {
            format = "EFF_CURRENT_INPUT_RANGES", version = 1, indexFingerprintVerified = true,
            streamingFingerprint = streaming.Fingerprint, persistentFingerprint = persistent.Fingerprint,
            ranges }, new JsonSerializerOptions { WriteIndented = true }));
        Console.WriteLine($"Current input ranges={ranges.Count}; locator fingerprint verified");
        return 0;
    }

    public static int Ranges(string[] args)
    {
        if (args.Length != 6)
            throw new ArgumentException("--resolve-target-input-ranges <source manifest> <matches> <persistent VFS> <streaming VFS> <new report>");
        if (File.Exists(args[5]))
            throw new IOException("Refusing to overwrite target input range report");
        using var source = JsonDocument.Parse(File.ReadAllBytes(args[1]));
        using var matches = JsonDocument.Parse(File.ReadAllBytes(args[2]));
        var selectedCabs = matches.RootElement.GetProperty("matches").EnumerateArray()
            .SelectMany(m => m.GetProperty("candidates").EnumerateArray())
            .Select(m => m.GetString().Split(':')[0]).ToHashSet(StringComparer.OrdinalIgnoreCase);
        var bundles = source.RootElement.GetProperty("serializedFiles").EnumerateArray()
            .Where(m => selectedCabs.Contains(m.GetProperty("cab").GetString()))
            .Select(m => m.GetProperty("sourceBundle").GetString()).Distinct().ToArray();
        var persistent = EndfieldVfsArchive.Open(args[3]);
        var streaming = EndfieldVfsArchive.Open(args[4]);
        var ranges = new List<object>();
        foreach (var bundle in bundles)
        {
            if ((!persistent.TryGet(bundle,out var entry) || !File.Exists(entry.SourceChk)) &&
                (!streaming.TryGet(bundle,out entry) || !File.Exists(entry.SourceChk)))
                throw new IOException("Selected diagnostic bundle absent from current VFS: " + bundle);
            if (entry.Encrypted)
                throw new IOException("Unsupported encrypted outer VFS range");
            ranges.Add(new { bundle, physicalPath = entry.SourceChk, offset = entry.Offset,
                bytes = entry.Length, outerEncrypted = entry.Encrypted,
                origin = "current VFS catalog; temporary diagnostics only; resource matching still uses logical path + asset" });
        }
        File.WriteAllText(args[5],JsonSerializer.Serialize(ranges,
            new JsonSerializerOptions { WriteIndented = true }));
        Console.WriteLine($"Resolved selected diagnostic ranges={ranges.Count}");
        return 0;
    }
    public static int Run(string[] args)
    {
        if (args.Length != 3)
            throw new ArgumentException("--inspect-observed-inputs <copies directory> <new output directory>");
        var output = Path.GetFullPath(args[2]);
        if (Directory.Exists(output))
            throw new IOException("Refusing to overwrite an existing input report");
        Directory.CreateDirectory(output);
        var reports = new List<object>();
        foreach (var input in Directory.GetFiles(args[1], "*.vfs").Order())
        {
            using var reader = new FileReader(input);
            var bundle = new VFSFile(reader, input, GameType.ArknightsEndfield);
            var folder = Path.Combine(output, Path.GetFileNameWithoutExtension(input));
            Directory.CreateDirectory(folder);
            var members = new List<object>();
            foreach (var member in bundle.fileList)
            {
                using var stream = member.stream;
                if (member.path != Path.GetFileName(member.path))
                    throw new IOException("Unexpected member path");
                stream.Position = 0;
                using var memory = new MemoryStream();
                stream.CopyTo(memory);
                var bytes = memory.ToArray();
                var file = Path.Combine(folder, member.path + ".bytes");
                File.WriteAllBytes(file, bytes);
                members.Add(new { name = member.path, bytes = bytes.Length,
                    sha256 = Convert.ToHexString(SHA256.HashData(bytes)), file });
            }
            reports.Add(new { input, rawBytes = new FileInfo(input).Length,
                decodedHeaderSize = bundle.m_Header.size,
                flags = (uint)bundle.m_Header.flags, encFlags = bundle.m_Header.encFlags, members });
        }
        File.WriteAllText(Path.Combine(output, "report.json"),
            JsonSerializer.Serialize(reports, new JsonSerializerOptions { WriteIndented = true }));
        Console.WriteLine($"Decoded inputs={reports.Count} output={output}");
        return 0;
    }
}

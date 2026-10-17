using System.Text;
using System.Text.Json;
using AnimeStudio;
using AnimeStudio.GUI;

// Blender's own offline helper. Shares the tested incremental parser and
// publication code; never starts AnimeStudio UI or refreshes author packages.
Console.OutputEncoding = new UTF8Encoding(false);
try
{
    if (args.Length == 1 && args[0] == "--protocol")
    {
        Console.WriteLine(JsonSerializer.Serialize(new { format = "EFF_SOURCE_INDEX_UPDATER", version = 1,
            update = "bundle-content/incremental", authorPackages = "unchanged" }));
        return 0;
    }
    if (args.Length != 3 || args[0] != "--update")
        throw new ArgumentException("Usage: --update GAME INDEX | --protocol");
    var index = Path.GetFullPath(args[2]);
    var directory = Path.GetDirectoryName(index)!;
    if (!Path.GetFileName(index).Equals("endfield_assets.eidx", StringComparison.OrdinalIgnoreCase) ||
        !Path.GetFileName(directory).Equals("index", StringComparison.OrdinalIgnoreCase))
        throw new ArgumentException("Source index must be a workspace index/endfield_assets.eidx");
    var workspace = Path.GetDirectoryName(directory)!;
    var archive = EndfieldVfsArchive.Open(args[1]);
    var progress = new Progress();
    var result = EndfieldVfsAssetIndexBuilder.BuildAsync(archive, workspace,
        GameManager.GetGameByType(GameType.ArknightsEndfield), progress, CancellationToken.None).GetAwaiter().GetResult();
    archive.VerifyCurrent();
    if (!EndfieldVfsAssetIndexBuilder.TryGetCurrentIndex(archive, workspace, out var current) ||
        !string.Equals(Path.GetFullPath(result), Path.GetFullPath(current), StringComparison.OrdinalIgnoreCase))
        throw new InvalidDataException("Source index update did not publish a complete current index");
    var last = progress.Last;
    Console.WriteLine("SOURCE-INDEX-PREPARED " + JsonSerializer.Serialize(new { index = result,
        fingerprint = archive.Fingerprint, parsed = last.ParsedBundles, reused = last.ReusedBundles,
        removed = last.RemovedBundles, milliseconds = last.Elapsed.TotalMilliseconds }));
    return 0;
}
catch (Exception error)
{
    Console.Error.WriteLine("SOURCE-INDEX-FAILED " + error);
    return 1;
}

sealed class Progress : IProgress<EndfieldIndexProgress>
{
    public EndfieldIndexProgress Last;
    public void Report(EndfieldIndexProgress value)
    {
        Last = value;
        if (value.ProcessedBundles == value.TotalBundles || value.ProcessedBundles % 4096 == 0)
            Console.WriteLine($"SOURCE-INDEX-PROGRESS {value.ProcessedBundles}/{value.TotalBundles} parsed={value.ParsedBundles} reused={value.ReusedBundles}");
    }
}

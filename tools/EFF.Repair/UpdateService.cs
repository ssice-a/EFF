using AnimeStudio.GUI;

namespace EffRepair;

// AnimeStudio and Repair use the same catalog, validation and download cache.
internal static class UpdateService
{
    internal static string DefaultSource => EndfieldIndexUpdates.DefaultSource;
    internal static Uri Https(string value) => EndfieldIndexUpdates.Https(value);
    internal static Task<RepairCatalog> Check(string source, CancellationToken token) => EndfieldIndexUpdates.Check(source, token);
    internal static RepairCatalog Parse(string json) => EndfieldIndexUpdates.Parse(json);
    internal static Task Download(IndexDownload index, string destination, IProgress<string>? progress, CancellationToken token) =>
        EndfieldIndexUpdates.Download(index, destination, progress, token);
}

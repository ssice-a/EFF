using AnimeStudio.GUI;
using System.Net;
using System.Net.Http.Headers;
using System.Text.Json;

namespace EffRepair;

internal sealed record IndexDownload(EndfieldIndexRelease Release, string Url);
internal sealed record RepairCatalog(string Format, string ToolVersion, string ToolUrl, IndexDownload[] Indexes);

internal static class UpdateService
{
    internal const string DefaultSource = "https://api.github.com/repos/ssice-a/EFF/releases/latest";
    private static readonly HttpClient Client = CreateClient();
    private static HttpClient CreateClient()
    {
        var client = new HttpClient { Timeout = TimeSpan.FromMinutes(15) };client.DefaultRequestHeaders.UserAgent.ParseAdd("EFF-Repair/0.2.0");return client;
    }
    internal static Uri Https(string value)
    {
        if (!Uri.TryCreate(value, UriKind.Absolute, out var uri) || uri.Scheme != Uri.UriSchemeHttps) throw new IOException("更新源或下载地址必须使用 HTTPS");return uri;
    }
    internal static async Task<RepairCatalog?> Check(string source, CancellationToken token)
    {
        using var timeout = CancellationTokenSource.CreateLinkedTokenSource(token);timeout.CancelAfter(TimeSpan.FromSeconds(15));
        using var response = await Client.GetAsync(Https(source), timeout.Token);
        if (response.StatusCode == HttpStatusCode.NotFound) return null;
        response.EnsureSuccessStatusCode();using var document = JsonDocument.Parse(await response.Content.ReadAsStringAsync(timeout.Token));
        var root = document.RootElement;
        if (root.TryGetProperty("assets", out var assets))
        {
            var asset = assets.EnumerateArray().FirstOrDefault(a => a.GetProperty("name").GetString() == "eff-index-catalog.json");
            if (asset.ValueKind == JsonValueKind.Undefined) return null;
            using var catalog = await Client.GetAsync(Https(asset.GetProperty("browser_download_url").GetString()!), timeout.Token);catalog.EnsureSuccessStatusCode();
            return Parse(await catalog.Content.ReadAsStringAsync(timeout.Token));
        }
        return Parse(root.GetRawText());
    }
    internal static RepairCatalog Parse(string json)
    {
        var result = JsonSerializer.Deserialize<RepairCatalog>(json) ?? throw new IOException("空更新目录");
        if (result.Format != "EFF_REPAIR_CATALOG/1" || result.Indexes is null) throw new IOException("不支持的更新目录结构");
        foreach (var index in result.Indexes)
        {
            Https(index.Url);
            if (index.Release.Format != "EFF_INDEX_RELEASE/1" || index.Release.Bytes <= 0 || index.Release.Sha256.Length != 64 || !index.Release.GameContent.StartsWith("EFF_BUNDLE_CONTENT/1:")) throw new IOException("索引发布信息不完整");
        }
        if (!string.IsNullOrEmpty(result.ToolUrl)) Https(result.ToolUrl);
        return result;
    }
    internal static async Task Download(IndexDownload index, string destination, IProgress<string>? progress, CancellationToken token)
    {
        var temp = destination + ".download";
        try
        {
            using var response = await Client.GetAsync(Https(index.Url), HttpCompletionOption.ResponseHeadersRead, token);response.EnsureSuccessStatusCode();
            using var input = await response.Content.ReadAsStreamAsync(token);using var output = File.Create(temp);
            var buffer = new byte[1024 * 1024];long bytes = 0;var reported = DateTime.MinValue;int n;
            while ((n = await input.ReadAsync(buffer, token)) > 0)
            {
                bytes += n;if (bytes > index.Release.Bytes) throw new IOException("下载超出发布文件大小");await output.WriteAsync(buffer.AsMemory(0, n), token);
                if (DateTime.UtcNow - reported > TimeSpan.FromMilliseconds(250)) { progress?.Report($"下载索引 {bytes / 1048576.0:F1} / {index.Release.Bytes / 1048576.0:F1} MB");reported = DateTime.UtcNow; }
            }
            output.Dispose();
            if (bytes != index.Release.Bytes || RepairService.Hash(temp) != index.Release.Sha256.ToUpperInvariant()) throw new IOException("索引下载大小或 SHA256 校验失败");
            File.Move(temp, destination, true);
        }
        finally { if (File.Exists(temp)) File.Delete(temp); }
    }
}

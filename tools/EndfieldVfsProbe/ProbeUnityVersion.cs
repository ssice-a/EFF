using System.Text.Json;
using System.Text.RegularExpressions;
using AnimeStudio;

/// <summary>Diagnostics use embedded metadata, with explicit or preserved evidence for stripped CABs.</summary>
internal static class ProbeUnityVersion
{
    public static string Override { get; private set; }

    public static string[] ReadOption(string[] args)
    {
        if (args.Length >= 2 && args[^2] == "--unity-version")
        {
            Override = Validate(args[^1]);
            return args[..^2];
        }
        return args;
    }

    public static Func<SerializedFile, string> FromManifest(JsonElement manifest)
    {
        var versions = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
        foreach (var row in manifest.GetProperty("serializedFiles").EnumerateArray())
        {
            if (!row.TryGetProperty("unityVersion", out var version) || string.IsNullOrWhiteSpace(version.GetString())) continue;
            var value = version.GetString();
            // Stripped metadata is not version evidence.
            if (value == "0.0.0") continue;
            versions.Add(row.GetProperty("cab").GetString(), Validate(value));
        }
        return file => versions.GetValueOrDefault(file.fileName);
    }

    private static string Validate(string value)
    {
        if (string.IsNullOrWhiteSpace(value) || !Regex.IsMatch(value, @"^\d+\.\d+\.\d+[abcfpx]\d+$"))
            throw new ArgumentException("An explicit Unity version such as 2021.3.34f5 is required for stripped metadata.");
        return value;
    }
}

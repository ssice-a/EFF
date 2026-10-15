using Microsoft.Win32;
using System.Diagnostics;

namespace EffRepair;

internal static class GameDiscovery
{
    internal static bool Valid(string root) => File.Exists(Path.Combine(root, "Endfield.exe")) && File.Exists(Path.Combine(root, "GameAssembly.dll")) && Directory.Exists(Path.Combine(root, "Endfield_Data", "StreamingAssets", "VFS"));
    internal static List<string> Find(string saved)
    {
        var candidates = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        void Add(string? path) { if (!string.IsNullOrWhiteSpace(path) && Valid(path)) candidates.Add(Path.GetFullPath(path)); }
        Add(saved);
        foreach (var process in Process.GetProcessesByName("Endfield")) using (process) { try { Add(Path.GetDirectoryName(process.MainModule?.FileName)); } catch { } }
        foreach (var drive in DriveInfo.GetDrives().Where(d => d.IsReady && d.DriveType == DriveType.Fixed))
        {
            Add(Path.Combine(drive.RootDirectory.FullName, "Hypergryph Launcher", "games", "Endfield Game"));
            Add(Path.Combine(drive.RootDirectory.FullName, "Games", "Endfield Game"));
        }
        foreach (var hive in new[] { Registry.CurrentUser, Registry.LocalMachine })
            foreach (var key in new[] { @"SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall", @"SOFTWARE\WOW6432Node\Microsoft\Windows\CurrentVersion\Uninstall" })
            {
                using var uninstall = hive.OpenSubKey(key);if (uninstall is null) continue;
                foreach (var child in uninstall.GetSubKeyNames())
                {
                    using var entry = uninstall.OpenSubKey(child);var name = entry?.GetValue("DisplayName") as string ?? "";
                    if (name.Contains("Endfield", StringComparison.OrdinalIgnoreCase) || name.Contains("终末地")) Add(entry?.GetValue("InstallLocation") as string);
                }
            }
        return candidates.OrderBy(p => p, StringComparer.OrdinalIgnoreCase).ToList();
    }
}

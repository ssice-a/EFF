using AnimeStudio.GUI;
using System.Diagnostics;
using System.Security.Cryptography;
using System.Text;
using System.Text.Json;

namespace EffRepair;

internal static class RepairService
{
    internal const string Receipt = "eff-repair.json";
    private static string StatePath(string mod)
    {
        var normalized = Path.GetFullPath(mod).TrimEnd('\\').Replace('\\', '/').ToLowerInvariant();
        return Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "EFF", "Repair", "receipts",
            Convert.ToHexString(SHA256.HashData(Encoding.UTF8.GetBytes(normalized))) + ".json");
    }
    private static bool LegacyReceipt(string file)
    {
        if (Path.GetFileName(file) != Receipt || !File.Exists(file)) return false;
        try { using var json = JsonDocument.Parse(File.ReadAllText(file));return json.RootElement.TryGetProperty("format", out var format) && format.GetString() == "EFF_REPAIR/1"; }
        catch (JsonException) { return false; }
    }
    internal static string Compiler => Path.Combine(AppContext.BaseDirectory, "native", "eff_resource_pack.exe");
    internal static string Hash(string file) { using var input = File.OpenRead(file);return Convert.ToHexString(SHA256.HashData(input)); }
    internal static IEnumerable<string> Files(string root)
    {
        if ((File.GetAttributes(root) & FileAttributes.ReparsePoint) != 0) throw new IOException("不能修复目录链接：" + root);
        foreach (var file in Directory.EnumerateFileSystemEntries(root))
        {
            var flags = File.GetAttributes(file);
            if ((flags & FileAttributes.ReparsePoint) != 0) throw new IOException("Mod 中存在文件或目录链接：" + file);
            if ((flags & FileAttributes.Directory) != 0) { foreach (var child in Files(file)) yield return child; }
            else yield return file;
        }
    }
    internal static Dictionary<string, string> Author(string root) => Files(root)
        .Where(p => Path.GetRelativePath(root, p) is not ("compiled.bin" or "source-inputs.bin") &&
                    !(Path.GetRelativePath(root, p) == Receipt && LegacyReceipt(p)))
        .ToDictionary(p => Path.GetRelativePath(root, p), Hash, StringComparer.OrdinalIgnoreCase);
    internal static string AuthorIdentity(string root)
    {
        using var hash = IncrementalHash.CreateHash(HashAlgorithmName.SHA256);
        foreach (var item in Author(root).OrderBy(p => p.Key, StringComparer.OrdinalIgnoreCase))
            hash.AppendData(Encoding.UTF8.GetBytes(item.Key.Replace('\\', '/') + "\n" + item.Value + "\n"));
        return Convert.ToHexString(hash.GetHashAndReset());
    }
    internal static List<string> Mods(string path)
    {
        path = Path.GetFullPath(path);
        if (!Directory.Exists(path)) return [];
        if (File.Exists(Path.Combine(path, "mod.ini"))) return [path];
        return Directory.EnumerateDirectories(path).Where(p => (File.GetAttributes(p) & FileAttributes.ReparsePoint) == 0 && File.Exists(Path.Combine(p, "mod.ini")))
            .OrderBy(p => p, StringComparer.OrdinalIgnoreCase).ToList();
    }
    internal static void RequireGameClosed(string game)
    {
        foreach (var process in Process.GetProcessesByName("Endfield"))
        {
            using (process)
            {
                string path;
                try { path = process.MainModule?.FileName ?? ""; } catch { throw new IOException("检测到运行中的游戏，请退出游戏后再修复 Mod。"); }
                if (string.Equals(Path.GetDirectoryName(path), Path.GetFullPath(game).TrimEnd('\\'), StringComparison.OrdinalIgnoreCase))
                    throw new IOException("请先退出游戏，再提交 Mod 修复。");
            }
        }
    }
    internal static async Task<string> Run(string[] arguments, CancellationToken token)
    {
        var start = new ProcessStartInfo(Compiler) { UseShellExecute = false, CreateNoWindow = true, RedirectStandardOutput = true, RedirectStandardError = true };
        foreach (var argument in arguments) start.ArgumentList.Add(argument);
        using var process = Process.Start(start) ?? throw new IOException("无法启动配套编译器");
        var stdout = process.StandardOutput.ReadToEndAsync();var stderr = process.StandardError.ReadToEndAsync();
        try { await process.WaitForExitAsync(token); }
        catch { if (!process.HasExited) process.Kill(true);await process.WaitForExitAsync();throw; }
        var result = await stdout + "\n" + await stderr;
        if (process.ExitCode != 0) throw new IOException("编译失败，原 Mod 保持。\n" + result);
        return result;
    }
    internal static async Task ValidateProtocol(string game, CancellationToken token)
    {
        using var produced = JsonDocument.Parse((await Run(["--protocol"], token)).Trim());
        var declaration = Path.Combine(game, "plugin", "package-protocol.json");
        if (!File.Exists(declaration)) throw new IOException("缺少已安装 DLL 的 package-protocol.json，请安装完整配套 EFF。");
        using var consumer = JsonDocument.Parse(File.ReadAllText(declaration));
        if (produced.RootElement.GetProperty("sourcePreparation").GetString() != "in-process/index-vfs+author-declarations") throw new IOException("修复编译器不支持当前索引查询");
        foreach (var key in new[] { "compiledMod" })
            if (produced.RootElement.GetProperty(key).GetString() != consumer.RootElement.GetProperty(key).GetString()) throw new IOException("编译器与已安装 EFF 不兼容：" + key);
        foreach (var key in new[] { "manifest", "author", "resource", "compiledInputView", "compiledRuntimeIndex" })
        {
            var structures = produced.RootElement.GetProperty("structures");
            if (structures.TryGetProperty(key, out var value) &&
                (!consumer.RootElement.GetProperty("structures").TryGetProperty(key, out var accepted) || value.GetString() != accepted.GetString()))
                throw new IOException("已安装 DLL 不支持修复输出：" + key);
        }
    }
    internal static async Task<string> Repair(string mod, string game, string index, string content,
        IProgress<string>? progress, CancellationToken token)
    {
        mod = Path.GetFullPath(mod).TrimEnd('\\');RequireGameClosed(game);await ValidateProtocol(game, token);
        if (!File.Exists(Path.Combine(mod, "mod.ini"))) throw new IOException("不是有效 Mod：" + mod);
        var author = AuthorIdentity(mod);var compiler = Hash(Compiler);var indexStamp = new FileInfo(index);
        var baseline = content + "/" + indexStamp.Length + "/" + indexStamp.LastWriteTimeUtc.Ticks;
        var receipt = StatePath(mod);
        if (File.Exists(receipt))
        {
            try
            {
                using var old = JsonDocument.Parse(File.ReadAllText(receipt));var r = old.RootElement;
                if (r.GetProperty("baseline").GetString() == baseline && r.GetProperty("author").GetString() == author && r.GetProperty("compiler").GetString() == compiler &&
                    r.GetProperty("compiled").GetString() == Hash(Path.Combine(mod, "compiled.bin")) && r.GetProperty("sources").GetString() == Hash(Path.Combine(mod, "source-inputs.bin")))
                    return Path.GetFileName(mod) + "：已适配，跳过";
            }
            catch (Exception error) when (error is IOException or JsonException or KeyNotFoundException) { }
        }
        var temporary = Path.Combine(Path.GetDirectoryName(mod)!, ".eff-repair-" + Guid.NewGuid().ToString("N"));
        var staged = Path.Combine(temporary, Path.GetFileName(mod));Directory.CreateDirectory(staged);
        try
        {
            foreach (var file in Files(mod))
            {
                if (Path.GetRelativePath(mod, file) == Receipt && LegacyReceipt(file)) continue;
                token.ThrowIfCancellationRequested();var target = Path.Combine(staged, Path.GetRelativePath(mod, file));Directory.CreateDirectory(Path.GetDirectoryName(target)!);File.Copy(file, target);
            }
            progress?.Report("正在修复 " + Path.GetFileName(mod));
            var previous = mod;
            // Keep the previous descriptor together with its compiled routes.
            // Preparation will replace only the staged descriptor. A legacy
            // development receipt must not enter either strict author reader.
            if (LegacyReceipt(Path.Combine(mod, Receipt)))
            {
                previous = Path.Combine(temporary, "previous", Path.GetFileName(mod));Directory.CreateDirectory(previous);
                foreach (var file in Files(staged))
                {
                    var target = Path.Combine(previous, Path.GetRelativePath(staged, file));Directory.CreateDirectory(Path.GetDirectoryName(target)!);File.Copy(file, target);
                }
            }
            var arguments = new List<string> { "--export-mod", staged, game, "--source-index", index, "--previous-mod", previous };
            var log = await Run(arguments.ToArray(), token);
            progress?.Report(log);
            if (!File.Exists(Path.Combine(staged, "compiled.bin")) || !File.Exists(Path.Combine(staged, "source-inputs.bin")) || author != AuthorIdentity(staged))
                throw new InvalidDataException("修复未生成完整结果或改动了作者文件，原 Mod 保持。");
            var record = JsonSerializer.Serialize(new { format = "EFF_REPAIR/1", baseline, author, compiler,
                compiled = Hash(Path.Combine(staged, "compiled.bin")), sources = Hash(Path.Combine(staged, "source-inputs.bin")), repairedUtc = DateTime.UtcNow });
            token.ThrowIfCancellationRequested();RequireGameClosed(game);
            var normalized = (mod + "/compiled.bin").Replace('\\', '/');normalized = new string(normalized.Select(c => c >= 'A' && c <= 'Z' ? (char)(c + 32) : c).ToArray());
            using var gate = new Mutex(false, @"Local\EFFModBlocks-" + Convert.ToHexString(SHA256.HashData(Encoding.UTF8.GetBytes(normalized))));
            bool held;try { held = gate.WaitOne(0); } catch (AbandonedMutexException) { held = true; }
            if (!held) throw new IOException("Mod 正在导出，请稍后重试。");
            try
            {
                if (AuthorIdentity(mod) != author) throw new IOException("修复期间 Mod 被修改，原目录保持，请重试。");
                var backup = Path.Combine(Path.GetDirectoryName(mod)!, ".eff-repair-backups", Path.GetFileName(mod), DateTime.UtcNow.ToString("yyyyMMdd-HHmmss") + "-" + Guid.NewGuid().ToString("N"));
                Directory.CreateDirectory(Path.GetDirectoryName(backup)!);Directory.Move(mod, backup);
                try { Directory.Move(staged, mod); } catch { Directory.Move(backup, mod);throw; }
                try
                {
                    Directory.CreateDirectory(Path.GetDirectoryName(receipt)!);var temp = receipt + "." + Guid.NewGuid().ToString("N") + ".tmp";
                    File.WriteAllText(temp, record);File.Move(temp, receipt, true);
                }
                catch (IOException error) { progress?.Report("Mod 已修复，但工具状态记录未保存：" + error.Message); }
                return Path.GetFileName(mod) + "：修复完成；备份 " + backup;
            }
            finally { gate.ReleaseMutex(); }
        }
        finally { if (Directory.Exists(temporary)) Directory.Delete(temporary, true); }
    }
}

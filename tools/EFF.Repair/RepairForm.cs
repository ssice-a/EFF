using AnimeStudio.GUI;
using System.Diagnostics;
using System.Text.Json;

namespace EffRepair;

internal sealed class RepairForm : Form
{
    private readonly TextBox game = new(), mods = new(), source = new() { Text = UpdateService.DefaultSource };
    private readonly CheckedListBox list = new() { Dock = DockStyle.Fill, CheckOnClick = true };
    private readonly TextBox log = new() { Dock = DockStyle.Fill, Multiline = true, ReadOnly = true, ScrollBars = ScrollBars.Vertical };
    private readonly Label status = new() { AutoSize = true, Text = "选择游戏与 Mod 目录，检查更新后修复。" };
    private readonly List<Control> actions = [];
    private CancellationTokenSource? cancellation;
    private readonly string data = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "EFF", "Repair");
    private RepairCatalog? catalog;
    private EndfieldVfsArchive? archive;
    private string[] paths = [];
    private string Workspace => Path.Combine(data, "workspace");
    internal RepairForm(bool autoCheck = true)
    {
        Text = "EFF Mod 修复工具 0.1";Width = 920;Height = 660;MinimumSize = new Size(920, 560);StartPosition = FormStartPosition.CenterScreen;
        Font = new Font("Microsoft YaHei UI", 9);BackColor = Color.FromArgb(245, 246, 249);AllowDrop = true;
        var layout = new TableLayoutPanel { Dock = DockStyle.Fill, Padding = new Padding(18), ColumnCount = 1, RowCount = 9 };
        layout.RowStyles.Add(new RowStyle(SizeType.Absolute, 44));layout.RowStyles.Add(new RowStyle(SizeType.Absolute, 38));layout.RowStyles.Add(new RowStyle(SizeType.Absolute, 38));
        layout.RowStyles.Add(new RowStyle(SizeType.Absolute, 38));layout.RowStyles.Add(new RowStyle(SizeType.Absolute, 42));layout.RowStyles.Add(new RowStyle(SizeType.Absolute, 32));
        layout.RowStyles.Add(new RowStyle(SizeType.Percent, 55));layout.RowStyles.Add(new RowStyle(SizeType.Absolute, 28));layout.RowStyles.Add(new RowStyle(SizeType.Percent, 45));
        layout.Controls.Add(new Label { Text = "EFF · 游戏外 Mod 修复", Font = new Font(Font.FontFamily, 17, FontStyle.Bold), AutoSize = true });
        layout.Controls.Add(PathRow("游戏目录", game, async () => { Browse(game);await Detect(); }));
        layout.Controls.Add(PathRow("Mod 目录", mods, () => { Browse(mods);Scan();return Task.CompletedTask; }));
        layout.Controls.Add(PathRow("更新源", source, () => { source.Text = UpdateService.DefaultSource;return Task.CompletedTask; }, "恢复默认"));
        var buttons = new FlowLayoutPanel { Dock = DockStyle.Fill };
        Button Add(string text, Func<Task> action) { var b = new Button { Text = text, AutoSize = true, Height = 30 };b.Click += async (_, _) => await Work(action);buttons.Controls.Add(b);actions.Add(b);return b; }
        Add("自动识别", Detect);Add("检查更新", Check);Add("下载对应索引", Download);Add("导入索引文件", Import);Add("修复已勾选 Mod", Repair);
        var cancel = new Button { Text = "取消", AutoSize = true };cancel.Click += (_, _) => cancellation?.Cancel();buttons.Controls.Add(cancel);
        layout.Controls.Add(buttons);layout.Controls.Add(status);layout.Controls.Add(list);layout.Controls.Add(new Label { Text = "任务记录（原 Mod 备份保存在目录旁的 .eff-repair-backups）", AutoSize = true });layout.Controls.Add(log);Controls.Add(layout);
        actions.AddRange([game, mods, source, list]);
        game.TextChanged += (_, _) => archive = null;
        DragEnter += (_, e) => { if (e.Data?.GetDataPresent(DataFormats.FileDrop) == true) e.Effect = DragDropEffects.Copy; };
        DragDrop += (_, e) => { if (cancellation is not null) return;var files = (string[]?)e.Data?.GetData(DataFormats.FileDrop);if (files?.Length == 1 && Directory.Exists(files[0])) { mods.Text = files[0];Scan(); } };
        FormClosing += (_, e) => { if (cancellation is not null) { cancellation.Cancel();e.Cancel = true;status.Text = "正在停止任务，完成后可关闭。"; } };
        LoadSettings();
        if (autoCheck) Shown += async (_, _) => await Work(async () => { await Detect();await Check(); });
    }
    private Control PathRow(string title, TextBox text, Func<Task> action, string button = "选择…")
    {
        var row = new TableLayoutPanel { Dock = DockStyle.Fill, ColumnCount = 3 };row.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 108));row.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));row.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 110));
        row.Controls.Add(new Label { Text = title, Dock = DockStyle.Fill, TextAlign = ContentAlignment.MiddleLeft, AutoEllipsis = true });text.Dock = DockStyle.Fill;row.Controls.Add(text);
        var choose = new Button { Text = button, Dock = DockStyle.Fill };choose.Click += async (_, _) => await Work(action);row.Controls.Add(choose);actions.Add(choose);return row;
    }
    private void Browse(TextBox target) { using var dialog = new FolderBrowserDialog { InitialDirectory = Directory.Exists(target.Text) ? target.Text : "" };if (dialog.ShowDialog(this) == DialogResult.OK) target.Text = dialog.SelectedPath; }
    private void Report(string value) { status.Text = value.Split('\n')[0];log.AppendText(DateTime.Now.ToString("HH:mm:ss ") + value + Environment.NewLine);Directory.CreateDirectory(data);File.AppendAllText(Path.Combine(data, "repair.log"), DateTime.Now.ToString("O ") + value + Environment.NewLine); }
    private async Task Work(Func<Task> action)
    {
        if (cancellation is not null) return;cancellation = new();foreach (var control in actions) control.Enabled = false;
        try { await action();SaveSettings(); }
        catch (OperationCanceledException) { Report("任务已取消，尚未提交的 Mod 保持原样。"); }
        catch (Exception error) { Report(error.Message); }
        finally { cancellation.Dispose();cancellation = null;foreach (var control in actions) control.Enabled = true; }
    }
    private async Task Detect()
    {
        var saved = game.Text;var found = await Task.Run(() => GameDiscovery.Find(saved));
        if (Directory.Exists(saved))
        {
            try { var resolved = EndfieldVfsArchive.ResolveSelection(saved);if (GameDiscovery.Valid(resolved)) game.Text = resolved; } catch { }
        }
        if (!GameDiscovery.Valid(game.Text) && found.Count == 1) game.Text = found[0];
        if (!GameDiscovery.Valid(game.Text)) { Report(found.Count > 1 ? "发现多个安装，请选择具体游戏目录：\n" + string.Join("\n", found) : "未找到游戏，请手动选择安装目录。");return; }
        var selected = game.Text;archive = await Task.Run(() => EndfieldVfsArchive.Open(selected));
        if (string.IsNullOrWhiteSpace(mods.Text)) mods.Text = Path.Combine(game.Text, "plugin", "mods");Scan();Report("已识别游戏：" + game.Text);
    }
    private void Scan() { paths = RepairService.Mods(mods.Text).ToArray();list.Items.Clear();foreach (var path in paths) list.Items.Add(Path.GetFileName(path), true); }
    private async Task Check()
    {
        Report("正在检查工具及索引更新…");
        try { catalog = await UpdateService.Check(source.Text, cancellation!.Token); }
        catch (OperationCanceledException) when (!cancellation!.IsCancellationRequested) { catalog = null;throw new IOException("更新检查超时，无法确认是否有新版本；请稍后重试。"); }
        catch (HttpRequestException error) { catalog = null;throw new IOException("无法检查更新：" + error.Message, error); }
        if (catalog is null) { Report("发布源尚未提供修复索引目录，当前无法取得在线索引；可导入维护者提供的完整索引文件。");return; }
        if (!string.IsNullOrEmpty(catalog.ToolVersion) && Version.TryParse(catalog.ToolVersion.TrimStart('v'), out var version) && version > new Version(0, 1, 0))
        {
            Report("修复工具有新版本：" + catalog.ToolVersion);
            if (!string.IsNullOrEmpty(catalog.ToolUrl) && MessageBox.Show(this, "修复工具有新版本，打开下载页面？", "工具更新", MessageBoxButtons.YesNo) == DialogResult.Yes)
                Process.Start(new ProcessStartInfo(UpdateService.Https(catalog.ToolUrl).AbsoluteUri) { UseShellExecute = true });
        }
        if (archive is null) { Report("已取得更新目录，请先选择游戏以匹配索引。");return; }
        var content = EndfieldPortableIndex.ContentIdentity(archive);var matches = catalog.Indexes.Where(i => i.Release.GameContent == content).ToArray();
        Report(matches.Length == 0 ? "游戏资源对应的完整索引尚未发布，请等待维护者更新。" : "对应索引可用：" + matches.Last().Release.Version + $"（{matches.Last().Release.Bytes / 1048576.0:F1} MB）");
    }
    private async Task Download()
    {
        if (archive is null) await Detect();if (archive is null) throw new IOException("请先选择有效游戏目录");if (catalog is null) await Check();
        var content = EndfieldPortableIndex.ContentIdentity(archive);var item = catalog?.Indexes.LastOrDefault(i => i.Release.GameContent == content) ?? throw new IOException("尚无匹配当前游戏资源的已发布索引");
        Directory.CreateDirectory(data);var package = Path.Combine(data, "current.effindex.gz");
        await UpdateService.Download(item, package, new Progress<string>(Report), cancellation!.Token);await Bind(package);
    }
    private async Task Import() { using var dialog = new OpenFileDialog { Filter = "完整 EFF 索引|*.effindex.gz|所有文件|*.*" };if (dialog.ShowDialog(this) == DialogResult.OK) await Bind(dialog.FileName); }
    private async Task Bind(string package)
    {
        if (archive is null) await Detect();if (archive is null) throw new IOException("请先选择游戏");Report("正在校验并导入完整索引…");
        await Task.Run(() => EndfieldPortableIndex.Import(archive, Workspace, package, cancellation!.Token));Report("索引已就绪；勾选 Mod 后点击修复。");
    }
    private async Task Repair()
    {
        if (!GameDiscovery.Valid(game.Text)) throw new IOException("请选择有效游戏目录");
        var selectedGame = game.Text;archive = await Task.Run(() => EndfieldVfsArchive.Open(selectedGame));
        if (!EndfieldVfsAssetIndexBuilder.TryGetCurrentIndex(archive, Workspace, out var index)) throw new IOException("请先下载或导入匹配当前游戏的完整索引");
        var selected = list.CheckedIndices.Cast<int>().Select(i => paths[i]).ToArray();if (selected.Length == 0) throw new IOException("请先勾选需要修复的 Mod");
        var progress = new Progress<string>(Report);var content = EndfieldPortableIndex.ContentIdentity(archive);int completed = 0, failed = 0;
        foreach (var mod in selected)
        {
            cancellation!.Token.ThrowIfCancellationRequested();
            try { Report(await Task.Run(() => RepairService.Repair(mod, selectedGame, index, content, progress, cancellation.Token)));completed++; }
            catch (Exception error) when (error is not OperationCanceledException) { failed++;Report(Path.GetFileName(mod) + "：" + error.Message); }
        }
        Report($"完成：{completed} 个成功或无需更新，{failed} 个失败。失败项目保留原包。");
    }
    private void LoadSettings()
    {
        try { using var json = JsonDocument.Parse(File.ReadAllText(Path.Combine(data, "settings.json")));game.Text = json.RootElement.GetProperty("game").GetString();mods.Text = json.RootElement.GetProperty("mods").GetString();source.Text = json.RootElement.GetProperty("source").GetString(); } catch (Exception error) when (error is IOException or JsonException or KeyNotFoundException) { }
    }
    private void SaveSettings() { Directory.CreateDirectory(data);File.WriteAllText(Path.Combine(data, "settings.json"), JsonSerializer.Serialize(new { game = game.Text, mods = mods.Text, source = source.Text })); }
}

using AnimeStudio.GUI;
using System.Text.Json;

namespace EffRepair;

internal static class Program
{
    internal static readonly Version ToolVersion = typeof(Program).Assembly.GetName().Version ?? new Version(0, 0, 0);
    [STAThread]
    static int Main(string[] args)
    {
        if (args.Length > 0)
        {
            try
            {
                if (args.Length == 3 && args[0] == "--check-updates")
                {
                    var catalog = UpdateService.Check(args[1], CancellationToken.None).GetAwaiter().GetResult();
                    File.WriteAllText(args[2], JsonSerializer.Serialize(new { status = catalog == null ? "notPublished" : "available", catalog }, new JsonSerializerOptions { WriteIndented = true }));return 0;
                }
                if (args.Length == 4 && args[0] == "--make-catalog")
                {
                    var release = JsonSerializer.Deserialize<EndfieldIndexRelease>(File.ReadAllText(args[1]))!;
                    var catalog = new RepairCatalog("EFF_REPAIR_CATALOG/1", ToolVersion.ToString(3), "https://github.com/ssice-a/EFF/releases", [new(release, UpdateService.Https(args[2]).AbsoluteUri)]);
                    var json = JsonSerializer.Serialize(catalog, new JsonSerializerOptions { WriteIndented = true });UpdateService.Parse(json);File.WriteAllText(args[3], json);return 0;
                }
                if (args.Length == 5 && args[0] == "--repair")
                {
                    var archive = EndfieldVfsArchive.Open(args[2]);
                    if (!EndfieldVfsAssetIndexBuilder.TryGetOrRebindCurrentIndex(archive, args[3], out var index)) throw new IOException("工作区索引无法按当前逻辑 Bundle 内容复用，请导入匹配当前内容的完整索引。");
                    var result = RepairService.Repair(args[1], args[2], index, EndfieldPortableIndex.ContentIdentity(archive), null, CancellationToken.None).GetAwaiter().GetResult();
                    File.WriteAllText(args[4], result);return 0;
                }
                if (args.Length == 2 && args[0] == "--smoke-ui")
                {
                    ApplicationConfiguration.Initialize();using var form = new RepairForm(false);form.Show();Application.DoEvents();
                    using var bitmap = new System.Drawing.Bitmap(form.Width, form.Height);form.DrawToBitmap(bitmap, new System.Drawing.Rectangle(0, 0, form.Width, form.Height));bitmap.Save(args[1]);return 0;
                }
                throw new ArgumentException("Usage: --repair MOD GAME WORKSPACE RESULT_FILE");
            }
            catch (Exception error) { Console.Error.WriteLine(error);return 1; }
        }
        ApplicationConfiguration.Initialize();Application.Run(new RepairForm());return 0;
    }
}

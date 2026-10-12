// Read-only extraction of selected SkeletalMorph resources for Blender baking.
using System;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Runtime.CompilerServices;
using System.Runtime.Loader;
using System.Text.Json;
using AnimeStudio;

internal static class FacialResources
{
    static int Main(string[] args)
    {
        if (args.Length != 4) { Console.Error.WriteLine("dependencies VFS selected-index output-directory"); return 2; }
        AssemblyLoadContext.Default.Resolving += (_, n) => {
            var path = Path.Combine(args[0], n.Name + ".dll");
            return File.Exists(path) ? AssemblyLoadContext.Default.LoadFromAssemblyPath(path) : null;
        };
        return Export(args);
    }

    [MethodImpl(MethodImplOptions.NoInlining)]
    static int Export(string[] args)
    {
        Directory.CreateDirectory(args[3]);
        var assembly = AssemblyLoadContext.Default.LoadFromAssemblyPath(Path.Combine(args[0], "EndfieldVfsProbe.dll"));
        var archiveType = assembly.GetType("AnimeStudio.GUI.EndfieldVfsArchive", true);
        var archive = archiveType.GetMethod("Open", BindingFlags.Public | BindingFlags.Static).Invoke(null, new object[] { args[1] });
        var extract = archiveType.GetMethod("ExtractToCache", BindingFlags.Public | BindingFlags.Instance);
        using var index = JsonDocument.Parse(File.ReadAllText(args[2]));
        var files = index.RootElement.EnumerateArray().Select(x => x.GetProperty("source").GetString()).Distinct()
            .Select(source => (string)extract.Invoke(archive, new object[] { source, args[3] })).ToArray();
        TypeFlags.SetTypes(new());
        foreach (var type in new[] { ClassIDType.MonoBehaviour, ClassIDType.MonoScript, ClassIDType.AssetBundle })
            TypeFlags.SetType(type, true, false);
        Logger.Default = new ConsoleLogger(); Logger.Flags = LoggerEvent.Error;
        var manager = new AssetsManager { Game = GameManager.GetGameByType(GameType.ArknightsEndfield),
            SpecifyUnityVersion = "2022.3.0f1", ResolveDependencies = false };
        manager.LoadFiles(files);
        var wanted = index.RootElement.EnumerateArray().Select(x => x.GetProperty("name").GetString()).ToHashSet();
        var count = 0;
        foreach (var value in manager.assetsFileList.SelectMany(f => f.Objects).OfType<MonoBehaviour>())
        {
            if (!wanted.Contains(value.m_Name)) continue;
            if (value.serializedType.m_Type == null) throw new Exception("Missing source type tree: " + value.m_Name);
            var stem = Path.Combine(args[3], "raw-" + value.m_Name);
            File.WriteAllBytes(stem + ".bin", value.GetRawData());
            File.WriteAllText(stem + ".types.json", JsonSerializer.Serialize(new {
                nodes = Nodes(value.serializedType),
                references = value.assetsFile.m_RefTypes?.Select(t => new {
                    type = t.m_KlassName, ns = t.m_NameSpace, asm = t.m_AsmName, nodes = Nodes(t) }) }));
            count++;
        }
        Console.WriteLine($"Selected resources={wanted.Count}, extracted bundles={files.Length}, decoded objects={count}");
        return count == wanted.Count ? 0 : 1;
    }

    static object Nodes(SerializedType type) => type.m_Type?.m_Nodes.Select(n => new {
        type = n.m_Type, name = n.m_Name, level = n.m_Level, flags = n.m_MetaFlag, size = n.m_ByteSize });
}

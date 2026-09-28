// Real AB -> production Mesh parser -> production EIEM mesh writer.
// Arguments: LIBRARY_DIRECTORY NEW_OUTPUT_DIRECTORY BUNDLE [BUNDLE...]
// No source bundles, existing packages, game files or Blender scenes are edited.
using System.Buffers.Binary;
using System.Reflection;
using System.Runtime.Loader;
using System.Runtime.InteropServices;
using System.Text.Json;
using AnimeStudio;
using VertexFormat = AnimeStudio.MeshHelper.VertexFormat;

internal static class Program
{
    [DllImport("kernel32.dll", CharSet = CharSet.Unicode)]
    private static extern bool SetDllDirectory(string directory);

    private static void Main(string[] args)
    {
        if (args.Length < 3)
            throw new ArgumentException("Expected LIBRARY_DIRECTORY NEW_OUTPUT_DIRECTORY BUNDLE...");
        var library = Path.GetFullPath(args[0]);
        AssemblyLoadContext.Default.Resolving += (_, name) =>
        {
            var path = Path.Combine(library, name.Name + ".dll");
            return File.Exists(path) ? AssemblyLoadContext.Default.LoadFromAssemblyPath(path) : null;
        };
        if (!SetDllDirectory(library)) throw new InvalidOperationException("Native library directory");
        Run(library, Path.GetFullPath(args[1]), args.Skip(2).ToArray());
    }

    private static void Require(bool condition, string message)
    {
        if (!condition) throw new InvalidDataException(message);
    }

    private static void Run(string library, string output, string[] bundles)
    {
        Require(!Directory.Exists(output) && !File.Exists(output), "Output must not already exist");
        Require(bundles.All(File.Exists), "A source bundle is missing");
        TypeFlags.SetTypes(new());
        TypeFlags.SetType(ClassIDType.Mesh, true, false);
        Logger.Default = new ConsoleLogger();
        Logger.Flags = LoggerEvent.Error;
        var manager = new AssetsManager
        {
            Game = GameManager.GetGameByType(GameType.ArknightsEndfield),
            SpecifyUnityVersion = "2021.3.34f5",
            ResolveDependencies = false,
        };
        manager.LoadFiles(bundles, false);
        var meshes = manager.assetsFileList.SelectMany(file => file.Objects.OfType<Mesh>()).ToArray();
        Require(meshes.Length > 0, "No source Mesh objects parsed");
        var packedMeshes = 0;
        var packedVertices = 0;
        foreach (var mesh in meshes)
        {
            var data = (VertexData)typeof(Mesh).GetField("m_VertexData",
                BindingFlags.Instance | BindingFlags.Public | BindingFlags.NonPublic)!.GetValue(mesh)!;
            var channel = data.m_Channels[1];
            if (channel.dimension != 1) continue;
            packedMeshes++;
            packedVertices += mesh.m_VertexCount;
            Require(mesh.m_Tangents?.Length == mesh.m_VertexCount * 4,
                $"{mesh.Name}: source has packed N/T but decoded tangent components = {mesh.m_Tangents?.Length ?? 0}, expected {mesh.m_VertexCount * 4}");
            var stream = data.m_Streams[channel.stream];
            for (var vertex = 0; vertex < mesh.m_VertexCount; vertex++)
            {
                var offset = checked((int)stream.offset + channel.offset + (int)stream.stride * vertex);
                var word = BinaryPrimitives.ReadUInt32LittleEndian(data.m_DataSize.AsSpan(offset, 4));
                Require((word & 0x40000000u) != 0, "Unverified unpacked scalar Normal");
                double nt = 0, tt = 0;
                for (var axis = 0; axis < 3; axis++)
                {
                    var tangent = mesh.m_Tangents[4 * vertex + axis];
                    Require(float.IsFinite(tangent), $"{mesh.Name}: non-finite tangent");
                    nt += mesh.m_Normals[3 * vertex + axis] * tangent;
                    tt += tangent * tangent;
                }
                Require(Math.Abs(nt) < 1e-5 && Math.Abs(tt - 1) < 1e-5,
                    $"{mesh.Name}: invalid tangent frame at {vertex}");
                Require(mesh.m_Tangents[4 * vertex + 3] == ((word >> 31) == 0 ? -1f : 1f),
                    $"{mesh.Name}: handedness lost at {vertex}");
            }
        }
        Require(packedMeshes > 0, "Test needs at least one packed source Mesh");
        CheckDecoderCases();

        // Exercise the real package writer, not a test reimplementation of EFFMESH.
        var gui = AssemblyLoadContext.Default.LoadFromAssemblyPath(Path.Combine(library, "AnimeStudio.GUI.dll"));
        var writerType = gui.GetType("AnimeStudio.GUI.EiemPackageWriter", true)!;
        var writer = Activator.CreateInstance(writerType,
            BindingFlags.Instance | BindingFlags.Public | BindingFlags.NonPublic,
            null, new object[] { null!, null! }, null)!;
        var write = writerType.GetMethod("WriteMesh", BindingFlags.Instance | BindingFlags.NonPublic)!;
        Directory.CreateDirectory(output);
        var manifest = new List<object>();
        for (var index = 0; index < meshes.Length; index++)
        {
            var mesh = meshes[index];
            var path = Path.Combine(output, $"{index}.mesh");
            write.Invoke(writer, new object[] { path, mesh, "", Array.Empty<string>() });
            manifest.Add(new
            {
                file = Path.GetFileName(path), name = mesh.Name, count = mesh.m_VertexCount,
                normals = mesh.m_Normals, tangents = mesh.m_Tangents,
                uvs = Enumerable.Range(0, 8).Select(mesh.GetUV).ToArray(),
                colors = mesh.m_Colors,
            });
        }
        File.WriteAllText(Path.Combine(output, "source.json"), JsonSerializer.Serialize(manifest));
        Console.WriteLine($"EIEM_SOURCE_TANGENTS_OK meshes={meshes.Length} packed_meshes={packedMeshes} packed_vertices={packedVertices}");
    }

    private static void CheckDecoderCases()
    {
        var decode = typeof(MeshHelper).GetMethod("DecompressEndfieldFrame")!;
        Require(decode != null, "Packed-frame decoder missing");
        var a = 1.0 / Math.Sqrt(2);
        var d = Math.Sqrt(2 * 260101.0);
        // N=+Z. Signed angle changes tangent orientation independently of N.
        var cases = new (int angle, double x, double y)[]
        {
            (0, -a, a), (511, a, -a), (-511, a, -a),
            (256, -509 / d, -511 / d), (-256, 511 / d, 509 / d),
        };
        foreach (var (angle, x, y) in cases)
        foreach (uint sign in new uint[] { 0, 1 })
        {
            uint word = 0x40000000u | ((uint)angle & 1023u) << 20 | sign << 31;
            var bytes = new byte[4];
            BinaryPrimitives.WriteUInt32LittleEndian(bytes, word);
            object[] args = { bytes, VertexFormat.Float, null!, null! };
            decode!.Invoke(null, args);
            var n = (float[])args[2]; var t = (float[])args[3];
            Require(n.SequenceEqual(new float[] { 0, 0, 1 }), "Golden normal");
            Require(Math.Abs(t[0] - x) < 1e-6 && Math.Abs(t[1] - y) < 1e-6 && t[2] == 0,
                $"Golden tangent angle {angle}");
            Require(t[3] == (sign == 0 ? -1f : 1f), "Golden handedness");
        }
        foreach (var (bytes, format) in new[]
        {
            (new byte[3], VertexFormat.Float),
            (new byte[4], VertexFormat.Float), // packed flag absent
            (new byte[4], VertexFormat.Float16),
        })
        {
            bool rejected = false;
            try { decode!.Invoke(null, new object[] { bytes, format, null!, null! }); }
            catch (TargetInvocationException error) when (error.InnerException is InvalidDataException)
            { rejected = true; }
            Require(rejected, "Malformed packed frame must not silently become a normal-only mesh");
        }
        Console.WriteLine("EIEM_FRAME_GOLDEN_OK cases=10 invalid=3");
    }
}

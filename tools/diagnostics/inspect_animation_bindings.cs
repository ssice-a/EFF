// Read-only AnimationClip evidence probe. Loads existing extracted bundles and
// prints bindings/ACL scalar storage; it does not export or modify resources.
using System;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Runtime.CompilerServices;
using System.Runtime.Loader;
using System.Text.Json;
using System.Text;
using System.Collections.Generic;
using Quaternion = System.Numerics.Quaternion;
using AnimeStudio;
using Ruri.ACL;

internal static class AnimationBindingsProbe
{
    private static int Main(string[] args)
    {
        if (args.Length < 3)
        {
            Console.Error.WriteLine("usage: probe dependency-directory physics-components.json bundle...");
            return 2;
        }
        AssemblyLoadContext.Default.Resolving += (_, name) =>
        {
            var path = Path.Combine(args[0], name.Name + ".dll");
            return File.Exists(path) ? AssemblyLoadContext.Default.LoadFromAssemblyPath(path) : null;
        };
        return Inspect(args[1], args.Skip(2).ToArray());
    }

    [MethodImpl(MethodImplOptions.NoInlining)]
    private static int Inspect(string physicsPath, string[] paths)
    {
        TypeFlags.SetTypes(new());
        TypeFlags.SetType(ClassIDType.AnimationClip, true, false);
        TypeFlags.SetType(ClassIDType.AssetBundle, true, false);
        TypeFlags.SetType(ClassIDType.MonoScript, true, false);
        Logger.Default = new ConsoleLogger();
        Logger.Flags = LoggerEvent.Error;
        var manager = new AssetsManager
        {
            Game = GameManager.GetGameByType(GameType.ArknightsEndfield),
            SpecifyUnityVersion = "2022.3.0f1",
            ResolveDependencies = false
        };
        manager.LoadFiles(paths);
        using var physics = JsonDocument.Parse(File.ReadAllText(physicsPath));
        var transforms = physics.RootElement.GetProperty("transforms").EnumerateArray()
            .ToDictionary(t => t.GetProperty("identity").GetString(),
                          t => t.GetProperty("path").GetString());
        var targets = new Dictionary<uint, (string group, string path)>();
        foreach (var component in physics.RootElement.GetProperty("components").EnumerateArray())
        {
            var ownerPath = component.GetProperty("ownerPath").GetString();
            if (component.GetProperty("script").GetProperty("type").GetString() != "BeyondBoneCloth") continue;
            foreach (var reference in component.GetProperty("references").EnumerateArray())
            {
                if (!reference.GetProperty("field").GetString().Contains("proxyMesh.transformData.transformArray")) continue;
                if (!transforms.TryGetValue(reference.GetProperty("identity").GetString(), out var path)) continue;
                path = path.Substring(path.IndexOf('/') + 1);
                targets[Crc32(path)] = (ownerPath.Split('/').Last(), path);
            }
        }
        foreach (var clip in manager.assetsFileList.SelectMany(f => f.Objects).OfType<AnimationClip>())
        {
            var bindings = clip.m_ClipBindingConstant?.genericBindings;
            var acl = clip.m_AclCompressedBuffer;
            Console.WriteLine(JsonSerializer.Serialize(new
            {
                clip.m_Name,
                clip.m_PathID,
                bindingCount = bindings?.Count,
                bindingTypes = bindings?.GroupBy(b => new { b.typeID, b.customType })
                    .Select(g => new { type = g.Key.typeID.ToString(), g.Key.customType, count = g.Count() }),
                bindings = bindings?.Select((b, index) => new
                {
                    index, b.path, b.attribute,
                    type = b.typeID.ToString(),
                    b.customType, b.isPPtrCurve, b.isIntCurve,
                    scriptFileId = b.script?.m_FileID,
                    scriptPathId = b.script?.m_PathID
                }).Where(b => b.type != "Transform" && !(b.type == "Animator" && b.customType == 8)),
                physicalPoseSamples = SamplePhysicalPose(clip, targets),
                physicsWeightCurves = SamplePhysicsWeights(clip),
                scalar = acl == null ? null : new
                {
                    acl.Version, acl.FloatCurveCount, acl.OutputTrackCount,
                    defaultCount = acl.m_DefaultIndexs?.Length,
                    constantIndexCount = acl.m_ConstantIndexs?.Length,
                    constantValueCount = acl.m_ConstantValues?.Length,
                    compressedFloatBytes = acl.FloatBufferData?.Length
                },
                floatCurves = clip.m_FloatCurves?.Select(c => new
                {
                    c.path, c.attribute, c.classID,
                    keys = c.curve.m_Curve.Select(k => new { k.time, k.value })
                })
            }));
        }
        return 0;
    }

    private static object SamplePhysicsWeights(AnimationClip clip)
    {
        var bindings = clip.m_ClipBindingConstant?.genericBindings;
        var acl = clip.m_AclCompressedBuffer;
        if (bindings == null || acl == null) return new { error = "Missing ACL bindings" };
        var keys = new[] { "MagicaClothWeight", "MagicaClothWeightAdd", "MagicaClothWeightForce",
                           "blendWeightProperty", "clothSimulateWeightProperty", "animationPoseRatioProperty" };
        var names = keys.ToDictionary(Crc32, key => key);
        var scalars = bindings.Select((b, i) => new { binding = b, index = i })
            .Where(x => x.binding.typeID != ClassIDType.Transform).ToArray();
        var wanted = scalars.Select((x, i) => new { x.binding, x.index, scalarIndex = i })
            .Where(x => names.ContainsKey(x.binding.attribute)).ToArray();
        int constantOffset = 0;
        var constants = new List<object>();
        foreach (var index in acl.m_ConstantIndexs ?? Array.Empty<ushort>())
        {
            if (index >= bindings.Count) throw new Exception("Constant binding index out of bounds");
            var binding = bindings[index];
            int width = binding.typeID != ClassIDType.Transform ? 1 : binding.attribute switch
            { 1 or 3 or 4 => 3, 2 => 4, _ => 1 };
            if (constantOffset + width > acl.m_ConstantValues.Length) throw new Exception("Constant value range out of bounds");
            if (names.TryGetValue(binding.attribute, out var name))
                constants.Add(new { name, index, value = acl.m_ConstantValues[constantOffset] });
            constantOffset += width;
        }
        if (constantOffset != (acl.m_ConstantValues?.Length ?? 0)) throw new Exception("Constant binding/value arity mismatch");
        return new
        {
            hashes = names.Select(x => new { name = x.Value, hash = x.Key }),
            scalarBindingCount = scalars.Length,
            matched = wanted.Select(x => new { name = names[x.binding.attribute], x.index, x.scalarIndex,
                                               type = x.binding.typeID.ToString(), x.binding.customType }),
            scalarAttributes = scalars.Select(x => x.binding.attribute),
            scalarHeader = acl.FloatBufferData?.Length >= 24 ? new {
                tracks = BitConverter.ToUInt32(acl.FloatBufferData, 16),
                samples = BitConverter.ToUInt32(acl.FloatBufferData, 20) } : null,
            constantBindingCount = acl.m_ConstantIndexs?.Length,
            constantWeightValues = constants,
            samples = SampleFloatBuffer(acl.FloatBufferData, wanted.Select(x => x.scalarIndex).ToArray())
        };
    }

    private static object SampleFloatBuffer(byte[] buffer, int[] indices)
    {
        if (buffer == null || buffer.Length == 0 || indices.Length == 0) return null;
        var type = typeof(AclDecompressor).Assembly.GetType("Ruri.ACL.AclCompressedTracks", true);
        var decoder = Activator.CreateInstance(type, new object[] { buffer });
        if (!(bool)type.GetProperty("IsValid").GetValue(decoder)) return new { error = "Invalid scalar ACL" };
        var tracks = (uint)type.GetProperty("NumTracks").GetValue(decoder);
        var count = (uint)type.GetProperty("NumSamples").GetValue(decoder);
        var rate = (float)type.GetProperty("SampleRate").GetValue(decoder);
        var method = type.GetMethod("DecompressFloats");
        var values = new float[tracks];
        var samples = new List<object>();
        for (uint frame = 0; frame < count; ++frame)
        {
            var time = frame / rate;
            if (!(bool)method.Invoke(decoder, new object[] { time, values })) throw new Exception("ACL scalar sampling failed");
            samples.Add(new { time, values = indices.Select(i => values[i]).ToArray() });
        }
        return new { tracks, count, rate, samples };
    }

    private static object SamplePhysicalPose(AnimationClip clip,
        Dictionary<uint, (string group, string path)> targets)
    {
        var buffer = clip.m_AclCompressedBuffer?.TransformBufferData;
        if (buffer == null) return null;
        var decoder = new AclDecompressor(buffer);
        if (!decoder.IsValid || !decoder.Sample(0, out var first)) return null;
        var paths = clip.m_ClipBindingConstant.genericBindings
            .Where(b => b.typeID == ClassIDType.Transform && b.attribute == 1)
            .Select(b => b.path).ToArray();
        if (paths.Length != first.Length) return new { error = "Transform track mapping does not match binding count",
                                                      bindingTracks = paths.Length, decodedTracks = first.Length };
        var maxAngle = new float[paths.Length];
        var maxTranslation = new float[paths.Length];
        var frameCount = BitConverter.ToUInt32(buffer, 20);
        var rate = BitConverter.ToSingle(buffer, 24);
        if (frameCount == 0 || !float.IsFinite(rate) || rate <= 0) throw new Exception("Invalid ACL sampling header");
        for (uint frame = 1; frame < frameCount; ++frame)
        {
            if (!decoder.Sample(frame / rate, out var current)) throw new Exception("ACL sampling failed");
            for (int i = 0; i < paths.Length; ++i)
            {
                var cosine = Math.Clamp(MathF.Abs(Quaternion.Dot(
                    Quaternion.Normalize(first[i].Rotation), Quaternion.Normalize(current[i].Rotation))), 0, 1);
                maxAngle[i] = MathF.Max(maxAngle[i], 2 * MathF.Acos(cosine) * 180 / MathF.PI);
                maxTranslation[i] = MathF.Max(maxTranslation[i], System.Numerics.Vector3.Distance(first[i].Translation, current[i].Translation));
            }
        }
        return new
        {
            samples = frameCount, decoder.Duration,
            matched = paths.Count(targets.ContainsKey),
            changing = paths.Select((hash, i) => new { hash, i }).Where(x => targets.ContainsKey(x.hash) &&
                (maxAngle[x.i] > 0.1f || maxTranslation[x.i] > 1e-5f))
                .Select(x => new { targets[x.hash].group, targets[x.hash].path, maxRotationDegrees = maxAngle[x.i], maxTranslation = maxTranslation[x.i] })
        };
    }

    private static uint Crc32(string value)
    {
        uint crc = 0xffffffff;
        foreach (byte b in Encoding.UTF8.GetBytes(value))
        {
            crc ^= b;
            for (int bit = 0; bit < 8; ++bit) crc = (crc >> 1) ^ ((crc & 1) != 0 ? 0xedb88320u : 0u);
        }
        return ~crc;
    }
}

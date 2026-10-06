// Offline Animator/AnimationClip binding collector. It does not load game DLLs.
using System.Text.Json;
using AnimeStudio;
using AnimeStudio.GUI;
using AssetObject = AnimeStudio.Object;

internal static class PrefabAnimationProbe
{
    private static readonly JsonSerializerOptions JsonOptions = new() { WriteIndented = true };

    public static int Run(string[] args)
    {
        if (args.Length != 6)
        {
            Console.Error.WriteLine("--inspect-prefab-animation VFS_ROOT WORKSPACE LOGICAL_PREFAB NEW_OUTPUT_JSON SOURCE_INDEX");
            return 2;
        }
        var output = Path.GetFullPath(args[4]);
        if (File.Exists(output) || Directory.Exists(output))
        {
            Console.Error.WriteLine("Animation evidence output must be a new file.");
            return 2;
        }
        var archive = EndfieldVfsArchive.Open(args[1]);
        var index = EndfieldIndexStore.LoadPrefabExportIndex(args[5], args[3]);
        if (index.VfsFingerprint != archive.Fingerprint)
            throw new InvalidDataException("Index fingerprint does not match the source VFS.");
        var sources = index.Prefab.Records.Select(record => record.Source)
            .Where(source => !string.IsNullOrEmpty(source))
            .Distinct(StringComparer.OrdinalIgnoreCase).ToArray();
        if (sources.Length != 1)
            throw new InvalidDataException($"Expected one Prefab source bundle, found {sources.Length}.");
        var sourceBundle = sources[0];
        var closure = index.Dependencies.ResolveBundleClosure(sourceBundle).ToArray();
        var paths = closure.Select(path => archive.ExtractToCache(path, args[2])).ToArray();

        TypeFlags.SetTypes(new());
        foreach (var type in new[] { ClassIDType.GameObject, ClassIDType.Transform,
                     ClassIDType.Animator, ClassIDType.Avatar, ClassIDType.AnimatorController,
                     ClassIDType.AnimatorOverrideController, ClassIDType.AnimationClip,
                     ClassIDType.AssetBundle })
            TypeFlags.SetType(type, true, false);
        Logger.Default = new ConsoleLogger();
        Logger.Flags = LoggerEvent.Error;
        var manager = new AssetsManager
        {
            Game = GameManager.GetGameByType(GameType.ArknightsEndfield),
            SpecifyUnityVersion = ProbeUnityVersion.Override, ResolveDependencies = false
        };
        manager.LoadFiles(paths, mergeSplitAssets: false);
        var root = EndfieldPrefabDocument.FindRoot(
            manager.assetsFileList.SelectMany(file => file.Objects).OfType<GameObject>(),
            index.Prefab, index.Dependencies.GetCabNames(sourceBundle))
            ?? throw new InvalidDataException("Prefab root not found in loaded source.");
        var hierarchy = new HashSet<string>();
        Visit(root, hierarchy);
        var animators = manager.assetsFileList.SelectMany(file => file.Objects).OfType<Animator>()
            .Where(animator => animator.m_GameObject.TryGet(out var owner) && hierarchy.Contains(Identity(owner)))
            .OrderBy(animator => OwnerPath(animator)).ToArray();

        var animatorRows = new List<object>();
        var allMatches = new List<BindingMatch>();
        foreach (var animator in animators)
        {
            animator.m_GameObject.TryGet(out var owner);
            if (!animator.m_Controller.TryGet(out var runtime))
            {
                animatorRows.Add(new { owner = OwnerPath(animator), controller = (string)null,
                    controllerType = (string)null, clips = 0, resolvedClips = 0, transformBindings = 0 });
                continue;
            }
            var controller = ResolveController(runtime);
            var clips = ResolveClips(runtime, controller).Distinct().
                OrderBy(clip => clip.m_Name, StringComparer.Ordinal).ThenBy(clip => clip.m_PathID).ToArray();
            var resolved = 0;
            var transformBindings = 0;
            foreach (var clip in clips)
            {
                var bindings = clip.m_ClipBindingConstant?.genericBindings;
                if (bindings == null && clip.m_MuscleClip?.m_Clip != null)
                    bindings = clip.m_MuscleClip.m_Clip.ConvertValueArrayToGenericBinding().genericBindings;
                if (bindings == null)
                    continue;
                resolved++;
                Dictionary<uint, string> pathsByHash = controller?.m_TOS;
                if (pathsByHash == null || bindings.Any(binding => binding.typeID == ClassIDType.Transform &&
                        binding.path != 0 && !pathsByHash.ContainsKey(binding.path)))
                    pathsByHash = clip.FindTOS();
                foreach (var binding in bindings.Where(binding => binding.typeID == ClassIDType.Transform))
                {
                    transformBindings++;
                    pathsByHash.TryGetValue(binding.path, out var bonePath);
                    bonePath ??= binding.path == 0 ? string.Empty : null;
                    if (bonePath == null || !IsTarget(bonePath))
                        continue;
                    allMatches.Add(new BindingMatch(OwnerPath(animator), runtime.Name, runtime.GetType().Name,
                        clip.m_Name, Identity(clip), bonePath, binding.path, BindingAttribute(binding.attribute)));
                }
            }
            animatorRows.Add(new { owner = OwnerPath(animator), controller = runtime.Name,
                controllerType = runtime.GetType().Name, clips = clips.Length, resolvedClips = resolved,
                transformBindings });
        }

        var report = new
        {
            purpose = "offline-prefab-animation-binding-evidence",
            prefab = args[3], vfsFingerprint = archive.Fingerprint, source = sourceBundle,
            closureBundles = closure.Length,
            targets = new[] { "Bip001_Spine2", "breast_base_L_a_01_jnt", "breast_base_L_a_02_jnt",
                              "breast_base_R_a_01_jnt", "breast_base_R_a_02_jnt" },
            animators = animatorRows,
            matches = allMatches.OrderBy(match => match.BonePath, StringComparer.Ordinal)
                .ThenBy(match => match.Clip, StringComparer.Ordinal).ThenBy(match => match.Attribute).ToArray()
        };
        Directory.CreateDirectory(Path.GetDirectoryName(output)!);
        File.WriteAllText(output, JsonSerializer.Serialize(report, JsonOptions));
        Console.WriteLine($"animators={animators.Length} matches={allMatches.Count} output={output}");
        foreach (var group in allMatches.GroupBy(match => match.BonePath).OrderBy(group => group.Key))
            Console.WriteLine($"{group.Key}: clips={group.Select(match => match.ClipIdentity).Distinct().Count()} bindings={group.Count()}");
        return 0;
    }

    private static AnimatorController ResolveController(RuntimeAnimatorController runtime)
    {
        if (runtime is AnimatorController controller)
            return controller;
        if (runtime is AnimatorOverrideController overrides && overrides.m_Controller.TryGet<AnimatorController>(out var original))
            return original;
        return null;
    }

    private static IEnumerable<AnimationClip> ResolveClips(RuntimeAnimatorController runtime, AnimatorController controller)
    {
        if (controller != null)
            foreach (var pointer in controller.m_AnimationClips)
                if (pointer.TryGet(out var clip))
                    yield return clip;
        if (runtime is AnimatorOverrideController overrides)
            foreach (var entry in overrides.m_Clips)
                if (entry.m_OverrideClip.TryGet(out var clip))
                    yield return clip;
    }

    private static bool IsTarget(string path)
    {
        var name = path.Replace('\\', '/').Split('/').LastOrDefault() ?? string.Empty;
        return name.Equals("Bip001_Spine2", StringComparison.OrdinalIgnoreCase) ||
               name.StartsWith("breast_base_L_", StringComparison.OrdinalIgnoreCase) ||
               name.StartsWith("breast_base_R_", StringComparison.OrdinalIgnoreCase);
    }

    private static string BindingAttribute(uint attribute) => attribute switch
    {
        1 => "localPosition", 2 => "localRotation", 3 => "localScale", 4 => "localEulerAngles",
        _ => "transformAttribute:" + attribute
    };

    private static string Identity(AssetObject value) => value.assetsFile.fileName + ":" + value.m_PathID;

    private static string OwnerPath(Component component)
    {
        return component.m_GameObject.TryGet(out var owner) ? OwnerPath(owner) : null;
    }

    private static string OwnerPath(GameObject owner)
    {
        var names = new List<string> { owner.m_Name };
        if (!owner.m_Transform.m_Father.IsNull && owner.m_Transform.m_Father.TryGet(out var parent))
        {
            while (parent != null && parent.m_GameObject.TryGet(out var parentOwner))
            {
                names.Add(parentOwner.m_Name);
                if (parent.m_Father.IsNull || !parent.m_Father.TryGet(out parent))
                    break;
            }
        }
        names.Reverse();
        return string.Join('/', names);
    }

    private static void Visit(GameObject owner, HashSet<string> seen)
    {
        if (!seen.Add(Identity(owner)) || owner.m_Transform == null)
            return;
        foreach (var pointer in owner.m_Transform.m_Children)
            if (pointer.TryGet(out var child) && child.m_GameObject.TryGet(out var childOwner))
                Visit(childOwner, seen);
    }

    private sealed record BindingMatch(string Animator, string Controller, string ControllerType,
        string Clip, string ClipIdentity, string BonePath, uint PathHash, string Attribute);
}

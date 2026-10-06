using System.Buffers.Binary;
using System.Collections;
using System.Collections.Specialized;
using System.Security.Cryptography;
using System.Text.Json;
using AnimeStudio;
using SkiaSharp;
using AssetObject = AnimeStudio.Object;

internal static partial class NativePackProbe
{
    public static int Verify(string[] args)
    {
        if (args.Length != 3) throw new ArgumentException("--verify-native-pack EXPERIMENT NEW_REPORT_JSON");
        var experiment = Path.GetFullPath(args[1]);
        var reportPath = Path.GetFullPath(args[2]);
        if (File.Exists(reportPath)) throw new IOException("Evidence already exists.");
        Directory.CreateDirectory(Path.GetDirectoryName(reportPath));
        var manager = NewManager();
        var originalLogger = Logger.Default;
        var parserLog = new NativeVerificationLogger();
        Logger.Default = parserLog;
        var stage = "manifest";
        try
        {
            using var buildDoc = JsonDocument.Parse(File.ReadAllText(Path.Combine(experiment, "build-manifest.json")));
            var build = buildDoc.RootElement;
            Check(build.GetProperty("format").GetString() == "EFF_NATIVE_WRITE_EXPERIMENT" &&
                build.GetProperty("version").GetInt32() == 1 && build.GetProperty("builtOffline").GetBoolean(), "Build manifest");
            var source = build.GetProperty("sourcePackage").GetString();
            using var baselineDoc = JsonDocument.Parse(File.ReadAllText(Path.Combine(source, "source", "manifest.json")));
            var baseline = baselineDoc.RootElement;
            manager.StrippedUnityVersionResolver = ProbeUnityVersion.FromManifest(baseline);
            var bundles = build.GetProperty("bundles").EnumerateArray().ToArray();
            var paths = new List<string>();
            foreach (var row in bundles)
            {
                var path = Safe(experiment, row.GetProperty("file").GetString());
                using var stream = File.OpenRead(path);
                Check(stream.Length == row.GetProperty("size").GetInt64() && Convert.ToHexString(SHA256.HashData(stream)) == row.GetProperty("sha256").GetString(), "Bundle hash");
                paths.Add(path);
            }
            stage = "AnimeStudio-native-bundle-load";
            manager.LoadFiles(paths.ToArray(), mergeSplitAssets: false);
            Check(parserLog.ErrorCount == 0, "AnimeStudio native parse errors: " + parserLog.ErrorCount);
            Check(manager.assetsFileList.Count == bundles.Length, "Native SerializedFile count");
            var objects = manager.assetsFileList.SelectMany(f => f.Objects).ToDictionary(Key);
            var changes = build.GetProperty("changedObjects").EnumerateArray().ToDictionary(r => r.GetProperty("identity").GetString());
            var preserved = 0;
            var metadataFilesByteIdentical = 0;
            stage = "native-metadata-and-unchanged-object-bytes";
            foreach (var row in baseline.GetProperty("serializedFiles").EnumerateArray())
            {
                var cab = row.GetProperty("cab").GetString();
                var file = manager.assetsFileList.Single(f => f.fileName == cab);
                var table = row.GetProperty("objects").EnumerateArray().ToArray();
                var additions = changes.Values.Count(c => c.GetProperty("added").GetBoolean() && c.GetProperty("identity").GetString().StartsWith(cab.ToLowerInvariant()+":"));
                Check(file.m_Objects.Count == table.Length+additions && file.Objects.Count == file.m_Objects.Count, "Object count / parse failure in " + cab);
                var types = row.GetProperty("types").EnumerateArray().ToArray();
                Check(file.m_Types.Count == types.Length, "Native type table count");
                for (var i = 0; i < types.Length; i++)
                    Check(file.m_Types[i].classID == types[i].GetProperty("classId").GetInt32() &&
                        Convert.ToHexString(file.m_Types[i].m_OldTypeHash) == types[i].GetProperty("oldTypeHash").GetString() &&
                        file.m_Types[i].m_Type.m_Nodes.Count == types[i].GetProperty("typeTreeNodes").GetInt32(), "Native type identity/schema");
                var external = row.GetProperty("externals").EnumerateArray().ToArray();
                Check(file.m_Externals.Count == external.Length, "Native external count");
                for (var i = 0; i < external.Length; i++)
                    Check(file.m_Externals[i].pathName == external[i].GetProperty("path").GetString() && file.m_Externals[i].guid.ToString() == external[i].GetProperty("guid").GetString(), "Native external identity");
                var before = File.ReadAllBytes(Safe(source, row.GetProperty("snapshot").GetProperty("file").GetString()));
                Check(Convert.ToHexString(SHA256.HashData(before)) == row.GetProperty("snapshot").GetProperty("sha256").GetString(), "Source snapshot hash");
                var savedPosition = file.reader.Position;
                file.reader.Position = 0;
                var afterMetadata = file.reader.ReadBytes(checked(48 + (int)file.header.m_MetadataSize));
                file.reader.Position = savedPosition;
                VerifyMetadataBytes(before, afterMetadata, cab);
                metadataFilesByteIdentical++;
                foreach (var obj in table)
                {
                    var key = cab.ToLowerInvariant()+":"+obj.GetProperty("pathId").GetInt64();
                    var actual = objects[key];
                    // Custom game ClassIDs are represented as UnknownType by AnimeStudio's enum.
                    // Check the serialized object table, which retains the actual native ClassID.
                    var info = file.m_Objects.Single(entry => entry.m_PathID == actual.m_PathID);
                    Check(info.classID == obj.GetProperty("classId").GetInt32() &&
                        info.typeID == obj.GetProperty("typeIndex").GetInt32(), "Native object class/type index: " + key);
                    var raw = actual.GetRawData();
                    if (!changes.ContainsKey(key))
                    {
                        Check(raw.AsSpan().SequenceEqual(before.AsSpan(obj.GetProperty("offset").GetInt32(), obj.GetProperty("size").GetInt32())), "Unmodified native object changed: " + key);
                        preserved++;
                    }
                }
            }
            stage = "original-raw-stream-prefix-preservation";
            long preservedStreamBytes = 0;
            var streams = baseline.GetProperty("rawStreams").EnumerateArray().ToArray();
            Check(manager.ResourceFileReaders.Count == streams.Length, "Native raw stream count");
            foreach (var row in streams)
            {
                var member = row.GetProperty("member").GetString();
                var snapshot = row.GetProperty("snapshot");
                var size = snapshot.GetProperty("size").GetInt64();
                using var sourceStream = File.OpenRead(Safe(source, snapshot.GetProperty("file").GetString()));
                Check(sourceStream.Length == size && Convert.ToHexString(SHA256.HashData(sourceStream)) == snapshot.GetProperty("sha256").GetString(), "Original raw stream hash: " + member);
                Check(manager.ResourceFileReaders.TryGetValue(member, out var parsedStream) && parsedStream.BaseStream.Length >= size, "Native raw stream missing/truncated: " + member);
                Check(PrefixHash(parsedStream.BaseStream, size) == snapshot.GetProperty("sha256").GetString(), "Original raw stream prefix changed: " + member);
                preservedStreamBytes += size;
            }
            stage = "changed-native-object-hashes";
            foreach (var row in changes.Values)
            {
                var key = row.GetProperty("identity").GetString();
                Check(Convert.ToHexString(SHA256.HashData(objects[key].GetRawData())) == row.GetProperty("payloadSha256").GetString(), "Rebuilt object payload differs: " + key);
            }
            stage = "author-native-Mesh-and-Renderer-correspondence";
            var meshResults = new List<object>();
            foreach (var row in build.GetProperty("parts").EnumerateArray())
            {
                var mesh = (Mesh)objects[row.GetProperty("mesh").GetString()];
                var renderer = (SkinnedMeshRenderer)objects[row.GetProperty("renderer").GetString()];
                var author = ReadAuthorMesh(row.GetProperty("authorFile").GetString());
                Check(mesh.m_VertexCount == author.Vertices.Length/3 && mesh.m_Indices.SequenceEqual(author.Indices), "Native Mesh counts/index values");
                Check(mesh.m_SubMeshes.Count == 1 && mesh.m_SubMeshes[0].vertexCount == mesh.m_VertexCount && mesh.m_SubMeshes[0].indexCount == author.Indices.Length, "Native single-submesh layout");
                Check(mesh.m_BindPose.Length == author.Bindposes.Length && mesh.m_BoneNameHashes.SequenceEqual(author.Hashes), "Native palette/hash layout");
                // Native payload is row-major; EFFMESH stores managed Unity matrices column-major.
                // AnimeStudio reads the raw order. Apply the same explicit coordinate conversion
                // as EiemPackageWriter.WriteMatrix, rather than comparing unlike representations.
                for (var i = 0; i < author.Bindposes.Length; i++)
                    for (var column = 0; column < 4; column++)
                        for (var matrixRow = 0; matrixRow < 4; matrixRow++)
                            Check(mesh.m_BindPose[i][column, matrixRow] == author.Bindposes[i][matrixRow + column * 4],
                                "Native bindpose values: " + Key(mesh) + " / slot " + i);
                var vertexError = Error(mesh.m_Vertices, author.Vertices, 0, "positions");
                var normalError = Error(mesh.m_Normals, author.Normals, .006f, "packed normals");
                var tangentError = Error(mesh.m_Tangents, author.Tangents, .015f, "packed tangents");
                var uv = new[] {mesh.m_UV0,mesh.m_UV1,mesh.m_UV2,mesh.m_UV3,mesh.m_UV4,mesh.m_UV5,mesh.m_UV6,mesh.m_UV7};
                for (var i=0; i<8; i++) Error(uv[i],author.Uvs[i],0,"UV"+i);
                Error(mesh.m_Colors,author.Colors,0,"colors");
                Check(mesh.m_Skin.Count == author.Weights.Length, "Native skin count");
                float weightError = 0;
                for (var i = 0; i < author.Weights.Length; i++)
                {
                    weightError = Math.Max(weightError, Error(mesh.m_Skin[i].weight, author.Weights[i], 3e-5f, "UNorm16 weights"));
                    Check(mesh.m_Skin[i].boneIndex.Select(x => (uint)x).SequenceEqual(author.Bones[i]), "Native bone index values");
                }
                Pointer(renderer.m_Mesh,row.GetProperty("mesh").GetString());
                Check(renderer.m_Materials.Count==1,"Native Renderer material slots");
                Pointer(renderer.m_Materials[0],row.GetProperty("material").GetString());
                var bones=row.GetProperty("boneTransforms").EnumerateArray().Select(x=>x.GetString()).ToArray();
                Check(renderer.m_Bones.Count==bones.Length,"Native Renderer bone palette count");
                for(var i=0;i<bones.Length;i++) Pointer(renderer.m_Bones[i],bones[i]);
                var rootBone = row.TryGetProperty("sourceRootBone", out var rowRoot) ? rowRoot.GetString() : build.GetProperty("sourceRootBone").GetString();
                var skinningRoot = row.TryGetProperty("sourceSkinningRoot", out var rowSkin) ? rowSkin.GetString() : build.GetProperty("sourceSkinningRoot").GetString();
                Pointer(renderer.m_RootBone,rootBone);
                Pointer(renderer.m_SkinningRoot,skinningRoot);
                VerifyShapes(mesh, author);
                if (row.TryGetProperty("shapeWeights", out var shapeWeights))
                    Error(renderer.m_BlendShapeWeights, shapeWeights.EnumerateArray().Select(w => w.GetSingle()).ToArray(), 0, "Native Renderer shape weights");
                else Check(renderer.m_BlendShapeWeights.Length == 0, "No unintended Renderer shapes");
                if (row.TryGetProperty("enabled", out var enabled)) Check(Convert.ToBoolean(renderer.ToType()["m_Enabled"]) == enabled.GetBoolean(), "Initial Renderer visibility");
                var bounds=row.GetProperty("bounds");
                var actualBounds=new[]{renderer.m_AABB.m_Center.X,renderer.m_AABB.m_Center.Y,renderer.m_AABB.m_Center.Z,renderer.m_AABB.m_Extent.X,renderer.m_AABB.m_Extent.Y,renderer.m_AABB.m_Extent.Z};
                var expectedBounds=new[]{"m_Center","m_Extent"}.SelectMany(k=>new[]{"x","y","z"}.Select(a=>bounds.GetProperty(k).GetProperty(a).GetSingle())).ToArray();
                Error(actualBounds,expectedBounds,1e-6f,"Renderer root-space bounds");
                var nativeTree=mesh.ToType();
                Check(mesh.reader.Position-mesh.reader.byteStart==mesh.byteSize,"Mesh TypeTree consumption");
                Check(Convert.ToInt32(nativeTree["m_BonesPerVertex"])==4 && ((IList)nativeTree["m_BonesAABB"]).Count==author.Bindposes.Length,"Native serialized skin initialization input");
                var boneBoundsError = VerifyMeshBounds(nativeTree, author);
                var rootSlot = Array.IndexOf(bones, rootBone);
                Check(rootSlot >= 0 && bones.Count(b => b == bones[rootSlot]) == 1, "Unique Renderer root bone bindpose");
                var rootBounds = ExpectedBounds(author, author.Bindposes[rootSlot], null);
                var independentlyExpectedRootBounds = Enumerable.Range(0, 3).Select(k => (float)((rootBounds[k] + rootBounds[k+3])*.5))
                    .Concat(Enumerable.Range(0, 3).Select(k => (float)((rootBounds[k+3] - rootBounds[k])*.5))).ToArray();
                var rootBoundsError = Error(actualBounds, independentlyExpectedRootBounds, 1e-6f, "Independent root-space Renderer bounds");
                meshResults.Add(new{mesh=Key(mesh),vertices=mesh.m_VertexCount,indices=mesh.m_Indices.Count,bones=mesh.m_BindPose.Length,
                    maxPositionError=vertexError,maxNormalError=normalError,maxTangentError=tangentError,maxWeightError=weightError,
                    maxBoneBoundsError=boneBoundsError,maxRendererBoundsError=rootBoundsError,shapeChannels=author.ShapeChannels.Length});
            }
            stage = "child-hierarchy-LOD-preload-references";
            if (build.TryGetProperty("children", out var children))
            {
                foreach (var child in children.EnumerateArray())
                    VerifyChild(objects, child.GetProperty("owner").GetString(), child.GetProperty("transform").GetString(), child.GetProperty("parent").GetString(), child.GetProperty("renderer").GetString());
            }
            else VerifyChild(objects, build.GetProperty("childOwner").GetString(), build.GetProperty("childTransform").GetString(), build.GetProperty("sourceTransform").GetString(), build.GetProperty("parts")[1].GetProperty("renderer").GetString());
            if (build.TryGetProperty("hiddenRenderers", out var hiddenRenderers))
                foreach (var hidden in hiddenRenderers.EnumerateArray())
                {
                    var renderer = (SkinnedMeshRenderer)objects[hidden.GetProperty("renderer").GetString()];
                    Check(!Convert.ToBoolean(renderer.ToType()["m_Enabled"]), "Hidden source Renderer remains enabled");
                    Pointer(renderer.m_Mesh, hidden.GetProperty("sourceMesh").GetString());
                    foreach (var lodObject in objects.Values.Where(o => o.serializedType.classID == 205))
                        foreach (OrderedDictionary lod in (IList)lodObject.ToType()["m_LODs"])
                            Check(!((IList)lod["renderers"]).Cast<OrderedDictionary>().Any(r => Convert.ToInt64(((OrderedDictionary)r["renderer"])["m_PathID"]) == renderer.m_PathID), "Hidden Renderer remains in LODGroup");
                }
            foreach(var update in build.GetProperty("lodUpdates").EnumerateArray())
            {
                var obj=objects[update.GetProperty("identity").GetString()];
                var tree=obj.ToType();Check(obj.reader.Position-obj.reader.byteStart==obj.byteSize,"LOD TypeTree consumption");
                var lod=(OrderedDictionary)((IList)tree["m_LODs"])[update.GetProperty("level").GetInt32()];
                var pointers=((IList)lod["renderers"]).Cast<OrderedDictionary>().Select(r=>(OrderedDictionary)r["renderer"]).ToArray();
                var added=long.Parse(update.GetProperty("addedRenderer").GetString().Split(':')[1]);
                Check(pointers.Count(p=>Convert.ToInt32(p["m_FileID"])==0 && Convert.ToInt64(p["m_PathID"])==added)==1,"LOD added Renderer reference");
            }
            foreach(var file in manager.assetsFileList)
            {
                foreach(var bundle in file.Objects.OfType<AssetBundle>())
                {
                    Check(bundle.m_PreloadTable.All(p=>p.IsNull || p.TryGet(out _)),"Unresolved preload PPtr");
                    var tree=bundle.ToType();Check(bundle.reader.Position-bundle.reader.byteStart==bundle.byteSize,"AssetBundle TypeTree consumption");
                    foreach(var entry in bundle.m_Container)
                        Check(entry.Value.preloadIndex>=0 && entry.Value.preloadSize>=0 && entry.Value.preloadIndex+entry.Value.preloadSize<=bundle.m_PreloadTable.Count,"Container preload range");
                }
            }
            stage = "Texture2D-native-decode";
            var pngDirectory=Path.Combine(experiment,"redecoded-textures");Directory.CreateDirectory(pngDirectory);
            var textures=new List<object>();
            foreach(var row in build.GetProperty("textures").EnumerateArray())
            {
                var tex=(Texture2D)objects[row.GetProperty("identity").GetString()];
                Check(tex.m_Width==row.GetProperty("width").GetInt32() && tex.m_Height==row.GetProperty("height").GetInt32() &&
                    tex.m_TextureFormat==TextureFormat.RGBA32 && tex.m_MipCount==row.GetProperty("mipCount").GetInt32() && tex.m_ColorSpace==row.GetProperty("colorSpace").GetInt32(),"Native Texture2D fields");
                var sourceTexture = baseline.GetProperty("resources").EnumerateArray().Single(r => r.GetProperty("identity").GetString() == Key(tex));
                Check(tex.m_ColorSpace == sourceTexture.GetProperty("details").GetProperty("colorSpace").GetInt32(), "Preserved source texture interpretation");
                Check(row.GetProperty("colorSpacePolicy").GetString() == "preserve-native-interpretation-for-unchanged-source-material" &&
                    row.GetProperty("colorSpaceDeclarationMismatch").GetBoolean() == (row.GetProperty("authorDeclaredColorSpace").GetInt32() != tex.m_ColorSpace), "Texture color space evidence");
                var raw=tex.image_data.GetData();
                long mipBytes = 0;
                var mipWidth = tex.m_Width; var mipHeight = tex.m_Height;
                for (var mip = 0; mip < tex.m_MipCount; mip++)
                {
                    mipBytes += (long)mipWidth * mipHeight * 4;
                    mipWidth = Math.Max(1, mipWidth/2); mipHeight = Math.Max(1, mipHeight/2);
                }
                Check(raw.LongLength == mipBytes && raw.LongLength == row.GetProperty("streamSize").GetInt64(), "Complete RGBA32 mip payload");
                var topDown=new byte[tex.m_Width*tex.m_Height*4];
                for(var y=0;y<tex.m_Height;y++) Buffer.BlockCopy(raw,(tex.m_Height-1-y)*tex.m_Width*4,topDown,y*tex.m_Width*4,tex.m_Width*4);
                Check(Convert.ToHexString(SHA256.HashData(topDown))==row.GetProperty("rgbaTopDownSha256").GetString(),"Native base texture pixels");
                using var image=tex.ConvertToImage(true);Check(image!=null,"AnimeStudio native Texture2D decode");
                using var encoded=image.Encode(SKEncodedImageFormat.Png,100);
                var png=Path.Combine(pngDirectory,row.GetProperty("section").GetString()+"_"+tex.m_PathID+".png");
                File.WriteAllBytes(png,encoded.ToArray());
                textures.Add(new{texture=Key(tex),width=tex.m_Width,height=tex.m_Height,mipCount=tex.m_MipCount,redecodedPng=png,authorPng=row.GetProperty("authorPng").GetString()});
            }
            var result=new{format="EFF_NATIVE_WRITE_VERIFICATION",version=1,verified=true,offlineOnly=true,nativeLoadVerified=false,gameReady=false,
                experiment,serializedFiles=manager.assetsFileList.Count,objects=objects.Count,unchangedObjectsByteIdentical=preserved,
                metadataFilesByteIdentical,rawStreamsWithOriginalPrefix=streams.Length,preservedStreamBytes,
                changedObjects=changes.Count,addedObjects=changes.Values.Count(r=>r.GetProperty("added").GetBoolean()),meshResults,textures,
                checks=new[]{"88 native UnityFS bundles / complete object parsing","Original type/external/script metadata byte identity","Unmodified native object payload identity",
                    "All original raw stream prefixes / append-only texture updates",
                    "Mesh arrays, packed tangent frames, weights, bindposes, UVs and indices","Single-material SMRs / exact author bone palettes / root and skinningRoot",
                    "Independent per-bone bounds and Renderer root-space bounds",
                    "Child hierarchy and native LODGroup references","AssetBundle preload PPtrs / ranges","Native Texture2D base pixels / AnimeStudio PNG decode / source color space / complete mip size"}};
            File.WriteAllText(reportPath,JsonSerializer.Serialize(result,JsonOptions)+"\n");
            Console.WriteLine(JsonSerializer.Serialize(new{verified=true,offlineOnly=true,nativeLoadVerified=false,objects=objects.Count,preserved,parts=meshResults.Count,textures=textures.Count,report=reportPath}));
            return 0;
        }
        catch(Exception error)
        {
            var result=new{verified=false,nativeLoadVerified=false,stage,error=error.GetType().Name+": "+error.Message,
                stack=error.ToString(),parserErrorCount=parserLog.ErrorCount,parserErrors=parserLog.Messages};
            File.WriteAllText(reportPath,JsonSerializer.Serialize(result,JsonOptions)+"\n");
            Console.Error.WriteLine(JsonSerializer.Serialize(result));
            return 1;
        }
        finally {manager.Clear(); Logger.Default = originalLogger;}
    }

    private static void VerifyChild(Dictionary<string, AssetObject> objects, string ownerId, string transformId, string parentId, string rendererId)
    {
        var childGo = (GameObject)objects[ownerId];
        var childTransform = (Transform)objects[transformId];
        var parent = (Transform)objects[parentId];
        var childRenderer = (SkinnedMeshRenderer)objects[rendererId];
        Pointer(childTransform.m_GameObject,Key(childGo));Pointer(childRenderer.m_GameObject,Key(childGo));
        Pointer(childTransform.m_Father,Key(parent));
        Check(parent.m_Children.Count(p=>p.TryGet(out var t) && Key(t)==Key(childTransform))==1,"Parent child reference");
        Check(childGo.m_Components.Count==2,"Child component count");
        Pointer(childGo.m_Components[0],Key(childTransform));Pointer(childGo.m_Components[1],Key(childRenderer));
        Check(childTransform.m_LocalPosition.X==0 && childTransform.m_LocalPosition.Y==0 && childTransform.m_LocalPosition.Z==0 &&
            childTransform.m_LocalRotation.X==0 && childTransform.m_LocalRotation.Y==0 && childTransform.m_LocalRotation.Z==0 && childTransform.m_LocalRotation.W==1 &&
            childTransform.m_LocalScale.X==1 && childTransform.m_LocalScale.Y==1 && childTransform.m_LocalScale.Z==1,"Child identity transform");
    }

    private static void VerifyShapes(Mesh mesh, AuthorMesh author)
    {
        Check(mesh.m_Shapes.vertices.Count == author.ShapeVertices.Length && mesh.m_Shapes.shapes.Count == author.ShapeFrames.Length && mesh.m_Shapes.channels.Count == author.ShapeChannels.Length, "Native shape array lengths");
        for (var i=0; i<author.ShapeVertices.Length; i++)
        {
            var actual=mesh.m_Shapes.vertices[i]; var expected=author.ShapeVertices[i];
            Check(actual.index==expected.Index,"Native shape vertex index");
            Error(new[]{actual.vertex.X,actual.vertex.Y,actual.vertex.Z},expected.Position,0,"Shape vertex delta");
            Error(new[]{actual.normal.X,actual.normal.Y,actual.normal.Z},expected.Normal,0,"Shape normal delta");
            Error(new[]{actual.tangent.X,actual.tangent.Y,actual.tangent.Z},expected.Tangent,0,"Shape tangent delta");
        }
        for (var i=0;i<author.ShapeFrames.Length;i++)
        {
            var actual=mesh.m_Shapes.shapes[i];var expected=author.ShapeFrames[i];
            Check(actual.firstVertex==expected.First && actual.vertexCount==expected.Count && actual.hasNormals==expected.Normals && actual.hasTangents==expected.Tangents && !expected.Extra,"Native shape frame values");
        }
        for (var i=0;i<author.ShapeChannels.Length;i++)
        {
            var actual=mesh.m_Shapes.channels[i];var expected=author.ShapeChannels[i];
            Check(actual.name==expected.Name && actual.nameHash==expected.Hash && actual.frameIndex==expected.First && actual.frameCount==expected.Count,"Native shape channel identity");
        }
        Error(mesh.m_Shapes.fullWeights,author.ShapeWeights,0,"Native shape frame weights");
    }

    // AssetsManager logs and absorbs individual read exceptions; preserve those
    // reasons in failure evidence instead of reducing them to an object-count error.
    private sealed class NativeVerificationLogger : ILogger
    {
        private readonly object gate = new();
        public int ErrorCount { get; private set; }
        public List<string> Messages { get; } = new();
        public void Log(LoggerEvent level, string message)
        {
            if (!level.HasFlag(LoggerEvent.Error)) return;
            lock (gate)
            {
                ErrorCount++;
                if (Messages.Count < 64) Messages.Add(message.Length <= 32768 ? message : message[..32768]);
            }
        }
    }

    private static string PrefixHash(Stream stream, long size)
    {
        var savedPosition = stream.Position;
        try
        {
            stream.Position = 0;
            using var hash = IncrementalHash.CreateHash(HashAlgorithmName.SHA256);
            var buffer = new byte[65536];
            while (size > 0)
            {
                var read = stream.Read(buffer, 0, (int)Math.Min(size, buffer.Length));
                Check(read > 0, "Native stream truncated during prefix validation");
                hash.AppendData(buffer, 0, read); size -= read;
            }
            return Convert.ToHexString(hash.GetHashAndReset());
        }
        finally { stream.Position = savedPosition; }
    }

    private static (int CountOffset, int TableEnd, int MetadataEnd) MetadataParts(byte[] bytes)
    {
        Check(BinaryPrimitives.ReadInt32BigEndian(bytes.AsSpan(8, 4)) == 22 && bytes[16] == 0, "Metadata verification requires v22 little-endian");
        var metadataEnd = checked(48 + BinaryPrimitives.ReadInt32BigEndian(bytes.AsSpan(20, 4)));
        using var reader = new BinaryReader(new MemoryStream(bytes, false));
        reader.BaseStream.Position = 48;
        while (reader.ReadByte() != 0) Check(reader.BaseStream.Position < metadataEnd, "Native version string unterminated");
        reader.ReadInt32(); Check(reader.ReadBoolean(), "Native TypeTree disabled");
        var types = reader.ReadInt32(); Check(types >= 0 && types < 100000, "Native type count");
        for (var i = 0; i < types; i++)
        {
            var classId = reader.ReadInt32(); reader.ReadBoolean(); reader.ReadInt16();
            if (classId == 114) reader.BaseStream.Position += 16;
            reader.BaseStream.Position += 16;
            var nodes = reader.ReadInt32(); var strings = reader.ReadInt32();
            Check(nodes >= 0 && strings >= 0, "Native type node/string lengths");
            reader.BaseStream.Position = checked(reader.BaseStream.Position + (long)nodes * 32 + strings);
            var dependencies = reader.ReadInt32(); Check(dependencies >= 0, "Native type dependency count");
            reader.BaseStream.Position = checked(reader.BaseStream.Position + (long)dependencies * 4);
            Check(reader.BaseStream.Position <= metadataEnd, "Native type metadata exceeds its range");
        }
        var countOffset = checked((int)reader.BaseStream.Position);
        var count = reader.ReadInt32(); Check(count >= 0, "Native object count");
        var tableEnd = checked(((countOffset + 7) & ~3) + count * 24);
        Check(tableEnd <= metadataEnd && metadataEnd <= bytes.Length, "Native metadata table range");
        return (countOffset, tableEnd, metadataEnd);
    }

    private static void VerifyMetadataBytes(byte[] before, byte[] after, string cab)
    {
        var original = MetadataParts(before); var rebuilt = MetadataParts(after);
        Check(before.AsSpan(0, 20).SequenceEqual(after.AsSpan(0, 20)) && before.AsSpan(40, 8).SequenceEqual(after.AsSpan(40, 8)), "Untouched native header bytes: " + cab);
        Check(before.AsSpan(48, original.CountOffset-48).SequenceEqual(after.AsSpan(48, rebuilt.CountOffset-48)), "Native type metadata bytes: " + cab);
        Check(before.AsSpan(original.TableEnd, original.MetadataEnd-original.TableEnd).SequenceEqual(after.AsSpan(rebuilt.TableEnd, rebuilt.MetadataEnd-rebuilt.TableEnd)), "Native script/external/reference metadata bytes: " + cab);
    }

    private static float[] NativeVector(object value)
    {
        var vector = (OrderedDictionary)value;
        return new[] { Convert.ToSingle(vector["x"]), Convert.ToSingle(vector["y"]), Convert.ToSingle(vector["z"]) };
    }

    private static double[] ExpectedBounds(AuthorMesh author, float[] matrix, int? boneSlot)
    {
        var count=author.Vertices.Length/3;
        var lower=author.Vertices.Select(v=>(double)v).ToArray();var upper=(double[])lower.Clone();
        foreach(var channel in author.ShapeChannels)
        {
            var channelLow=new double[count*3];var channelHigh=new double[count*3];
            for(var f=channel.First;f<channel.First+channel.Count;f++)
            {
                var frame=author.ShapeFrames[f];Check(author.ShapeWeights[f]>0,"Positive shape frame weight");
                for(var i=frame.First;i<frame.First+frame.Count;i++)
                {
                    var v=author.ShapeVertices[i];
                    for(var axis=0;axis<3;axis++)
                    {
                        var delta=v.Position[axis]*100.0/author.ShapeWeights[f];var k=v.Index*3+axis;
                        channelLow[k]=Math.Min(channelLow[k],delta);channelHigh[k]=Math.Max(channelHigh[k],delta);
                    }
                }
            }
            for(var k=0;k<lower.Length;k++){lower[k]+=channelLow[k];upper[k]+=channelHigh[k];}
        }
        var bounds=new[]{double.PositiveInfinity,double.PositiveInfinity,double.PositiveInfinity,double.NegativeInfinity,double.NegativeInfinity,double.NegativeInfinity};
        var any=false;
        for(var vertex=0;vertex<count;vertex++)
        {
            if(boneSlot.HasValue && !Enumerable.Range(0,4).Any(k=>author.Bones[vertex][k]==boneSlot.Value && author.Weights[vertex][k]>0))continue;
            any=true;
            for(var axis=0;axis<3;axis++)
            {
                var low=lower[vertex*3+axis];var high=upper[vertex*3+axis];
                if(matrix!=null)
                {
                    low=matrix[axis+12];high=matrix[axis+12];
                    for(var k=0;k<3;k++)
                    {
                        var a=matrix[axis+k*4]*lower[vertex*3+k];var b=matrix[axis+k*4]*upper[vertex*3+k];
                        low+=Math.Min(a,b);high+=Math.Max(a,b);
                    }
                }
                bounds[axis]=Math.Min(bounds[axis],low);bounds[axis+3]=Math.Max(bounds[axis+3],high);
            }
        }
        return any?bounds:new double[6];
    }

    private static float VerifyMeshBounds(OrderedDictionary tree, AuthorMesh author)
    {
        var boneBounds = (IList)tree["m_BonesAABB"];
        float maximum = 0;
        for (var slot = 0; slot < author.Bindposes.Length; slot++)
        {
            var native = (OrderedDictionary)boneBounds[slot];
            var actual = NativeVector(native["m_Min"]).Concat(NativeVector(native["m_Max"])).ToArray();
            var expected = ExpectedBounds(author, author.Bindposes[slot], slot).Select(v => (float)v).ToArray();
            maximum = Math.Max(maximum, Error(actual, expected, 1e-6f, "Bone-space AABB / slot " + slot));
        }
        var local = (OrderedDictionary)tree["m_LocalAABB"];
        var bounds = ExpectedBounds(author, null, null);
        var localExpected = Enumerable.Range(0,3).Select(k => (float)((bounds[k] + bounds[k+3])*.5))
            .Concat(Enumerable.Range(0,3).Select(k => (float)((bounds[k+3] - bounds[k])*.5))).ToArray();
        Error(NativeVector(local["m_Center"]).Concat(NativeVector(local["m_Extent"])).ToArray(), localExpected, 1e-6f, "Mesh model-space AABB");
        return maximum;
    }

    private static string Safe(string root,string relative)
    {
        var path=Path.GetFullPath(Path.Combine(root,relative));
        Check(path.StartsWith(Path.GetFullPath(root).TrimEnd(Path.DirectorySeparatorChar)+Path.DirectorySeparatorChar,StringComparison.OrdinalIgnoreCase),"Path escapes package");
        return path;
    }
    private static string Key(AssetObject obj)=>obj.assetsFile.fileName.ToLowerInvariant()+":"+obj.m_PathID;
    private static void Pointer<T>(PPtr<T> pointer,string identity) where T:AssetObject
    {
        Check(pointer!=null && !pointer.IsNull && pointer.Cast<AssetObject>().TryGet(out var target) && Key(target)==identity,"Native PPtr: "+identity);
    }
    private static float Error(float[] actual,float[] expected,float tolerance,string field)
    {
        actual ??=Array.Empty<float>();expected ??=Array.Empty<float>();
        Check(actual.Length==expected.Length,"Native array length: "+field);
        float error=0;
        for(var i=0;i<actual.Length;i++)
        {
            Check(float.IsFinite(actual[i]) && float.IsFinite(expected[i]),"Native array nonfinite: "+field);
            error=Math.Max(error,Math.Abs(actual[i]-expected[i]));
        }
        Check(error<=tolerance,"Native array precision: "+field+" / "+error);
        return error;
    }
    private sealed record AuthorShapeVertex(uint Index,float[] Position,float[] Normal,float[] Tangent);
    private sealed record AuthorShapeFrame(string Name,uint First,uint Count,bool Normals,bool Tangents,bool Extra);
    private sealed record AuthorShapeChannel(string Name,uint Hash,int First,int Count);
    private sealed record AuthorMesh(float[] Vertices,float[] Normals,float[] Tangents,float[] Colors,float[][] Uvs,uint[] Indices,float[][] Weights,uint[][] Bones,float[][] Bindposes,uint[] Hashes,
        AuthorShapeVertex[] ShapeVertices,AuthorShapeFrame[] ShapeFrames,AuthorShapeChannel[] ShapeChannels,float[] ShapeWeights);
    private static AuthorMesh ReadAuthorMesh(string path)
    {
        using var reader=new BinaryReader(File.OpenRead(path));
        Check(System.Text.Encoding.ASCII.GetString(reader.ReadBytes(8))=="EFFMESH\0" && reader.ReadInt32()==6,"Author mesh v6");
        Check(reader.ReadString()=="unity-y-up-left-handed","Author coordinate");reader.ReadString();reader.ReadString();var count=reader.ReadInt32();
        float[] Floats(){var n=reader.ReadInt32();Check(n>=0 && n<=100000000,"Author float array");return Enumerable.Range(0,n).Select(_=>reader.ReadSingle()).ToArray();}
        uint[] UInts(){var n=reader.ReadInt32();Check(n>=0 && n<=100000000,"Author index array");return Enumerable.Range(0,n).Select(_=>reader.ReadUInt32()).ToArray();}
        void Strings(){var n=reader.ReadInt32();for(var i=0;i<n;i++)reader.ReadString();}
        void Sources(int n){for(var i=0;i<n;i++){reader.ReadString();reader.ReadString();reader.ReadUInt32();}}
        var vertices=Floats();var normals=Floats();var tangents=Floats();var colors=Floats();var uvs=Enumerable.Range(0,8).Select(_=>Floats()).ToArray();var indices=UInts();
        var subs=reader.ReadInt32();for(var i=0;i<subs;i++)reader.ReadBytes(24);
        var skins=reader.ReadInt32();var weights=new float[skins][];var bones=new uint[skins][];
        for(var i=0;i<skins;i++){weights[i]=Enumerable.Range(0,4).Select(_=>reader.ReadSingle()).ToArray();bones[i]=Enumerable.Range(0,4).Select(_=>reader.ReadUInt32()).ToArray();}
        var bindCount=reader.ReadInt32();var bindposes=Enumerable.Range(0,bindCount).Select(_=>Enumerable.Range(0,16).Select(_=>reader.ReadSingle()).ToArray()).ToArray();var hashes=UInts();
        Strings();Strings();Sources(reader.ReadInt32());var candidateSlots=reader.ReadInt32();for(var i=0;i<candidateSlots;i++)Sources(reader.ReadInt32());
        float[] Vector()=>Enumerable.Range(0,3).Select(_=>reader.ReadSingle()).ToArray();
        var shapeVertices=Enumerable.Range(0,reader.ReadInt32()).Select(_=>new AuthorShapeVertex(reader.ReadUInt32(),Vector(),Vector(),Vector())).ToArray();
        var shapeFrames=Enumerable.Range(0,reader.ReadInt32()).Select(_=>new AuthorShapeFrame(reader.ReadString(),reader.ReadUInt32(),reader.ReadUInt32(),reader.ReadBoolean(),reader.ReadBoolean(),reader.ReadBoolean())).ToArray();
        var shapeChannels=Enumerable.Range(0,reader.ReadInt32()).Select(_=>new AuthorShapeChannel(reader.ReadString(),reader.ReadUInt32(),checked((int)reader.ReadUInt32()),checked((int)reader.ReadUInt32()))).ToArray();
        var shapeWeights=Floats();Check(reader.ReadInt32()==0,"Author additional shape-normal payload unsupported");
        Check(reader.BaseStream.Position==reader.BaseStream.Length && count*3==vertices.Length,"Complete author mesh read");
        return new(vertices,normals,tangents,colors,uvs,indices,weights,bones,bindposes,hashes,shapeVertices,shapeFrames,shapeChannels,shapeWeights);
    }
}

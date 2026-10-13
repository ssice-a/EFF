using System.Security.Cryptography;
using System.Text;
using AnimeStudio.GUI;

internal static class LayerTests
{
    private sealed record Item(string Path, byte[] Payload, bool Present = true);
    private static readonly byte[] Key = Convert.FromHexString("E95B317AC4F828569D23A86BF271DCB53E846FA75C924D671DBA8E38F4CA52E1");
    private static readonly BlcCipher Cipher = typeof(EndfieldVfsArchive).GetMethod("ChaCha20Xor", System.Reflection.BindingFlags.NonPublic | System.Reflection.BindingFlags.Static)!.CreateDelegate<BlcCipher>();
    private static void Check(bool value, string message) { if (!value) throw new InvalidDataException(message); }
    private static void Reject(Action action, string message) { try { action(); } catch (InvalidDataException) { return; } catch (FileNotFoundException) { return; } throw new Exception("Expected rejection: " + message); }
    private static void Block(string root, int version, params Item[] items)
    {
        var dir = Path.Combine(root, "test");Directory.CreateDirectory(dir);
        using var data = new MemoryStream();using var writer = new BinaryWriter(data, Encoding.ASCII, true);
        writer.Write(4);writer.Write(version);var name=Encoding.ASCII.GetBytes("Data/Bundles/Windows/main");writer.Write((ushort)name.Length);writer.Write(name);writer.Write(1UL);
        writer.Write(items.Length);writer.Write(items.Sum(i => (long)i.Payload.Length));writer.Write((byte)0);writer.Write(items.Length);
        foreach (var item in items)
        {
            var digest=MD5.HashData(item.Payload);var chunk=MD5.HashData(Encoding.UTF8.GetBytes(item.Path + version));
            writer.Write(chunk);writer.Write(digest);writer.Write((long)item.Payload.Length);writer.Write((byte)0);writer.Write(0);writer.Write(1);
            var path=Encoding.ASCII.GetBytes("Data/"+item.Path);writer.Write((ushort)path.Length);writer.Write(path);writer.Write(0L);writer.Write(chunk);writer.Write(digest);writer.Write(0L);writer.Write((long)item.Payload.Length);writer.Write((byte)0);writer.Write((byte)0);writer.Write(0);
            if(item.Present) File.WriteAllBytes(Path.Combine(dir,Convert.ToHexString(chunk)+".chk"),item.Payload);
        }
        writer.Write(0);writer.Flush();var plain=data.ToArray();byte[] nonce=new byte[12];Cipher(plain,Key,nonce,1);
        File.WriteAllBytes(Path.Combine(dir,"test.blc"),nonce.Concat(plain).ToArray());
    }
    public static void WriteFixture(string root)
    {
        if(Directory.Exists(root)) throw new IOException("Synthetic fixture already exists");
        var game=Path.Combine(root,"games","Endfield Game");
        Block(Path.Combine(game,"Endfield_Data","StreamingAssets","VFS"),100,new("bundles/same.ab",new byte[]{1,2}),new("bundles/keep.ab",new byte[]{3}),new("bundles/removed.ab",new byte[]{4}),new("bundles/missing.ab",new byte[]{9}));
        Block(Path.Combine(game,"Endfield_Data","Persistent","VFS"),200,new("bundles/same.ab",new byte[]{5,6}),new("bundles/keep.ab",new byte[]{3},false),new("bundles/new.ab",new byte[]{7}),new("bundles/missing.ab",new byte[]{8},false));
        Console.WriteLine(EndfieldVfsArchive.Open(game).Fingerprint);
    }
    public static void Run()
    {
        var root=Path.Combine(Path.GetTempPath(),"EFFVfsLayers-"+Guid.NewGuid().ToString("N"));var game=Path.Combine(root,"games","Endfield Game");
        var shipped=Path.Combine(game,"Endfield_Data","StreamingAssets","VFS");var hot=Path.Combine(game,"Endfield_Data","Persistent","VFS");
        try
        {
            Check(EndfieldVfsArchive.NormalizeLogicalPath("Data/Json/floor+1.json")=="Json/floor+1.json","Punctuation must not truncate logical paths");
            Block(shipped,100,new("bundles/same.ab",new byte[]{1,2}),new("bundles/keep.ab",new byte[]{3}),new("bundles/removed.ab",new byte[]{4}),new("bundles/missing.ab",new byte[]{9}));
            Block(hot,200,new("bundles/same.ab",new byte[]{5,6}),new("bundles/keep.ab",new byte[]{3},false),new("bundles/new.ab",new byte[]{7}),new("bundles/missing.ab",new byte[]{8},false));
            var effective=EndfieldVfsArchive.Open(Path.Combine(root,"games"));
            Check(effective.LayerCount==2&&effective.Entries.Count==4,"Effective block replacement count");
            Check(effective.ReadPayload("bundles/same.ab").SequenceEqual(new byte[]{5,6}),"Updated hot bytes");
            Check(effective.ReadPayload("bundles/keep.ab").Single()==3,"Same-content base fallback");
            Check(effective.ReadPayload("bundles/new.ab").Single()==7,"New hot-only resource");
            Check(!effective.TryGet("bundles/removed.ab",out _),"Removed record must not return from base");
            Check(effective.MissingPayloadCount==1,"Incomplete hot payload is reported");
            Reject(()=>effective.ReadPayload("bundles/missing.ab"),"Never use old different bytes for unavailable new resource");
            Check(EndfieldVfsArchive.Open(shipped).Fingerprint==effective.Fingerprint&&EndfieldVfsArchive.Open(hot).Fingerprint==effective.Fingerprint,"Existing VFS selections include sibling layers");
            var blc=Path.Combine(hot,"test","test.blc");var stamp=File.GetLastWriteTimeUtc(blc);
            Block(hot,200,new("bundles/same.ab",new byte[]{5,0}),new("bundles/keep.ab",new byte[]{3},false),new("bundles/new.ab",new byte[]{7}),new("bundles/missing.ab",new byte[]{8},false));
            File.SetLastWriteTimeUtc(blc,stamp);
            Reject(effective.VerifyCurrent,"Open archive cannot silently use stale metadata");
            Check(EndfieldVfsArchive.Open(game).Fingerprint!=effective.Fingerprint,"BLC content change invalidates identity even with preserved size/time");
            Block(shipped,300,new Item("bundles/same.ab",new byte[]{1,2}));
            Check(EndfieldVfsArchive.Open(game).ReadPayload("bundles/same.ab").SequenceEqual(new byte[]{1,2}),"Older Persistent block cannot override a newer base");
            Block(hot,300,new Item("bundles/same.ab",new byte[]{5,6}));
            Reject(()=>EndfieldVfsArchive.Open(game),"Same-version conflict");
            Directory.CreateDirectory(Path.Combine(root,"games","Other Endfield","Endfield_Data","StreamingAssets","VFS"));
            Reject(()=>EndfieldVfsArchive.Open(Path.Combine(root,"games")),"Multiple installations require exact selection");
            Console.WriteLine("PASS VFS layers: game/parent/VFS selection, new/changed/removed resources, content-verified fallback, unavailable new resource rejection, BLC content identity, obsolete Persistent exclusion, ambiguity rejection");
        }
        finally { if(Directory.Exists(root)) Directory.Delete(root,true); }
    }
}

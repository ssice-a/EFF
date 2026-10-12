param(
    [Parameter(Mandatory=$true)][string]$Game,
    [string]$Bundle = 'main/03c21cd847f08573c23a8c9a.ab',
    [string]$Mods
)
$ErrorActionPreference = 'Stop'
# Read-only diagnostic: inspect the shipped manifest in memory, without
# unpacking bundles or installing a cache. Run with PowerShell 7 / .NET 9.
Add-Type -TypeDefinition @'
using System;
using System.Text;
public static class EiemManifestKeyProbe {
    public static ulong Hash(string value) {
        unchecked {
            ulong a=0x1505,b=a;
            for(int i=0;i<value.Length;i+=2) {
                a=(a*33)^(ulong)char.ToLowerInvariant(value[i]);
                if(i+1<value.Length)b=(b*33)^(ulong)char.ToLowerInvariant(value[i+1]);
            }
            return a+b*0x5D588B65;
        }
    }
    public static void Inspect(byte[] bytes,string[] targets) {
        int p=4,n=BitConverter.ToInt32(bytes,p);p+=4+n*2;
        p+=4;n=BitConverter.ToInt32(bytes,p);p+=4+n*2;
        n=BitConverter.ToInt32(bytes,p);p+=4+n*2;
        n=BitConverter.ToInt32(bytes,p);p+=4+n;
        n=BitConverter.ToInt32(bytes,p);p+=4+n;
        n=BitConverter.ToInt32(bytes,p);
        int array=p+4,data=array+n+4,count=BitConverter.ToInt32(bytes,array);
        var wanted=new System.Collections.Generic.HashSet<string>(targets,StringComparer.Ordinal);
        var found=new System.Collections.Generic.HashSet<string>(StringComparer.Ordinal);
        for(int i=0;i<count;i++) {
            int row=array+4+i*48,at=data+BitConverter.ToInt32(bytes,row+4);
            int len=BitConverter.ToInt32(bytes,at);
            string label=Encoding.Unicode.GetString(bytes,at+4,len);
            if(!wanted.Contains(label))continue;
            ulong key=BitConverter.ToUInt64(bytes,row+24),raw=Hash(label);
            if(targets.Length==1)Console.WriteLine("name="+label+" stored="+key.ToString("X16")+
                " raw="+raw.ToString("X16")+" masked="+(raw&0x0FFFFFFFFFFFFFFF).ToString("X16")+
                " category="+bytes[row+40]);
            if(!found.Add(label))throw new Exception("Duplicate Bundle name: "+label);
        }
        if(found.Count!=wanted.Count)throw new Exception("Referenced Bundles missing: "+string.Join(",",System.Linq.Enumerable.Except(wanted,found)));
        Console.WriteLine("PASS manifest Bundle keys: records="+count+" referenced="+found.Count);
    }
}
'@
$probe = Join-Path $PSScriptRoot '..\EndfieldVfsProbe\bin\Release\net9.0\EndfieldVfsProbe.dll'
$assembly = [Reflection.Assembly]::LoadFrom([IO.Path]::GetFullPath($probe))
$type = $assembly.GetType('AnimeStudio.GUI.EndfieldVfsArchive',$true)
$vfs = [string](Join-Path $Game 'Endfield_Data\StreamingAssets\VFS')
$archive = $type.GetMethod('Open').Invoke($null,@($vfs))
$read = $type.GetMethods() | Where-Object { $_.Name -eq 'ReadPayload' -and $_.IsPublic }
$compressed = $read.Invoke($archive,@('Bundles/Windows/manifest.hgmmap'))
$input = [IO.MemoryStream]::new($compressed,$false)
$brotli = [IO.Compression.BrotliStream]::new($input,[IO.Compression.CompressionMode]::Decompress)
$output = [IO.MemoryStream]::new()
try {
    $brotli.CopyTo($output)
    [string[]]$targets = @($Bundle)
    if($Mods) {
        $reader = Join-Path $PSScriptRoot '..\..\bin\compiled-mod-set\compiled_mod_set_smoke.exe'
        $targets = @(& $reader --bundle-names $Game $Mods)
        if($LASTEXITCODE -ne 0 -or !$targets.Count){throw 'Cannot read structured compiled Bundle references'}
    }
    [EiemManifestKeyProbe]::Inspect($output.ToArray(),$targets)
} finally {
    $output.Dispose();$brotli.Dispose();$input.Dispose()
}

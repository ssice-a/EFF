param(
    [string[]]$Modules = @('LuaScripts/Phase/CharInfo/PhaseCharInfo.lua', 'LuaScripts/Phase/CharFormation/PhaseCharFormation.lua'),
    [string]$Pattern = 'LoadModel|UnloadModel|ChangeLayer|OnAwake|modelLoader|charModel|modelMono',
    [int]$Context = 3,
    [string]$ListPattern = ''
)
# Read-only, targeted script evidence for the inspected installation. No game
# scripts are exported, executed, changed, or included in a release.
$ErrorActionPreference = 'Stop'
$probePath = Join-Path $PSScriptRoot '../EndfieldVfsProbe/bin/Release/net9.0/EndfieldVfsProbe.dll'
$probeAsm = [Reflection.Assembly]::LoadFrom((Resolve-Path -LiteralPath $probePath).Path)
$archiveClass = $probeAsm.GetType('AnimeStudio.GUI.EndfieldVfsArchive')
$archive = $archiveClass.GetMethod('Open').Invoke($null, @('D:/Hypergryph Launcher/games/Endfield Game/Endfield_Data/StreamingAssets/VFS'))
if ($ListPattern) {
    $archive.Entries.Values | Where-Object { $_.LogicalPath -match '^LuaScripts/' -and $_.LogicalPath -match $ListPattern } | Select-Object LogicalPath, Length
    return
}
Add-Type -TypeDefinition @'
using System;
public static class UILuaEvidenceCipher {
    static uint M(uint z, uint y, uint sum, int p, uint e, uint[] key) {
        return unchecked((((z >> 5) ^ (y << 2)) + ((y >> 3) ^ (z << 4))) ^ ((sum ^ y) + (key[(p & 3) ^ e] ^ z)));
    }
    public static byte[] Decrypt(byte[] bytes, byte[] keyBytes) {
        if (bytes.Length < 8 || bytes.Length % 4 != 0) throw new Exception("Invalid XXTEA extent");
        var v = new uint[bytes.Length / 4]; Buffer.BlockCopy(bytes, 0, v, 0, bytes.Length);
        var fixedKey = new byte[16]; Array.Copy(keyBytes, fixedKey, Math.Min(16, keyBytes.Length));
        var key = new uint[4]; Buffer.BlockCopy(fixedKey, 0, key, 0, 16);
        int n = v.Length - 1; uint y = v[0], sum = unchecked((uint)(6 + 52 / v.Length) * 0x9E3779B9U);
        while (sum != 0) {
            uint e = (sum >> 2) & 3;
            for (int p = n; p > 0; --p) { v[p] = unchecked(v[p] - M(v[p - 1], y, sum, p, e, key)); y = v[p]; }
            v[0] = unchecked(v[0] - M(v[n], y, sum, 0, e, key)); y = v[0]; sum = unchecked(sum - 0x9E3779B9U);
        }
        uint length = v[n];
        if (length < n * 4 - 3 || length > n * 4) throw new Exception("Invalid XXTEA length");
        var result = new byte[length]; Buffer.BlockCopy(v, 0, result, 0, (int)length); return result;
    }
}
'@
# LuaCypher.GetKey's inspected native flow: KEYS[1,5,3,2] + "==",
# Base.Beyond.IOOO000iiI.IooiIIO decodes base64 and subtracts the UTF8 root.
$encodedKey = [Convert]::FromBase64String('paekyazWk3ud35+exmF5og==')
$keyMask = [Text.Encoding]::UTF8.GetBytes('Assets/Beyond/InitialAssets/')
$luaKey = [byte[]]::new($encodedKey.Length)
for ($i = 0; $i -lt $luaKey.Length; $i++) { $luaKey[$i] = ($encodedKey[$i] - $keyMask[$i % $keyMask.Length] + 256) % 256 }
foreach ($module in $Modules) {
    $encrypted = [Text.Encoding]::UTF8.GetString($archive.ReadPayload($module))
    $plain = [UILuaEvidenceCipher]::Decrypt([Convert]::FromBase64String($encrypted), $luaKey)
    $lines = [Text.Encoding]::UTF8.GetString($plain) -split '\r?\n'
    Write-Output "$module ($($plain.Length) bytes, $($lines.Length) lines)"
    $selected = [Collections.Generic.SortedSet[int]]::new()
    for ($i = 0; $i -lt $lines.Length; $i++) {
        if ($lines[$i] -match $Pattern) {
            for ($j = [Math]::Max(0, $i - $Context); $j -le [Math]::Min($lines.Length - 1, $i + $Context); $j++) { [void]$selected.Add($j) }
        }
    }
    foreach ($i in $selected) { Write-Output "$($i + 1): $($lines[$i])" }
}

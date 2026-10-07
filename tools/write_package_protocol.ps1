param([Parameter(Mandatory=$true)][string]$Output)
$ErrorActionPreference = 'Stop'
$taskRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$taskHeader = Get-Content (Join-Path $taskRoot 'src/eiem_package_protocol.h') -Raw
function Read-Contract([string]$Name) {
    $match = [regex]::Match($taskHeader, "constexpr const char\* $Name\s*=\s*`"([^`"]+)`"")
    if (-not $match.Success) { throw "Missing protocol field $Name" }
    return $match.Groups[1].Value
}
$compiled = Get-Content (Join-Path $taskRoot 'src/eiem_compiled_mod.h') -Raw
$match = [regex]::Match($compiled, 'constexpr const char\* structure="([^"]+)"')
if (-not $match.Success) { throw 'Missing compiled Mod structure' }
$structures = [ordered]@{}
foreach ($row in @(@('manifest','manifestStructure'), @('author','authorStructure'), @('resource','resourceStructure'), @('current','currentStructure'), @('modSet','setStructure'), @('runtimeRules','ruleStructure'), @('manifestEncoding','ruleEncoding'))) {
    $structures[$row[0]] = Read-Contract $row[1]
}
$structures['manifestInputs'] = @($structures.manifestEncoding, 'self-described-roots/le64', 'legacy-root-markers/le64', 'legacy-root-addresses/le64')
$declaration = [ordered]@{format='EFF_OFFLINE_PACKAGE_PROTOCOL'; compiledMod=$match.Groups[1].Value; structures=$structures}
$declaration | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $Output -Encoding utf8

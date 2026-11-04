[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidatePattern('^v\d+\.\d+\.\d+$')]
    [string]$Tag
)

$ErrorActionPreference = 'Stop'
$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$repository = 'ssice-a/EFF'
$releaseRoot = Join-Path $repoRoot "bin\local-releases\$Tag"
$assets = Join-Path $releaseRoot 'assets'
$remoteRoot = Join-Path $releaseRoot ('.remote-verification-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))

function Invoke-Git([string[]]$Arguments, [string]$Name) {
    Write-Host "==> $Name" -ForegroundColor Cyan
    & git -C $repoRoot @Arguments
    if ($LASTEXITCODE -ne 0) { throw "$Name failed with exit code $LASTEXITCODE" }
}

function Assert-Clean([string]$Path, [string]$Name) {
    $status = @(& git -C $Path status --porcelain --untracked-files=all)
    if ($LASTEXITCODE -ne 0) { throw "Cannot inspect $Name worktree" }
    if ($status.Count -gt 0) { throw "$Name has uncommitted changes; refusing to publish.`n$($status -join "`n")" }
}

function Assert-SubmodulesMatchCommit {
    foreach ($path in @('tools/Blender', 'tools/AnimeStudio', 'EFF-Mod-Manager')) {
        $line = (& git -C $repoRoot ls-tree HEAD $path).Trim()
        if ($LASTEXITCODE -ne 0 -or $line -notmatch ('\s' + [regex]::Escape($path) + '$')) { throw "Missing committed submodule gitlink: $path" }
        $expected = ($line -split '\s+')[2]
        $actual = (& git -C (Join-Path $repoRoot $path) rev-parse HEAD).Trim()
        if ($LASTEXITCODE -ne 0 -or $actual -ne $expected) { throw "$path does not match the local EFF gitlink" }
        Assert-Clean (Join-Path $repoRoot $path) $path
    }
}

function Get-RemoteTagCommit {
    $lines = @(& git -C $repoRoot ls-remote origin "refs/tags/$Tag" "refs/tags/$Tag^{}" 2>$null)
    $exitCode = $LASTEXITCODE
    if ($exitCode -eq 2) { return $null }
    if ($exitCode -ne 0) { throw 'Could not inspect the remote release tag' }
    $peeled = $lines | Where-Object { $_ -match ('\srefs/tags/' + [regex]::Escape($Tag) + '\^\{\}$') } | Select-Object -First 1
    $line = if ($peeled) { $peeled } else { $lines | Where-Object { $_ -match ('\srefs/tags/' + [regex]::Escape($Tag) + '$') } | Select-Object -First 1 }
    if (-not $line) { return $null }
    return ($line -split '\s+')[0]
}

if (-not (Test-Path -LiteralPath $assets -PathType Container)) { throw "Prepared local release assets not found: $assets" }
if (-not (Test-Path -LiteralPath (Join-Path $releaseRoot 'preparation.json') -PathType Leaf)) { throw "Preparation receipt missing for $Tag" }
$receipt = Get-Content -LiteralPath (Join-Path $releaseRoot 'preparation.json') -Raw | ConvertFrom-Json
$head = (& git -C $repoRoot rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0 -or $receipt.tag -ne $Tag -or $receipt.sourceCommit -ne $head) { throw 'Prepared release receipt does not match the current local source' }
if ((& git -C $repoRoot rev-parse "$Tag^{commit}").Trim() -ne $head) { throw "Local tag $Tag does not point at HEAD" }
Assert-Clean $repoRoot 'EFF'
Assert-SubmodulesMatchCommit

$verifyArgs = @('tools/release/verify_release_assets.py', '--directory', $assets, '--tag', $Tag)
& python @verifyArgs
if ($LASTEXITCODE -ne 0) { throw 'Prepared release assets failed local validation' }
if (-not (Test-Path -LiteralPath (Join-Path $repoRoot 'RELEASE_NOTES.md') -PathType Leaf)) { throw 'RELEASE_NOTES.md is missing' }

Invoke-Git @('fetch', 'origin', 'main', '--tags', '--quiet') 'Fetch current remote main and tags'
$ancestorOutput = & git -C $repoRoot merge-base --is-ancestor origin/main HEAD 2>$null
if ($LASTEXITCODE -ne 0) { throw 'Local HEAD is not a fast-forward descendant of origin/main; reconcile locally before publishing' }
$remoteTagCommit = Get-RemoteTagCommit
if ($remoteTagCommit -and $remoteTagCommit -ne $head) { throw "Remote tag $Tag points at a different commit; refusing to move it" }
$existingRelease = & gh release view $Tag --repo $repository --json tagName 2>$null
if ($LASTEXITCODE -eq 0 -and $existingRelease) {
    $releaseState = & gh release view $Tag --repo $repository --json isDraft 2>$null | ConvertFrom-Json
    if (-not $releaseState.isDraft) { throw "GitHub Release $Tag is already published; refusing to replace its assets" }
    $resumeDraft = $true
} else {
    $resumeDraft = $false
}

& gh auth status
if ($LASTEXITCODE -ne 0) { throw 'GitHub CLI is not authenticated' }
Invoke-Git @('push', 'origin', 'HEAD:refs/heads/main') 'Push exact local source commit to remote main'
if (-not $remoteTagCommit) {
    Invoke-Git @('push', 'origin', "refs/tags/$Tag") 'Push local release tag'
}

$runtime = Join-Path $assets "EFF_${Tag}_dll.zip"
$tools = Join-Path $assets "EFF_Shared_Native_Tools_${Tag}.zip"
$manifest = Join-Path $assets 'eff-native-tools.json'
if ($resumeDraft) {
    & gh release upload $Tag $runtime $tools $manifest --repo $repository --clobber
    if ($LASTEXITCODE -ne 0) { throw 'Could not refresh the existing draft with local assets' }
} else {
    $createArgs = @('release', 'create', $Tag, $runtime, $tools, $manifest,
        '--repo', $repository, '--verify-tag', '--draft', '--title', "EFF $Tag",
        '--notes-file', (Join-Path $repoRoot 'RELEASE_NOTES.md'))
    & gh @createArgs
    if ($LASTEXITCODE -ne 0) { throw 'GitHub draft release creation failed; local assets remain ready for retry' }
}

New-Item -ItemType Directory -Force -Path $remoteRoot | Out-Null
$downloadArgs = @('release', 'download', $Tag, '--repo', $repository, '--dir', $remoteRoot,
    '--pattern', ([IO.Path]::GetFileName($runtime)),
    '--pattern', ([IO.Path]::GetFileName($tools)),
    '--pattern', ([IO.Path]::GetFileName($manifest)))
& gh @downloadArgs
if ($LASTEXITCODE -ne 0) { throw 'Could not download the uploaded draft assets for verification; draft remains unpublished' }

$expectedNames = @(([IO.Path]::GetFileName($runtime)), ([IO.Path]::GetFileName($tools)), ([IO.Path]::GetFileName($manifest))) | Sort-Object
$actualNames = @(Get-ChildItem -LiteralPath $remoteRoot -File | Select-Object -ExpandProperty Name | Sort-Object)
if (Compare-Object $expectedNames $actualNames) { throw 'Downloaded draft contains missing or unexpected assets' }
$remoteVerifyArgs = @('tools/release/verify_release_assets.py', '--directory', $remoteRoot, '--tag', $Tag)
& python @remoteVerifyArgs
if ($LASTEXITCODE -ne 0) { throw 'Remote draft assets failed release verification; draft remains unpublished' }
foreach ($name in $expectedNames) {
    $localHash = (Get-FileHash -LiteralPath (Join-Path $assets $name) -Algorithm SHA256).Hash
    $remoteHash = (Get-FileHash -LiteralPath (Join-Path $remoteRoot $name) -Algorithm SHA256).Hash
    if ($localHash -ne $remoteHash) { throw "Uploaded asset differs from local build: $name" }
}

& gh release edit $Tag --repo $repository --draft=false --latest
if ($LASTEXITCODE -ne 0) { throw "Assets verified, but GitHub could not publish draft $Tag" }
Write-Host "Published $Tag from local commit $head. No cloud build or test job was run." -ForegroundColor Green
Write-Host "Assets: $assets" -ForegroundColor Green

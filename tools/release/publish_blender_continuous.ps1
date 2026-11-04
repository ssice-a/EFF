[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$BuildDirectory
)

$ErrorActionPreference = 'Stop'
$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$source = Join-Path $repoRoot 'tools\Blender'
$repository = 'ssice-a/EFF-blender'
$build = [IO.Path]::GetFullPath($BuildDirectory)
$revision = (& git -C $source rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0 -or $revision -notmatch '^[0-9a-f]{40}$') { throw 'Cannot read Blender source revision' }

$archive = Join-Path $build "EFF_Blender_main-$revision.zip"
$feed = Join-Path $build 'eff-blender-update.json'
if (-not (Test-Path -LiteralPath $archive -PathType Leaf) -or -not (Test-Path -LiteralPath $feed -PathType Leaf)) {
    throw "Local continuous package or feed is missing: $build"
}

$status = @(& git -C $source status --porcelain --untracked-files=all)
if ($LASTEXITCODE -ne 0) { throw 'Cannot inspect Blender worktree' }
if ($status.Count -gt 0) { throw "Commit Blender source before publishing its local package.`n$($status -join "`n")" }
& python (Join-Path $PSScriptRoot 'verify_blender_update.py') --archive $archive --feed $feed --revision $revision
if ($LASTEXITCODE -ne 0) { throw 'Local Blender update package failed verification' }

& gh auth status
if ($LASTEXITCODE -ne 0) { throw 'GitHub CLI is not authenticated' }
& git -C $source fetch origin main --quiet
if ($LASTEXITCODE -ne 0) { throw 'Could not fetch Blender remote main' }
& git -C $source merge-base --is-ancestor origin/main HEAD 2>$null
if ($LASTEXITCODE -ne 0) { throw 'Blender HEAD is not a fast-forward descendant of remote main' }
& git -C $source push origin 'HEAD:refs/heads/main'
if ($LASTEXITCODE -ne 0) { throw 'Could not push the verified local Blender source commit to main' }

$existing = & gh release view continuous --repo $repository --json isDraft,isPrerelease 2>$null
if ($LASTEXITCODE -ne 0) {
    & gh release create continuous --repo $repository --target $revision --prerelease --latest=false --title 'Blender continuous updates' --notes 'Locally built and verified add-on packages for Blender repository updates. This is the continuous update channel, not a versioned stable release.'
    if ($LASTEXITCODE -ne 0) { throw 'Could not create Blender continuous update channel' }
} else {
    $channel = $existing | ConvertFrom-Json
    if ($channel.isDraft -or -not $channel.isPrerelease) { throw 'The continuous tag exists but is not a published prerelease channel' }
}

& gh release upload continuous $archive $feed --repo $repository --clobber
if ($LASTEXITCODE -ne 0) { throw 'Could not upload local Blender continuous update assets' }

$verifyRoot = Join-Path $build '.remote-verification'
New-Item -ItemType Directory -Force -Path $verifyRoot | Out-Null
& gh release download continuous --repo $repository --dir $verifyRoot --pattern ([IO.Path]::GetFileName($archive)) --pattern 'eff-blender-update.json'
if ($LASTEXITCODE -ne 0) { throw 'Could not download Blender continuous assets after upload' }
$remoteArchive = Join-Path $verifyRoot ([IO.Path]::GetFileName($archive))
$remoteFeed = Join-Path $verifyRoot 'eff-blender-update.json'
& python (Join-Path $PSScriptRoot 'verify_blender_update.py') --archive $remoteArchive --feed $remoteFeed --revision $revision
if ($LASTEXITCODE -ne 0) { throw 'Uploaded Blender continuous assets failed verification' }
$localHash = (Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash
$remoteHash = (Get-FileHash -LiteralPath $remoteArchive -Algorithm SHA256).Hash
if ($localHash -ne $remoteHash) { throw 'Uploaded Blender update ZIP differs from the local package' }
Write-Host "Published local Blender update $revision to the continuous channel. No cloud package build or regression run was used." -ForegroundColor Green

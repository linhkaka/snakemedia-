param([string]$Output = "dist/SNAKEMEDIA-Studio-windows-x64")
$ErrorActionPreference = 'Stop'
if ($env:GITHUB_REPOSITORY -notmatch '^[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+$') {
    throw 'Set GITHUB_REPOSITORY to the repository publishing this build source before packaging.'
}
$root = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$dist = [IO.Path]::GetFullPath((Join-Path $root $Output))
if (-not $dist.StartsWith($root + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Output must be inside this source checkout.'
}
if (Test-Path -LiteralPath $dist) { throw "Output already exists; choose a new destination: $dist" }
New-Item -ItemType Directory -Path $dist | Out-Null
$built = Join-Path $root 'src/_build'
$exe = Join-Path $built 'snakemedia-studio.exe'
if (-not (Test-Path -LiteralPath $exe)) { throw 'No SNAKEMEDIA executable. Build first.' }
Copy-Item -LiteralPath $exe -Destination $dist
Copy-Item -LiteralPath (Join-Path $built 'corelib.dll') -Destination $dist
$plugins = Join-Path $dist 'plugins'
New-Item -ItemType Directory -Path $plugins | Out-Null
Get-ChildItem -LiteralPath (Join-Path $built 'plugins') -Filter '*.dll' | Copy-Item -Destination $plugins
$binaries = @((Join-Path $dist 'snakemedia-studio.exe'), (Join-Path $dist 'corelib.dll'))
$binaries += @(Get-ChildItem -LiteralPath $plugins -Filter '*.dll' | ForEach-Object FullName)
& windeployqt --release --compiler-runtime --dir $dist @binaries
if ($LASTEXITCODE -ne 0) { throw 'windeployqt failed.' }
if (-not (Test-Path -LiteralPath (Join-Path $dist 'platforms/qwindows.dll'))) { throw 'Qt platform plugin is missing.' }
if (-not (Test-Path -LiteralPath (Join-Path $dist 'sqldrivers/qsqlite.dll'))) { throw 'SQLite plugin is missing.' }
if ($env:OPENSSL_PREFIX) {
    Get-ChildItem (Join-Path $env:OPENSSL_PREFIX 'bin/libcrypto*.dll'),(Join-Path $env:OPENSSL_PREFIX 'bin/libssl*.dll') | Copy-Item -Destination $dist
}
if ($env:VCToolsRedistDir) {
    Get-ChildItem (Join-Path $env:VCToolsRedistDir 'x64/Microsoft.VC143.CRT/*.dll') | Copy-Item -Destination $dist
}
Copy-Item -LiteralPath (Join-Path $root 'LICENSE') -Destination (Join-Path $dist 'LICENSE.txt')
Copy-Item -LiteralPath (Join-Path $root 'NOTICE.txt') -Destination $dist
Copy-Item -LiteralPath (Join-Path $root 'docs/HUONG_DAN_SNAKEMEDIA.txt') -Destination $dist
$licenseDir = Join-Path $dist 'licenses'
New-Item -ItemType Directory -Path $licenseDir | Out-Null
foreach ($entry in @(@('pdfhummus','LICENSE'),@('pdftextextraction','LICENSE'),@('qgumboparser','LICENSE'),@('qtkeychain','COPYING'),@('hunspell','COPYING'))) {
    Copy-Item -LiteralPath (Join-Path $root "src/3rd_party/$($entry[0])/$($entry[1])") -Destination (Join-Path $licenseDir "$($entry[0]).txt")
}
if ($env:QT_ROOT_DIR -and (Test-Path (Join-Path $env:QT_ROOT_DIR 'LICENSES'))) {
    Copy-Item (Join-Path $env:QT_ROOT_DIR 'LICENSES') -Destination (Join-Path $licenseDir 'Qt') -Recurse
}
$repository = $env:GITHUB_REPOSITORY
$commit = (& git -C $root rev-parse HEAD).Trim()
@"
SNAKEMEDIA Studio — modified from Story Architect and Jenny Script.
GPL-3.0; see LICENSE.txt and NOTICE.txt. No warranty.
Source repository: https://github.com/$repository
Exact build commit: $commit
Corresponding source: SNAKEMEDIA-Studio-source.tar.gz in this workflow's source artifact.
The archive includes the initialized public submodules used in the build.
Build workflow: .github/workflows/build_snakemedia.yml
Google OAuth is optional and needs this repository's own client settings.
This build never installs Jenny Script updates.
"@ | Set-Content -LiteralPath (Join-Path $dist 'SOURCE_INFO.txt') -Encoding utf8
Get-ChildItem -LiteralPath $dist -Recurse -File | ForEach-Object {
    $relative = $_.FullName.Substring($dist.Length + 1).Replace('\','/')
    "$( (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant())  $relative"
} | Set-Content -LiteralPath (Join-Path $dist 'SHA256SUMS.txt') -Encoding ascii
Write-Output "Portable folder: $dist"

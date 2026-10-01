<#
Build + package a Sonic the Hedgehog 2 Windows release. Windows counterpart to
tools/build-linux.sh.

Ships ONE windows zip (never a bare exe; the exe is useless without SDL2.dll
and the recomp-ui assets/ next to it):

  release\SonicTheHedgehog2Recomp-windows-x64-v<Version>.zip (+ .sha256)

The version is automatic: CMakeLists.txt derives it from git (tag v0.8.2 ->
0.8.2; off a tag 0.8.2-3-g<sha>; "-dirty" with uncommitted edits). It is the
netplay lobby key and names the zip, so tag the commit to cut a release.

Steps:
  1. configure build-release-windows with the prod flags (no TCP cmd server,
     no observability rings, no chip_trace, no reverse debugger) and rollback
     netplay ON;
  2. build the native target (skip with -NoBuild to package an existing tree);
  3. stage the exe, SDL2.dll, assets/, LICENSE, THIRD-PARTY-LICENSES.md and
     README.txt, then walk the real PE import graph and deploy the MSVC
     runtime app-locally, so the zip starts on a machine without the VC++
     redistributable. Any non-OS import left unresolved fails the release;
  4. hand the allowlist to segagenesisrecomp\tools\package_release.py, which
     refuses ROMs/dumps/saves/build junk and dev-only targets.

The owner ROM must be at game\sonic2.bin: generated C is produced from it at
build time. It is never packaged.

Example:
  powershell -File tools\make_release.ps1
  powershell -File tools\make_release.ps1 -NoBuild
#>
param(
  [string]$BuildDir = 'build-release-windows',
  [switch]$NoBuild,
  [switch]$NoNetplay,
  [int]$Jobs = 0
)

$ErrorActionPreference = 'Stop'
# Windows PowerShell 5.1 turns any native stderr line into a terminating error
# under 'Stop' (cmake warnings, MSBuild notes). Native calls go through
# this wrapper and are judged by their exit code instead.
function Invoke-Native([string]$Exe, [string[]]$Arguments) {
  $prev = $ErrorActionPreference
  $ErrorActionPreference = 'Continue'
  try {
    & $Exe @Arguments 2>&1 | ForEach-Object { "$_" }
  } finally { $ErrorActionPreference = $prev }
}
$root = Split-Path -Parent $PSScriptRoot
$target = 'SonicTheHedgehog2Recomp'

# Engine root resolves exactly as CMakeLists.txt does.
$engine = if ($env:GENESIS_RECOMP_ROOT) { $env:GENESIS_RECOMP_ROOT }
          elseif (Test-Path -LiteralPath (Join-Path $root 'engine-local')) { Join-Path $root 'engine-local' }
          else { Join-Path $root 'segagenesisrecomp' }
foreach ($req in @(
    @((Join-Path $engine 'runner\main.c'), "engine not initialized at $engine; run 'git submodule update --init --recursive'"),
    @((Join-Path $root 'recomp-ui\recomp_ui.cmake'), "recomp-ui is not initialized; run 'git submodule update --init --recursive'"),
    @((Join-Path $root 'game\sonic2.bin'), 'owner ROM missing: place Sonic the Hedgehog 2 (World) (Rev A) at game\sonic2.bin (generation input; never packaged)'),
    @((Join-Path $engine 'tools\package_release.py'), "package_release.py missing under $engine\tools"))) {
  if (-not (Test-Path -LiteralPath $req[0])) { throw $req[1] }
}
if (-not $NoNetplay) {
  foreach ($sub in @('recomp-net', 'rbengine')) {
    if (-not (Test-Path -LiteralPath (Join-Path $engine "external\$sub\CMakeLists.txt"))) {
      throw "netplay needs $engine\external\$sub (git submodule update --init --recursive), or pass -NoNetplay"
    }
  }
}

$build = Join-Path $root $BuildDir

# --- 1-2. configure + build -------------------------------------------------
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path -LiteralPath $vswhere)) { throw "vswhere.exe not found: $vswhere" }
$vs = (& $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath)
if (-not $vs) { throw 'no Visual Studio installation with the x64 C++ toolset found' }
$cmake = Join-Path $vs 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
if (-not (Test-Path -LiteralPath $cmake)) { $cmake = (Get-Command cmake -ErrorAction Stop).Source }

if (-not $NoBuild) {
  # Quote every -D: PowerShell rewrites an unquoted -DX=0.8.2 into "0".
  $cfg = @('-S', $root, '-B', $build, '-G', 'Visual Studio 17 2022', '-A', 'x64',
    '-DSONIC_REVERSE_DEBUG=OFF', '-DGEN_ENABLE_TRACE=OFF', '-DGEN_DEV_TRACE=OFF',
    '-DBUILD_TESTING=OFF',
    "-DGENESISRECOMP_NETPLAY=$(if ($NoNetplay) { 'OFF' } else { 'ON' })")
  if ($env:GENESIS_RECOMP_ROOT) { $cfg += "-DGENESIS_RECOMP_ROOT=$env:GENESIS_RECOMP_ROOT" }
  Write-Host "[1/4] configure $BuildDir"
  Invoke-Native $cmake $cfg | Out-Host
  if ($LASTEXITCODE -ne 0) { throw "configure failed ($LASTEXITCODE)" }
  Write-Host "[2/4] build $target"
  $bld = @('--build', $build, '--config', 'Release', '--target', $target)
  if ($Jobs -gt 0) { $bld += @('--parallel', "$Jobs") }
  Invoke-Native $cmake $bld | Out-Host
  if ($LASTEXITCODE -ne 0) { throw "build failed ($LASTEXITCODE)" }
}

$versionFile = Join-Path $build 'sonic2_version.txt'
if (-not (Test-Path -LiteralPath $versionFile)) { throw "$versionFile missing; configure $BuildDir first (drop -NoBuild)" }
$Version = (Get-Content -LiteralPath $versionFile -TotalCount 1).Trim()   # derived from git by CMakeLists.txt
Write-Host "      version: $Version"

$outDir = Join-Path $build 'Release'
$exe = Join-Path $outDir "$target.exe"
$assets = Join-Path $outDir 'assets'
if (-not (Test-Path -LiteralPath $exe)) { throw "release executable missing: $exe" }
if (-not (Test-Path -LiteralPath $assets)) { throw "recomp-ui launcher assets/ missing: $assets" }

# A netplay exe must carry the version it is named as: it is the lobby key.
$exeText = [Text.Encoding]::ASCII.GetString([IO.File]::ReadAllBytes($exe))
if (-not $NoNetplay -and -not $exeText.Contains($Version)) {
  throw "$exe is not stamped with version '$Version'; rebuild without -NoBuild"
}
$cache = Get-Content -LiteralPath (Join-Path $build 'CMakeCache.txt')
foreach ($want in @('SONIC_REVERSE_DEBUG:BOOL=OFF', 'GEN_ENABLE_TRACE:BOOL=OFF', 'GEN_DEV_TRACE:BOOL=OFF')) {
  if (-not ($cache -contains $want)) { throw "$BuildDir is not a prod configuration (missing $want)" }
}

# --- 3. runtime closure -----------------------------------------------------
Write-Host '[3/4] resolve runtime DLL closure'
function Get-PeImports([string]$Path) {
  $b = [IO.File]::ReadAllBytes($Path)
  if ($b.Length -lt 0x40 -or $b[0] -ne 0x4D -or $b[1] -ne 0x5A) { throw "$Path is not a PE image" }
  $pe = [BitConverter]::ToInt32($b, 0x3C)
  $machine = [BitConverter]::ToUInt16($b, $pe + 4)
  if ($machine -ne 0x8664) { throw ('{0} is not x64 (machine 0x{1:X4})' -f $Path, $machine) }
  $nsec = [BitConverter]::ToUInt16($b, $pe + 6)
  $optSize = [BitConverter]::ToUInt16($b, $pe + 20)
  $opt = $pe + 24
  $dir = $opt + $(if ([BitConverter]::ToUInt16($b, $opt) -eq 0x20B) { 112 } else { 96 })
  $irva = [BitConverter]::ToUInt32($b, $dir + 8)
  if ($irva -eq 0) { return @() }
  $secs = for ($i = 0; $i -lt $nsec; $i++) {
    $s = $opt + $optSize + 40 * $i
    [pscustomobject]@{ VA = [BitConverter]::ToUInt32($b, $s + 12)
      Span = [Math]::Max([BitConverter]::ToUInt32($b, $s + 8), [BitConverter]::ToUInt32($b, $s + 16))
      Raw = [BitConverter]::ToUInt32($b, $s + 20) }
  }
  $toOff = { param($rva) foreach ($s in $secs) { if ($rva -ge $s.VA -and $rva -lt $s.VA + $s.Span) { return [int]($s.Raw + $rva - $s.VA) } }; -1 }
  $d = & $toOff $irva
  $names = @()
  for ($i = 0; $i -lt 4096; $i++) {
    $nrva = [BitConverter]::ToUInt32($b, $d + 20 * $i + 12)
    if ($nrva -eq 0) { break }
    $o = & $toOff $nrva; $e = $o
    while ($b[$e] -ne 0) { $e++ }
    $names += [Text.Encoding]::ASCII.GetString($b, $o, $e - $o)
  }
  return $names
}
# The MSVC runtime lives in System32 only when the VC++ redistributable is
# installed, so it is NOT an OS DLL: deploy it app-locally from the toolset.
$crtDir = Get-ChildItem -Path (Join-Path $vs 'VC\Redist\MSVC\*\x64\Microsoft.VC14*.CRT') -Directory |
  Sort-Object FullName | Select-Object -Last 1
if (-not $crtDir) { throw "no VC\Redist\MSVC\*\x64\Microsoft.VC14*.CRT under $vs" }
$isCrt = { param($n) $n -match '^(msvcp140.*|vcruntime140.*|concrt140|vccorlib140)\.dll$' }
$isOs = { param($n)
  $n -match '^(api-ms-win-|ext-ms-)' -or
  (-not (& $isCrt $n) -and (Test-Path -LiteralPath (Join-Path $env:SystemRoot "System32\$n") -PathType Leaf)) }

$staged = [ordered]@{}   # dll name -> source path
$queue = New-Object Collections.Generic.Queue[string]
$queue.Enqueue($exe)
$seen = New-Object Collections.Generic.HashSet[string] ([StringComparer]::OrdinalIgnoreCase)
while ($queue.Count -gt 0) {
  $pe = $queue.Dequeue()
  foreach ($dep in Get-PeImports $pe) {
    if (-not $seen.Add($dep) -or (& $isOs $dep)) { continue }
    $src = @((Join-Path $outDir $dep), (Join-Path $crtDir.FullName $dep)) |
      Where-Object { Test-Path -LiteralPath $_ -PathType Leaf } | Select-Object -First 1
    if (-not $src) {
      throw "unresolved runtime dependency '$dep' (imported by $([IO.Path]::GetFileName($pe))); searched $outDir and $($crtDir.FullName)"
    }
    $staged[$dep] = $src
    $queue.Enqueue($src)
  }
}
Write-Host "      staged: $($staged.Keys -join ', ')"

# --- 4. package ---------------------------------------------------------------
Write-Host '[4/4] package (compliance allowlist)'
$zip = Join-Path $root "release\$target-windows-x64-v$Version.zip"
if (Test-Path -LiteralPath $zip) { Remove-Item -LiteralPath $zip -Force }
$py = Get-Command py -ErrorAction SilentlyContinue
$pyArgs = @()
if ($py) { $pyArgs += '-3' } else { $py = Get-Command python -ErrorAction Stop }
$pyArgs += @((Join-Path $engine 'tools\package_release.py'),
  '--exe', $exe,
  '--extra') + @($staged.Values) + @(
  '--asset-dir', $assets,
  '--license', (Join-Path $engine 'LICENSE.md'),
  '--notices', (Join-Path $engine 'THIRD-PARTY-LICENSES.md'),
  '--readme', (Join-Path $root 'release\README.txt'),
  '--out', $zip)
Invoke-Native $py.Source $pyArgs | Out-Host
if ($LASTEXITCODE -ne 0) { throw "package_release.py refused the package ($LASTEXITCODE)" }

# package_release.py stores entries by basename / with '/' separators; re-read
# and check the zip carries exactly the resolved closure.
Add-Type -AssemblyName System.IO.Compression.FileSystem
$z = [IO.Compression.ZipFile]::OpenRead($zip)
try {
  $names = @($z.Entries | ForEach-Object FullName)
} finally { $z.Dispose() }
foreach ($need in @("$target.exe", 'LICENSE', 'THIRD-PARTY-LICENSES.md', 'README.txt', 'assets/fonts/LatoLatin-Regular.ttf') + @($staged.Keys)) {
  if (-not ($names -contains $need)) { throw "zip is missing $need" }
}
$bad = @($names | Where-Object { $_.Contains('\') })
if ($bad.Count) { throw "zip has non-portable entry names: $($bad -join ', ')" }

$hash = (Get-FileHash -LiteralPath $zip -Algorithm SHA256).Hash.ToLowerInvariant()
"$hash  $([IO.Path]::GetFileName($zip))" | Out-File -LiteralPath "$zip.sha256" -Encoding ascii
Write-Host "BUILT: $zip"
Write-Host "$hash  $([IO.Path]::GetFileName($zip))"

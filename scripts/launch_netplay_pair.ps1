param(
    [string]$Executable = (Join-Path $PSScriptRoot '..\build-netplay\Release\SonicTheHedgehog2Recomp.exe'),
    [string]$Rom = (Join-Path $PSScriptRoot '..\build-netplay\Release\sonic2.bin'),
    [int]$Frames = 0,
    [int]$InputDelay = 2,
    [ValidateSet('Neutral', 'Campaign', 'Versus')]
    [string]$Scenario = 'Neutral',
    [switch]$Headless
)

$ErrorActionPreference = 'Stop'
$exePath = (Resolve-Path -LiteralPath $Executable).Path
$romPath = (Resolve-Path -LiteralPath $Rom).Path
$runDir = Split-Path -Parent $exePath
$sessionId = [uint32]([DateTimeOffset]::UtcNow.ToUnixTimeSeconds() -band 0x7fffffff)

function Start-NetplayPeer {
    param([int]$Slot, [string]$Bind, [string]$Peer, [string]$LogName,
          [string]$InputScript)

    $info = New-Object System.Diagnostics.ProcessStartInfo
    $info.FileName = $exePath
    $info.WorkingDirectory = $runDir
    $info.UseShellExecute = -not $Headless
    $args = '"{0}" --no-launcher' -f $romPath
    if ($Frames -gt 0) { $args += " --max-frames $Frames" }
    if ($InputScript) { $args += ' --input-script "{0}"' -f $InputScript }
    if ($Headless) {
        $args += ' --hash-frames 60 --hash-on-mode'
        $info.UseShellExecute = $false
        $info.CreateNoWindow = $true
        $info.RedirectStandardOutput = $true
        $info.RedirectStandardError = $true
    }
    $info.Arguments = $args
    $info.EnvironmentVariables['GENESIS_NETPLAY'] = '1'
    $info.EnvironmentVariables['GENESIS_NET_SLOT'] = [string]$Slot
    $info.EnvironmentVariables['GENESIS_NET_INPUT_PLAYER'] = '0'
    $info.EnvironmentVariables['GENESIS_NET_DELAY'] = [string]$InputDelay
    $info.EnvironmentVariables['GENESIS_NET_SESSION_ID'] = [string]$sessionId
    $info.EnvironmentVariables['GENESIS_NET_BIND'] = $Bind
    $info.EnvironmentVariables['GENESIS_NET_PEER'] = $Peer
    $info.EnvironmentVariables['GENESIS_NET_TRANSPORT'] = 'lan'
    $process = [System.Diagnostics.Process]::Start($info)
    return [pscustomobject]@{ Process = $process; LogName = $LogName }
}

$hostInput = ''
$guestInput = ''
if ($Scenario -eq 'Campaign') {
    $hostInput = (Resolve-Path (Join-Path $PSScriptRoot '..\tools\netplay_campaign_host.input')).Path
    $guestInput = (Resolve-Path (Join-Path $PSScriptRoot '..\tools\netplay_campaign_guest.input')).Path
} elseif ($Scenario -eq 'Versus') {
    $hostInput = (Resolve-Path (Join-Path $PSScriptRoot '..\tools\netplay_versus_host.input')).Path
    $guestInput = (Resolve-Path (Join-Path $PSScriptRoot '..\tools\netplay_versus_guest.input')).Path
}

$hostPeer = Start-NetplayPeer -Slot 0 -Bind '127.0.0.1:7777' -Peer '' `
    -LogName 'netplay-host.log' -InputScript $hostInput
Start-Sleep -Milliseconds 200
$guestPeer = Start-NetplayPeer -Slot 1 -Bind '127.0.0.1:7778' -Peer '127.0.0.1:7777' `
    -LogName 'netplay-guest.log' -InputScript $guestInput

Write-Host "Started Sonic 2 netplay pair (session $sessionId)."
Write-Host "Host PID=$($hostPeer.Process.Id), guest PID=$($guestPeer.Process.Id)"

if ($Headless) {
    $logs = @{}
    foreach ($peer in @($hostPeer, $guestPeer)) {
        $stdout = $peer.Process.StandardOutput.ReadToEnd()
        $stderr = $peer.Process.StandardError.ReadToEnd()
        $peer.Process.WaitForExit()
        $logPath = Join-Path $runDir $peer.LogName
        [IO.File]::WriteAllText($logPath, $stdout + $stderr)
        $logs[$peer.LogName] = $stdout + $stderr
        if ($peer.Process.ExitCode -ne 0) {
            throw "$($peer.LogName) exited with code $($peer.Process.ExitCode); see $logPath"
        }
        Write-Host "$($peer.LogName): exit 0"
    }
    $hostHashes = [regex]::Matches($logs['netplay-host.log'], '(?m)^\[FBHASH\].*$') |
        ForEach-Object { $_.Value }
    $guestHashes = [regex]::Matches($logs['netplay-guest.log'], '(?m)^\[FBHASH\].*$') |
        ForEach-Object { $_.Value }
    if (($hostHashes -join "`n") -ne ($guestHashes -join "`n")) {
        throw 'Peer framebuffer hashes diverged; inspect netplay-host.log and netplay-guest.log'
    }
    if ($Scenario -eq 'Campaign' -and
        $logs['netplay-host.log'] -notmatch 'mode=0x0C') {
        throw 'Campaign scenario did not reach gameplay mode 0x0C'
    }
    if ($Scenario -eq 'Versus' -and
        $logs['netplay-host.log'] -notmatch 'h=448') {
        throw 'Versus scenario did not reach the native 448-line split-screen output'
    }
    Write-Host "Peer hashes match at $($hostHashes.Count) checkpoints."
}

# Build first. Two real processes verify shield impacts with simulated latency/jitter.
param(
    [string]$EngineRoot = "$env:USERPROFILE\Epic Games\UE_5.8",
    [ValidateRange(0,500)][int]$LagMs = 80,
    [ValidateRange(0,200)][int]$JitterMs = 20,
    [ValidateRange(1024,65533)][int]$Port = 17977
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$editor = Join-Path $EngineRoot 'Engine/Binaries/Win64/UnrealEditor.exe'
$project = Join-Path $projectRoot 'Shieldbreak.uproject'
$logs = Join-Path $projectRoot 'Saved/Logs'
New-Item -ItemType Directory -Force -Path $logs | Out-Null
$hostLog = Join-Path $logs 'ShieldFeedback-Server.log'
$clientLog = Join-Path $logs 'ShieldFeedback-Client.log'
foreach ($path in @($hostLog,$clientLog)) { if (Test-Path -LiteralPath $path) { Remove-Item -LiteralPath $path } }
if (Get-NetUDPEndpoint -LocalPort $Port -ErrorAction SilentlyContinue) { throw "UDP port $Port is occupied." }
$server = $null
$client = $null
function Wait-Marker($process, [string]$path, [string]$pattern) {
    $deadline = (Get-Date).AddMinutes(3)
    do {
        $contents = if (Test-Path -LiteralPath $path) { Get-Content -Raw -LiteralPath $path } else { '' }
        if ($contents -match $pattern) { return $contents }
        if ($process.HasExited) { throw "Process exited before marker '$pattern': $path" }
        if ((Get-Date) -gt $deadline) { throw "Timeout waiting for '$pattern': $path" }
        Start-Sleep -Seconds 1
    } while ($true)
}
try {
    $common = "-game -nullrhi -nosound -unattended -SBShieldFeedbackTest -ExecCmds=`"NetEmulation.PktLag $LagMs,NetEmulation.PktLagVariance $JitterMs`""
    $arguments = "`"$project`" /Engine/Maps/Entry?listen?Map=Keel?Mode=Slayer?Teams=1?Bots=0 -port=$Port $common -abslog=`"$hostLog`""
    $server = Start-Process $editor -ArgumentList $arguments -WindowStyle Hidden -PassThru
    [void](Wait-Marker $server $hostLog 'Game Engine Initialized')
    $arguments = "`"$project`" 127.0.0.1:$Port $common -abslog=`"$clientLog`""
    $client = Start-Process $editor -ArgumentList $arguments -WindowStyle Hidden -PassThru
    [void](Wait-Marker $client $clientLog 'ShieldFeedback: client ready\.')
    [void](Wait-Marker $server $hostLog 'ShieldFeedback: probe completed')
    [void](Wait-Marker $client $clientLog 'ShieldFeedback: impact=4')
    Start-Sleep -Seconds 2
    foreach ($path in @($hostLog,$clientLog)) {
        $contents = Get-Content -Raw -LiteralPath $path
        $impacts = [regex]::Matches($contents, 'ShieldFeedback: impact=(\d+) absorbed=([\d.]+) broken=(\d) overshield=(\d) role=(\d)')
        if ($impacts.Count -ne 4) { throw "Expected four absorbed hits, not health-only/decay events: $path ($($impacts.Count))" }
        $expected = @('1,20.0,0,0','2,20.0,0,0','3,30.0,1,0','4,1.0,0,1')
        for ($index=0; $index -lt 4; $index++) {
            $hit = $impacts[$index]
            $actual = (1..4 | ForEach-Object { $hit.Groups[$_].Value }) -join ','
            if ($actual -ne $expected[$index]) { throw "Unexpected shield event: $actual in $path" }
            $expectedRole = if ($path -eq $hostLog) { '3' } else { '1' }
            if ($hit.Groups[5].Value -ne $expectedRole) { throw "Shield event was not received on the expected network role: $path" }
        }
        if ($contents -notmatch "PktLag set to $LagMs" -or $contents -notmatch "PktLagVariance set to $JitterMs") {
            throw "Network emulation was not confirmed in $path"
        }
        if ($contents -match 'Fatal error:|Assertion failed:|Ensure condition failed:') { throw "Unreal failure in $path" }
    }
    Write-Output "PASS: host and remote client received all four shield impacts, including break and overshield; health-only damage and passive decay produced none."
    Write-Output "Configured packet lag $LagMs ms and variance $JitterMs ms on both processes; this is a local emulation, not an internet test."
    Write-Output "Reports: $hostLog ; $clientLog"
}
finally {
    foreach ($process in @($client,$server)) { if ($process -and !$process.HasExited) { Stop-Process -Id $process.Id } }
}

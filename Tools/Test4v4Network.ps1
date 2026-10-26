# Build first. Real local listen-server/client regression, with rendering disabled for this 8 GB PC.
param([string]$EngineRoot = "$env:USERPROFILE\Epic Games\UE_5.8")
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$projectFile = Join-Path $projectRoot 'Shieldbreak.uproject'
$editorExe = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor.exe'
$logRoot = Join-Path $projectRoot 'Saved\Logs'
New-Item -ItemType Directory -Force -Path $logRoot | Out-Null
$serverLog = Join-Path $logRoot '4v4Network-Server.log'
$clientLog = Join-Path $logRoot '4v4Network-Client.log'
foreach ($testLog in @($serverLog, $clientLog)) {
    if (Test-Path -LiteralPath $testLog) { Remove-Item -LiteralPath $testLog }
}
$serverProcess = $null
$clientProcess = $null
function Read-ServerLog {
    if (Test-Path -LiteralPath $serverLog) { return Get-Content -Raw -LiteralPath $serverLog }
    return ''
}
$fullHostRoster = 'NetTest roster humans=1 bots=7 red=4 blue=4 total=8 limit=8 requested=16 skill=2 map=Keel mode=0'
$joinedRoster = 'NetTest roster humans=2 bots=6 red=4 blue=4 total=8 limit=8 requested=16 skill=2 map=Keel mode=0'
try {
    # Deliberately request a larger URL limit; server rules must still enforce eight.
    $serverArgs = "`"$projectFile`" /Engine/Maps/Entry?listen?Map=Keel?Mode=Slayer?Teams=1?Bots=7?MaxPlayers=16 -game -nullrhi -nosound -unattended -SBNetworkTest -abslog=`"$serverLog`""
    $serverProcess = Start-Process $editorExe -ArgumentList $serverArgs -WindowStyle Hidden -PassThru
    Write-Output "Starting 4v4 server $($serverProcess.Id)"
    $readyDeadline = (Get-Date).AddMinutes(5)
    do {
        Start-Sleep -Seconds 2
        if ($serverProcess.HasExited) { throw 'Server exited during startup.' }
        $serverText = Read-ServerLog
        if ((Get-Date) -gt $readyDeadline) { throw 'Server never reached a balanced eight-slot host/bot roster.' }
    } until ($serverText -match $fullHostRoster)
    Write-Output 'PASS: host controls accepted; oversized URL/bot requests remain capped to balanced 4v4.'
    $clientArgs = "`"$projectFile`" 127.0.0.1 -game -nullrhi -nosound -unattended -SBNetworkTest -SBAutoShot=24 -SBAutoShotCount=1 -SBAutoShotInterval=1 -SBAutoQuit -abslog=`"$clientLog`""
    $clientProcess = Start-Process $editorExe -ArgumentList $clientArgs -WindowStyle Hidden -PassThru
    Write-Output "Joining client $($clientProcess.Id)"
    if (!$clientProcess.WaitForExit(300000)) { throw 'Client join/probe run timed out.' }
    if ($clientProcess.ExitCode -ne 0) { throw "Client exit code $($clientProcess.ExitCode)" }
    $clientText = Get-Content -Raw -LiteralPath $clientLog
    $serverText = Read-ServerLog
    if ($clientText -notmatch 'Welcomed by server' -or $serverText -notmatch 'Join succeeded:') { throw 'Network join did not complete.' }
    if ($clientText -notmatch 'NetTest: sending remote match-control probes.') { throw 'Client did not send its probes.' }
    foreach ($command in @('SB_Bots', 'SB_Skill', 'SB_Map')) {
        if ($serverText -notmatch "Rejected remote match control: $command") { throw "Server did not reject $command from the remote client." }
    }
    Write-Output 'PASS: remote bot-count, difficulty and map-change requests were rejected on the server.'
    $joinSnapshot = [regex]::Match($serverText, $joinedRoster)
    if (!$joinSnapshot.Success) { throw 'Human arrival did not replace a bot while preserving four players per side.' }
    Write-Output 'PASS: joining human replaced a bot (2 humans + 6 bots, 4 per team).'
    $disconnectDeadline = (Get-Date).AddSeconds(40)
    do {
        Start-Sleep -Seconds 2
        $serverText = Read-ServerLog
        $refilled = [regex]::Matches($serverText, $fullHostRoster) | Where-Object { $_.Index -gt $joinSnapshot.Index }
        if ((Get-Date) -gt $disconnectDeadline) { throw 'Bot slot did not refill after the client disconnected.' }
    } until ($refilled)
    Write-Output 'PASS: disconnect refilled the bot slot (1 human + 7 bots, 4 per team).'
    $rosters = [regex]::Matches($serverText, 'NetTest roster humans=\d+ bots=\d+ red=(\d+) blue=(\d+) total=(\d+) limit=(\d+) requested=\d+ skill=(\d+) map=(\w+) mode=(\d+)')
    foreach ($snapshot in $rosters) {
        if ([int]$snapshot.Groups[1].Value -gt 4 -or [int]$snapshot.Groups[2].Value -gt 4 -or [int]$snapshot.Groups[3].Value -gt 8) { throw 'Roster exceeded 4v4 capacity.' }
        if ($snapshot.Groups[4].Value -ne '8' -or $snapshot.Groups[5].Value -ne '2' -or $snapshot.Groups[6].Value -ne 'Keel' -or $snapshot.Groups[7].Value -ne '0') { throw 'Host settings or player limit changed during the test.' }
    }
    $bothLogs = $serverText + $clientText
    if ($bothLogs -match 'Fatal error:|Assertion failed:|Ensure condition failed:|SupportsObject:.*ShieldbreakArena.*NOT Supported|Stably named sub-object not found') { throw 'Unreal assertion or arena-reference regression detected.' }
    Write-Output "PASS: all $($rosters.Count) roster samples stayed within 4v4 capacity and preserved host settings."
    Write-Output "Reports: $serverLog ; $clientLog"
}
finally {
    foreach ($testProcess in @($clientProcess, $serverProcess)) {
        if ($testProcess -and !$testProcess.HasExited) { Stop-Process -Id $testProcess.Id }
    }
}

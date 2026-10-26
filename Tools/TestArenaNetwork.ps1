# Build the editor target first. Runs a real listen server and joining client without rendering.
# Headless execution keeps this regression usable on low-memory development PCs.
param(
    [string]$Map = 'Keel',
    [string]$EngineRoot = "$env:USERPROFILE\Epic Games\UE_5.8"
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$projectFile = Join-Path $projectRoot 'Shieldbreak.uproject'
$editorExe = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor.exe'
$logRoot = Join-Path $projectRoot 'Saved\Logs'
if ($Map -notin @('Keel', 'Bastion', 'Sanctum', 'Creekbed', 'Spire', 'Crucible')) {
    throw "Unknown map: $Map"
}
New-Item -ItemType Directory -Force -Path $logRoot | Out-Null
$serverLog = Join-Path $logRoot "ArenaNetwork-$Map-Server.log"
$clientLog = Join-Path $logRoot "ArenaNetwork-$Map-Client.log"
foreach ($testLog in @($serverLog, $clientLog)) {
    if (Test-Path -LiteralPath $testLog) { Remove-Item -LiteralPath $testLog }
}
$serverProcess = $null
$clientProcess = $null
try {
    $serverArgs = "`"$projectFile`" /Engine/Maps/Entry?listen?Map=${Map}?Bots=1 -game -nullrhi -nosound -unattended -SBAutoShot=180 -SBAutoShotCount=1 -SBAutoShotInterval=1 -SBAutoQuit -abslog=`"$serverLog`""
    $serverProcess = Start-Process $editorExe -ArgumentList $serverArgs -WindowStyle Hidden -PassThru
    $readyDeadline = (Get-Date).AddMinutes(4)
    do {
        Start-Sleep -Seconds 2
        $ready = (Test-Path $serverLog) -and (Select-String $serverLog -Pattern 'Game Engine Initialized' -Quiet)
        if ($serverProcess.HasExited) { throw 'Listen server exited before accepting a client.' }
        if ((Get-Date) -gt $readyDeadline) { throw 'Listen server startup timed out.' }
    } until ($ready)
    $clientArgs = "`"$projectFile`" 127.0.0.1 -game -nullrhi -nosound -unattended -SBAutoShot=30 -SBAutoShotCount=1 -SBAutoShotInterval=1 -SBAutoQuit -abslog=`"$clientLog`""
    $clientProcess = Start-Process $editorExe -ArgumentList $clientArgs -WindowStyle Hidden -PassThru
    Write-Output "Testing $Map (server $($serverProcess.Id), client $($clientProcess.Id))"
    if (!$clientProcess.WaitForExit(240000)) { throw 'Client test timed out.' }
    if ($clientProcess.ExitCode -ne 0) { throw "Client exited with $($clientProcess.ExitCode)." }
    $serverText = Get-Content -Raw $serverLog
    $clientText = Get-Content -Raw $clientLog
    if ($serverText -notmatch 'Join succeeded:' -or $clientText -notmatch 'Welcomed by server') {
        throw 'Client did not complete a network join.'
    }
    if ($clientText -notmatch "Built map $Map " -or $serverText -notmatch "Built map $Map ") {
        throw 'Server and client did not build the requested map.'
    }
    if ($clientText -notmatch 'AutoShot: requested') { throw 'Client did not finish its in-game observation period.' }
    $badReferences = 'SupportsObject:.*ShieldbreakArena.*NOT Supported|GetObjectFromNetGUID.*(Warning|Failed)|Unable to resolve.*(ShieldbreakArena|Piece_)|Stably named sub-object not found'
    if ($serverText -match $badReferences -or $clientText -match $badReferences) {
        throw 'Arena network reference regression detected. Inspect the server/client logs.'
    }
    if (($serverText + $clientText) -match 'Fatal error:|Assertion failed:|Ensure condition failed:') {
        throw 'An Unreal assertion occurred. Inspect the server/client logs.'
    }
    Write-Output "PASS: $Map client joined, built the same map, and ran for 30 game seconds without arena reference errors."
    Write-Output "Logs: $serverLog ; $clientLog"
}
finally {
    # Only stop the two processes started by this test, never another editor or game.
    foreach ($testProcess in @($clientProcess, $serverProcess)) {
        if ($testProcess -and !$testProcess.HasExited) { Stop-Process -Id $testProcess.Id }
    }
}

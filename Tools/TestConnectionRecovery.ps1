# Build first. Real local connection-loss, failed-join and retry regressions; no rendering.
param(
    [string]$EngineRoot = "$env:USERPROFILE\Epic Games\UE_5.8",
    [ValidateRange(1024, 65533)][int]$Port = 17877
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$projectFile = Join-Path $projectRoot 'Shieldbreak.uproject'
$editorExe = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor.exe'
if (!(Test-Path -LiteralPath $editorExe)) { throw "Editor not found: $editorExe" }
$logRoot = Join-Path $projectRoot 'Saved\Logs'
New-Item -ItemType Directory -Force -Path $logRoot | Out-Null
$hostLog = Join-Path $logRoot 'ConnectionRecovery-HostLoss-Server.log'
$lossLog = Join-Path $logRoot 'ConnectionRecovery-HostLoss-Client.log'
$retryHostLog = Join-Path $logRoot 'ConnectionRecovery-Retry-Server.log'
$retryLog = Join-Path $logRoot 'ConnectionRecovery-Retry-Client.log'
foreach ($testLog in @($hostLog, $lossLog, $retryHostLog, $retryLog)) {
    if (Test-Path -LiteralPath $testLog) { Remove-Item -LiteralPath $testLog }
}
$serverProcess = $null
$clientProcess = $null
$failedPort = $Port + 1
$matchAddress = "127.0.0.1:$Port"
$failedAddress = "127.0.0.1:$failedPort"
$commonArgs = '-game -nullrhi -nosound -unattended'
$fatalPattern = 'ConnectionTest FAIL:|Fatal error:|Assertion failed:|Ensure condition failed:'
function Read-TestLog([string]$Path) {
    if (Test-Path -LiteralPath $Path) { return Get-Content -Raw -LiteralPath $Path }
    return ''
}
function Wait-TestMarker($Process, [string]$Path, [string]$Pattern, [int]$Seconds = 180) {
    $deadline = (Get-Date).AddSeconds($Seconds)
    do {
        $contents = Read-TestLog $Path
        if ($contents -match $fatalPattern) { throw "Regression error in $Path" }
        if ($contents -match $Pattern) { return $contents }
        if ($Process.HasExited) { throw "Process $($Process.Id) exited before '$Pattern'. Log: $Path" }
        if ((Get-Date) -gt $deadline) { throw "Timed out waiting for '$Pattern'. Log: $Path" }
        Start-Sleep -Seconds 2
    } while ($true)
}
function Start-TestServer([string]$Path) {
    $arguments = "`"$projectFile`" /Engine/Maps/Entry?listen?Map=Keel?Mode=Slayer?Teams=1?Bots=0 -port=$Port $commonArgs -abslog=`"$Path`""
    $process = Start-Process $editorExe -ArgumentList $arguments -WindowStyle Hidden -PassThru
    try { [void](Wait-TestMarker $process $Path 'Game Engine Initialized' 240) }
    catch {
        if (!$process.HasExited) { Stop-Process -Id $process.Id }
        throw
    }
    return $process
}
try {
    # Refuse to collide with an existing local server, including the deliberately unavailable endpoint.
    foreach ($testPort in @($Port, $failedPort)) {
        if (Get-NetUDPEndpoint -LocalPort $testPort -ErrorAction SilentlyContinue) { throw "UDP port $testPort is already in use." }
    }
    $serverProcess = Start-TestServer $hostLog
    $clientArgs = "`"$projectFile`" $matchAddress $commonArgs -SBConnectionTest=HostLoss -abslog=`"$lossLog`""
    $clientProcess = Start-Process $editorExe -ArgumentList $clientArgs -WindowStyle Hidden -PassThru
    [void](Wait-TestMarker $clientProcess $lossLog 'ConnectionTest: HostLoss joined\.')
    if ((Read-TestLog $hostLog) -notmatch 'Join succeeded:') { throw 'Host-loss client did not complete a server join.' }
    Stop-Process -Id $serverProcess.Id
    $serverProcess = $null
    $lossText = Wait-TestMarker $clientProcess $lossLog 'ConnectionTest PASS: HostLoss recovered frontend retry=1\.'
    if (!$clientProcess.WaitForExit(30000) -or $clientProcess.ExitCode -ne 0) { throw 'Recovered host-loss client did not exit cleanly.' }
    $clientProcess = $null
    Write-Output 'PASS: a real joining client returned to the front-end retry screen after its local host stopped.'

    $serverProcess = Start-TestServer $retryHostLog
    $timeoutOverride = '-ini:Engine:[/Script/OnlineSubsystemUtils.IpNetDriver]:InitialConnectTimeout=5.0'
    $retryArgs = "`"$projectFile`" $commonArgs $timeoutOverride -SBConnectionTest=Retry -SBFailedAddress=$failedAddress -SBRetryAddress=$matchAddress -abslog=`"$retryLog`""
    $clientProcess = Start-Process $editorExe -ArgumentList $retryArgs -WindowStyle Hidden -PassThru
    $retryText = Wait-TestMarker $clientProcess $retryLog 'ConnectionTest PASS: Retry joined match with no stale error screen\.'
    if ($retryText -notmatch 'ConnectionTest PASS: FailedJoin recovered frontend retry=1 retained=') { throw 'Failed join did not verify retained-address recovery.' }
    if ($retryText -notmatch 'Welcomed by server' -or (Read-TestLog $retryHostLog) -notmatch 'Join succeeded:') { throw 'Retry did not complete a real local join.' }
    if (!$clientProcess.WaitForExit(30000) -or $clientProcess.ExitCode -ne 0) { throw 'Retry client did not exit cleanly.' }
    $clientProcess = $null
    foreach ($entry in @(@($retryText, 'attempting unavailable address'), @($retryText, 'retrying address'))) {
        if ([regex]::Matches($entry[0], $entry[1]).Count -ne 1) { throw "Connection attempt duplicated: $($entry[1])" }
    }
    if (((Read-TestLog $hostLog) + (Read-TestLog $retryHostLog) + $lossText + $retryText) -match $fatalPattern) { throw 'A connection regression or Unreal assertion was logged.' }
    Write-Output 'PASS: a failed join retained its address and restored retry; one subsequent attempt joined a real local host without a stale error screen.'
    Write-Output "Reports: $hostLog ; $lossLog ; $retryHostLog ; $retryLog"
}
finally {
    # Only stop processes owned by this test, never another editor/game or unrelated listener.
    foreach ($testProcess in @($clientProcess, $serverProcess)) {
        if ($testProcess -and !$testProcess.HasExited) { Stop-Process -Id $testProcess.Id }
    }
}

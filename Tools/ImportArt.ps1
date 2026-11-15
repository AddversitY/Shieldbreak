# LOD reduction needs editor subsystems, which Python commandlets do not initialize.
param([string]$Folders = '')
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$exe = Join-Path $env:USERPROFILE 'Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$project = Join-Path $root 'Shieldbreak.uproject'
$script = Join-Path $PSScriptRoot 'import_art.py'
$log = Join-Path $root 'Saved\Logs\ImportArt.log'
$report = Join-Path $root 'Saved\Logs\ImportArt.txt'
New-Item -ItemType Directory -Force (Split-Path $log -Parent) | Out-Null
if (Test-Path -LiteralPath $report) { Remove-Item -LiteralPath $report }
$previousFolders = $env:SB_IMPORT_FOLDERS
try {
    $env:SB_IMPORT_FOLDERS = $Folders
    $arguments = "`"$project`" /Engine/Maps/Entry -ExecutePythonScript=`"$script`" -nullrhi -nosound -unattended -nosplash -abslog=`"$log`""
    $process = Start-Process $exe -ArgumentList $arguments -WindowStyle Hidden -PassThru -Wait
    if ($process.ExitCode -ne 0) { throw "Character import exited with $($process.ExitCode). See $log" }
    if (!(Test-Path -LiteralPath $report)) { throw "Character import did not finish its report. See $log" }
    $text = Get-Content -Raw -LiteralPath $report
    $expected = @(Get-ChildItem (Join-Path $root 'Art\Export\Armor'), (Join-Path $root 'Art\Export\Weapons') -Filter '*.fbx').Count
    $actual = [regex]::Matches($text, 'LODs 3 ').Count
    if ($actual -ne $expected -or $text -match 'FAILED|unmatched slots') {
        throw "Character import validation failed: $actual of $expected meshes have three LODs. See $report"
    }
    Write-Output "Art import PASS: $actual weapon/armor meshes, three LODs each, all slots matched."
} finally {
    $env:SB_IMPORT_FOLDERS = $previousFolders
}

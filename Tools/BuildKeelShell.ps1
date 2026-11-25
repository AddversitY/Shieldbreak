# Author Keel's shell kit (Art/blender/build_keel_shell_kit.py) and optionally its preview renders. Import with ImportKeelShell.bat.
param([string]$BlenderExe = '', [switch]$Render)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
if (!$BlenderExe) { $BlenderExe = Join-Path $projectRoot 'Saved/Tools/Blender/blender-4.5.15-windows-x64/blender.exe' }
if (!(Test-Path -LiteralPath $BlenderExe)) { throw 'Blender was not found. Pass -BlenderExe with your local executable path; see docs/DESKTOP_SETUP.md.' }
$logRoot = Join-Path $projectRoot 'Saved/Logs'
New-Item -ItemType Directory -Path $logRoot -Force | Out-Null
$extra = @()
if ($Render) { $extra = @('--', '--render') }
& $BlenderExe --background --factory-startup --python-exit-code 1 --python (Join-Path $projectRoot 'Art/blender/build_keel_shell_kit.py') @extra *> (Join-Path $logRoot 'KeelShellBuild.log')
if ($LASTEXITCODE -ne 0) { throw "Keel shell kit authoring failed ($LASTEXITCODE). See Saved/Logs/KeelShellBuild.log." }
Get-Content (Join-Path $projectRoot 'Art/Export/MapKit/keel_shell_report.txt')

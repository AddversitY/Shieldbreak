# Author the original Spire condenser. Import with ImportSpireHero.bat afterward.
param([string]$BlenderExe = '')
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
if (!$BlenderExe) { $BlenderExe = Join-Path $projectRoot 'Saved/Tools/Blender/blender-4.5.15-windows-x64/blender.exe' }
if (!(Test-Path -LiteralPath $BlenderExe)) { throw 'Blender was not found. Pass -BlenderExe with your local executable path; see docs/DESKTOP_SETUP.md.' }
$logRoot = Join-Path $projectRoot 'Saved/Logs'
New-Item -ItemType Directory -Path $logRoot -Force | Out-Null
& $BlenderExe --background --factory-startup --python-exit-code 1 --python (Join-Path $projectRoot 'Art/blender/build_spire_hero.py') *> (Join-Path $logRoot 'SpireHeroBuild.log')
if ($LASTEXITCODE -ne 0) { throw "Spire authoring failed ($LASTEXITCODE). See Saved/Logs/SpireHeroBuild.log." }
Get-Content (Join-Path $projectRoot 'Art/Export/MapKit/spire_hero_report.txt')

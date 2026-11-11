@echo off
rem Writes the grenade pass asset facts (toss hand path, textures, materials), headless.
rem Results: Saved\Logs\GrenadeAssets.txt (log: Saved\Logs\InspectGrenadeAssets.log)
setlocal
set "PROJ=%~dp0..\Shieldbreak.uproject"
set "UE=%USERPROFILE%\Epic Games\UE_5.8"
if not exist "%~dp0..\Saved\Logs" mkdir "%~dp0..\Saved\Logs"
"%UE%\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "%PROJ%" -run=pythonscript -script="%~dp0inspect_grenade_assets.py" -unattended -nosplash -nullrhi -stdout > "%~dp0..\Saved\Logs\InspectGrenadeAssets.log" 2>&1
echo EXITCODE=%ERRORLEVEL% >> "%~dp0..\Saved\Logs\InspectGrenadeAssets.log"
endlocal

@echo off
rem Keel brain tank (Art/blender/build_keel_brain_tank.py): narrow import of SM_SB_Kit_KeelBrainTank*, then its engine
rem materials (wet brain, glass, fluid haze), slot assignment and Nanite (Tools/create_keel_brain_tank.py).
rem Logs: Saved\Logs\KeelBrainTankImport.log, Saved\Logs\KeelBrainTank.txt
setlocal
set "UE=%USERPROFILE%\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
set "PROJ=%~dp0..\Shieldbreak.uproject"
if not exist "%~dp0..\Saved\Logs" mkdir "%~dp0..\Saved\Logs"
"%UE%" "%PROJ%" -run=pythonscript -script="%~dp0import_map_art.py" -SBMapArtPrefix=SM_SB_Kit_KeelBrainTank -unattended -nosplash -nullrhi -stdout > "%~dp0..\Saved\Logs\KeelBrainTankImport.log" 2>&1
if errorlevel 1 exit /b %ERRORLEVEL%
"%UE%" "%PROJ%" -run=pythonscript -script="%~dp0create_keel_brain_tank.py" -unattended -nosplash -nullrhi -stdout > "%~dp0..\Saved\Logs\KeelBrainTankMaterials.log" 2>&1
exit /b %ERRORLEVEL%

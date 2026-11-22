@echo off
rem Narrow import of Keel's option B modules (Art/blender/build_keel_columns.py): column shrouds
rem (SM_SB_Kit_AlienColumn_*), door portals (SM_SB_Kit_AlienPortal_*) window frames (SM_SB_Kit_AlienWindow*) cover (SM_SB_Kit_AlienPlinth_*) the Cradle pod (SM_SB_Kit_AlienPod) and ramp edges (SM_SB_Kit_AlienRampEdge).
rem Author first: Saved\Tools\Blender\...\blender.exe --background --factory-startup --python Art\blender\build_keel_columns.py
setlocal
set "UE=%USERPROFILE%\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
set "PROJ=%~dp0..\Shieldbreak.uproject"
if not exist "%~dp0..\Saved\Logs" mkdir "%~dp0..\Saved\Logs"
for %%P in (AlienColumn AlienPortal AlienWindow AlienPlinth AlienPod AlienRampEdge) do (
  "%UE%" "%PROJ%" -run=pythonscript -script="%~dp0import_map_art.py" -SBMapArtPrefix=SM_SB_Kit_%%P -unattended -nosplash -nullrhi -stdout > "%~dp0..\Saved\Logs\Keel%%PImport.log" 2>&1
)
exit /b %ERRORLEVEL%

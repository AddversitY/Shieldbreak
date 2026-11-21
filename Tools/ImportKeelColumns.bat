@echo off
rem Narrow import of Keel's option B modules: column shrouds (SM_SB_Kit_AlienColumn_*) and door portals (SM_SB_Kit_AlienPortal_*); author first with
rem Saved\Tools\Blender\...\blender.exe --background --factory-startup --python Art\blender\build_keel_columns.py
setlocal
if not exist "%~dp0..\Saved\Logs" mkdir "%~dp0..\Saved\Logs"
"%USERPROFILE%\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "%~dp0..\Shieldbreak.uproject" -run=pythonscript -script="%~dp0import_map_art.py" -SBMapArtPrefix=SM_SB_Kit_AlienColumn -unattended -nosplash -nullrhi -stdout > "%~dp0..\Saved\Logs\KeelColumnsImport.log" 2>&1
"%USERPROFILE%\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "%~dp0..\Shieldbreak.uproject" -run=pythonscript -script="%~dp0import_map_art.py" -SBMapArtPrefix=SM_SB_Kit_AlienPortal -unattended -nosplash -nullrhi -stdout > "%~dp0..\Saved\Logs\KeelPortalsImport.log" 2>&1
exit /b %ERRORLEVEL%

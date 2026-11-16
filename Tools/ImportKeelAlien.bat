@echo off
rem Narrow import of Keel's alien bridge kit (SM_SB_Kit_Alien*); does not reimport the rest of the map kit.
setlocal
if not exist "%~dp0..\Saved\Logs" mkdir "%~dp0..\Saved\Logs"
"%USERPROFILE%\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "%~dp0..\Shieldbreak.uproject" -run=pythonscript -script="%~dp0import_map_art.py" -SBMapArtPrefix=SM_SB_Kit_Alien -unattended -nosplash -nullrhi -stdout > "%~dp0..\Saved\Logs\KeelAlienImport.log" 2>&1
"%USERPROFILE%\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "%~dp0..\Shieldbreak.uproject" -run=pythonscript -script="%~dp0create_keel_alien_material.py" -unattended -nosplash -nullrhi -stdout > "%~dp0..\Saved\Logs\KeelAlienMaterial.log" 2>&1
exit /b %ERRORLEVEL%

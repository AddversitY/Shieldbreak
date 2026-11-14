@echo off
rem Narrow import of Keel's Cradle pod (SM_SB_Kit_CradlePod); does not reimport the rest of the map kit.
setlocal
if not exist "%~dp0..\Saved\Logs" mkdir "%~dp0..\Saved\Logs"
"%USERPROFILE%\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "%~dp0..\Shieldbreak.uproject" -run=pythonscript -script="%~dp0import_map_art.py" -SBMapArtPrefix=SM_SB_Kit_CradlePod -unattended -nosplash -nullrhi -stdout > "%~dp0..\Saved\Logs\KeelHeroImport.log" 2>&1
exit /b %ERRORLEVEL%

@echo off
rem Imports the map set pieces (Art\Export\MapKit\*.fbx) into Content\Shieldbreak\Art\MapKit, headless.
rem Results: Saved\Logs\MapArtImport.txt (log: Saved\Logs\MapArtImport.log)
setlocal
if not exist "%~dp0..\Saved\Logs" mkdir "%~dp0..\Saved\Logs"
"%USERPROFILE%\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "%~dp0..\Shieldbreak.uproject" -run=pythonscript -script="%~dp0import_map_art.py" -unattended -nosplash -nullrhi -stdout > "%~dp0..\Saved\Logs\MapArtImport.log" 2>&1
echo EXITCODE=%ERRORLEVEL% >> "%~dp0..\Saved\Logs\MapArtImport.log"
endlocal

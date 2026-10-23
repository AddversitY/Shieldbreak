@echo off
rem Exports the mannequin and Lyra guns as FBX to Saved\ArtExport (reference for modeling new art). Log: Saved\Logs\ExportArtReference.log
setlocal
set "PROJ=%~dp0..\Shieldbreak.uproject"
set "UE=%USERPROFILE%\Epic Games\UE_5.8"
if not exist "%~dp0..\Saved\Logs" mkdir "%~dp0..\Saved\Logs"
"%UE%\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "%PROJ%" -run=pythonscript -script="%~dp0export_art_reference.py" -unattended -nosplash -nullrhi -stdout > "%~dp0..\Saved\Logs\ExportArtReference.log" 2>&1
echo EXITCODE=%ERRORLEVEL% >> "%~dp0..\Saved\Logs\ExportArtReference.log"
endlocal

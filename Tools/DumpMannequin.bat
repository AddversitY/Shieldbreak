@echo off
rem Writes the mannequin's skin as Saved\ArtExport\SKM_Manny.obj (Geometry Script; no render data needed).
rem Report: Saved\ArtExport\dump_report.txt (log: Saved\Logs\DumpMannequin.log)
setlocal
set "PROJ=%~dp0..\Shieldbreak.uproject"
set "UE=%USERPROFILE%\Epic Games\UE_5.8"
if not exist "%~dp0..\Saved\Logs" mkdir "%~dp0..\Saved\Logs"
"%UE%\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "%PROJ%" -run=pythonscript -script="%~dp0dump_mannequin_mesh.py" -EnablePlugins=GeometryScripting -unattended -nosplash -nullrhi -stdout > "%~dp0..\Saved\Logs\DumpMannequin.log" 2>&1
echo EXITCODE=%ERRORLEVEL% >> "%~dp0..\Saved\Logs\DumpMannequin.log"
endlocal

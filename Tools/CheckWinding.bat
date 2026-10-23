@echo off
rem Checks the imported art meshes face outward (signed volume vs the engine cube), headless.
rem Results: Saved\Logs\CheckWinding.txt (log: Saved\Logs\CheckWinding.log)
setlocal
set "PROJ=%~dp0..\Shieldbreak.uproject"
set "UE=%USERPROFILE%\Epic Games\UE_5.8"
if not exist "%~dp0..\Saved\Logs" mkdir "%~dp0..\Saved\Logs"
"%UE%\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "%PROJ%" -run=pythonscript -script="%~dp0check_winding.py" -EnablePlugins=GeometryScripting -unattended -nosplash -nullrhi -stdout > "%~dp0..\Saved\Logs\CheckWinding.log" 2>&1
echo EXITCODE=%ERRORLEVEL% >> "%~dp0..\Saved\Logs\CheckWinding.log"
endlocal

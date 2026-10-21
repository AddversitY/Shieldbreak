@echo off
rem Runs Tools\check_imported_assets.py headless; results in Saved\Logs\ImportCheck.txt
setlocal
set "PROJ=%~dp0..\Shieldbreak.uproject"
set "UE=%USERPROFILE%\Epic Games\UE_5.8"
"%UE%\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "%PROJ%" -run=pythonscript -script="%~dp0check_imported_assets.py" -unattended -nosplash -nullrhi -stdout > "%~dp0..\Saved\Logs\CheckImports.log" 2>&1
echo EXITCODE=%ERRORLEVEL% >> "%~dp0..\Saved\Logs\CheckImports.log"
endlocal

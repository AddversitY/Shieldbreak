@echo off
rem Builds the editor (picks up plugin changes), then runs Tools\check_imported_assets.py headless.
rem Results: Saved\Logs\ImportCheck.txt (log: Saved\Logs\CheckImports.log)
setlocal
set "PROJ=%~dp0..\Shieldbreak.uproject"
set "UE=%USERPROFILE%\Epic Games\UE_5.8"
set "LOG=%~dp0..\Saved\Logs\CheckImports.log"
echo === Build === > "%LOG%"
call "%UE%\Engine\Build\BatchFiles\Build.bat" ShieldbreakEditor Win64 Development -Project="%PROJ%" -WaitMutex -NoHotReload >> "%LOG%" 2>&1
echo BUILDCODE=%ERRORLEVEL% >> "%LOG%"
echo === Check === >> "%LOG%"
"%UE%\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "%PROJ%" -run=pythonscript -script="%~dp0check_imported_assets.py" -unattended -nosplash -nullrhi -stdout >> "%LOG%" 2>&1
echo EXITCODE=%ERRORLEVEL% >> "%LOG%"
endlocal

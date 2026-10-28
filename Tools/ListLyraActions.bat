@echo off
rem Writes Lyra action animation lengths, skeletons and additive types, headless.
rem Results: Saved\Logs\LyraActions.txt (log: Saved\Logs\ListLyraActions.log)
setlocal
set "PROJ=%~dp0..\Shieldbreak.uproject"
set "UE=%USERPROFILE%\Epic Games\UE_5.8"
if not exist "%~dp0..\Saved\Logs" mkdir "%~dp0..\Saved\Logs"
"%UE%\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "%PROJ%" -run=pythonscript -script="%~dp0list_lyra_actions.py" -unattended -nosplash -nullrhi -stdout > "%~dp0..\Saved\Logs\ListLyraActions.log" 2>&1
echo EXITCODE=%ERRORLEVEL% >> "%~dp0..\Saved\Logs\ListLyraActions.log"
endlocal

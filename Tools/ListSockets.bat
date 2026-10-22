@echo off
rem Writes socket names of the imported weapon/character meshes to Saved\Logs\Sockets.txt
setlocal
set "PROJ=%~dp0..\Shieldbreak.uproject"
set "UE=%USERPROFILE%\Epic Games\UE_5.8"
"%UE%\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "%PROJ%" -run=pythonscript -script="%~dp0list_sockets.py" -unattended -nosplash -nullrhi -stdout > "%~dp0..\Saved\Logs\ListSockets.log" 2>&1
echo EXITCODE=%ERRORLEVEL% >> "%~dp0..\Saved\Logs\ListSockets.log"
endlocal

@echo off
rem Imports Art\Export\**\*.fbx (scripted weapon/armor art) into Content\Shieldbreak\Art, headless.
rem Results: Saved\Logs\ImportArt.txt (log: Saved\Logs\ImportArt.log)
setlocal
set "PROJ=%~dp0..\Shieldbreak.uproject"
set "UE=%USERPROFILE%\Epic Games\UE_5.8"
if not exist "%~dp0..\Saved\Logs" mkdir "%~dp0..\Saved\Logs"
"%UE%\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "%PROJ%" -run=pythonscript -script="%~dp0import_art.py" -unattended -nosplash -nullrhi -stdout > "%~dp0..\Saved\Logs\ImportArt.log" 2>&1
echo EXITCODE=%ERRORLEVEL% >> "%~dp0..\Saved\Logs\ImportArt.log"
endlocal

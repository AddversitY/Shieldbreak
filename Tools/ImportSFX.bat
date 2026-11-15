@echo off
rem Imports Art\Export\Audio\SFX\*.wav (combat sound banks) into Content\Shieldbreak\Audio\SFX, headless.
rem Results: Saved\Logs\ImportSFX.txt (log: Saved\Logs\ImportSFX.log)
setlocal
set "PROJ=%~dp0..\Shieldbreak.uproject"
set "UE=%USERPROFILE%\Epic Games\UE_5.8"
if not exist "%~dp0..\Saved\Logs" mkdir "%~dp0..\Saved\Logs"
"%UE%\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "%PROJ%" -run=pythonscript -script="%~dp0import_sfx.py" -unattended -nosplash -nullrhi -stdout > "%~dp0..\Saved\Logs\ImportSFX.log" 2>&1
echo EXITCODE=%ERRORLEVEL% >> "%~dp0..\Saved\Logs\ImportSFX.log"
endlocal

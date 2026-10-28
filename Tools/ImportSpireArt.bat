@echo off
setlocal
"%USERPROFILE%\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "%~dp0..\Shieldbreak.uproject" -run=pythonscript -script="%~dp0import_spire_art.py" -unattended -nosplash -nullrhi -stdout > "%~dp0..\Saved\Logs\SpireImport.log" 2>&1
exit /b %ERRORLEVEL%

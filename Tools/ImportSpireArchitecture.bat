@echo off
rem Run Art/build_spire_architecture.py first with standard Python.
setlocal
if not exist "%~dp0..\Saved\Logs" mkdir "%~dp0..\Saved\Logs"
"%USERPROFILE%\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "%~dp0..\Shieldbreak.uproject" -run=pythonscript -script="%~dp0import_spire_architecture.py" -unattended -nosplash -nullrhi -stdout > "%~dp0..\Saved\Logs\SpireArchitectureImport.log" 2>&1
exit /b %ERRORLEVEL%

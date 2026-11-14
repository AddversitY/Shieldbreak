@echo off
rem Keel's animated light materials. Report: Saved\Logs\KeelFX.txt
setlocal
if not exist "%~dp0..\Saved\Logs" mkdir "%~dp0..\Saved\Logs"
if exist "%~dp0..\Saved\Logs\KeelFX.txt" del "%~dp0..\Saved\Logs\KeelFX.txt"
"%USERPROFILE%\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "%~dp0..\Shieldbreak.uproject" -run=pythonscript -script="%~dp0create_keel_fx.py" -unattended -nosplash -nullrhi -stdout > "%~dp0..\Saved\Logs\KeelFXImport.log" 2>&1
exit /b %ERRORLEVEL%




@echo off
rem Run Art/textures/make_keel_space.py first (Python, numpy, Pillow). Report: Saved\Logs\KeelSpace.txt
setlocal
if not exist "%~dp0..\Saved\Logs" mkdir "%~dp0..\Saved\Logs"
if exist "%~dp0..\Saved\Logs\KeelSpace.txt" del "%~dp0..\Saved\Logs\KeelSpace.txt"
"%USERPROFILE%\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "%~dp0..\Shieldbreak.uproject" -run=pythonscript -script="%~dp0create_keel_space.py" -unattended -nosplash -nullrhi -stdout > "%~dp0..\Saved\Logs\KeelSpaceImport.log" 2>&1
exit /b %ERRORLEVEL%



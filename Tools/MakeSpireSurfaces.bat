@echo off
rem Run Art/textures/make_spire_surfaces.py first (Python, numpy, Pillow).
setlocal
if not exist "%~dp0..\Saved\Logs" mkdir "%~dp0..\Saved\Logs"
"%USERPROFILE%\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "%~dp0..\Shieldbreak.uproject" -run=pythonscript -script="%~dp0create_spire_surfaces.py" -unattended -nosplash -nullrhi -stdout > "%~dp0..\Saved\Logs\SpireSurfacesImport.log" 2>&1
exit /b %ERRORLEVEL%

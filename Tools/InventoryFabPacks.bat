@echo off
rem Lists the Fab packs' static meshes (Tools/inventory_fab_packs.py). Report: Saved\Logs\FabPacks.txt and FabPacks.json
setlocal
if not exist "%~dp0..\Saved\Logs" mkdir "%~dp0..\Saved\Logs"
"%USERPROFILE%\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "%~dp0..\Shieldbreak.uproject" -run=pythonscript -script="%~dp0inventory_fab_packs.py" -unattended -nosplash -nullrhi -stdout > "%~dp0..\Saved\Logs\FabPacksRun.log" 2>&1
exit /b %ERRORLEVEL%

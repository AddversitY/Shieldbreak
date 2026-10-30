@echo off
rem Imports the sci-fi kit pieces (Art\Export\SciFiKit\*.fbx) and their textures into Content\Shieldbreak\Art\SciFiKit, headless.
rem Results: Saved\Logs\SciFiKitImport.txt (log: Saved\Logs\SciFiKitImport.log)
setlocal
if not exist "%~dp0..\Saved\Logs" mkdir "%~dp0..\Saved\Logs"
"%USERPROFILE%\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "%~dp0..\Shieldbreak.uproject" -run=pythonscript -script="%~dp0import_scifi_kit.py" -unattended -nosplash -nullrhi -stdout > "%~dp0..\Saved\Logs\SciFiKitImport.log" 2>&1
echo EXITCODE=%ERRORLEVEL% >> "%~dp0..\Saved\Logs\SciFiKitImport.log"
endlocal

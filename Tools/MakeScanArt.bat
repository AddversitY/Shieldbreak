@echo off
rem Checks the scanned art (Tools\GetScanArt.bat) and builds M_SB_Scan and its instances, headless.
rem Results: Saved\Logs\ScanArt.txt (log: Saved\Logs\ScanArt.log)
setlocal
if not exist "%~dp0..\Saved\Logs" mkdir "%~dp0..\Saved\Logs"
"%USERPROFILE%\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "%~dp0..\Shieldbreak.uproject" -run=pythonscript -script="%~dp0make_scan_art.py" -unattended -nosplash -nullrhi -stdout > "%~dp0..\Saved\Logs\ScanArt.log" 2>&1
echo EXITCODE=%ERRORLEVEL% >> "%~dp0..\Saved\Logs\ScanArt.log"
endlocal

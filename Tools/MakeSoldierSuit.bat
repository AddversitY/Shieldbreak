@echo off
rem Builds SK_SB_Soldier (the AI suit skinned to the mannequin skeleton), headless.
rem Results: Saved\Logs\MakeSoldierSuit.txt (log: Saved\Logs\MakeSoldierSuit.log)
setlocal
set "PROJ=%~dp0..\Shieldbreak.uproject"
set "UE=%USERPROFILE%\Epic Games\UE_5.8"
if not exist "%~dp0..\Saved\Logs" mkdir "%~dp0..\Saved\Logs"
"%UE%\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "%PROJ%" -run=pythonscript -script="%~dp0make_soldier_suit.py" -EnablePlugins=GeometryScripting -unattended -nosplash -nullrhi -stdout > "%~dp0..\Saved\Logs\MakeSoldierSuit.log" 2>&1
echo EXITCODE=%ERRORLEVEL% >> "%~dp0..\Saved\Logs\MakeSoldierSuit.log"
endlocal

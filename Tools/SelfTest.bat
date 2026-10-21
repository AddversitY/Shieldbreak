@echo off
rem Builds the editor target, then runs the scripted gameplay self-test (-SBSelfTest).
rem Report: Saved\Logs\SelfTest.txt   Run log: Saved\Logs\SelfTestRun.log
setlocal
set "PROJ=%~dp0..\Shieldbreak.uproject"
set "UE=%USERPROFILE%\Epic Games\UE_5.8"
set "LOG=%~dp0..\Saved\Logs\SelfTestRun.log"
if not exist "%~dp0..\Saved\Logs" mkdir "%~dp0..\Saved\Logs"
if exist "%~dp0..\Saved\Logs\SelfTest.txt" del "%~dp0..\Saved\Logs\SelfTest.txt"
echo === Build === > "%LOG%"
call "%UE%\Engine\Build\BatchFiles\Build.bat" ShieldbreakEditor Win64 Development -Project="%PROJ%" -WaitMutex -NoHotReload >> "%LOG%" 2>&1
echo BUILDCODE=%ERRORLEVEL% >> "%LOG%"
if not "%ERRORLEVEL%"=="0" goto :done
echo === Run === >> "%LOG%"
"%UE%\Engine\Binaries\Win64\UnrealEditor.exe" "%PROJ%" -game -windowed -ResX=1280 -ResY=720 -log -SBSelfTest >> "%LOG%" 2>&1
echo RUNCODE=%ERRORLEVEL% >> "%LOG%"
:done
echo DONE >> "%LOG%"
endlocal

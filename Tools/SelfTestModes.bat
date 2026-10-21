@echo off
rem Builds the editor target, then runs the objective-mode self-tests (CTF, KOTH, Oddball) one after another.
rem Usage: SelfTestModes.bat [mode ...]   (default: CTF KOTH Oddball)
rem Reports: Saved\Logs\SelfTest-<Mode>.txt   Run log: Saved\Logs\SelfTestModesRun.log
rem Screenshots: Saved\Screenshots\SelfTest\<mode>_N.png
setlocal
set "PROJ=%~dp0..\Shieldbreak.uproject"
set "UE=%USERPROFILE%\Epic Games\UE_5.8"
set "LOGS=%~dp0..\Saved\Logs"
set "LOG=%LOGS%\SelfTestModesRun.log"
set "MODES=%*"
if "%MODES%"=="" set "MODES=CTF KOTH Oddball"
if not exist "%LOGS%" mkdir "%LOGS%"
echo === Build === > "%LOG%"
call "%UE%\Engine\Build\BatchFiles\Build.bat" ShieldbreakEditor Win64 Development -Project="%PROJ%" -WaitMutex -NoHotReload >> "%LOG%" 2>&1
echo BUILDCODE=%ERRORLEVEL% >> "%LOG%"
if not "%ERRORLEVEL%"=="0" goto :done
for %%M in (%MODES%) do call :run %%M
:done
echo DONE >> "%LOG%"
endlocal
goto :eof

:run
if exist "%LOGS%\SelfTest-%1.txt" del "%LOGS%\SelfTest-%1.txt"
echo === Run %1 === >> "%LOG%"
start "Shieldbreak self-test %1" /wait "%UE%\Engine\Binaries\Win64\UnrealEditor.exe" "%PROJ%" "/Engine/Maps/Entry?Mode=%1" -game -windowed -ResX=1280 -ResY=720 -log -abslog="%LOGS%\SelfTest-%1-Run.log" -SBSelfTest
echo RUNCODE=%ERRORLEVEL% >> "%LOG%"
goto :eof

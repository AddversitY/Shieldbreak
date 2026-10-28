@echo off
rem Builds the editor target, then runs the self-test map tour on every launch map (spawns, pickups, lifts,
rem a 75 s bot match with stuck detection, screenshots, FPS), plus CTF on Keel and KOTH on Bastion.
rem Reports: Saved\Logs\SelfTest-<Map>[-<Mode>].txt   Run log: Saved\Logs\MapTourRun.log
rem Screenshots: Saved\Screenshots\SelfTest\<map>[-<mode>]_N.png
setlocal
set "PROJ=%~dp0..\Shieldbreak.uproject"
set "UE=%USERPROFILE%\Epic Games\UE_5.8"
set "LOGS=%~dp0..\Saved\Logs"
set "LOG=%LOGS%\MapTourRun.log"
if not exist "%LOGS%" mkdir "%LOGS%"
echo === Build === > "%LOG%"
call "%UE%\Engine\Build\BatchFiles\Build.bat" ShieldbreakEditor Win64 Development -Project="%PROJ%" -WaitMutex -NoHotReload >> "%LOG%" 2>&1
echo BUILDCODE=%ERRORLEVEL% >> "%LOG%"
if not "%ERRORLEVEL%"=="0" goto :done
call :run Keel Slayer
call :run Bastion Slayer
call :run Sanctum Slayer
call :run Creekbed Slayer
call :run Spire Slayer
call :run Keel CTF
call :run Bastion KOTH
:done
echo DONE >> "%LOG%"
endlocal
goto :eof

:run
echo === Run %1 %2 === >> "%LOG%"
start "Shieldbreak map tour %1 %2" /wait "%UE%\Engine\Binaries\Win64\UnrealEditor.exe" "%PROJ%" "/Engine/Maps/Entry?Map=%1?Mode=%2" -game -windowed -ResX=1280 -ResY=720 -log -abslog="%LOGS%\MapTour-%1-%2.log" -SBSelfTest
echo RUNCODE=%ERRORLEVEL% >> "%LOG%"
goto :eof

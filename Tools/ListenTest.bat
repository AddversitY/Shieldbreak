@echo off
rem Two-instance multiplayer test on one PC: a listen server (with bots) and a client joining 127.0.0.1.
rem Each instance takes screenshots (Saved\Screenshots\AutoShot\server_NN.png / client_NN.png) and quits.
rem Logs: Saved\Logs\ListenServer.log, Saved\Logs\ListenClient.log, Saved\Logs\ListenTestRun.log
setlocal
set "PROJ=%~dp0..\Shieldbreak.uproject"
set "UE=%USERPROFILE%\Epic Games\UE_5.8"
set "LOGS=%~dp0..\Saved\Logs"
set "LOG=%LOGS%\ListenTestRun.log"
set "MODE=%~1"
if "%MODE%"=="" set "MODE=Slayer"
if not exist "%LOGS%" mkdir "%LOGS%"
echo === Build === > "%LOG%"
call "%UE%\Engine\Build\BatchFiles\Build.bat" ShieldbreakEditor Win64 Development -Project="%PROJ%" -WaitMutex -NoHotReload >> "%LOG%" 2>&1
echo BUILDCODE=%ERRORLEVEL% >> "%LOG%"
if not "%ERRORLEVEL%"=="0" goto :done
echo === Server (Mode=%MODE%) === >> "%LOG%"
start "Shieldbreak server" "%UE%\Engine\Binaries\Win64\UnrealEditor.exe" "%PROJ%" /Engine/Maps/Entry?listen?Mode=%MODE%?Bots=2 -game -windowed -ResX=960 -ResY=540 -WinX=0 -WinY=40 -log -abslog="%LOGS%\ListenServer.log" -SBAutoShot=90 -SBAutoShotCount=4 -SBAutoShotInterval=10 -SBAutoShotPrefix=server -SBAutoQuit
rem Give the server time to load (shader/texture builds on a cold start take a while).
rem (ping, not timeout: timeout refuses to run without a console, and Git Bash shadows it with its own)
ping -n 61 127.0.0.1 > nul
echo === Client === >> "%LOG%"
"%UE%\Engine\Binaries\Win64\UnrealEditor.exe" "%PROJ%" 127.0.0.1 -game -windowed -ResX=960 -ResY=540 -WinX=960 -WinY=40 -log -abslog="%LOGS%\ListenClient.log" -SBAutoShot=45 -SBAutoShotCount=4 -SBAutoShotInterval=10 -SBAutoShotPrefix=client -SBAutoQuit
echo CLIENTCODE=%ERRORLEVEL% >> "%LOG%"
:done
echo DONE >> "%LOG%"
endlocal

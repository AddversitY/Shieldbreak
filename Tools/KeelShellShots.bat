@echo off
rem Keel shell kit: matched shots of the new look and the old look (-SBNoKeelShell) at Keel's fixed and hero cameras,
rem HUD off. Run after Tools\BuildEditor.bat and Tools\ImportKeelShell.bat.
rem Optional: set SBEXTRA=<extra game args> for both runs (for example -SBKeelShellEV=2 or -SBSky=0.8 to tune the light).
rem Shots: Saved\Screenshots\Shots\Keel_shell_<view>.png and Keel_old_<view>.png
rem Reports (with fixed-view FPS): Saved\Logs\SelfTest-Shots-Keel-shell.txt and -old.txt
setlocal
set "PROJ=%~dp0..\Shieldbreak.uproject"
set "UE=%USERPROFILE%\Epic Games\UE_5.8"
set "LOGS=%~dp0..\Saved\Logs"
if not exist "%LOGS%" mkdir "%LOGS%"
call :run shell ""
call :run old "-SBNoKeelShell"
endlocal
goto :eof

:run
start "Shieldbreak Keel shell shots %1" /wait "%UE%\Engine\Binaries\Win64\UnrealEditor.exe" "%PROJ%" "/Engine/Maps/Entry?Map=Keel?Mode=Slayer" -game -windowed -ResX=1280 -ResY=720 -log -abslog="%LOGS%\Shots-Keel-%1.log" -SBSelfTest -SBShots -SBHeroShots -SBCleanShots %~2 -SBShotTag=%1 %SBEXTRA%
if exist "%LOGS%\SelfTest-Shots.txt" move /y "%LOGS%\SelfTest-Shots.txt" "%LOGS%\SelfTest-Shots-Keel-%1.txt" > nul
goto :eof

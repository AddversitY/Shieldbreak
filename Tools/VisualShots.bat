@echo off
rem Fixed-camera before/after screenshots of every launch map (-SBShots), plus first-person shots on Keel (-SBFPShots).
rem "before" = -SBFlatMaterials -SBNoGrade (the look before the 2026-10-25 visual pass). Build first (Tools\BuildEditor.bat).
rem Usage: VisualShots.bat [maps...]   (default: Keel Bastion Sanctum Creekbed Spire, then the first-person set) Optional: set SBEXTRA=<extra game args> and SBTAG=<tag suffix>
rem Shots: Saved\Screenshots\Shots\<map>_<tag>_<view>.png   Reports (with fixed-view FPS): Saved\Logs\SelfTest-Shots-<map>-<tag>.txt
setlocal
set "PROJ=%~dp0..\Shieldbreak.uproject"
set "UE=%USERPROFILE%\Epic Games\UE_5.8"
set "LOGS=%~dp0..\Saved\Logs"
if not exist "%LOGS%" mkdir "%LOGS%"
set "MAPS=%*"
if "%MAPS%"=="" set "MAPS=Keel Bastion Sanctum Creekbed Spire FP"
for %%M in (%MAPS%) do (
  if /i "%%M"=="FP" (
    call :run Keel fp "-SBFPShots"
  ) else (
    call :run %%M after "-SBShots"
    call :run %%M before "-SBShots -SBFlatMaterials -SBNoGrade"
  )
)
endlocal
goto :eof

:run
start "Shieldbreak shots %1 %2" /wait "%UE%\Engine\Binaries\Win64\UnrealEditor.exe" "%PROJ%" "/Engine/Maps/Entry?Map=%1?Mode=Slayer" -game -windowed -ResX=1280 -ResY=720 -log -abslog="%LOGS%\Shots-%1-%2%SBTAG%.log" -SBSelfTest %~3 -SBShotTag=%2%SBTAG% %SBEXTRA%
if exist "%LOGS%\SelfTest-Shots.txt" move /y "%LOGS%\SelfTest-Shots.txt" "%LOGS%\SelfTest-Shots-%1-%2%SBTAG%.txt" > nul
goto :eof

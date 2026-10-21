@echo off
rem Builds the editor target, then runs the game windowed and saves screenshots to Saved\Screenshots\AutoShot.
rem Usage: PlayTest.bat [first-shot-seconds] [shot-count]
setlocal
set "PROJ=%~dp0Shieldbreak.uproject"
set "UE=%USERPROFILE%\Epic Games\UE_5.8"
set "FIRST=%~1"
if "%FIRST%"=="" set "FIRST=8"
set "COUNT=%~2"
if "%COUNT%"=="" set "COUNT=4"
if not exist "%~dp0Saved\Logs" mkdir "%~dp0Saved\Logs"
set "LOG=%~dp0Saved\Logs\PlayTest.log"

echo === Build === > "%LOG%"
call "%UE%\Engine\Build\BatchFiles\Build.bat" ShieldbreakEditor Win64 Development -Project="%PROJ%" -WaitMutex -NoHotReload >> "%LOG%" 2>&1
set "BUILDCODE=%ERRORLEVEL%"
echo BUILDCODE=%BUILDCODE% >> "%LOG%"
if not "%BUILDCODE%"=="0" goto :done

echo === Run === >> "%LOG%"
"%UE%\Engine\Binaries\Win64\UnrealEditor.exe" "%PROJ%" -game -windowed -ResX=1600 -ResY=900 -log -SBAutoShot=%FIRST% -SBAutoShotCount=%COUNT% -SBAutoShotInterval=4 -SBAutoQuit >> "%LOG%" 2>&1
echo RUNCODE=%ERRORLEVEL% >> "%LOG%"

:done
echo DONE >> "%LOG%"
endlocal

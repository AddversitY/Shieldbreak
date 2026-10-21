@echo off
rem Builds the editor target, then runs the objective-mode self-tests (CTF, KOTH, Oddball) one after another.
rem Reports: Saved\Logs\SelfTest-<Mode>.txt   Run log: Saved\Logs\SelfTestModesRun.log
rem Screenshots: Saved\Screenshots\SelfTest\<mode>_N.png
setlocal
set "PROJ=%~dp0..\Shieldbreak.uproject"
set "UE=%USERPROFILE%\Epic Games\UE_5.8"
set "LOG=%~dp0..\Saved\Logs\SelfTestModesRun.log"
if not exist "%~dp0..\Saved\Logs" mkdir "%~dp0..\Saved\Logs"
echo === Build === > "%LOG%"
call "%UE%\Engine\Build\BatchFiles\Build.bat" ShieldbreakEditor Win64 Development -Project="%PROJ%" -WaitMutex -NoHotReload >> "%LOG%" 2>&1
echo BUILDCODE=%ERRORLEVEL% >> "%LOG%"
if not "%ERRORLEVEL%"=="0" goto :done
for %%M in (CTF KOTH Oddball) do (
  if exist "%~dp0..\Saved\Logs\SelfTest-%%M.txt" del "%~dp0..\Saved\Logs\SelfTest-%%M.txt"
  echo === Run %%M === >> "%LOG%"
  "%UE%\Engine\Binaries\Win64\UnrealEditor.exe" "%PROJ%" /Engine/Maps/Entry?Mode=%%M -game -windowed -ResX=1280 -ResY=720 -log -SBSelfTest >> "%LOG%" 2>&1
  echo RUNCODE=%ERRORLEVEL% >> "%LOG%"
)
:done
echo DONE >> "%LOG%"
endlocal

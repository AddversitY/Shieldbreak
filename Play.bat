@echo off
rem Play Shieldbreak: opens the main menu (pick map, mode and bots, then PLAY). Builds first if the code changed.
setlocal
set "PROJ=%~dp0Shieldbreak.uproject"
set "UE=%USERPROFILE%\Epic Games\UE_5.8"
echo Building (only takes long if the code changed)...
call "%UE%\Engine\Build\BatchFiles\Build.bat" ShieldbreakEditor Win64 Development -Project="%PROJ%" -WaitMutex -NoHotReload
if errorlevel 1 (
  echo Build failed. See the lines above.
  pause
  exit /b 1
)
start "" "%UE%\Engine\Binaries\Win64\UnrealEditor.exe" "%PROJ%" -game -windowed -ResX=1600 -ResY=900
endlocal

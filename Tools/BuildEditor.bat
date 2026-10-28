@echo off
rem Builds the ShieldbreakEditor target. Output goes to Saved\Logs\BuildEditor.log as well as the console.
setlocal
set "PROJ=%~dp0..\Shieldbreak.uproject"
set "UE=%USERPROFILE%\Epic Games\UE_5.8"
if not exist "%~dp0..\Saved\Logs" mkdir "%~dp0..\Saved\Logs"
set "LOG=%~dp0..\Saved\Logs\BuildEditor.log"

echo === Toolchain === > "%LOG%"
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if exist "%VSWHERE%" (
  "%VSWHERE%" -all -products * -format text >> "%LOG%" 2>&1
) else (
  echo vswhere not found: Visual Studio is not installed >> "%LOG%"
)
dir /b "%ProgramFiles(x86)%\Windows Kits\10\Include" >> "%LOG%" 2>&1

echo === Build === >> "%LOG%"
call "%UE%\Engine\Build\BatchFiles\Build.bat" ShieldbreakEditor Win64 Development -Project="%PROJ%" -WaitMutex -NoHotReload >> "%LOG%" 2>&1
echo EXITCODE=%ERRORLEVEL% >> "%LOG%"
echo Build finished with code %ERRORLEVEL%. See Saved\Logs\BuildEditor.log
endlocal

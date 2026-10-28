@echo off
rem Packages a standalone Win64 Development game (build, cook, stage, pak, archive) with RunUAT BuildCookRun.
rem Output: Saved\Packaged\Windows\Shieldbreak.exe   Log: Saved\Logs\Package.log
rem Run Tools\ImportArt.bat first if Art\Export changed: the cook only packs what's in Content\.
setlocal
set "ROOT=%~dp0.."
for %%I in ("%ROOT%") do set "ROOT=%%~fI"
set "PROJ=%ROOT%\Shieldbreak.uproject"
set "UE=%USERPROFILE%\Epic Games\UE_5.8"
set "LOG=%ROOT%\Saved\Logs\Package.log"
if not exist "%ROOT%\Saved\Logs" mkdir "%ROOT%\Saved\Logs"
call "%UE%\Engine\Build\BatchFiles\RunUAT.bat" BuildCookRun -project="%PROJ%" -noP4 -unattended -utf8output ^
  -platform=Win64 -clientconfig=Development -build -cook -stage -pak -archive ^
  -archivedirectory="%ROOT%\Saved\Packaged" -IgnoreCookErrors > "%LOG%" 2>&1
set "CODE=%ERRORLEVEL%"
echo EXITCODE=%CODE% >> "%LOG%"
rem -IgnoreCookErrors: a few unused Lyra Blueprints (they need Lyra's C++) come along by reference and fail to
rem compile. Every error line is kept in PackageErrors.txt; anything outside that known Lyra set needs a look.
findstr /c:"Error:" "%LOG%" > "%ROOT%\Saved\Logs\PackageErrors.txt"
echo Package finished with code %CODE%. See Saved\Logs\Package.log and Saved\Logs\PackageErrors.txt
endlocal

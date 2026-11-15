@echo off
rem Imports Art\Export\**\*.fbx (scripted weapon/armor art) into Content\Shieldbreak\Art, headless.
rem Results: Saved\Logs\ImportArt.txt (log: Saved\Logs\ImportArt.log)
setlocal
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0ImportArt.ps1"
exit /b %ERRORLEVEL%

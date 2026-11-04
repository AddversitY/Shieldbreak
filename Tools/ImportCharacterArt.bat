@echo off
rem Imports only the character and weapon art (Art\Export\Weapons and Armor) with LODs; map kits, textures and audio are left alone.
rem Results: Saved\Logs\ImportArt.txt (log: Saved\Logs\ImportArt.log)
setlocal
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0ImportArt.ps1" -Folders Weapons,Armor
exit /b %ERRORLEVEL%
endlocal

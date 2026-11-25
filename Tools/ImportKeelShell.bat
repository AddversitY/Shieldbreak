@echo off
rem Narrow import of Keel's shell kit (SM_SB_Kit_KeelShell_*, Art/blender/build_keel_shell_kit.py), then Nanite on for those
rem meshes (they are 4k-44k triangles each, sized for Nanite). Does not reimport the rest of the map kit.
rem Author first: Tools\BuildKeelShell.ps1
setlocal
set "UE=%USERPROFILE%\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
set "PROJ=%~dp0..\Shieldbreak.uproject"
if not exist "%~dp0..\Saved\Logs" mkdir "%~dp0..\Saved\Logs"
"%UE%" "%PROJ%" -run=pythonscript -script="%~dp0import_map_art.py" -SBMapArtPrefix=SM_SB_Kit_KeelShell -unattended -nosplash -nullrhi -stdout > "%~dp0..\Saved\Logs\KeelShellImport.log" 2>&1
if errorlevel 1 exit /b %ERRORLEVEL%
"%UE%" "%PROJ%" -run=pythonscript -script="%~dp0enable_keel_shell_nanite.py" -unattended -nosplash -nullrhi -stdout > "%~dp0..\Saved\Logs\KeelShellNanite.log" 2>&1
exit /b %ERRORLEVEL%

@echo off
rem Creates the bullet hit / muzzle flash materials in /Game/Shieldbreak/Art/FX, headless.
rem Results: Saved\Logs\ImpactFXMaterials.txt (log: Saved\Logs\CreateImpactFX.log)
setlocal
set "PROJ=%~dp0..\Shieldbreak.uproject"
set "UE=%USERPROFILE%\Epic Games\UE_5.8"
if not exist "%~dp0..\Saved\Logs" mkdir "%~dp0..\Saved\Logs"
"%UE%\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "%PROJ%" -run=pythonscript -script="%~dp0create_impact_fx_materials.py" -unattended -nosplash -stdout > "%~dp0..\Saved\Logs\CreateImpactFX.log" 2>&1
echo EXITCODE=%ERRORLEVEL% >> "%~dp0..\Saved\Logs\CreateImpactFX.log"
endlocal

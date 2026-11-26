@echo off
rem Keel's deep-space windows: textures (Art/textures/make_keel_deep_space.py, needs Python with numpy and Pillow), then
rem M_SB_KeelDeepSpace and MI_SB_KeelWindowGlass (Tools/create_keel_deep_space.py). Run Tools\ImportKeelBrainTank.bat
rem first (the window glass is a child of the tank glass). Log: Saved\Logs\KeelDeepSpace.txt
setlocal
set "UE=%USERPROFILE%\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
set "PROJ=%~dp0..\Shieldbreak.uproject"
if not exist "%~dp0..\Saved\Logs" mkdir "%~dp0..\Saved\Logs"
python "%~dp0..\Art\textures\make_keel_deep_space.py"
if errorlevel 1 exit /b %ERRORLEVEL%
"%UE%" "%PROJ%" -run=pythonscript -script="%~dp0create_keel_deep_space.py" -unattended -nosplash -nullrhi -stdout > "%~dp0..\Saved\Logs\KeelDeepSpaceMake.log" 2>&1
exit /b %ERRORLEVEL%

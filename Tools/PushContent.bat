@echo off
rem Commits the imported Content/ through Git LFS and pushes, using this PC's GitHub login.
rem Log: Saved\Logs\PushContent.log
setlocal
cd /d "%~dp0.."
set "LOG=%~dp0..\Saved\Logs\PushContent.log"
echo === LFS === > "%LOG%"
git lfs install --local >> "%LOG%" 2>&1
git lfs version >> "%LOG%" 2>&1
echo === Pull === >> "%LOG%"
git pull --ff-only origin claude/peaceful-hamilton-xcl6c8 >> "%LOG%" 2>&1
echo === Add === >> "%LOG%"
git add Content >> "%LOG%" 2>&1
git -c core.autocrlf=false commit -F "%~dp0content_commit_msg.txt" >> "%LOG%" 2>&1
echo COMMITCODE=%ERRORLEVEL% >> "%LOG%"
echo === Push === >> "%LOG%"
git push origin HEAD:claude/peaceful-hamilton-xcl6c8 >> "%LOG%" 2>&1
echo PUSHCODE=%ERRORLEVEL% >> "%LOG%"
endlocal

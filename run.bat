@echo off
setlocal
cd /d "%~dp0"

set "EXE=build\Release\rr_overlay.exe"
if not exist "%EXE%" goto not_built

set "PROC=%~1"
if "%PROC%"=="" set /p PROC=process exe name, for example robrun.exe: 
if "%PROC%"=="" goto no_name

echo INSERT toggles the menu, END shuts it down
"%EXE%" --process "%PROC%" --config robrun_cfg.json
if errorlevel 1 goto attach_failed
endlocal
exit /b 0

:not_built
echo not built yet, run build.bat first
exit /b 1

:no_name
echo no process name given
exit /b 1

:attach_failed
echo attach failed. the game must be running, and this window may need to be
echo started as administrator.
exit /b 1

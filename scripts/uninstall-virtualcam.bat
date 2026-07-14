@echo off
REM Unregister RailShot Virtual Camera. Run as Administrator.
setlocal
set "SCRIPT_DIR=%~dp0"
set "DLL=%SCRIPT_DIR%..\build\Release\railshot-virtualcam64.dll"
if not exist "%DLL%" set "DLL=%SCRIPT_DIR%..\build\Release\plugins\railshot-virtualcam64.dll"
if not exist "%DLL%" (
  echo ERROR: railshot-virtualcam64.dll not found.
  exit /b 1
)
echo Unregistering "%DLL%" ...
regsvr32 /u /s "%DLL%"
if errorlevel 1 (
  echo Unregistration failed. Right-click and Run as administrator.
  exit /b 1
)
echo RailShot Virtual Camera unregistered.
endlocal

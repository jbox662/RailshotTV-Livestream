@echo off
REM Register RailShot Virtual Camera (64-bit DirectShow filter). Run as Administrator.
setlocal
set "SCRIPT_DIR=%~dp0"
set "DLL=%SCRIPT_DIR%..\build\Release\railshot-virtualcam64.dll"
if not exist "%DLL%" set "DLL=%SCRIPT_DIR%..\build\Release\plugins\railshot-virtualcam64.dll"
if not exist "%DLL%" (
  echo ERROR: railshot-virtualcam64.dll not found. Build Release first.
  echo Looked in: %SCRIPT_DIR%..\build\Release\
  exit /b 1
)
echo Registering "%DLL%" ...
regsvr32 /s "%DLL%"
if errorlevel 1 (
  echo Registration failed. Right-click and Run as administrator.
  exit /b 1
)
echo RailShot Virtual Camera registered successfully.
endlocal

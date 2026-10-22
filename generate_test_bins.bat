@echo off
setlocal
if /I "%~1"=="/?" (
  echo Generate FF-filled App test BINs from the packaged DefaultBuild image.
  echo Uses Windows PowerShell/.NET and the matching sibling Boot header reader.
  echo Noninteractive: powershell -NoProfile -ExecutionPolicy Bypass -File generate_test_bins.ps1
  exit /b 0
)
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0generate_test_bins.ps1" -Pause %*
exit /b %errorlevel%

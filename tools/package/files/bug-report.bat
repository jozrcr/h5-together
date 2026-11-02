@echo off
rem open-homm5 bug report helper for Windows: runs bug-report.ps1 (nothing else happens here).
rem This file must keep Windows (CRLF) line endings.
setlocal

if not exist "%~dp0bug-report.ps1" (
    echo bug-report.ps1 is not in the same folder as this file.
    echo Please extract the whole zip first - right-click the zip, then Extract All -
    echo and run bug-report.bat from the new folder.
    echo.
    pause
    exit /b 7
)

powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0bug-report.ps1" %*
set "CODE=%ERRORLEVEL%"

rem Codes 1 and 2 mean PowerShell itself could not run the script, so the script
rem never got to wait for Enter. Wait here, so the message stays on screen.
if %CODE% GEQ 1 if %CODE% LEQ 2 (
    echo.
    echo The bug report helper could not start. Please tell us what the lines above say.
    pause
)
exit /b %CODE%

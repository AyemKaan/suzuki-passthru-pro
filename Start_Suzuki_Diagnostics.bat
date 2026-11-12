@echo off
title Suzuki PassThru Pro J2534
cd /d "%~dp0"
echo Starting Suzuki PassThru Pro J2534 Diagnostic Server...
start "" http://127.0.0.1:8890
suzuki_diag_server.exe
pause

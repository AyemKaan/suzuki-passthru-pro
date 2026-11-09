@echo off
title Suzuki OBD Professional J2534
cd /d "%~dp0"
echo Suzuki Profesyonel J2534 Teshis Sunucusu Baslatiliyor...
start "" http://127.0.0.1:8080
suzuki_diag_server.exe
pause

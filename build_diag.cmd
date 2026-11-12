@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars32.bat"
if errorlevel 1 exit /b 1
cd /d "%~dp0"
cl /nologo /std:c++17 /EHsc /W4 /utf-8 /MT suzuki_diag_server.cpp /Fe:suzuki_diag_server.exe /link /MACHINE:X86
if errorlevel 1 exit /b 1
copy /Y suzuki_diag_server.exe suzuki_diag_server.fixed.exe >nul

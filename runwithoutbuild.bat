@echo off
setlocal
cd /d "%~dp0"

if not exist build\Release\gau.exe (
    echo build\Release\gau.exe not found. Run run.bat first.
    exit /b 1
)

build\Release\gau.exe

@echo off
setlocal
cd /d "%~dp0"

if not exist build (
    cmake -S . -B build -G "Visual Studio 17 2022" -A x64
    if errorlevel 1 exit /b 1
)

cmake --build build --config Release
if errorlevel 1 exit /b 1

build\Release\gau.exe

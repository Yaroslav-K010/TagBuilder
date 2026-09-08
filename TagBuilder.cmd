@echo off
setlocal

set "PROJECT_DIR=%~dp0"
set "QT_BIN=C:\Qt\6.8.3\mingw_64\bin"
set "MINGW_BIN=C:\Program Files\JetBrains\CLion 2026.2.2\bin\mingw\bin"
set "BUILD_DIR=%PROJECT_DIR%build-mingw"
set "EXE_PATH=%BUILD_DIR%\TagBuilder.exe"

set "PATH=%QT_BIN%;%MINGW_BIN%;%PATH%"

if not exist "%EXE_PATH%" (
    echo Building TagBuilder...
    cmake -G "MinGW Makefiles" -S "%PROJECT_DIR%" -B "%BUILD_DIR%" -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="C:/Qt/6.8.3/mingw_64"
    if errorlevel 1 exit /b 1
    cmake --build "%BUILD_DIR%" --config Release
    if errorlevel 1 exit /b 1
)

start "" "%EXE_PATH%"

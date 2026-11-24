@echo off
title DUPLICATE-X Local C REST Server
cd /d "%~dp0"
echo ===================================================
echo  DUPLICATE-X C11 REST API Server (Team DataVault)
echo  http://127.0.0.1:8080
echo ===================================================
backend\build\duplicate-x.exe --server
if %ERRORLEVEL% NEQ 0 (
    echo.
    echo [ERROR] Server exited with code %ERRORLEVEL%.
)
pause


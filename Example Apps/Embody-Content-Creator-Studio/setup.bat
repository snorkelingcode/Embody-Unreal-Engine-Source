@echo off
setlocal

set STUDIO_DIR=%~dp0

echo ============================================================
echo  Embody Content Creator Studio ^— Setup
echo ============================================================
echo.

REM ── 1. Install SignallingWebServer dependencies ───────────────
echo [1/2] Installing SignallingWebServer dependencies...
cd /d "%STUDIO_DIR%signalling"
call npm install --omit=dev
if errorlevel 1 (
    echo.
    echo  ERROR: SignallingWebServer npm install failed.
    pause & exit /b 1
)
echo       Done.
echo.

REM ── 2. Install Electron dependencies ─────────────────────────
echo [2/2] Installing studio dependencies...
cd /d "%STUDIO_DIR%"
call npm install
if errorlevel 1 (
    echo.
    echo  ERROR: Studio npm install failed.
    pause & exit /b 1
)
echo       Done.
echo.

echo ============================================================
echo  Setup complete.
echo.
echo  Run:  npm start
echo ============================================================
echo.
pause

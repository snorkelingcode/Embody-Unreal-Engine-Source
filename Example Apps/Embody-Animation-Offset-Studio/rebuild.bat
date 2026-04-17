@echo off
setlocal

set REACT_DIR=C:\Program Files\Epic Games\UE_5.6\Engine\Plugins\Media\PixelStreaming2\Resources\WebServers\Frontend\implementations\react
set SIGNALLING_WWW=C:\Program Files\Epic Games\UE_5.6\Engine\Plugins\Media\PixelStreaming2\Resources\WebServers\SignallingWebServer\www
set STUDIO_WWW=%~dp0signalling\www

echo ============================================================
echo  Embody Mannequin Studio — Rebuild Frontend
echo ============================================================
echo.

REM ── 1. Build React frontend ─────────────────────────────────
echo [1/2] Building React frontend...
cd /d "%REACT_DIR%"
call npm run build
if errorlevel 1 (
    echo ERROR: React build failed.
    pause & exit /b 1
)
echo       Done.

REM ── 2. Copy updated www to studio signalling folder ─────────
echo [2/2] Syncing www to studio...
if not exist "%STUDIO_WWW%" (
    echo ERROR: Studio signalling\www not found. Run setup.bat first.
    pause & exit /b 1
)
xcopy /E /I /Y /Q "%SIGNALLING_WWW%" "%STUDIO_WWW%"
if errorlevel 1 (
    echo ERROR: Could not copy www files.
    pause & exit /b 1
)
echo       Done.

echo.
echo ============================================================
echo  Rebuild complete. Restart the studio with:  npm start
echo ============================================================
pause

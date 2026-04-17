@echo off
setlocal enabledelayedexpansion

set STUDIO_DIR=%~dp0
set VENV_DIR=%STUDIO_DIR%server_env

echo ============================================================
echo  Embody Animation Offset Studio ^— Setup
echo ============================================================
echo.

REM ── 1. Install SignallingWebServer dependencies ───────────────
echo [1/3] Installing SignallingWebServer dependencies...
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
echo [2/3] Installing studio dependencies...
cd /d "%STUDIO_DIR%"
call npm install
if errorlevel 1 (
    echo.
    echo  ERROR: Studio npm install failed.
    pause & exit /b 1
)
echo       Done.
echo.

REM ── 3. Set up Python animation server ────────────────────────
echo [3/3] Setting up Python animation server...
echo.

REM Find a working Python 3.8+ command
set PYTHON_CMD=
for %%P in (python python3 py) do (
    if not defined PYTHON_CMD (
        %%P --version >nul 2>&1
        if not errorlevel 1 (
            REM Check version is 3.8+
            for /f "tokens=2" %%V in ('%%P --version 2^>^&1') do (
                for /f "tokens=1,2 delims=." %%A in ("%%V") do (
                    if %%A geq 3 (
                        if %%B geq 8 (
                            set PYTHON_CMD=%%P
                        )
                    )
                )
            )
        )
    )
)

if not defined PYTHON_CMD (
    echo  ERROR: Python 3.8 or newer is required but was not found.
    echo.
    echo  Download Python from: https://www.python.org/downloads/
    echo  Make sure to check "Add Python to PATH" during installation.
    echo.
    pause & exit /b 1
)

echo  Found Python: %PYTHON_CMD%
echo.

REM ── Create virtual environment ───────────────────────────────
echo  Creating virtual environment...
if exist "%VENV_DIR%" rd /s /q "%VENV_DIR%"
%PYTHON_CMD% -m venv "%VENV_DIR%"
if errorlevel 1 (
    echo  ERROR: Could not create virtual environment.
    pause & exit /b 1
)

set PY=%VENV_DIR%\Scripts\python.exe
set PIP=%VENV_DIR%\Scripts\pip.exe

echo  Upgrading pip...
"%PY%" -m pip install --upgrade pip --quiet
echo.

REM ── Install base Python packages ─────────────────────────────
echo  Installing server dependencies...
"%PIP%" install fastapi "uvicorn[standard]" numpy sentence-transformers --quiet
if errorlevel 1 (
    echo  ERROR: Failed to install Python packages.
    pause & exit /b 1
)
echo.

REM ── Detect GPU and install PyTorch ───────────────────────────
echo  Detecting GPU...

REM Use Python to detect CUDA version from nvidia-smi
set TORCH_INDEX=cpu
"%PY%" -c "import subprocess,re,sys; r=subprocess.run(['nvidia-smi'],capture_output=True,text=True); m=re.search(r'CUDA Version:\s*(\d+)\.(\d+)',r.stdout); sys.exit(0) if m else sys.exit(1)" >nul 2>&1
if not errorlevel 1 (
    REM GPU found — get CUDA major version
    for /f %%C in ('"%PY%" -c "import subprocess,re; r=subprocess.run([\"nvidia-smi\"],capture_output=True,text=True); m=re.search(r\"CUDA Version:\s*(\d+)\",r.stdout); print(m.group(1) if m else \"0\")"') do set CUDA_MAJOR=%%C

    echo  NVIDIA GPU detected ^(CUDA !CUDA_MAJOR!.x^)

    if !CUDA_MAJOR! geq 12 (
        set TORCH_INDEX=cu121
    ) else if !CUDA_MAJOR! equ 11 (
        set TORCH_INDEX=cu118
    ) else (
        echo  CUDA version too old ^(!CUDA_MAJOR!.x^) — installing CPU PyTorch.
        set TORCH_INDEX=cpu
    )
) else (
    echo  No NVIDIA GPU detected — installing CPU PyTorch.
    echo  Training will work but will be slower.
)

echo.
if "%TORCH_INDEX%"=="cpu" (
    echo  Installing PyTorch ^(CPU^)...
    "%PIP%" install torch --index-url https://download.pytorch.org/whl/cpu --quiet
) else (
    echo  Installing PyTorch ^(CUDA %TORCH_INDEX%^)...
    "%PIP%" install torch --index-url https://download.pytorch.org/whl/%TORCH_INDEX% --quiet
)

if errorlevel 1 (
    echo  ERROR: PyTorch installation failed.
    pause & exit /b 1
)

REM Confirm torch installed correctly
"%PY%" -c "import torch; gpu='YES ('+torch.cuda.get_device_name(0)+')' if torch.cuda.is_available() else 'NO (CPU only)'; print('  PyTorch OK  |  GPU:', gpu)"

echo.
echo       Done.
echo.

REM ── Done ─────────────────────────────────────────────────────
echo ============================================================
echo  Setup complete.
echo.
echo  The animation server starts automatically with the studio.
echo  Run:  npm start
echo ============================================================
echo.
pause

#!/usr/bin/env bash
set -e

STUDIO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
VENV_DIR="$STUDIO_DIR/server_env"

echo "============================================================"
echo " Embody Animation Offset Studio — Setup"
echo "============================================================"
echo

# ── 1. Install SignallingWebServer dependencies ───────────────
echo "[1/3] Installing SignallingWebServer dependencies..."
cd "$STUDIO_DIR/signalling"
npm install --omit=dev
echo "      Done."
echo

# ── 2. Install Electron dependencies ─────────────────────────
echo "[2/3] Installing studio dependencies..."
cd "$STUDIO_DIR"
npm install
echo "      Done."
echo

# ── 3. Set up Python animation server ────────────────────────
echo "[3/3] Setting up Python animation server..."
echo

# Find Python 3.8+
PYTHON_CMD=""
for cmd in python3 python; do
    if command -v "$cmd" &>/dev/null; then
        version=$("$cmd" -c "import sys; print(sys.version_info >= (3,8))" 2>/dev/null)
        if [ "$version" = "True" ]; then
            PYTHON_CMD="$cmd"
            break
        fi
    fi
done

if [ -z "$PYTHON_CMD" ]; then
    echo "  ERROR: Python 3.8 or newer is required but was not found."
    echo
    echo "  Install Python via your package manager, e.g.:"
    echo "    sudo apt install python3 python3-venv"
    echo
    exit 1
fi

echo "  Found Python: $PYTHON_CMD ($($PYTHON_CMD --version))"
echo

# Create virtual environment
echo "  Creating virtual environment..."
rm -rf "$VENV_DIR"
"$PYTHON_CMD" -m venv "$VENV_DIR"

PY="$VENV_DIR/bin/python"
PIP="$VENV_DIR/bin/pip"

echo "  Upgrading pip..."
"$PY" -m pip install --upgrade pip --quiet
echo

# Install base packages
echo "  Installing server dependencies..."
"$PIP" install fastapi "uvicorn[standard]" numpy sentence-transformers --quiet
echo

# Detect GPU and install matching PyTorch
echo "  Detecting GPU..."

TORCH_INDEX="cpu"
if command -v nvidia-smi &>/dev/null; then
    CUDA_MAJOR=$(nvidia-smi | grep -oP "CUDA Version: \K[0-9]+" | head -1)
    if [ -n "$CUDA_MAJOR" ]; then
        echo "  NVIDIA GPU detected (CUDA ${CUDA_MAJOR}.x)"
        if [ "$CUDA_MAJOR" -ge 12 ]; then
            TORCH_INDEX="cu121"
        elif [ "$CUDA_MAJOR" -eq 11 ]; then
            TORCH_INDEX="cu118"
        else
            echo "  CUDA version too old — installing CPU PyTorch."
        fi
    fi
else
    echo "  No NVIDIA GPU detected — installing CPU PyTorch."
    echo "  Training will work but will be slow."
fi

echo
if [ "$TORCH_INDEX" = "cpu" ]; then
    echo "  Installing PyTorch (CPU)..."
    "$PIP" install torch --index-url https://download.pytorch.org/whl/cpu --quiet
else
    echo "  Installing PyTorch (CUDA $TORCH_INDEX)..."
    "$PIP" install torch --index-url "https://download.pytorch.org/whl/$TORCH_INDEX" --quiet
fi

"$PY" -c "import torch; gpu='YES ('+torch.cuda.get_device_name(0)+')' if torch.cuda.is_available() else 'NO (CPU only)'; print('  PyTorch OK  |  GPU:', gpu)"

echo
echo "      Done."
echo

echo "============================================================"
echo "  Setup complete."
echo
echo "  Run:  npm start"
echo "============================================================"
echo

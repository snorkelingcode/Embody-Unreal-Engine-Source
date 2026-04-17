#!/usr/bin/env bash
set -e

STUDIO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

echo "============================================================"
echo " Embody Content Creator Studio — Setup"
echo "============================================================"
echo

# ── 1. Install SignallingWebServer dependencies ───────────────
echo "[1/2] Installing SignallingWebServer dependencies..."
cd "$STUDIO_DIR/signalling"
npm install --omit=dev
echo "      Done."
echo

# ── 2. Install Electron dependencies ─────────────────────────
echo "[2/2] Installing studio dependencies..."
cd "$STUDIO_DIR"
npm install
echo "      Done."
echo

echo "============================================================"
echo "  Setup complete."
echo
echo "  Run:  npm start"
echo "============================================================"
echo

# Embody Animation Offset Studio

A cross-platform desktop tool for posing and animating a UE5 Mannequin in real time, with a built-in AI server for training and generating text-to-animation on your own data using your own GPU.

No Unreal Engine installation required. Everything is bundled.

---

## What it does

### Animate
Control individual bones via sliders, build poses, and play them back live in the UE viewport streamed directly to the app window.

### Keyframe Editor
Sequence saved poses into animations with custom durations per keyframe. Preview and save them as `.emanim` files.

### Train AI
Select any combination of your saved poses and animations as training data. Configure epochs and learning rate, name your model, and train a Motion Diffusion Transformer directly on your GPU. Training runs locally — your data never leaves your machine.

### Generate
Type a text prompt and generate a full animation from your trained model. Select from any saved model checkpoint. Results play back instantly on the mannequin.

---

## Prerequisites

| Requirement | Notes |
|---|---|
| Node.js 18+ | [nodejs.org](https://nodejs.org) |
| Python 3.8+ | [python.org](https://www.python.org/downloads) — check "Add to PATH" on Windows |
| NVIDIA GPU | CUDA 11+ recommended. CPU works but training is slow. |
| Embody game build | See [Windows](../Windows) or [Linux](../Linux) folder |

---

## Setup

Run once. Does not require Unreal Engine.

**Windows:**
```
setup.bat
```

**Linux:**
```
chmod +x setup.sh && ./setup.sh
```

The script will:
1. Install SignallingWebServer dependencies (`signalling/node_modules`)
2. Install Electron app dependencies (`node_modules`)
3. Create a Python virtual environment (`server_env/`)
4. Install AI server packages (FastAPI, PyTorch, sentence-transformers)
5. Auto-detect your CUDA version and install the matching PyTorch GPU build

---

## Running

```
npm start
```

On first launch you will be prompted to browse for the Embody executable. The path is remembered for future launches.

The studio automatically starts and manages three background processes:
- **SignallingWebServer** — WebSocket relay between the game and the browser window
- **Embody** — the packaged UE mannequin, rendering offscreen
- **Animation server** — local Python/FastAPI server handling AI training and generation

All three shut down cleanly when you close the studio.

---

## The AI model

The animation server runs a **Motion Diffusion Transformer (MDT)** trained entirely on your poses and animations.

**Architecture:**
- 8-layer transformer denoiser with self-attention + cross-attention to text
- Text conditioning via `sentence-transformers/all-mpnet-base-v2` (768-dim, frozen)
- 1000-step DDPM with cosine noise schedule, 50-step DDIM inference
- Classifier-free guidance (scale 7.5, 10% text drop during training)
- Exponential Moving Average (EMA) weights used for inference
- Mixed precision (FP16) on CUDA

**Data augmentation** (applied per epoch to grow small datasets):
- Time warping, rotation noise, scale variation
- Left/right mirror, temporal reversal

**Trained models** are saved to `anim_models/<name>.pt` and auto-loaded on startup.

**Dataset guidance for FPS upper-body additive animations:**
- 40–60 labeled poses covers the core pose space (aim directions, reload states, leans, hit reactions)
- Each concept needs at least 2–3 contrasting examples for the model to generalize
- Adding saved animations alongside poses significantly improves temporal coherence

---

## Project structure

```
├── main.js                  # Electron main — process lifecycle, IPC, platform detection
├── preload.js               # Electron context bridge
├── server.py                # Python AI server (FastAPI + PyTorch MDT)
├── setup.bat                # Windows setup
├── setup.sh                 # Linux setup
├── rebuild.bat              # Dev only: rebuild frontend from UE 5.6 source
├── requirements_server.txt  # Python package list
├── package.json
└── signalling/
    ├── dist/                # Pre-built SignallingWebServer (Node.js)
    ├── www/                 # Pre-built frontend (mannequin.html + JS bundle)
    ├── config.json
    └── package.json
```

---

## License

MIT

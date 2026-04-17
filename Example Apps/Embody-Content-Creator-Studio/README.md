# Embody Content Creator Studio

A cross-platform desktop tool for directing and recording content with a UE5 character in real time. Control the camera, set the lighting, apply post-processing effects, drive voice with ElevenLabs or your microphone, and record video clips — all streamed live from the packaged Embody game.

No Unreal Engine installation required. Everything is bundled.

---

## What it does

### Camera
Select from preset shots (Default, Extreme Close, Close, High Angle, Low Angle, Medium, Wide) or dial in a fully custom 6-axis camera position.

### Environments
Load scene presets built from spawnable assets. Each environment is a named collection of props and a directional light value. Switch environments or clear the scene with one click.

### Post-Processing
Apply color grading presets (Monochromatic, Noir, Vintage Film, Pastel Dream, Cold Blue, Teal & Orange, and more) or run animated post-processing sequences (Pulse, Day to Night, Cyberpunk Glitch, Dream Sequence, Neon Nights, Underwater, Fever Dream, Strobe, Color Shift, Breathing).

### Voice / Script (ElevenLabs)
Type a script, select a character voice, and send TTS audio directly to the character. The emotion analyzer can automatically select a matching facial expression based on your script text.

### Voice / Microphone
Record your own voiceover with the built-in mic recorder, preview it, then play it back against the live stream.

### BYOB (Bring Your Own Beat)
Drag and drop any audio or video file. Play it in sync with the stream or use it as a backing track for your recording.

### Stream Recorder
Record the live Pixel Streaming output as a video clip. Save clips individually or stitch multiple clips together with optional background audio.

---

## Prerequisites

| Requirement | Notes |
|---|---|
| Node.js 18+ | [nodejs.org](https://nodejs.org) |
| Embody game build | See [Windows](../../Windows) or [Linux](../../Linux) folder |

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

The script installs:
1. SignallingWebServer dependencies (`signalling/node_modules`)
2. Electron app dependencies (`node_modules`)

---

## Running

```
npm start
```

On first launch you will be prompted to browse for the Embody executable. The path is remembered for future launches.

The studio automatically starts and manages two background processes:
- **SignallingWebServer** — WebSocket relay between the game and the browser window
- **Embody** — the packaged UE character, rendering offscreen

Both shut down cleanly when you close the studio.

---

## ElevenLabs TTS

To use the voice/script tab, set your ElevenLabs API key and voice IDs in a `.env` file in the project root (for development) or configure them through the in-app settings panel.

```
LUCY_ELEVENLABS_VOICE_ID=your_voice_id
```

Voice generation runs through the ElevenLabs API — your API key is never stored in the source code.

---

## Project structure

```
├── main.js              # Electron main — process lifecycle, IPC
├── preload.js           # Electron context bridge
├── setup.bat            # Windows setup
├── setup.sh             # Linux setup
├── rebuild.bat          # Dev only: rebuild frontend from UE 5.6 source
├── package.json
└── signalling/
    ├── dist/            # Pre-built SignallingWebServer (Node.js)
    ├── www/             # Pre-built frontend (content-creator.html + JS bundle)
    ├── config.json
    └── package.json
```

---

## License

MIT

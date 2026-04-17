# Embody

A high-performance Unreal Engine 5 module for multi-instance MetaHuman pixel streaming, plus a full suite of open-source tooling for building on top of it — example desktop apps, a complete WebRTC command reference, and pre-packaged game builds for Windows and Linux.

---

## Repository Structure

```
├── Source/              # UE5 C++ module (pixel streaming, GPU optimization, post-processing)
├── Automation/          # Python scripts for editor and build automation
│   ├── master.py                        # Central dispatch CLI
│   ├── MetaHumans/                      # MetaHuman generation and management
│   └── Packaging/                       # UAT-based project packaging
├── Example Apps/        # Standalone Electron desktop apps
│   ├── Embody-Animation-Offset-Studio/   # Procedural bone animation + AI training
│   └── Embody-Content-Creator-Studio/    # Camera, lighting, voice, and recording tool
├── Commands/            # Full WebRTC/DataChannel command reference
│   ├── Public/          # Open commands (camera, post-processing, customization, scenes, TTS, animation)
│   └── Private/         # Advanced commands (multi-instance, optimization)
├── Windows/             # Windows game build — download link + setup guide
└── Linux/               # Linux game build — download link + setup guide
```

---

## Example Apps

Both apps are cross-platform Electron wrappers that boot the packaged Embody game offscreen and stream it live into the app window via Pixel Streaming 2. No Unreal Engine installation required.

### [Embody Animation Offset Studio](Example%20Apps/Embody-Animation-Offset-Studio)

Control individual bones via sliders, build and save poses, sequence them into keyframe animations, and train a Motion Diffusion Transformer on your own data to generate text-to-animation. Everything runs locally on your GPU.

### [Embody Content Creator Studio](Example%20Apps/Embody-Content-Creator-Studio)

Direct and record content with a live UE5 character. Camera presets, environment loading, color grading, animated post-processing effects, ElevenLabs TTS, mic recording, BYOB audio/video, and a stream recorder with clip stitching.

**Setup (both apps):**

```
# Windows
setup.bat

# Linux
chmod +x setup.sh && ./setup.sh

# Run
npm start
```

---

## Commands

A structured reference for every WebRTC DataChannel command supported by the Embody game build. Each command has a `.json` (structured) and `.txt` (plain text) file.

**Public commands include:**
- `MAN_{bone}_{pitch}_{yaw}_{roll}` — Procedural bone rotation offsets (54 bones)
- `CAMSHOT.{preset}` / `CAMSTREAM_{x}_{y}_{z}_{pitch}_{yaw}_{roll}` — Camera control
- `CGG_*` — Color grading (saturation, contrast, gamma, gain, offset per tonal range)
- `EMOTION_{name}_{strength}_{blend}_{mode}` — Facial emotion morphs
- `OF_` / `OFC_` / `EYE_*` / `HS_` / `MKUP*` — Character customization
- `SPWN` / `SPWNMOVE` / `SPWNDEL` / `DL_*` / `LIGHT*` — Scene and lighting
- `BloomInt_` / `CHROME_Int_` / `VIG_` / `GRAIN_*` — Post-processing effects
- ElevenLabs TTS, Kokoro TTS, subtitles, narration mode

See [`Commands/Public/README.md`](Commands/Public/README.md) for the full index.

---

## Game Builds

Pre-packaged UE5 builds of the Embody character — no Unreal Engine required to run.

- **[Windows](Windows/README.md)** — Download from Google Drive, run `Embody.exe`
- **[Linux](Linux/README.md)** — Download from Google Drive, run `./Embody.sh`

Key launch flags: `-RenderOffScreen` (headless), `-Windowed`, `-ForceRes -ResX=1920 -ResY=1080`

---

## Automation

Python scripts for automating editor and build tasks. All scripts use UE5's Python Remote Execution plugin — the editor must be running with remote execution enabled.

```bash
# Create a randomised MetaHuman (runs in editor)
python master.py create_metahuman

# Delete MetaHumans by name
python master.py delete_metahuman MH_Random_abc12345

# Package for Windows or Linux (does NOT require the editor)
python master.py package_windows
python master.py package_linux --shipping
python master.py package_all --clean
```

**Requirements:** UE 5.7, Python 3.x, `PythonScriptPlugin` enabled in Project Settings. Update the paths at the top of `master.py` and `Packaging/package_project.py` to match your machine.

**Note:** `Source/ThirdParty/Lib/` (Whisper CUDA binaries) is gitignored due to file size. Download from [whisper.cpp releases](https://github.com/ggerganov/whisper.cpp/releases).

---

## Source Module

The `Source/` directory is a UE5 C++ module. Drop it into any UE5 project with Pixel Streaming 2 enabled.

**Features:**
- Procedural bone driver — runtime animation offset system via `AnimNode_ProceduralBoneDriver` and `ProceduralAnimComponent`, with thread-safe snapshot copy for animation-thread safety
- Multi-instance streaming — `MultiInstanceOptimizer` with GPU optimization profiles (MaxInstances / Balanced / Quality)
- Post-processing — `EmbodyPostProcessBPLibrary` with color grading, bloom, chromatic aberration, vignette, grain
- Camera — `EmbodyCameraBPLibrary` with shot presets and 6-axis streaming control
- Emotion morphs — `EmbodyEmotionBPLibrary`
- Speech subtitles — `SpeechSubtitleBFL`
- Whisper transcription — `WhisperTranscriber` (local on-device STT via whisper.cpp)
- Room wander AI — `RoomWanderComponent` + `WanderPOI`
- Dual WebRTC + TCP interface — `TCPActor`
- Blueprint introspection utilities — `BlueprintIntrospectionBFL` (editor module)

**Setup:**

1. Copy `Source/` into your UE5 project
2. Regenerate Visual Studio project files
3. Enable the **Pixel Streaming 2** plugin in Project Settings
4. Compile

**Dependencies:** PixelStreaming2, EnhancedInput, AIModule, NavigationSystem, ProceduralMeshComponent, Networking/Sockets

**Optimization profiles:**

| Profile | Instances | Resolution | Hair | Shadows | FPS | Bitrate |
|---------|-----------|------------|------|---------|-----|---------|
| MaxInstances | 30+ | 40% + TSR | Off | 512px | 30 | 2 Mbps |
| Balanced | 15–20 | 60% + TSR | 2 MSAA | 1024px | 30 | 4 Mbps |
| Quality | 8–12 | 75% + TSR | 4 MSAA | 2048px | 30 | 6 Mbps |

---

## License

MIT

# Embody
<img width="600" height="492" alt="image" src="https://github.com/user-attachments/assets/7e435375-73a0-432f-b840-6ca60c5e18bb" />


Unreal Engine 5 is one of the most powerful rendering engines ever built, however it's also notoriously hard to work with. A feature rich editor, complex build pipelines, and a high barrier to entry for anyone who isn't a game developer or animator.

Embody removes that barrier. It's a UE5 module + tooling layer that lets you build production applications on top of Unreal Engine without touching the editor. You download a pre-packaged game build, connect to it over WebRTC or TCP, and control everything. The characters, cameras, lighting, animations, post-processing, audio, and more. The heavy UE plumbing is already done. You just build on top of it.

The example apps in this repo show what's possible out of the box. But the real value is that the same primitives that power them are available to anyone: a web app, a Python script, a Node server, a mobile app, a Unity frontend pretty much anything that can open a WebSocket can drive a photorealistic UE5 character in real time.

---

## What you can build

- **AI companion or virtual assistant** — stream a live character, send voice/TTS audio, control expressions and gaze, react to user input in real time
- **Content creation tool** — direct a character like a virtual film set: camera angles, lighting, color grading, environment, record video clips
- **Animation pipeline** — pose bones programmatically, build keyframe sequences, train a generative model on your own data, generate animations from text prompts
- **Training simulation** — put a responsive character in any scenario without scripting custom UE Blueprint logic
- **Live streamer avatar** — run the game headless on a server, stream it into any front-end

None of these require opening Unreal Engine's editor or writing a line of C++ or Blueprint.

---

## How it works

The packaged Embody game runs offscreen and connects to a SignallingWebServer over WebSocket (Pixel Streaming 2). Your application connects to the same server and sends commands as plain text strings over the WebRTC DataChannel or TCP socket.

```
Your App  ──WebSocket──►  SignallingWebServer  ──WebRTC──►  Embody (UE5, headless)
          ◄──video stream──────────────────────────────────
```

Every visual and behavioral system in the game is exposed as a command. You never recompile UE. You never open the editor. You send a string and the game responds.

---

## Repository Structure

```
├── Source/              # UE5 C++ module — the hook layer inside the game
├── Automation/          # Python scripts for editor and build automation
│   ├── master.py                          # Central dispatch CLI
│   ├── MetaHumans/                        # MetaHuman generation and management
│   └── Packaging/                         # UAT-based project packaging
├── Example Apps/        # Standalone Electron desktop apps built on the command layer
│   ├── Embody-Animation-Offset-Studio/    # Bone animation authoring + AI text-to-animation
│   └── Embody-Content-Creator-Studio/     # Camera, lighting, voice, recording tool
├── Commands/            # Full command reference for every exposed system
│   ├── Public/          # Camera, post-processing, customization, scenes, TTS, animation offsets
│   └── Private/         # Multi-instance, optimization, session tracking
├── Windows/             # Pre-packaged Windows game build
└── Linux/               # Pre-packaged Linux game build
```

---

## Getting Started

### 1. Download a game build

No Unreal Engine required.

- **[Windows build](Windows/README.md)** — download, extract, run `Embody.exe`
- **[Linux build](Linux/README.md)** — download, extract, `chmod +x Embody.sh && ./Embody.sh`

### 2. Run an example app

The example apps are cross-platform Electron wrappers. They boot the game headless and stream it into the app window automatically.

```
# Windows
setup.bat && npm start

# Linux
chmod +x setup.sh && ./setup.sh && npm start
```

On first launch you'll be prompted to point the app at your game executable. That's it.

### 3. Send your own commands

Once the game is running and the SignallingWebServer is up, connect to `ws://localhost:8080` and start sending commands:

```js
// Tilt the head down and right
dataChannel.send('MAN_head_-15.0_20.0_0.0');

// Switch to a close-up camera
dataChannel.send('CAMSHOT.Close');

// Fire a facial emotion
dataChannel.send('EMOTION_Joy_0.8_0.5_0');

// Apply a color grade
dataChannel.send('CGG_Global_Saturation_0.3_0.3_0.3_1');
```

The full command reference is in [`Commands/`](Commands/).

---

## Example Apps

### [Embody Animation Offset Studio](Example%20Apps/Embody-Animation-Offset-Studio)

Control individual bones via sliders, build and save poses, sequence them into keyframe animations, and train a Motion Diffusion Transformer on your own data to generate text-to-animation. Training can run locally on your GPU or you can connect to cloud with your own configurations. 

### [Embody Content Creator Studio](Example%20Apps/Embody-Content-Creator-Studio)

Direct and record content with a live UE5 character. Camera presets, environment loading, color grading, animated post-processing effects, ElevenLabs TTS, mic recording, BYOB audio/video, and a stream recorder with multi-clip stitching. Live Link with voice chat commands will release soon. 

---

## Command Layer

Every exposed system has a `.json` and `.txt` command file in `Commands/`. A sampling of what's available:

| Category | Commands |
|---|---|
| Animation offsets | `MAN_{bone}_{pitch}_{yaw}_{roll}` — 54 bones, real-time additive rotation |
| Camera | `CAMSHOT.{preset}`, `CAMSTREAM_{x}_{y}_{z}_{pitch}_{yaw}_{roll}` |
| Emotions | `EMOTION_{name}_{strength}_{blend}_{mode}` |
| Character | `OF_` clothing, `EYE_*` eye morphs (43 params), `HS_` hairstyle, `MKUP*` makeup, `SKC.` skin |
| Color grading | `CGG_*` — saturation, contrast, gamma, gain, offset per tonal range |
| Post-processing | `BloomInt_`, `CHROME_Int_`, `VIG_`, `GRAIN_*`, `CG_Temp_`, exposure |
| Scenes | `SPWN`, `SPWNMOVE`, `SPWNDEL`, `DL_*` lighting, `LIGHTC_`, `LIGHTI_` |
| Voice | ElevenLabs TTS, Kokoro (local), microphone, subtitles, narration mode |

See [`Commands/Public/README.md`](Commands/Public/README.md) for the full index.

---

## Automation 

This is for editor and is not related to the packaged game builds. The editor version is not yet open source as the packaged game still relies on third party plugins and assets. However these will eventually be replaced and the editor version will be open source as well. Editor content will still be public, but only content in which I have the right to open source.

Python CLI for editor and build tasks using UE5's Python Remote Execution plugin. These scripts can automate simple tasks and perform blueprint introspection allowing LLMs and agents to find the context they need, in order to modify the game, add new features, and release builds. 

```bash
# Generate a randomised MetaHuman in the editor
python master.py create_metahuman

# Delete MetaHumans
python master.py delete_metahuman MH_Random_abc12345

# Package builds (does not require the editor to be open)
python master.py package_windows --shipping
python master.py package_linux
python master.py package_all --clean
```

**Requirements:** UE 5.7, Python 3.x, `PythonScriptPlugin` enabled in Project Settings. Update the project and output paths at the top of `master.py` and `Packaging/package_project.py`.

---

## Source Module

This is for editor and is not related to the packaged game builds. The editor version is not yet open source as the packaged game still relies on third party plugins and assets. However these will eventually be replaced and the editor version will be open source as well. Editor content will still be public, but only content in which I have the right to open source.

The `Source/` directory is the C++ layer inside the game that makes all of this possible. Drop it into any UE5 project with Pixel Streaming 2 enabled and compile.

**What it exposes over the command layer:**
- Procedural bone driver (`AnimNode_ProceduralBoneDriver`, `ProceduralAnimComponent`) — thread-safe runtime animation offsets on all 54 mannequin bones
- Emotion morphs (`EmbodyEmotionBPLibrary`) — blend facial expressions at runtime
- Camera system (`EmbodyCameraBPLibrary`) — shot presets + full 6-axis streaming control
- Post-processing (`EmbodyPostProcessBPLibrary`) — color grading, bloom, chromatic aberration, vignette, grain
- Speech subtitles (`SpeechSubtitleBFL`) — timed subtitle display synced to TTS
- On-device transcription (`WhisperTranscriber`) — local STT via whisper.cpp, no cloud required
- Room wander AI (`RoomWanderComponent`, `WanderPOI`) — autonomous character navigation
- WebRTC + TCP command router (`TCPActor`) — receives all DataChannel and TCP commands and dispatches them to the systems above
- GPU optimization profiles (`MultiInstanceOptimizer`) — MaxInstances / Balanced / Quality presets with TSR and VRS

**Setup:**

1. Copy `Source/` into your UE5 project
2. Regenerate Visual Studio project files
3. Enable **Pixel Streaming 2** in Project Settings
4. Compile

**Dependencies:** PixelStreaming2, EnhancedInput, AIModule, NavigationSystem, ProceduralMeshComponent, Networking/Sockets
> `Source/ThirdParty/Lib/` (Whisper CUDA binaries, 47–124 MB) is gitignored. Download from [whisper.cpp releases](https://github.com/ggerganov/whisper.cpp/releases).
---

## License

MIT

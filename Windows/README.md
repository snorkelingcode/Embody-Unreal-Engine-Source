# Embody — Windows Build

Download the packaged Windows build from Google Drive:

**[Download Embody (Windows)](https://drive.google.com/file/d/1MR6MYONR1o9UJgunFV7TBt7Bk7PyO9aB/view?usp=sharing)**

---

## Getting started

### 1. Download and extract

Download the archive from the link above and extract it to a folder of your choice.

### 2. Use with Embody Animation Offset Studio

The Windows build is designed to run headless (offscreen) through the studio. See the [Embody Animation Offset Studio](../Apps/Embody-Animation-Offset-Studio/README.md) for setup instructions.

Once the studio is set up, run:

```
npm start
```

On first launch, browse to `Embody.exe` when prompted. The path is remembered for future launches.

---

## Running standalone (without the studio)

Double-click `Embody.exe` or launch from the command line:

```
Embody.exe
```

---

## Launch flags

| Flag | Description |
|---|---|
| `-RenderOffScreen` | Runs the game with no visible window. Used when streaming through the studio. |
| `-Windowed` | Forces a visible window. Useful for running standalone or debugging. |
| `-ForceRes -ResX=1920 -ResY=1080` | Forces a specific render resolution. Pair with `-RenderOffScreen` to control stream quality. |

---

## Requirements

- Windows 10 or Windows 11
- NVIDIA GPU with up-to-date drivers recommended
- DirectX 11 or higher

---

## Troubleshooting

**"Windows protected your PC" warning on launch**

Click **More info** → **Run anyway**. This is a standard SmartScreen prompt for unsigned executables.

**Black screen / no render**

Make sure your GPU drivers are up to date. For NVIDIA, download the latest from [nvidia.com/drivers](https://www.nvidia.com/drivers).

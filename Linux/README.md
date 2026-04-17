# Embody — Linux Build

Download the packaged Linux build from Google Drive:

**[Download Embody (Linux)](https://drive.google.com/file/d/1CfUS4QPC0OPQajw7Z5fzmtCM7TifxP3s/view?usp=sharing)**

---

## Getting started

### 1. Download and extract

Download the archive from the link above and extract it to a folder of your choice.

### 2. Make the executable runnable

```bash
chmod +x Embody.sh
```

### 3. Use with Embody Animation Offset Studio

The Linux build is designed to run headless (offscreen) through the studio. See the [Embody Animation Offset Studio](../Apps/Embody-Animation-Offset-Studio/README.md) for setup instructions.

Once the studio is set up, run:

```bash
npm start
```

On first launch, browse to the `Embody.sh` executable when prompted.

---

## Running standalone (without the studio)

If you want to run the game directly:

```bash
./Embody.sh
```

---

## Launch flags

These flags are passed as command line arguments and are especially useful for developers.

### Rendering mode

| Flag | Description |
|---|---|
| `-RenderOffScreen` | Runs the game with no visible window. Used when streaming through the studio. |
| `-Windowed` | Forces a visible window. Useful for running standalone or debugging. |
| `-ForceRes -ResX=1920 -ResY=1080` | Forces a specific render resolution. Pair with `-RenderOffScreen` to control stream quality. |

---

## Requirements

- Ubuntu 20.04+ or equivalent
- NVIDIA GPU with up-to-date drivers recommended
- Vulkan support required

---

## Troubleshooting

**"Permission denied" when launching**
```bash
chmod +x Embody.sh
```

**Black screen / no render**
Make sure your GPU drivers are up to date and Vulkan is supported:
```bash
vulkaninfo | head -20
```

**Missing libraries**
```bash
sudo apt install libc6 libstdc++6 libgcc-s1
```

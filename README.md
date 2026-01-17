# Embody

A high-performance Unreal Engine 5 module for multi-instance MetaHuman pixel streaming. Optimized for running 20-30+ concurrent streaming instances of animated digital humans on a single GPU.

## Features

- **Multi-Instance Streaming** - Manage concurrent MetaHuman instances with independent player controllers and WebRTC data channels
- **GPU Optimization Profiles** - Three presets (MaxInstances, Balanced, Quality) with TSR upscaling and Variable Rate Shading
- **Real-Time Graphics Control** - Runtime CVar manipulation for shadows, bloom, hair, subsurface scattering, and more
- **Post-Processing System** - 21 color presets plus screen overlay effects (blood, frost, glitch, VHS, etc.)
- **GPU Monitoring** - nvidia-smi integration for real-time metrics (SM utilization, VRAM, encoder load, temperature)
- **Network Communication** - Dual WebRTC + TCP interface for remote control

## Project Structure

```
Source/
├── Embody/
│   ├── Embody.h/.cpp                    # Module entry point
│   ├── Embody.Build.cs                  # Build configuration
│   ├── NEUROSYNCCharacter.h/.cpp        # Character controller
│   ├── NEUROSYNCGameMode.h/.cpp         # Game mode
│   ├── TCPActor.h/.cpp                  # TCP/WebRTC communication
│   ├── EmbodyPostProcessing.h/.cpp      # Post-processing effects
│   ├── GraphicsSettingsBFL.h/.cpp       # Graphics optimization library
│   ├── WorldTransformCollector.h/.cpp   # Level data exporter
│   ├── Rendering/
│   │   └── MultiInstanceOptimizer.h/.cpp
│   └── Streaming/
│       └── MultiInstanceManager.h/.cpp
├── Embody.Target.cs
└── EmbodyEditor.Target.cs
```

## Requirements

- Unreal Engine 5.x
- NVIDIA GPU with Pixel Streaming 2 support
- NVIDIA driver with nvidia-smi (for GPU monitoring)

## Dependencies

- PixelStreaming2
- EnhancedInput
- AIModule
- NavigationSystem
- ProceduralMeshComponent
- Networking / Sockets

## Setup

1. Copy the `Source/` directory to your UE5 project
2. Regenerate Visual Studio project files
3. Enable the **Pixel Streaming 2** plugin in Project Settings
4. Compile the project

## Usage

### Multi-Instance Streaming

```cpp
// Get the manager singleton
UMultiInstanceManager* Manager = UMultiInstanceManager::Get(GetWorld());

// Initialize
Manager->Initialize(GetWorld(), 8888, 30); // Base port, max instances

// Create and configure an instance
FStreamingInstance* Instance = Manager->CreateInstance(MetaHumanClass, SpawnTransform);
Manager->RegisterStreamer(Instance->InstanceID);
Manager->PossessMetaHuman(Instance->InstanceID);
```

### GPU Optimization

```cpp
// Apply optimization profile
UMultiInstanceOptimizer::ApplyMultiInstanceOptimizations(EOptimizationProfile::MaxInstances);

// Or use custom settings
FMultiInstanceSettings Settings;
Settings.ScreenPercentage = 50.0f;
Settings.bEnableHairRendering = false;
Settings.TargetFrameRate = 30;
UMultiInstanceOptimizer::ApplyCustomOptimizations(Settings);

// Reset when done
UMultiInstanceOptimizer::ResetToDefaultRendering();
```

### Post-Processing Effects

```cpp
// Apply color preset
PostProcessComponent->ApplyColorPreset(EColorPreset::Cyberpunk);

// Trigger screen effects
PostProcessComponent->StartBloodVignette(0.5f, 3.0f);
PostProcessComponent->PlayBloodSplatter();
PostProcessComponent->StartFrost(0.7f);

// Animate properties
PostProcessComponent->SetSaturation(1.2f);
PostProcessComponent->FocusOnActor(TargetActor, 0.5f);
```

### GPU Monitoring

```cpp
// Start monitoring
UGraphicsSettingsBFL::StartGPUMonitoring(0.5f); // Poll interval

// Get metrics
FGPUMonitorData Data = UGraphicsSettingsBFL::GetGPUMonitorData();
// Data.SMUtilization, Data.EncoderUtilization, Data.VRAMUsedMB, Data.Temperature

// Estimate capacity
int32 MaxInstances = UGraphicsSettingsBFL::GetEstimatedMaxInstances(GetWorld());
```

## Optimization Profiles

| Profile | Instances | Resolution | Hair | Shadows | FPS | Bitrate |
|---------|-----------|------------|------|---------|-----|---------|
| MaxInstances | 30+ | 40% + TSR | Off | 512px | 30 | 2 Mbps |
| Balanced | 15-20 | 60% + TSR | 2 MSAA | 1024px | 30 | 4 Mbps |
| Quality | 8-12 | 75% + TSR | 4 MSAA | 2048px | 30 | 6 Mbps |

## Color Presets

Default, Cinematic, Noir, Vintage, Cyberpunk, Horror, Dream, NightVision, Thermal, Bleach, Sepia, Arctic, Sunset, Neon, BleedingOut, Drunk, Poisoned, Electrocuted, Glitch, VHS, Retrowave

## Screen Effects

- Blood splatters and drips (animated)
- Screen cracks with healing
- Frost and rain overlays
- VHS and glitch corruption
- EMP and electrical effects
- Poison and burn status
- Lens dirt and light leaks

## Network Interface

**WebRTC**: Receives commands via Pixel Streaming 2 data channels
**TCP**: Server on port 7777 for direct connections

```cpp
// In Blueprint, bind to the delegate
TCPActor->OnStringReceived.AddDynamic(this, &AMyActor::HandleCommand);
```

## License

Proprietary - All rights reserved.

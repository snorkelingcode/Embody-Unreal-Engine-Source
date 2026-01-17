// MultiInstanceOptimizer.cpp
// Implementation of GPU optimization for multi-instance pixel streaming
//
// Copyright (c) 2025 Embody AI

#include "MultiInstanceOptimizer.h"
#include "HAL/IConsoleManager.h"
#include "Engine/Engine.h"
#include "RHI.h"
#include "RHIGlobals.h"
#include "Async/Async.h"
#include "TimerManager.h"
#include "Engine/World.h"

// Define log category
DEFINE_LOG_CATEGORY_STATIC(LogMultiInstanceOptimizer, Log, All);

// ============================================
// LOGGING HELPERS
// ============================================

void UMultiInstanceOptimizer::LogOptimization(const FString& Message)
{
    UE_LOG(LogMultiInstanceOptimizer, Log, TEXT("[Optimizer] %s"), *Message);
}

void UMultiInstanceOptimizer::LogWarning(const FString& Message)
{
    UE_LOG(LogMultiInstanceOptimizer, Warning, TEXT("[Optimizer] %s"), *Message);
}

void UMultiInstanceOptimizer::LogError(const FString& Message)
{
    UE_LOG(LogMultiInstanceOptimizer, Error, TEXT("[Optimizer] %s"), *Message);
}

// ============================================
// CVAR HELPERS
// ============================================

bool UMultiInstanceOptimizer::SetCVar(const TCHAR* CVarName, int32 Value)
{
    IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(CVarName);
    if (CVar)
    {
        int32 OldValue = CVar->GetInt();
        CVar->Set(Value, ECVF_SetByCode);
        LogOptimization(FString::Printf(TEXT("%s: %d -> %d"), CVarName, OldValue, Value));
        return true;
    }
    else
    {
        LogWarning(FString::Printf(TEXT("CVar not found: %s"), CVarName));
        return false;
    }
}

bool UMultiInstanceOptimizer::SetCVar(const TCHAR* CVarName, float Value)
{
    IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(CVarName);
    if (CVar)
    {
        float OldValue = CVar->GetFloat();
        CVar->Set(Value, ECVF_SetByCode);
        LogOptimization(FString::Printf(TEXT("%s: %.2f -> %.2f"), CVarName, OldValue, Value));
        return true;
    }
    else
    {
        LogWarning(FString::Printf(TEXT("CVar not found: %s"), CVarName));
        return false;
    }
}

int32 UMultiInstanceOptimizer::GetCVarInt(const TCHAR* CVarName)
{
    IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(CVarName);
    return CVar ? CVar->GetInt() : 0;
}

float UMultiInstanceOptimizer::GetCVarFloat(const TCHAR* CVarName)
{
    IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(CVarName);
    return CVar ? CVar->GetFloat() : 0.0f;
}

// ============================================
// SYSTEM READINESS CHECKS
// ============================================

bool UMultiInstanceOptimizer::IsRenderingSystemReady()
{
    // Check if engine is valid
    if (!GEngine)
    {
        return false;
    }

    // Check if RHI is initialized
    if (!GDynamicRHI || !GIsRHIInitialized)
    {
        return false;
    }

    // Check if we have a valid game viewport (not required for all operations)
    // But its presence indicates the rendering system is fully ready
    if (GEngine->GameViewport)
    {
        return true;
    }

    // Even without GameViewport, if RHI is ready, most CVars can be set
    return true;
}

bool UMultiInstanceOptimizer::IsVRSHardwareAvailableSafe()
{
    // Don't check VRS if RHI isn't ready
    if (!GDynamicRHI || !GIsRHIInitialized)
    {
        return false;
    }

    // VRS globals are safe to access once RHI is initialized
    return GRHISupportsAttachmentVariableRateShading || GRHISupportsPipelineVariableRateShading;
}

// ============================================
// PROFILE SETTINGS
// ============================================

FMultiInstanceSettings UMultiInstanceOptimizer::GetProfileSettings(EMultiInstanceProfile Profile)
{
    FMultiInstanceSettings Settings;

    switch (Profile)
    {
    case EMultiInstanceProfile::MaxInstances:
        // ============================================
        // MAX INSTANCES (30+): Aggressive optimization
        // Target: Minimum GPU usage per instance
        // ============================================
        Settings.ScreenPercentage = 40;
        Settings.bEnableTSR = true;
        Settings.TSRQuality = 1;
        Settings.bEnableVRS = true;
        Settings.bContrastAdaptiveVRS = true;
        Settings.bEnableHairStrands = false;  // Disable entirely for max instances
        Settings.HairMSAASamples = 1;
        Settings.bHairDeepShadows = false;
        Settings.bHairVoxelization = false;
        Settings.SSSQuality = 0;
        Settings.bSSSHalfResolution = true;
        Settings.ShadowResolution = 512;
        Settings.bVirtualShadowMaps = false;
        Settings.bContactShadows = false;
        Settings.CSMCascades = 2;
        Settings.GIMethod = 0;  // No GI
        Settings.bLumenReflections = false;
        Settings.bHardwareRayTracing = false;
        Settings.bVolumetricFog = false;
        Settings.bMotionBlur = false;
        Settings.BloomQuality = 1;
        Settings.AOLevels = 0;
        Settings.StreamingPoolSizeMB = 512;
        Settings.MaxTempMemoryMB = 64;
        Settings.MaxAnisotropy = 2;
        Settings.MaxFPS = 30;
        Settings.EncoderBitrateMbps = 2;
        Settings.EncoderMinQP = 26;
        Settings.SkeletalMeshLODBias = 2;
        Settings.ForceLOD = -1;
        Settings.bAnimationURO = true;
        break;

    case EMultiInstanceProfile::Balanced:
        // ============================================
        // BALANCED (15-20): Good quality with decent instance count
        // ============================================
        Settings.ScreenPercentage = 60;
        Settings.bEnableTSR = true;
        Settings.TSRQuality = 1;
        Settings.bEnableVRS = true;
        Settings.bContrastAdaptiveVRS = true;
        Settings.bEnableHairStrands = true;
        Settings.HairMSAASamples = 2;
        Settings.bHairDeepShadows = false;
        Settings.bHairVoxelization = false;
        Settings.SSSQuality = 1;
        Settings.bSSSHalfResolution = true;
        Settings.ShadowResolution = 1024;
        Settings.bVirtualShadowMaps = false;
        Settings.bContactShadows = false;
        Settings.CSMCascades = 3;
        Settings.GIMethod = 0;  // No GI for streaming
        Settings.bLumenReflections = false;
        Settings.bHardwareRayTracing = false;
        Settings.bVolumetricFog = false;
        Settings.bMotionBlur = false;
        Settings.BloomQuality = 2;
        Settings.AOLevels = 1;
        Settings.StreamingPoolSizeMB = 1000;
        Settings.MaxTempMemoryMB = 128;
        Settings.MaxAnisotropy = 4;
        Settings.MaxFPS = 30;
        Settings.EncoderBitrateMbps = 4;
        Settings.EncoderMinQP = 22;
        Settings.SkeletalMeshLODBias = 1;
        Settings.ForceLOD = -1;
        Settings.bAnimationURO = true;
        break;

    case EMultiInstanceProfile::Quality:
        // ============================================
        // QUALITY (8-12): Higher visual quality
        // ============================================
        Settings.ScreenPercentage = 75;
        Settings.bEnableTSR = true;
        Settings.TSRQuality = 2;
        Settings.bEnableVRS = false;  // Disable for quality
        Settings.bContrastAdaptiveVRS = false;
        Settings.bEnableHairStrands = true;
        Settings.HairMSAASamples = 4;
        Settings.bHairDeepShadows = true;
        Settings.bHairVoxelization = false;
        Settings.SSSQuality = 2;
        Settings.bSSSHalfResolution = false;
        Settings.ShadowResolution = 2048;
        Settings.bVirtualShadowMaps = false;
        Settings.bContactShadows = true;
        Settings.CSMCascades = 4;
        Settings.GIMethod = 0;  // Still no GI for streaming
        Settings.bLumenReflections = false;
        Settings.bHardwareRayTracing = false;
        Settings.bVolumetricFog = false;
        Settings.bMotionBlur = false;
        Settings.BloomQuality = 3;
        Settings.AOLevels = 2;
        Settings.StreamingPoolSizeMB = 1500;
        Settings.MaxTempMemoryMB = 192;
        Settings.MaxAnisotropy = 8;
        Settings.MaxFPS = 30;
        Settings.EncoderBitrateMbps = 6;
        Settings.EncoderMinQP = 20;
        Settings.SkeletalMeshLODBias = 0;
        Settings.ForceLOD = -1;
        Settings.bAnimationURO = true;
        break;

    case EMultiInstanceProfile::Custom:
    default:
        // Return default settings for custom profile
        break;
    }

    return Settings;
}

// ============================================
// PRIMARY OPTIMIZATION FUNCTIONS
// ============================================

EOptimizationResult UMultiInstanceOptimizer::ApplyMultiInstanceOptimizations(EMultiInstanceProfile Profile)
{
    LogOptimization(FString::Printf(TEXT("Applying profile: %s"),
        Profile == EMultiInstanceProfile::MaxInstances ? TEXT("MaxInstances") :
        Profile == EMultiInstanceProfile::Balanced ? TEXT("Balanced") :
        Profile == EMultiInstanceProfile::Quality ? TEXT("Quality") : TEXT("Custom")));

    if (Profile == EMultiInstanceProfile::Custom)
    {
        LogWarning(TEXT("Custom profile selected - use ApplyCustomOptimizations() instead"));
        return EOptimizationResult::Failed;
    }

    FMultiInstanceSettings Settings = GetProfileSettings(Profile);
    return ApplyCustomOptimizations(Settings);
}

EOptimizationResult UMultiInstanceOptimizer::ApplyCustomOptimizations(const FMultiInstanceSettings& Settings)
{
    // Check if rendering system is ready
    if (!IsRenderingSystemReady())
    {
        LogWarning(TEXT("Rendering system not ready - deferring optimization"));

        // Defer to next frame using AsyncTask
        FMultiInstanceSettings SettingsCopy = Settings;
        AsyncTask(ENamedThreads::GameThread, [SettingsCopy]()
        {
            // Try again on next game thread tick
            if (IsRenderingSystemReady())
            {
                ApplyOptimizationsInternal(SettingsCopy);
            }
            else
            {
                // Still not ready, try with a timer
                if (GEngine && GEngine->GetWorld())
                {
                    FTimerHandle TimerHandle;
                    GEngine->GetWorld()->GetTimerManager().SetTimer(
                        TimerHandle,
                        [SettingsCopy]()
                        {
                            ApplyOptimizationsInternal(SettingsCopy);
                        },
                        0.1f, // 100ms delay
                        false
                    );
                }
            }
        });

        return EOptimizationResult::Pending;
    }

    return ApplyOptimizationsInternal(Settings);
}

EOptimizationResult UMultiInstanceOptimizer::ApplyOptimizationsInternal(const FMultiInstanceSettings& Settings)
{
    LogOptimization(TEXT("========================================"));
    LogOptimization(TEXT("Applying Multi-Instance Optimizations"));
    LogOptimization(TEXT("========================================"));

    bool bAllSuccess = true;

    // ============================================
    // RESOLUTION & UPSCALING
    // ============================================
    LogOptimization(TEXT("--- Resolution & Upscaling ---"));

    bAllSuccess &= SetCVar(TEXT("r.ScreenPercentage"), (float)Settings.ScreenPercentage);

    if (Settings.bEnableTSR)
    {
        bAllSuccess &= SetCVar(TEXT("r.AntiAliasingMethod"), 4);  // TSR
        bAllSuccess &= SetCVar(TEXT("r.TemporalAA.Upsampling"), 1);
        bAllSuccess &= SetCVar(TEXT("r.TSR.History.UpdateQuality"), Settings.TSRQuality);
    }

    // ============================================
    // VARIABLE RATE SHADING
    // ============================================
    LogOptimization(TEXT("--- Variable Rate Shading ---"));

    if (Settings.bEnableVRS && IsVRSHardwareAvailableSafe())
    {
        bAllSuccess &= SetCVar(TEXT("r.VRS.Enable"), 1);
        bAllSuccess &= SetCVar(TEXT("r.VRS.ContrastAdaptiveShading"), Settings.bContrastAdaptiveVRS ? 1 : 0);
        LogOptimization(TEXT("VRS enabled (hardware supported)"));
    }
    else if (Settings.bEnableVRS)
    {
        LogWarning(TEXT("VRS requested but hardware not supported"));
    }
    else
    {
        bAllSuccess &= SetCVar(TEXT("r.VRS.Enable"), 0);
    }

    // ============================================
    // METAHUMAN - HAIR
    // ============================================
    LogOptimization(TEXT("--- MetaHuman Hair ---"));

    bAllSuccess &= SetCVar(TEXT("r.HairStrands.Enable"), Settings.bEnableHairStrands ? 1 : 0);

    if (Settings.bEnableHairStrands)
    {
        bAllSuccess &= SetCVar(TEXT("r.HairStrands.Visibility.MSAA.SamplePerPixel"), Settings.HairMSAASamples);
        bAllSuccess &= SetCVar(TEXT("r.HairStrands.DeepShadow.Enable"), Settings.bHairDeepShadows ? 1 : 0);
        bAllSuccess &= SetCVar(TEXT("r.HairStrands.Voxelization.Enable"), Settings.bHairVoxelization ? 1 : 0);
        bAllSuccess &= SetCVar(TEXT("r.HairStrands.SkyLighting.IntegrationType"), 1);  // Simple
        bAllSuccess &= SetCVar(TEXT("r.HairStrands.SkyAO.Enable"), 0);  // Disable sky AO
    }

    // ============================================
    // METAHUMAN - SKIN/SSS
    // ============================================
    LogOptimization(TEXT("--- MetaHuman Skin/SSS ---"));

    bAllSuccess &= SetCVar(TEXT("r.SSS.Quality"), Settings.SSSQuality);
    bAllSuccess &= SetCVar(TEXT("r.SSS.HalfRes"), Settings.bSSSHalfResolution ? 1 : 0);
    bAllSuccess &= SetCVar(TEXT("r.SSS.SampleSet"), 1);  // Reduced sample set

    // ============================================
    // SHADOWS
    // ============================================
    LogOptimization(TEXT("--- Shadows ---"));

    bAllSuccess &= SetCVar(TEXT("r.Shadow.MaxResolution"), Settings.ShadowResolution);
    bAllSuccess &= SetCVar(TEXT("r.Shadow.MaxCSMResolution"), Settings.ShadowResolution);
    bAllSuccess &= SetCVar(TEXT("r.Shadow.Virtual.Enable"), Settings.bVirtualShadowMaps ? 1 : 0);
    bAllSuccess &= SetCVar(TEXT("r.ContactShadows"), Settings.bContactShadows ? 1 : 0);
    bAllSuccess &= SetCVar(TEXT("r.Shadow.CSM.MaxCascades"), Settings.CSMCascades);
    bAllSuccess &= SetCVar(TEXT("r.Shadow.RadiusThreshold"), 0.05f);  // Cull small shadows
    bAllSuccess &= SetCVar(TEXT("r.Shadow.CSMCaching"), 1);  // Enable CSM caching

    // ============================================
    // GLOBAL ILLUMINATION
    // ============================================
    LogOptimization(TEXT("--- Global Illumination ---"));

    bAllSuccess &= SetCVar(TEXT("r.DynamicGlobalIlluminationMethod"), Settings.GIMethod);
    bAllSuccess &= SetCVar(TEXT("r.Lumen.Reflections.Allow"), Settings.bLumenReflections ? 1 : 0);
    bAllSuccess &= SetCVar(TEXT("r.Lumen.HardwareRayTracing"), Settings.bHardwareRayTracing ? 1 : 0);
    bAllSuccess &= SetCVar(TEXT("r.RayTracing.Shadows"), 0);  // Always disable RT shadows for streaming
    bAllSuccess &= SetCVar(TEXT("r.Lumen.TraceMeshSDFs"), 0);  // Disable mesh SDFs (deprecated in 5.6+)

    // ============================================
    // REFLECTIONS
    // ============================================
    LogOptimization(TEXT("--- Reflections ---"));

    bAllSuccess &= SetCVar(TEXT("r.ReflectionMethod"), 2);  // SSR
    bAllSuccess &= SetCVar(TEXT("r.SSR.Quality"), 2);
    bAllSuccess &= SetCVar(TEXT("r.SSR.HalfResSceneColor"), 1);

    // ============================================
    // POST PROCESSING
    // ============================================
    LogOptimization(TEXT("--- Post Processing ---"));

    bAllSuccess &= SetCVar(TEXT("r.VolumetricFog"), Settings.bVolumetricFog ? 1 : 0);
    bAllSuccess &= SetCVar(TEXT("r.MotionBlurQuality"), Settings.bMotionBlur ? 3 : 0);
    bAllSuccess &= SetCVar(TEXT("r.MotionBlur.Amount"), Settings.bMotionBlur ? 0.5f : 0.0f);
    bAllSuccess &= SetCVar(TEXT("r.BloomQuality"), Settings.BloomQuality);
    bAllSuccess &= SetCVar(TEXT("r.AmbientOcclusionLevels"), Settings.AOLevels);
    bAllSuccess &= SetCVar(TEXT("r.DepthOfFieldQuality"), 1);  // Low DOF
    bAllSuccess &= SetCVar(TEXT("r.LensFlareQuality"), 0);  // Disable lens flares

    // ============================================
    // STREAMING & MEMORY
    // ============================================
    LogOptimization(TEXT("--- Streaming & Memory ---"));

    bAllSuccess &= SetCVar(TEXT("r.Streaming.PoolSize"), Settings.StreamingPoolSizeMB);
    bAllSuccess &= SetCVar(TEXT("r.Streaming.MaxTempMemoryAllowed"), Settings.MaxTempMemoryMB);
    bAllSuccess &= SetCVar(TEXT("r.MaxAnisotropy"), Settings.MaxAnisotropy);
    bAllSuccess &= SetCVar(TEXT("r.Streaming.Boost"), 0);  // Disable streaming boost
    bAllSuccess &= SetCVar(TEXT("r.Streaming.FullyLoadUsedTextures"), 0);

    // ============================================
    // FRAME RATE
    // ============================================
    LogOptimization(TEXT("--- Frame Rate ---"));

    bAllSuccess &= SetCVar(TEXT("t.MaxFPS"), Settings.MaxFPS);

    // ============================================
    // LOD & SKELETAL MESH
    // ============================================
    LogOptimization(TEXT("--- LOD & Skeletal Mesh ---"));

    bAllSuccess &= SetCVar(TEXT("r.SkeletalMeshLODBias"), Settings.SkeletalMeshLODBias);
    bAllSuccess &= SetCVar(TEXT("r.ForceLOD"), Settings.ForceLOD);
    bAllSuccess &= SetCVar(TEXT("a.URO.Enable"), Settings.bAnimationURO ? 1 : 0);
    bAllSuccess &= SetCVar(TEXT("a.URO.ForceInterpolation"), 1);

    // ============================================
    // ADDITIONAL OPTIMIZATIONS
    // ============================================
    LogOptimization(TEXT("--- Additional Optimizations ---"));

    // Nanite settings for multi-instance
    bAllSuccess &= SetCVar(TEXT("r.Nanite.MaxPixelsPerEdge"), 2);  // Less detail
    bAllSuccess &= SetCVar(TEXT("r.Nanite.ViewMeshLODBias.Offset"), 1);

    // Disable expensive features
    bAllSuccess &= SetCVar(TEXT("r.VolumetricCloud"), 0);  // Disable volumetric clouds
    bAllSuccess &= SetCVar(TEXT("r.HeterogeneousVolumes"), 0);  // Disable heterogeneous volumes
    bAllSuccess &= SetCVar(TEXT("r.Tonemapper.Quality"), 1);  // Low tonemapper quality

    // Skin cache optimization
    bAllSuccess &= SetCVar(TEXT("r.SkinCache.Mode"), 1);
    bAllSuccess &= SetCVar(TEXT("r.SkinCache.RecomputeTangents"), 0);  // Disable for perf

    // GPU-specific optimizations
    bAllSuccess &= SetCVar(TEXT("r.GPUCrashDebugging"), 0);  // Disable GPU crash debugging in prod

    LogOptimization(TEXT("========================================"));
    LogOptimization(FString::Printf(TEXT("Optimization complete. Success: %s"),
        bAllSuccess ? TEXT("All CVars applied") : TEXT("Some CVars failed")));
    LogOptimization(TEXT("========================================"));

    return bAllSuccess ? EOptimizationResult::Success : EOptimizationResult::PartialSuccess;
}

void UMultiInstanceOptimizer::ResetToDefaultRendering()
{
    LogOptimization(TEXT("Resetting to default rendering settings"));

    // Resolution
    SetCVar(TEXT("r.ScreenPercentage"), 100.0f);
    SetCVar(TEXT("r.AntiAliasingMethod"), 4);  // TSR default

    // VRS
    SetCVar(TEXT("r.VRS.Enable"), 0);

    // Hair
    SetCVar(TEXT("r.HairStrands.Enable"), 1);
    SetCVar(TEXT("r.HairStrands.Visibility.MSAA.SamplePerPixel"), 4);

    // SSS
    SetCVar(TEXT("r.SSS.Quality"), 3);
    SetCVar(TEXT("r.SSS.HalfRes"), 0);

    // Shadows
    SetCVar(TEXT("r.Shadow.MaxResolution"), 4096);
    SetCVar(TEXT("r.Shadow.Virtual.Enable"), 1);

    // GI
    SetCVar(TEXT("r.DynamicGlobalIlluminationMethod"), 2);  // Lumen
    SetCVar(TEXT("r.Lumen.Reflections.Allow"), 1);

    // Post processing
    SetCVar(TEXT("r.VolumetricFog"), 1);
    SetCVar(TEXT("r.BloomQuality"), 5);
    SetCVar(TEXT("r.AmbientOcclusionLevels"), 3);

    // Streaming
    SetCVar(TEXT("r.Streaming.PoolSize"), 3000);
    SetCVar(TEXT("r.MaxAnisotropy"), 8);

    // Frame rate
    SetCVar(TEXT("t.MaxFPS"), 0);  // Unlimited

    // LOD
    SetCVar(TEXT("r.SkeletalMeshLODBias"), 0);
    SetCVar(TEXT("r.ForceLOD"), -1);

    LogOptimization(TEXT("Default rendering restored"));
}

// ============================================
// QUICK OPTIMIZATION FUNCTIONS
// ============================================

void UMultiInstanceOptimizer::ApplyMaximumSMReduction()
{
    LogOptimization(TEXT("Applying MAXIMUM SM reduction for highest instance count"));

    // Ultra-aggressive settings
    SetCVar(TEXT("r.ScreenPercentage"), 25.0f);  // Minimum resolution
    SetCVar(TEXT("r.AntiAliasingMethod"), 4);  // TSR
    SetCVar(TEXT("r.HairStrands.Enable"), 0);  // No hair
    SetCVar(TEXT("r.SSS.Quality"), 0);
    SetCVar(TEXT("r.SSS.HalfRes"), 1);
    SetCVar(TEXT("r.Shadow.MaxResolution"), 256);
    SetCVar(TEXT("r.Shadow.Virtual.Enable"), 0);
    SetCVar(TEXT("r.DynamicGlobalIlluminationMethod"), 0);
    SetCVar(TEXT("r.VolumetricFog"), 0);
    SetCVar(TEXT("r.BloomQuality"), 0);
    SetCVar(TEXT("r.AmbientOcclusionLevels"), 0);
    SetCVar(TEXT("r.Streaming.PoolSize"), 256);
    SetCVar(TEXT("t.MaxFPS"), 24);
    SetCVar(TEXT("r.SkeletalMeshLODBias"), 3);

    // Enable VRS if available
    if (IsVRSHardwareAvailableSafe())
    {
        SetCVar(TEXT("r.VRS.Enable"), 1);
        SetCVar(TEXT("r.VRS.ContrastAdaptiveShading"), 1);
    }

    LogOptimization(TEXT("Maximum SM reduction applied - targeting 30+ instances"));
}

void UMultiInstanceOptimizer::EnableMetaHumanOptimization()
{
    LogOptimization(TEXT("Enabling MetaHuman-specific optimizations"));

    // Hair optimizations
    SetCVar(TEXT("r.HairStrands.Enable"), 1);
    SetCVar(TEXT("r.HairStrands.Visibility.MSAA.SamplePerPixel"), 1);
    SetCVar(TEXT("r.HairStrands.DeepShadow.Enable"), 0);
    SetCVar(TEXT("r.HairStrands.Voxelization.Enable"), 0);
    SetCVar(TEXT("r.HairStrands.SkyLighting.IntegrationType"), 1);
    SetCVar(TEXT("r.HairStrands.SkyAO.Enable"), 0);
    SetCVar(TEXT("r.HairStrands.RasterLOD"), 1);

    // Skin/SSS optimizations
    SetCVar(TEXT("r.SSS.Quality"), 1);
    SetCVar(TEXT("r.SSS.HalfRes"), 1);
    SetCVar(TEXT("r.SSS.SampleSet"), 1);
    SetCVar(TEXT("r.SSS.Burley.Quality"), 1);

    // Eye quality
    SetCVar(TEXT("r.Eye.Quality"), 1);

    // Morph targets / skeletal mesh
    SetCVar(TEXT("r.MorphTarget.Mode"), 1);
    SetCVar(TEXT("r.SkinCache.Mode"), 1);
    SetCVar(TEXT("r.SkeletalMeshLODBias"), 1);

    // Animation URO
    SetCVar(TEXT("a.URO.Enable"), 1);
    SetCVar(TEXT("a.URO.ForceInterpolation"), 1);

    LogOptimization(TEXT("MetaHuman optimizations enabled"));
}

void UMultiInstanceOptimizer::ConfigurePixelStreamingEncoder(int32 BitrateMbps, int32 MaxFPS)
{
    LogOptimization(FString::Printf(TEXT("Configuring Pixel Streaming encoder: %d Mbps, %d FPS"), BitrateMbps, MaxFPS));

    // Frame rate cap
    SetCVar(TEXT("t.MaxFPS"), MaxFPS);

    // Pixel streaming encoder settings
    // Note: Some of these may require Pixel Streaming 2 plugin to be loaded
    SetCVar(TEXT("PixelStreaming.Encoder.TargetBitrate"), BitrateMbps * 1000000);
    SetCVar(TEXT("PixelStreaming.Encoder.MinQP"), 20);
    SetCVar(TEXT("PixelStreaming.Encoder.MaxQP"), 40);
    SetCVar(TEXT("PixelStreaming.WebRTC.MinBitrate"), (BitrateMbps / 2) * 1000000);
    SetCVar(TEXT("PixelStreaming.WebRTC.MaxBitrate"), BitrateMbps * 1000000);
    SetCVar(TEXT("PixelStreaming.WebRTC.StartBitrate"), (BitrateMbps * 3 / 4) * 1000000);
    SetCVar(TEXT("PixelStreaming.WebRTC.Fps"), MaxFPS);

    LogOptimization(TEXT("Pixel Streaming encoder configured"));
}

// ============================================
// INDIVIDUAL SETTING FUNCTIONS
// ============================================

void UMultiInstanceOptimizer::SetScreenPercentage(int32 Percentage, bool bEnableTSR)
{
    int32 ClampedPct = FMath::Clamp(Percentage, 25, 100);
    SetCVar(TEXT("r.ScreenPercentage"), (float)ClampedPct);

    if (bEnableTSR)
    {
        SetCVar(TEXT("r.AntiAliasingMethod"), 4);
        SetCVar(TEXT("r.TemporalAA.Upsampling"), 1);
    }

    LogOptimization(FString::Printf(TEXT("Screen percentage set to %d%% with TSR %s"),
        ClampedPct, bEnableTSR ? TEXT("enabled") : TEXT("disabled")));
}

void UMultiInstanceOptimizer::ConfigureVRS(bool bEnable, bool bContrastAdaptive)
{
    if (bEnable)
    {
        if (IsVRSHardwareAvailableSafe())
        {
            SetCVar(TEXT("r.VRS.Enable"), 1);
            SetCVar(TEXT("r.VRS.ContrastAdaptiveShading"), bContrastAdaptive ? 1 : 0);
            LogOptimization(TEXT("VRS enabled"));
        }
        else
        {
            LogWarning(TEXT("VRS requested but hardware not available"));
        }
    }
    else
    {
        SetCVar(TEXT("r.VRS.Enable"), 0);
        LogOptimization(TEXT("VRS disabled"));
    }
}

void UMultiInstanceOptimizer::ConfigureHairRendering(bool bEnable, int32 MSAASamples)
{
    SetCVar(TEXT("r.HairStrands.Enable"), bEnable ? 1 : 0);

    if (bEnable)
    {
        int32 ClampedSamples = FMath::Clamp(MSAASamples, 1, 8);
        SetCVar(TEXT("r.HairStrands.Visibility.MSAA.SamplePerPixel"), ClampedSamples);
    }

    LogOptimization(FString::Printf(TEXT("Hair rendering %s with %d MSAA samples"),
        bEnable ? TEXT("enabled") : TEXT("disabled"), MSAASamples));
}

void UMultiInstanceOptimizer::ConfigureShadows(int32 Resolution, bool bVirtualShadowMaps)
{
    int32 ClampedRes = FMath::Clamp(Resolution, 256, 4096);
    SetCVar(TEXT("r.Shadow.MaxResolution"), ClampedRes);
    SetCVar(TEXT("r.Shadow.MaxCSMResolution"), ClampedRes);
    SetCVar(TEXT("r.Shadow.Virtual.Enable"), bVirtualShadowMaps ? 1 : 0);

    LogOptimization(FString::Printf(TEXT("Shadows configured: %d resolution, VSM %s"),
        ClampedRes, bVirtualShadowMaps ? TEXT("enabled") : TEXT("disabled")));
}

void UMultiInstanceOptimizer::SetStreamingPoolSize(int32 SizeMB)
{
    int32 ClampedSize = FMath::Clamp(SizeMB, 256, 4096);
    SetCVar(TEXT("r.Streaming.PoolSize"), ClampedSize);
    LogOptimization(FString::Printf(TEXT("Streaming pool size set to %d MB"), ClampedSize));
}

// ============================================
// QUERY FUNCTIONS
// ============================================

bool UMultiInstanceOptimizer::IsVRSSupported()
{
    return IsVRSHardwareAvailableSafe();
}

int32 UMultiInstanceOptimizer::GetCurrentScreenPercentage()
{
    return (int32)GetCVarFloat(TEXT("r.ScreenPercentage"));
}

int32 UMultiInstanceOptimizer::GetEstimatedMaxInstances()
{
    // Get current screen percentage
    float ScreenPct = GetCVarFloat(TEXT("r.ScreenPercentage"));
    if (ScreenPct <= 0) ScreenPct = 100.0f;

    // Get current settings to estimate GPU load per instance
    bool bHairEnabled = GetCVarInt(TEXT("r.HairStrands.Enable")) != 0;
    int32 SSSQuality = GetCVarInt(TEXT("r.SSS.Quality"));
    bool bVSM = GetCVarInt(TEXT("r.Shadow.Virtual.Enable")) != 0;
    int32 GIMethod = GetCVarInt(TEXT("r.DynamicGlobalIlluminationMethod"));
    bool bVolumetricFog = GetCVarInt(TEXT("r.VolumetricFog")) != 0;

    // Base estimate: 100% screen = ~30% GPU, scales with pixel count
    float PixelScale = (ScreenPct / 100.0f) * (ScreenPct / 100.0f);
    float BaseGPUPct = 30.0f * PixelScale;

    // Add costs for expensive features
    if (bHairEnabled) BaseGPUPct += 15.0f;
    if (SSSQuality > 0) BaseGPUPct += 5.0f;
    if (bVSM) BaseGPUPct += 10.0f;
    if (GIMethod == 2) BaseGPUPct += 15.0f;  // Lumen
    if (bVolumetricFog) BaseGPUPct += 5.0f;

    // Target 90% GPU utilization
    float TargetUtilization = 90.0f;
    int32 MaxInstances = FMath::Max(1, FMath::FloorToInt(TargetUtilization / BaseGPUPct));

    LogOptimization(FString::Printf(TEXT("Estimated max instances: %d (%.1f%% GPU per instance)"),
        MaxInstances, BaseGPUPct));

    return MaxInstances;
}

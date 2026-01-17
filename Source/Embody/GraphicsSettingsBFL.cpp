// GraphicsSettingsBFL.cpp
// Graphics settings control for Pixel Streaming optimization
// Thread-safe implementation for multi-client Pixel Streaming

#include "GraphicsSettingsBFL.h"
#include "HAL/IConsoleManager.h"
#include "Engine/Engine.h"
#include "Misc/Paths.h"
#include "HAL/PlatformProcess.h"
#include "Misc/DateTime.h"
#include "RHI.h"
#include "RHIGlobals.h"
#include "Async/Async.h"
#include "HAL/RunnableThread.h"
#include "EngineUtils.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Rendering/SkeletalMeshRenderData.h"

DEFINE_LOG_CATEGORY_STATIC(LogGraphicsSettings, Log, All);

// Static variable initialization
EEmbodyGraphicsPreset UGraphicsSettingsBFL::CurrentPreset = EEmbodyGraphicsPreset::Optimized;
FEmbodyGPUMonitorData UGraphicsSettingsBFL::CachedGPUData;
bool UGraphicsSettingsBFL::bIsMonitoring = false;
int32 UGraphicsSettingsBFL::CurrentInstanceCount = 1;

// Thread safety primitives
FCriticalSection UGraphicsSettingsBFL::GPUDataMutex;
FCriticalSection UGraphicsSettingsBFL::SettingsMutex;
TAtomic<bool> UGraphicsSettingsBFL::bAsyncPollInProgress(false);
TAtomic<int64> UGraphicsSettingsBFL::LastPollTimestamp(0);

// ============================================
// CVAR HELPERS
// ============================================

void UGraphicsSettingsBFL::SetCVar(const TCHAR* CVarName, int32 Value)
{
    IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(CVarName);
    if (CVar)
    {
        int32 OldValue = CVar->GetInt();
        CVar->Set(Value, ECVF_SetByCode);
        UE_LOG(LogGraphicsSettings, Log, TEXT("SetCVar: %s = %d (was %d)"), CVarName, Value, OldValue);
    }
    else
    {
        UE_LOG(LogGraphicsSettings, Warning, TEXT("SetCVar: CVar not found: %s"), CVarName);
    }
}

void UGraphicsSettingsBFL::SetCVar(const TCHAR* CVarName, float Value)
{
    IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(CVarName);
    if (CVar)
    {
        float OldValue = CVar->GetFloat();
        CVar->Set(Value, ECVF_SetByCode);
        UE_LOG(LogGraphicsSettings, Log, TEXT("SetCVar: %s = %.2f (was %.2f)"), CVarName, Value, OldValue);
    }
    else
    {
        UE_LOG(LogGraphicsSettings, Warning, TEXT("SetCVar: CVar not found: %s"), CVarName);
    }
}

int32 UGraphicsSettingsBFL::GetCVarInt(const TCHAR* CVarName)
{
    IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(CVarName);
    if (CVar)
    {
        return CVar->GetInt();
    }
    return 0;
}

float UGraphicsSettingsBFL::GetCVarFloat(const TCHAR* CVarName)
{
    IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(CVarName);
    if (CVar)
    {
        return CVar->GetFloat();
    }
    return 0.0f;
}

// ============================================
// PRESET MANAGEMENT
// ============================================

void UGraphicsSettingsBFL::SetGraphicsPreset(EEmbodyGraphicsPreset Preset)
{
    FScopeLock Lock(&SettingsMutex);
    CurrentPreset = Preset;

    if (Preset == EEmbodyGraphicsPreset::Optimized)
    {
        // ============================================
        // OPTIMIZED - Gameplay/Third Person
        // Target: 170+ FPS
        // ============================================

        // Motion Blur (OFF - causes metahuman distortion)
        SetCVar(TEXT("r.MotionBlurQuality"), 0);
        SetCVar(TEXT("r.MotionBlur.Amount"), 0.0f);

        // Textures
        SetCVar(TEXT("r.MipMapLODBias"), 0);
        SetCVar(TEXT("r.Streaming.PoolSize"), 2000);
        SetCVar(TEXT("r.Streaming.FullyLoadUsedTextures"), 0);
        SetCVar(TEXT("r.Streaming.MaxTempMemoryAllowed"), 256);
        SetCVar(TEXT("r.MaxAnisotropy"), 8);

        // Skin/SSS
        SetCVar(TEXT("r.SSS.Quality"), 0);
        SetCVar(TEXT("r.SSS.HalfRes"), 1);
        SetCVar(TEXT("r.SSS.Scale"), 1.0f);
        SetCVar(TEXT("r.SSS.SampleSet"), 1);

        // Shadows
        SetCVar(TEXT("r.Shadow.MaxResolution"), 1024);
        SetCVar(TEXT("r.Shadow.MaxCSMResolution"), 1024);
        SetCVar(TEXT("r.Shadow.RadiusThreshold"), 0.05f);
        SetCVar(TEXT("r.Shadow.DistanceScale"), 1.0f);
        SetCVar(TEXT("r.Shadow.CSM.MaxCascades"), 4);
        SetCVar(TEXT("r.ContactShadows"), 0);
        SetCVar(TEXT("r.Shadow.Virtual.Enable"), 0);
        SetCVar(TEXT("r.Shadow.CSMCaching"), 1);

        // Hair
        SetCVar(TEXT("r.HairStrands.Enable"), 1);
        SetCVar(TEXT("r.HairStrands.SkyLighting.IntegrationType"), 1);
        SetCVar(TEXT("r.HairStrands.SkyAO.Enable"), 0);
        SetCVar(TEXT("r.HairStrands.Visibility.MSAA.SamplePerPixel"), 2);

        // Global Illumination
        SetCVar(TEXT("r.DynamicGlobalIlluminationMethod"), 0);
        SetCVar(TEXT("r.Lumen.Reflections.Allow"), 0);
        SetCVar(TEXT("r.Lumen.Final.Quality"), 1);

        // Reflections
        SetCVar(TEXT("r.ReflectionMethod"), 2);
        SetCVar(TEXT("r.SSR.Quality"), 2);
        SetCVar(TEXT("r.SSR.HalfResSceneColor"), 1);

        // Anti-Aliasing - TSR at lower settings
        SetCVar(TEXT("r.AntiAliasingMethod"), 4);
        SetCVar(TEXT("r.ScreenPercentage"), 100);
        SetCVar(TEXT("r.TSR.History.ScreenPercentage"), 100);
        SetCVar(TEXT("r.TSR.History.UpdateQuality"), 1);
        SetCVar(TEXT("r.TemporalAA.Quality"), 1);
        SetCVar(TEXT("r.TemporalAASamples"), 4);

        // DOF
        SetCVar(TEXT("r.DOF.Kernel.MaxBackgroundRadius"), 0.05f);
        SetCVar(TEXT("r.DOF.Kernel.MaxForegroundRadius"), 0.05f);
        SetCVar(TEXT("r.DOF.Gather.RingCount"), 3);

        // Post Processing
        SetCVar(TEXT("r.Tonemapper.Quality"), 3);
        SetCVar(TEXT("r.BloomQuality"), 3);
        SetCVar(TEXT("r.LensFlareQuality"), 1);
        SetCVar(TEXT("r.EyeAdaptationQuality"), 2);

        // Volumetrics
        SetCVar(TEXT("r.VolumetricFog"), 0);
        SetCVar(TEXT("r.VolumetricCloud"), 1);

        // AO
        SetCVar(TEXT("r.AmbientOcclusionLevels"), 0);

        // Ray Tracing
        SetCVar(TEXT("r.RayTracing.Shadows"), 0);
        SetCVar(TEXT("r.Lumen.HardwareRayTracing"), 0);

        // LOD
        SetCVar(TEXT("r.ForceLOD"), -1);
        SetCVar(TEXT("r.SkeletalMeshLODBias"), 0);
        SetCVar(TEXT("r.StaticMeshLODDistanceScale"), 1.0f);
    }
    else if (Preset == EEmbodyGraphicsPreset::MaxSettings)
    {
        // ============================================
        // MAX SETTINGS - VTuber/Close-up/Customization
        // Target: Cinematic MetaHuman Quality on 4090
        // ============================================

        // Motion Blur (OFF - causes metahuman distortion)
        SetCVar(TEXT("r.MotionBlurQuality"), 0);
        SetCVar(TEXT("r.MotionBlur.Amount"), 0.0f);

        // Textures (Maximum Detail)
        SetCVar(TEXT("r.MipMapLODBias"), -2);
        SetCVar(TEXT("r.Streaming.PoolSize"), 8000);
        SetCVar(TEXT("r.Streaming.FullyLoadUsedTextures"), 1);
        SetCVar(TEXT("r.Streaming.MaxTempMemoryAllowed"), 512);
        SetCVar(TEXT("r.MaxAnisotropy"), 16);
        SetCVar(TEXT("r.Streaming.Boost"), 1);
        SetCVar(TEXT("r.Streaming.UseFixedPoolSize"), 1);

        // Skin SSS
        SetCVar(TEXT("r.SSS.Quality"), 3);
        SetCVar(TEXT("r.SSS.HalfRes"), 0);
        SetCVar(TEXT("r.SSS.Scale"), 1.0f);
        SetCVar(TEXT("r.SSS.SampleSet"), 2);
        SetCVar(TEXT("r.SSS.Checkerboard"), 0);
        SetCVar(TEXT("r.SeparateTranslucency"), 1);
        SetCVar(TEXT("r.SSS.Burley.Quality"), 2);
        SetCVar(TEXT("r.SSS.Burley.NumSamplesNearLight"), 16);

        // Eyes
        SetCVar(TEXT("r.Eye.Quality"), 2);
        SetCVar(TEXT("r.Refraction.Blur"), 1);

        // Shadows - 8K Virtual Shadow Maps
        SetCVar(TEXT("r.Shadow.MaxResolution"), 8192);
        SetCVar(TEXT("r.Shadow.MaxCSMResolution"), 8192);
        SetCVar(TEXT("r.Shadow.RadiusThreshold"), 0.01f);
        SetCVar(TEXT("r.Shadow.DistanceScale"), 2.0f);
        SetCVar(TEXT("r.Shadow.CSM.MaxCascades"), 10);
        SetCVar(TEXT("r.ContactShadows"), 1);
        SetCVar(TEXT("r.Shadow.Virtual.Enable"), 1);
        SetCVar(TEXT("r.Shadow.Virtual.MaxPhysicalPages"), 4096);
        SetCVar(TEXT("r.Shadow.Virtual.ResolutionLodBiasLocal"), -1.0f);
        SetCVar(TEXT("r.Shadow.CSMCaching"), 0);

        // Hair - Full Strand Quality
        SetCVar(TEXT("r.HairStrands.Enable"), 1);
        SetCVar(TEXT("r.HairStrands.SkyLighting.IntegrationType"), 2);
        SetCVar(TEXT("r.HairStrands.SkyAO.Enable"), 1);
        SetCVar(TEXT("r.HairStrands.Visibility.MSAA.SamplePerPixel"), 8);
        SetCVar(TEXT("r.HairStrands.DeepShadow.Enable"), 1);
        SetCVar(TEXT("r.HairStrands.Voxelization.Enable"), 1);
        SetCVar(TEXT("r.HairStrands.RasterLOD"), 0);
        SetCVar(TEXT("r.HairStrands.Strands.Raytracing"), 1);
        SetCVar(TEXT("r.HairStrands.Interpolation.Quality"), 2);

        // Global Illumination - Full Lumen + Hardware RT
        SetCVar(TEXT("r.DynamicGlobalIlluminationMethod"), 2);
        SetCVar(TEXT("r.Lumen.Reflections.Allow"), 1);
        SetCVar(TEXT("r.Lumen.ScreenProbeGather.FullResolutionJitterWidth"), 1);
        SetCVar(TEXT("r.Lumen.Final.Quality"), 4);
        SetCVar(TEXT("r.Lumen.ScreenProbeGather.RadianceCache.NumProbesToTraceBudget"), 500);
        SetCVar(TEXT("r.Lumen.TraceMeshSDFs.Allow"), 1);
        SetCVar(TEXT("r.Lumen.ScreenProbeGather.ScreenTraces.HZBTraceMaxIterations"), 128);
        SetCVar(TEXT("r.Lumen.DiffuseIndirect.Allow"), 1);
        SetCVar(TEXT("r.Lumen.ScreenProbeGather.TwoSidedFoliageBackfaceDiffuse"), 1);

        // Reflections - Mirror Quality Lumen
        SetCVar(TEXT("r.ReflectionMethod"), 2);
        SetCVar(TEXT("r.Lumen.Reflections.ScreenSpaceReconstruction"), 1);
        SetCVar(TEXT("r.Lumen.Reflections.MaxRoughnessToTrace"), 0.6f);
        SetCVar(TEXT("r.SSR.Quality"), 4);
        SetCVar(TEXT("r.SSR.HalfResSceneColor"), 0);

        // Ray Tracing - 4090 Power
        SetCVar(TEXT("r.RayTracing.Shadows"), 1);
        SetCVar(TEXT("r.Lumen.HardwareRayTracing"), 1);
        SetCVar(TEXT("r.Lumen.HardwareRayTracing.LightingMode"), 2);
        SetCVar(TEXT("r.RayTracing.GlobalIllumination"), 1);
        SetCVar(TEXT("r.RayTracing.AmbientOcclusion"), 1);

        // Anti-Aliasing - Maximum Quality TSR
        SetCVar(TEXT("r.AntiAliasingMethod"), 4);
        SetCVar(TEXT("r.ScreenPercentage"), 100);
        SetCVar(TEXT("r.TSR.History.ScreenPercentage"), 200);
        SetCVar(TEXT("r.TSR.History.UpdateQuality"), 3);
        SetCVar(TEXT("r.TSR.RejectionAntiAliasingQuality"), 2);
        SetCVar(TEXT("r.TSR.ShadingRejection.Flickering"), 1);
        SetCVar(TEXT("r.TSR.ShadingRejection.TileOverscan"), 3);
        SetCVar(TEXT("r.TSR.Velocity.WeightClampingSampleCount"), 16);
        SetCVar(TEXT("r.TSR.History.R11G11B10.Precision"), 1);
        SetCVar(TEXT("r.Tonemapper.Sharpen"), 0.1f);
        SetCVar(TEXT("r.TemporalAA.Quality"), 2);
        SetCVar(TEXT("r.TemporalAASamples"), 8);
        SetCVar(TEXT("r.TemporalAA.Algorithm"), 1);

        // Depth of Field - Cinematic
        SetCVar(TEXT("r.DOF.Kernel.MaxBackgroundRadius"), 0.1f);
        SetCVar(TEXT("r.DOF.Kernel.MaxForegroundRadius"), 0.1f);
        SetCVar(TEXT("r.DOF.Gather.RingCount"), 5);
        SetCVar(TEXT("r.DOF.Gather.AccumulatorQuality"), 1);
        SetCVar(TEXT("r.DOF.TemporalAAQuality"), 1);

        // Post Processing - Film Quality
        SetCVar(TEXT("r.Tonemapper.Quality"), 5);
        SetCVar(TEXT("r.BloomQuality"), 5);
        SetCVar(TEXT("r.Bloom.Cross"), 0.0f);
        SetCVar(TEXT("r.LensFlareQuality"), 3);
        SetCVar(TEXT("r.EyeAdaptationQuality"), 3);
        SetCVar(TEXT("r.SceneColorFringe.Max"), 0.0f);
        SetCVar(TEXT("r.SceneColorFringeQuality"), 0);

        // Volumetrics - Full Quality
        SetCVar(TEXT("r.VolumetricFog"), 1);
        SetCVar(TEXT("r.VolumetricFog.GridPixelSize"), 4);
        SetCVar(TEXT("r.VolumetricFog.GridSizeZ"), 128);
        SetCVar(TEXT("r.VolumetricFog.HistoryWeight"), 0.9f);
        SetCVar(TEXT("r.VolumetricCloud"), 1);
        SetCVar(TEXT("r.VolumetricCloud.ShadowSampleCount"), 16);
        SetCVar(TEXT("r.VolumetricCloud.ViewRaySampleMaxCount"), 192);

        // Ambient Occlusion - GTAO High Quality
        SetCVar(TEXT("r.AmbientOcclusionLevels"), 3);
        SetCVar(TEXT("r.GTAO.Combined"), 1);
        SetCVar(TEXT("r.GTAO.UseNormals"), 1);
        SetCVar(TEXT("r.GTAO.SpatialFilter"), 1);
        SetCVar(TEXT("r.GTAO.NumAngles"), 4);
        SetCVar(TEXT("r.GTAO.Downsample"), 0);

        // LOD - Force Maximum Detail
        SetCVar(TEXT("r.ForceLOD"), 0);
        SetCVar(TEXT("r.SkeletalMeshLODBias"), -2);
        SetCVar(TEXT("r.StaticMeshLODDistanceScale"), 0.5f);

        // Material Quality - Maximum
        SetCVar(TEXT("r.DetailMode"), 2);
        SetCVar(TEXT("r.MaterialQualityLevel"), 3);

        // Nanite - Highest Detail
        SetCVar(TEXT("r.Nanite.MaxPixelsPerEdge"), 1);
        SetCVar(TEXT("r.Nanite.ViewMeshLODBias.Offset"), -2);

        // Skin Cache
        SetCVar(TEXT("r.SkinCache.Mode"), 1);
        SetCVar(TEXT("r.SkinCache.RecomputeTangents"), 2);
    }
    // Custom preset doesn't apply any settings - they're set individually
}

EEmbodyGraphicsPreset UGraphicsSettingsBFL::GetCurrentGraphicsPreset()
{
    FScopeLock Lock(&SettingsMutex);
    return CurrentPreset;
}

// ============================================
// SETTINGS RETRIEVAL
// ============================================

FEmbodyGraphicsSettings UGraphicsSettingsBFL::GetCurrentSettings()
{
    FScopeLock Lock(&SettingsMutex);
    FEmbodyGraphicsSettings Settings;

    Settings.Preset = CurrentPreset;
    Settings.bTextureStreaming = GetCVarInt(TEXT("r.TextureStreaming")) != 0;
    Settings.StreamingPoolSize = GetCVarInt(TEXT("r.Streaming.PoolSize"));
    Settings.MaxAnisotropy = GetCVarInt(TEXT("r.MaxAnisotropy"));
    Settings.ShadowResolution = GetCVarInt(TEXT("r.Shadow.MaxResolution"));
    Settings.bVirtualShadowMaps = GetCVarInt(TEXT("r.Shadow.Virtual.Enable")) != 0;
    Settings.bContactShadows = GetCVarInt(TEXT("r.ContactShadows")) != 0;
    Settings.SSSQuality = GetCVarInt(TEXT("r.SSS.Quality"));
    Settings.bSSSHalfRes = GetCVarInt(TEXT("r.SSS.HalfRes")) != 0;
    Settings.bHairStrands = GetCVarInt(TEXT("r.HairStrands.Enable")) != 0;
    Settings.HairMSAASamples = GetCVarInt(TEXT("r.HairStrands.Visibility.MSAA.SamplePerPixel"));
    Settings.GIMethod = GetCVarInt(TEXT("r.DynamicGlobalIlluminationMethod"));
    Settings.bLumenReflections = GetCVarInt(TEXT("r.Lumen.Reflections.Allow")) != 0;
    Settings.bHardwareRayTracing = GetCVarInt(TEXT("r.Lumen.HardwareRayTracing")) != 0;
    Settings.AAMethod = GetCVarInt(TEXT("r.AntiAliasingMethod"));
    Settings.ScreenPercentage = GetCVarInt(TEXT("r.ScreenPercentage"));
    Settings.TSRHistoryPercentage = GetCVarInt(TEXT("r.TSR.History.ScreenPercentage"));
    Settings.bVolumetricFog = GetCVarInt(TEXT("r.VolumetricFog")) != 0;
    Settings.BloomQuality = GetCVarInt(TEXT("r.BloomQuality"));
    Settings.AOLevels = GetCVarInt(TEXT("r.AmbientOcclusionLevels"));
    Settings.ForceLOD = GetCVarInt(TEXT("r.ForceLOD"));
    Settings.SkeletalMeshLODBias = GetCVarInt(TEXT("r.SkeletalMeshLODBias"));
    Settings.bRTShadows = GetCVarInt(TEXT("r.RayTracing.Shadows")) != 0;
    Settings.bRTGI = GetCVarInt(TEXT("r.RayTracing.GlobalIllumination")) != 0;
    Settings.bRTAO = GetCVarInt(TEXT("r.RayTracing.AmbientOcclusion")) != 0;

    return Settings;
}

FString UGraphicsSettingsBFL::GetSettingsAsJSON()
{
    FEmbodyGraphicsSettings Settings = GetCurrentSettings();

    // Read UE5.7 Performance CVars directly
    bool bAsyncCompute = false;
    bool bLumenMeshSDFs = false;
    bool bStochasticInterpolation = false;
    bool bTileClassification = false;

    if (IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.RDG.AsyncCompute")))
    {
        bAsyncCompute = CVar->GetInt() != 0;
    }
    if (IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Lumen.TraceMeshSDFs")))
    {
        bLumenMeshSDFs = CVar->GetInt() != 0;
    }
    if (IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Lumen.ScreenProbeGather.StochasticInterpolation")))
    {
        bStochasticInterpolation = CVar->GetInt() != 0;
    }
    if (IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Lumen.ScreenProbeGather.IntegrationTileClassification")))
    {
        bTileClassification = CVar->GetInt() != 0;
    }

    FString JSON = FString::Printf(TEXT("{"
        "\"preset\":%d,"
        "\"textureStreaming\":%s,"
        "\"streamingPoolSize\":%d,"
        "\"maxAnisotropy\":%d,"
        "\"shadowResolution\":%d,"
        "\"virtualShadowMaps\":%s,"
        "\"contactShadows\":%s,"
        "\"sssQuality\":%d,"
        "\"sssHalfRes\":%s,"
        "\"hairStrands\":%s,"
        "\"hairMSAASamples\":%d,"
        "\"giMethod\":%d,"
        "\"lumenReflections\":%s,"
        "\"hardwareRayTracing\":%s,"
        "\"aaMethod\":%d,"
        "\"screenPercentage\":%d,"
        "\"tsrHistoryPercentage\":%d,"
        "\"volumetricFog\":%s,"
        "\"bloomQuality\":%d,"
        "\"aoLevels\":%d,"
        "\"forceLOD\":%d,"
        "\"skeletalMeshLODBias\":%d,"
        "\"rtShadows\":%s,"
        "\"rtGI\":%s,"
        "\"rtAO\":%s,"
        "\"asyncCompute\":%s,"
        "\"lumenMeshSDFs\":%s,"
        "\"stochasticInterpolation\":%s,"
        "\"tileClassification\":%s"
        "}"),
        (int32)Settings.Preset,
        Settings.bTextureStreaming ? TEXT("true") : TEXT("false"),
        Settings.StreamingPoolSize,
        Settings.MaxAnisotropy,
        Settings.ShadowResolution,
        Settings.bVirtualShadowMaps ? TEXT("true") : TEXT("false"),
        Settings.bContactShadows ? TEXT("true") : TEXT("false"),
        Settings.SSSQuality,
        Settings.bSSSHalfRes ? TEXT("true") : TEXT("false"),
        Settings.bHairStrands ? TEXT("true") : TEXT("false"),
        Settings.HairMSAASamples,
        Settings.GIMethod,
        Settings.bLumenReflections ? TEXT("true") : TEXT("false"),
        Settings.bHardwareRayTracing ? TEXT("true") : TEXT("false"),
        Settings.AAMethod,
        Settings.ScreenPercentage,
        Settings.TSRHistoryPercentage,
        Settings.bVolumetricFog ? TEXT("true") : TEXT("false"),
        Settings.BloomQuality,
        Settings.AOLevels,
        Settings.ForceLOD,
        Settings.SkeletalMeshLODBias,
        Settings.bRTShadows ? TEXT("true") : TEXT("false"),
        Settings.bRTGI ? TEXT("true") : TEXT("false"),
        Settings.bRTAO ? TEXT("true") : TEXT("false"),
        bAsyncCompute ? TEXT("true") : TEXT("false"),
        bLumenMeshSDFs ? TEXT("true") : TEXT("false"),
        bStochasticInterpolation ? TEXT("true") : TEXT("false"),
        bTileClassification ? TEXT("true") : TEXT("false")
    );

    return JSON;
}

FEmbodyPerformanceMetrics UGraphicsSettingsBFL::GetPerformanceMetrics()
{
    FEmbodyPerformanceMetrics Metrics;

    // Get frame time stats
    extern ENGINE_API float GAverageFPS;
    extern ENGINE_API float GAverageMS;

    Metrics.FPS = GAverageFPS;
    Metrics.FrameTimeMs = GAverageMS;

    // GPU time comes from nvidia-smi polling instead of RHI
    Metrics.GPUTimeMs = 0.0f;

    return Metrics;
}

FString UGraphicsSettingsBFL::GetPerformanceMetricsAsJSON()
{
    FEmbodyPerformanceMetrics Metrics = GetPerformanceMetrics();

    return FString::Printf(TEXT("{"
        "\"fps\":%.1f,"
        "\"frameTimeMs\":%.2f,"
        "\"gpuTimeMs\":%.2f,"
        "\"gameThreadTimeMs\":%.2f,"
        "\"renderThreadTimeMs\":%.2f"
        "}"),
        Metrics.FPS,
        Metrics.FrameTimeMs,
        Metrics.GPUTimeMs,
        Metrics.GameThreadTimeMs,
        Metrics.RenderThreadTimeMs
    );
}

// ============================================
// INSTANCE ESTIMATION
// ============================================

int32 UGraphicsSettingsBFL::GetGPUVRAM(EEmbodyGPUType GPUType)
{
    switch (GPUType)
    {
        case EEmbodyGPUType::RTX_4090:      return 24576;  // 24GB
        case EEmbodyGPUType::RTX_6000_Ada:  return 49152;  // 48GB
        case EEmbodyGPUType::RTX_4080_Super: return 16384; // 16GB
        case EEmbodyGPUType::RTX_4080:      return 16384;  // 16GB
        case EEmbodyGPUType::RTX_3090:      return 24576;  // 24GB
        case EEmbodyGPUType::RTX_3080_Ti:   return 12288;  // 12GB
        default:                            return 8192;   // 8GB default
    }
}

FEmbodyInstanceEstimate UGraphicsSettingsBFL::EstimateMaxInstances(EEmbodyGPUType GPUType, int32 CustomVRAMMB)
{
    FEmbodyInstanceEstimate Estimate;

    int32 VRAMMB = (GPUType == EEmbodyGPUType::Custom && CustomVRAMMB > 0)
        ? CustomVRAMMB
        : GetGPUVRAM(GPUType);

    // Get current settings to estimate VRAM per instance
    FEmbodyGraphicsSettings Settings = GetCurrentSettings();

    // Base VRAM usage depends on settings
    int32 BaseVRAMPerInstance = 1500; // MB - Base UE5 + MetaHuman

    // Add VRAM based on settings
    if (Settings.bVirtualShadowMaps)
        BaseVRAMPerInstance += 500;
    if (Settings.bHardwareRayTracing)
        BaseVRAMPerInstance += 400;
    if (Settings.GIMethod == 2) // Lumen
        BaseVRAMPerInstance += 300;
    if (Settings.TSRHistoryPercentage > 100)
        BaseVRAMPerInstance += 200;
    if (!Settings.bSSSHalfRes)
        BaseVRAMPerInstance += 100;
    if (Settings.HairMSAASamples > 4)
        BaseVRAMPerInstance += 150;

    // Add streaming pool size
    BaseVRAMPerInstance += Settings.StreamingPoolSize / 2;

    Estimate.EstimatedVRAMPerInstanceMB = BaseVRAMPerInstance;

    // Calculate max by VRAM (leave 2GB for OS/driver overhead)
    int32 AvailableVRAM = VRAMMB - 2048;
    Estimate.MaxByVRAM = FMath::Max(1, AvailableVRAM / BaseVRAMPerInstance);

    // Estimate max by GPU compute based on settings
    // Max Settings = ~65% GPU per instance, Optimized = ~30%
    float GPUPercentPerInstance = (Settings.Preset == EEmbodyGraphicsPreset::MaxSettings) ? 65.0f : 30.0f;

    // Adjust based on specific settings
    if (Settings.bHardwareRayTracing)
        GPUPercentPerInstance += 15.0f;
    if (Settings.GIMethod == 2)
        GPUPercentPerInstance += 10.0f;
    if (Settings.bVolumetricFog)
        GPUPercentPerInstance += 5.0f;

    float TargetGPUUtilization = 85.0f; // Target max 85% GPU usage
    Estimate.MaxByGPU = FMath::Max(1, FMath::FloorToInt(TargetGPUUtilization / GPUPercentPerInstance));

    // The limiting factor determines max instances
    Estimate.MaxInstances = FMath::Min(Estimate.MaxByVRAM, Estimate.MaxByGPU);

    // Determine bottleneck
    if (Estimate.MaxByGPU < Estimate.MaxByVRAM)
    {
        Estimate.Bottleneck = TEXT("GPU Compute");
    }
    else
    {
        Estimate.Bottleneck = TEXT("VRAM");
    }

    return Estimate;
}

// ============================================
// INDIVIDUAL SETTING CONTROLS
// ============================================

void UGraphicsSettingsBFL::SetTextureStreaming(bool bEnabled)
{
    FScopeLock Lock(&SettingsMutex);
    SetCVar(TEXT("r.TextureStreaming"), bEnabled ? 1 : 0);
    CurrentPreset = EEmbodyGraphicsPreset::Custom;
}

void UGraphicsSettingsBFL::SetStreamingPoolSize(int32 SizeMB)
{
    FScopeLock Lock(&SettingsMutex);
    SetCVar(TEXT("r.Streaming.PoolSize"), SizeMB);
    CurrentPreset = EEmbodyGraphicsPreset::Custom;
}

void UGraphicsSettingsBFL::SetMaxAnisotropy(int32 Value)
{
    FScopeLock Lock(&SettingsMutex);
    SetCVar(TEXT("r.MaxAnisotropy"), FMath::Clamp(Value, 1, 16));
    CurrentPreset = EEmbodyGraphicsPreset::Custom;
}

void UGraphicsSettingsBFL::SetShadowResolution(int32 Resolution)
{
    FScopeLock Lock(&SettingsMutex);
    SetCVar(TEXT("r.Shadow.MaxResolution"), Resolution);
    SetCVar(TEXT("r.Shadow.MaxCSMResolution"), Resolution);
    CurrentPreset = EEmbodyGraphicsPreset::Custom;
}

void UGraphicsSettingsBFL::SetVirtualShadowMaps(bool bEnabled)
{
    FScopeLock Lock(&SettingsMutex);
    SetCVar(TEXT("r.Shadow.Virtual.Enable"), bEnabled ? 1 : 0);
    CurrentPreset = EEmbodyGraphicsPreset::Custom;
}

void UGraphicsSettingsBFL::SetContactShadows(bool bEnabled)
{
    FScopeLock Lock(&SettingsMutex);
    SetCVar(TEXT("r.ContactShadows"), bEnabled ? 1 : 0);
    CurrentPreset = EEmbodyGraphicsPreset::Custom;
}

void UGraphicsSettingsBFL::SetSSSQuality(int32 Quality)
{
    FScopeLock Lock(&SettingsMutex);
    SetCVar(TEXT("r.SSS.Quality"), FMath::Clamp(Quality, 0, 3));
    CurrentPreset = EEmbodyGraphicsPreset::Custom;
}

void UGraphicsSettingsBFL::SetSSSHalfRes(bool bHalfRes)
{
    FScopeLock Lock(&SettingsMutex);
    SetCVar(TEXT("r.SSS.HalfRes"), bHalfRes ? 1 : 0);
    CurrentPreset = EEmbodyGraphicsPreset::Custom;
}

void UGraphicsSettingsBFL::SetHairStrands(bool bEnabled)
{
    FScopeLock Lock(&SettingsMutex);
    SetCVar(TEXT("r.HairStrands.Enable"), bEnabled ? 1 : 0);
    CurrentPreset = EEmbodyGraphicsPreset::Custom;
}

void UGraphicsSettingsBFL::SetHairMSAASamples(int32 Samples)
{
    FScopeLock Lock(&SettingsMutex);
    SetCVar(TEXT("r.HairStrands.Visibility.MSAA.SamplePerPixel"), FMath::Clamp(Samples, 1, 8));
    CurrentPreset = EEmbodyGraphicsPreset::Custom;
}

void UGraphicsSettingsBFL::SetGIMethod(int32 Method)
{
    FScopeLock Lock(&SettingsMutex);
    SetCVar(TEXT("r.DynamicGlobalIlluminationMethod"), FMath::Clamp(Method, 0, 2));
    CurrentPreset = EEmbodyGraphicsPreset::Custom;
}

void UGraphicsSettingsBFL::SetLumenReflections(bool bEnabled)
{
    FScopeLock Lock(&SettingsMutex);
    SetCVar(TEXT("r.Lumen.Reflections.Allow"), bEnabled ? 1 : 0);
    CurrentPreset = EEmbodyGraphicsPreset::Custom;
}

void UGraphicsSettingsBFL::SetHardwareRayTracing(bool bEnabled)
{
    FScopeLock Lock(&SettingsMutex);
    SetCVar(TEXT("r.Lumen.HardwareRayTracing"), bEnabled ? 1 : 0);
    CurrentPreset = EEmbodyGraphicsPreset::Custom;
}

void UGraphicsSettingsBFL::SetAAMethod(int32 Method)
{
    FScopeLock Lock(&SettingsMutex);
    SetCVar(TEXT("r.AntiAliasingMethod"), Method);
    CurrentPreset = EEmbodyGraphicsPreset::Custom;
}

void UGraphicsSettingsBFL::SetScreenPercentage(int32 Percentage)
{
    FScopeLock Lock(&SettingsMutex);
    SetCVar(TEXT("r.ScreenPercentage"), FMath::Clamp(Percentage, 50, 200));
    CurrentPreset = EEmbodyGraphicsPreset::Custom;
}

void UGraphicsSettingsBFL::SetTSRHistoryPercentage(int32 Percentage)
{
    FScopeLock Lock(&SettingsMutex);
    SetCVar(TEXT("r.TSR.History.ScreenPercentage"), FMath::Clamp(Percentage, 100, 200));
    CurrentPreset = EEmbodyGraphicsPreset::Custom;
}

void UGraphicsSettingsBFL::SetVolumetricFog(bool bEnabled)
{
    FScopeLock Lock(&SettingsMutex);
    SetCVar(TEXT("r.VolumetricFog"), bEnabled ? 1 : 0);
    CurrentPreset = EEmbodyGraphicsPreset::Custom;
}

void UGraphicsSettingsBFL::SetBloomQuality(int32 Quality)
{
    FScopeLock Lock(&SettingsMutex);
    SetCVar(TEXT("r.BloomQuality"), FMath::Clamp(Quality, 0, 5));
    CurrentPreset = EEmbodyGraphicsPreset::Custom;
}

void UGraphicsSettingsBFL::SetAOLevels(int32 Levels)
{
    FScopeLock Lock(&SettingsMutex);
    SetCVar(TEXT("r.AmbientOcclusionLevels"), FMath::Clamp(Levels, 0, 3));
    CurrentPreset = EEmbodyGraphicsPreset::Custom;
}

void UGraphicsSettingsBFL::SetForceLOD(int32 LOD)
{
    FScopeLock Lock(&SettingsMutex);
    SetCVar(TEXT("r.ForceLOD"), LOD);
    CurrentPreset = EEmbodyGraphicsPreset::Custom;
}

void UGraphicsSettingsBFL::SetSkeletalMeshLODBias(int32 Bias)
{
    FScopeLock Lock(&SettingsMutex);
    SetCVar(TEXT("r.SkeletalMeshLODBias"), Bias);
    CurrentPreset = EEmbodyGraphicsPreset::Custom;
}

void UGraphicsSettingsBFL::SetRTShadows(bool bEnabled)
{
    FScopeLock Lock(&SettingsMutex);
    SetCVar(TEXT("r.RayTracing.Shadows"), bEnabled ? 1 : 0);
    CurrentPreset = EEmbodyGraphicsPreset::Custom;
}

void UGraphicsSettingsBFL::SetRTGI(bool bEnabled)
{
    FScopeLock Lock(&SettingsMutex);
    SetCVar(TEXT("r.RayTracing.GlobalIllumination"), bEnabled ? 1 : 0);
    CurrentPreset = EEmbodyGraphicsPreset::Custom;
}

void UGraphicsSettingsBFL::SetRTAO(bool bEnabled)
{
    FScopeLock Lock(&SettingsMutex);
    SetCVar(TEXT("r.RayTracing.AmbientOcclusion"), bEnabled ? 1 : 0);
    CurrentPreset = EEmbodyGraphicsPreset::Custom;
}

// ============================================
// COMMAND PARSING (for Pixel Streaming)
// ============================================

bool UGraphicsSettingsBFL::ParseGraphicsCommand(const FString& Command, FString& OutResponse)
{
    // Command format: "Graphics_<Action>_<Params>"
    // Examples:
    //   Graphics_GetSettings
    //   Graphics_GetMetrics
    //   Graphics_SetPreset_Optimized
    //   Graphics_SetPreset_MaxSettings
    //   Graphics_Set_TextureStreaming_true
    //   Graphics_Set_ShadowResolution_2048
    //   Graphics_EstimateInstances_RTX_4090

    // Safety check - don't process if engine is shutting down
    if (IsEngineExitRequested() || !GEngine)
    {
        OutResponse = TEXT("{\"success\":false,\"error\":\"Engine shutting down\"}");
        return true;
    }

    UE_LOG(LogGraphicsSettings, Log, TEXT("ParseGraphicsCommand received: %s"), *Command);

    if (!Command.StartsWith(TEXT("Graphics_")))
    {
        UE_LOG(LogGraphicsSettings, Warning, TEXT("Command does not start with Graphics_: %s"), *Command);
        return false;
    }

    FString Cmd = Command.Mid(9); // Remove "Graphics_" prefix
    UE_LOG(LogGraphicsSettings, Log, TEXT("Processing command: %s"), *Cmd);

    // Get Settings
    if (Cmd.Equals(TEXT("GetSettings"), ESearchCase::IgnoreCase))
    {
        OutResponse = GetSettingsAsJSON();
        UE_LOG(LogGraphicsSettings, Log, TEXT("GetSettings response: %s"), *OutResponse);
        return true;
    }

    // Get Metrics
    if (Cmd.Equals(TEXT("GetMetrics"), ESearchCase::IgnoreCase))
    {
        OutResponse = GetPerformanceMetricsAsJSON();
        return true;
    }

    // GPU Monitor commands
    if (Cmd.Equals(TEXT("StartMonitor"), ESearchCase::IgnoreCase))
    {
        StartGPUMonitoring();
        OutResponse = TEXT("{\"success\":true,\"monitoring\":true}");
        return true;
    }

    if (Cmd.Equals(TEXT("StopMonitor"), ESearchCase::IgnoreCase))
    {
        StopGPUMonitoring();
        OutResponse = TEXT("{\"success\":true,\"monitoring\":false}");
        return true;
    }

    if (Cmd.Equals(TEXT("PollGPU"), ESearchCase::IgnoreCase))
    {
        UE_LOG(LogGraphicsSettings, Log, TEXT("PollGPU: Polling GPU metrics..."));
        PollGPUMetrics();
        OutResponse = GetGPUMonitorDataAsJSON();
        UE_LOG(LogGraphicsSettings, Log, TEXT("PollGPU response: %s"), *OutResponse);
        return true;
    }

    if (Cmd.Equals(TEXT("GetGPUData"), ESearchCase::IgnoreCase))
    {
        OutResponse = GetGPUMonitorDataAsJSON();
        return true;
    }

    // Scene Analysis - Get expensive objects
    if (Cmd.Equals(TEXT("AnalyzeScene"), ESearchCase::IgnoreCase) ||
        Cmd.StartsWith(TEXT("AnalyzeScene_"), ESearchCase::IgnoreCase))
    {
        int32 MaxResults = 20;
        if (Cmd.StartsWith(TEXT("AnalyzeScene_")))
        {
            MaxResults = FCString::Atoi(*Cmd.Mid(13));
            MaxResults = FMath::Clamp(MaxResults, 5, 50);
        }

        UE_LOG(LogGraphicsSettings, Log, TEXT("AnalyzeScene: Analyzing scene objects (max %d results)..."), MaxResults);

        // Get world from GEngine
        UWorld* World = nullptr;
        if (GEngine)
        {
            for (const FWorldContext& Context : GEngine->GetWorldContexts())
            {
                if (Context.WorldType == EWorldType::Game || Context.WorldType == EWorldType::PIE)
                {
                    World = Context.World();
                    break;
                }
            }
        }

        if (World)
        {
            OutResponse = GetSceneAnalysisAsJSON(World, MaxResults);
            UE_LOG(LogGraphicsSettings, Log, TEXT("AnalyzeScene response: %s"), *OutResponse.Left(500));
        }
        else
        {
            OutResponse = TEXT("{\"error\":\"No game world found\",\"expensiveObjects\":[],\"recommendations\":[]}");
            UE_LOG(LogGraphicsSettings, Warning, TEXT("AnalyzeScene: No game world found"));
        }
        return true;
    }

    if (Cmd.StartsWith(TEXT("SetInstanceCount_"), ESearchCase::IgnoreCase))
    {
        int32 Count = FCString::Atoi(*Cmd.Mid(17));
        SetCurrentInstanceCount(Count);
        OutResponse = FString::Printf(TEXT("{\"success\":true,\"instanceCount\":%d}"), Count);
        return true;
    }

    // SM Optimization commands
    if (Cmd.Equals(TEXT("EnableSMOptimization"), ESearchCase::IgnoreCase))
    {
        EnableSMOptimization();
        OutResponse = FString::Printf(TEXT("{\"success\":true,\"smOptimization\":true,\"vrsSupported\":%s}"),
            IsVRSSupported() ? TEXT("true") : TEXT("false"));
        return true;
    }

    if (Cmd.Equals(TEXT("DisableSMOptimization"), ESearchCase::IgnoreCase))
    {
        DisableSMOptimization();
        OutResponse = TEXT("{\"success\":true,\"smOptimization\":false}");
        return true;
    }

    if (Cmd.Equals(TEXT("GetSMReduction"), ESearchCase::IgnoreCase))
    {
        float Reduction = GetCurrentSMReduction();
        OutResponse = FString::Printf(TEXT("{\"success\":true,\"smReduction\":%.1f,\"vrsSupported\":%s}"),
            Reduction, IsVRSSupported() ? TEXT("true") : TEXT("false"));
        return true;
    }

    if (Cmd.StartsWith(TEXT("SetMinScreenPct_"), ESearchCase::IgnoreCase))
    {
        float Pct = FCString::Atof(*Cmd.Mid(16));
        SetMinScreenPercentage(Pct);
        OutResponse = FString::Printf(TEXT("{\"success\":true,\"minScreenPercentage\":%.1f}"), Pct);
        return true;
    }

    if (Cmd.StartsWith(TEXT("SetMaxScreenPct_"), ESearchCase::IgnoreCase))
    {
        float Pct = FCString::Atof(*Cmd.Mid(16));
        SetMaxScreenPercentage(Pct);
        OutResponse = FString::Printf(TEXT("{\"success\":true,\"maxScreenPercentage\":%.1f}"), Pct);
        return true;
    }

    // Set Preset
    if (Cmd.StartsWith(TEXT("SetPreset_"), ESearchCase::IgnoreCase))
    {
        FString PresetName = Cmd.Mid(10);
        if (PresetName.Equals(TEXT("Optimized"), ESearchCase::IgnoreCase))
        {
            SetGraphicsPreset(EEmbodyGraphicsPreset::Optimized);
            OutResponse = TEXT("{\"success\":true,\"preset\":\"Optimized\"}");
            return true;
        }
        else if (PresetName.Equals(TEXT("MaxSettings"), ESearchCase::IgnoreCase))
        {
            SetGraphicsPreset(EEmbodyGraphicsPreset::MaxSettings);
            OutResponse = TEXT("{\"success\":true,\"preset\":\"MaxSettings\"}");
            return true;
        }
    }

    // Estimate Instances
    if (Cmd.StartsWith(TEXT("EstimateInstances_"), ESearchCase::IgnoreCase))
    {
        FString GPUName = Cmd.Mid(18);
        EEmbodyGPUType GPUType = EEmbodyGPUType::RTX_4090;

        if (GPUName.Contains(TEXT("4090")))
            GPUType = EEmbodyGPUType::RTX_4090;
        else if (GPUName.Contains(TEXT("6000")))
            GPUType = EEmbodyGPUType::RTX_6000_Ada;
        else if (GPUName.Contains(TEXT("4080")))
            GPUType = EEmbodyGPUType::RTX_4080;
        else if (GPUName.Contains(TEXT("3090")))
            GPUType = EEmbodyGPUType::RTX_3090;

        FEmbodyInstanceEstimate Estimate = EstimateMaxInstances(GPUType);
        OutResponse = FString::Printf(TEXT("{"
            "\"maxByVRAM\":%d,"
            "\"maxByGPU\":%d,"
            "\"maxInstances\":%d,"
            "\"bottleneck\":\"%s\","
            "\"estimatedVRAMPerInstance\":%d"
            "}"),
            Estimate.MaxByVRAM,
            Estimate.MaxByGPU,
            Estimate.MaxInstances,
            *Estimate.Bottleneck,
            Estimate.EstimatedVRAMPerInstanceMB
        );
        return true;
    }

    // Individual setting commands: Graphics_Set_<SettingName>_<Value>
    if (Cmd.StartsWith(TEXT("Set_"), ESearchCase::IgnoreCase))
    {
        FString SettingCmd = Cmd.Mid(4);
        int32 UnderscorePos;
        if (SettingCmd.FindLastChar('_', UnderscorePos))
        {
            FString SettingName = SettingCmd.Left(UnderscorePos);
            FString ValueStr = SettingCmd.Mid(UnderscorePos + 1);

            UE_LOG(LogGraphicsSettings, Log, TEXT("Set command: Setting=%s, Value=%s"), *SettingName, *ValueStr);

            // Parse and apply setting
            if (SettingName.Equals(TEXT("TextureStreaming"), ESearchCase::IgnoreCase))
            {
                SetTextureStreaming(ValueStr.Equals(TEXT("true"), ESearchCase::IgnoreCase) || ValueStr.Equals(TEXT("1")));
            }
            else if (SettingName.Equals(TEXT("StreamingPoolSize"), ESearchCase::IgnoreCase))
            {
                SetStreamingPoolSize(FCString::Atoi(*ValueStr));
            }
            else if (SettingName.Equals(TEXT("ShadowResolution"), ESearchCase::IgnoreCase))
            {
                SetShadowResolution(FCString::Atoi(*ValueStr));
            }
            else if (SettingName.Equals(TEXT("VirtualShadowMaps"), ESearchCase::IgnoreCase))
            {
                SetVirtualShadowMaps(ValueStr.Equals(TEXT("true"), ESearchCase::IgnoreCase) || ValueStr.Equals(TEXT("1")));
            }
            else if (SettingName.Equals(TEXT("ContactShadows"), ESearchCase::IgnoreCase))
            {
                SetContactShadows(ValueStr.Equals(TEXT("true"), ESearchCase::IgnoreCase) || ValueStr.Equals(TEXT("1")));
            }
            else if (SettingName.Equals(TEXT("SSSQuality"), ESearchCase::IgnoreCase))
            {
                SetSSSQuality(FCString::Atoi(*ValueStr));
            }
            else if (SettingName.Equals(TEXT("HairStrands"), ESearchCase::IgnoreCase))
            {
                SetHairStrands(ValueStr.Equals(TEXT("true"), ESearchCase::IgnoreCase) || ValueStr.Equals(TEXT("1")));
            }
            else if (SettingName.Equals(TEXT("GIMethod"), ESearchCase::IgnoreCase))
            {
                SetGIMethod(FCString::Atoi(*ValueStr));
            }
            else if (SettingName.Equals(TEXT("LumenReflections"), ESearchCase::IgnoreCase))
            {
                SetLumenReflections(ValueStr.Equals(TEXT("true"), ESearchCase::IgnoreCase) || ValueStr.Equals(TEXT("1")));
            }
            else if (SettingName.Equals(TEXT("HardwareRayTracing"), ESearchCase::IgnoreCase))
            {
                SetHardwareRayTracing(ValueStr.Equals(TEXT("true"), ESearchCase::IgnoreCase) || ValueStr.Equals(TEXT("1")));
            }
            else if (SettingName.Equals(TEXT("VolumetricFog"), ESearchCase::IgnoreCase))
            {
                SetVolumetricFog(ValueStr.Equals(TEXT("true"), ESearchCase::IgnoreCase) || ValueStr.Equals(TEXT("1")));
            }
            else if (SettingName.Equals(TEXT("RTShadows"), ESearchCase::IgnoreCase))
            {
                SetRTShadows(ValueStr.Equals(TEXT("true"), ESearchCase::IgnoreCase) || ValueStr.Equals(TEXT("1")));
            }
            else if (SettingName.Equals(TEXT("RTGI"), ESearchCase::IgnoreCase))
            {
                SetRTGI(ValueStr.Equals(TEXT("true"), ESearchCase::IgnoreCase) || ValueStr.Equals(TEXT("1")));
            }
            else if (SettingName.Equals(TEXT("RTAO"), ESearchCase::IgnoreCase))
            {
                SetRTAO(ValueStr.Equals(TEXT("true"), ESearchCase::IgnoreCase) || ValueStr.Equals(TEXT("1")));
            }
            else if (SettingName.Equals(TEXT("ScreenPercentage"), ESearchCase::IgnoreCase))
            {
                SetScreenPercentage(FCString::Atoi(*ValueStr));
            }
            else if (SettingName.Equals(TEXT("TSRHistoryPercentage"), ESearchCase::IgnoreCase))
            {
                SetTSRHistoryPercentage(FCString::Atoi(*ValueStr));
            }
            // UE5.7 Performance CVars
            else if (SettingName.Equals(TEXT("AsyncCompute"), ESearchCase::IgnoreCase))
            {
                bool bEnable = ValueStr.Equals(TEXT("true"), ESearchCase::IgnoreCase) || ValueStr.Equals(TEXT("1"));
                SetCVar(TEXT("r.RDG.AsyncCompute"), bEnable ? 1 : 0);
                UE_LOG(LogGraphicsSettings, Log, TEXT("Set AsyncCompute (r.RDG.AsyncCompute) = %d"), bEnable ? 1 : 0);
            }
            else if (SettingName.Equals(TEXT("LumenMeshSDFs"), ESearchCase::IgnoreCase))
            {
                bool bEnable = ValueStr.Equals(TEXT("true"), ESearchCase::IgnoreCase) || ValueStr.Equals(TEXT("1"));
                SetCVar(TEXT("r.Lumen.TraceMeshSDFs"), bEnable ? 1 : 0);
                UE_LOG(LogGraphicsSettings, Log, TEXT("Set LumenMeshSDFs (r.Lumen.TraceMeshSDFs) = %d"), bEnable ? 1 : 0);
            }
            else if (SettingName.Equals(TEXT("StochasticInterpolation"), ESearchCase::IgnoreCase))
            {
                bool bEnable = ValueStr.Equals(TEXT("true"), ESearchCase::IgnoreCase) || ValueStr.Equals(TEXT("1"));
                SetCVar(TEXT("r.Lumen.ScreenProbeGather.StochasticInterpolation"), bEnable ? 1 : 0);
                UE_LOG(LogGraphicsSettings, Log, TEXT("Set StochasticInterpolation = %d"), bEnable ? 1 : 0);
            }
            else if (SettingName.Equals(TEXT("TileClassification"), ESearchCase::IgnoreCase))
            {
                bool bEnable = ValueStr.Equals(TEXT("true"), ESearchCase::IgnoreCase) || ValueStr.Equals(TEXT("1"));
                SetCVar(TEXT("r.Lumen.ScreenProbeGather.IntegrationTileClassification"), bEnable ? 1 : 0);
                UE_LOG(LogGraphicsSettings, Log, TEXT("Set TileClassification = %d"), bEnable ? 1 : 0);
            }
            else
            {
                OutResponse = TEXT("{\"success\":false,\"error\":\"Unknown setting\"}");
                return true;
            }

            OutResponse = FString::Printf(TEXT("{\"success\":true,\"setting\":\"%s\",\"value\":\"%s\"}"), *SettingName, *ValueStr);
            return true;
        }
    }

    OutResponse = TEXT("{\"success\":false,\"error\":\"Unknown command\"}");
    return true;
}

bool UGraphicsSettingsBFL::ApplySettingsFromJSON(const FString& JSONString)
{
    // Basic JSON parsing - in production you'd use FJsonSerializer
    // This is a simplified implementation

    if (JSONString.Contains(TEXT("\"preset\":0")))
        SetGraphicsPreset(EEmbodyGraphicsPreset::Optimized);
    else if (JSONString.Contains(TEXT("\"preset\":1")))
        SetGraphicsPreset(EEmbodyGraphicsPreset::MaxSettings);

    // Individual settings would be parsed here
    // For now, use ParseGraphicsCommand for individual settings

    return true;
}

// ============================================
// REAL-TIME GPU MONITORING (nvidia-smi)
// Async implementation to prevent game thread blocking
// ============================================

void UGraphicsSettingsBFL::StartGPUMonitoring()
{
    {
        FScopeLock Lock(&GPUDataMutex);
        bIsMonitoring = true;
        CachedGPUData.bIsMonitoring = true;
    }
    PollGPUMetrics(); // Trigger initial async poll
}

void UGraphicsSettingsBFL::StopGPUMonitoring()
{
    FScopeLock Lock(&GPUDataMutex);
    bIsMonitoring = false;
    CachedGPUData.bIsMonitoring = false;
}

bool UGraphicsSettingsBFL::IsGPUMonitoringActive()
{
    FScopeLock Lock(&GPUDataMutex);
    return bIsMonitoring;
}

void UGraphicsSettingsBFL::SetCurrentInstanceCount(int32 Count)
{
    FScopeLock Lock(&GPUDataMutex);
    CurrentInstanceCount = FMath::Max(1, Count);
}

int32 UGraphicsSettingsBFL::GetEstimatedVRAMPerInstance()
{
    FScopeLock Lock(&GPUDataMutex);
    const int32 SafeInstanceCount = FMath::Max(1, CurrentInstanceCount);
    if (CachedGPUData.MemoryUsedMB <= 0)
    {
        return 2048; // Default estimate
    }
    return CachedGPUData.MemoryUsedMB / SafeInstanceCount;
}

void UGraphicsSettingsBFL::PollGPUMetrics()
{
    // Safety check - don't poll if we're shutting down
    if (IsEngineExitRequested() || !GEngine)
    {
        UE_LOG(LogGraphicsSettings, Warning, TEXT("PollGPUMetrics: Skipping - engine shutting down"));
        return;
    }

    // Check if monitoring is active
    {
        FScopeLock Lock(&GPUDataMutex);
        if (!bIsMonitoring)
        {
            UE_LOG(LogGraphicsSettings, Warning, TEXT("PollGPUMetrics: Skipping - monitoring stopped"));
            return;
        }
    }

    // Prevent overlapping async polls
    bool bExpected = false;
    if (!bAsyncPollInProgress.CompareExchange(bExpected, true))
    {
        UE_LOG(LogGraphicsSettings, Log, TEXT("PollGPUMetrics: Async poll already in progress, skipping"));
        return;
    }

    UE_LOG(LogGraphicsSettings, Log, TEXT("PollGPUMetrics: Starting async nvidia-smi poll"));

    // Run nvidia-smi on a background thread to prevent game thread blocking
    Async(EAsyncExecution::ThreadPool, []()
    {
        PollGPUMetricsAsync_Internal();
    });
}

void UGraphicsSettingsBFL::PollGPUMetricsAsync_Internal()
{
    // This runs on a background thread - do NOT access game thread resources directly

    FString NvidiaSmiPath = TEXT("nvidia-smi");
    FString StdOut;
    FString StdErr;
    int32 ReturnCode;

    // Temporary data structure to collect results before locking
    FEmbodyGPUMonitorData TempData;

    // Query basic GPU info - name first to avoid CSV parsing issues
    FString Args = TEXT("--query-gpu=name,utilization.gpu,memory.used,memory.total,temperature.gpu,power.draw,power.limit,fan.speed --format=csv,noheader,nounits");

    bool bSuccess = FPlatformProcess::ExecProcess(
        *NvidiaSmiPath,
        *Args,
        &ReturnCode,
        &StdOut,
        &StdErr
    );

    if (bSuccess && ReturnCode == 0 && !StdOut.IsEmpty())
    {
        // Parse: name, utilization.gpu, memory.used, memory.total, temperature.gpu, power.draw, power.limit, fan.speed
        TArray<FString> Parts;
        StdOut.TrimStartAndEnd().ParseIntoArray(Parts, TEXT(","), true);

        if (Parts.Num() >= 8)
        {
            TempData.GPUName = Parts[0].TrimStartAndEnd();
            TempData.SM = FCString::Atoi(*Parts[1].TrimStartAndEnd());
            TempData.MemoryUsedMB = FCString::Atoi(*Parts[2].TrimStartAndEnd());
            TempData.MemoryTotalMB = FCString::Atoi(*Parts[3].TrimStartAndEnd());
            TempData.Temperature = FCString::Atoi(*Parts[4].TrimStartAndEnd());
            TempData.PowerDraw = FCString::Atoi(*Parts[5].TrimStartAndEnd());
            TempData.PowerLimit = FCString::Atoi(*Parts[6].TrimStartAndEnd());
            TempData.FanSpeed = FCString::Atoi(*Parts[7].TrimStartAndEnd());

            // Calculate memory percent
            if (TempData.MemoryTotalMB > 0)
            {
                TempData.MemoryPercent = (TempData.MemoryUsedMB * 100) / TempData.MemoryTotalMB;
            }
        }

        // Update timestamp
        TempData.Timestamp = FDateTime::Now().ToString(TEXT("%H:%M:%S"));
    }

    // Run dmon once to get encoder/decoder utilization (overwrites SM with more accurate value)
    Args = TEXT("dmon -s u -c 1");
    FString DmonOut;
    if (FPlatformProcess::ExecProcess(*NvidiaSmiPath, *Args, &ReturnCode, &DmonOut, &StdErr) && ReturnCode == 0)
    {
        // Parse dmon output - format is: # gpu   sm   mem   enc   dec
        TArray<FString> Lines;
        DmonOut.ParseIntoArray(Lines, TEXT("\n"), true);

        for (const FString& Line : Lines)
        {
            if (Line.StartsWith(TEXT("#")) || Line.IsEmpty())
                continue;

            TArray<FString> Values;
            Line.TrimStartAndEnd().ParseIntoArray(Values, TEXT(" "), true);

            // Remove empty entries
            Values.RemoveAll([](const FString& S) { return S.IsEmpty(); });

            if (Values.Num() >= 5)
            {
                // Values: gpu_idx, sm, mem, enc, dec
                TempData.SM = FCString::Atoi(*Values[1]);
                TempData.MemoryPercent = FCString::Atoi(*Values[2]);
                TempData.Encoder = FCString::Atoi(*Values[3]);
                TempData.Decoder = FCString::Atoi(*Values[4]);
                break;
            }
        }
    }

    // Now lock and copy data to cached storage
    {
        FScopeLock Lock(&GPUDataMutex);

        // Only update if monitoring is still active
        if (bIsMonitoring)
        {
            // Copy temp data to cached data
            CachedGPUData.GPUName = TempData.GPUName;
            CachedGPUData.SM = TempData.SM;
            CachedGPUData.Encoder = TempData.Encoder;
            CachedGPUData.Decoder = TempData.Decoder;
            CachedGPUData.MemoryPercent = TempData.MemoryPercent;
            CachedGPUData.MemoryUsedMB = TempData.MemoryUsedMB;
            CachedGPUData.MemoryTotalMB = TempData.MemoryTotalMB;
            CachedGPUData.Temperature = TempData.Temperature;
            CachedGPUData.PowerDraw = TempData.PowerDraw;
            CachedGPUData.PowerLimit = TempData.PowerLimit;
            CachedGPUData.FanSpeed = TempData.FanSpeed;
            CachedGPUData.Timestamp = TempData.Timestamp;
            CachedGPUData.bIsMonitoring = true;

            // Calculate max instances while holding the lock
            CalculateMaxInstances(CachedGPUData);
        }

        // Update last poll timestamp
        LastPollTimestamp.Store(FDateTime::Now().GetTicks());
    }

    // Release the async poll lock
    bAsyncPollInProgress.Store(false);

    UE_LOG(LogGraphicsSettings, Log, TEXT("PollGPUMetrics: Async poll complete - SM=%d, Enc=%d, VRAM=%d%%"),
        TempData.SM, TempData.Encoder, TempData.MemoryPercent);
}

bool UGraphicsSettingsBFL::ParseNvidiaSmiOutput(const FString& Output, FEmbodyGPUMonitorData& OutData)
{
    // Parsing now done inline in PollGPUMetrics
    return true;
}

void UGraphicsSettingsBFL::CalculateMaxInstances(FEmbodyGPUMonitorData& Data)
{
    // NOTE: This function is called while GPUDataMutex is already held
    // Do not acquire GPUDataMutex here to avoid deadlock

    // Target 95% utilization - leave some headroom but don't be too conservative
    const float TargetUtilization = 95.0f;
    const int32 InstanceCount = FMath::Max(1, CurrentInstanceCount);

    // Calculate SM per instance and max instances
    if (Data.SM > 0)
    {
        float SMPerInstance = (float)Data.SM / (float)InstanceCount;
        // Use RoundToInt for more practical estimates
        Data.MaxInstancesBySM = FMath::Max(1, FMath::RoundToInt(TargetUtilization / SMPerInstance));
    }
    else
    {
        Data.MaxInstancesBySM = 10; // Unknown, assume good
    }

    // Calculate Encoder per instance and max instances
    if (Data.Encoder > 0)
    {
        float EncPerInstance = (float)Data.Encoder / (float)InstanceCount;
        Data.MaxInstancesByEncoder = FMath::Max(1, FMath::RoundToInt(TargetUtilization / EncPerInstance));
    }
    else
    {
        Data.MaxInstancesByEncoder = 12; // NVENC typically handles many streams
    }

    // Calculate VRAM per instance and max instances
    // Use MemoryPercent (bandwidth from dmon) for consistency with SM/Encoder
    if (Data.MemoryPercent > 0)
    {
        float VRAMPerInstance = (float)Data.MemoryPercent / (float)InstanceCount;
        Data.MaxInstancesByVRAM = FMath::Max(1, FMath::RoundToInt(TargetUtilization / VRAMPerInstance));
    }
    else if (Data.MemoryUsedMB > 0 && Data.MemoryTotalMB > 0)
    {
        // Fallback to capacity-based calculation
        int32 VRAMPerInstance = Data.MemoryUsedMB / InstanceCount;
        int32 AvailableVRAM = Data.MemoryTotalMB - 2048; // Leave 2GB for OS/driver
        Data.MaxInstancesByVRAM = FMath::Max(1, AvailableVRAM / FMath::Max(1, VRAMPerInstance));
    }
    else
    {
        Data.MaxInstancesByVRAM = 10; // Unknown
    }

    // Find the bottleneck (minimum of all)
    Data.MaxInstances = FMath::Min3(Data.MaxInstancesBySM, Data.MaxInstancesByEncoder, Data.MaxInstancesByVRAM);

    // Determine bottleneck
    if (Data.MaxInstancesBySM <= Data.MaxInstancesByEncoder && Data.MaxInstancesBySM <= Data.MaxInstancesByVRAM)
    {
        Data.Bottleneck = TEXT("GPU (SM)");
    }
    else if (Data.MaxInstancesByEncoder <= Data.MaxInstancesBySM && Data.MaxInstancesByEncoder <= Data.MaxInstancesByVRAM)
    {
        Data.Bottleneck = TEXT("NVENC");
    }
    else
    {
        Data.Bottleneck = TEXT("VRAM");
    }
}

FEmbodyGPUMonitorData UGraphicsSettingsBFL::GetGPUMonitorData()
{
    FScopeLock Lock(&GPUDataMutex);
    return CachedGPUData;
}

FString UGraphicsSettingsBFL::GetGPUMonitorDataAsJSON()
{
    // Lock and copy data to avoid holding mutex during string formatting
    FEmbodyGPUMonitorData DataCopy;
    int32 InstanceCountCopy;
    {
        FScopeLock Lock(&GPUDataMutex);
        DataCopy = CachedGPUData;
        InstanceCountCopy = CurrentInstanceCount;
    }

    return FString::Printf(TEXT("{"
        "\"sm\":%d,"
        "\"encoder\":%d,"
        "\"decoder\":%d,"
        "\"memoryPercent\":%d,"
        "\"memoryUsedMB\":%d,"
        "\"memoryTotalMB\":%d,"
        "\"temperature\":%d,"
        "\"powerDraw\":%d,"
        "\"powerLimit\":%d,"
        "\"fanSpeed\":%d,"
        "\"gpuName\":\"%s\","
        "\"isMonitoring\":%s,"
        "\"timestamp\":\"%s\","
        "\"maxInstancesBySM\":%d,"
        "\"maxInstancesByEncoder\":%d,"
        "\"maxInstancesByVRAM\":%d,"
        "\"bottleneck\":\"%s\","
        "\"maxInstances\":%d,"
        "\"currentInstances\":%d"
        "}"),
        DataCopy.SM,
        DataCopy.Encoder,
        DataCopy.Decoder,
        DataCopy.MemoryPercent,
        DataCopy.MemoryUsedMB,
        DataCopy.MemoryTotalMB,
        DataCopy.Temperature,
        DataCopy.PowerDraw,
        DataCopy.PowerLimit,
        DataCopy.FanSpeed,
        *DataCopy.GPUName,
        DataCopy.bIsMonitoring ? TEXT("true") : TEXT("false"),
        *DataCopy.Timestamp,
        DataCopy.MaxInstancesBySM,
        DataCopy.MaxInstancesByEncoder,
        DataCopy.MaxInstancesByVRAM,
        *DataCopy.Bottleneck,
        DataCopy.MaxInstances,
        InstanceCountCopy
    );
}

// ============================================
// SM OPTIMIZATION (Sparse Temporal Renderer)
// ============================================

void UGraphicsSettingsBFL::SetDynamicResolution(bool bEnabled)
{
    IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.SparseTemporalRenderer.DynamicResolution"));
    if (CVar)
    {
        CVar->Set(bEnabled ? 1 : 0, ECVF_SetByConsole);
        UE_LOG(LogGraphicsSettings, Log, TEXT("SM Optimization: Dynamic Resolution %s"), bEnabled ? TEXT("ENABLED") : TEXT("DISABLED"));
    }
}

void UGraphicsSettingsBFL::SetMinScreenPercentage(float Percentage)
{
    float Clamped = FMath::Clamp(Percentage, 25.0f, 100.0f);
    IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.SparseTemporalRenderer.MinScreenPercentage"));
    if (CVar)
    {
        CVar->Set(Clamped, ECVF_SetByConsole);
        UE_LOG(LogGraphicsSettings, Log, TEXT("SM Optimization: Min Screen Percentage = %.1f%%"), Clamped);
    }
}

void UGraphicsSettingsBFL::SetMaxScreenPercentage(float Percentage)
{
    float Clamped = FMath::Clamp(Percentage, 50.0f, 100.0f);
    IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.SparseTemporalRenderer.MaxScreenPercentage"));
    if (CVar)
    {
        CVar->Set(Clamped, ECVF_SetByConsole);
        UE_LOG(LogGraphicsSettings, Log, TEXT("SM Optimization: Max Screen Percentage = %.1f%%"), Clamped);
    }
}

void UGraphicsSettingsBFL::SetUseBuiltinVRS(bool bEnabled)
{
    IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.SparseTemporalRenderer.UseBuiltinVRS"));
    if (CVar)
    {
        CVar->Set(bEnabled ? 1 : 0, ECVF_SetByConsole);
        UE_LOG(LogGraphicsSettings, Log, TEXT("SM Optimization: Built-in VRS %s"), bEnabled ? TEXT("ENABLED") : TEXT("DISABLED"));
    }
}

float UGraphicsSettingsBFL::GetCurrentSMReduction()
{
    // Get current screen percentage
    IConsoleVariable* ScreenPctCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ScreenPercentage"));
    if (ScreenPctCVar)
    {
        float ScreenPct = ScreenPctCVar->GetFloat();
        // SM reduction is roughly proportional to pixel count reduction
        // At 50% screen percentage, we're rendering 25% of the pixels = 75% reduction
        float PixelReduction = 1.0f - (ScreenPct / 100.0f) * (ScreenPct / 100.0f);
        return PixelReduction * 100.0f;
    }
    return 0.0f;
}

bool UGraphicsSettingsBFL::IsVRSSupported()
{
    // UE5.7 uses attachment or pipeline VRS
    return GRHISupportsAttachmentVariableRateShading || GRHISupportsPipelineVariableRateShading;
}

void UGraphicsSettingsBFL::EnableSMOptimization()
{
    UE_LOG(LogGraphicsSettings, Log, TEXT("Enabling SM Optimization Mode"));

    // Basic SM optimization via CVars
    SetCVar(TEXT("r.ScreenPercentage"), 50.0f);
    SetCVar(TEXT("r.AntiAliasingMethod"), 4);  // TSR
    SetCVar(TEXT("r.VolumetricFog"), 0);
    SetCVar(TEXT("r.MotionBlurQuality"), 0);
    SetCVar(TEXT("r.DepthOfFieldQuality"), 0);
    SetCVar(TEXT("r.BloomQuality"), 1);

    UE_LOG(LogGraphicsSettings, Log, TEXT("SM Optimization Mode ENABLED"));
}

void UGraphicsSettingsBFL::DisableSMOptimization()
{
    UE_LOG(LogGraphicsSettings, Log, TEXT("Disabling SM Optimization Mode"));

    // Restore default settings
    SetCVar(TEXT("r.ScreenPercentage"), 100.0f);
    SetCVar(TEXT("r.VolumetricFog"), 1);
    SetCVar(TEXT("r.BloomQuality"), 5);

    UE_LOG(LogGraphicsSettings, Log, TEXT("SM Optimization Mode DISABLED"));
}

void UGraphicsSettingsBFL::ApplyMaxSMReduction()
{
    UE_LOG(LogGraphicsSettings, Log, TEXT("Applying MAXIMUM SM Reduction"));

    SetCVar(TEXT("r.ScreenPercentage"), 25.0f);
    SetCVar(TEXT("r.AntiAliasingMethod"), 4);  // TSR
    SetCVar(TEXT("r.VolumetricFog"), 0);
    SetCVar(TEXT("r.MotionBlurQuality"), 0);
    SetCVar(TEXT("r.DepthOfFieldQuality"), 0);
    SetCVar(TEXT("r.BloomQuality"), 0);
    SetCVar(TEXT("r.Shadow.MaxResolution"), 512);
    SetCVar(TEXT("r.Shadow.MaxCSMResolution"), 512);

    UE_LOG(LogGraphicsSettings, Log, TEXT("MAXIMUM SM Reduction APPLIED"));
}

// ============================================
// SCENE ANALYSIS - Identify expensive objects
// ============================================

FEmbodyExpensiveObject UGraphicsSettingsBFL::AnalyzeActor(AActor* Actor)
{
    FEmbodyExpensiveObject Result;

    if (!Actor)
    {
        return Result;
    }

    Result.ObjectName = Actor->GetName();
    Result.ClassName = Actor->GetClass()->GetName();

    float CostScore = 0.0f;
    TArray<FString> CostFactors;

    // Check for SkeletalMeshComponents
    TArray<USkeletalMeshComponent*> SkeletalMeshComps;
    Actor->GetComponents<USkeletalMeshComponent>(SkeletalMeshComps);

    for (USkeletalMeshComponent* SkelComp : SkeletalMeshComps)
    {
        if (!SkelComp || !SkelComp->GetSkeletalMeshAsset())
        {
            continue;
        }

        Result.bIsSkeletalMesh = true;
        USkeletalMesh* Mesh = SkelComp->GetSkeletalMeshAsset();

        // Bone count
        Result.BoneCount = Mesh->GetRefSkeleton().GetNum();

        // Estimate triangle count based on bone count
        // MetaHumans typically have 50k-150k triangles with 200+ bones
        if (Result.BoneCount > 200)
        {
            Result.TriangleCount += 100000;  // MetaHuman-class detail
        }
        else if (Result.BoneCount > 50)
        {
            Result.TriangleCount += 30000;   // Standard character
        }
        else
        {
            Result.TriangleCount += 5000;    // Simple skeletal mesh
        }
        if (Result.BoneCount > 100)
        {
            CostScore += Result.BoneCount * 0.5f;  // High bone count is expensive
            CostFactors.Add(FString::Printf(TEXT("HighBones:%d"), Result.BoneCount));
        }

        // Morph targets
        Result.MorphTargetCount = Mesh->GetMorphTargets().Num();
        if (Result.MorphTargetCount > 0)
        {
            CostScore += Result.MorphTargetCount * 10.0f;  // Morphs are expensive
            CostFactors.Add(FString::Printf(TEXT("Morphs:%d"), Result.MorphTargetCount));
        }

        // Materials
        Result.MaterialCount += SkelComp->GetNumMaterials();

        // Check materials for SSS
        for (int32 i = 0; i < SkelComp->GetNumMaterials(); i++)
        {
            UMaterialInterface* Mat = SkelComp->GetMaterial(i);
            if (Mat)
            {
                FString MatName = Mat->GetName().ToLower();
                if (MatName.Contains(TEXT("skin")) || MatName.Contains(TEXT("sss")) ||
                    MatName.Contains(TEXT("subsurface")) || MatName.Contains(TEXT("head")) ||
                    MatName.Contains(TEXT("body")))
                {
                    Result.bHasSSS = true;
                    CostScore += 500.0f;  // SSS is very expensive
                    CostFactors.Add(TEXT("SSS"));
                    break;
                }
            }
        }
    }

    // Check for StaticMeshComponents
    TArray<UStaticMeshComponent*> StaticMeshComps;
    Actor->GetComponents<UStaticMeshComponent>(StaticMeshComps);

    for (UStaticMeshComponent* StaticComp : StaticMeshComps)
    {
        if (!StaticComp || !StaticComp->GetStaticMesh())
        {
            continue;
        }

        UStaticMesh* Mesh = StaticComp->GetStaticMesh();

        // Get triangle count from render data
        if (Mesh->GetRenderData() && Mesh->GetRenderData()->LODResources.Num() > 0)
        {
            // Use GetNumTriangles which is the safe way to get total triangles
            Result.TriangleCount += Mesh->GetRenderData()->LODResources[0].GetNumTriangles();
        }

        Result.MaterialCount += StaticComp->GetNumMaterials();
    }

    // Check for Groom/Hair components (by class name since we don't want hard dependency)
    TArray<UActorComponent*> AllComps;
    Actor->GetComponents(AllComps);

    for (UActorComponent* Comp : AllComps)
    {
        if (Comp)
        {
            FString CompClassName = Comp->GetClass()->GetName();
            if (CompClassName.Contains(TEXT("Groom")) || CompClassName.Contains(TEXT("Hair")))
            {
                Result.bHasHairStrands = true;
                CostScore += 2000.0f;  // Hair strands are VERY expensive
                CostFactors.Add(TEXT("HairStrands"));
            }
        }
    }

    // Triangle cost
    if (Result.TriangleCount > 0)
    {
        CostScore += Result.TriangleCount * 0.01f;  // 0.01 per triangle
        if (Result.TriangleCount > 100000)
        {
            CostFactors.Add(FString::Printf(TEXT("HighPoly:%dk"), Result.TriangleCount / 1000));
        }
    }

    // Material cost
    CostScore += Result.MaterialCount * 50.0f;  // Each material adds cost

    // Skeletal mesh base cost
    if (Result.bIsSkeletalMesh)
    {
        CostScore += 200.0f;  // Base skeletal mesh cost (skinning)
        CostFactors.Add(TEXT("Skeletal"));
    }

    Result.CostScore = CostScore;

    // Build cost breakdown string
    if (CostFactors.Num() > 0)
    {
        Result.CostBreakdown = FString::Join(CostFactors, TEXT(", "));
    }
    else
    {
        Result.CostBreakdown = TEXT("Standard");
    }

    return Result;
}

FEmbodySceneAnalysis UGraphicsSettingsBFL::AnalyzeSceneObjects(UObject* WorldContextObject, int32 MaxResults)
{
    FEmbodySceneAnalysis Result;
    Result.Timestamp = FDateTime::Now().ToString();

    if (!WorldContextObject)
    {
        UE_LOG(LogGraphicsSettings, Warning, TEXT("AnalyzeSceneObjects: No world context"));
        return Result;
    }

    UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
    if (!World)
    {
        UE_LOG(LogGraphicsSettings, Warning, TEXT("AnalyzeSceneObjects: Could not get world"));
        return Result;
    }

    UE_LOG(LogGraphicsSettings, Log, TEXT("AnalyzeSceneObjects: Starting scene analysis..."));

    TArray<FEmbodyExpensiveObject> AllObjects;

    // Iterate through all actors
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        AActor* Actor = *It;
        if (!Actor || Actor->IsPendingKillPending())
        {
            continue;
        }

        // Skip editor-only actors
        if (Actor->HasAnyFlags(RF_Transient))
        {
            continue;
        }

        FEmbodyExpensiveObject ObjData = AnalyzeActor(Actor);

        // Only include objects with some cost
        if (ObjData.CostScore > 0.0f)
        {
            AllObjects.Add(ObjData);
            Result.TotalSceneCost += ObjData.CostScore;
            Result.ObjectCount++;
        }
    }

    // Sort by cost (highest first)
    AllObjects.Sort([](const FEmbodyExpensiveObject& A, const FEmbodyExpensiveObject& B)
    {
        return A.CostScore > B.CostScore;
    });

    // Take top N results
    int32 ResultCount = FMath::Min(MaxResults, AllObjects.Num());
    for (int32 i = 0; i < ResultCount; i++)
    {
        Result.ExpensiveObjects.Add(AllObjects[i]);
    }

    // Generate recommendations
    bool bHasHairWarning = false;
    bool bHasSSSWarning = false;
    bool bHasMorphWarning = false;
    bool bHasHighPolyWarning = false;

    for (const FEmbodyExpensiveObject& Obj : Result.ExpensiveObjects)
    {
        if (Obj.bHasHairStrands && !bHasHairWarning)
        {
            Result.Recommendations.Add(FString::Printf(TEXT("HAIR: '%s' uses hair strands (~15%% SM). Consider r.HairStrands.Visibility.MSAA.SamplePerPixel=1"), *Obj.ObjectName));
            bHasHairWarning = true;
        }
        if (Obj.bHasSSS && !bHasSSSWarning)
        {
            Result.Recommendations.Add(FString::Printf(TEXT("SSS: '%s' uses subsurface scattering (~8%% SM). Enable r.SSS.HalfRes=1"), *Obj.ObjectName));
            bHasSSSWarning = true;
        }
        if (Obj.MorphTargetCount > 50 && !bHasMorphWarning)
        {
            Result.Recommendations.Add(FString::Printf(TEXT("MORPHS: '%s' has %d morph targets. Consider reducing facial bones/morphs"), *Obj.ObjectName, Obj.MorphTargetCount));
            bHasMorphWarning = true;
        }
        if (Obj.TriangleCount > 200000 && !bHasHighPolyWarning)
        {
            Result.Recommendations.Add(FString::Printf(TEXT("POLY: '%s' has %dk triangles. Consider LOD or mesh simplification"), *Obj.ObjectName, Obj.TriangleCount / 1000));
            bHasHighPolyWarning = true;
        }
    }

    UE_LOG(LogGraphicsSettings, Log, TEXT("AnalyzeSceneObjects: Found %d objects, Total cost: %.0f"), Result.ObjectCount, Result.TotalSceneCost);

    // Log top 5 expensive objects
    for (int32 i = 0; i < FMath::Min(5, Result.ExpensiveObjects.Num()); i++)
    {
        const FEmbodyExpensiveObject& Obj = Result.ExpensiveObjects[i];
        UE_LOG(LogGraphicsSettings, Log, TEXT("  #%d: %s (%.0f) - %s"),
            i + 1, *Obj.ObjectName, Obj.CostScore, *Obj.CostBreakdown);
    }

    return Result;
}

FString UGraphicsSettingsBFL::GetSceneAnalysisAsJSON(UObject* WorldContextObject, int32 MaxResults)
{
    FEmbodySceneAnalysis Analysis = AnalyzeSceneObjects(WorldContextObject, MaxResults);

    FString JSON = TEXT("{");
    JSON += FString::Printf(TEXT("\"totalCost\":%.0f,"), Analysis.TotalSceneCost);
    JSON += FString::Printf(TEXT("\"objectCount\":%d,"), Analysis.ObjectCount);
    JSON += FString::Printf(TEXT("\"timestamp\":\"%s\","), *Analysis.Timestamp);

    // Expensive objects array
    JSON += TEXT("\"expensiveObjects\":[");
    for (int32 i = 0; i < Analysis.ExpensiveObjects.Num(); i++)
    {
        const FEmbodyExpensiveObject& Obj = Analysis.ExpensiveObjects[i];
        if (i > 0) JSON += TEXT(",");
        JSON += TEXT("{");
        JSON += FString::Printf(TEXT("\"name\":\"%s\","), *Obj.ObjectName.Replace(TEXT("\""), TEXT("\\\"")));
        JSON += FString::Printf(TEXT("\"class\":\"%s\","), *Obj.ClassName);
        JSON += FString::Printf(TEXT("\"cost\":%.0f,"), Obj.CostScore);
        JSON += FString::Printf(TEXT("\"triangles\":%d,"), Obj.TriangleCount);
        JSON += FString::Printf(TEXT("\"materials\":%d,"), Obj.MaterialCount);
        JSON += FString::Printf(TEXT("\"hasHair\":%s,"), Obj.bHasHairStrands ? TEXT("true") : TEXT("false"));
        JSON += FString::Printf(TEXT("\"hasSSS\":%s,"), Obj.bHasSSS ? TEXT("true") : TEXT("false"));
        JSON += FString::Printf(TEXT("\"isSkeletal\":%s,"), Obj.bIsSkeletalMesh ? TEXT("true") : TEXT("false"));
        JSON += FString::Printf(TEXT("\"morphs\":%d,"), Obj.MorphTargetCount);
        JSON += FString::Printf(TEXT("\"bones\":%d,"), Obj.BoneCount);
        JSON += FString::Printf(TEXT("\"breakdown\":\"%s\""), *Obj.CostBreakdown);
        JSON += TEXT("}");
    }
    JSON += TEXT("],");

    // Recommendations array
    JSON += TEXT("\"recommendations\":[");
    for (int32 i = 0; i < Analysis.Recommendations.Num(); i++)
    {
        if (i > 0) JSON += TEXT(",");
        JSON += FString::Printf(TEXT("\"%s\""), *Analysis.Recommendations[i].Replace(TEXT("\""), TEXT("\\\"")));
    }
    JSON += TEXT("]");

    JSON += TEXT("}");

    return JSON;
}

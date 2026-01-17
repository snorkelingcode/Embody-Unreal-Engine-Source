// GraphicsSettingsBFL.h
// Graphics settings control for Pixel Streaming optimization
// Exposes CVars for runtime adjustment and monitoring
// Thread-safe for multi-client Pixel Streaming scenarios

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "HAL/CriticalSection.h"
#include "Async/Async.h"
#include "GraphicsSettingsBFL.generated.h"

UENUM(BlueprintType)
enum class EEmbodyGraphicsPreset : uint8
{
    Optimized           UMETA(DisplayName = "Optimized"),
    MaxSettings         UMETA(DisplayName = "Max Settings"),
    Custom              UMETA(DisplayName = "Custom")
};

UENUM(BlueprintType)
enum class EEmbodyGPUType : uint8
{
    RTX_4090     UMETA(DisplayName = "RTX 4090 (24GB)"),
    RTX_6000_Ada UMETA(DisplayName = "RTX 6000 Ada (48GB)"),
    RTX_4080_Super UMETA(DisplayName = "RTX 4080 SUPER (16GB)"),
    RTX_4080     UMETA(DisplayName = "RTX 4080 (16GB)"),
    RTX_3090     UMETA(DisplayName = "RTX 3090 (24GB)"),
    RTX_3080_Ti  UMETA(DisplayName = "RTX 3080 Ti (12GB)"),
    Custom       UMETA(DisplayName = "Custom VRAM")
};

// Real-time GPU monitoring data from nvidia-smi
USTRUCT(BlueprintType)
struct FEmbodyGPUMonitorData
{
    GENERATED_BODY()

    // GPU SM (Shader/Compute) utilization %
    UPROPERTY(BlueprintReadOnly, Category = "GPU Monitor")
    int32 SM = 0;

    // NVENC Encoder utilization %
    UPROPERTY(BlueprintReadOnly, Category = "GPU Monitor")
    int32 Encoder = 0;

    // NVDEC Decoder utilization %
    UPROPERTY(BlueprintReadOnly, Category = "GPU Monitor")
    int32 Decoder = 0;

    // Memory utilization %
    UPROPERTY(BlueprintReadOnly, Category = "GPU Monitor")
    int32 MemoryPercent = 0;

    // Memory used in MB
    UPROPERTY(BlueprintReadOnly, Category = "GPU Monitor")
    int32 MemoryUsedMB = 0;

    // Memory total in MB
    UPROPERTY(BlueprintReadOnly, Category = "GPU Monitor")
    int32 MemoryTotalMB = 0;

    // GPU Temperature in Celsius
    UPROPERTY(BlueprintReadOnly, Category = "GPU Monitor")
    int32 Temperature = 0;

    // Power draw in Watts
    UPROPERTY(BlueprintReadOnly, Category = "GPU Monitor")
    int32 PowerDraw = 0;

    // Power limit in Watts
    UPROPERTY(BlueprintReadOnly, Category = "GPU Monitor")
    int32 PowerLimit = 0;

    // Fan speed %
    UPROPERTY(BlueprintReadOnly, Category = "GPU Monitor")
    int32 FanSpeed = 0;

    // GPU Name
    UPROPERTY(BlueprintReadOnly, Category = "GPU Monitor")
    FString GPUName;

    // Is monitoring active
    UPROPERTY(BlueprintReadOnly, Category = "GPU Monitor")
    bool bIsMonitoring = false;

    // Timestamp
    UPROPERTY(BlueprintReadOnly, Category = "GPU Monitor")
    FString Timestamp;

    // Calculated: Max instances by SM (target 85%)
    UPROPERTY(BlueprintReadOnly, Category = "GPU Monitor")
    int32 MaxInstancesBySM = 0;

    // Calculated: Max instances by Encoder (target 85%)
    UPROPERTY(BlueprintReadOnly, Category = "GPU Monitor")
    int32 MaxInstancesByEncoder = 0;

    // Calculated: Max instances by VRAM
    UPROPERTY(BlueprintReadOnly, Category = "GPU Monitor")
    int32 MaxInstancesByVRAM = 0;

    // Calculated: Current bottleneck
    UPROPERTY(BlueprintReadOnly, Category = "GPU Monitor")
    FString Bottleneck;

    // Calculated: Recommended max instances
    UPROPERTY(BlueprintReadOnly, Category = "GPU Monitor")
    int32 MaxInstances = 0;
};

// Settings struct for frontend communication
USTRUCT(BlueprintType)
struct FEmbodyGraphicsSettings
{
    GENERATED_BODY()

    // Current preset
    UPROPERTY(BlueprintReadWrite, Category = "Graphics")
    EEmbodyGraphicsPreset Preset = EEmbodyGraphicsPreset::Optimized;

    // Texture Streaming
    UPROPERTY(BlueprintReadWrite, Category = "Textures")
    bool bTextureStreaming = true;

    UPROPERTY(BlueprintReadWrite, Category = "Textures")
    int32 StreamingPoolSize = 2000;

    UPROPERTY(BlueprintReadWrite, Category = "Textures")
    int32 MaxAnisotropy = 8;

    // Shadows
    UPROPERTY(BlueprintReadWrite, Category = "Shadows")
    int32 ShadowResolution = 1024;

    UPROPERTY(BlueprintReadWrite, Category = "Shadows")
    bool bVirtualShadowMaps = false;

    UPROPERTY(BlueprintReadWrite, Category = "Shadows")
    bool bContactShadows = false;

    // Skin/SSS
    UPROPERTY(BlueprintReadWrite, Category = "Skin")
    int32 SSSQuality = 0;

    UPROPERTY(BlueprintReadWrite, Category = "Skin")
    bool bSSSHalfRes = true;

    // Hair
    UPROPERTY(BlueprintReadWrite, Category = "Hair")
    bool bHairStrands = true;

    UPROPERTY(BlueprintReadWrite, Category = "Hair")
    int32 HairMSAASamples = 2;

    // Global Illumination
    UPROPERTY(BlueprintReadWrite, Category = "GI")
    int32 GIMethod = 0; // 0=None, 1=SSGI, 2=Lumen

    UPROPERTY(BlueprintReadWrite, Category = "GI")
    bool bLumenReflections = false;

    UPROPERTY(BlueprintReadWrite, Category = "GI")
    bool bHardwareRayTracing = false;

    // Anti-Aliasing
    UPROPERTY(BlueprintReadWrite, Category = "AA")
    int32 AAMethod = 4; // 2=TAA, 4=TSR

    UPROPERTY(BlueprintReadWrite, Category = "AA")
    int32 ScreenPercentage = 100;

    UPROPERTY(BlueprintReadWrite, Category = "AA")
    int32 TSRHistoryPercentage = 100;

    // Post Processing
    UPROPERTY(BlueprintReadWrite, Category = "PostProcess")
    bool bVolumetricFog = false;

    UPROPERTY(BlueprintReadWrite, Category = "PostProcess")
    int32 BloomQuality = 3;

    UPROPERTY(BlueprintReadWrite, Category = "PostProcess")
    int32 AOLevels = 0;

    // LOD
    UPROPERTY(BlueprintReadWrite, Category = "LOD")
    int32 ForceLOD = -1;

    UPROPERTY(BlueprintReadWrite, Category = "LOD")
    int32 SkeletalMeshLODBias = 0;

    // Ray Tracing
    UPROPERTY(BlueprintReadWrite, Category = "RayTracing")
    bool bRTShadows = false;

    UPROPERTY(BlueprintReadWrite, Category = "RayTracing")
    bool bRTGI = false;

    UPROPERTY(BlueprintReadWrite, Category = "RayTracing")
    bool bRTAO = false;
};

// Performance metrics struct
USTRUCT(BlueprintType)
struct FEmbodyPerformanceMetrics
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Performance")
    float FPS = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Performance")
    float FrameTimeMs = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Performance")
    float GPUTimeMs = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Performance")
    float GameThreadTimeMs = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Performance")
    float RenderThreadTimeMs = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Performance")
    int64 TextureMemoryMB = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Performance")
    int64 MeshMemoryMB = 0;
};

// Instance estimation struct
USTRUCT(BlueprintType)
struct FEmbodyInstanceEstimate
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Scaling")
    int32 MaxByVRAM = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Scaling")
    int32 MaxByGPU = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Scaling")
    int32 MaxInstances = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Scaling")
    FString Bottleneck = TEXT("Unknown");

    UPROPERTY(BlueprintReadOnly, Category = "Scaling")
    int32 EstimatedVRAMPerInstanceMB = 2048;
};

// Expensive object data - identifies GPU-heavy objects in the scene
USTRUCT(BlueprintType)
struct FEmbodyExpensiveObject
{
    GENERATED_BODY()

    // Actor/Object name
    UPROPERTY(BlueprintReadOnly, Category = "GPU Cost")
    FString ObjectName;

    // Actor class name
    UPROPERTY(BlueprintReadOnly, Category = "GPU Cost")
    FString ClassName;

    // Estimated GPU cost score (higher = more expensive)
    UPROPERTY(BlueprintReadOnly, Category = "GPU Cost")
    float CostScore = 0.0f;

    // Triangle count
    UPROPERTY(BlueprintReadOnly, Category = "GPU Cost")
    int32 TriangleCount = 0;

    // Number of materials
    UPROPERTY(BlueprintReadOnly, Category = "GPU Cost")
    int32 MaterialCount = 0;

    // Has hair strands (very expensive)
    UPROPERTY(BlueprintReadOnly, Category = "GPU Cost")
    bool bHasHairStrands = false;

    // Has SSS/skin materials
    UPROPERTY(BlueprintReadOnly, Category = "GPU Cost")
    bool bHasSSS = false;

    // Is skeletal mesh (morphs, skinning)
    UPROPERTY(BlueprintReadOnly, Category = "GPU Cost")
    bool bIsSkeletalMesh = false;

    // Number of morph targets
    UPROPERTY(BlueprintReadOnly, Category = "GPU Cost")
    int32 MorphTargetCount = 0;

    // Number of bones
    UPROPERTY(BlueprintReadOnly, Category = "GPU Cost")
    int32 BoneCount = 0;

    // Cost breakdown string
    UPROPERTY(BlueprintReadOnly, Category = "GPU Cost")
    FString CostBreakdown;
};

// Scene analysis result
USTRUCT(BlueprintType)
struct FEmbodySceneAnalysis
{
    GENERATED_BODY()

    // Total estimated scene cost
    UPROPERTY(BlueprintReadOnly, Category = "Scene Analysis")
    float TotalSceneCost = 0.0f;

    // Number of objects analyzed
    UPROPERTY(BlueprintReadOnly, Category = "Scene Analysis")
    int32 ObjectCount = 0;

    // Top expensive objects (sorted by cost)
    UPROPERTY(BlueprintReadOnly, Category = "Scene Analysis")
    TArray<FEmbodyExpensiveObject> ExpensiveObjects;

    // Summary recommendations
    UPROPERTY(BlueprintReadOnly, Category = "Scene Analysis")
    TArray<FString> Recommendations;

    // Timestamp of analysis
    UPROPERTY(BlueprintReadOnly, Category = "Scene Analysis")
    FString Timestamp;
};

UCLASS()
class EMBODY_API UGraphicsSettingsBFL : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:

    // ============================================
    // PRESET MANAGEMENT
    // ============================================

    UFUNCTION(BlueprintCallable, Category = "Embody|Graphics", meta = (DisplayName = "Set Graphics Preset"))
    static void SetGraphicsPreset(EEmbodyGraphicsPreset Preset);

    UFUNCTION(BlueprintPure, Category = "Embody|Graphics")
    static EEmbodyGraphicsPreset GetCurrentGraphicsPreset();

    // ============================================
    // SETTINGS RETRIEVAL
    // ============================================

    /** Get all current graphics settings as a struct */
    UFUNCTION(BlueprintPure, Category = "Embody|Graphics")
    static FEmbodyGraphicsSettings GetCurrentSettings();

    /** Get current settings as JSON string for frontend */
    UFUNCTION(BlueprintPure, Category = "Embody|Graphics")
    static FString GetSettingsAsJSON();

    /** Get current performance metrics */
    UFUNCTION(BlueprintPure, Category = "Embody|Graphics")
    static FEmbodyPerformanceMetrics GetPerformanceMetrics();

    /** Get performance metrics as JSON string */
    UFUNCTION(BlueprintPure, Category = "Embody|Graphics")
    static FString GetPerformanceMetricsAsJSON();

    // ============================================
    // INSTANCE ESTIMATION
    // ============================================

    /** Estimate max instances for a given GPU type */
    UFUNCTION(BlueprintPure, Category = "Embody|Graphics")
    static FEmbodyInstanceEstimate EstimateMaxInstances(EEmbodyGPUType GPUType, int32 CustomVRAMMB = 0);

    /** Get VRAM for GPU type in MB */
    UFUNCTION(BlueprintPure, Category = "Embody|Graphics")
    static int32 GetGPUVRAM(EEmbodyGPUType GPUType);

    // ============================================
    // REAL-TIME GPU MONITORING (nvidia-smi)
    // ============================================

    /** Start GPU monitoring - runs nvidia-smi in background */
    UFUNCTION(BlueprintCallable, Category = "Embody|GPU Monitor")
    static void StartGPUMonitoring();

    /** Stop GPU monitoring */
    UFUNCTION(BlueprintCallable, Category = "Embody|GPU Monitor")
    static void StopGPUMonitoring();

    /** Get current GPU monitoring data */
    UFUNCTION(BlueprintPure, Category = "Embody|GPU Monitor")
    static FEmbodyGPUMonitorData GetGPUMonitorData();

    /** Get GPU monitor data as JSON string for frontend */
    UFUNCTION(BlueprintPure, Category = "Embody|GPU Monitor")
    static FString GetGPUMonitorDataAsJSON();

    /** Check if monitoring is active */
    UFUNCTION(BlueprintPure, Category = "Embody|GPU Monitor")
    static bool IsGPUMonitoringActive();

    /** Poll nvidia-smi once and update cached data */
    UFUNCTION(BlueprintCallable, Category = "Embody|GPU Monitor")
    static void PollGPUMetrics();

    /** Set the current instance count (for per-instance calculations) */
    UFUNCTION(BlueprintCallable, Category = "Embody|GPU Monitor")
    static void SetCurrentInstanceCount(int32 Count);

    /** Get estimated VRAM per instance based on current usage */
    UFUNCTION(BlueprintPure, Category = "Embody|GPU Monitor")
    static int32 GetEstimatedVRAMPerInstance();

    // ============================================
    // SCENE ANALYSIS - Identify expensive objects
    // ============================================

    /** Analyze all visible objects and return their estimated GPU cost */
    UFUNCTION(BlueprintCallable, Category = "Embody|Scene Analysis", meta = (WorldContext = "WorldContextObject"))
    static FEmbodySceneAnalysis AnalyzeSceneObjects(UObject* WorldContextObject, int32 MaxResults = 20);

    /** Get scene analysis as JSON string for frontend */
    UFUNCTION(BlueprintCallable, Category = "Embody|Scene Analysis", meta = (WorldContext = "WorldContextObject"))
    static FString GetSceneAnalysisAsJSON(UObject* WorldContextObject, int32 MaxResults = 20);

    /** Analyze a specific actor's GPU cost */
    UFUNCTION(BlueprintCallable, Category = "Embody|Scene Analysis")
    static FEmbodyExpensiveObject AnalyzeActor(AActor* Actor);

    // ============================================
    // INDIVIDUAL SETTING CONTROLS
    // ============================================

    // Texture Streaming
    UFUNCTION(BlueprintCallable, Category = "Embody|Graphics|Textures")
    static void SetTextureStreaming(bool bEnabled);

    UFUNCTION(BlueprintCallable, Category = "Embody|Graphics|Textures")
    static void SetStreamingPoolSize(int32 SizeMB);

    UFUNCTION(BlueprintCallable, Category = "Embody|Graphics|Textures")
    static void SetMaxAnisotropy(int32 Value);

    // Shadows
    UFUNCTION(BlueprintCallable, Category = "Embody|Graphics|Shadows")
    static void SetShadowResolution(int32 Resolution);

    UFUNCTION(BlueprintCallable, Category = "Embody|Graphics|Shadows")
    static void SetVirtualShadowMaps(bool bEnabled);

    UFUNCTION(BlueprintCallable, Category = "Embody|Graphics|Shadows")
    static void SetContactShadows(bool bEnabled);

    // Skin/SSS
    UFUNCTION(BlueprintCallable, Category = "Embody|Graphics|Skin")
    static void SetSSSQuality(int32 Quality);

    UFUNCTION(BlueprintCallable, Category = "Embody|Graphics|Skin")
    static void SetSSSHalfRes(bool bHalfRes);

    // Hair
    UFUNCTION(BlueprintCallable, Category = "Embody|Graphics|Hair")
    static void SetHairStrands(bool bEnabled);

    UFUNCTION(BlueprintCallable, Category = "Embody|Graphics|Hair")
    static void SetHairMSAASamples(int32 Samples);

    // Global Illumination
    UFUNCTION(BlueprintCallable, Category = "Embody|Graphics|GI")
    static void SetGIMethod(int32 Method);

    UFUNCTION(BlueprintCallable, Category = "Embody|Graphics|GI")
    static void SetLumenReflections(bool bEnabled);

    UFUNCTION(BlueprintCallable, Category = "Embody|Graphics|GI")
    static void SetHardwareRayTracing(bool bEnabled);

    // Anti-Aliasing
    UFUNCTION(BlueprintCallable, Category = "Embody|Graphics|AA")
    static void SetAAMethod(int32 Method);

    UFUNCTION(BlueprintCallable, Category = "Embody|Graphics|AA")
    static void SetScreenPercentage(int32 Percentage);

    UFUNCTION(BlueprintCallable, Category = "Embody|Graphics|AA")
    static void SetTSRHistoryPercentage(int32 Percentage);

    // Post Processing
    UFUNCTION(BlueprintCallable, Category = "Embody|Graphics|PostProcess")
    static void SetVolumetricFog(bool bEnabled);

    UFUNCTION(BlueprintCallable, Category = "Embody|Graphics|PostProcess")
    static void SetBloomQuality(int32 Quality);

    UFUNCTION(BlueprintCallable, Category = "Embody|Graphics|PostProcess")
    static void SetAOLevels(int32 Levels);

    // LOD
    UFUNCTION(BlueprintCallable, Category = "Embody|Graphics|LOD")
    static void SetForceLOD(int32 LOD);

    UFUNCTION(BlueprintCallable, Category = "Embody|Graphics|LOD")
    static void SetSkeletalMeshLODBias(int32 Bias);

    // Ray Tracing
    UFUNCTION(BlueprintCallable, Category = "Embody|Graphics|RayTracing")
    static void SetRTShadows(bool bEnabled);

    UFUNCTION(BlueprintCallable, Category = "Embody|Graphics|RayTracing")
    static void SetRTGI(bool bEnabled);

    UFUNCTION(BlueprintCallable, Category = "Embody|Graphics|RayTracing")
    static void SetRTAO(bool bEnabled);

    // ============================================
    // SM OPTIMIZATION (Sparse Temporal Renderer)
    // ============================================

    /** Enable/disable dynamic resolution scaling based on scene stability */
    UFUNCTION(BlueprintCallable, Category = "Embody|SM Optimization", meta = (DisplayName = "Set Dynamic Resolution"))
    static void SetDynamicResolution(bool bEnabled);

    /** Set minimum screen percentage when scene is stable (25-100, default 50) */
    UFUNCTION(BlueprintCallable, Category = "Embody|SM Optimization")
    static void SetMinScreenPercentage(float Percentage);

    /** Set maximum screen percentage when scene has motion (50-100, default 100) */
    UFUNCTION(BlueprintCallable, Category = "Embody|SM Optimization")
    static void SetMaxScreenPercentage(float Percentage);

    /** Enable/disable hardware VRS when supported */
    UFUNCTION(BlueprintCallable, Category = "Embody|SM Optimization")
    static void SetUseBuiltinVRS(bool bEnabled);

    /** Get current SM reduction percentage (from dynamic resolution) */
    UFUNCTION(BlueprintPure, Category = "Embody|SM Optimization")
    static float GetCurrentSMReduction();

    /** Check if VRS hardware is supported */
    UFUNCTION(BlueprintPure, Category = "Embody|SM Optimization")
    static bool IsVRSSupported();

    /** Enable SM optimization mode - sets aggressive dynamic resolution + VRS */
    UFUNCTION(BlueprintCallable, Category = "Embody|SM Optimization", meta = (DisplayName = "Enable SM Optimization"))
    static void EnableSMOptimization();

    /** Disable SM optimization mode - returns to full resolution */
    UFUNCTION(BlueprintCallable, Category = "Embody|SM Optimization", meta = (DisplayName = "Disable SM Optimization"))
    static void DisableSMOptimization();

    /** Apply maximum SM reduction for highest instance count (25% resolution, hair OFF, aggressive LOD) */
    UFUNCTION(BlueprintCallable, Category = "Embody|SM Optimization", meta = (DisplayName = "Apply Max SM Reduction"))
    static void ApplyMaxSMReduction();

    // ============================================
    // COMMAND PARSING (for Pixel Streaming)
    // ============================================

    /** Parse and execute a graphics command from Pixel Streaming frontend */
    UFUNCTION(BlueprintCallable, Category = "Embody|Graphics")
    static bool ParseGraphicsCommand(const FString& Command, FString& OutResponse);

    /** Apply settings from JSON string */
    UFUNCTION(BlueprintCallable, Category = "Embody|Graphics")
    static bool ApplySettingsFromJSON(const FString& JSONString);

private:

    static void SetCVar(const TCHAR* CVarName, int32 Value);
    static void SetCVar(const TCHAR* CVarName, float Value);
    static int32 GetCVarInt(const TCHAR* CVarName);
    static float GetCVarFloat(const TCHAR* CVarName);
    static EEmbodyGraphicsPreset CurrentPreset;

    // GPU Monitoring state - protected by GPUDataMutex
    static FEmbodyGPUMonitorData CachedGPUData;
    static bool bIsMonitoring;
    static int32 CurrentInstanceCount;

    // Thread safety - critical section for multi-client access
    static FCriticalSection GPUDataMutex;
    static FCriticalSection SettingsMutex;

    // Async polling state
    static TAtomic<bool> bAsyncPollInProgress;
    static TAtomic<int64> LastPollTimestamp;

    // Parse nvidia-smi output
    static bool ParseNvidiaSmiOutput(const FString& Output, FEmbodyGPUMonitorData& OutData);
    static void CalculateMaxInstances(FEmbodyGPUMonitorData& Data);

    // Internal async implementation
    static void PollGPUMetricsAsync_Internal();
};

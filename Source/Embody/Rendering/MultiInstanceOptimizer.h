// MultiInstanceOptimizer.h
// Blueprint Function Library for GPU optimization in multi-instance pixel streaming scenarios
//
// PURPOSE: Enable running 20-30 MetaHuman game instances on a single GPU for pixel streaming
//
// USAGE: This is a STANDALONE utility that does NOT auto-run.
// Call from any Blueprint (Level Blueprint, GameMode, Actor BeginPlay, etc.) when needed:
//   Event BeginPlay -> Apply Multi Instance Optimizations (Profile: MaxInstances)
//
// IMPORTANT: Does NOT modify NEUROSYNCGameMode or any GameMode
//
// Copyright (c) 2025 Embody AI

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "MultiInstanceOptimizer.generated.h"

/**
 * Optimization profile presets for multi-instance deployment
 * Each profile balances quality vs instance count differently
 */
UENUM(BlueprintType)
enum class EMultiInstanceProfile : uint8
{
    /** Maximum instances (30+): Aggressive optimization for highest instance count
     *  - 40% screen resolution with TSR upscaling
     *  - Hair strands disabled (use cards instead)
     *  - SSS at half resolution, quality 0
     *  - 512px shadows, no VSM
     *  - 30 FPS cap, 2Mbps encoder bitrate
     *  - VRS enabled (4x4 for stable, 2x2 for motion)
     */
    MaxInstances    UMETA(DisplayName = "Max Instances (30+)"),

    /** Balanced (15-20 instances): Good quality with reasonable instance count
     *  - 60% screen resolution with TSR upscaling
     *  - Hair strands at 2 MSAA samples
     *  - SSS at half resolution, quality 1
     *  - 1024px shadows
     *  - 30 FPS cap, 4Mbps encoder bitrate
     *  - VRS enabled (2x2 for stable)
     */
    Balanced        UMETA(DisplayName = "Balanced (15-20)"),

    /** Quality (8-12 instances): Higher visual quality with fewer instances
     *  - 75% screen resolution with TSR upscaling
     *  - Hair strands at 4 MSAA samples
     *  - Full resolution SSS, quality 2
     *  - 2048px shadows
     *  - 30 FPS cap, 6Mbps encoder bitrate
     *  - VRS disabled
     */
    Quality         UMETA(DisplayName = "Quality (8-12)"),

    /** Custom: Use individual settings, don't apply preset */
    Custom          UMETA(DisplayName = "Custom")
};

/**
 * Result of optimization application
 */
UENUM(BlueprintType)
enum class EOptimizationResult : uint8
{
    /** All optimizations applied successfully */
    Success         UMETA(DisplayName = "Success"),

    /** Optimizations deferred - will apply on next frame */
    Pending         UMETA(DisplayName = "Pending"),

    /** Some optimizations failed to apply */
    PartialSuccess  UMETA(DisplayName = "Partial Success"),

    /** Optimization failed */
    Failed          UMETA(DisplayName = "Failed")
};

/**
 * Detailed optimization settings structure for fine-grained control
 */
USTRUCT(BlueprintType)
struct FMultiInstanceSettings
{
    GENERATED_BODY()

    // ============================================
    // RESOLUTION & UPSCALING
    // ============================================

    /** Screen percentage (25-100). Lower = more instances. TSR upscales to full resolution. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resolution", meta = (ClampMin = "25", ClampMax = "100"))
    int32 ScreenPercentage = 50;

    /** Enable TSR (Temporal Super Resolution) for upscaling */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resolution")
    bool bEnableTSR = true;

    /** TSR history quality (1-3). Higher = better temporal stability but more cost */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resolution", meta = (ClampMin = "1", ClampMax = "3"))
    int32 TSRQuality = 1;

    // ============================================
    // VARIABLE RATE SHADING
    // ============================================

    /** Enable hardware VRS (Variable Rate Shading) on supported GPUs */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRS")
    bool bEnableVRS = true;

    /** Enable contrast-adaptive VRS (automatically reduces shading in low-contrast areas) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRS")
    bool bContrastAdaptiveVRS = true;

    // ============================================
    // METAHUMAN - HAIR
    // ============================================

    /** Enable hair strands rendering (very expensive - disable for max instances) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MetaHuman|Hair")
    bool bEnableHairStrands = true;

    /** Hair MSAA samples (1-8). Lower = cheaper. 1-2 for multi-instance. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MetaHuman|Hair", meta = (ClampMin = "1", ClampMax = "8"))
    int32 HairMSAASamples = 2;

    /** Enable hair deep shadows (expensive) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MetaHuman|Hair")
    bool bHairDeepShadows = false;

    /** Enable hair voxelization for volumetric effects (expensive) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MetaHuman|Hair")
    bool bHairVoxelization = false;

    // ============================================
    // METAHUMAN - SKIN/SSS
    // ============================================

    /** SSS (Subsurface Scattering) quality level (0-3). 0 = cheapest. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MetaHuman|Skin", meta = (ClampMin = "0", ClampMax = "3"))
    int32 SSSQuality = 0;

    /** Render SSS at half resolution (significant savings) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MetaHuman|Skin")
    bool bSSSHalfResolution = true;

    // ============================================
    // SHADOWS
    // ============================================

    /** Maximum shadow map resolution (256-4096). 512-1024 for multi-instance. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shadows", meta = (ClampMin = "256", ClampMax = "4096"))
    int32 ShadowResolution = 1024;

    /** Enable Virtual Shadow Maps (very expensive - disable for multi-instance) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shadows")
    bool bVirtualShadowMaps = false;

    /** Enable contact shadows (moderate cost) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shadows")
    bool bContactShadows = false;

    /** Maximum CSM (Cascaded Shadow Map) cascades (1-10). 2-4 for multi-instance. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shadows", meta = (ClampMin = "1", ClampMax = "10"))
    int32 CSMCascades = 3;

    // ============================================
    // GLOBAL ILLUMINATION
    // ============================================

    /** GI Method: 0=None, 1=SSGI, 2=Lumen. 0 for max instances. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Global Illumination", meta = (ClampMin = "0", ClampMax = "2"))
    int32 GIMethod = 0;

    /** Enable Lumen reflections (requires GIMethod=2) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Global Illumination")
    bool bLumenReflections = false;

    /** Enable hardware ray tracing for Lumen (very expensive) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Global Illumination")
    bool bHardwareRayTracing = false;

    // ============================================
    // POST PROCESSING
    // ============================================

    /** Enable volumetric fog (expensive) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Post Processing")
    bool bVolumetricFog = false;

    /** Enable motion blur */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Post Processing")
    bool bMotionBlur = false;

    /** Bloom quality (0-5). 0-2 for multi-instance. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Post Processing", meta = (ClampMin = "0", ClampMax = "5"))
    int32 BloomQuality = 2;

    /** Ambient occlusion levels (0-3). 0 for max instances. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Post Processing", meta = (ClampMin = "0", ClampMax = "3"))
    int32 AOLevels = 0;

    // ============================================
    // STREAMING & MEMORY
    // ============================================

    /** Texture streaming pool size in MB (256-4096). 512-1500 for multi-instance. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Memory", meta = (ClampMin = "256", ClampMax = "4096"))
    int32 StreamingPoolSizeMB = 1000;

    /** Maximum temporary memory for streaming in MB */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Memory", meta = (ClampMin = "32", ClampMax = "512"))
    int32 MaxTempMemoryMB = 128;

    /** Maximum anisotropic filtering (1-16). 4-8 for multi-instance. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Memory", meta = (ClampMin = "1", ClampMax = "16"))
    int32 MaxAnisotropy = 4;

    // ============================================
    // FRAME RATE & ENCODER
    // ============================================

    /** Maximum frame rate (0 = unlimited). 24-30 for multi-instance. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Performance", meta = (ClampMin = "0", ClampMax = "120"))
    int32 MaxFPS = 30;

    /** Target encoder bitrate in Mbps (1-20). 2-6 for multi-instance. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Encoder", meta = (ClampMin = "1", ClampMax = "20"))
    int32 EncoderBitrateMbps = 4;

    /** Minimum encoder QP (quality parameter). Higher = more compression. 18-28 typical. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Encoder", meta = (ClampMin = "10", ClampMax = "40"))
    int32 EncoderMinQP = 22;

    // ============================================
    // LOD & SKELETAL MESH
    // ============================================

    /** Skeletal mesh LOD bias. Positive values force lower (cheaper) LODs. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LOD", meta = (ClampMin = "0", ClampMax = "4"))
    int32 SkeletalMeshLODBias = 1;

    /** Force specific LOD level (-1 = automatic). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LOD", meta = (ClampMin = "-1", ClampMax = "7"))
    int32 ForceLOD = -1;

    /** Enable animation update rate optimization (skip updates when not visible) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LOD")
    bool bAnimationURO = true;
};

/**
 * Blueprint Function Library for multi-instance GPU optimization
 *
 * This class provides static functions callable from any Blueprint to apply
 * GPU optimizations for running multiple game instances with MetaHumans
 * on a single GPU for pixel streaming.
 *
 * CRITICAL: This is a STANDALONE utility. It does NOT auto-run.
 * You must call it explicitly from your Blueprint when you want optimizations applied.
 */
UCLASS()
class EMBODY_API UMultiInstanceOptimizer : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:

    // ============================================
    // PRIMARY OPTIMIZATION FUNCTIONS
    // ============================================

    /**
     * Apply multi-instance optimizations using a preset profile
     *
     * Call this from any Blueprint's BeginPlay or when you want to apply optimizations.
     * Safe to call during BeginPlay - will defer if rendering system not ready.
     *
     * @param Profile The optimization profile to apply
     * @return Result indicating success, pending, or failure
     */
    UFUNCTION(BlueprintCallable, Category = "Embody|Multi-Instance Optimization",
        meta = (DisplayName = "Apply Multi Instance Optimizations"))
    static EOptimizationResult ApplyMultiInstanceOptimizations(EMultiInstanceProfile Profile = EMultiInstanceProfile::Balanced);

    /**
     * Apply custom optimization settings
     *
     * For fine-grained control over individual settings.
     *
     * @param Settings Custom settings structure
     * @return Result indicating success, pending, or failure
     */
    UFUNCTION(BlueprintCallable, Category = "Embody|Multi-Instance Optimization",
        meta = (DisplayName = "Apply Custom Optimizations"))
    static EOptimizationResult ApplyCustomOptimizations(const FMultiInstanceSettings& Settings);

    /**
     * Reset all optimizations to default UE5 values
     *
     * Use this to restore normal rendering when not in multi-instance mode.
     */
    UFUNCTION(BlueprintCallable, Category = "Embody|Multi-Instance Optimization",
        meta = (DisplayName = "Reset To Default Rendering"))
    static void ResetToDefaultRendering();

    // ============================================
    // QUICK OPTIMIZATION FUNCTIONS
    // ============================================

    /**
     * Apply maximum SM (Streaming Multiprocessor) reduction
     *
     * Applies the most aggressive GPU optimizations for maximum instance count.
     * Use when you need 30+ instances regardless of quality.
     */
    UFUNCTION(BlueprintCallable, Category = "Embody|Multi-Instance Optimization|Quick",
        meta = (DisplayName = "Apply Maximum SM Reduction"))
    static void ApplyMaximumSMReduction();

    /**
     * Enable MetaHuman optimization mode
     *
     * Specifically targets MetaHuman-heavy scenes with appropriate hair,
     * skin, and skeletal mesh optimizations.
     */
    UFUNCTION(BlueprintCallable, Category = "Embody|Multi-Instance Optimization|Quick",
        meta = (DisplayName = "Enable MetaHuman Optimization"))
    static void EnableMetaHumanOptimization();

    /**
     * Configure pixel streaming encoder for multi-instance
     *
     * Sets encoder parameters optimal for multiple streams.
     *
     * @param BitrateMbps Target bitrate per stream (1-20)
     * @param MaxFPS Maximum frame rate (24-60)
     */
    UFUNCTION(BlueprintCallable, Category = "Embody|Multi-Instance Optimization|Quick",
        meta = (DisplayName = "Configure Pixel Streaming Encoder"))
    static void ConfigurePixelStreamingEncoder(int32 BitrateMbps = 4, int32 MaxFPS = 30);

    // ============================================
    // INDIVIDUAL SETTING FUNCTIONS
    // ============================================

    /**
     * Set screen percentage with TSR upscaling
     *
     * @param Percentage Screen percentage (25-100)
     * @param bEnableTSR Enable Temporal Super Resolution upscaling
     */
    UFUNCTION(BlueprintCallable, Category = "Embody|Multi-Instance Optimization|Settings",
        meta = (DisplayName = "Set Screen Percentage"))
    static void SetScreenPercentage(int32 Percentage, bool bEnableTSR = true);

    /**
     * Configure Variable Rate Shading
     *
     * @param bEnable Enable VRS
     * @param bContrastAdaptive Enable contrast-adaptive shading rate selection
     */
    UFUNCTION(BlueprintCallable, Category = "Embody|Multi-Instance Optimization|Settings",
        meta = (DisplayName = "Configure VRS"))
    static void ConfigureVRS(bool bEnable, bool bContrastAdaptive = true);

    /**
     * Configure MetaHuman hair rendering
     *
     * @param bEnable Enable hair strands (false = use cards)
     * @param MSAASamples MSAA samples for hair (1-8)
     */
    UFUNCTION(BlueprintCallable, Category = "Embody|Multi-Instance Optimization|Settings",
        meta = (DisplayName = "Configure Hair Rendering"))
    static void ConfigureHairRendering(bool bEnable, int32 MSAASamples = 2);

    /**
     * Configure shadow quality
     *
     * @param Resolution Shadow map resolution (256-4096)
     * @param bVirtualShadowMaps Enable VSM
     */
    UFUNCTION(BlueprintCallable, Category = "Embody|Multi-Instance Optimization|Settings",
        meta = (DisplayName = "Configure Shadows"))
    static void ConfigureShadows(int32 Resolution, bool bVirtualShadowMaps = false);

    /**
     * Set texture streaming pool size
     *
     * @param SizeMB Pool size in megabytes (256-4096)
     */
    UFUNCTION(BlueprintCallable, Category = "Embody|Multi-Instance Optimization|Settings",
        meta = (DisplayName = "Set Streaming Pool Size"))
    static void SetStreamingPoolSize(int32 SizeMB);

    // ============================================
    // QUERY FUNCTIONS
    // ============================================

    /**
     * Check if VRS hardware is available on the current GPU
     */
    UFUNCTION(BlueprintPure, Category = "Embody|Multi-Instance Optimization|Info",
        meta = (DisplayName = "Is VRS Supported"))
    static bool IsVRSSupported();

    /**
     * Get current screen percentage
     */
    UFUNCTION(BlueprintPure, Category = "Embody|Multi-Instance Optimization|Info",
        meta = (DisplayName = "Get Current Screen Percentage"))
    static int32 GetCurrentScreenPercentage();

    /**
     * Get estimated max instances for current settings
     *
     * Based on current GPU usage and settings, estimates how many
     * instances could run on the GPU.
     */
    UFUNCTION(BlueprintPure, Category = "Embody|Multi-Instance Optimization|Info",
        meta = (DisplayName = "Get Estimated Max Instances"))
    static int32 GetEstimatedMaxInstances();

    /**
     * Get the settings for a preset profile
     *
     * @param Profile The profile to get settings for
     * @return Settings structure for the profile
     */
    UFUNCTION(BlueprintPure, Category = "Embody|Multi-Instance Optimization|Info",
        meta = (DisplayName = "Get Profile Settings"))
    static FMultiInstanceSettings GetProfileSettings(EMultiInstanceProfile Profile);

private:

    // ============================================
    // INTERNAL HELPERS
    // ============================================

    /** Check if rendering system is ready for CVar modification */
    static bool IsRenderingSystemReady();

    /** Safe VRS hardware check with exception handling */
    static bool IsVRSHardwareAvailableSafe();

    /** Internal implementation of optimization application */
    static EOptimizationResult ApplyOptimizationsInternal(const FMultiInstanceSettings& Settings);

    /** Set a console variable safely */
    static bool SetCVar(const TCHAR* CVarName, int32 Value);
    static bool SetCVar(const TCHAR* CVarName, float Value);

    /** Get a console variable value */
    static int32 GetCVarInt(const TCHAR* CVarName);
    static float GetCVarFloat(const TCHAR* CVarName);

    /** Log category for optimization messages */
    static void LogOptimization(const FString& Message);
    static void LogWarning(const FString& Message);
    static void LogError(const FString& Message);
};

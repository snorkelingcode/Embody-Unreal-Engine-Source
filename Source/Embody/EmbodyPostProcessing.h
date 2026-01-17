// EmbodyPostProcessing.h
// Post Processing with REAL-TIME editable properties in Details Panel
// Includes Demo Mode to test all effects

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Components/PostProcessComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/Texture2D.h"
#include "EmbodyPostProcessing.generated.h"

// Screen overlay effect state
USTRUCT(BlueprintType)
struct FScreenOverlayEffect
{
	GENERATED_BODY()

	UPROPERTY()
	FName EffectName;

	UPROPERTY()
	UMaterialInstanceDynamic* Material = nullptr;

	UPROPERTY()
	float CurrentIntensity = 0.0f;

	UPROPERTY()
	float TargetIntensity = 0.0f;

	UPROPERTY()
	float FadeSpeed = 2.0f;

	UPROPERTY()
	FVector2D UVOffset = FVector2D::ZeroVector;

	UPROPERTY()
	FVector2D UVSpeed = FVector2D::ZeroVector;

	UPROPERTY()
	bool bPulse = false;

	UPROPERTY()
	float PulseSpeed = 1.0f;

	UPROPERTY()
	float PulsePhase = 0.0f;
};

// ============================================
// COLOR PRESETS - Simple, Working Effects
// ============================================

UENUM(BlueprintType)
enum class EEmbodyColorPreset : uint8
{
	Default			UMETA(DisplayName = "Default"),
	Cinematic		UMETA(DisplayName = "Cinematic"),
	Noir			UMETA(DisplayName = "Noir"),
	Vintage			UMETA(DisplayName = "Vintage"),
	Cyberpunk		UMETA(DisplayName = "Cyberpunk"),
	Horror			UMETA(DisplayName = "Horror"),
	Dream			UMETA(DisplayName = "Dream"),
	NightVision		UMETA(DisplayName = "Night Vision"),
	Thermal			UMETA(DisplayName = "Thermal"),
	Bleach			UMETA(DisplayName = "Bleach Bypass"),
	Sepia			UMETA(DisplayName = "Sepia"),
	Arctic			UMETA(DisplayName = "Arctic"),
	Sunset			UMETA(DisplayName = "Sunset"),
	Neon			UMETA(DisplayName = "Neon"),
	BleedingOut		UMETA(DisplayName = "Bleeding Out"),
	Drunk			UMETA(DisplayName = "Drunk"),
	Poisoned		UMETA(DisplayName = "Poisoned"),
	HighOnDrugs		UMETA(DisplayName = "High"),
	Electrocuted	UMETA(DisplayName = "Electrocuted"),
	GlitchCorruption UMETA(DisplayName = "Glitch"),
	VHS				UMETA(DisplayName = "VHS"),
	Retrowave		UMETA(DisplayName = "Retrowave")
};

// ============================================
// MAIN POST PROCESS COMPONENT
// ALL PROPERTIES EDITABLE IN DETAILS PANEL!
// ============================================

UCLASS(ClassGroup=(Embody), meta=(BlueprintSpawnableComponent, DisplayName="Embody Post Process"))
class EMBODY_API UEmbodyPostProcessingComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UEmbodyPostProcessingComponent();

	virtual void OnRegister() override;
	virtual void OnUnregister() override;
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	// ============================================
	// COLOR GRADING - Live Editable!
	// ============================================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embody PP|Color Grading", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float Saturation = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embody PP|Color Grading", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float Contrast = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embody PP|Color Grading", meta = (ClampMin = "0.5", ClampMax = "1.5"))
	float Gamma = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embody PP|Color Grading", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float Gain = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embody PP|Color Grading", meta = (ClampMin = "-1.0", ClampMax = "1.0"))
	float Offset = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embody PP|Color Grading", meta = (ClampMin = "-100.0", ClampMax = "100.0"))
	float Temperature = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embody PP|Color Grading", meta = (ClampMin = "-100.0", ClampMax = "100.0"))
	float Tint = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embody PP|Color Grading")
	FVector4 SaturationRGBA = FVector4(1, 1, 1, 1);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embody PP|Color Grading")
	FVector4 ContrastRGBA = FVector4(1, 1, 1, 1);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embody PP|Color Grading")
	FVector4 GammaRGBA = FVector4(1, 1, 1, 1);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embody PP|Color Grading")
	FVector4 GainRGBA = FVector4(1, 1, 1, 1);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embody PP|Color Grading")
	FVector4 OffsetRGBA = FVector4(0, 0, 0, 0);

	// ============================================
	// BLOOM - Live Editable!
	// ============================================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embody PP|Bloom", meta = (ClampMin = "0.0", ClampMax = "8.0"))
	float BloomIntensity = 0.675f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embody PP|Bloom", meta = (ClampMin = "-1.0", ClampMax = "8.0"))
	float BloomThreshold = -1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embody PP|Bloom", meta = (ClampMin = "1.0", ClampMax = "64.0"))
	float BloomSizeScale = 4.0f;

	// ============================================
	// VIGNETTE - Live Editable!
	// ============================================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embody PP|Vignette", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float VignetteIntensity = 0.4f;

	// ============================================
	// EFFECTS - Live Editable!
	// ============================================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embody PP|Effects", meta = (ClampMin = "0.0", ClampMax = "5.0"))
	float ChromaticAberration = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embody PP|Effects", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float FilmGrainIntensity = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embody PP|Effects", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Sharpen = 0.0f;

	// ============================================
	// DEPTH OF FIELD - Live Editable!
	// ============================================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embody PP|DOF", meta = (ClampMin = "0.0"))
	float FocalDistance = 500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embody PP|DOF", meta = (ClampMin = "0.7", ClampMax = "32.0"))
	float Aperture = 4.0f;

	// ============================================
	// EXPOSURE - Live Editable!
	// ============================================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embody PP|Exposure", meta = (ClampMin = "-15.0", ClampMax = "15.0"))
	float ExposureBias = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embody PP|Exposure", meta = (ClampMin = "-10.0", ClampMax = "20.0"))
	float AutoExposureMinBrightness = -10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embody PP|Exposure", meta = (ClampMin = "-10.0", ClampMax = "20.0"))
	float AutoExposureMaxBrightness = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embody PP|Exposure", meta = (ClampMin = "0.02", ClampMax = "20.0"))
	float AutoExposureSpeedUp = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embody PP|Exposure", meta = (ClampMin = "0.02", ClampMax = "20.0"))
	float AutoExposureSpeedDown = 1.0f;

	// ============================================
	// ADVANCED COLOR GRADING
	// ============================================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embody PP|Color Grading Advanced")
	FVector4 ColorSaturationShadows = FVector4(1, 1, 1, 1);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embody PP|Color Grading Advanced")
	FVector4 ColorSaturationMidtones = FVector4(1, 1, 1, 1);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embody PP|Color Grading Advanced")
	FVector4 ColorSaturationHighlights = FVector4(1, 1, 1, 1);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embody PP|Color Grading Advanced")
	FVector4 ColorContrastShadows = FVector4(1, 1, 1, 1);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embody PP|Color Grading Advanced")
	FVector4 ColorContrastMidtones = FVector4(1, 1, 1, 1);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embody PP|Color Grading Advanced")
	FVector4 ColorContrastHighlights = FVector4(1, 1, 1, 1);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embody PP|Color Grading Advanced")
	FVector4 ColorGammaShadows = FVector4(1, 1, 1, 1);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embody PP|Color Grading Advanced")
	FVector4 ColorGammaMidtones = FVector4(1, 1, 1, 1);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embody PP|Color Grading Advanced")
	FVector4 ColorGammaHighlights = FVector4(1, 1, 1, 1);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embody PP|Color Grading Advanced")
	FVector4 ColorGainShadows = FVector4(1, 1, 1, 1);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embody PP|Color Grading Advanced")
	FVector4 ColorGainMidtones = FVector4(1, 1, 1, 1);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embody PP|Color Grading Advanced")
	FVector4 ColorGainHighlights = FVector4(1, 1, 1, 1);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embody PP|Color Grading Advanced")
	FVector4 ColorOffsetShadows = FVector4(0, 0, 0, 0);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embody PP|Color Grading Advanced")
	FVector4 ColorOffsetMidtones = FVector4(0, 0, 0, 0);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embody PP|Color Grading Advanced")
	FVector4 ColorOffsetHighlights = FVector4(0, 0, 0, 0);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embody PP|Color Grading Advanced", meta = (ClampMin = "-1.0", ClampMax = "1.0"))
	float ColorCorrectionShadowsMax = 0.09f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embody PP|Color Grading Advanced", meta = (ClampMin = "-1.0", ClampMax = "1.0"))
	float ColorCorrectionHighlightsMin = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embody PP|Color Grading Advanced", meta = (ClampMin = "1.0", ClampMax = "10.0"))
	float ColorCorrectionHighlightsMax = 1.0f;

	// ============================================
	// TONEMAPPER
	// ============================================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embody PP|Tonemapper", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float FilmSlope = 0.88f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embody PP|Tonemapper", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float FilmToe = 0.55f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embody PP|Tonemapper", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float FilmShoulder = 0.26f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embody PP|Tonemapper", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float FilmBlackClip = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embody PP|Tonemapper", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float FilmWhiteClip = 0.04f;

	// ============================================
	// MOTION BLUR
	// ============================================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embody PP|Motion Blur", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MotionBlurAmount = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embody PP|Motion Blur", meta = (ClampMin = "0.0", ClampMax = "100.0"))
	float MotionBlurMax = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embody PP|Motion Blur", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MotionBlurPerObjectSize = 0.0f;

	// ============================================
	// AMBIENT OCCLUSION
	// ============================================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embody PP|Ambient Occlusion", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float AmbientOcclusionIntensity = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embody PP|Ambient Occlusion", meta = (ClampMin = "0.0", ClampMax = "500.0"))
	float AmbientOcclusionRadius = 200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embody PP|Ambient Occlusion", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float AmbientOcclusionStaticFraction = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embody PP|Ambient Occlusion", meta = (ClampMin = "0.1", ClampMax = "10.0"))
	float AmbientOcclusionFadeDistance = 8000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embody PP|Ambient Occlusion", meta = (ClampMin = "0.1", ClampMax = "10.0"))
	float AmbientOcclusionFadeRadius = 5000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embody PP|Ambient Occlusion", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float AmbientOcclusionPower = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embody PP|Ambient Occlusion", meta = (ClampMin = "0.0", ClampMax = "0.1"))
	float AmbientOcclusionBias = 0.03f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embody PP|Ambient Occlusion", meta = (ClampMin = "0.0", ClampMax = "4.0"))
	float AmbientOcclusionQuality = 50.0f;

	// ============================================
	// ADVANCED BLOOM
	// ============================================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embody PP|Bloom Advanced")
	FLinearColor Bloom1Tint = FLinearColor(0.3465f, 0.3465f, 0.3465f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embody PP|Bloom Advanced")
	FLinearColor Bloom2Tint = FLinearColor(0.138f, 0.138f, 0.138f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embody PP|Bloom Advanced")
	FLinearColor Bloom3Tint = FLinearColor(0.1176f, 0.1176f, 0.1176f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embody PP|Bloom Advanced")
	FLinearColor Bloom4Tint = FLinearColor(0.066f, 0.066f, 0.066f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embody PP|Bloom Advanced")
	FLinearColor Bloom5Tint = FLinearColor(0.066f, 0.066f, 0.066f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embody PP|Bloom Advanced")
	FLinearColor Bloom6Tint = FLinearColor(0.061f, 0.061f, 0.061f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embody PP|Bloom Advanced", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Bloom1Size = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embody PP|Bloom Advanced", meta = (ClampMin = "0.0", ClampMax = "4.0"))
	float Bloom2Size = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embody PP|Bloom Advanced", meta = (ClampMin = "0.0", ClampMax = "8.0"))
	float Bloom3Size = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embody PP|Bloom Advanced", meta = (ClampMin = "0.0", ClampMax = "16.0"))
	float Bloom4Size = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embody PP|Bloom Advanced", meta = (ClampMin = "0.0", ClampMax = "32.0"))
	float Bloom5Size = 30.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embody PP|Bloom Advanced", meta = (ClampMin = "0.0", ClampMax = "64.0"))
	float Bloom6Size = 64.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embody PP|Bloom Advanced", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float BloomConvolutionScatterDispersion = 1.0f;

	// ============================================
	// ADVANCED DOF
	// ============================================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embody PP|DOF Advanced", meta = (ClampMin = "0.0"))
	float DepthOfFieldSensorWidth = 24.576f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embody PP|DOF Advanced", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float DepthOfFieldDepthBlurAmount = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embody PP|DOF Advanced", meta = (ClampMin = "0.0"))
	float DepthOfFieldDepthBlurRadius = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embody PP|DOF Advanced", meta = (ClampMin = "0.0"))
	float DepthOfFieldFocalRegion = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embody PP|DOF Advanced", meta = (ClampMin = "0.0"))
	float DepthOfFieldNearTransitionRegion = 300.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embody PP|DOF Advanced", meta = (ClampMin = "0.0"))
	float DepthOfFieldFarTransitionRegion = 500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embody PP|DOF Advanced", meta = (ClampMin = "0.0", ClampMax = "4.0"))
	float DepthOfFieldNearBlurSize = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embody PP|DOF Advanced", meta = (ClampMin = "0.0", ClampMax = "4.0"))
	float DepthOfFieldFarBlurSize = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embody PP|DOF Advanced", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float DepthOfFieldOcclusion = 0.4f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embody PP|DOF Advanced", meta = (ClampMin = "0.0", ClampMax = "10.0"))
	float DepthOfFieldSkyFocusDistance = 0.0f;

	// ============================================
	// LENS FLARE
	// ============================================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embody PP|Lens Flare", meta = (ClampMin = "0.0", ClampMax = "10.0"))
	float LensFlareIntensity = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embody PP|Lens Flare")
	FLinearColor LensFlareTint = FLinearColor(1, 1, 1);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embody PP|Lens Flare", meta = (ClampMin = "0.0", ClampMax = "8.0"))
	float LensFlareThreshold = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embody PP|Lens Flare", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float LensFlareBokehSize = 3.0f;

	// ============================================
	// SCENE COLOR / IMAGE EFFECTS
	// ============================================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embody PP|Scene Color")
	FLinearColor SceneColorTint = FLinearColor(1, 1, 1, 1);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embody PP|Scene Color", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SceneFringeIntensity = 0.0f;

	// ============================================
	// LOCAL EXPOSURE
	// ============================================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embody PP|Local Exposure", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float LocalExposureHighlightContrastScale = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embody PP|Local Exposure", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float LocalExposureShadowContrastScale = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embody PP|Local Exposure", meta = (ClampMin = "0.01", ClampMax = "1.0"))
	float LocalExposureDetailStrength = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embody PP|Local Exposure", meta = (ClampMin = "1.0", ClampMax = "100.0"))
	float LocalExposureBlurredLuminanceBlend = 0.6f;

	// ============================================
	// ANIMATION - Live Editable!
	// ============================================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embody PP|Animation")
	bool bEnableAnimation = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embody PP|Animation", meta = (ClampMin = "0.1", ClampMax = "10.0", EditCondition = "bEnableAnimation"))
	float AnimationSpeed = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embody PP|Animation", meta = (ClampMin = "0.0", ClampMax = "1.0", EditCondition = "bEnableAnimation"))
	float PulseIntensity = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embody PP|Animation", meta = (ClampMin = "0.0", ClampMax = "1.0", EditCondition = "bEnableAnimation"))
	float FlickerIntensity = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embody PP|Animation", meta = (ClampMin = "0.0", ClampMax = "1.0", EditCondition = "bEnableAnimation"))
	float ColorCycleIntensity = 0.0f;

	// ============================================
	// CURRENT PRESET (for display)
	// ============================================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embody PP|Preset")
	EEmbodyColorPreset CurrentPreset = EEmbodyColorPreset::Default;

	// ============================================
	// DEMO MODE - Test All Effects!
	// ============================================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embody PP|Demo Mode")
	bool bDemoMode = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embody PP|Demo Mode", meta = (ClampMin = "1.0", ClampMax = "10.0", EditCondition = "bDemoMode"))
	float DemoEffectDuration = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embody PP|Demo Mode", meta = (EditCondition = "bDemoMode"))
	bool bDemoIncludeOverlays = true;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Embody PP|Demo Mode")
	FString CurrentDemoEffectName;

	// ============================================
	// BLUEPRINT CALLABLE - Apply Preset
	// ============================================

	UFUNCTION(BlueprintCallable, Category = "Embody PP|Presets", meta = (DisplayName = "Apply Color Preset"))
	void ApplyColorPreset(EEmbodyColorPreset Preset);

	UFUNCTION(BlueprintCallable, Category = "Embody PP|Presets")
	void ResetToDefault();

	// ============================================
	// DEMO MODE CONTROL
	// ============================================

	/** Start demo mode - cycles through ALL presets, prints name of each */
	UFUNCTION(BlueprintCallable, Category = "Embody PP|Demo Mode", meta = (DisplayName = "Start Effect Demo"))
	void StartDemoMode(float EffectDuration = 3.0f);

	UFUNCTION(BlueprintCallable, Category = "Embody PP|Demo Mode")
	void StopDemoMode();

	// ============================================
	// SIMPLE SETTER NODES (match frontend commands)
	// ============================================

	UFUNCTION(BlueprintCallable, Category = "Embody PP|Color", meta = (DisplayName = "Set Saturation"))
	void SetSaturation(float Value);

	UFUNCTION(BlueprintCallable, Category = "Embody PP|Color", meta = (DisplayName = "Set Contrast"))
	void SetContrast(float Value);

	UFUNCTION(BlueprintCallable, Category = "Embody PP|Color", meta = (DisplayName = "Set Gamma"))
	void SetGamma(float Value);

	UFUNCTION(BlueprintCallable, Category = "Embody PP|Color", meta = (DisplayName = "Set Gain"))
	void SetGain(float Value);

	UFUNCTION(BlueprintCallable, Category = "Embody PP|Color", meta = (DisplayName = "Set Temperature"))
	void SetTemperature(float Value);

	UFUNCTION(BlueprintCallable, Category = "Embody PP|Color", meta = (DisplayName = "Set Tint"))
	void SetTint(float Value);

	UFUNCTION(BlueprintCallable, Category = "Embody PP|Bloom", meta = (DisplayName = "Set Bloom Intensity"))
	void SetBloomIntensity(float Value);

	UFUNCTION(BlueprintCallable, Category = "Embody PP|Bloom", meta = (DisplayName = "Set Bloom Size"))
	void SetBloomSize(float Value);

	UFUNCTION(BlueprintCallable, Category = "Embody PP|Vignette", meta = (DisplayName = "Set Vignette"))
	void SetVignetteIntensity(float Value);

	UFUNCTION(BlueprintCallable, Category = "Embody PP|Effects", meta = (DisplayName = "Set Chromatic Aberration"))
	void SetChromaticAberration(float Value);

	UFUNCTION(BlueprintCallable, Category = "Embody PP|Effects", meta = (DisplayName = "Set Film Grain"))
	void SetFilmGrain(float Value);

	UFUNCTION(BlueprintCallable, Category = "Embody PP|Effects", meta = (DisplayName = "Set Sharpen"))
	void SetSharpen(float Value);

	UFUNCTION(BlueprintCallable, Category = "Embody PP|Exposure", meta = (DisplayName = "Set Exposure"))
	void SetExposure(float Value);

	UFUNCTION(BlueprintCallable, Category = "Embody PP|DOF", meta = (DisplayName = "Set Focus Distance"))
	void SetFocusDistance(float Value);

	UFUNCTION(BlueprintCallable, Category = "Embody PP|DOF", meta = (DisplayName = "Set Aperture"))
	void SetAperture(float Value);

	/** Focus on actor with animated pull */
	UFUNCTION(BlueprintCallable, Category = "Embody PP|DOF", meta = (DisplayName = "Focus On Actor"))
	void FocusOnActor(AActor* Target, float Duration = 0.5f);

	// ============================================
	// RGBA SETTERS (match CGG_ frontend commands)
	// ============================================

	UFUNCTION(BlueprintCallable, Category = "Embody PP|Color RGBA", meta = (DisplayName = "Set Saturation RGBA"))
	void SetSaturationRGBA(float R, float G, float B, float A = 1.0f);

	UFUNCTION(BlueprintCallable, Category = "Embody PP|Color RGBA", meta = (DisplayName = "Set Contrast RGBA"))
	void SetContrastRGBA(float R, float G, float B, float A = 1.0f);

	UFUNCTION(BlueprintCallable, Category = "Embody PP|Color RGBA", meta = (DisplayName = "Set Gamma RGBA"))
	void SetGammaRGBA(float R, float G, float B, float A = 1.0f);

	UFUNCTION(BlueprintCallable, Category = "Embody PP|Color RGBA", meta = (DisplayName = "Set Gain RGBA"))
	void SetGainRGBA(float R, float G, float B, float A = 1.0f);

	UFUNCTION(BlueprintCallable, Category = "Embody PP|Color RGBA", meta = (DisplayName = "Set Offset RGBA"))
	void SetOffsetRGBA(float R, float G, float B, float A = 0.0f);

	// ============================================
	// IMPACT EFFECTS
	// ============================================

	UFUNCTION(BlueprintCallable, Category = "Embody PP|Impacts", meta = (DisplayName = "Damage Flash"))
	void DamageFlash(FLinearColor Color = FLinearColor(1, 0, 0, 1), float Intensity = 1.0f);

	UFUNCTION(BlueprintCallable, Category = "Embody PP|Impacts", meta = (DisplayName = "Heal Flash"))
	void HealFlash(float Intensity = 1.0f);

	UFUNCTION(BlueprintCallable, Category = "Embody PP|Impacts", meta = (DisplayName = "Screen Flash"))
	void ScreenFlash(FLinearColor Color, float Duration = 0.2f);

	// ============================================
	// GETTERS
	// ============================================

	UFUNCTION(BlueprintPure, Category = "Embody PP")
	FString GetCurrentPresetName() const;

	UFUNCTION(BlueprintPure, Category = "Embody PP")
	bool IsDemoModeActive() const { return bDemoMode; }

	// ============================================
	// SCREEN OVERLAY TEXTURES
	// ============================================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embody PP|Textures|Blood")
	UTexture2D* Tex_BloodSplatter01;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embody PP|Textures|Blood")
	UTexture2D* Tex_BloodSplatter02;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embody PP|Textures|Blood")
	UTexture2D* Tex_BloodSplatter03;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embody PP|Textures|Blood")
	UTexture2D* Tex_BloodVignette;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embody PP|Textures|Blood")
	UTexture2D* Tex_BloodDrip;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embody PP|Textures|Blood")
	UTexture2D* Tex_Veins;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embody PP|Textures|Damage")
	UTexture2D* Tex_ScreenCrack01;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embody PP|Textures|Damage")
	UTexture2D* Tex_ScreenCrack02;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embody PP|Textures|Damage")
	UTexture2D* Tex_DamageIndicator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embody PP|Textures|Environment")
	UTexture2D* Tex_Frost01;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embody PP|Textures|Environment")
	UTexture2D* Tex_Frost02;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embody PP|Textures|Environment")
	UTexture2D* Tex_FrostNoise;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embody PP|Textures|Environment")
	UTexture2D* Tex_RainDrops;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embody PP|Textures|Environment")
	UTexture2D* Tex_RainStreaks;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embody PP|Textures|Environment")
	UTexture2D* Tex_HeatNoise;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embody PP|Textures|Environment")
	UTexture2D* Tex_Caustics;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embody PP|Textures|Environment")
	UTexture2D* Tex_Condensation;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embody PP|Textures|Tech")
	UTexture2D* Tex_VHSNoise;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embody PP|Textures|Tech")
	UTexture2D* Tex_GlitchBlocks;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embody PP|Textures|Tech")
	UTexture2D* Tex_GlitchLines;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embody PP|Textures|Tech")
	UTexture2D* Tex_EMP;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embody PP|Textures|Tech")
	UTexture2D* Tex_Electric;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embody PP|Textures|Tech")
	UTexture2D* Tex_Hologram;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embody PP|Textures|Status")
	UTexture2D* Tex_Poison;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embody PP|Textures|Status")
	UTexture2D* Tex_Burn;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embody PP|Textures|Status")
	UTexture2D* Tex_Freeze;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embody PP|Textures|Camera")
	UTexture2D* Tex_LensDirt;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embody PP|Textures|Camera")
	UTexture2D* Tex_LightLeak01;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embody PP|Textures|Camera")
	UTexture2D* Tex_LightLeak02;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embody PP|Textures|Psychedelic")
	UTexture2D* Tex_Spiral;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embody PP|Textures|Psychedelic")
	UTexture2D* Tex_AcidPattern;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embody PP|Textures|Psychedelic")
	UTexture2D* Tex_Kaleidoscope;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embody PP|Textures|Psychedelic")
	UTexture2D* Tex_ColorAberration;

	// Base material for overlays (assign M_PP_Overlay)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Embody PP|Materials")
	UMaterialInterface* OverlayBaseMaterial;

	// ============================================
	// SCREEN OVERLAY EFFECTS - Animated Textures!
	// ============================================

	/** Play blood splatter on screen */
	UFUNCTION(BlueprintCallable, Category = "Embody PP|Screen Effects", meta = (DisplayName = "Play Blood Splatter"))
	void PlayBloodSplatter(int32 SplatterIndex = 1, float Intensity = 1.0f, float FadeTime = 3.0f);

	/** Start pulsing blood vignette (low health) */
	UFUNCTION(BlueprintCallable, Category = "Embody PP|Screen Effects", meta = (DisplayName = "Start Blood Vignette"))
	void StartBloodVignette(float Intensity = 0.5f, bool bPulseWithHeartbeat = true);

	UFUNCTION(BlueprintCallable, Category = "Embody PP|Screen Effects")
	void StopBloodVignette(float FadeTime = 1.0f);

	/** Start blood dripping down screen */
	UFUNCTION(BlueprintCallable, Category = "Embody PP|Screen Effects", meta = (DisplayName = "Start Blood Drip"))
	void StartBloodDrip(float Intensity = 0.7f, float DripSpeed = 0.3f);

	UFUNCTION(BlueprintCallable, Category = "Embody PP|Screen Effects")
	void StopBloodDrip(float FadeTime = 2.0f);

	/** Crack the screen */
	UFUNCTION(BlueprintCallable, Category = "Embody PP|Screen Effects", meta = (DisplayName = "Crack Screen"))
	void CrackScreen(int32 Pattern = 1, float Intensity = 1.0f);

	UFUNCTION(BlueprintCallable, Category = "Embody PP|Screen Effects")
	void HealCracks(float FadeTime = 2.0f);

	/** Start frost effect */
	UFUNCTION(BlueprintCallable, Category = "Embody PP|Screen Effects", meta = (DisplayName = "Start Frost"))
	void StartFrost(float Intensity = 0.5f, float BuildSpeed = 0.3f);

	UFUNCTION(BlueprintCallable, Category = "Embody PP|Screen Effects")
	void StopFrost(float MeltSpeed = 0.5f);

	/** Start rain on screen */
	UFUNCTION(BlueprintCallable, Category = "Embody PP|Screen Effects", meta = (DisplayName = "Start Rain"))
	void StartRain(float Intensity = 0.7f, float DropSpeed = 0.5f);

	UFUNCTION(BlueprintCallable, Category = "Embody PP|Screen Effects")
	void StopRain(float FadeTime = 1.0f);

	/** Start VHS effect */
	UFUNCTION(BlueprintCallable, Category = "Embody PP|Screen Effects", meta = (DisplayName = "Start VHS"))
	void StartVHS(float Intensity = 0.7f);

	UFUNCTION(BlueprintCallable, Category = "Embody PP|Screen Effects")
	void StopVHS(float FadeTime = 0.3f);

	/** Start glitch effect */
	UFUNCTION(BlueprintCallable, Category = "Embody PP|Screen Effects", meta = (DisplayName = "Start Glitch"))
	void StartGlitch(float Intensity = 0.5f);

	UFUNCTION(BlueprintCallable, Category = "Embody PP|Screen Effects")
	void StopGlitch(float FadeTime = 0.2f);

	/** Play EMP burst */
	UFUNCTION(BlueprintCallable, Category = "Embody PP|Screen Effects", meta = (DisplayName = "Play EMP"))
	void PlayEMP(float Intensity = 1.0f, float Duration = 0.5f);

	/** Start electric shock */
	UFUNCTION(BlueprintCallable, Category = "Embody PP|Screen Effects", meta = (DisplayName = "Start Electric"))
	void StartElectric(float Intensity = 0.8f);

	UFUNCTION(BlueprintCallable, Category = "Embody PP|Screen Effects")
	void StopElectric(float FadeTime = 0.3f);

	/** Start poison effect */
	UFUNCTION(BlueprintCallable, Category = "Embody PP|Screen Effects", meta = (DisplayName = "Start Poison"))
	void StartPoison(float Intensity = 0.6f);

	UFUNCTION(BlueprintCallable, Category = "Embody PP|Screen Effects")
	void StopPoison(float FadeTime = 1.0f);

	/** Start burn effect */
	UFUNCTION(BlueprintCallable, Category = "Embody PP|Screen Effects", meta = (DisplayName = "Start Burn"))
	void StartBurn(float Intensity = 0.7f);

	UFUNCTION(BlueprintCallable, Category = "Embody PP|Screen Effects")
	void StopBurn(float FadeTime = 1.0f);

	/** Start freeze frame */
	UFUNCTION(BlueprintCallable, Category = "Embody PP|Screen Effects", meta = (DisplayName = "Start Freeze"))
	void StartFreeze(float Intensity = 0.8f);

	UFUNCTION(BlueprintCallable, Category = "Embody PP|Screen Effects")
	void StopFreeze(float FadeTime = 1.5f);

	/** Set lens dirt intensity */
	UFUNCTION(BlueprintCallable, Category = "Embody PP|Screen Effects", meta = (DisplayName = "Set Lens Dirt"))
	void SetLensDirt(float Intensity = 0.3f);

	/** Start light leak */
	UFUNCTION(BlueprintCallable, Category = "Embody PP|Screen Effects", meta = (DisplayName = "Start Light Leak"))
	void StartLightLeak(int32 LeakIndex = 1, float Intensity = 0.5f);

	UFUNCTION(BlueprintCallable, Category = "Embody PP|Screen Effects")
	void StopLightLeak(float FadeTime = 1.0f);

	/** Start psychedelic effect */
	UFUNCTION(BlueprintCallable, Category = "Embody PP|Screen Effects", meta = (DisplayName = "Start Psychedelic"))
	void StartPsychedelic(float Intensity = 0.7f, float Speed = 1.0f);

	UFUNCTION(BlueprintCallable, Category = "Embody PP|Screen Effects")
	void StopPsychedelic(float FadeTime = 2.0f);

	/** Play any texture overlay */
	UFUNCTION(BlueprintCallable, Category = "Embody PP|Screen Effects", meta = (DisplayName = "Play Texture Overlay"))
	void PlayTextureOverlay(UTexture2D* Texture, FName EffectName, float Intensity = 1.0f, float FadeInTime = 0.2f, float Duration = 0.0f);

	/** Stop specific overlay by name */
	UFUNCTION(BlueprintCallable, Category = "Embody PP|Screen Effects")
	void StopOverlay(FName EffectName, float FadeTime = 0.5f);

	/** Stop all overlay effects */
	UFUNCTION(BlueprintCallable, Category = "Embody PP|Screen Effects")
	void StopAllOverlays(float FadeTime = 1.0f);

	/** Auto-load textures from Content/Textures/PostProcess */
	UFUNCTION(BlueprintCallable, Category = "Embody PP|Setup")
	void AutoLoadTextures();

protected:
	UPROPERTY()
	UPostProcessComponent* PostProcessComp;

	// Animation time
	float MasterTime;

	// Demo mode state
	int32 DemoIndex;
	float DemoTimer;

	// Focus pull state
	TWeakObjectPtr<AActor> FocusTarget;
	float FocusPullTimer;
	float FocusPullDuration;
	float FocusStartDistance;

	// Flash state
	float FlashTimer;
	float FlashDuration;
	FLinearColor FlashColor;

	// Overlay effects
	UPROPERTY()
	TArray<FScreenOverlayEffect> ActiveOverlays;

	UPROPERTY()
	UMaterial* GeneratedOverlayMaterial;

	void InitializePostProcess();
	void ApplyAllSettings();
	void UpdateAnimation(float DeltaTime);
	void UpdateDemoMode(float DeltaTime);
	void UpdateFlash(float DeltaTime);
	void UpdateFocusPull(float DeltaTime);
	void UpdateOverlays(float DeltaTime);

	FString GetPresetNameString(EEmbodyColorPreset Preset) const;

	// Overlay helpers
	void CreateOverlayMaterial();
	FScreenOverlayEffect* FindOverlay(FName EffectName);
	FScreenOverlayEffect& GetOrCreateOverlay(FName EffectName, UTexture2D* Texture);
	void RemoveOverlay(FName EffectName);
	UMaterialInstanceDynamic* CreateOverlayMID(UTexture2D* Texture);
};

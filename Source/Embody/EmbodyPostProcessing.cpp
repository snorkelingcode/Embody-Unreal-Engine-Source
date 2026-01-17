// EmbodyPostProcessing.cpp
// Post Processing with Real-Time Editor Viewport Updates

#include "EmbodyPostProcessing.h"
#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "Engine/Engine.h"

UEmbodyPostProcessingComponent::UEmbodyPostProcessingComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
	bTickInEditor = true; // Important for editor updates!

	MasterTime = 0.0f;
	DemoIndex = 0;
	DemoTimer = 0.0f;
	FocusPullTimer = 0.0f;
	FocusPullDuration = 0.0f;
	FocusStartDistance = 0.0f;
	FlashTimer = 0.0f;
	FlashDuration = 0.0f;
	FlashColor = FLinearColor::White;
	PostProcessComp = nullptr;
}

void UEmbodyPostProcessingComponent::OnRegister()
{
	Super::OnRegister();

	// Create PostProcessComponent here so it exists in editor!
	if (!PostProcessComp && GetOwner())
	{
		PostProcessComp = NewObject<UPostProcessComponent>(GetOwner(), TEXT("EmbodyPP"));
		if (PostProcessComp)
		{
			PostProcessComp->RegisterComponent();
			PostProcessComp->AttachToComponent(GetOwner()->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
			PostProcessComp->bUnbound = true;
			PostProcessComp->Priority = 100.0f;

			// ============================================
			// Enable ALL overrides for full control
			// ============================================

			// Color Grading - Basic
			PostProcessComp->Settings.bOverride_ColorSaturation = true;
			PostProcessComp->Settings.bOverride_ColorContrast = true;
			PostProcessComp->Settings.bOverride_ColorGamma = true;
			PostProcessComp->Settings.bOverride_ColorGain = true;
			PostProcessComp->Settings.bOverride_ColorOffset = true;
			PostProcessComp->Settings.bOverride_WhiteTemp = true;
			PostProcessComp->Settings.bOverride_WhiteTint = true;

			// Color Grading - Advanced
			PostProcessComp->Settings.bOverride_ColorSaturationShadows = true;
			PostProcessComp->Settings.bOverride_ColorSaturationMidtones = true;
			PostProcessComp->Settings.bOverride_ColorSaturationHighlights = true;
			PostProcessComp->Settings.bOverride_ColorContrastShadows = true;
			PostProcessComp->Settings.bOverride_ColorContrastMidtones = true;
			PostProcessComp->Settings.bOverride_ColorContrastHighlights = true;
			PostProcessComp->Settings.bOverride_ColorGammaShadows = true;
			PostProcessComp->Settings.bOverride_ColorGammaMidtones = true;
			PostProcessComp->Settings.bOverride_ColorGammaHighlights = true;
			PostProcessComp->Settings.bOverride_ColorGainShadows = true;
			PostProcessComp->Settings.bOverride_ColorGainMidtones = true;
			PostProcessComp->Settings.bOverride_ColorGainHighlights = true;
			PostProcessComp->Settings.bOverride_ColorOffsetShadows = true;
			PostProcessComp->Settings.bOverride_ColorOffsetMidtones = true;
			PostProcessComp->Settings.bOverride_ColorOffsetHighlights = true;
			PostProcessComp->Settings.bOverride_ColorCorrectionShadowsMax = true;
			PostProcessComp->Settings.bOverride_ColorCorrectionHighlightsMin = true;
			PostProcessComp->Settings.bOverride_ColorCorrectionHighlightsMax = true;

			// Tonemapper
			PostProcessComp->Settings.bOverride_FilmSlope = true;
			PostProcessComp->Settings.bOverride_FilmToe = true;
			PostProcessComp->Settings.bOverride_FilmShoulder = true;
			PostProcessComp->Settings.bOverride_FilmBlackClip = true;
			PostProcessComp->Settings.bOverride_FilmWhiteClip = true;

			// Bloom
			PostProcessComp->Settings.bOverride_BloomIntensity = true;
			PostProcessComp->Settings.bOverride_BloomThreshold = true;
			PostProcessComp->Settings.bOverride_BloomSizeScale = true;
			PostProcessComp->Settings.bOverride_Bloom1Tint = true;
			PostProcessComp->Settings.bOverride_Bloom2Tint = true;
			PostProcessComp->Settings.bOverride_Bloom3Tint = true;
			PostProcessComp->Settings.bOverride_Bloom4Tint = true;
			PostProcessComp->Settings.bOverride_Bloom5Tint = true;
			PostProcessComp->Settings.bOverride_Bloom6Tint = true;
			PostProcessComp->Settings.bOverride_Bloom1Size = true;
			PostProcessComp->Settings.bOverride_Bloom2Size = true;
			PostProcessComp->Settings.bOverride_Bloom3Size = true;
			PostProcessComp->Settings.bOverride_Bloom4Size = true;
			PostProcessComp->Settings.bOverride_Bloom5Size = true;
			PostProcessComp->Settings.bOverride_Bloom6Size = true;
			PostProcessComp->Settings.bOverride_BloomConvolutionScatterDispersion = true;

			// Vignette & Effects
			PostProcessComp->Settings.bOverride_VignetteIntensity = true;
			PostProcessComp->Settings.bOverride_SceneFringeIntensity = true;
			PostProcessComp->Settings.bOverride_FilmGrainIntensity = true;
			PostProcessComp->Settings.bOverride_Sharpen = true;
			PostProcessComp->Settings.bOverride_SceneColorTint = true;

			// Depth of Field
			PostProcessComp->Settings.bOverride_DepthOfFieldFstop = true;
			PostProcessComp->Settings.bOverride_DepthOfFieldFocalDistance = true;
			PostProcessComp->Settings.bOverride_DepthOfFieldSensorWidth = true;
			PostProcessComp->Settings.bOverride_DepthOfFieldDepthBlurAmount = true;
			PostProcessComp->Settings.bOverride_DepthOfFieldDepthBlurRadius = true;
			PostProcessComp->Settings.bOverride_DepthOfFieldFocalRegion = true;
			PostProcessComp->Settings.bOverride_DepthOfFieldNearTransitionRegion = true;
			PostProcessComp->Settings.bOverride_DepthOfFieldFarTransitionRegion = true;
			PostProcessComp->Settings.bOverride_DepthOfFieldNearBlurSize = true;
			PostProcessComp->Settings.bOverride_DepthOfFieldFarBlurSize = true;
			PostProcessComp->Settings.bOverride_DepthOfFieldOcclusion = true;
			PostProcessComp->Settings.bOverride_DepthOfFieldSkyFocusDistance = true;

			// Exposure
			PostProcessComp->Settings.bOverride_AutoExposureBias = true;
			PostProcessComp->Settings.bOverride_AutoExposureMinBrightness = true;
			PostProcessComp->Settings.bOverride_AutoExposureMaxBrightness = true;
			PostProcessComp->Settings.bOverride_AutoExposureSpeedUp = true;
			PostProcessComp->Settings.bOverride_AutoExposureSpeedDown = true;

			// Local Exposure
			PostProcessComp->Settings.bOverride_LocalExposureHighlightContrastScale = true;
			PostProcessComp->Settings.bOverride_LocalExposureShadowContrastScale = true;
			PostProcessComp->Settings.bOverride_LocalExposureDetailStrength = true;
			PostProcessComp->Settings.bOverride_LocalExposureBlurredLuminanceBlend = true;

			// Motion Blur
			PostProcessComp->Settings.bOverride_MotionBlurAmount = true;
			PostProcessComp->Settings.bOverride_MotionBlurMax = true;
			PostProcessComp->Settings.bOverride_MotionBlurPerObjectSize = true;

			// Ambient Occlusion
			PostProcessComp->Settings.bOverride_AmbientOcclusionIntensity = true;
			PostProcessComp->Settings.bOverride_AmbientOcclusionRadius = true;
			PostProcessComp->Settings.bOverride_AmbientOcclusionStaticFraction = true;
			PostProcessComp->Settings.bOverride_AmbientOcclusionFadeDistance = true;
			PostProcessComp->Settings.bOverride_AmbientOcclusionFadeRadius = true;
			PostProcessComp->Settings.bOverride_AmbientOcclusionPower = true;
			PostProcessComp->Settings.bOverride_AmbientOcclusionBias = true;
			PostProcessComp->Settings.bOverride_AmbientOcclusionQuality = true;

			// Lens Flare
			PostProcessComp->Settings.bOverride_LensFlareIntensity = true;
			PostProcessComp->Settings.bOverride_LensFlareTint = true;
			PostProcessComp->Settings.bOverride_LensFlareThreshold = true;
			PostProcessComp->Settings.bOverride_LensFlareBokehSize = true;

			ApplyAllSettings();
		}
	}
}

void UEmbodyPostProcessingComponent::OnUnregister()
{
	if (PostProcessComp)
	{
		PostProcessComp->DestroyComponent();
		PostProcessComp = nullptr;
	}
	Super::OnUnregister();
}

void UEmbodyPostProcessingComponent::BeginPlay()
{
	Super::BeginPlay();
	// PostProcessComp already created in OnRegister
	ApplyAllSettings();
}

void UEmbodyPostProcessingComponent::InitializePostProcess()
{
	// Kept for compatibility but OnRegister handles this now
	if (PostProcessComp)
	{
		ApplyAllSettings();
	}
}

void UEmbodyPostProcessingComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Early exit - only process if we have active effects that need per-frame updates
	bool bNeedsUpdate = bEnableAnimation || bDemoMode || FlashTimer > 0.0f ||
	                    FocusPullDuration > 0.0f || ActiveOverlays.Num() > 0;

	if (!bNeedsUpdate)
	{
		// ZERO per-frame overhead when no active effects
		return;
	}

	MasterTime += DeltaTime;

	UpdateDemoMode(DeltaTime);
	UpdateAnimation(DeltaTime);
	UpdateFlash(DeltaTime);
	UpdateFocusPull(DeltaTime);
	UpdateOverlays(DeltaTime);
	ApplyAllSettings();
}

#if WITH_EDITOR
void UEmbodyPostProcessingComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.Property)
	{
		FName PropName = PropertyChangedEvent.Property->GetFName();
		if (PropName == GET_MEMBER_NAME_CHECKED(UEmbodyPostProcessingComponent, CurrentPreset))
		{
			ApplyColorPreset(CurrentPreset);
		}
	}

	ApplyAllSettings();
}
#endif

void UEmbodyPostProcessingComponent::ApplyAllSettings()
{
	if (!PostProcessComp) return;

	// ============================================
	// COLOR GRADING - Basic
	// ============================================
	PostProcessComp->Settings.ColorSaturation = SaturationRGBA * Saturation;
	PostProcessComp->Settings.ColorContrast = ContrastRGBA * Contrast;
	PostProcessComp->Settings.ColorGamma = GammaRGBA * Gamma;
	PostProcessComp->Settings.ColorGain = GainRGBA * Gain;
	PostProcessComp->Settings.ColorOffset = OffsetRGBA + FVector4(Offset, Offset, Offset, 0);

	// Temperature and tint
	PostProcessComp->Settings.WhiteTemp = 6500.0f + Temperature * 30.0f;
	PostProcessComp->Settings.WhiteTint = Tint;

	// ============================================
	// COLOR GRADING - Advanced (Shadows/Midtones/Highlights)
	// ============================================
	PostProcessComp->Settings.ColorSaturationShadows = ColorSaturationShadows;
	PostProcessComp->Settings.ColorSaturationMidtones = ColorSaturationMidtones;
	PostProcessComp->Settings.ColorSaturationHighlights = ColorSaturationHighlights;
	PostProcessComp->Settings.ColorContrastShadows = ColorContrastShadows;
	PostProcessComp->Settings.ColorContrastMidtones = ColorContrastMidtones;
	PostProcessComp->Settings.ColorContrastHighlights = ColorContrastHighlights;
	PostProcessComp->Settings.ColorGammaShadows = ColorGammaShadows;
	PostProcessComp->Settings.ColorGammaMidtones = ColorGammaMidtones;
	PostProcessComp->Settings.ColorGammaHighlights = ColorGammaHighlights;
	PostProcessComp->Settings.ColorGainShadows = ColorGainShadows;
	PostProcessComp->Settings.ColorGainMidtones = ColorGainMidtones;
	PostProcessComp->Settings.ColorGainHighlights = ColorGainHighlights;
	PostProcessComp->Settings.ColorOffsetShadows = ColorOffsetShadows;
	PostProcessComp->Settings.ColorOffsetMidtones = ColorOffsetMidtones;
	PostProcessComp->Settings.ColorOffsetHighlights = ColorOffsetHighlights;
	PostProcessComp->Settings.ColorCorrectionShadowsMax = ColorCorrectionShadowsMax;
	PostProcessComp->Settings.ColorCorrectionHighlightsMin = ColorCorrectionHighlightsMin;
	PostProcessComp->Settings.ColorCorrectionHighlightsMax = ColorCorrectionHighlightsMax;

	// ============================================
	// TONEMAPPER
	// ============================================
	PostProcessComp->Settings.FilmSlope = FilmSlope;
	PostProcessComp->Settings.FilmToe = FilmToe;
	PostProcessComp->Settings.FilmShoulder = FilmShoulder;
	PostProcessComp->Settings.FilmBlackClip = FilmBlackClip;
	PostProcessComp->Settings.FilmWhiteClip = FilmWhiteClip;

	// ============================================
	// BLOOM
	// ============================================
	PostProcessComp->Settings.BloomIntensity = BloomIntensity;
	PostProcessComp->Settings.BloomThreshold = BloomThreshold;
	PostProcessComp->Settings.BloomSizeScale = BloomSizeScale;
	PostProcessComp->Settings.Bloom1Tint = Bloom1Tint;
	PostProcessComp->Settings.Bloom2Tint = Bloom2Tint;
	PostProcessComp->Settings.Bloom3Tint = Bloom3Tint;
	PostProcessComp->Settings.Bloom4Tint = Bloom4Tint;
	PostProcessComp->Settings.Bloom5Tint = Bloom5Tint;
	PostProcessComp->Settings.Bloom6Tint = Bloom6Tint;
	PostProcessComp->Settings.Bloom1Size = Bloom1Size;
	PostProcessComp->Settings.Bloom2Size = Bloom2Size;
	PostProcessComp->Settings.Bloom3Size = Bloom3Size;
	PostProcessComp->Settings.Bloom4Size = Bloom4Size;
	PostProcessComp->Settings.Bloom5Size = Bloom5Size;
	PostProcessComp->Settings.Bloom6Size = Bloom6Size;
	PostProcessComp->Settings.BloomConvolutionScatterDispersion = BloomConvolutionScatterDispersion;

	// ============================================
	// VIGNETTE
	// ============================================
	PostProcessComp->Settings.VignetteIntensity = VignetteIntensity;

	// ============================================
	// EFFECTS
	// ============================================
	PostProcessComp->Settings.SceneFringeIntensity = ChromaticAberration;
	PostProcessComp->Settings.FilmGrainIntensity = FilmGrainIntensity;
	PostProcessComp->Settings.Sharpen = Sharpen;
	PostProcessComp->Settings.SceneColorTint = SceneColorTint;

	// ============================================
	// DEPTH OF FIELD
	// ============================================
	PostProcessComp->Settings.DepthOfFieldFocalDistance = FocalDistance;
	PostProcessComp->Settings.DepthOfFieldFstop = Aperture;
	PostProcessComp->Settings.DepthOfFieldSensorWidth = DepthOfFieldSensorWidth;
	PostProcessComp->Settings.DepthOfFieldDepthBlurAmount = DepthOfFieldDepthBlurAmount;
	PostProcessComp->Settings.DepthOfFieldDepthBlurRadius = DepthOfFieldDepthBlurRadius;
	PostProcessComp->Settings.DepthOfFieldFocalRegion = DepthOfFieldFocalRegion;
	PostProcessComp->Settings.DepthOfFieldNearTransitionRegion = DepthOfFieldNearTransitionRegion;
	PostProcessComp->Settings.DepthOfFieldFarTransitionRegion = DepthOfFieldFarTransitionRegion;
	PostProcessComp->Settings.DepthOfFieldNearBlurSize = DepthOfFieldNearBlurSize;
	PostProcessComp->Settings.DepthOfFieldFarBlurSize = DepthOfFieldFarBlurSize;
	PostProcessComp->Settings.DepthOfFieldOcclusion = DepthOfFieldOcclusion;
	PostProcessComp->Settings.DepthOfFieldSkyFocusDistance = DepthOfFieldSkyFocusDistance;

	// ============================================
	// EXPOSURE
	// ============================================
	PostProcessComp->Settings.AutoExposureBias = ExposureBias;
	PostProcessComp->Settings.AutoExposureMinBrightness = AutoExposureMinBrightness;
	PostProcessComp->Settings.AutoExposureMaxBrightness = AutoExposureMaxBrightness;
	PostProcessComp->Settings.AutoExposureSpeedUp = AutoExposureSpeedUp;
	PostProcessComp->Settings.AutoExposureSpeedDown = AutoExposureSpeedDown;

	// ============================================
	// LOCAL EXPOSURE
	// ============================================
	PostProcessComp->Settings.LocalExposureHighlightContrastScale = LocalExposureHighlightContrastScale;
	PostProcessComp->Settings.LocalExposureShadowContrastScale = LocalExposureShadowContrastScale;
	PostProcessComp->Settings.LocalExposureDetailStrength = LocalExposureDetailStrength;
	PostProcessComp->Settings.LocalExposureBlurredLuminanceBlend = LocalExposureBlurredLuminanceBlend;

	// ============================================
	// MOTION BLUR
	// ============================================
	PostProcessComp->Settings.MotionBlurAmount = MotionBlurAmount;
	PostProcessComp->Settings.MotionBlurMax = MotionBlurMax;
	PostProcessComp->Settings.MotionBlurPerObjectSize = MotionBlurPerObjectSize;

	// ============================================
	// AMBIENT OCCLUSION
	// ============================================
	PostProcessComp->Settings.AmbientOcclusionIntensity = AmbientOcclusionIntensity;
	PostProcessComp->Settings.AmbientOcclusionRadius = AmbientOcclusionRadius;
	PostProcessComp->Settings.AmbientOcclusionStaticFraction = AmbientOcclusionStaticFraction;
	PostProcessComp->Settings.AmbientOcclusionFadeDistance = AmbientOcclusionFadeDistance;
	PostProcessComp->Settings.AmbientOcclusionFadeRadius = AmbientOcclusionFadeRadius;
	PostProcessComp->Settings.AmbientOcclusionPower = AmbientOcclusionPower;
	PostProcessComp->Settings.AmbientOcclusionBias = AmbientOcclusionBias;
	PostProcessComp->Settings.AmbientOcclusionQuality = AmbientOcclusionQuality;

	// ============================================
	// LENS FLARE
	// ============================================
	PostProcessComp->Settings.LensFlareIntensity = LensFlareIntensity;
	PostProcessComp->Settings.LensFlareTint = LensFlareTint;
	PostProcessComp->Settings.LensFlareThreshold = LensFlareThreshold;
	PostProcessComp->Settings.LensFlareBokehSize = LensFlareBokehSize;
}

void UEmbodyPostProcessingComponent::UpdateAnimation(float DeltaTime)
{
	if (!bEnableAnimation) return;

	float time = MasterTime * AnimationSpeed;

	// Pulse effect on vignette
	if (PulseIntensity > 0.0f)
	{
		float pulse = FMath::Sin(time * 3.0f) * 0.5f + 0.5f;
		VignetteIntensity = 0.3f + pulse * PulseIntensity * 0.4f;
	}

	// Flicker effect on gain
	if (FlickerIntensity > 0.0f)
	{
		if (FMath::FRand() < 0.1f * FlickerIntensity)
		{
			Gain = FMath::FRandRange(0.8f, 1.2f);
		}
		else
		{
			Gain = FMath::FInterpTo(Gain, 1.0f, DeltaTime, 5.0f);
		}
	}

	// Color cycle
	if (ColorCycleIntensity > 0.0f)
	{
		float hue = FMath::Fmod(time * 0.1f, 1.0f);
		FLinearColor cycleColor = FLinearColor::MakeFromHSV8((uint8)(hue * 255), 128, 255);
		GainRGBA = FVector4(
			1.0f + (cycleColor.R - 0.5f) * ColorCycleIntensity * 0.3f,
			1.0f + (cycleColor.G - 0.5f) * ColorCycleIntensity * 0.3f,
			1.0f + (cycleColor.B - 0.5f) * ColorCycleIntensity * 0.3f,
			1.0f
		);
	}
}

void UEmbodyPostProcessingComponent::UpdateDemoMode(float DeltaTime)
{
	if (!bDemoMode) return;

	DemoTimer += DeltaTime;

	if (DemoTimer >= DemoEffectDuration)
	{
		DemoTimer = 0.0f;
		DemoIndex++;

		int32 numPresets = (int32)EEmbodyColorPreset::Retrowave + 1;
		int32 numOverlays = bDemoIncludeOverlays ? 12 : 0;  // Number of overlay effects
		int32 totalEffects = numPresets + numOverlays;

		if (DemoIndex >= totalEffects)
		{
			DemoIndex = 0;
		}

		// Stop any previous overlay
		StopAllOverlays(0.3f);

		if (DemoIndex < numPresets)
		{
			// Show color preset
			EEmbodyColorPreset nextPreset = (EEmbodyColorPreset)DemoIndex;
			ApplyColorPreset(nextPreset);
			CurrentDemoEffectName = GetPresetNameString(nextPreset);
		}
		else if (bDemoIncludeOverlays)
		{
			// Show overlay effect
			ResetToDefault();  // Clear color effects
			int32 overlayIndex = DemoIndex - numPresets;

			switch (overlayIndex)
			{
			case 0:
				PlayBloodSplatter(1, 0.8f, DemoEffectDuration);
				CurrentDemoEffectName = TEXT("Blood Splatter");
				break;
			case 1:
				StartBloodVignette(0.6f, true);
				CurrentDemoEffectName = TEXT("Blood Vignette");
				break;
			case 2:
				StartFrost(0.7f, 0.5f);
				CurrentDemoEffectName = TEXT("Frost");
				break;
			case 3:
				StartRain(0.6f, 0.4f);
				CurrentDemoEffectName = TEXT("Rain");
				break;
			case 4:
				StartVHS(0.7f);
				CurrentDemoEffectName = TEXT("VHS");
				break;
			case 5:
				StartGlitch(0.6f);
				CurrentDemoEffectName = TEXT("Glitch");
				break;
			case 6:
				StartElectric(0.7f);
				CurrentDemoEffectName = TEXT("Electric Shock");
				break;
			case 7:
				StartPoison(0.6f);
				CurrentDemoEffectName = TEXT("Poison");
				break;
			case 8:
				StartBurn(0.7f);
				CurrentDemoEffectName = TEXT("Burn");
				break;
			case 9:
				StartFreeze(0.7f);
				CurrentDemoEffectName = TEXT("Freeze");
				break;
			case 10:
				StartPsychedelic(0.6f, 1.0f);
				CurrentDemoEffectName = TEXT("Psychedelic");
				break;
			case 11:
				CrackScreen(1, 0.8f);
				CurrentDemoEffectName = TEXT("Screen Crack");
				break;
			}
		}

		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, DemoEffectDuration, FColor::Cyan,
				FString::Printf(TEXT("Effect: %s"), *CurrentDemoEffectName));
		}
	}
}

void UEmbodyPostProcessingComponent::UpdateFlash(float DeltaTime)
{
	if (FlashTimer > 0.0f)
	{
		FlashTimer -= DeltaTime;
		float alpha = FMath::Clamp(FlashTimer / FlashDuration, 0.0f, 1.0f);

		if (PostProcessComp)
		{
			PostProcessComp->Settings.SceneColorTint = FMath::Lerp(FLinearColor::White, FlashColor, alpha);
		}
	}
	else if (PostProcessComp)
	{
		PostProcessComp->Settings.SceneColorTint = FLinearColor::White;
	}
}

void UEmbodyPostProcessingComponent::UpdateFocusPull(float DeltaTime)
{
	if (!FocusTarget.IsValid() || FocusPullDuration <= 0.0f) return;

	FocusPullTimer += DeltaTime;
	float t = FMath::Clamp(FocusPullTimer / FocusPullDuration, 0.0f, 1.0f);
	t = FMath::SmoothStep(0.0f, 1.0f, t);

	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (PC && PC->PlayerCameraManager)
	{
		FVector CamLoc = PC->PlayerCameraManager->GetCameraLocation();
		FVector TargetLoc = FocusTarget->GetActorLocation();
		float targetDist = FVector::Dist(CamLoc, TargetLoc);

		FocalDistance = FMath::Lerp(FocusStartDistance, targetDist, t);
	}

	if (t >= 1.0f)
	{
		FocusPullDuration = 0.0f;
	}
}

// ============================================
// PRESET SYSTEM - ALL VALUES RESET TO AVOID BLACK SCREEN
// ============================================

void UEmbodyPostProcessingComponent::ApplyColorPreset(EEmbodyColorPreset Preset)
{
	CurrentPreset = Preset;
	CurrentDemoEffectName = GetPresetNameString(Preset);

	// ALWAYS reset ALL values to safe defaults first!
	Saturation = 1.0f;
	Contrast = 1.0f;
	Gamma = 1.0f;
	Gain = 1.0f;
	Offset = 0.0f;
	Temperature = 0.0f;
	Tint = 0.0f;
	SaturationRGBA = FVector4(1, 1, 1, 1);
	ContrastRGBA = FVector4(1, 1, 1, 1);
	GammaRGBA = FVector4(1, 1, 1, 1);
	GainRGBA = FVector4(1, 1, 1, 1);
	OffsetRGBA = FVector4(0, 0, 0, 0);
	BloomIntensity = 0.675f;
	BloomThreshold = -1.0f;
	BloomSizeScale = 4.0f;
	VignetteIntensity = 0.4f;
	ChromaticAberration = 0.0f;
	FilmGrainIntensity = 0.0f;
	Sharpen = 0.0f;
	ExposureBias = 0.0f;
	FocalDistance = 500.0f;
	Aperture = 4.0f;
	bEnableAnimation = false;
	PulseIntensity = 0.0f;
	FlickerIntensity = 0.0f;
	ColorCycleIntensity = 0.0f;

	// Now apply preset-specific values
	switch (Preset)
	{
	case EEmbodyColorPreset::Default:
		// Already reset to defaults
		break;

	case EEmbodyColorPreset::Cinematic:
		Saturation = 0.9f;
		Contrast = 1.1f;
		Gamma = 0.95f;
		Temperature = -5.0f;
		BloomIntensity = 0.8f;
		VignetteIntensity = 0.5f;
		break;

	case EEmbodyColorPreset::Noir:
		Saturation = 0.0f;
		Contrast = 1.3f;
		Gamma = 0.95f;
		VignetteIntensity = 0.6f;
		FilmGrainIntensity = 0.15f;
		break;

	case EEmbodyColorPreset::Vintage:
		Saturation = 0.7f;
		Contrast = 0.95f;
		Temperature = 15.0f;
		Tint = 10.0f;
		GainRGBA = FVector4(1.1f, 1.0f, 0.9f, 1.0f);
		VignetteIntensity = 0.5f;
		FilmGrainIntensity = 0.1f;
		break;

	case EEmbodyColorPreset::Cyberpunk:
		Saturation = 1.2f;
		Contrast = 1.15f;
		GainRGBA = FVector4(0.95f, 0.95f, 1.15f, 1.0f);
		BloomIntensity = 1.2f;
		ChromaticAberration = 0.8f;
		VignetteIntensity = 0.4f;
		break;

	case EEmbodyColorPreset::Horror:
		Saturation = 0.6f;
		Contrast = 1.2f;
		Gamma = 0.9f;
		GainRGBA = FVector4(0.95f, 0.9f, 0.9f, 1.0f);
		VignetteIntensity = 0.6f;
		FilmGrainIntensity = 0.2f;
		bEnableAnimation = true;
		FlickerIntensity = 0.2f;
		break;

	case EEmbodyColorPreset::Dream:
		Saturation = 0.85f;
		Contrast = 0.9f;
		GainRGBA = FVector4(1.0f, 1.0f, 1.1f, 1.0f);
		BloomIntensity = 1.5f;
		BloomThreshold = 0.0f;
		VignetteIntensity = 0.3f;
		break;

	case EEmbodyColorPreset::NightVision:
		Saturation = 0.0f;
		Contrast = 1.3f;
		GainRGBA = FVector4(0.3f, 1.2f, 0.3f, 1.0f);
		VignetteIntensity = 0.5f;
		FilmGrainIntensity = 0.3f;
		bEnableAnimation = true;
		FlickerIntensity = 0.15f;
		break;

	case EEmbodyColorPreset::Thermal:
		Saturation = 0.0f;
		Contrast = 1.4f;
		GainRGBA = FVector4(1.3f, 0.7f, 0.3f, 1.0f);
		BloomIntensity = 0.5f;
		break;

	case EEmbodyColorPreset::Bleach:
		Saturation = 0.4f;
		Contrast = 1.3f;
		Gamma = 1.05f;
		VignetteIntensity = 0.4f;
		break;

	case EEmbodyColorPreset::Sepia:
		Saturation = 0.0f;
		GainRGBA = FVector4(1.15f, 1.0f, 0.8f, 1.0f);
		Contrast = 1.05f;
		VignetteIntensity = 0.5f;
		FilmGrainIntensity = 0.08f;
		break;

	case EEmbodyColorPreset::Arctic:
		Saturation = 0.7f;
		Temperature = -25.0f;
		GainRGBA = FVector4(0.95f, 0.98f, 1.1f, 1.0f);
		BloomIntensity = 0.9f;
		break;

	case EEmbodyColorPreset::Sunset:
		Saturation = 1.15f;
		Temperature = 35.0f;
		GainRGBA = FVector4(1.1f, 0.98f, 0.9f, 1.0f);
		BloomIntensity = 0.9f;
		VignetteIntensity = 0.3f;
		break;

	case EEmbodyColorPreset::Neon:
		Saturation = 1.4f;
		Contrast = 1.2f;
		BloomIntensity = 1.8f;
		BloomThreshold = 0.3f;
		ChromaticAberration = 1.0f;
		bEnableAnimation = true;
		ColorCycleIntensity = 0.3f;
		break;

	case EEmbodyColorPreset::BleedingOut:
		Saturation = 0.5f;
		Contrast = 1.05f;
		GainRGBA = FVector4(1.15f, 0.95f, 0.95f, 1.0f);
		VignetteIntensity = 0.55f;
		bEnableAnimation = true;
		PulseIntensity = 0.6f;
		break;

	case EEmbodyColorPreset::Drunk:
		Saturation = 1.05f;
		Contrast = 0.95f;
		ChromaticAberration = 1.8f;
		BloomIntensity = 1.0f;
		break;

	case EEmbodyColorPreset::Poisoned:
		Saturation = 0.8f;
		GainRGBA = FVector4(0.9f, 1.1f, 0.85f, 1.0f);
		ChromaticAberration = 1.0f;
		VignetteIntensity = 0.45f;
		bEnableAnimation = true;
		PulseIntensity = 0.4f;
		break;

	case EEmbodyColorPreset::HighOnDrugs:
		Saturation = 1.5f;
		BloomIntensity = 1.8f;
		BloomThreshold = 0.0f;
		ChromaticAberration = 2.0f;
		bEnableAnimation = true;
		ColorCycleIntensity = 0.7f;
		PulseIntensity = 0.2f;
		break;

	case EEmbodyColorPreset::Electrocuted:
		Saturation = 0.85f;
		Contrast = 1.3f;
		GainRGBA = FVector4(0.95f, 0.98f, 1.2f, 1.0f);
		ChromaticAberration = 2.0f;
		bEnableAnimation = true;
		FlickerIntensity = 0.7f;
		break;

	case EEmbodyColorPreset::GlitchCorruption:
		Saturation = 1.0f;
		Contrast = 1.1f;
		ChromaticAberration = 3.0f;
		FilmGrainIntensity = 0.35f;
		bEnableAnimation = true;
		FlickerIntensity = 0.5f;
		break;

	case EEmbodyColorPreset::VHS:
		Saturation = 0.75f;
		Contrast = 0.95f;
		ChromaticAberration = 1.5f;
		FilmGrainIntensity = 0.2f;
		VignetteIntensity = 0.4f;
		bEnableAnimation = true;
		FlickerIntensity = 0.1f;
		break;

	case EEmbodyColorPreset::Retrowave:
		Saturation = 1.3f;
		GainRGBA = FVector4(1.05f, 0.95f, 1.1f, 1.0f);
		BloomIntensity = 1.2f;
		ChromaticAberration = 0.7f;
		VignetteIntensity = 0.3f;
		bEnableAnimation = true;
		ColorCycleIntensity = 0.2f;
		break;
	}

	ApplyAllSettings();
}

void UEmbodyPostProcessingComponent::ResetToDefault()
{
	ApplyColorPreset(EEmbodyColorPreset::Default);
}

// ============================================
// DEMO MODE
// ============================================

void UEmbodyPostProcessingComponent::StartDemoMode(float EffectDuration)
{
	bDemoMode = true;
	DemoEffectDuration = EffectDuration;
	DemoIndex = 0;
	DemoTimer = EffectDuration; // Trigger first effect immediately

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Green,
			TEXT("Demo Mode Started - Cycling through all effects"));
	}
}

void UEmbodyPostProcessingComponent::StopDemoMode()
{
	bDemoMode = false;
	ResetToDefault();

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Yellow,
			TEXT("Demo Mode Stopped"));
	}
}

// ============================================
// SIMPLE SETTERS
// ============================================

void UEmbodyPostProcessingComponent::SetSaturation(float Value)
{
	Saturation = Value;
	ApplyAllSettings();
}

void UEmbodyPostProcessingComponent::SetContrast(float Value)
{
	Contrast = Value;
	ApplyAllSettings();
}

void UEmbodyPostProcessingComponent::SetGamma(float Value)
{
	Gamma = Value;
	ApplyAllSettings();
}

void UEmbodyPostProcessingComponent::SetGain(float Value)
{
	Gain = Value;
	ApplyAllSettings();
}

void UEmbodyPostProcessingComponent::SetTemperature(float Value)
{
	Temperature = Value;
	ApplyAllSettings();
}

void UEmbodyPostProcessingComponent::SetTint(float Value)
{
	Tint = Value;
	ApplyAllSettings();
}

void UEmbodyPostProcessingComponent::SetBloomIntensity(float Value)
{
	BloomIntensity = Value;
	ApplyAllSettings();
}

void UEmbodyPostProcessingComponent::SetBloomSize(float Value)
{
	BloomSizeScale = Value;
	ApplyAllSettings();
}

void UEmbodyPostProcessingComponent::SetVignetteIntensity(float Value)
{
	VignetteIntensity = Value;
	ApplyAllSettings();
}

void UEmbodyPostProcessingComponent::SetChromaticAberration(float Value)
{
	ChromaticAberration = Value;
	ApplyAllSettings();
}

void UEmbodyPostProcessingComponent::SetFilmGrain(float Value)
{
	FilmGrainIntensity = Value;
	ApplyAllSettings();
}

void UEmbodyPostProcessingComponent::SetSharpen(float Value)
{
	Sharpen = Value;
	ApplyAllSettings();
}

void UEmbodyPostProcessingComponent::SetExposure(float Value)
{
	ExposureBias = Value;
	ApplyAllSettings();
}

void UEmbodyPostProcessingComponent::SetFocusDistance(float Value)
{
	FocalDistance = Value;
	ApplyAllSettings();
}

void UEmbodyPostProcessingComponent::SetAperture(float Value)
{
	Aperture = Value;
	ApplyAllSettings();
}

void UEmbodyPostProcessingComponent::FocusOnActor(AActor* Target, float Duration)
{
	if (!Target) return;

	FocusTarget = Target;
	FocusPullDuration = Duration;
	FocusPullTimer = 0.0f;
	FocusStartDistance = FocalDistance;
}

// ============================================
// RGBA SETTERS
// ============================================

void UEmbodyPostProcessingComponent::SetSaturationRGBA(float R, float G, float B, float A)
{
	SaturationRGBA = FVector4(R, G, B, A);
	ApplyAllSettings();
}

void UEmbodyPostProcessingComponent::SetContrastRGBA(float R, float G, float B, float A)
{
	ContrastRGBA = FVector4(R, G, B, A);
	ApplyAllSettings();
}

void UEmbodyPostProcessingComponent::SetGammaRGBA(float R, float G, float B, float A)
{
	GammaRGBA = FVector4(R, G, B, A);
	ApplyAllSettings();
}

void UEmbodyPostProcessingComponent::SetGainRGBA(float R, float G, float B, float A)
{
	GainRGBA = FVector4(R, G, B, A);
	ApplyAllSettings();
}

void UEmbodyPostProcessingComponent::SetOffsetRGBA(float R, float G, float B, float A)
{
	OffsetRGBA = FVector4(R, G, B, A);
	ApplyAllSettings();
}

// ============================================
// IMPACT EFFECTS
// ============================================

void UEmbodyPostProcessingComponent::DamageFlash(FLinearColor Color, float Intensity)
{
	FlashColor = Color * Intensity;
	FlashDuration = 0.15f;
	FlashTimer = FlashDuration;
}

void UEmbodyPostProcessingComponent::HealFlash(float Intensity)
{
	FlashColor = FLinearColor(0.2f, 1.0f, 0.3f, 1.0f) * Intensity;
	FlashDuration = 0.2f;
	FlashTimer = FlashDuration;
}

void UEmbodyPostProcessingComponent::ScreenFlash(FLinearColor Color, float Duration)
{
	FlashColor = Color;
	FlashDuration = Duration;
	FlashTimer = Duration;
}

// ============================================
// GETTERS
// ============================================

FString UEmbodyPostProcessingComponent::GetCurrentPresetName() const
{
	return GetPresetNameString(CurrentPreset);
}

FString UEmbodyPostProcessingComponent::GetPresetNameString(EEmbodyColorPreset Preset) const
{
	switch (Preset)
	{
	case EEmbodyColorPreset::Default: return TEXT("Default");
	case EEmbodyColorPreset::Cinematic: return TEXT("Cinematic");
	case EEmbodyColorPreset::Noir: return TEXT("Noir");
	case EEmbodyColorPreset::Vintage: return TEXT("Vintage");
	case EEmbodyColorPreset::Cyberpunk: return TEXT("Cyberpunk");
	case EEmbodyColorPreset::Horror: return TEXT("Horror");
	case EEmbodyColorPreset::Dream: return TEXT("Dream");
	case EEmbodyColorPreset::NightVision: return TEXT("Night Vision");
	case EEmbodyColorPreset::Thermal: return TEXT("Thermal");
	case EEmbodyColorPreset::Bleach: return TEXT("Bleach Bypass");
	case EEmbodyColorPreset::Sepia: return TEXT("Sepia");
	case EEmbodyColorPreset::Arctic: return TEXT("Arctic");
	case EEmbodyColorPreset::Sunset: return TEXT("Sunset");
	case EEmbodyColorPreset::Neon: return TEXT("Neon");
	case EEmbodyColorPreset::BleedingOut: return TEXT("Bleeding Out");
	case EEmbodyColorPreset::Drunk: return TEXT("Drunk");
	case EEmbodyColorPreset::Poisoned: return TEXT("Poisoned");
	case EEmbodyColorPreset::HighOnDrugs: return TEXT("High");
	case EEmbodyColorPreset::Electrocuted: return TEXT("Electrocuted");
	case EEmbodyColorPreset::GlitchCorruption: return TEXT("Glitch");
	case EEmbodyColorPreset::VHS: return TEXT("VHS");
	case EEmbodyColorPreset::Retrowave: return TEXT("Retrowave");
	default: return TEXT("Unknown");
	}
}

// ============================================
// SCREEN OVERLAY SYSTEM
// ============================================

void UEmbodyPostProcessingComponent::CreateOverlayMaterial()
{
	// Try to load existing material first
	if (!OverlayBaseMaterial)
	{
		OverlayBaseMaterial = LoadObject<UMaterialInterface>(nullptr,
			TEXT("/Game/Materials/PostProcess/M_PP_Overlay.M_PP_Overlay"));
	}

	// If still no material, we'll create dynamic materials from engine default
	if (!OverlayBaseMaterial)
	{
		UE_LOG(LogTemp, Warning, TEXT("EmbodyPP: No overlay base material found. Create M_PP_Overlay in Content/Materials/PostProcess/"));
	}
}

UMaterialInstanceDynamic* UEmbodyPostProcessingComponent::CreateOverlayMID(UTexture2D* Texture)
{
	if (!OverlayBaseMaterial || !Texture) return nullptr;

	UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(OverlayBaseMaterial, this);
	if (MID)
	{
		MID->SetTextureParameterValue(TEXT("OverlayTexture"), Texture);
		MID->SetScalarParameterValue(TEXT("Intensity"), 0.0f);
	}
	return MID;
}

FScreenOverlayEffect* UEmbodyPostProcessingComponent::FindOverlay(FName EffectName)
{
	for (FScreenOverlayEffect& Effect : ActiveOverlays)
	{
		if (Effect.EffectName == EffectName)
		{
			return &Effect;
		}
	}
	return nullptr;
}

FScreenOverlayEffect& UEmbodyPostProcessingComponent::GetOrCreateOverlay(FName EffectName, UTexture2D* Texture)
{
	FScreenOverlayEffect* Existing = FindOverlay(EffectName);
	if (Existing)
	{
		// Update texture if different
		if (Texture && Existing->Material)
		{
			Existing->Material->SetTextureParameterValue(TEXT("OverlayTexture"), Texture);
		}
		return *Existing;
	}

	// Create new overlay
	FScreenOverlayEffect NewEffect;
	NewEffect.EffectName = EffectName;
	NewEffect.Material = CreateOverlayMID(Texture);

	if (NewEffect.Material && PostProcessComp)
	{
		PostProcessComp->Settings.AddBlendable(NewEffect.Material, 1.0f);
	}

	ActiveOverlays.Add(NewEffect);
	return ActiveOverlays.Last();
}

void UEmbodyPostProcessingComponent::RemoveOverlay(FName EffectName)
{
	for (int32 i = ActiveOverlays.Num() - 1; i >= 0; --i)
	{
		if (ActiveOverlays[i].EffectName == EffectName)
		{
			if (ActiveOverlays[i].Material && PostProcessComp)
			{
				PostProcessComp->Settings.RemoveBlendable(ActiveOverlays[i].Material);
			}
			ActiveOverlays.RemoveAt(i);
		}
	}
}

void UEmbodyPostProcessingComponent::UpdateOverlays(float DeltaTime)
{
	for (int32 i = ActiveOverlays.Num() - 1; i >= 0; --i)
	{
		FScreenOverlayEffect& Effect = ActiveOverlays[i];

		// Linear fade toward target (FadeSpeed = units per second)
		if (!FMath::IsNearlyEqual(Effect.CurrentIntensity, Effect.TargetIntensity, 0.001f))
		{
			float FadeAmount = Effect.FadeSpeed * DeltaTime;
			if (Effect.CurrentIntensity > Effect.TargetIntensity)
			{
				Effect.CurrentIntensity = FMath::Max(Effect.CurrentIntensity - FadeAmount, Effect.TargetIntensity);
			}
			else
			{
				Effect.CurrentIntensity = FMath::Min(Effect.CurrentIntensity + FadeAmount, Effect.TargetIntensity);
			}
		}

		// Update UV animation
		if (!Effect.UVSpeed.IsNearlyZero())
		{
			Effect.UVOffset += Effect.UVSpeed * DeltaTime;
			Effect.UVOffset.X = FMath::Fmod(Effect.UVOffset.X, 1.0f);
			Effect.UVOffset.Y = FMath::Fmod(Effect.UVOffset.Y, 1.0f);
		}

		// Pulse
		float FinalIntensity = Effect.CurrentIntensity;
		if (Effect.bPulse)
		{
			Effect.PulsePhase += DeltaTime * Effect.PulseSpeed;
			float Pulse = FMath::Sin(Effect.PulsePhase * 6.28f) * 0.5f + 0.5f;
			FinalIntensity *= 0.7f + Pulse * 0.3f;
		}

		// Apply to material
		if (Effect.Material)
		{
			Effect.Material->SetScalarParameterValue(TEXT("Intensity"), FinalIntensity);
			Effect.Material->SetVectorParameterValue(TEXT("UVOffset"),
				FLinearColor(Effect.UVOffset.X, Effect.UVOffset.Y, 0, 0));
		}

		// Remove if faded out
		if (Effect.CurrentIntensity <= 0.001f && Effect.TargetIntensity <= 0.0f)
		{
			if (Effect.Material && PostProcessComp)
			{
				PostProcessComp->Settings.RemoveBlendable(Effect.Material);
			}
			ActiveOverlays.RemoveAt(i);
		}
	}
}

// ============================================
// SCREEN EFFECT FUNCTIONS
// ============================================

void UEmbodyPostProcessingComponent::PlayBloodSplatter(int32 SplatterIndex, float Intensity, float FadeTime)
{
	UTexture2D* Tex = nullptr;
	FName EffectName = NAME_None;

	switch (SplatterIndex)
	{
		case 1: Tex = Tex_BloodSplatter01; EffectName = TEXT("BloodSplatter1"); break;
		case 2: Tex = Tex_BloodSplatter02; EffectName = TEXT("BloodSplatter2"); break;
		case 3: Tex = Tex_BloodSplatter03; EffectName = TEXT("BloodSplatter3"); break;
		default: Tex = Tex_BloodSplatter01; EffectName = TEXT("BloodSplatter1"); break;
	}

	if (!Tex) return;

	FScreenOverlayEffect& Effect = GetOrCreateOverlay(EffectName, Tex);
	Effect.CurrentIntensity = Intensity;
	Effect.TargetIntensity = 0.0f;
	Effect.FadeSpeed = 1.0f / FMath::Max(FadeTime, 0.1f);
}

void UEmbodyPostProcessingComponent::StartBloodVignette(float Intensity, bool bPulseWithHeartbeat)
{
	if (!Tex_BloodVignette) return;

	FScreenOverlayEffect& Effect = GetOrCreateOverlay(TEXT("BloodVignette"), Tex_BloodVignette);
	Effect.TargetIntensity = Intensity;
	Effect.FadeSpeed = 0.5f;  // Fade in over ~1 second
	Effect.bPulse = bPulseWithHeartbeat;
	Effect.PulseSpeed = 1.2f;  // ~72 BPM
}

void UEmbodyPostProcessingComponent::StopBloodVignette(float FadeTime)
{
	FScreenOverlayEffect* Effect = FindOverlay(TEXT("BloodVignette"));
	if (Effect)
	{
		Effect->TargetIntensity = 0.0f;
		Effect->FadeSpeed = 1.0f / FMath::Max(FadeTime, 0.1f);
		Effect->bPulse = false;
	}
}

void UEmbodyPostProcessingComponent::StartBloodDrip(float Intensity, float DripSpeed)
{
	if (!Tex_BloodDrip) return;

	FScreenOverlayEffect& Effect = GetOrCreateOverlay(TEXT("BloodDrip"), Tex_BloodDrip);
	Effect.TargetIntensity = Intensity;
	Effect.FadeSpeed = 0.5f;  // Fade in over ~1.5 seconds
	Effect.UVSpeed = FVector2D(0.0f, DripSpeed);
}

void UEmbodyPostProcessingComponent::StopBloodDrip(float FadeTime)
{
	FScreenOverlayEffect* Effect = FindOverlay(TEXT("BloodDrip"));
	if (Effect)
	{
		Effect->TargetIntensity = 0.0f;
		Effect->FadeSpeed = 1.0f / FMath::Max(FadeTime, 0.1f);
	}
}

void UEmbodyPostProcessingComponent::CrackScreen(int32 Pattern, float Intensity)
{
	UTexture2D* Tex = (Pattern == 2) ? Tex_ScreenCrack02 : Tex_ScreenCrack01;
	if (!Tex) return;

	FScreenOverlayEffect& Effect = GetOrCreateOverlay(TEXT("ScreenCrack"), Tex);
	Effect.CurrentIntensity = Intensity;  // Instant crack
	Effect.TargetIntensity = Intensity;
	Effect.FadeSpeed = 1.0f;
}

void UEmbodyPostProcessingComponent::HealCracks(float FadeTime)
{
	FScreenOverlayEffect* Effect = FindOverlay(TEXT("ScreenCrack"));
	if (Effect)
	{
		Effect->TargetIntensity = 0.0f;
		Effect->FadeSpeed = 1.0f / FMath::Max(FadeTime, 0.1f);
	}
}

void UEmbodyPostProcessingComponent::StartFrost(float Intensity, float BuildSpeed)
{
	UTexture2D* Tex = Tex_Frost01 ? Tex_Frost01 : Tex_Frost02;
	if (!Tex) return;

	FScreenOverlayEffect& Effect = GetOrCreateOverlay(TEXT("Frost"), Tex);
	Effect.TargetIntensity = Intensity;
	Effect.FadeSpeed = BuildSpeed;
}

void UEmbodyPostProcessingComponent::StopFrost(float MeltSpeed)
{
	FScreenOverlayEffect* Effect = FindOverlay(TEXT("Frost"));
	if (Effect)
	{
		Effect->TargetIntensity = 0.0f;
		Effect->FadeSpeed = MeltSpeed;
	}
}

void UEmbodyPostProcessingComponent::StartRain(float Intensity, float DropSpeed)
{
	if (!Tex_RainStreaks) return;

	FScreenOverlayEffect& Effect = GetOrCreateOverlay(TEXT("Rain"), Tex_RainStreaks);
	Effect.TargetIntensity = Intensity;
	Effect.FadeSpeed = 0.5f;  // Gradual rain start
	Effect.UVSpeed = FVector2D(0.0f, DropSpeed);
}

void UEmbodyPostProcessingComponent::StopRain(float FadeTime)
{
	FScreenOverlayEffect* Effect = FindOverlay(TEXT("Rain"));
	if (Effect)
	{
		Effect->TargetIntensity = 0.0f;
		Effect->FadeSpeed = 1.0f / FMath::Max(FadeTime, 0.1f);
	}
}

void UEmbodyPostProcessingComponent::StartVHS(float Intensity)
{
	if (!Tex_VHSNoise) return;

	FScreenOverlayEffect& Effect = GetOrCreateOverlay(TEXT("VHS"), Tex_VHSNoise);
	Effect.TargetIntensity = Intensity;
	Effect.FadeSpeed = 1.0f;  // Quick but visible fade
	Effect.UVSpeed = FVector2D(0.1f, 0.02f);
}

void UEmbodyPostProcessingComponent::StopVHS(float FadeTime)
{
	FScreenOverlayEffect* Effect = FindOverlay(TEXT("VHS"));
	if (Effect)
	{
		Effect->TargetIntensity = 0.0f;
		Effect->FadeSpeed = 1.0f / FMath::Max(FadeTime, 0.1f);
	}
}

void UEmbodyPostProcessingComponent::StartGlitch(float Intensity)
{
	UTexture2D* Tex = Tex_GlitchBlocks ? Tex_GlitchBlocks : Tex_GlitchLines;
	if (!Tex) return;

	FScreenOverlayEffect& Effect = GetOrCreateOverlay(TEXT("Glitch"), Tex);
	Effect.TargetIntensity = Intensity;
	Effect.FadeSpeed = 2.0f;  // Fast glitch appearance
	Effect.UVSpeed = FVector2D(0.5f, 0.0f);
}

void UEmbodyPostProcessingComponent::StopGlitch(float FadeTime)
{
	FScreenOverlayEffect* Effect = FindOverlay(TEXT("Glitch"));
	if (Effect)
	{
		Effect->TargetIntensity = 0.0f;
		Effect->FadeSpeed = 1.0f / FMath::Max(FadeTime, 0.1f);
	}
}

void UEmbodyPostProcessingComponent::PlayEMP(float Intensity, float Duration)
{
	if (!Tex_EMP) return;

	FScreenOverlayEffect& Effect = GetOrCreateOverlay(TEXT("EMP"), Tex_EMP);
	Effect.CurrentIntensity = Intensity;
	Effect.TargetIntensity = 0.0f;
	Effect.FadeSpeed = 1.0f / FMath::Max(Duration, 0.1f);
}

void UEmbodyPostProcessingComponent::StartElectric(float Intensity)
{
	if (!Tex_Electric) return;

	FScreenOverlayEffect& Effect = GetOrCreateOverlay(TEXT("Electric"), Tex_Electric);
	Effect.TargetIntensity = Intensity;
	Effect.FadeSpeed = 3.0f;  // Quick shock onset
	Effect.bPulse = true;
	Effect.PulseSpeed = 8.0f;  // Fast flicker
}

void UEmbodyPostProcessingComponent::StopElectric(float FadeTime)
{
	FScreenOverlayEffect* Effect = FindOverlay(TEXT("Electric"));
	if (Effect)
	{
		Effect->TargetIntensity = 0.0f;
		Effect->FadeSpeed = 1.0f / FMath::Max(FadeTime, 0.1f);
		Effect->bPulse = false;
	}
}

void UEmbodyPostProcessingComponent::StartPoison(float Intensity)
{
	if (!Tex_Poison) return;

	FScreenOverlayEffect& Effect = GetOrCreateOverlay(TEXT("Poison"), Tex_Poison);
	Effect.TargetIntensity = Intensity;
	Effect.FadeSpeed = 0.4f;  // Slow poison creep
	Effect.UVSpeed = FVector2D(0.0f, -0.15f);  // Bubbles rise
	Effect.bPulse = true;
	Effect.PulseSpeed = 0.5f;
}

void UEmbodyPostProcessingComponent::StopPoison(float FadeTime)
{
	FScreenOverlayEffect* Effect = FindOverlay(TEXT("Poison"));
	if (Effect)
	{
		Effect->TargetIntensity = 0.0f;
		Effect->FadeSpeed = 1.0f / FMath::Max(FadeTime, 0.1f);
	}
}

void UEmbodyPostProcessingComponent::StartBurn(float Intensity)
{
	if (!Tex_Burn) return;

	FScreenOverlayEffect& Effect = GetOrCreateOverlay(TEXT("Burn"), Tex_Burn);
	Effect.TargetIntensity = Intensity;
	Effect.FadeSpeed = 0.5f;  // Burning spreads gradually
	Effect.bPulse = true;
	Effect.PulseSpeed = 2.0f;
}

void UEmbodyPostProcessingComponent::StopBurn(float FadeTime)
{
	FScreenOverlayEffect* Effect = FindOverlay(TEXT("Burn"));
	if (Effect)
	{
		Effect->TargetIntensity = 0.0f;
		Effect->FadeSpeed = 1.0f / FMath::Max(FadeTime, 0.1f);
	}
}

void UEmbodyPostProcessingComponent::StartFreeze(float Intensity)
{
	if (!Tex_Freeze) return;

	FScreenOverlayEffect& Effect = GetOrCreateOverlay(TEXT("Freeze"), Tex_Freeze);
	Effect.TargetIntensity = Intensity;
	Effect.FadeSpeed = 1.0f;
}

void UEmbodyPostProcessingComponent::StopFreeze(float FadeTime)
{
	FScreenOverlayEffect* Effect = FindOverlay(TEXT("Freeze"));
	if (Effect)
	{
		Effect->TargetIntensity = 0.0f;
		Effect->FadeSpeed = 1.0f / FMath::Max(FadeTime, 0.1f);
	}
}

void UEmbodyPostProcessingComponent::SetLensDirt(float Intensity)
{
	if (!Tex_LensDirt) return;

	FScreenOverlayEffect& Effect = GetOrCreateOverlay(TEXT("LensDirt"), Tex_LensDirt);
	Effect.TargetIntensity = Intensity;
	Effect.FadeSpeed = 3.0f;
}

void UEmbodyPostProcessingComponent::StartLightLeak(int32 LeakIndex, float Intensity)
{
	UTexture2D* Tex = (LeakIndex == 2) ? Tex_LightLeak02 : Tex_LightLeak01;
	if (!Tex) return;

	FScreenOverlayEffect& Effect = GetOrCreateOverlay(TEXT("LightLeak"), Tex);
	Effect.TargetIntensity = Intensity;
	Effect.FadeSpeed = 2.0f;
}

void UEmbodyPostProcessingComponent::StopLightLeak(float FadeTime)
{
	FScreenOverlayEffect* Effect = FindOverlay(TEXT("LightLeak"));
	if (Effect)
	{
		Effect->TargetIntensity = 0.0f;
		Effect->FadeSpeed = 1.0f / FMath::Max(FadeTime, 0.1f);
	}
}

void UEmbodyPostProcessingComponent::StartPsychedelic(float Intensity, float Speed)
{
	UTexture2D* Tex = Tex_Spiral ? Tex_Spiral : Tex_AcidPattern;
	if (!Tex) return;

	FScreenOverlayEffect& Effect = GetOrCreateOverlay(TEXT("Psychedelic"), Tex);
	Effect.TargetIntensity = Intensity;
	Effect.FadeSpeed = 1.0f;
	Effect.UVSpeed = FVector2D(Speed * 0.05f, Speed * 0.03f);
	Effect.bPulse = true;
	Effect.PulseSpeed = Speed * 0.5f;
}

void UEmbodyPostProcessingComponent::StopPsychedelic(float FadeTime)
{
	FScreenOverlayEffect* Effect = FindOverlay(TEXT("Psychedelic"));
	if (Effect)
	{
		Effect->TargetIntensity = 0.0f;
		Effect->FadeSpeed = 1.0f / FMath::Max(FadeTime, 0.1f);
	}
}

void UEmbodyPostProcessingComponent::PlayTextureOverlay(UTexture2D* Texture, FName EffectName, float Intensity, float FadeInTime, float Duration)
{
	if (!Texture) return;

	FScreenOverlayEffect& Effect = GetOrCreateOverlay(EffectName, Texture);
	Effect.TargetIntensity = Intensity;
	Effect.FadeSpeed = 1.0f / FMath::Max(FadeInTime, 0.1f);

	// If duration specified, start fading after that
	if (Duration > 0.0f)
	{
		// We'd need a timer here, for now just set immediate fade
		Effect.CurrentIntensity = Intensity;
		Effect.TargetIntensity = 0.0f;
		Effect.FadeSpeed = 1.0f / FMath::Max(Duration, 0.1f);
	}
}

void UEmbodyPostProcessingComponent::StopOverlay(FName EffectName, float FadeTime)
{
	FScreenOverlayEffect* Effect = FindOverlay(EffectName);
	if (Effect)
	{
		Effect->TargetIntensity = 0.0f;
		Effect->FadeSpeed = 1.0f / FMath::Max(FadeTime, 0.1f);
	}
}

void UEmbodyPostProcessingComponent::StopAllOverlays(float FadeTime)
{
	float Speed = 1.0f / FMath::Max(FadeTime, 0.1f);
	for (FScreenOverlayEffect& Effect : ActiveOverlays)
	{
		Effect.TargetIntensity = 0.0f;
		Effect.FadeSpeed = Speed;
		Effect.bPulse = false;
	}
}

void UEmbodyPostProcessingComponent::AutoLoadTextures()
{
	// Load all textures from Content/Textures/PostProcess
	const FString BasePath = TEXT("/Game/Textures/PostProcess/");

	Tex_BloodSplatter01 = LoadObject<UTexture2D>(nullptr, *(BasePath + TEXT("T_Blood_Splatter_01")));
	Tex_BloodSplatter02 = LoadObject<UTexture2D>(nullptr, *(BasePath + TEXT("T_Blood_Splatter_02")));
	Tex_BloodSplatter03 = LoadObject<UTexture2D>(nullptr, *(BasePath + TEXT("T_Blood_Splatter_03")));
	Tex_BloodVignette = LoadObject<UTexture2D>(nullptr, *(BasePath + TEXT("T_Blood_Vignette")));
	Tex_BloodDrip = LoadObject<UTexture2D>(nullptr, *(BasePath + TEXT("T_Blood_Drip_Mask")));
	Tex_Veins = LoadObject<UTexture2D>(nullptr, *(BasePath + TEXT("T_Veins_Overlay")));

	Tex_ScreenCrack01 = LoadObject<UTexture2D>(nullptr, *(BasePath + TEXT("T_Screen_Crack_01")));
	Tex_ScreenCrack02 = LoadObject<UTexture2D>(nullptr, *(BasePath + TEXT("T_Screen_Crack_02")));
	Tex_DamageIndicator = LoadObject<UTexture2D>(nullptr, *(BasePath + TEXT("T_Directional_Damage_S")));

	Tex_Frost01 = LoadObject<UTexture2D>(nullptr, *(BasePath + TEXT("T_Frost_Pattern_01")));
	Tex_Frost02 = LoadObject<UTexture2D>(nullptr, *(BasePath + TEXT("T_Frost_Pattern_02")));
	Tex_FrostNoise = LoadObject<UTexture2D>(nullptr, *(BasePath + TEXT("T_Frost_Noise")));
	Tex_RainDrops = LoadObject<UTexture2D>(nullptr, *(BasePath + TEXT("T_Rain_Drops_Normal")));
	Tex_RainStreaks = LoadObject<UTexture2D>(nullptr, *(BasePath + TEXT("T_Rain_Streaks_Mask")));
	Tex_HeatNoise = LoadObject<UTexture2D>(nullptr, *(BasePath + TEXT("T_Heat_Distortion_Noise")));
	Tex_Caustics = LoadObject<UTexture2D>(nullptr, *(BasePath + TEXT("T_Underwater_Caustics")));
	Tex_Condensation = LoadObject<UTexture2D>(nullptr, *(BasePath + TEXT("T_Condensation_Drops")));

	Tex_VHSNoise = LoadObject<UTexture2D>(nullptr, *(BasePath + TEXT("T_VHS_Noise")));
	Tex_GlitchBlocks = LoadObject<UTexture2D>(nullptr, *(BasePath + TEXT("T_Digital_Corruption")));
	Tex_GlitchLines = LoadObject<UTexture2D>(nullptr, *(BasePath + TEXT("T_Glitch_Lines")));
	Tex_EMP = LoadObject<UTexture2D>(nullptr, *(BasePath + TEXT("T_EMP_Burst")));
	Tex_Electric = LoadObject<UTexture2D>(nullptr, *(BasePath + TEXT("T_Electric_Shock")));
	Tex_Hologram = LoadObject<UTexture2D>(nullptr, *(BasePath + TEXT("T_Hologram_Scanlines")));

	Tex_Poison = LoadObject<UTexture2D>(nullptr, *(BasePath + TEXT("T_Poison_Bubbles")));
	Tex_Burn = LoadObject<UTexture2D>(nullptr, *(BasePath + TEXT("T_Burn_Edges")));
	Tex_Freeze = LoadObject<UTexture2D>(nullptr, *(BasePath + TEXT("T_Freeze_Ice_Frame")));

	Tex_LensDirt = LoadObject<UTexture2D>(nullptr, *(BasePath + TEXT("T_Lens_Dirt")));
	Tex_LightLeak01 = LoadObject<UTexture2D>(nullptr, *(BasePath + TEXT("T_Light_Leak_01")));
	Tex_LightLeak02 = LoadObject<UTexture2D>(nullptr, *(BasePath + TEXT("T_Light_Leak_02")));

	Tex_Spiral = LoadObject<UTexture2D>(nullptr, *(BasePath + TEXT("T_Trip_Spiral")));
	Tex_AcidPattern = LoadObject<UTexture2D>(nullptr, *(BasePath + TEXT("T_Acid_Pattern")));
	Tex_Kaleidoscope = LoadObject<UTexture2D>(nullptr, *(BasePath + TEXT("T_Kaleidoscope_Mask")));
	Tex_ColorAberration = LoadObject<UTexture2D>(nullptr, *(BasePath + TEXT("T_Color_Aberration_Map")));

	UE_LOG(LogTemp, Log, TEXT("EmbodyPP: Auto-loaded textures from %s"), *BasePath);
}

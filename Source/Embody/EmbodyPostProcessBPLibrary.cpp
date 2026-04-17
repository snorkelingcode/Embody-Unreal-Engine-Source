// EmbodyPostProcessBPLibrary.cpp
// Unified post-process command processor for TCP commands.

#include "EmbodyPostProcessBPLibrary.h"
#include "CineCameraComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogEmbodyPP, Log, All);

// ---------------------------------------------------------------------------
// All top-level prefixes that identify a post-process command.
// Order does not matter here — we only check StartsWith.
// ---------------------------------------------------------------------------
static const TCHAR* PPPrefixes[] =
{
	TEXT("BloomInt_"),
	TEXT("Bloom_Size_"),
	TEXT("BloomMeth_"),
	TEXT("CHROME_"),
	TEXT("EXPO_"),
	TEXT("GRAIN_"),
	TEXT("VIG_"),
	TEXT("SHARP_"),
	TEXT("CG_Temp_"),
	TEXT("CG_Tint_"),
	TEXT("CGG_"),
};

// ---------------------------------------------------------------------------
// IsPostProcessCommand
// ---------------------------------------------------------------------------
bool UEmbodyPostProcessBPLibrary::IsPostProcessCommand(const FString& Command)
{
	for (const TCHAR* Prefix : PPPrefixes)
	{
		if (Command.StartsWith(Prefix))
		{
			return true;
		}
	}
	return false;
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
TArray<float> UEmbodyPostProcessBPLibrary::ParseFloats(const FString& ParamString)
{
	TArray<float> Result;
	TArray<FString> Parts;
	ParamString.ParseIntoArray(Parts, TEXT("_"));
	for (const FString& P : Parts)
	{
		if (!P.IsEmpty())
		{
			Result.Add(FCString::Atof(*P));
		}
	}
	return Result;
}

FVector4 UEmbodyPostProcessBPLibrary::MakeVec4(const TArray<float>& V, float DefaultW)
{
	return FVector4(
		V.IsValidIndex(0) ? V[0] : 0.f,
		V.IsValidIndex(1) ? V[1] : 0.f,
		V.IsValidIndex(2) ? V[2] : 0.f,
		V.IsValidIndex(3) ? V[3] : DefaultW
	);
}

// ---------------------------------------------------------------------------
// Helper: try to match a prefix and return the tail (parameter portion).
// ---------------------------------------------------------------------------
static bool TryMatch(const FString& Command, const TCHAR* Prefix, FString& OutParams)
{
	if (Command.StartsWith(Prefix))
	{
		OutParams = Command.RightChop(FCString::Strlen(Prefix));
		return true;
	}
	return false;
}

// ---------------------------------------------------------------------------
// ApplyColorGrading — sets the correct PostProcessSettings field
// for a given Level (Global/Shadows/Midtones/Highlights) and Property
// (Saturation/Contrast/Gamma/Gain/Offset).
// ---------------------------------------------------------------------------
void UEmbodyPostProcessBPLibrary::ApplyColorGrading(
	FPostProcessSettings& PP,
	const FString& Level,
	const FString& Property,
	const TArray<float>& Values)
{
	const FVector4 Vec = MakeVec4(Values);

	// Macro to reduce repetition for each Property × Level combination.
#define SET_CG(PropName) \
	if (Level == TEXT("Global"))     { PP.bOverride_Color##PropName = true; PP.Color##PropName = Vec; } \
	else if (Level == TEXT("Shadows"))    { PP.bOverride_Color##PropName##Shadows = true; PP.Color##PropName##Shadows = Vec; } \
	else if (Level == TEXT("Midtones"))   { PP.bOverride_Color##PropName##Midtones = true; PP.Color##PropName##Midtones = Vec; } \
	else if (Level == TEXT("Highlights")) { PP.bOverride_Color##PropName##Highlights = true; PP.Color##PropName##Highlights = Vec; }

	if (Property == TEXT("Saturation"))      { SET_CG(Saturation) }
	else if (Property == TEXT("Contrast"))    { SET_CG(Contrast) }
	else if (Property == TEXT("Gamma"))       { SET_CG(Gamma) }
	else if (Property == TEXT("Gain"))        { SET_CG(Gain) }
	else if (Property == TEXT("Offset"))      { SET_CG(Offset) }
	else
	{
		UE_LOG(LogEmbodyPP, Warning, TEXT("Unknown color grading property: %s"), *Property);
	}

#undef SET_CG
}

// ---------------------------------------------------------------------------
// ApplyPostProcessCommand — the main dispatcher
// ---------------------------------------------------------------------------
void UEmbodyPostProcessBPLibrary::ApplyPostProcessCommand(
	UCineCameraComponent* CineCamera,
	const FString& Command)
{
	if (!CineCamera)
	{
		UE_LOG(LogEmbodyPP, Warning, TEXT("ApplyPostProcessCommand: null CineCamera"));
		return;
	}

	FPostProcessSettings& PP = CineCamera->PostProcessSettings;
	FString Params;

	// ==== BLOOM ====
	if (TryMatch(Command, TEXT("BloomInt_"), Params))
	{
		PP.bOverride_BloomIntensity = true;
		PP.BloomIntensity = FCString::Atof(*Params);
	}
	else if (TryMatch(Command, TEXT("Bloom_Size_"), Params))
	{
		PP.bOverride_BloomSizeScale = true;
		PP.BloomSizeScale = FCString::Atof(*Params);
	}
	else if (TryMatch(Command, TEXT("BloomMeth_"), Params))
	{
		PP.bOverride_BloomMethod = true;
		if (Params.Equals(TEXT("Convolution"), ESearchCase::IgnoreCase))
		{
			PP.BloomMethod = BM_FFT;
		}
		else
		{
			PP.BloomMethod = BM_SOG;
		}
	}

	// ==== CHROMATIC ABERRATION ====
	else if (TryMatch(Command, TEXT("CHROME_Int_"), Params))
	{
		PP.bOverride_SceneFringeIntensity = true;
		PP.SceneFringeIntensity = FCString::Atof(*Params);
	}

	// ==== EXPOSURE ====
	else if (TryMatch(Command, TEXT("EXPO_Comp_"), Params))
	{
		PP.bOverride_AutoExposureBias = true;
		PP.AutoExposureBias = FCString::Atof(*Params);
	}
	else if (TryMatch(Command, TEXT("EXPO_MinEV100_"), Params))
	{
		PP.bOverride_AutoExposureMinBrightness = true;
		PP.AutoExposureMinBrightness = FCString::Atof(*Params);
	}
	else if (TryMatch(Command, TEXT("EXPO_MaxEV100_"), Params))
	{
		PP.bOverride_AutoExposureMaxBrightness = true;
		PP.AutoExposureMaxBrightness = FCString::Atof(*Params);
	}
	else if (TryMatch(Command, TEXT("EXPO_SpeedUp_"), Params))
	{
		PP.bOverride_AutoExposureSpeedUp = true;
		PP.AutoExposureSpeedUp = FCString::Atof(*Params);
	}
	else if (TryMatch(Command, TEXT("EXPO_SpeedDown_"), Params))
	{
		PP.bOverride_AutoExposureSpeedDown = true;
		PP.AutoExposureSpeedDown = FCString::Atof(*Params);
	}

	// ==== FILM GRAIN ====
	else if (TryMatch(Command, TEXT("GRAIN_GrainInten_"), Params))
	{
		PP.bOverride_FilmGrainIntensity = true;
		PP.FilmGrainIntensity = FCString::Atof(*Params);
	}
	else if (TryMatch(Command, TEXT("GRAIN_GrainShadow_"), Params))
	{
		PP.bOverride_FilmGrainIntensityShadows = true;
		PP.FilmGrainIntensityShadows = FCString::Atof(*Params);
	}
	else if (TryMatch(Command, TEXT("GRAIN_GrainMidtones_"), Params))
	{
		PP.bOverride_FilmGrainIntensityMidtones = true;
		PP.FilmGrainIntensityMidtones = FCString::Atof(*Params);
	}
	else if (TryMatch(Command, TEXT("GRAIN_GrainHighlight_"), Params))
	{
		PP.bOverride_FilmGrainIntensityHighlights = true;
		PP.FilmGrainIntensityHighlights = FCString::Atof(*Params);
	}
	else if (TryMatch(Command, TEXT("GRAIN_HighlightsMax_"), Params))
	{
		PP.bOverride_FilmGrainHighlightsMax = true;
		PP.FilmGrainHighlightsMax = FCString::Atof(*Params);
	}
	else if (TryMatch(Command, TEXT("GRAIN_HighlightsMin_"), Params))
	{
		PP.bOverride_FilmGrainHighlightsMin = true;
		PP.FilmGrainHighlightsMin = FCString::Atof(*Params);
	}
	else if (TryMatch(Command, TEXT("GRAIN_ShadowMax_"), Params))
	{
		PP.bOverride_FilmGrainShadowsMax = true;
		PP.FilmGrainShadowsMax = FCString::Atof(*Params);
	}
	else if (TryMatch(Command, TEXT("GRAIN_TexelSize_"), Params))
	{
		PP.bOverride_FilmGrainTexelSize = true;
		PP.FilmGrainTexelSize = FCString::Atof(*Params);
	}

	// ==== VIGNETTE ====
	else if (TryMatch(Command, TEXT("VIG_"), Params))
	{
		PP.bOverride_VignetteIntensity = true;
		PP.VignetteIntensity = FCString::Atof(*Params);
	}

	// ==== SHARPEN ====
	else if (TryMatch(Command, TEXT("SHARP_"), Params))
	{
		PP.bOverride_Sharpen = true;
		PP.Sharpen = FCString::Atof(*Params);
	}

	// ==== WHITE BALANCE ====
	else if (TryMatch(Command, TEXT("CG_Temp_"), Params))
	{
		PP.bOverride_WhiteTemp = true;
		PP.WhiteTemp = FCString::Atof(*Params);
	}
	else if (TryMatch(Command, TEXT("CG_Tint_"), Params))
	{
		PP.bOverride_WhiteTint = true;
		PP.WhiteTint = FCString::Atof(*Params);
	}

	// ==== CGG — Color Grading Group (specific sub-commands first) ====
	else if (Command.StartsWith(TEXT("CGG_")))
	{
		ApplyCGGCommand(PP, Command);
	}

	else
	{
		UE_LOG(LogEmbodyPP, Warning, TEXT("Unknown PP command: %s"), *Command);
	}
}

// ---------------------------------------------------------------------------
// ApplyCGGCommand — handles all CGG_ prefixed commands
// ---------------------------------------------------------------------------
void UEmbodyPostProcessBPLibrary::ApplyCGGCommand(FPostProcessSettings& PP, const FString& Command)
{
	FString Params;

	// ---- Shadows/Highlights correction (check BEFORE general color grading) ----
	if (TryMatch(Command, TEXT("CGG_Shadows_Max_"), Params))
	{
		PP.bOverride_ColorCorrectionShadowsMax = true;
		PP.ColorCorrectionShadowsMax = FCString::Atof(*Params);
	}
	else if (TryMatch(Command, TEXT("CGG_Highlights_Max_"), Params))
	{
		PP.bOverride_ColorCorrectionHighlightsMax = true;
		PP.ColorCorrectionHighlightsMax = FCString::Atof(*Params);
	}
	else if (TryMatch(Command, TEXT("CGG_Highlights_Min_"), Params))
	{
		PP.bOverride_ColorCorrectionHighlightsMin = true;
		PP.ColorCorrectionHighlightsMin = FCString::Atof(*Params);
	}

	// ---- Lens Flare ----
	else if (TryMatch(Command, TEXT("CGG_LenseFlare_Intensity_"), Params))
	{
		PP.bOverride_LensFlareIntensity = true;
		PP.LensFlareIntensity = FCString::Atof(*Params);
	}
	else if (TryMatch(Command, TEXT("CGG_LenseFlare_Tint_"), Params))
	{
		TArray<float> V = ParseFloats(Params);
		PP.bOverride_LensFlareTint = true;
		PP.LensFlareTint = FLinearColor(
			V.IsValidIndex(0) ? V[0] : 1.f,
			V.IsValidIndex(1) ? V[1] : 1.f,
			V.IsValidIndex(2) ? V[2] : 1.f,
			V.IsValidIndex(3) ? V[3] : 1.f);
	}
	else if (TryMatch(Command, TEXT("CGG_LenseFlare_BokehSize_"), Params))
	{
		PP.bOverride_LensFlareBokehSize = true;
		PP.LensFlareBokehSize = FCString::Atof(*Params);
	}
	else if (TryMatch(Command, TEXT("CGG_LenseFlare_Threshold_"), Params))
	{
		PP.bOverride_LensFlareThreshold = true;
		PP.LensFlareThreshold = FCString::Atof(*Params);
	}

	// ---- Misc / Tonemapper ----
	else if (TryMatch(Command, TEXT("CGG_Misc_FilmSlope_"), Params))
	{
		PP.bOverride_FilmSlope = true;
		PP.FilmSlope = FCString::Atof(*Params);
	}
	else if (TryMatch(Command, TEXT("CGG_Misc_FilmToe_"), Params))
	{
		PP.bOverride_FilmToe = true;
		PP.FilmToe = FCString::Atof(*Params);
	}
	else if (TryMatch(Command, TEXT("CGG_Misc_FilmShoulder_"), Params))
	{
		PP.bOverride_FilmShoulder = true;
		PP.FilmShoulder = FCString::Atof(*Params);
	}
	else if (TryMatch(Command, TEXT("CGG_Misc_FilmBlackClip_"), Params))
	{
		PP.bOverride_FilmBlackClip = true;
		PP.FilmBlackClip = FCString::Atof(*Params);
	}
	else if (TryMatch(Command, TEXT("CGG_Misc_FilmWhiteClip_"), Params))
	{
		PP.bOverride_FilmWhiteClip = true;
		PP.FilmWhiteClip = FCString::Atof(*Params);
	}
	else if (TryMatch(Command, TEXT("CGG_Misc_ToneCurveAmount_"), Params))
	{
		PP.bOverride_ToneCurveAmount = true;
		PP.ToneCurveAmount = FCString::Atof(*Params);
	}
	else if (TryMatch(Command, TEXT("CGG_Misc_LUTintensity_"), Params))
	{
		PP.bOverride_ColorGradingIntensity = true;
		PP.ColorGradingIntensity = FCString::Atof(*Params);
	}

	// ---- Dirt Mask ----
	else if (TryMatch(Command, TEXT("CGG_Dirt_Intensity_"), Params))
	{
		PP.bOverride_BloomDirtMaskIntensity = true;
		PP.BloomDirtMaskIntensity = FCString::Atof(*Params);
	}

	// ---- Local Exposure ----
	else if (TryMatch(Command, TEXT("CGG_LenseExpo_HighlightContrast_"), Params))
	{
		PP.bOverride_LocalExposureHighlightContrastScale = true;
		PP.LocalExposureHighlightContrastScale = FCString::Atof(*Params);
	}
	else if (TryMatch(Command, TEXT("CGG_LenseExpo_ShadowContrast_"), Params))
	{
		PP.bOverride_LocalExposureShadowContrastScale = true;
		PP.LocalExposureShadowContrastScale = FCString::Atof(*Params);
	}
	else if (TryMatch(Command, TEXT("CGG_LenseExpo_DetailStrength_"), Params))
	{
		PP.bOverride_LocalExposureDetailStrength = true;
		PP.LocalExposureDetailStrength = FCString::Atof(*Params);
	}
	else if (TryMatch(Command, TEXT("CGG_LenseExpo_BlurredLumiBlend_"), Params))
	{
		PP.bOverride_LocalExposureBlurredLuminanceBlend = true;
		PP.LocalExposureBlurredLuminanceBlend = FCString::Atof(*Params);
	}

	// ---- General Color Grading: CGG_{Level}_{Property}_{R}_{G}_{B}[_{A}] ----
	else
	{
		// Remove "CGG_" prefix
		FString Rest = Command.RightChop(4);

		// Determine Level
		FString Level;
		if      (Rest.StartsWith(TEXT("Global_")))     { Level = TEXT("Global");     Rest = Rest.RightChop(7); }
		else if (Rest.StartsWith(TEXT("Shadows_")))    { Level = TEXT("Shadows");    Rest = Rest.RightChop(8); }
		else if (Rest.StartsWith(TEXT("Midtones_")))   { Level = TEXT("Midtones");   Rest = Rest.RightChop(9); }
		else if (Rest.StartsWith(TEXT("Highlights_"))) { Level = TEXT("Highlights"); Rest = Rest.RightChop(11); }

		if (Level.IsEmpty())
		{
			UE_LOG(LogEmbodyPP, Warning, TEXT("Unknown CGG command: %s"), *Command);
			return;
		}

		// Determine Property and extract params
		FString Property;
		if      (Rest.StartsWith(TEXT("Saturation_"))) { Property = TEXT("Saturation"); Rest = Rest.RightChop(11); }
		else if (Rest.StartsWith(TEXT("Contrast_")))   { Property = TEXT("Contrast");   Rest = Rest.RightChop(9); }
		else if (Rest.StartsWith(TEXT("Gamma_")))      { Property = TEXT("Gamma");      Rest = Rest.RightChop(6); }
		else if (Rest.StartsWith(TEXT("Gain_")))       { Property = TEXT("Gain");       Rest = Rest.RightChop(5); }
		else if (Rest.StartsWith(TEXT("Offset_")))     { Property = TEXT("Offset");     Rest = Rest.RightChop(7); }

		if (Property.IsEmpty())
		{
			UE_LOG(LogEmbodyPP, Warning, TEXT("Unknown CGG property in: %s"), *Command);
			return;
		}

		TArray<float> Values = ParseFloats(Rest);
		ApplyColorGrading(PP, Level, Property, Values);
	}
}

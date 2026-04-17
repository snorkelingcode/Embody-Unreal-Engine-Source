// EmbodyPostProcessBPLibrary.h
// Unified command processor for TCP -> CineCamera PostProcessSettings.
// Two nodes: IsPostProcessCommand (detect) and ApplyPostProcessCommand (apply).

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "EmbodyPostProcessBPLibrary.generated.h"

class UCineCameraComponent;
struct FPostProcessSettings;

UCLASS()
class EMBODY_API UEmbodyPostProcessBPLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	/** Check if a raw TCP command string is a post-process command.
	 *  Returns true if the string starts with any known PP prefix. */
	UFUNCTION(BlueprintCallable, Category = "Embody|Post Process",
		meta = (DisplayName = "Is Post Process Command"))
	static bool IsPostProcessCommand(const FString& Command);

	/** Parse a post-process command and apply it to a CineCamera's PostProcessSettings.
	 *  Handles bloom, exposure, color grading, film grain, vignette, etc. */
	UFUNCTION(BlueprintCallable, Category = "Embody|Post Process",
		meta = (DisplayName = "Apply Post Process Command"))
	static void ApplyPostProcessCommand(UCineCameraComponent* CineCamera, const FString& Command);

private:

	/** Parse underscore-delimited floats from a parameter string. */
	static TArray<float> ParseFloats(const FString& ParamString);

	/** Build an FVector4 from parsed floats, with a default W value. */
	static FVector4 MakeVec4(const TArray<float>& V, float DefaultW = 1.0f);

	/** Apply a color grading vector (Saturation/Contrast/Gamma/Gain/Offset)
	 *  to the correct PostProcessSettings field for the given level. */
	static void ApplyColorGrading(FPostProcessSettings& PP,
		const FString& Level, const FString& Property, const TArray<float>& Values);

	/** Handle all CGG_ prefixed commands (color grading group, lens flare, misc, etc). */
	static void ApplyCGGCommand(FPostProcessSettings& PP, const FString& Command);
};

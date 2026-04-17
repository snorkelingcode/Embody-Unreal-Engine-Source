// EmbodyEmotionBPLibrary.h
// TCP command processor for ZenBlink emotion/face parameters.
//
// Command format:
//   EMOTION_{Emotion}_{Strength}_{Blend}_{BlendMode}

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "EmbodyEmotionBPLibrary.generated.h"

class UZenBlinkComponent;

UCLASS()
class EMBODY_API UEmbodyEmotionBPLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	/** Check if a raw TCP command string is an emotion command. */
	UFUNCTION(BlueprintCallable, Category = "Embody|Emotion",
		meta = (DisplayName = "Is Emotion Command"))
	static bool IsEmotionCommand(const FString& Command);

	/** Parse and apply an emotion command to a ZenBlink component.
	 *  Format: EMOTION_{Emotion}_{Strength}_{Blend}_{BlendMode}
	 *  @param ZenBlink  - The ZenBlink component to modify
	 *  @param Command   - Raw TCP string */
	UFUNCTION(BlueprintCallable, Category = "Embody|Emotion",
		meta = (DisplayName = "Apply Emotion Command"))
	static void ApplyEmotionCommand(UZenBlinkComponent* ZenBlink, const FString& Command);
};

// EmbodyStringBPLibrary.h
// String parsing utilities for BYOB commands with file paths.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "EmbodyStringBPLibrary.generated.h"

UCLASS()
class EMBODY_API UEmbodyStringBPLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	/** Parses a BYOB parameter string by underscore, but treats file paths
	 *  (containing .mp3, .wav, .ogg, .flac) as a single token.
	 *  Underscores inside the file path are preserved; parsing resumes
	 *  after the first underscore following the file extension.
	 *
	 *  Example: "C:\\path\\file_name.mp3_param1_param2"
	 *  Returns: ["C:\\path\\file_name.mp3", "param1", "param2"]
	 *
	 *  @param Input  - The string to parse (typically everything after TTS_BYOB_)
	 *  @return Array of parsed tokens */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Embody|String",
		meta = (DisplayName = "BYOB Parse Into Array"))
	static TArray<FString> BYOBParseIntoArray(const FString& Input);
};

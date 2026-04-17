// EmbodyStringBPLibrary.cpp

#include "EmbodyStringBPLibrary.h"

TArray<FString> UEmbodyStringBPLibrary::BYOBParseIntoArray(const FString& Input)
{
	TArray<FString> Result;

	if (Input.IsEmpty())
	{
		return Result;
	}

	// Audio file extensions to look for (case-insensitive)
	static const TArray<FString> AudioExtensions = {
		TEXT(".mp3"), TEXT(".wav"), TEXT(".ogg"), TEXT(".flac")
	};

	// Search for a known audio extension in the input
	FString LowerInput = Input.ToLower();
	int32 ExtEnd = INDEX_NONE;

	for (const FString& Ext : AudioExtensions)
	{
		int32 Found = LowerInput.Find(Ext);
		if (Found != INDEX_NONE)
		{
			ExtEnd = Found + Ext.Len();
			break;
		}
	}

	if (ExtEnd == INDEX_NONE)
	{
		// No audio extension found — fall back to normal underscore parsing
		Input.ParseIntoArray(Result, TEXT("_"), true);
		return Result;
	}

	// Split at the first underscore to get the prefix token (e.g., "BYOB")
	int32 FirstUnderscore = INDEX_NONE;
	Input.FindChar(TEXT('_'), FirstUnderscore);

	if (FirstUnderscore != INDEX_NONE && FirstUnderscore < ExtEnd)
	{
		// Token[0]: everything before the first underscore (e.g., "BYOB")
		Result.Add(Input.Left(FirstUnderscore));

		// Token[1]: file path from after the first underscore to the end of the extension
		Result.Add(Input.Mid(FirstUnderscore + 1, ExtEnd - FirstUnderscore - 1));
	}
	else
	{
		// No underscore before the extension — entire left portion is the file path
		Result.Add(Input.Left(ExtEnd));
	}

	// Parse anything remaining after the extension
	if (ExtEnd < Input.Len() && Input[ExtEnd] == TEXT('_'))
	{
		FString Remainder = Input.Mid(ExtEnd + 1);
		if (!Remainder.IsEmpty())
		{
			TArray<FString> Tail;
			Remainder.ParseIntoArray(Tail, TEXT("_"), true);
			Result.Append(Tail);
		}
	}

	return Result;
}

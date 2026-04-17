// EmbodyEmotionBPLibrary.cpp
// TCP command processor for ZenBlink emotion/face parameters.
//
// Command format:
//   EMOTION_{Emotion}_{Strength}_{Blend}_{BlendMode}
//
//   Emotion   — enum name (Neutral, Happy, Sad, Surprised, etc.)
//   Strength  — FaceEmotionStrength 0.0–1.0
//   Blend     — FaceAnimationBlend  0.0–1.0
//   BlendMode — Override or BlendByWeight

#include "EmbodyEmotionBPLibrary.h"
#include "ZenBlinkComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogEmbodyEmotion, Log, All);

// ---------------------------------------------------------------------------
// Emotion name → EZenEmotion mapping
// ---------------------------------------------------------------------------
static bool ParseEmotion(const FString& Name, EZenEmotion& OutEmotion)
{
	static const TMap<FString, EZenEmotion> Map = {
		{ TEXT("Neutral"),      EZenEmotion::Neutral },
		{ TEXT("Happy"),        EZenEmotion::Happy },
		{ TEXT("Sad"),          EZenEmotion::Sad },
		{ TEXT("Surprised"),    EZenEmotion::Surprised },
		{ TEXT("Fearful"),      EZenEmotion::Fearful },
		{ TEXT("Focused"),      EZenEmotion::Focused },
		{ TEXT("Disgusted"),    EZenEmotion::Disgusted },
		{ TEXT("Childish"),     EZenEmotion::Childish },
		{ TEXT("Tired"),        EZenEmotion::Tired },
		{ TEXT("Annoyed"),      EZenEmotion::Annoyed },
		{ TEXT("Confused"),     EZenEmotion::Confused },
		{ TEXT("Curious"),      EZenEmotion::Curious },
		{ TEXT("Embarrassed"),  EZenEmotion::Embarrassed },
		{ TEXT("Angry"),        EZenEmotion::Angry },
		{ TEXT("Bored"),        EZenEmotion::Bored },
		{ TEXT("Excited"),      EZenEmotion::Excited },
		{ TEXT("Relaxed"),      EZenEmotion::Relaxed },
		{ TEXT("Suspicious"),   EZenEmotion::Suspicious },
		{ TEXT("Proud"),        EZenEmotion::Proud },
		{ TEXT("Pained"),       EZenEmotion::Pained },
		{ TEXT("Nervous"),      EZenEmotion::Nervous },
		{ TEXT("Love"),         EZenEmotion::Love }
	};

	if (const EZenEmotion* Found = Map.Find(Name))
	{
		OutEmotion = *Found;
		return true;
	}
	return false;
}

// ---------------------------------------------------------------------------
// BlendMode name → EZenBlinkAnimBlendMode mapping
// ---------------------------------------------------------------------------
static bool ParseBlendMode(const FString& Name, EZenBlinkAnimBlendMode& OutMode)
{
	if (Name.Equals(TEXT("Override"), ESearchCase::IgnoreCase))
	{
		OutMode = EZenBlinkAnimBlendMode::Override;
		return true;
	}
	if (Name.Equals(TEXT("BlendByWeight"), ESearchCase::IgnoreCase))
	{
		OutMode = EZenBlinkAnimBlendMode::BlendByWeight;
		return true;
	}
	return false;
}

// ---------------------------------------------------------------------------
// IsEmotionCommand
// ---------------------------------------------------------------------------
bool UEmbodyEmotionBPLibrary::IsEmotionCommand(const FString& Command)
{
	return Command.StartsWith(TEXT("EMOTION_"));
}

// ---------------------------------------------------------------------------
// ApplyEmotionCommand
// Format: EMOTION_{Emotion}_{Strength}_{Blend}_{BlendMode}
//   Strength and Blend are floats 0.0–1.0
//   BlendMode is Override or BlendByWeight
// ---------------------------------------------------------------------------
void UEmbodyEmotionBPLibrary::ApplyEmotionCommand(
	UZenBlinkComponent* ZenBlink,
	const FString& Command)
{
	if (!ZenBlink)
	{
		UE_LOG(LogEmbodyEmotion, Warning, TEXT("ApplyEmotionCommand: null ZenBlink component"));
		return;
	}

	if (!Command.StartsWith(TEXT("EMOTION_")))
	{
		UE_LOG(LogEmbodyEmotion, Warning, TEXT("Not an emotion command: %s"), *Command);
		return;
	}

	// Strip prefix and parse: Emotion_Strength_Blend_BlendMode
	FString ParamString = Command.RightChop(8); // len("EMOTION_") = 8
	TArray<FString> Parts;
	ParamString.ParseIntoArray(Parts, TEXT("_"));

	if (Parts.Num() < 4)
	{
		UE_LOG(LogEmbodyEmotion, Warning,
			TEXT("EMOTION needs 4 params (Emotion_Strength_Blend_BlendMode), got %d: %s"),
			Parts.Num(), *Command);
		return;
	}

	// Parse emotion enum
	EZenEmotion Emotion;
	if (!ParseEmotion(Parts[0], Emotion))
	{
		UE_LOG(LogEmbodyEmotion, Warning, TEXT("Unknown emotion: %s"), *Parts[0]);
		return;
	}

	// Parse strength (0.0–1.0)
	const float Strength = FMath::Clamp(FCString::Atof(*Parts[1]), 0.f, 1.f);

	// Parse blend (0.0–1.0)
	const float Blend = FMath::Clamp(FCString::Atof(*Parts[2]), 0.f, 1.f);

	// Parse blend mode
	EZenBlinkAnimBlendMode BlendMode;
	if (!ParseBlendMode(Parts[3], BlendMode))
	{
		UE_LOG(LogEmbodyEmotion, Warning, TEXT("Unknown blend mode: %s (use Override or BlendByWeight)"), *Parts[3]);
		return;
	}

	// Apply to ZenBlink component
	ZenBlink->FaceAnimation = true;
	ZenBlink->Emotion = Emotion;
	ZenBlink->FaceEmotionStrength = Strength;
	ZenBlink->FaceAnimationBlend = Blend;
	ZenBlink->FaceBlendMode = BlendMode;

	UE_LOG(LogEmbodyEmotion, Log, TEXT("EMOTION — %s Strength=%.2f Blend=%.2f Mode=%s"),
		*Parts[0], Strength, Blend, *Parts[3]);
}

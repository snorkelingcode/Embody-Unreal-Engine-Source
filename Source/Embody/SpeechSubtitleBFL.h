// SpeechSubtitleBFL.h
// Blueprint Function Library for subtitle display.
//
// Blueprint flow (TTS — text known):
//   1. SetSpeechText(Actor, Text)              — when TTS starts
//   2. NotifySpeechPCMChunk(Actor, ...)        — on each OnGeneratePCMData_Event
//   3. GetCurrentSubtitle(Actor)               — read the visible text
//   4. ClearSubtitle(Actor)                    — on OnAudioPlaybackFinished
//
// Blueprint flow (Whisper — text unknown):
//   1. FeedWhisperAudio(Actor, PCM, ...)       — each time decoded audio is available
//   2. EndWhisperSubtitle(Actor)               — when all audio has been fed
//   (subtitles appear automatically via WB_Subtitles widget)

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "WhisperTranscriber.h"
#include "SpeechSubtitleBFL.generated.h"

UCLASS()
class EMBODY_API USpeechSubtitleBFL : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	// ── TTS Subtitle API (text known) ──

	/** Store the speech text for an actor. Call once when TTS starts.
	 *  Splits into words and prepares for playback-driven reveal. */
	UFUNCTION(BlueprintCallable, Category = "Subtitle",
		meta = (DisplayName = "Set Speech Text"))
	static void SetSpeechText(AActor* Speaker, const FString& Text);

	/** Feed real-time PCM data from OnGeneratePCMData_Event (TTS path). */
	UFUNCTION(BlueprintCallable, Category = "Subtitle",
		meta = (DisplayName = "Notify Speech PCM Chunk"))
	static void NotifySpeechPCMChunk(
		AActor* Speaker,
		const TArray<float>& PCMData,
		int32 SampleRate,
		int32 NumChannels);

	/** Returns the words revealed so far (space-separated). */
	UFUNCTION(BlueprintPure, Category = "Subtitle",
		meta = (DisplayName = "Get Current Subtitle"))
	static FString GetCurrentSubtitle(AActor* Speaker);

	/** Returns the full original text (TTS path only). */
	UFUNCTION(BlueprintPure, Category = "Subtitle",
		meta = (DisplayName = "Get Full Speech Text"))
	static FString GetFullSpeechText(AActor* Speaker);

	/** Make all remaining TTS words visible. */
	UFUNCTION(BlueprintCallable, Category = "Subtitle",
		meta = (DisplayName = "Finish Subtitle"))
	static void FinishSubtitle(AActor* Speaker);

	/** Clear TTS subtitle state for an actor. */
	UFUNCTION(BlueprintCallable, Category = "Subtitle",
		meta = (DisplayName = "Clear Subtitle"))
	static void ClearSubtitle(AActor* Speaker);

	/** Whether a subtitle is active for this actor. */
	UFUNCTION(BlueprintPure, Category = "Subtitle",
		meta = (DisplayName = "Is Subtitle Active"))
	static bool IsSubtitleActive(AActor* Speaker);

	// ── Whisper Subtitle API (text unknown) ──

	/** Feed decoded audio data to whisper for transcription.
	 *  Auto-creates the whisper session on first call per speaker.
	 *  @param Speaker - The actor speaking
	 *  @param PCMData - Decoded float PCM samples
	 *  @param SampleRate - Sample rate of the decoded audio (e.g. 44100)
	 *  @param NumChannels - Number of channels (e.g. 1 or 2)
	 *  @param InitialPrompt - Optional: known text to bias recognition (e.g. ElevenLabs TTS text) */
	UFUNCTION(BlueprintCallable, Category = "Subtitle",
		meta = (DisplayName = "Feed Whisper Audio"))
	static void FeedWhisperAudio(
		AActor* Speaker,
		const TArray<float>& PCMData,
		int32 SampleRate,
		int32 NumChannels,
		const FString& InitialPrompt);

	/** Signal that all audio has been fed. Whisper will finish processing
	 *  and the full transcription will be pushed to WB_Subtitles automatically.
	 *  @param Speaker - The actor speaking */
	UFUNCTION(BlueprintCallable, Category = "Subtitle",
		meta = (DisplayName = "End Whisper Subtitle"))
	static void EndWhisperSubtitle(AActor* Speaker);

private:

	struct FSpeechState
	{
		FString FullText;
		FString VisibleText;
		TArray<FString> Words;
		int32 WordsRevealed = 0;

		float AccumulatedPlaybackSeconds = 0.f;
		float EstimatedTotalSeconds = 0.f;
		float SecondsPerWord = 0.f;

		bool bActive = false;
		bool bPlaybackStarted = false;

		// Whisper mode
		bool bWhisperMode = false;
		int32 DrainPollsWithNoWords = 0;
		FTimerHandle PollTimerHandle;   // live polling during playback
		FTimerHandle DrainTimerHandle;  // final drain after EndWhisperSubtitle
		FTimerHandle ClearDisplayTimerHandle;  // delayed subtitle clear
		TSharedPtr<FWhisperTranscriber> Transcriber;
	};

	static TMap<TWeakObjectPtr<AActor>, FSpeechState> States;

	/** Poll whisper for the latest full transcription result.
	 *  Returns true if the visible text changed. */
	static bool PollWhisperResult(FSpeechState& State);

	/** Find the WB_Subtitles widget and call DisplaySubtitle(bTalking, Text). */
	static void PushToSubtitleWidget(UWorld* World, bool bTalking, const FString& Text);

	/** Timer callback: polls whisper for results after EndWhisperSubtitle. */
	static void DrainWhisperWords(TWeakObjectPtr<AActor> WeakSpeaker);
};

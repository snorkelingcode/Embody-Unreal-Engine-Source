// SpeechSubtitleBFL.cpp

#include "SpeechSubtitleBFL.h"
#include "GameFramework/Actor.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogSpeechSubtitle, Log, All);

// Static state map
TMap<TWeakObjectPtr<AActor>, USpeechSubtitleBFL::FSpeechState> USpeechSubtitleBFL::States;

// ─────────────────────────────────────────────────────────────────────────────
//  TTS Path: SetSpeechText
// ─────────────────────────────────────────────────────────────────────────────

void USpeechSubtitleBFL::SetSpeechText(AActor* Speaker, const FString& Text)
{
	if (!Speaker || Text.IsEmpty())
	{
		return;
	}

	FSpeechState State;
	State.FullText = Text;
	Text.ParseIntoArray(State.Words, TEXT(" "), true);

	if (State.Words.Num() == 0)
	{
		return;
	}

	State.bActive = true;
	State.WordsRevealed = 0;
	State.AccumulatedPlaybackSeconds = 0.f;
	State.bPlaybackStarted = false;

	State.EstimatedTotalSeconds = State.Words.Num() * 0.35f;
	State.SecondsPerWord = 0.35f;

	States.Add(Speaker, MoveTemp(State));

	UE_LOG(LogSpeechSubtitle, Log, TEXT("SetSpeechText [%s]: %d words, est %.1fs"),
		*Speaker->GetName(), States[Speaker].Words.Num(),
		States[Speaker].EstimatedTotalSeconds);
}

// ─────────────────────────────────────────────────────────────────────────────
//  TTS Path: NotifySpeechPCMChunk
// ─────────────────────────────────────────────────────────────────────────────

void USpeechSubtitleBFL::NotifySpeechPCMChunk(
	AActor* Speaker,
	const TArray<float>& PCMData,
	int32 SampleRate,
	int32 NumChannels)
{
	if (!Speaker)
	{
		return;
	}

	FSpeechState* State = States.Find(Speaker);
	if (!State || !State->bActive || State->bWhisperMode)
	{
		return;
	}

	if (!State->bPlaybackStarted)
	{
		State->bPlaybackStarted = true;
		UE_LOG(LogSpeechSubtitle, Log, TEXT("Playback started [%s]."),
			*Speaker->GetName());
	}

	// TTS mode: reveal words by playback timing
	if (SampleRate > 0 && NumChannels > 0 && PCMData.Num() > 0)
	{
		const int32 NumFrames = PCMData.Num() / NumChannels;
		const float ChunkSeconds = static_cast<float>(NumFrames) / SampleRate;
		State->AccumulatedPlaybackSeconds += ChunkSeconds;
	}

	const float Progress = (State->EstimatedTotalSeconds > 0.f)
		? State->AccumulatedPlaybackSeconds / State->EstimatedTotalSeconds
		: 0.f;

	int32 TargetRevealed = FMath::FloorToInt32(Progress * State->Words.Num()) + 1;
	TargetRevealed = FMath::Clamp(TargetRevealed, 0, State->Words.Num());

	if (TargetRevealed > State->WordsRevealed)
	{
		State->WordsRevealed = TargetRevealed;

		// Rebuild visible text
		State->VisibleText.Empty();
		for (int32 i = 0; i < State->WordsRevealed && i < State->Words.Num(); ++i)
		{
			if (i > 0) State->VisibleText += TEXT(" ");
			State->VisibleText += State->Words[i];
		}
	}
}

// ─────────────────────────────────────────────────────────────────────────────
//  Getters
// ─────────────────────────────────────────────────────────────────────────────

FString USpeechSubtitleBFL::GetCurrentSubtitle(AActor* Speaker)
{
	if (!Speaker)
	{
		return FString();
	}

	FSpeechState* State = States.Find(Speaker);
	if (!State || !State->bActive)
	{
		return FString();
	}

	return State->VisibleText;
}

FString USpeechSubtitleBFL::GetFullSpeechText(AActor* Speaker)
{
	if (!Speaker)
	{
		return FString();
	}

	const FSpeechState* State = States.Find(Speaker);
	return State ? State->FullText : FString();
}

// ─────────────────────────────────────────────────────────────────────────────
//  TTS State management
// ─────────────────────────────────────────────────────────────────────────────

void USpeechSubtitleBFL::FinishSubtitle(AActor* Speaker)
{
	if (!Speaker)
	{
		return;
	}

	FSpeechState* State = States.Find(Speaker);
	if (!State || !State->bActive || State->bWhisperMode)
	{
		return;
	}

	// TTS mode: reveal any remaining words
	if (State->WordsRevealed < State->Words.Num())
	{
		State->WordsRevealed = State->Words.Num();

		State->VisibleText.Empty();
		for (int32 i = 0; i < State->Words.Num(); ++i)
		{
			if (i > 0) State->VisibleText += TEXT(" ");
			State->VisibleText += State->Words[i];
		}

		UE_LOG(LogSpeechSubtitle, Log,
			TEXT("FinishSubtitle [%s]: revealed all %d words (playback %.2fs)."),
			*Speaker->GetName(), State->Words.Num(),
			State->AccumulatedPlaybackSeconds);
	}
}

void USpeechSubtitleBFL::ClearSubtitle(AActor* Speaker)
{
	if (!Speaker)
	{
		return;
	}

	FSpeechState* State = States.Find(Speaker);
	if (!State)
	{
		return;
	}

	// Whisper mode is managed by EndWhisperSubtitle — don't interfere
	if (State->bWhisperMode)
	{
		UE_LOG(LogSpeechSubtitle, Log,
			TEXT("ClearSubtitle [%s]: ignored (whisper mode, managed by EndWhisperSubtitle)."),
			*Speaker->GetName());
		return;
	}

	States.Remove(Speaker);
}

bool USpeechSubtitleBFL::IsSubtitleActive(AActor* Speaker)
{
	if (!Speaker)
	{
		return false;
	}

	const FSpeechState* State = States.Find(Speaker);
	return State && State->bActive;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Whisper Path: FeedWhisperAudio
// ─────────────────────────────────────────────────────────────────────────────

void USpeechSubtitleBFL::FeedWhisperAudio(
	AActor* Speaker,
	const TArray<float>& PCMData,
	int32 SampleRate,
	int32 NumChannels,
	const FString& InitialPrompt)
{
	if (!Speaker || PCMData.Num() == 0)
	{
		return;
	}

	FSpeechState* State = States.Find(Speaker);

	// Auto-create whisper session on first call
	if (!State || !State->bWhisperMode)
	{
		FSpeechState NewState;
		NewState.bActive = true;
		NewState.bWhisperMode = true;
		NewState.Transcriber = MakeShared<FWhisperTranscriber>(SampleRate, NumChannels, InitialPrompt);
		NewState.Transcriber->StartThread();

		States.Add(Speaker, MoveTemp(NewState));
		State = States.Find(Speaker);

		UE_LOG(LogSpeechSubtitle, Log,
			TEXT("===== WHISPER SESSION START [%s] ====="),
			*Speaker->GetName());
		UE_LOG(LogSpeechSubtitle, Log,
			TEXT("  Source: rate=%d ch=%d | prompt: \"%s\""),
			SampleRate, NumChannels,
			InitialPrompt.IsEmpty() ? TEXT("(none)") : *InitialPrompt);

		// Start a live poll timer so subtitles appear DURING playback
		TWeakObjectPtr<AActor> WeakSpeaker(Speaker);
		if (UWorld* World = Speaker->GetWorld())
		{
			World->GetTimerManager().SetTimer(
				State->PollTimerHandle,
				FTimerDelegate::CreateLambda([WeakSpeaker]()
				{
					AActor* S = WeakSpeaker.Get();
					if (!S) return;
					FSpeechState* St = States.Find(S);
					if (!St || !St->bWhisperMode) return;

					if (PollWhisperResult(*St))
					{
						PushToSubtitleWidget(S->GetWorld(), true, St->VisibleText);
					}
				}),
				0.1f,   // poll every 100ms
				true);  // looping
		}
	}

	// Throttle feed logging
	static int32 FeedCallCount = 0;
	FeedCallCount++;
	if (FeedCallCount % 100 == 1)
	{
		UE_LOG(LogSpeechSubtitle, Log,
			TEXT("  [FEED #%d] %d samples @ %dHz %dch"),
			FeedCallCount, PCMData.Num(), SampleRate, NumChannels);
	}

	if (State->Transcriber.IsValid())
	{
		State->Transcriber->EnqueuePCM(PCMData);
	}
}

// ─────────────────────────────────────────────────────────────────────────────
//  Whisper Path: EndWhisperSubtitle
// ─────────────────────────────────────────────────────────────────────────────

void USpeechSubtitleBFL::EndWhisperSubtitle(AActor* Speaker)
{
	if (!Speaker)
	{
		return;
	}

	FSpeechState* State = States.Find(Speaker);
	if (!State || !State->bWhisperMode || !State->Transcriber.IsValid())
	{
		UE_LOG(LogSpeechSubtitle, Warning,
			TEXT("EndWhisperSubtitle [%s]: no active whisper session."),
			Speaker ? *Speaker->GetName() : TEXT("null"));
		return;
	}

	// Stop the live poll timer
	if (UWorld* World = Speaker->GetWorld())
	{
		World->GetTimerManager().ClearTimer(State->PollTimerHandle);
	}

	State->Transcriber->SignalEndOfAudio();
	State->DrainPollsWithNoWords = 0;

	UE_LOG(LogSpeechSubtitle, Log,
		TEXT("===== WHISPER END-OF-AUDIO [%s] | current: \"%s\" ====="),
		*Speaker->GetName(), *State->VisibleText);

	TWeakObjectPtr<AActor> WeakSpeaker(Speaker);
	if (UWorld* World = Speaker->GetWorld())
	{
		World->GetTimerManager().SetTimer(
			State->DrainTimerHandle,
			FTimerDelegate::CreateLambda([WeakSpeaker]()
			{
				DrainWhisperWords(WeakSpeaker);
			}),
			0.033f, // ~30 Hz
			true);  // looping
	}
}

// ─────────────────────────────────────────────────────────────────────────────
//  Whisper drain timer
// ─────────────────────────────────────────────────────────────────────────────

void USpeechSubtitleBFL::DrainWhisperWords(TWeakObjectPtr<AActor> WeakSpeaker)
{
	AActor* Speaker = WeakSpeaker.Get();
	if (!Speaker)
	{
		States.Remove(WeakSpeaker);
		return;
	}

	FSpeechState* State = States.Find(WeakSpeaker);
	if (!State)
	{
		return;
	}

	bool bGotNew = PollWhisperResult(*State);

	if (bGotNew)
	{
		State->DrainPollsWithNoWords = 0;
		PushToSubtitleWidget(Speaker->GetWorld(), true, State->VisibleText);

		UE_LOG(LogSpeechSubtitle, Log,
			TEXT("  [DRAIN] \"%s\""), *State->VisibleText);
	}
	else
	{
		State->DrainPollsWithNoWords++;
	}

	// After ~1s with no new results, whisper is done
	if (State->DrainPollsWithNoWords > 30)
	{
		UE_LOG(LogSpeechSubtitle, Log,
			TEXT("===== WHISPER SESSION COMPLETE [%s] ====="),
			*Speaker->GetName());
		UE_LOG(LogSpeechSubtitle, Log,
			TEXT("  Final: \"%s\" | clearing in 3s"),
			*State->VisibleText);

		// Push final text
		if (!State->VisibleText.IsEmpty())
		{
			PushToSubtitleWidget(Speaker->GetWorld(), true, State->VisibleText);
		}

		// Stop drain timer
		if (UWorld* World = Speaker->GetWorld())
		{
			World->GetTimerManager().ClearTimer(State->DrainTimerHandle);

			// Clear the subtitle display after 3 seconds
			World->GetTimerManager().SetTimer(
				State->ClearDisplayTimerHandle,
				FTimerDelegate::CreateLambda([WeakSpeaker]()
				{
					AActor* S = WeakSpeaker.Get();
					if (S)
					{
						PushToSubtitleWidget(S->GetWorld(), false, FString());
						UE_LOG(LogSpeechSubtitle, Log,
							TEXT("===== WHISPER SUBTITLE CLEARED [%s] ====="),
							*S->GetName());
					}
					States.Remove(WeakSpeaker);
				}),
				3.0f,    // clear after 3 seconds
				false);  // one-shot
		}

		// Clean up transcriber (but keep state alive for the clear timer)
		if (State->Transcriber.IsValid())
		{
			State->Transcriber->Stop();
			State->Transcriber.Reset();
		}
	}
}

// ─────────────────────────────────────────────────────────────────────────────
//  Push subtitle text to WB_Subtitles widget
// ─────────────────────────────────────────────────────────────────────────────

void USpeechSubtitleBFL::PushToSubtitleWidget(
	UWorld* World, bool bTalking, const FString& Text)
{
	if (!World)
	{
		return;
	}

	// Find WB_Subtitles widget class
	static UClass* WidgetClass = nullptr;
	if (!WidgetClass)
	{
		WidgetClass = LoadClass<UUserWidget>(
			nullptr,
			TEXT("/Game/Widgets/WB_Subtitles.WB_Subtitles_C"));

		if (!WidgetClass)
		{
			UE_LOG(LogSpeechSubtitle, Warning,
				TEXT("PushToSubtitleWidget: Could not find /Game/Widgets/WB_Subtitles.WB_Subtitles_C!"));
			return;
		}
	}

	// Find all instances of the widget
	TArray<UUserWidget*> FoundWidgets;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(
		World, FoundWidgets, WidgetClass, true);

	if (FoundWidgets.Num() == 0)
	{
		return;
	}

	for (UUserWidget* Widget : FoundWidgets)
	{
		UFunction* Func = Widget->FindFunction(FName(TEXT("DisplaySubtitle")));
		if (Func)
		{
			struct FDisplaySubtitleParams
			{
				bool isTalking;
				FString Text;
			};
			FDisplaySubtitleParams Params;
			Params.isTalking = bTalking;
			Params.Text = Text;
			Widget->ProcessEvent(Func, &Params);
		}
	}
}

// ─────────────────────────────────────────────────────────────────────────────
//  Poll whisper for latest full result (growing buffer — full replacement)
// ─────────────────────────────────────────────────────────────────────────────

bool USpeechSubtitleBFL::PollWhisperResult(FSpeechState& State)
{
	if (!State.Transcriber.IsValid())
	{
		return false;
	}

	TArray<FTimestampedWord> NewResult;
	if (!State.Transcriber->DequeueResult(NewResult))
	{
		return false;
	}

	// Build visible text from the full result, joining sub-word tokens
	State.Words.Empty();
	for (const FTimestampedWord& TW : NewResult)
	{
		if (!TW.bWordStart && State.Words.Num() > 0)
		{
			// Sub-word token: join to previous word
			State.Words.Last() += TW.Word;
		}
		else
		{
			State.Words.Add(TW.Word);
		}
	}

	// Rebuild visible text — show only the last ~20 words (subtitle window)
	constexpr int32 MaxVisibleWords = 20;
	const int32 StartIdx = FMath::Max(0, State.Words.Num() - MaxVisibleWords);

	State.VisibleText.Empty();
	for (int32 i = StartIdx; i < State.Words.Num(); ++i)
	{
		if (i > StartIdx) State.VisibleText += TEXT(" ");
		State.VisibleText += State.Words[i];
	}

	State.WordsRevealed = State.Words.Num();
	return true;
}

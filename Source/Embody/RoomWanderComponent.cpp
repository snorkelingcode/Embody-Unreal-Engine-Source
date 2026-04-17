// RoomWanderComponent.cpp
//
// ABP_Manny state machine facts (from Aura analysis):
//   - Idle → Walking:     triggered by  isWalking == true  (direct transition)
//   - Walking → Idle:     NO direct transition — Walking is in the "FromEmote"
//                         alias, which transitions to Idle when
//                         Animation == "Speaking"
//   - GroundSpeed, ShouldMove, Velocity: exist but are NEVER read by the ABP
//   - Walking state plays a fixed walk anim (not a blend space)

#include "RoomWanderComponent.h"
#include "WanderPOI.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Kismet/GameplayStatics.h"

DEFINE_LOG_CATEGORY_STATIC(LogWander, Log, All);

URoomWanderComponent::URoomWanderComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Lifecycle
// ─────────────────────────────────────────────────────────────────────────────

void URoomWanderComponent::BeginPlay()
{
	Super::BeginPlay();

	CachedCharacter = Cast<ACharacter>(GetOwner());
	if (!CachedCharacter)
	{
		UE_LOG(LogWander, Error, TEXT("Owner is not ACharacter — disabled."));
		return;
	}

	CachedMovement = CachedCharacter->GetCharacterMovement();
	if (!CachedMovement)
	{
		UE_LOG(LogWander, Error, TEXT("No CharacterMovementComponent — disabled."));
		return;
	}

	if (EnsureAnimInstance())
	{
		UE_LOG(LogWander, Log, TEXT("AnimInstance ready on '%s'."),
			*CachedCharacter->GetName());
	}
	else
	{
		UE_LOG(LogWander, Warning,
			TEXT("AnimInstance not ready — will retry lazily."));
	}

	OriginalMaxWalkSpeed = CachedMovement->MaxWalkSpeed;
	bOriginalOrientToMovement = CachedMovement->bOrientRotationToMovement;
	bOriginalUseControllerDesiredRotation = CachedMovement->bUseControllerDesiredRotation;
	bOriginalUseControllerRotationYaw = CachedCharacter->bUseControllerRotationYaw;

	if (WanderCenter.IsNearlyZero())
	{
		WanderCenter = CachedCharacter->GetActorLocation();
	}

	GatherPOIs();

	if (bAutoStart)
	{
		ResumeWandering();
	}
}

void URoomWanderComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(IdleTimerHandle);
		World->GetTimerManager().ClearTimer(ObstacleCheckHandle);
	}

	RestoreAutoRotation();
	if (CachedMovement)
	{
		CachedMovement->MaxWalkSpeed = OriginalMaxWalkSpeed;
	}

	Super::EndPlay(EndPlayReason);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Tick
// ─────────────────────────────────────────────────────────────────────────────

void URoomWanderComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bIsWandering || !CachedCharacter || !CachedMovement)
	{
		return;
	}

	// --- Settling: smoothly rotate to POI facing direction after arrival ---
	if (CurrentState == EWanderState::Settling)
	{
		SetAnimBool(FName("isWalking"), false);

		const FRotator CurrentRot = CachedCharacter->GetActorRotation();
		const FRotator TargetRot(0.f, SettlingTargetYaw, 0.f);
		const FRotator NewRot = FMath::RInterpTo(
			CurrentRot, TargetRot, DeltaTime, RotationInterpSpeed);
		CachedCharacter->SetActorRotation(FRotator(0.f, NewRot.Yaw, 0.f));

		// Orbit camera while settling
		if (AController* PC = CachedCharacter->GetController())
		{
			const FRotator ControlRot = PC->GetControlRotation();
			const FRotator DesiredControlRot(ControlRot.Pitch, NewRot.Yaw, ControlRot.Roll);
			const FRotator NewControlRot = FMath::RInterpTo(
				ControlRot, DesiredControlRot, DeltaTime, CameraFollowSpeed);
			PC->SetControlRotation(NewControlRot);
		}

		// Check if rotation is close enough to finish settling
		const float YawDelta = FMath::Abs(FRotator::NormalizeAxis(CurrentRot.Yaw - SettlingTargetYaw));
		if (YawDelta < 2.f)
		{
			CachedCharacter->SetActorRotation(TargetRot);
			CurrentState = EWanderState::Idle;
			StartIdleTimerWithDuration(SettlingDwellTime);
		}
		return;
	}

	// --- Idle: reinforce isWalking=false so the ABP stays in Idle ---
	if (CurrentState == EWanderState::Idle)
	{
		SetAnimBool(FName("isWalking"), false);

		// Orbit camera to face the character's front while idle
		if (AController* PC = CachedCharacter->GetController())
		{
			const FRotator ControlRot = PC->GetControlRotation();
			const float DesiredYaw = CachedCharacter->GetActorRotation().Yaw;
			const FRotator DesiredControlRot(ControlRot.Pitch, DesiredYaw, ControlRot.Roll);
			const FRotator NewControlRot = FMath::RInterpTo(
				ControlRot, DesiredControlRot, DeltaTime, CameraFollowSpeed);
			PC->SetControlRotation(NewControlRot);
		}
		return;
	}

	// --- Walking ---

	WalkElapsedTime += DeltaTime;

	if (WalkElapsedTime > WalkTimeout)
	{
		UE_LOG(LogWander, Log, TEXT("Walk timed out (%.1fs)."), WalkElapsedTime);
		CurrentTargetPOI = nullptr;
		StopWalking();
		StartIdleTimer();
		return;
	}

	// --- Stuck detection ---
	const float Speed = CachedMovement->Velocity.Size();
	if (WalkElapsedTime > 1.0f && Speed < StuckSpeedLimit)
	{
		StuckTime += DeltaTime;
		if (StuckTime > StuckThreshold)
		{
			UE_LOG(LogWander, Log, TEXT("Stuck (speed=%.1f for %.2fs). Stopping."),
				Speed, StuckTime);
			CurrentTargetPOI = nullptr;
			StopWalking();
			StartIdleTimer();
			return;
		}
	}
	else
	{
		StuckTime = 0.f;
	}

	const FVector CurrentLoc = CachedCharacter->GetActorLocation();
	const float DistXY = FVector::Dist2D(CurrentLoc, CurrentDestination);

	if (DistXY <= ArrivalTolerance)
	{
		UE_LOG(LogWander, Log, TEXT("Arrived (dist=%.1f)."), DistXY);
		StopWalking();

		if (CurrentTargetPOI)
		{
			// Enter settling state to smoothly rotate to POI facing direction
			SettlingTargetYaw = CurrentTargetPOI->GetArrivalActorYaw();
			SettlingDwellTime = CurrentTargetPOI->DwellTime > 0.f
				? CurrentTargetPOI->DwellTime
				: FMath::FRandRange(MinIdleTime, MaxIdleTime);
			CurrentTargetPOI = nullptr;
			CurrentState = EWanderState::Settling;
			UE_LOG(LogWander, Log, TEXT("Settling to yaw=%.1f."), SettlingTargetYaw);
		}
		else
		{
			const float RandomYawOffset = FMath::FRandRange(-45.f, 45.f);
			FRotator FinalRot = CachedCharacter->GetActorRotation();
			FinalRot.Yaw += RandomYawOffset;
			CachedCharacter->SetActorRotation(FinalRot);
			StartIdleTimer();
		}
		return;
	}

	// --- Move toward destination ---
	const FVector Direction = (CurrentDestination - CurrentLoc).GetSafeNormal2D();

	// Read mesh Yaw offset live (Gameplay event may rotate mesh after spawn).
	float MeshYaw = 0.f;
	if (USkeletalMeshComponent* Mesh = CachedCharacter->GetMesh())
	{
		MeshYaw = Mesh->GetRelativeRotation().Yaw;
	}

	// MetaHuman meshes face along local -X, so add 180° to flip the actor
	// so the visual face points toward the destination. The MeshYaw (-8°
	// at BeginPlay) is a small aesthetic tweak on top of that.
	FRotator TargetRot = Direction.Rotation();
	TargetRot.Yaw += 180.f - MeshYaw;
	const FRotator SmoothedRot = FMath::RInterpTo(
		CachedCharacter->GetActorRotation(),
		TargetRot,
		DeltaTime,
		RotationInterpSpeed);
	CachedCharacter->SetActorRotation(FRotator(0.f, SmoothedRot.Yaw, 0.f));

	// --- Camera face-lock: orbit camera to face the character ---
	if (AController* PC = CachedCharacter->GetController())
	{
		const FRotator ControlRot = PC->GetControlRotation();
		const float DesiredYaw = Direction.Rotation().Yaw;
		const FRotator DesiredControlRot(ControlRot.Pitch, DesiredYaw, ControlRot.Roll);
		const FRotator NewControlRot = FMath::RInterpTo(
			ControlRot, DesiredControlRot, DeltaTime, CameraFollowSpeed);
		PC->SetControlRotation(NewControlRot);
	}

	CachedCharacter->AddMovementInput(Direction, 1.f);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Anim Instance Helpers
// ─────────────────────────────────────────────────────────────────────────────

bool URoomWanderComponent::EnsureAnimInstance()
{
	if (CachedAnimInstance)
	{
		return true;
	}

	if (!CachedCharacter)
	{
		return false;
	}

	CachedAnimInstance = FindAnimInstanceWithProperty(FName("isWalking"));

	if (CachedAnimInstance)
	{
		UE_LOG(LogWander, Log, TEXT("AnimInstance found: %s"),
			*CachedAnimInstance->GetClass()->GetName());

		auto Check = [this](FName N)
		{
			FProperty* P = CachedAnimInstance->GetClass()->FindPropertyByName(N);
			UE_LOG(LogWander, Log, TEXT("  '%s': %s"),
				*N.ToString(), P ? TEXT("FOUND") : TEXT("NOT FOUND"));
		};
		Check(FName("isWalking"));
		Check(FName("Animation"));

		return true;
	}

	return false;
}

UAnimInstance* URoomWanderComponent::FindAnimInstanceWithProperty(FName PropertyName) const
{
	if (!CachedCharacter)
	{
		return nullptr;
	}

	if (USkeletalMeshComponent* MainMesh = CachedCharacter->GetMesh())
	{
		if (UAnimInstance* Inst = MainMesh->GetAnimInstance())
		{
			if (Inst->GetClass()->FindPropertyByName(PropertyName))
			{
				return Inst;
			}
		}
	}

	TArray<USkeletalMeshComponent*> AllMeshes;
	CachedCharacter->GetComponents<USkeletalMeshComponent>(AllMeshes);
	for (USkeletalMeshComponent* Mesh : AllMeshes)
	{
		if (UAnimInstance* Inst = Mesh->GetAnimInstance())
		{
			if (Inst->GetClass()->FindPropertyByName(PropertyName))
			{
				return Inst;
			}
		}
	}

	return nullptr;
}

void URoomWanderComponent::SetAnimBool(FName PropertyName, bool bValue)
{
	if (!EnsureAnimInstance())
	{
		return;
	}

	FProperty* Prop = CachedAnimInstance->GetClass()->FindPropertyByName(PropertyName);
	if (!Prop)
	{
		return;
	}

	if (FBoolProperty* BoolProp = CastField<FBoolProperty>(Prop))
	{
		BoolProp->SetPropertyValue_InContainer(CachedAnimInstance, bValue);
	}
}

void URoomWanderComponent::SetAnimString(FName PropertyName, const FString& Value)
{
	if (!EnsureAnimInstance())
	{
		return;
	}

	FProperty* Prop = CachedAnimInstance->GetClass()->FindPropertyByName(PropertyName);
	if (!Prop)
	{
		return;
	}

	if (FStrProperty* StrProp = CastField<FStrProperty>(Prop))
	{
		StrProp->SetPropertyValue_InContainer(CachedAnimInstance, Value);
	}
}

// ─────────────────────────────────────────────────────────────────────────────
//  Rotation Control
// ─────────────────────────────────────────────────────────────────────────────

void URoomWanderComponent::DisableAutoRotation()
{
	if (bRotationOverrideActive)
	{
		return;
	}

	if (CachedMovement)
	{
		CachedMovement->bOrientRotationToMovement = false;
		CachedMovement->bUseControllerDesiredRotation = false;
	}
	if (CachedCharacter)
	{
		CachedCharacter->bUseControllerRotationYaw = false;
	}

	bRotationOverrideActive = true;
}

void URoomWanderComponent::RestoreAutoRotation()
{
	if (!bRotationOverrideActive)
	{
		return;
	}

	if (CachedMovement)
	{
		CachedMovement->bOrientRotationToMovement = bOriginalOrientToMovement;
		CachedMovement->bUseControllerDesiredRotation = bOriginalUseControllerDesiredRotation;
	}
	if (CachedCharacter)
	{
		CachedCharacter->bUseControllerRotationYaw = bOriginalUseControllerRotationYaw;
	}

	bRotationOverrideActive = false;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Blueprint Interface
// ─────────────────────────────────────────────────────────────────────────────

void URoomWanderComponent::PauseWandering()
{
	UE_LOG(LogWander, Log, TEXT("PauseWandering."));

	bIsWandering = false;
	SetComponentTickEnabled(false);

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(IdleTimerHandle);
		World->GetTimerManager().ClearTimer(ObstacleCheckHandle);
	}

	CurrentTargetPOI = nullptr;
	if (CurrentState == EWanderState::Walking)
	{
		StopWalking();
	}
}

void URoomWanderComponent::ResumeWandering()
{
	if (!CachedCharacter || !CachedMovement)
	{
		return;
	}

	UE_LOG(LogWander, Log, TEXT("ResumeWandering."));

	GatherPOIs();

	bIsWandering = true;
	SetComponentTickEnabled(true);
	CurrentState = EWanderState::Idle;
	StartIdleTimer();
}

void URoomWanderComponent::SetWanderArea(FVector Center, FVector HalfExtents)
{
	WanderCenter = Center;
	WanderHalfExtents = HalfExtents;
}

// ─────────────────────────────────────────────────────────────────────────────
//  State Machine
// ─────────────────────────────────────────────────────────────────────────────

void URoomWanderComponent::StartIdleTimer()
{
	CurrentState = EWanderState::Idle;

	const float IdleTime = FMath::FRandRange(MinIdleTime, MaxIdleTime);
	UE_LOG(LogWander, Log, TEXT("Idle for %.1fs."), IdleTime);

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			IdleTimerHandle,
			this,
			&URoomWanderComponent::OnIdleTimerExpired,
			IdleTime,
			false);
	}
}

void URoomWanderComponent::StartIdleTimerWithDuration(float Duration)
{
	CurrentState = EWanderState::Idle;

	UE_LOG(LogWander, Log, TEXT("Idle for %.1fs."), Duration);

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			IdleTimerHandle,
			this,
			&URoomWanderComponent::OnIdleTimerExpired,
			Duration,
			false);
	}
}

void URoomWanderComponent::OnIdleTimerExpired()
{
	if (!bIsWandering)
	{
		return;
	}

	if (!bAutonomousMode)
	{
		// Non-autonomous: just keep idling until commanded via GoToPOI()
		StartIdleTimer();
		return;
	}

	// Autonomous: pick a weighted POI
	AWanderPOI* ChosenPOI = PickWeightedPOI();
	if (ChosenPOI)
	{
		BeginWalkToPOI(ChosenPOI);
	}
	else
	{
		UE_LOG(LogWander, Log, TEXT("No eligible POIs. Idling."));
		StartIdleTimer();
	}
}

FVector URoomWanderComponent::PickRandomDestination()
{
	const FVector CurrentLoc = CachedCharacter->GetActorLocation();
	UWorld* World = GetWorld();

	// Cardinal directions for wall proximity checks
	static const FVector CardinalDirs[] = {
		FVector(1.f, 0.f, 0.f), FVector(-1.f, 0.f, 0.f),
		FVector(0.f, 1.f, 0.f), FVector(0.f, -1.f, 0.f)
	};

	for (int32 Attempt = 0; Attempt < 10; ++Attempt)
	{
		float MoveDistance;
		if (FMath::FRand() < SmallMoveProbability)
		{
			MoveDistance = FMath::FRandRange(SmallMoveMin, SmallMoveMax);
		}
		else
		{
			MoveDistance = FMath::FRandRange(LargeMoveMin, LargeMoveMax);
		}

		const float AngleRad = FMath::FRandRange(0.f, 2.f * PI);
		const FVector Offset(
			FMath::Cos(AngleRad) * MoveDistance,
			FMath::Sin(AngleRad) * MoveDistance,
			0.f);

		FVector Candidate = CurrentLoc + Offset;

		Candidate.X = FMath::Clamp(Candidate.X,
			WanderCenter.X - WanderHalfExtents.X,
			WanderCenter.X + WanderHalfExtents.X);
		Candidate.Y = FMath::Clamp(Candidate.Y,
			WanderCenter.Y - WanderHalfExtents.Y,
			WanderCenter.Y + WanderHalfExtents.Y);
		Candidate.Z = CurrentLoc.Z;

		// Validate wall proximity in 4 cardinal directions
		bool bTooCloseToWall = false;
		if (World)
		{
			FCollisionQueryParams Params(SCENE_QUERY_STAT(WallCheck), false, CachedCharacter);
			for (const FVector& Dir : CardinalDirs)
			{
				FHitResult Hit;
				if (World->LineTraceSingleByChannel(
					Hit,
					Candidate,
					Candidate + Dir * MinWallDistance,
					ECC_Visibility,
					Params))
				{
					bTooCloseToWall = true;
					break;
				}
			}
		}

		if (!bTooCloseToWall)
		{
			return Candidate;
		}
	}

	// All attempts failed — stay put (will idle in place)
	UE_LOG(LogWander, Log, TEXT("PickRandomDestination: all attempts too close to walls, staying put."));
	return CurrentLoc;
}

void URoomWanderComponent::BeginWalkTo(const FVector& Destination)
{
	CurrentState = EWanderState::Walking;
	CurrentDestination = Destination;
	WalkElapsedTime = 0.f;
	StuckTime = 0.f;

	const float Dist = FVector::Dist2D(CachedCharacter->GetActorLocation(), Destination);
	UE_LOG(LogWander, Log, TEXT("Walking to (%.0f, %.0f) — dist %.0f."),
		Destination.X, Destination.Y, Dist);

	CachedMovement->MaxWalkSpeed = WanderSpeed;

	// Clear the Animation string FIRST so that "Speaking" (left over from
	// the last StopWalking) doesn't immediately trigger FromEmote → Idle
	// the instant we enter the Walking state.
	SetAnimString(FName("Animation"), TEXT(""));

	// isWalking=true triggers the Idle → Walking transition in ABP_Manny.
	SetAnimBool(FName("isWalking"), true);

	DisableAutoRotation();

	// Start forward obstacle check timer
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			ObstacleCheckHandle,
			this,
			&URoomWanderComponent::CheckForwardObstacle,
			0.3f,
			true);
	}
}

void URoomWanderComponent::StopWalking()
{
	UE_LOG(LogWander, Log, TEXT("StopWalking."));

	CurrentState = EWanderState::Idle;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ObstacleCheckHandle);
	}

	if (CachedMovement)
	{
		CachedMovement->MaxWalkSpeed = OriginalMaxWalkSpeed;
		CachedMovement->StopMovementImmediately();
	}

	RestoreAutoRotation();

	// isWalking=false prevents re-entering the Walking state.
	SetAnimBool(FName("isWalking"), false);

	// Animation="Speaking" triggers the FromEmote → Idle transition.
	// This is the ONLY way Walking returns to Idle in ABP_Manny.
	SetAnimString(FName("Animation"), TEXT("Speaking"));

	UE_LOG(LogWander, Log, TEXT("Set Animation='Speaking', isWalking=false."));
}

void URoomWanderComponent::CheckForwardObstacle()
{
	if (CurrentState != EWanderState::Walking || !CachedCharacter)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const FVector Start = CachedCharacter->GetActorLocation();
	const FVector Forward = (CurrentDestination - Start).GetSafeNormal2D();
	const FVector End = Start + Forward * ForwardProbeDistance;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(ObstacleCheck), false, CachedCharacter);
	FHitResult Hit;

	// Sphere sweep forward with a 30cm radius
	const float SweepRadius = 30.f;
	if (World->SweepSingleByChannel(
		Hit,
		Start,
		End,
		FQuat::Identity,
		ECC_Visibility,
		FCollisionShape::MakeSphere(SweepRadius),
		Params))
	{
		UE_LOG(LogWander, Log, TEXT("Forward obstacle detected (%s at %.0fcm). Stopping."),
			*Hit.GetActor()->GetName(), Hit.Distance);
		CurrentTargetPOI = nullptr;
		StopWalking();
		StartIdleTimer();
	}
}

// ─────────────────────────────────────────────────────────────────────────────
//  POI Discovery & Selection
// ─────────────────────────────────────────────────────────────────────────────

void URoomWanderComponent::DiscoverPOIs()
{
	GatherPOIs();
}

void URoomWanderComponent::GatherPOIs()
{
	DiscoveredPOIs.Empty();

	if (bAutoDiscoverPOIs)
	{
		TArray<AActor*> FoundActors;
		UGameplayStatics::GetAllActorsOfClass(this, AWanderPOI::StaticClass(), FoundActors);
		for (AActor* Actor : FoundActors)
		{
			if (AWanderPOI* POI = Cast<AWanderPOI>(Actor))
			{
				DiscoveredPOIs.AddUnique(POI);
			}
		}
	}

	// Merge manual POIs
	for (const TSoftObjectPtr<AWanderPOI>& Soft : ManualPOIs)
	{
		if (AWanderPOI* POI = Soft.Get())
		{
			DiscoveredPOIs.AddUnique(POI);
		}
	}

	UE_LOG(LogWander, Log, TEXT("GatherPOIs: found %d POIs."), DiscoveredPOIs.Num());
}

AWanderPOI* URoomWanderComponent::PickWeightedPOI()
{
	TArray<AWanderPOI*> Eligible;
	TArray<float> Weights;
	float TotalWeight = 0.f;

	for (int32 i = 0; i < DiscoveredPOIs.Num(); ++i)
	{
		AWanderPOI* POI = DiscoveredPOIs[i];
		if (!POI || !POI->bEnabled)
		{
			continue;
		}
		if (i == LastVisitedPOIIndex && DiscoveredPOIs.Num() > 1)
		{
			continue;
		}
		Eligible.Add(POI);
		Weights.Add(POI->Priority);
		TotalWeight += POI->Priority;
	}

	if (Eligible.Num() == 0 || TotalWeight <= 0.f)
	{
		return nullptr;
	}

	float Roll = FMath::FRandRange(0.f, TotalWeight);
	for (int32 i = 0; i < Eligible.Num(); ++i)
	{
		Roll -= Weights[i];
		if (Roll <= 0.f)
		{
			LastVisitedPOIIndex = DiscoveredPOIs.IndexOfByKey(Eligible[i]);
			return Eligible[i];
		}
	}

	LastVisitedPOIIndex = DiscoveredPOIs.IndexOfByKey(Eligible.Last());
	return Eligible.Last();
}

AWanderPOI* URoomWanderComponent::FindPOIByType(EPOIType Type) const
{
	for (AWanderPOI* POI : DiscoveredPOIs)
	{
		if (POI && POI->bEnabled && POI->POIType == Type)
		{
			return POI;
		}
	}
	return nullptr;
}

void URoomWanderComponent::BeginWalkToPOI(AWanderPOI* POI)
{
	CurrentTargetPOI = POI;
	const FVector Destination = POI->GetStandLocation();
	UE_LOG(LogWander, Log, TEXT("Heading to POI '%s'."), *POI->GetName());
	BeginWalkTo(Destination);
}

void URoomWanderComponent::GoToPOI(EPOIType TargetType)
{
	if (!CachedCharacter || !CachedMovement)
	{
		return;
	}

	AWanderPOI* POI = FindPOIByType(TargetType);
	if (!POI)
	{
		UE_LOG(LogWander, Warning, TEXT("GoToPOI: no enabled POI of type %d found."),
			static_cast<int32>(TargetType));
		return;
	}

	// If currently walking, stop first
	if (CurrentState == EWanderState::Walking)
	{
		CurrentTargetPOI = nullptr;
		StopWalking();
	}

	// Cancel any pending idle timer
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(IdleTimerHandle);
	}

	// Make sure wandering is active so tick processes movement
	if (!bIsWandering)
	{
		bIsWandering = true;
		SetComponentTickEnabled(true);
	}

	BeginWalkToPOI(POI);
}

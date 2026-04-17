// RoomWanderComponent.h
// Autonomous "living in a room" wandering behavior for MetaHuman characters.
// Attach to any ACharacter-based MetaHuman. The character will idle, pick a
// nearby spot, walk there slowly, pause, and repeat.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "WanderPOI.h"
#include "RoomWanderComponent.generated.h"

class ACharacter;
class UCharacterMovementComponent;
class UAnimInstance;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class EMBODY_API URoomWanderComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URoomWanderComponent();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	// ── Wander Area ──

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wander|Area")
	FVector WanderCenter = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wander|Area")
	FVector WanderHalfExtents = FVector(300.f, 300.f, 50.f);

	// ── Movement ──

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wander|Movement")
	float WanderSpeed = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wander|Movement")
	float ArrivalTolerance = 50.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wander|Movement")
	float RotationInterpSpeed = 3.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wander|Movement")
	float WalkTimeout = 15.f;

	// ── Idle Timing ──

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wander|Timing")
	float MinIdleTime = 5.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wander|Timing")
	float MaxIdleTime = 15.f;

	// ── Distance Distribution ──

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wander|Distance")
	float SmallMoveMin = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wander|Distance")
	float SmallMoveMax = 300.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wander|Distance")
	float LargeMoveMin = 300.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wander|Distance")
	float LargeMoveMax = 600.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wander|Distance",
		meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SmallMoveProbability = 0.7f;

	// ── Camera ──

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wander|Camera")
	float CameraFollowSpeed = 1.5f;

	// ── Avoidance ──

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wander|Avoidance")
	float MinWallDistance = 250.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wander|Avoidance")
	float ForwardProbeDistance = 150.f;

	// ── POI ──

	/** Automatically find all AWanderPOI actors in the level. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wander|POI")
	bool bAutoDiscoverPOIs = true;

	/** Manually assigned POIs (used in addition to auto-discovered ones). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wander|POI")
	TArray<TSoftObjectPtr<AWanderPOI>> ManualPOIs;

	/** Re-scan the level for AWanderPOI actors. */
	UFUNCTION(BlueprintCallable, Category = "Wander|POI")
	void DiscoverPOIs();

	// ── Autonomous Mode ──

	/** When true, character autonomously picks POIs to visit.
	 *  When false, character idles until commanded via GoToPOI(). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wander")
	bool bAutonomousMode = true;

	/** Command the character to walk to a specific POI by type.
	 *  Works in both autonomous and non-autonomous mode.
	 *  Interrupts current walk if one is in progress. */
	UFUNCTION(BlueprintCallable, Category = "Wander")
	void GoToPOI(EPOIType TargetType);

	// ── Auto-start ──

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wander")
	bool bAutoStart = true;

	// ── Runtime State ──

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wander")
	bool bIsWandering = false;

	// ── Blueprint Interface ──

	UFUNCTION(BlueprintCallable, Category = "Wander")
	void PauseWandering();

	UFUNCTION(BlueprintCallable, Category = "Wander")
	void ResumeWandering();

	UFUNCTION(BlueprintCallable, Category = "Wander")
	void SetWanderArea(FVector Center, FVector HalfExtents);

private:
	enum class EWanderState : uint8
	{
		Idle,
		Walking,
		Settling   // Arrived at POI, smoothly rotating to face direction
	};

	void StartIdleTimer();
	void StartIdleTimerWithDuration(float Duration);
	void OnIdleTimerExpired();
	FVector PickRandomDestination();
	void BeginWalkTo(const FVector& Destination);
	void StopWalking();
	void CheckForwardObstacle();

	// ── POI helpers ──

	void GatherPOIs();
	AWanderPOI* PickWeightedPOI();
	AWanderPOI* FindPOIByType(EPOIType Type) const;
	void BeginWalkToPOI(AWanderPOI* POI);

	// ── Anim instance helpers ──

	bool EnsureAnimInstance();
	UAnimInstance* FindAnimInstanceWithProperty(FName PropertyName) const;
	void SetAnimBool(FName PropertyName, bool bValue);
	void SetAnimString(FName PropertyName, const FString& Value);

	// ── Rotation helpers ──

	void DisableAutoRotation();
	void RestoreAutoRotation();

	// ── Cached references ──

	UPROPERTY()
	TObjectPtr<ACharacter> CachedCharacter;

	UPROPERTY()
	TObjectPtr<UCharacterMovementComponent> CachedMovement;

	UPROPERTY()
	TObjectPtr<UAnimInstance> CachedAnimInstance;

	// ── POI state ──

	UPROPERTY()
	TArray<TObjectPtr<AWanderPOI>> DiscoveredPOIs;

	UPROPERTY()
	TObjectPtr<AWanderPOI> CurrentTargetPOI;

	int32 LastVisitedPOIIndex = -1;

	// ── Runtime state ──

	EWanderState CurrentState = EWanderState::Idle;
	FVector CurrentDestination = FVector::ZeroVector;
	float OriginalMaxWalkSpeed = 0.f;
	float WalkElapsedTime = 0.f;
	float SettlingTargetYaw = 0.f;
	float SettlingDwellTime = 0.f;

	bool bOriginalOrientToMovement = false;
	bool bOriginalUseControllerDesiredRotation = false;
	bool bOriginalUseControllerRotationYaw = false;
	bool bRotationOverrideActive = false;

	FTimerHandle IdleTimerHandle;
	FTimerHandle ObstacleCheckHandle;

	// ── Stuck detection ──

	float StuckTime = 0.f;
	float StuckThreshold = 0.5f;
	float StuckSpeedLimit = 5.f;
};

// WanderPOI.h
// Placeable Point of Interest for RoomWanderComponent.
// Orient the green arrow to set the character's visual facing direction on arrival.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WanderPOI.generated.h"

class UArrowComponent;

UENUM(BlueprintType)
enum class EPOIType : uint8
{
	Generic       UMETA(DisplayName = "Generic"),
	Nightstand    UMETA(DisplayName = "Nightstand"),
	Window        UMETA(DisplayName = "Window"),
	ComputerDesk  UMETA(DisplayName = "ComputerDesk")
};

UCLASS(Blueprintable, meta = (DisplayName = "Wander POI"))
class EMBODY_API AWanderPOI : public AActor
{
	GENERATED_BODY()

public:
	AWanderPOI();

	// ── Components ──

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "POI")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "POI")
	TObjectPtr<UArrowComponent> DirectionArrow;

	// ── Settings ──

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "POI")
	EPOIType POIType = EPOIType::Generic;

	/** Seconds to idle here. 0 = use component default. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "POI", meta = (ClampMin = "0.0"))
	float DwellTime = 0.f;

	/** Higher priority = more likely to be chosen. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "POI", meta = (ClampMin = "0.01"))
	float Priority = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "POI")
	bool bEnabled = true;

	// ── Helpers ──

	/** Location the character should walk to. */
	UFUNCTION(BlueprintCallable, Category = "POI")
	FVector GetStandLocation() const;

	/**
	 * Actor yaw the MetaHuman should be set to on arrival so that
	 * the visual face (mesh -X) points along the arrow's +X direction.
	 */
	UFUNCTION(BlueprintCallable, Category = "POI")
	float GetArrivalActorYaw() const;
};

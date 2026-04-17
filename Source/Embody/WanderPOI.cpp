// WanderPOI.cpp

#include "WanderPOI.h"
#include "Components/ArrowComponent.h"

AWanderPOI::AWanderPOI()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	// Green arrow showing the character's desired visual facing direction
	DirectionArrow = CreateDefaultSubobject<UArrowComponent>(TEXT("DirectionArrow"));
	DirectionArrow->SetupAttachment(SceneRoot);
	DirectionArrow->SetArrowColor(FLinearColor::Green);
	DirectionArrow->ArrowSize = 1.5f;
	DirectionArrow->SetHiddenInGame(true);
}

FVector AWanderPOI::GetStandLocation() const
{
	return GetActorLocation();
}

float AWanderPOI::GetArrivalActorYaw() const
{
	// Arrow +X = desired visual face direction.
	// MetaHuman mesh faces -X, so actor yaw = arrow yaw + 180.
	return GetActorRotation().Yaw + 180.f;
}

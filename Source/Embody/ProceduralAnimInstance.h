#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "BoneControllers/AnimNode_ModifyBone.h"
#include "ProceduralAnimInstance.generated.h"

class UProceduralAnimComponent;

/**
 * Animation instance that applies procedural bone rotations from ProceduralAnimComponent.
 *
 * Set this as the Anim Class on your character's skeletal mesh, or use it as a
 * Linked Anim Layer. It reads bone overrides from ProceduralAnimComponent every
 * frame and applies them as additive rotations in bone space.
 *
 * Works with any skeleton — automatically validates bone names against the skeleton
 * at runtime. Invalid bone names are silently skipped.
 */
UCLASS()
class EMBODY_API UProceduralAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

	// All current bone rotation overrides (read by the anim graph)
	UPROPERTY(BlueprintReadOnly, Category = "Procedural Animation")
	TMap<FName, FRotator> BoneRotationOverrides;

protected:
	// Cached reference to the component
	UPROPERTY()
	TWeakObjectPtr<UProceduralAnimComponent> ProceduralAnimComp;
};

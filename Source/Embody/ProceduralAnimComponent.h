#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ProceduralAnimComponent.generated.h"

UCLASS(ClassGroup=(Animation), meta=(BlueprintSpawnableComponent))
class EMBODY_API UProceduralAnimComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UProceduralAnimComponent();

	UFUNCTION(BlueprintCallable, Category = "Procedural Animation")
	void SetBoneRotation(FName BoneName, FRotator Rotation);

	UFUNCTION(BlueprintCallable, Category = "Procedural Animation")
	void ClearBoneRotation(FName BoneName);

	UFUNCTION(BlueprintCallable, Category = "Procedural Animation")
	void ClearAllBoneRotations();

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Procedural Animation")
	FRotator GetBoneRotation(FName BoneName) const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Procedural Animation")
	bool HasBoneRotation(FName BoneName) const;

	// Returns a thread-safe snapshot copy — safe to call from any thread.
	TMap<FName, FRotator> GetAllBoneRotations() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Procedural Animation")
	TArray<FName> GetOverriddenBoneNames() const;

	// Parse and apply a MAN_ command: MAN_BoneName_Pitch_Yaw_Roll
	UFUNCTION(BlueprintCallable, Category = "Procedural Animation")
	bool ApplyMannequinCommand(const FString& Command);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Procedural Animation")
	TMap<FName, FRotator> BoneRotations;

private:
	mutable FCriticalSection BoneDataLock;
};

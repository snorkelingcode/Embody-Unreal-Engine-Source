// EmbodyCameraBPLibrary.h
// Unified command processor for TCP -> SpringArm + CineCamera control.
// Two nodes: IsCameraCommand (detect) and ApplyCameraCommand (apply).
//
// Command formats:
//   CAMSHOT.{Preset}                         — snap to a predefined camera angle
//   CAMSTREAM_{x}_{y}_{z}_{pitch}_{yaw}_{roll} — 6-axis offset from current CAMSHOT base

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "EmbodyCameraBPLibrary.generated.h"

class USpringArmComponent;
class UCineCameraComponent;
class USkeletalMeshComponent;

// Stores the base transform for a CAMSHOT preset so CAMSTREAM can offset from it.
USTRUCT(BlueprintType)
struct FEmbodyCamShotPreset
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float ArmLength = 50.f;

	// CineCamera local rotation (pitch/yaw/roll relative to spring arm end)
	// Spring arm rotation is pawn-controlled, so all look rotation goes here.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FRotator CameraRotation = FRotator::ZeroRotator;

	// Spring arm socket offset (lateral / vertical positioning)
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector SocketOffset = FVector::ZeroVector;
};

UCLASS()
class EMBODY_API UEmbodyCameraBPLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	/** Check if a raw TCP command string is a camera command (CAMSHOT or CAMSTREAM). */
	UFUNCTION(BlueprintCallable, Category = "Embody|Camera",
		meta = (DisplayName = "Is Camera Command"))
	static bool IsCameraCommand(const FString& Command);

	/** Parse and apply a camera command to a SpringArm + CineCamera pair.
	 *  For CAMSHOT: snaps to a preset and stores the base transform.
	 *  For CAMSTREAM: applies 6-axis offsets on top of the stored CAMSHOT base.
	 *  @param SpringArm   - The spring arm controlling distance and orbit
	 *  @param CineCamera  - The cine camera for roll and fine rotation
	 *  @param Command     - Raw TCP string (CAMSHOT.X or CAMSTREAM_x_y_z_p_y_r) */
	UFUNCTION(BlueprintCallable, Category = "Embody|Camera",
		meta = (DisplayName = "Apply Camera Command"))
	static void ApplyCameraCommand(
		USpringArmComponent* SpringArm,
		UCineCameraComponent* CineCamera,
		const FString& Command);

	/** Get the default preset values for a named CAMSHOT.
	 *  Returns false if the preset name is unknown. */
	UFUNCTION(BlueprintCallable, Category = "Embody|Camera",
		meta = (DisplayName = "Get CamShot Preset"))
	static bool GetCamShotPreset(const FString& PresetName, FEmbodyCamShotPreset& OutPreset);

	/** Debug: Log bone Z-heights from a skeletal mesh to the output log.
	 *  Call this on the Body SkeletalMeshComponent after spawn. */
	UFUNCTION(BlueprintCallable, Category = "Embody|Camera",
		meta = (DisplayName = "Log Bone Heights"))
	static void LogBoneHeights(USkeletalMeshComponent* BodyMesh);

private:

	// Current base state set by the last CAMSHOT command.
	// CAMSTREAM offsets are applied on top of this.
	static FEmbodyCamShotPreset CurrentBase;

	static void ApplyCamShot(USpringArmComponent* SpringArm, UCineCameraComponent* CineCamera, const FString& PresetName);
	static void ApplyCamStream(USpringArmComponent* SpringArm, UCineCameraComponent* CineCamera, const FString& ParamString);
	static void ApplyViewMode(UCineCameraComponent* CineCamera, const FString& ViewName);
};

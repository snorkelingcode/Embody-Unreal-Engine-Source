// EmbodyCameraBPLibrary.cpp
// Unified camera command processor for TCP commands.
//
// The spring arm uses bUsePawnControlRotation = true, so its rotation is
// driven by the controller. We NEVER touch spring arm rotation.
// All look rotation (pitch, yaw, roll) is applied to the CineCamera's
// relative rotation instead.

#include "EmbodyCameraBPLibrary.h"
#include "GameFramework/SpringArmComponent.h"
#include "CineCameraComponent.h"
#include "GameFramework/Actor.h"
#include "Components/SkeletalMeshComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogEmbodyCam, Log, All);

// Static storage for the current CAMSHOT base (persists between calls)
FEmbodyCamShotPreset UEmbodyCameraBPLibrary::CurrentBase;

// Z offset from the last CAMSHOT preset, persists for CAMSTREAM calls
static float CurrentSpringArmZOffset = 0.f;

// ---------------------------------------------------------------------------
// Per-character spring arm base position.
// Head bone Z: Lucy=52.71, Sarah=54.19, Harry=64.95, Zach=66.57
// Harry's Z=58 is intentionally below head bone (67 frames top of head).
// ---------------------------------------------------------------------------
static FVector GetCharacterSpringArmBase(const AActor* Owner)
{
	if (Owner)
	{
		const FString ClassName = Owner->GetClass()->GetName();
		if (ClassName.Contains(TEXT("Lucy")))   return FVector(0.f, 2.f, 55.f);
		if (ClassName.Contains(TEXT("Sarah")))  return FVector(0.f, 2.f, 56.f);
		if (ClassName.Contains(TEXT("Harry")))  return FVector(0.f, 2.f, 58.f);
		if (ClassName.Contains(TEXT("Zach")))   return FVector(0.f, 2.f, 69.f);
	}
	return FVector(0.f, 2.f, 55.f);
}

// ---------------------------------------------------------------------------
// Per-preset spring arm Z offset from the character's head-height base.
// Moves the camera pivot up/down for different shot types.
// ---------------------------------------------------------------------------
static float GetPresetZOffset(const FString& PresetName)
{
	if (PresetName == TEXT("HighAngle"))  return 25.f;   // above head
	if (PresetName == TEXT("LowAngle"))  return -30.f;   // upper chest
	if (PresetName == TEXT("Medium"))     return -30.f;   // upper chest
	if (PresetName == TEXT("WideShot"))   return -55.f;   // near ground
	// Default, ExtremeClose, Close — stay at head height
	return 0.f;
}

// ---------------------------------------------------------------------------
// Built-in CAMSHOT presets
// Values: ArmLength, CameraRotation(Pitch, Yaw, Roll), SocketOffset(X, Y, Z)
//
// Vertical framing is handled by GetPresetZOffset (moves spring arm Z),
// NOT by SocketOffset. SocketOffset is reserved for CAMSTREAM fine-tuning.
// Arm lengths match the original Blueprint Camshot values.
// ---------------------------------------------------------------------------
static const TMap<FString, FEmbodyCamShotPreset>& GetPresets()
{
	static TMap<FString, FEmbodyCamShotPreset> Presets;
	if (Presets.Num() == 0)
	{
		// Default — standard conversational shot at head height
		Presets.Add(TEXT("Default"),      { 55.f,   FRotator(0.f, 0.f, 0.f),    FVector::ZeroVector });

		// Extreme Close — tight on the face
		Presets.Add(TEXT("ExtremeClose"), { 40.f,   FRotator(0.f, 0.f, 0.f),    FVector::ZeroVector });

		// Close — head and shoulders
		Presets.Add(TEXT("Close"),        { 50.f,   FRotator(0.f, 0.f, 0.f),    FVector::ZeroVector });

		// High Angle — camera raised, looking down (Z offset +25)
		Presets.Add(TEXT("HighAngle"),    { 70.f,   FRotator(-25.f, 0.f, 0.f),  FVector::ZeroVector });

		// Low Angle — camera lowered, looking up (Z offset -30)
		Presets.Add(TEXT("LowAngle"),     { 70.f,   FRotator(25.f, 0.f, 0.f),   FVector::ZeroVector });

		// Medium — waist up (Z offset -30)
		Presets.Add(TEXT("Medium"),       { 180.f,  FRotator(0.f, 0.f, 0.f),    FVector::ZeroVector });

		// Wide Shot — full body and environment (Z offset -55, socketZ raises camera to waist height)
		Presets.Add(TEXT("WideShot"),     { 325.f,  FRotator(-10.f, 0.f, 0.f),  FVector(0.f, 0.f, 40.f) });
	}
	return Presets;
}

// ---------------------------------------------------------------------------
// IsCameraCommand
// ---------------------------------------------------------------------------
bool UEmbodyCameraBPLibrary::IsCameraCommand(const FString& Command)
{
	return Command.StartsWith(TEXT("CAMSHOT.")) || Command.StartsWith(TEXT("CAMSTREAM_")) || Command.StartsWith(TEXT("VIEW."));
}

// ---------------------------------------------------------------------------
// GetCamShotPreset
// ---------------------------------------------------------------------------
bool UEmbodyCameraBPLibrary::GetCamShotPreset(const FString& PresetName, FEmbodyCamShotPreset& OutPreset)
{
	const auto& Presets = GetPresets();
	if (const FEmbodyCamShotPreset* Found = Presets.Find(PresetName))
	{
		OutPreset = *Found;
		return true;
	}
	return false;
}

// ---------------------------------------------------------------------------
// ApplyCameraCommand — main dispatcher
// ---------------------------------------------------------------------------
void UEmbodyCameraBPLibrary::ApplyCameraCommand(
	USpringArmComponent* SpringArm,
	UCineCameraComponent* CineCamera,
	const FString& Command)
{
	if (!SpringArm || !CineCamera)
	{
		UE_LOG(LogEmbodyCam, Warning, TEXT("ApplyCameraCommand: null SpringArm or CineCamera"));
		return;
	}

	// Reset spring arm to character's head-height base position
	AActor* Owner = SpringArm->GetOwner();
	SpringArm->SetRelativeLocation(GetCharacterSpringArmBase(Owner));

	if (Command.StartsWith(TEXT("CAMSHOT.")))
	{
		FString PresetName = Command.RightChop(8);  // len("CAMSHOT.") = 8
		ApplyCamShot(SpringArm, CineCamera, PresetName);
	}
	else if (Command.StartsWith(TEXT("CAMSTREAM_")))
	{
		// Re-apply the Z offset from the last CAMSHOT before CAMSTREAM adjustments
		FVector Loc = SpringArm->GetRelativeLocation();
		Loc.Z += CurrentSpringArmZOffset;
		SpringArm->SetRelativeLocation(Loc);

		FString ParamString = Command.RightChop(10); // len("CAMSTREAM_") = 10
		ApplyCamStream(SpringArm, CineCamera, ParamString);
	}
	else if (Command.StartsWith(TEXT("VIEW.")))
	{
		FString ViewName = Command.RightChop(5); // len("VIEW.") = 5
		ApplyViewMode(CineCamera, ViewName);
	}
	else
	{
		UE_LOG(LogEmbodyCam, Warning, TEXT("Unknown camera command: %s"), *Command);
	}
}

// ---------------------------------------------------------------------------
// ApplyCamShot — snap to a preset, reset CAMSTREAM offsets
// ---------------------------------------------------------------------------
void UEmbodyCameraBPLibrary::ApplyCamShot(
	USpringArmComponent* SpringArm,
	UCineCameraComponent* CineCamera,
	const FString& PresetName)
{
	FEmbodyCamShotPreset Preset;
	if (!GetCamShotPreset(PresetName, Preset))
	{
		UE_LOG(LogEmbodyCam, Warning, TEXT("Unknown CAMSHOT preset: %s"), *PresetName);
		return;
	}

	// Store as the current base for CAMSTREAM offsets
	CurrentBase = Preset;

	// Adjust spring arm Z for this preset (raise for HighAngle, lower for Medium/WideShot)
	CurrentSpringArmZOffset = GetPresetZOffset(PresetName);
	FVector Loc = SpringArm->GetRelativeLocation();
	Loc.Z += CurrentSpringArmZOffset;
	SpringArm->SetRelativeLocation(Loc);

	// Spring arm: distance and socket offset
	SpringArm->TargetArmLength = Preset.ArmLength;
	SpringArm->SocketOffset = Preset.SocketOffset;

	// CineCamera: all look rotation (pitch/yaw/roll)
	CineCamera->SetRelativeRotation(Preset.CameraRotation);

	UE_LOG(LogEmbodyCam, Verbose, TEXT("CAMSHOT.%s — Arm=%.0f Z=%.0f CamRot=%s"),
		*PresetName,
		Preset.ArmLength,
		Loc.Z,
		*Preset.CameraRotation.ToString());
}

// ---------------------------------------------------------------------------
// ApplyCamStream — apply 6-axis offsets on top of the current CAMSHOT base
// Format: x_y_z_pitch_yaw_roll (all floats, underscore-delimited)
//
// Mapping (spring arm rotation is pawn-controlled, never touched):
//   x     → SocketOffset.Y (lateral)     added to base SocketOffset.Y
//   y     → SocketOffset.Z (vertical)    added to base SocketOffset.Z
//   z     → TargetArmLength              subtracted from base (positive z = closer)
//   pitch → CineCamera pitch             added to base CameraRotation.Pitch
//   yaw   → CineCamera yaw              added to base CameraRotation.Yaw
//   roll  → CineCamera roll              added to base CameraRotation.Roll
// ---------------------------------------------------------------------------
void UEmbodyCameraBPLibrary::ApplyCamStream(
	USpringArmComponent* SpringArm,
	UCineCameraComponent* CineCamera,
	const FString& ParamString)
{
	TArray<FString> Parts;
	ParamString.ParseIntoArray(Parts, TEXT("_"));

	if (Parts.Num() < 6)
	{
		UE_LOG(LogEmbodyCam, Warning, TEXT("CAMSTREAM needs 6 params (x_y_z_pitch_yaw_roll), got %d: %s"),
			Parts.Num(), *ParamString);
		return;
	}

	const float X     = FCString::Atof(*Parts[0]);
	const float Y     = FCString::Atof(*Parts[1]);
	const float Z     = FCString::Atof(*Parts[2]);
	const float Pitch = FCString::Atof(*Parts[3]);
	const float Yaw   = FCString::Atof(*Parts[4]);
	const float Roll  = FCString::Atof(*Parts[5]);

	// Spring arm length: base minus Z offset (positive Z = push in = shorter arm)
	SpringArm->TargetArmLength = FMath::Max(CurrentBase.ArmLength - Z, 10.f);

	// Socket offset: base + lateral (X) and vertical (Y) offsets
	FVector Offset = CurrentBase.SocketOffset;
	Offset.Y += X;  // X from frontend = lateral = socket Y
	Offset.Z += Y;  // Y from frontend = vertical = socket Z
	SpringArm->SocketOffset = Offset;

	// CineCamera rotation: base + all rotation offsets
	FRotator CamRot = CurrentBase.CameraRotation;
	CamRot.Pitch += Pitch;
	CamRot.Yaw   += Yaw;
	CamRot.Roll  += Roll;
	CineCamera->SetRelativeRotation(CamRot);

	UE_LOG(LogEmbodyCam, Verbose, TEXT("CAMSTREAM — Arm=%0.f Offset=%s CamRot=%s"),
		SpringArm->TargetArmLength,
		*Offset.ToString(),
		*CamRot.ToString());
}

// ---------------------------------------------------------------------------
// ApplyViewMode — switch filmback between Mobile and Desktop
// ---------------------------------------------------------------------------
void UEmbodyCameraBPLibrary::ApplyViewMode(
	UCineCameraComponent* CineCamera,
	const FString& ViewName)
{
	if (ViewName.Equals(TEXT("Mobile"), ESearchCase::IgnoreCase))
	{
		CineCamera->Filmback.SensorWidth = 7.088156f;
		CineCamera->Filmback.SensorHeight = 13.365f;
		CineCamera->Filmback.SensorHorizontalOffset = 0.f;
		CineCamera->Filmback.SensorVerticalOffset = 0.f;
		CineCamera->SetConstraintAspectRatio(true);
		UE_LOG(LogEmbodyCam, Log, TEXT("VIEW.Mobile — Filmback set to portrait (9:16)"));
	}
	else if (ViewName.Equals(TEXT("Desktop"), ESearchCase::IgnoreCase))
	{
		CineCamera->Filmback.SensorWidth = 23.76f;
		CineCamera->Filmback.SensorHeight = 13.365f;
		CineCamera->Filmback.SensorHorizontalOffset = 0.f;
		CineCamera->Filmback.SensorVerticalOffset = 0.f;
		CineCamera->SetConstraintAspectRatio(false);
		UE_LOG(LogEmbodyCam, Log, TEXT("VIEW.Desktop — Filmback set to landscape (16:9)"));
	}
	else
	{
		UE_LOG(LogEmbodyCam, Warning, TEXT("Unknown VIEW mode: %s"), *ViewName);
	}
}

// ---------------------------------------------------------------------------
// LogBoneHeights — debug tool to print bone Z positions relative to capsule root
// ---------------------------------------------------------------------------
void UEmbodyCameraBPLibrary::LogBoneHeights(USkeletalMeshComponent* BodyMesh)
{
	if (!BodyMesh)
	{
		UE_LOG(LogEmbodyCam, Warning, TEXT("LogBoneHeights: null BodyMesh"));
		return;
	}

	AActor* Owner = BodyMesh->GetOwner();
	const FString ActorName = Owner ? Owner->GetClass()->GetName() : TEXT("Unknown");
	const float ActorZ = Owner ? Owner->GetActorLocation().Z : 0.f;

	// Bones to measure
	static const FName BoneNames[] = {
		TEXT("head"),
		TEXT("neck_01"),
		TEXT("neck_02"),
		TEXT("spine_05"),
		TEXT("spine_04"),
		TEXT("spine_03"),
		TEXT("spine_02"),
		TEXT("spine_01"),
		TEXT("pelvis"),
		TEXT("thigh_l"),
		TEXT("calf_l"),
		TEXT("foot_l")
	};

	UE_LOG(LogEmbodyCam, Warning, TEXT("===== BONE HEIGHTS: %s (ActorZ=%.1f) ====="), *ActorName, ActorZ);

	for (const FName& BoneName : BoneNames)
	{
		const int32 BoneIndex = BodyMesh->GetBoneIndex(BoneName);
		if (BoneIndex == INDEX_NONE)
		{
			UE_LOG(LogEmbodyCam, Warning, TEXT("  %s: NOT FOUND"), *BoneName.ToString());
			continue;
		}

		const FVector WorldPos = BodyMesh->GetBoneLocation(BoneName);
		const float RelativeZ = WorldPos.Z - ActorZ;
		UE_LOG(LogEmbodyCam, Warning, TEXT("  %s: Z=%.2f (world=%.2f)"), *BoneName.ToString(), RelativeZ, WorldPos.Z);
	}

	UE_LOG(LogEmbodyCam, Warning, TEXT("===== END BONE HEIGHTS ====="));
}

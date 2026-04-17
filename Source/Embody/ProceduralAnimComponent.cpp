#include "ProceduralAnimComponent.h"

UProceduralAnimComponent::UProceduralAnimComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UProceduralAnimComponent::SetBoneRotation(FName BoneName, FRotator Rotation)
{
	FScopeLock Lock(&BoneDataLock);
	BoneRotations.Add(BoneName, Rotation);
}

void UProceduralAnimComponent::ClearBoneRotation(FName BoneName)
{
	FScopeLock Lock(&BoneDataLock);
	BoneRotations.Remove(BoneName);
}

void UProceduralAnimComponent::ClearAllBoneRotations()
{
	FScopeLock Lock(&BoneDataLock);
	BoneRotations.Empty();
}

FRotator UProceduralAnimComponent::GetBoneRotation(FName BoneName) const
{
	FScopeLock Lock(&BoneDataLock);
	const FRotator* Found = BoneRotations.Find(BoneName);
	return Found ? *Found : FRotator::ZeroRotator;
}

bool UProceduralAnimComponent::HasBoneRotation(FName BoneName) const
{
	FScopeLock Lock(&BoneDataLock);
	return BoneRotations.Contains(BoneName);
}

TMap<FName, FRotator> UProceduralAnimComponent::GetAllBoneRotations() const
{
	FScopeLock Lock(&BoneDataLock);
	return BoneRotations;  // snapshot copy — animation thread iterates this, not the live map
}

TArray<FName> UProceduralAnimComponent::GetOverriddenBoneNames() const
{
	FScopeLock Lock(&BoneDataLock);
	TArray<FName> Names;
	BoneRotations.GetKeys(Names);
	return Names;
}

bool UProceduralAnimComponent::ApplyMannequinCommand(const FString& Command)
{
	if (!Command.StartsWith(TEXT("MAN_")))
	{
		return false;
	}

	FString Remainder = Command.RightChop(4);

	TArray<FString> Parts;
	Remainder.ParseIntoArray(Parts, TEXT("_"), true);

	if (Parts.Num() < 4)
	{
		return false;
	}

	// Last 3 parts are Pitch, Yaw, Roll
	float X = FCString::Atof(*Parts[Parts.Num() - 3]);
	float Y = FCString::Atof(*Parts[Parts.Num() - 2]);
	float Z = FCString::Atof(*Parts[Parts.Num() - 1]);

	// Everything before the last 3 parts is the bone name
	TArray<FString> NameParts;
	for (int32 i = 0; i < Parts.Num() - 3; i++)
	{
		NameParts.Add(Parts[i]);
	}
	FString BoneNameStr = FString::Join(NameParts, TEXT("_")).ToLower();
	FName BoneName = FName(*BoneNameStr);

	SetBoneRotation(BoneName, FRotator(X, Y, Z));
	return true;
}

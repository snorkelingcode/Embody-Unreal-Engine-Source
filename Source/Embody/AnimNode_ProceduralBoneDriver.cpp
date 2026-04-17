#include "AnimNode_ProceduralBoneDriver.h"
#include "ProceduralAnimComponent.h"
#include "Animation/AnimInstanceProxy.h"
#include "Components/SkeletalMeshComponent.h"

FAnimNode_ProceduralBoneDriver::FAnimNode_ProceduralBoneDriver()
	: Alpha(1.0f)
	, bCacheValid(false)
{
}

void FAnimNode_ProceduralBoneDriver::Initialize_AnyThread(const FAnimationInitializeContext& Context)
{
	FAnimNode_Base::Initialize_AnyThread(Context);
	SourcePose.Initialize(Context);
	bCacheValid = false;
}

void FAnimNode_ProceduralBoneDriver::CacheBones_AnyThread(const FAnimationCacheBonesContext& Context)
{
	FAnimNode_Base::CacheBones_AnyThread(Context);
	SourcePose.CacheBones(Context);
	bCacheValid = false;
}

void FAnimNode_ProceduralBoneDriver::Update_AnyThread(const FAnimationUpdateContext& Context)
{
	FAnimNode_Base::Update_AnyThread(Context);
	SourcePose.Update(Context);
}

void FAnimNode_ProceduralBoneDriver::RebuildBoneCache(const FBoneContainer& RequiredBones, const TMap<FName, FRotator>& Rotations)
{
	CachedBoneIndices.Empty();

	for (const auto& Pair : Rotations)
	{
		int32 BoneIndex = RequiredBones.GetPoseBoneIndexForBoneName(Pair.Key);

		if (BoneIndex == INDEX_NONE)
		{
			FString LowerName = Pair.Key.ToString().ToLower();
			BoneIndex = RequiredBones.GetPoseBoneIndexForBoneName(FName(*LowerName));
		}

		if (BoneIndex != INDEX_NONE)
		{
			FCompactPoseBoneIndex CompactIndex = RequiredBones.MakeCompactPoseIndex(FMeshPoseBoneIndex(BoneIndex));
			if (CompactIndex != INDEX_NONE)
			{
				CachedBoneIndices.Add(Pair.Key, CompactIndex);
			}
		}
	}

	bCacheValid = true;
}

void FAnimNode_ProceduralBoneDriver::Evaluate_AnyThread(FPoseContext& Output)
{
	SourcePose.Evaluate(Output);

	if (Alpha <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	// Read directly from ProceduralAnimComponent on the owning actor
	const TMap<FName, FRotator>* RotationsPtr = nullptr;
	TMap<FName, FRotator> DirectRotations;

	// Try the pin-bound BoneRotations first
	if (BoneRotations.Num() > 0)
	{
		for (const auto& Pair : BoneRotations)
		{
			if (Pair.Key != NAME_None)
			{
				DirectRotations.Add(Pair.Key, Pair.Value);
			}
		}
		if (DirectRotations.Num() > 0)
		{
			RotationsPtr = &DirectRotations;
		}
	}

	// If pin binding didn't work, read directly from the component
	if (!RotationsPtr || RotationsPtr->Num() == 0)
	{
		USkeletalMeshComponent* SkelComp = Output.AnimInstanceProxy->GetSkelMeshComponent();
		if (SkelComp)
		{
			AActor* Owner = SkelComp->GetOwner();
			if (Owner)
			{
				UProceduralAnimComponent* AnimComp = Owner->FindComponentByClass<UProceduralAnimComponent>();
				if (AnimComp)
				{
					DirectRotations = AnimComp->GetAllBoneRotations();
					if (DirectRotations.Num() > 0)
					{
						RotationsPtr = &DirectRotations;

						static bool bLoggedDirect = false;
						if (!bLoggedDirect)
						{
							UE_LOG(LogTemp, Log, TEXT("[MAN] ProceduralBoneDriver reading DIRECTLY from component (%d bones)"), DirectRotations.Num());
							bLoggedDirect = true;
						}
					}
				}
			}
		}
	}

	if (!RotationsPtr || RotationsPtr->Num() == 0)
	{
		return;
	}

	const TMap<FName, FRotator>& Rotations = *RotationsPtr;
	const FBoneContainer& RequiredBones = Output.Pose.GetBoneContainer();

	if (!bCacheValid || CachedBoneIndices.Num() != Rotations.Num())
	{
		RebuildBoneCache(RequiredBones, Rotations);
	}

	for (const auto& Pair : Rotations)
	{
		const FCompactPoseBoneIndex* CompactIndexPtr = CachedBoneIndices.Find(Pair.Key);
		if (!CompactIndexPtr || *CompactIndexPtr == INDEX_NONE) continue;

		FCompactPoseBoneIndex CompactIndex = *CompactIndexPtr;
		if (!Output.Pose.IsValidIndex(CompactIndex)) continue;

		FTransform& BoneTransform = Output.Pose[CompactIndex];
		FQuat OverrideRotation = Pair.Value.Quaternion();

		if (Alpha < 1.0f - KINDA_SMALL_NUMBER)
		{
			OverrideRotation = FQuat::Slerp(FQuat::Identity, OverrideRotation, Alpha);
		}

		BoneTransform.SetRotation(BoneTransform.GetRotation() * OverrideRotation);
		BoneTransform.NormalizeRotation();
	}
}

void FAnimNode_ProceduralBoneDriver::GatherDebugData(FNodeDebugData& DebugData)
{
	FString DebugLine = DebugData.GetNodeName(this);
	DebugLine += FString::Printf(TEXT(" (Alpha: %.2f)"), Alpha);
	DebugData.AddDebugItem(DebugLine);
	SourcePose.GatherDebugData(DebugData);
}

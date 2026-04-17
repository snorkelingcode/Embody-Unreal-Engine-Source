#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNodeBase.h"
#include "BoneContainer.h"
#include "AnimNode_ProceduralBoneDriver.generated.h"

/**
 * Anim graph node that applies procedural rotation offsets to multiple bones.
 * Reads a TMap<FName, FRotator> and applies each entry as an additive rotation
 * in bone space. One node handles the entire skeleton — no per-bone wiring needed.
 *
 * Usage in ABP_Manny AnimGraph:
 *   1. Add this node after your animation pose
 *   2. Connect BoneRotations to ProceduralAnimInstance::BoneRotationOverrides
 *   3. Done — all bones are driven automatically
 */
USTRUCT(BlueprintInternalUseOnly)
struct EMBODY_API FAnimNode_ProceduralBoneDriver : public FAnimNode_Base
{
	GENERATED_BODY()

	// Input pose
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Links)
	FPoseLink SourcePose;

	// Map of bone names to rotation offsets (fed from ProceduralAnimInstance)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural Animation", meta = (PinShownByDefault))
	TMap<FName, FRotator> BoneRotations;

	// Alpha blend (0 = no effect, 1 = full effect)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural Animation", meta = (PinShownByDefault, ClampMin = "0.0", ClampMax = "1.0"))
	float Alpha = 1.0f;

public:
	FAnimNode_ProceduralBoneDriver();

	// FAnimNode_Base interface
	virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override;
	virtual void CacheBones_AnyThread(const FAnimationCacheBonesContext& Context) override;
	virtual void Update_AnyThread(const FAnimationUpdateContext& Context) override;
	virtual void Evaluate_AnyThread(FPoseContext& Output) override;
	virtual void GatherDebugData(FNodeDebugData& DebugData) override;

private:
	// Cached bone indices for quick lookup
	TMap<FName, FCompactPoseBoneIndex> CachedBoneIndices;
	bool bCacheValid = false;

	void RebuildBoneCache(const FBoneContainer& RequiredBones, const TMap<FName, FRotator>& Rotations);
};

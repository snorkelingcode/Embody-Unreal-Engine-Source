#include "ProceduralAnimInstance.h"
#include "ProceduralAnimComponent.h"
#include "Components/SkeletalMeshComponent.h"

void UProceduralAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	AActor* Owner = GetOwningActor();
	if (Owner)
	{
		ProceduralAnimComp = Owner->FindComponentByClass<UProceduralAnimComponent>();

		if (ProceduralAnimComp.IsValid())
		{
			UE_LOG(LogTemp, Log, TEXT("[MAN] ProceduralAnimInstance found component on '%s'"), *Owner->GetName());
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[MAN] ProceduralAnimInstance could NOT find ProceduralAnimComponent on '%s'"), *Owner->GetName());
		}
	}

	// Log the component hierarchy to find the right skeletal mesh
	if (Owner)
	{
		UE_LOG(LogTemp, Log, TEXT("[MAN] === Component Hierarchy for '%s' (Class: %s) ==="), *Owner->GetName(), *Owner->GetClass()->GetName());
		TArray<UActorComponent*> AllComponents;
		Owner->GetComponents(AllComponents);
		for (UActorComponent* Comp : AllComponents)
		{
			if (USkeletalMeshComponent* SMC = Cast<USkeletalMeshComponent>(Comp))
			{
				FString MeshName = SMC->GetSkeletalMeshAsset() ? SMC->GetSkeletalMeshAsset()->GetName() : TEXT("None");
				FString AnimClassName = SMC->GetAnimInstance() ? SMC->GetAnimInstance()->GetClass()->GetName() : TEXT("None");
				UE_LOG(LogTemp, Log, TEXT("[MAN]   SkeletalMesh: '%s' | Asset: '%s' | AnimInstance: '%s' | IsThis: %s"),
					*SMC->GetName(), *MeshName, *AnimClassName,
					(SMC == GetSkelMeshComponent()) ? TEXT("YES") : TEXT("NO"));
			}
		}
		UE_LOG(LogTemp, Log, TEXT("[MAN] === End Component Hierarchy ==="));
	}

	// Log all bone names from the skeleton
	USkeletalMeshComponent* SkelMesh = GetSkelMeshComponent();
	if (SkelMesh && SkelMesh->GetSkeletalMeshAsset())
	{
		const FReferenceSkeleton& RefSkel = SkelMesh->GetSkeletalMeshAsset()->GetRefSkeleton();
		int32 NumBones = RefSkel.GetNum();
		UE_LOG(LogTemp, Log, TEXT("[MAN] === Skeleton Bones (%d total) ==="), NumBones);
		for (int32 i = 0; i < NumBones; i++)
		{
			FName BoneName = RefSkel.GetBoneName(i);
			int32 ParentIdx = RefSkel.GetParentIndex(i);
			FName ParentName = (ParentIdx != INDEX_NONE) ? RefSkel.GetBoneName(ParentIdx) : FName(TEXT("ROOT"));
			UE_LOG(LogTemp, Log, TEXT("[MAN]   [%d] %s (parent: %s)"), i, *BoneName.ToString(), *ParentName.ToString());
		}
		UE_LOG(LogTemp, Log, TEXT("[MAN] === End Skeleton Bones ==="));
	}
}

void UProceduralAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	// Re-find component if we lost it
	if (!ProceduralAnimComp.IsValid())
	{
		AActor* Owner = GetOwningActor();
		if (Owner)
		{
			ProceduralAnimComp = Owner->FindComponentByClass<UProceduralAnimComponent>();
			if (ProceduralAnimComp.IsValid())
			{
				UE_LOG(LogTemp, Log, TEXT("[MAN] ProceduralAnimInstance late-found component on '%s'"), *Owner->GetName());
			}
		}
	}

	if (ProceduralAnimComp.IsValid())
	{
		BoneRotationOverrides = ProceduralAnimComp->GetAllBoneRotations();

		// Debug: log when we have data
		static int32 LastCount = -1;
		int32 Count = BoneRotationOverrides.Num();
		if (Count != LastCount)
		{
			UE_LOG(LogTemp, Log, TEXT("[MAN] AnimInstance update: Component has %d bone overrides (Component: %p on Actor: %s)"),
				Count, ProceduralAnimComp.Get(), *ProceduralAnimComp->GetOwner()->GetName());
			for (const auto& Pair : BoneRotationOverrides)
			{
				UE_LOG(LogTemp, Log, TEXT("[MAN]   -> '%s': (%.1f, %.1f, %.1f)"), *Pair.Key.ToString(), Pair.Value.Pitch, Pair.Value.Yaw, Pair.Value.Roll);
			}
			LastCount = Count;
		}
	}
	else
	{
		BoneRotationOverrides.Empty();
	}
}

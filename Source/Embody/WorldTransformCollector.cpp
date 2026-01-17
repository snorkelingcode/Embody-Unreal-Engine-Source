#include "WorldTransformCollector.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Components/StaticMeshComponent.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"

AWorldTransformCollector::AWorldTransformCollector()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AWorldTransformCollector::BeginPlay()
{
	Super::BeginPlay();

	if (bCollectOnBeginPlay)
	{
		FTimerHandle DelayHandle;
		GetWorld()->GetTimerManager().SetTimer(DelayHandle, [this]()
		{
			CollectStaticMeshTransforms();

			if (bIncludeAllActors)
			{
				CollectAllActorTransforms();
			}

			if (bAutoExportToFile)
			{
				ExportStaticMeshTransformsToFile(ExportFileName);
			}
		}, CollectionDelay, false);
	}
}

void AWorldTransformCollector::CollectStaticMeshTransforms()
{
	StaticMeshTransforms.Empty();

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (TActorIterator<AStaticMeshActor> It(World); It; ++It)
	{
		AStaticMeshActor* MeshActor = *It;

		if (!MeshActor)
		{
			continue;
		}

		// Get the static mesh component and its world transform
		UStaticMeshComponent* MeshComp = MeshActor->GetStaticMeshComponent();
		if (!MeshComp)
		{
			continue;
		}

		FTransform ComponentTransform = MeshComp->GetComponentTransform();

		// Validate transform to avoid matrix inversion errors
		FVector Scale = ComponentTransform.GetScale3D();
		if (Scale.ContainsNaN() || Scale.IsNearlyZero(KINDA_SMALL_NUMBER))
		{
			UE_LOG(LogTemp, Warning, TEXT("WorldTransformCollector: Skipping actor '%s' with invalid scale"), *MeshActor->GetName());
			continue;
		}

		FStaticMeshTransformData TransformData;
		TransformData.MeshName = MeshActor->GetName();
		TransformData.WorldTransform = ComponentTransform;
		TransformData.MeshActorReference = MeshActor;

		// Get the static mesh asset name
		if (MeshComp->GetStaticMesh())
		{
			TransformData.AssetName = MeshComp->GetStaticMesh()->GetName();
		}
		else
		{
			TransformData.AssetName = TEXT("None");
		}

		// Debug logging to verify rotation values
		FRotator ActorRot = MeshActor->GetActorRotation();
		FRotator CompRot = ComponentTransform.Rotator();
		UE_LOG(LogTemp, Log, TEXT("%s - Actor Rot: P=%.2f Y=%.2f R=%.2f | Component Rot: P=%.2f Y=%.2f R=%.2f"),
			*MeshActor->GetName(),
			ActorRot.Pitch, ActorRot.Yaw, ActorRot.Roll,
			CompRot.Pitch, CompRot.Yaw, CompRot.Roll);

		StaticMeshTransforms.Add(TransformData);
	}

	UE_LOG(LogTemp, Log, TEXT("WorldTransformCollector: Collected %d static mesh transforms"), StaticMeshTransforms.Num());
}

void AWorldTransformCollector::CollectStaticMeshTransformsByTag(FName Tag)
{
	StaticMeshTransforms.Empty();

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (TActorIterator<AStaticMeshActor> It(World); It; ++It)
	{
		AStaticMeshActor* MeshActor = *It;

		if (!MeshActor || !MeshActor->ActorHasTag(Tag))
		{
			continue;
		}

		// Get the static mesh component and its world transform
		UStaticMeshComponent* MeshComp = MeshActor->GetStaticMeshComponent();
		if (!MeshComp)
		{
			continue;
		}

		FTransform ComponentTransform = MeshComp->GetComponentTransform();

		// Validate transform to avoid matrix inversion errors
		FVector Scale = ComponentTransform.GetScale3D();
		if (Scale.ContainsNaN() || Scale.IsNearlyZero(KINDA_SMALL_NUMBER))
		{
			UE_LOG(LogTemp, Warning, TEXT("WorldTransformCollector: Skipping actor '%s' with invalid scale"), *MeshActor->GetName());
			continue;
		}

		FStaticMeshTransformData TransformData;
		TransformData.MeshName = MeshActor->GetName();
		TransformData.WorldTransform = ComponentTransform;
		TransformData.MeshActorReference = MeshActor;

		if (MeshComp->GetStaticMesh())
		{
			TransformData.AssetName = MeshComp->GetStaticMesh()->GetName();
		}
		else
		{
			TransformData.AssetName = TEXT("None");
		}

		StaticMeshTransforms.Add(TransformData);
	}

	UE_LOG(LogTemp, Log, TEXT("WorldTransformCollector: Collected %d static mesh transforms with tag %s"),
		StaticMeshTransforms.Num(), *Tag.ToString());
}

void AWorldTransformCollector::CollectAllActorTransforms()
{
	ActorTransforms.Empty();

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;

		if (!Actor || Actor == this)
		{
			continue;
		}

		FActorTransformData TransformData;
		TransformData.ActorName = Actor->GetName();
		TransformData.ActorClass = Actor->GetClass()->GetName();
		TransformData.WorldTransform = Actor->GetActorTransform();
		TransformData.ActorReference = Actor;

		ActorTransforms.Add(TransformData);
	}

	UE_LOG(LogTemp, Log, TEXT("WorldTransformCollector: Collected %d actor transforms"), ActorTransforms.Num());
}

FString AWorldTransformCollector::GetStaticMeshTransformsAsJSON() const
{
	FString JSONString = TEXT("[");

	for (int32 i = 0; i < StaticMeshTransforms.Num(); i++)
	{
		const FStaticMeshTransformData& Data = StaticMeshTransforms[i];
		FVector Location = Data.WorldTransform.GetLocation();
		FRotator Rotation = Data.WorldTransform.Rotator();
		FQuat Quaternion = Data.WorldTransform.GetRotation();
		FVector Scale = Data.WorldTransform.GetScale3D();

		FString MeshJSON = FString::Printf(
			TEXT("{\"name\":\"%s\",\"asset\":\"%s\",\"location\":{\"x\":%.4f,\"y\":%.4f,\"z\":%.4f},\"rotation\":{\"x\":%.4f,\"y\":%.4f,\"z\":%.4f},\"quaternion\":{\"x\":%.6f,\"y\":%.6f,\"z\":%.6f,\"w\":%.6f},\"scale\":{\"x\":%.4f,\"y\":%.4f,\"z\":%.4f}}"),
			*Data.MeshName,
			*Data.AssetName,
			Location.X, Location.Y, Location.Z,
			Rotation.Pitch, Rotation.Yaw, Rotation.Roll,
			Quaternion.X, Quaternion.Y, Quaternion.Z, Quaternion.W,
			Scale.X, Scale.Y, Scale.Z
		);

		JSONString += MeshJSON;

		if (i < StaticMeshTransforms.Num() - 1)
		{
			JSONString += TEXT(",");
		}
	}

	JSONString += TEXT("]");
	return JSONString;
}

bool AWorldTransformCollector::ExportStaticMeshTransformsToFile(const FString& FileName)
{
	FString JSONString = GetStaticMeshTransformsAsJSON();

	// Save to project's Saved folder
	FString FilePath = FPaths::ProjectSavedDir() + FileName;

	if (FFileHelper::SaveStringToFile(JSONString, *FilePath))
	{
		UE_LOG(LogTemp, Log, TEXT("WorldTransformCollector: Successfully exported to %s"), *FilePath);
		return true;
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("WorldTransformCollector: Failed to export to %s"), *FilePath);
		return false;
	}
}

void AWorldTransformCollector::ClearCollectedData()
{
	StaticMeshTransforms.Empty();
	ActorTransforms.Empty();
}

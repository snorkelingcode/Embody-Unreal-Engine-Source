#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/StaticMeshActor.h"
#include "WorldTransformCollector.generated.h"

USTRUCT(BlueprintType)
struct FStaticMeshTransformData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Transform")
	FString MeshName;

	UPROPERTY(BlueprintReadOnly, Category = "Transform")
	FString AssetName;

	UPROPERTY(BlueprintReadOnly, Category = "Transform")
	FTransform WorldTransform;

	UPROPERTY(BlueprintReadOnly, Category = "Transform")
	AStaticMeshActor* MeshActorReference = nullptr;
};

USTRUCT(BlueprintType)
struct FActorTransformData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Transform")
	FString ActorName;

	UPROPERTY(BlueprintReadOnly, Category = "Transform")
	FString ActorClass;

	UPROPERTY(BlueprintReadOnly, Category = "Transform")
	FTransform WorldTransform;

	UPROPERTY(BlueprintReadOnly, Category = "Transform")
	AActor* ActorReference = nullptr;
};

UCLASS()
class EMBODY_API AWorldTransformCollector : public AActor
{
	GENERATED_BODY()

public:
	AWorldTransformCollector();

protected:
	virtual void BeginPlay() override;

public:
	// Collects transforms from all static meshes placed in the level
	UFUNCTION(BlueprintCallable, Category = "Transform Collector")
	void CollectStaticMeshTransforms();

	// Collects transforms from static meshes with a specific tag
	UFUNCTION(BlueprintCallable, Category = "Transform Collector")
	void CollectStaticMeshTransformsByTag(FName Tag);

	// Collects all world transforms from all actors in the level
	UFUNCTION(BlueprintCallable, Category = "Transform Collector")
	void CollectAllActorTransforms();

	// Returns the collected static mesh transform data
	UFUNCTION(BlueprintCallable, Category = "Transform Collector")
	TArray<FStaticMeshTransformData> GetStaticMeshTransforms() const { return StaticMeshTransforms; }

	// Returns the collected actor transform data
	UFUNCTION(BlueprintCallable, Category = "Transform Collector")
	TArray<FActorTransformData> GetActorTransforms() const { return ActorTransforms; }

	// Returns static mesh transforms as a JSON string
	UFUNCTION(BlueprintCallable, Category = "Transform Collector")
	FString GetStaticMeshTransformsAsJSON() const;

	// Exports static mesh transforms to a JSON file
	UFUNCTION(BlueprintCallable, Category = "Transform Collector")
	bool ExportStaticMeshTransformsToFile(const FString& FileName);

	// Clears all collected data
	UFUNCTION(BlueprintCallable, Category = "Transform Collector")
	void ClearCollectedData();

	// If true, automatically collect static mesh transforms on BeginPlay
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transform Collector")
	bool bCollectOnBeginPlay = true;

	// If true, also collect all actor transforms on BeginPlay
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transform Collector")
	bool bIncludeAllActors = false;

	// If true, automatically export to JSON file after collecting
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transform Collector")
	bool bAutoExportToFile = false;

	// Default filename for JSON export (saved in project's Saved folder)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transform Collector")
	FString ExportFileName = TEXT("TransformData.json");

	// Delay in seconds before collecting transforms on BeginPlay (allows actors to initialize)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transform Collector")
	float CollectionDelay = 0.5f;

private:
	UPROPERTY()
	TArray<FStaticMeshTransformData> StaticMeshTransforms;

	UPROPERTY()
	TArray<FActorTransformData> ActorTransforms;
};

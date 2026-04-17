// BlueprintIntrospectionBFL.h
// Editor-only utility exposing Blueprint internals to Python:
//   - Read event graph nodes (type, title, connections)
//   - Read SCS component hierarchy (names, types, mesh/groom refs)
//   - Swap skeletal mesh / groom asset on named SCS components
//   - Read/write material overrides on SCS components

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "BlueprintIntrospectionBFL.generated.h"

USTRUCT(BlueprintType)
struct FBPNodeInfo
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FString NodeType;

    UPROPERTY(BlueprintReadOnly)
    FString NodeTitle;

    UPROPERTY(BlueprintReadOnly)
    FString NodeComment;

    UPROPERTY(BlueprintReadOnly)
    int32 NodePosX = 0;

    UPROPERTY(BlueprintReadOnly)
    int32 NodePosY = 0;
};

USTRUCT(BlueprintType)
struct FBPComponentInfo
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FString VariableName;

    UPROPERTY(BlueprintReadOnly)
    FString ComponentClass;

    UPROPERTY(BlueprintReadOnly)
    FString ParentVariableName;

    UPROPERTY(BlueprintReadOnly)
    FString MeshAssetPath;

    UPROPERTY(BlueprintReadOnly)
    FString GroomAssetPath;

    UPROPERTY(BlueprintReadOnly)
    FString GroomBindingPath;

    UPROPERTY(BlueprintReadOnly)
    FString AnimClassPath;

    UPROPERTY(BlueprintReadOnly)
    TArray<FString> MaterialOverrides;

    UPROPERTY(BlueprintReadOnly)
    FVector RelativeLocation = FVector::ZeroVector;

    UPROPERTY(BlueprintReadOnly)
    FRotator RelativeRotation = FRotator::ZeroRotator;

    UPROPERTY(BlueprintReadOnly)
    FVector RelativeScale = FVector::OneVector;

    /** SpringArm only: target arm length */
    UPROPERTY(BlueprintReadOnly)
    float ArmLength = 0.f;

    /** SpringArm only: use pawn control rotation */
    UPROPERTY(BlueprintReadOnly)
    bool bUsePawnControlRotation = false;
};

UCLASS()
class EMBODYEDITOR_API UBlueprintIntrospectionBFL : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    // -----------------------------------------------------------------------
    // Graph reading
    // -----------------------------------------------------------------------

    /** Get all nodes from the event graph of a blueprint. */
    UFUNCTION(BlueprintCallable, Category = "Embody|BP Introspection")
    static TArray<FBPNodeInfo> GetEventGraphNodes(UBlueprint* Blueprint);

    /** Get all nodes from a named graph in a blueprint. */
    UFUNCTION(BlueprintCallable, Category = "Embody|BP Introspection")
    static TArray<FBPNodeInfo> GetGraphNodes(UBlueprint* Blueprint, const FString& GraphName);

    /** Get names of all graphs in a blueprint. */
    UFUNCTION(BlueprintCallable, Category = "Embody|BP Introspection")
    static TArray<FString> GetGraphNames(UBlueprint* Blueprint);

    /** Get names of all variables in a blueprint. */
    UFUNCTION(BlueprintCallable, Category = "Embody|BP Introspection")
    static TArray<FString> GetVariableNames(UBlueprint* Blueprint);

    // -----------------------------------------------------------------------
    // Graph copy/paste (export nodes as text, import into another BP)
    // -----------------------------------------------------------------------

    /** Export all nodes from a blueprint's event graph as copy/paste text. */
    UFUNCTION(BlueprintCallable, Category = "Embody|BP Introspection")
    static FString ExportEventGraph(UBlueprint* Blueprint);

    /** Export all nodes from a named graph as copy/paste text. */
    UFUNCTION(BlueprintCallable, Category = "Embody|BP Introspection")
    static FString ExportGraph(UBlueprint* Blueprint, const FString& GraphName);

    /** Import nodes from copy/paste text into a blueprint's event graph.
     *  Merges with existing nodes (does not clear first). */
    UFUNCTION(BlueprintCallable, Category = "Embody|BP Introspection")
    static bool ImportIntoEventGraph(UBlueprint* Blueprint, const FString& NodesText);

    /** Clear all nodes from a blueprint's event graph. */
    UFUNCTION(BlueprintCallable, Category = "Embody|BP Introspection")
    static bool ClearEventGraph(UBlueprint* Blueprint);

    /** Copy all variables from one blueprint to another (skips duplicates). */
    UFUNCTION(BlueprintCallable, Category = "Embody|BP Introspection")
    static int32 CopyVariables(UBlueprint* SourceBP, UBlueprint* TargetBP);

    /** Copy all function graphs from one blueprint to another. */
    UFUNCTION(BlueprintCallable, Category = "Embody|BP Introspection")
    static int32 CopyFunctionGraphs(UBlueprint* SourceBP, UBlueprint* TargetBP);

    /** Refresh all nodes in a blueprint (reconstructs stale pins after import). */
    UFUNCTION(BlueprintCallable, Category = "Embody|BP Introspection")
    static void RefreshAllNodes(UBlueprint* Blueprint);

    /** Copy SCS component nodes from source BP to target BP (skips nodes that
     *  already exist by variable name). Preserves parent-child relationships. */
    UFUNCTION(BlueprintCallable, Category = "Embody|BP Introspection")
    static int32 CopySCSNodes(UBlueprint* SourceBP, UBlueprint* TargetBP);

    // -----------------------------------------------------------------------
    // SCS component reading
    // -----------------------------------------------------------------------

    /** Get info about all SCS components in a blueprint. */
    UFUNCTION(BlueprintCallable, Category = "Embody|BP Introspection")
    static TArray<FBPComponentInfo> GetSCSComponents(UBlueprint* Blueprint);

    /** Get the mesh asset path for a named SCS component. */
    UFUNCTION(BlueprintCallable, Category = "Embody|BP Introspection")
    static FString GetComponentMeshPath(UBlueprint* Blueprint, const FString& ComponentName);

    // -----------------------------------------------------------------------
    // SCS component writing (swap mesh/groom/material)
    // -----------------------------------------------------------------------

    /** Set the skeletal mesh on a named SCS component. Returns true on success. */
    UFUNCTION(BlueprintCallable, Category = "Embody|BP Introspection")
    static bool SetComponentSkeletalMesh(UBlueprint* Blueprint, const FString& ComponentName, USkeletalMesh* NewMesh);

    /** Set the groom asset on a named SCS GroomComponent. Returns true on success. */
    UFUNCTION(BlueprintCallable, Category = "Embody|BP Introspection")
    static bool SetComponentGroomAsset(UBlueprint* Blueprint, const FString& ComponentName, UObject* NewGroomAsset);

    /** Set a material override at a specific slot index on a named SCS component. */
    UFUNCTION(BlueprintCallable, Category = "Embody|BP Introspection")
    static bool SetComponentMaterialOverride(UBlueprint* Blueprint, const FString& ComponentName, int32 SlotIndex, UMaterialInterface* NewMaterial);

    /** Set the groom binding on a named SCS GroomComponent. */
    UFUNCTION(BlueprintCallable, Category = "Embody|BP Introspection")
    static bool SetComponentGroomBinding(UBlueprint* Blueprint, const FString& ComponentName, UObject* NewBinding);

    /** Set the anim class on a named SCS SkeletalMeshComponent. */
    UFUNCTION(BlueprintCallable, Category = "Embody|BP Introspection")
    static bool SetComponentAnimClass(UBlueprint* Blueprint, const FString& ComponentName, UClass* NewAnimClass);

    /** Reparent an SCS node to be a child of another SCS node (by variable name).
     *  Use ParentName="" to attach to a native component instead (set NativeParentName). */
    UFUNCTION(BlueprintCallable, Category = "Embody|BP Introspection")
    static bool SetSCSNodeParent(UBlueprint* Blueprint, const FString& ChildName, const FString& ParentName);

    /** Attach an SCS node to an inherited/native component (e.g. "CharacterMesh0"). */
    UFUNCTION(BlueprintCallable, Category = "Embody|BP Introspection")
    static bool AttachSCSNodeToNative(UBlueprint* Blueprint, const FString& ChildName, const FString& NativeComponentName);

    /** Set the relative transform on a named SCS scene component. */
    UFUNCTION(BlueprintCallable, Category = "Embody|BP Introspection")
    static bool SetComponentTransform(UBlueprint* Blueprint, const FString& ComponentName,
        FVector Location, FRotator Rotation, FVector Scale);

    /** Set transform using explicit pitch/yaw/roll floats (avoids Python Rotator ambiguity). */
    UFUNCTION(BlueprintCallable, Category = "Embody|BP Introspection")
    static bool SetComponentTransformExplicit(UBlueprint* Blueprint, const FString& ComponentName,
        FVector Location, float Pitch, float Yaw, float Roll, FVector Scale);

private:
    /** Find an SCS node by variable name. */
    static USCS_Node* FindSCSNode(UBlueprint* Blueprint, const FString& ComponentName);
};

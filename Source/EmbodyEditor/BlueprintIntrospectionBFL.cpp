// BlueprintIntrospectionBFL.cpp

#include "BlueprintIntrospectionBFL.h"
#include "Engine/Blueprint.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraphUtilities.h"
#include "K2Node.h"
#include "K2Node_Event.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Materials/MaterialInterface.h"

// -----------------------------------------------------------------------
// Helpers
// -----------------------------------------------------------------------

USCS_Node* UBlueprintIntrospectionBFL::FindSCSNode(UBlueprint* Blueprint, const FString& ComponentName)
{
    if (!Blueprint || !Blueprint->SimpleConstructionScript)
    {
        return nullptr;
    }

    const TArray<USCS_Node*>& AllNodes = Blueprint->SimpleConstructionScript->GetAllNodes();
    for (USCS_Node* Node : AllNodes)
    {
        if (Node && Node->GetVariableName().ToString() == ComponentName)
        {
            return Node;
        }
    }
    return nullptr;
}

// -----------------------------------------------------------------------
// Graph reading
// -----------------------------------------------------------------------

TArray<FString> UBlueprintIntrospectionBFL::GetGraphNames(UBlueprint* Blueprint)
{
    TArray<FString> Result;
    if (!Blueprint) return Result;

    for (UEdGraph* Graph : Blueprint->UbergraphPages)
    {
        Result.Add(Graph->GetName());
    }
    for (UEdGraph* Graph : Blueprint->FunctionGraphs)
    {
        Result.Add(Graph->GetName());
    }
    return Result;
}

TArray<FBPNodeInfo> UBlueprintIntrospectionBFL::GetEventGraphNodes(UBlueprint* Blueprint)
{
    TArray<FBPNodeInfo> Result;
    if (!Blueprint) return Result;

    for (UEdGraph* Graph : Blueprint->UbergraphPages)
    {
        if (Graph->GetName() == TEXT("EventGraph"))
        {
            return GetGraphNodes(Blueprint, TEXT("EventGraph"));
        }
    }
    // If no "EventGraph", return first ubergraph
    if (Blueprint->UbergraphPages.Num() > 0)
    {
        return GetGraphNodes(Blueprint, Blueprint->UbergraphPages[0]->GetName());
    }
    return Result;
}

TArray<FBPNodeInfo> UBlueprintIntrospectionBFL::GetGraphNodes(UBlueprint* Blueprint, const FString& GraphName)
{
    TArray<FBPNodeInfo> Result;
    if (!Blueprint) return Result;

    UEdGraph* TargetGraph = nullptr;

    for (UEdGraph* Graph : Blueprint->UbergraphPages)
    {
        if (Graph->GetName() == GraphName)
        {
            TargetGraph = Graph;
            break;
        }
    }
    if (!TargetGraph)
    {
        for (UEdGraph* Graph : Blueprint->FunctionGraphs)
        {
            if (Graph->GetName() == GraphName)
            {
                TargetGraph = Graph;
                break;
            }
        }
    }
    if (!TargetGraph) return Result;

    for (UEdGraphNode* Node : TargetGraph->Nodes)
    {
        FBPNodeInfo Info;
        Info.NodeType = Node->GetClass()->GetName();
        Info.NodeTitle = Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString();
        Info.NodeComment = Node->NodeComment;
        Info.NodePosX = Node->NodePosX;
        Info.NodePosY = Node->NodePosY;
        Result.Add(Info);
    }

    return Result;
}

TArray<FString> UBlueprintIntrospectionBFL::GetVariableNames(UBlueprint* Blueprint)
{
    TArray<FString> Result;
    if (!Blueprint) return Result;

    for (const FBPVariableDescription& Var : Blueprint->NewVariables)
    {
        Result.Add(Var.VarName.ToString());
    }
    return Result;
}

// -----------------------------------------------------------------------
// Graph copy/paste
// -----------------------------------------------------------------------

FString UBlueprintIntrospectionBFL::ExportEventGraph(UBlueprint* Blueprint)
{
    if (!Blueprint) return FString();

    for (UEdGraph* Graph : Blueprint->UbergraphPages)
    {
        if (Graph->GetName() == TEXT("EventGraph"))
        {
            return ExportGraph(Blueprint, TEXT("EventGraph"));
        }
    }
    if (Blueprint->UbergraphPages.Num() > 0)
    {
        return ExportGraph(Blueprint, Blueprint->UbergraphPages[0]->GetName());
    }
    return FString();
}

FString UBlueprintIntrospectionBFL::ExportGraph(UBlueprint* Blueprint, const FString& GraphName)
{
    if (!Blueprint) return FString();

    UEdGraph* TargetGraph = nullptr;
    for (UEdGraph* Graph : Blueprint->UbergraphPages)
    {
        if (Graph->GetName() == GraphName) { TargetGraph = Graph; break; }
    }
    if (!TargetGraph)
    {
        for (UEdGraph* Graph : Blueprint->FunctionGraphs)
        {
            if (Graph->GetName() == GraphName) { TargetGraph = Graph; break; }
        }
    }
    if (!TargetGraph || TargetGraph->Nodes.Num() == 0) return FString();

    TSet<UObject*> NodesToExport;
    for (UEdGraphNode* Node : TargetGraph->Nodes)
    {
        NodesToExport.Add(Node);
    }

    FString ExportedText;
    FEdGraphUtilities::ExportNodesToText(NodesToExport, ExportedText);
    return ExportedText;
}

bool UBlueprintIntrospectionBFL::ImportIntoEventGraph(UBlueprint* Blueprint, const FString& NodesText)
{
    if (!Blueprint || NodesText.IsEmpty()) return false;

    UEdGraph* EventGraph = nullptr;
    for (UEdGraph* Graph : Blueprint->UbergraphPages)
    {
        if (Graph->GetName() == TEXT("EventGraph"))
        {
            EventGraph = Graph;
            break;
        }
    }
    if (!EventGraph && Blueprint->UbergraphPages.Num() > 0)
    {
        EventGraph = Blueprint->UbergraphPages[0];
    }
    if (!EventGraph) return false;

    TSet<UEdGraphNode*> PastedNodes;
    FEdGraphUtilities::ImportNodesFromText(EventGraph, NodesText, PastedNodes);

    // Notify the blueprint that nodes were added
    for (UEdGraphNode* Node : PastedNodes)
    {
        Node->GetGraph()->NotifyGraphChanged();
    }

    Blueprint->Modify();
    return PastedNodes.Num() > 0;
}

bool UBlueprintIntrospectionBFL::ClearEventGraph(UBlueprint* Blueprint)
{
    if (!Blueprint) return false;

    UEdGraph* EventGraph = nullptr;
    for (UEdGraph* Graph : Blueprint->UbergraphPages)
    {
        if (Graph->GetName() == TEXT("EventGraph"))
        {
            EventGraph = Graph;
            break;
        }
    }
    if (!EventGraph) return false;

    // Remove all nodes except the default event nodes that UE auto-creates
    TArray<UEdGraphNode*> NodesToRemove;
    for (UEdGraphNode* Node : EventGraph->Nodes)
    {
        NodesToRemove.Add(Node);
    }

    for (UEdGraphNode* Node : NodesToRemove)
    {
        EventGraph->RemoveNode(Node);
    }

    Blueprint->Modify();
    return true;
}

int32 UBlueprintIntrospectionBFL::CopyVariables(UBlueprint* SourceBP, UBlueprint* TargetBP)
{
    if (!SourceBP || !TargetBP) return 0;

    int32 Count = 0;
    for (const FBPVariableDescription& SrcVar : SourceBP->NewVariables)
    {
        // Skip if variable already exists
        bool bExists = false;
        for (const FBPVariableDescription& TgtVar : TargetBP->NewVariables)
        {
            if (TgtVar.VarName == SrcVar.VarName)
            {
                bExists = true;
                break;
            }
        }
        if (!bExists)
        {
            TargetBP->NewVariables.Add(SrcVar);
            Count++;
        }
    }

    if (Count > 0)
    {
        TargetBP->Modify();
    }
    return Count;
}

int32 UBlueprintIntrospectionBFL::CopyFunctionGraphs(UBlueprint* SourceBP, UBlueprint* TargetBP)
{
    if (!SourceBP || !TargetBP) return 0;

    int32 Count = 0;
    for (UEdGraph* SrcGraph : SourceBP->FunctionGraphs)
    {
        // Skip UserConstructionScript (auto-generated)
        if (SrcGraph->GetName() == TEXT("UserConstructionScript")) continue;

        // Skip if already exists
        bool bExists = false;
        for (UEdGraph* TgtGraph : TargetBP->FunctionGraphs)
        {
            if (TgtGraph->GetName() == SrcGraph->GetName())
            {
                bExists = true;
                break;
            }
        }
        if (bExists) continue;

        // Export from source, create new graph in target, import
        TSet<UObject*> NodesToExport;
        for (UEdGraphNode* Node : SrcGraph->Nodes)
        {
            NodesToExport.Add(Node);
        }
        FString ExportedText;
        FEdGraphUtilities::ExportNodesToText(NodesToExport, ExportedText);

        // Create the function graph in target
        UEdGraph* NewGraph = FBlueprintEditorUtils::CreateNewGraph(
            TargetBP, FName(*SrcGraph->GetName()), UEdGraph::StaticClass(), SrcGraph->Schema ? SrcGraph->Schema->GetClass() : nullptr);

        if (NewGraph)
        {
            TargetBP->FunctionGraphs.Add(NewGraph);

            TSet<UEdGraphNode*> PastedNodes;
            FEdGraphUtilities::ImportNodesFromText(NewGraph, ExportedText, PastedNodes);
            Count++;
        }
    }

    if (Count > 0)
    {
        TargetBP->Modify();
    }
    return Count;
}

void UBlueprintIntrospectionBFL::RefreshAllNodes(UBlueprint* Blueprint)
{
    if (!Blueprint) return;
    FBlueprintEditorUtils::RefreshAllNodes(Blueprint);
    Blueprint->Modify();
}

int32 UBlueprintIntrospectionBFL::CopySCSNodes(UBlueprint* SourceBP, UBlueprint* TargetBP)
{
    if (!SourceBP || !TargetBP) return 0;
    if (!SourceBP->SimpleConstructionScript || !TargetBP->SimpleConstructionScript) return 0;

    USimpleConstructionScript* SrcSCS = SourceBP->SimpleConstructionScript;
    USimpleConstructionScript* TgtSCS = TargetBP->SimpleConstructionScript;

    const TArray<USCS_Node*>& SrcNodes = SrcSCS->GetAllNodes();
    const TArray<USCS_Node*>& TgtNodes = TgtSCS->GetAllNodes();

    // Build set of existing variable names in target
    TSet<FName> ExistingNames;
    for (USCS_Node* Node : TgtNodes)
    {
        if (Node)
        {
            ExistingNames.Add(Node->GetVariableName());
        }
    }

    // First pass: create all missing nodes (without parent relationships)
    TMap<FName, USCS_Node*> NewNodeMap; // SrcVarName -> new TgtNode
    int32 Count = 0;

    for (USCS_Node* SrcNode : SrcNodes)
    {
        if (!SrcNode || !SrcNode->ComponentTemplate) continue;

        FName VarName = SrcNode->GetVariableName();
        if (ExistingNames.Contains(VarName)) continue;

        // Create a new SCS node by duplicating the source's component template
        USCS_Node* NewNode = TgtSCS->CreateNode(
            SrcNode->ComponentTemplate->GetClass(),
            VarName
        );

        if (!NewNode) continue;

        // Copy all properties from source component template to new one
        if (NewNode->ComponentTemplate && SrcNode->ComponentTemplate)
        {
            UEngine::CopyPropertiesForUnrelatedObjects(
                SrcNode->ComponentTemplate,
                NewNode->ComponentTemplate
            );
        }

        // Copy attachment info
        NewNode->AttachToName = SrcNode->AttachToName;

        NewNodeMap.Add(VarName, NewNode);
        Count++;
    }

    // Second pass: set up parent-child relationships
    for (USCS_Node* SrcNode : SrcNodes)
    {
        if (!SrcNode) continue;
        FName VarName = SrcNode->GetVariableName();
        USCS_Node** NewNodePtr = NewNodeMap.Find(VarName);
        if (!NewNodePtr) continue;
        USCS_Node* NewNode = *NewNodePtr;

        // Find the parent in the source
        USCS_Node* SrcParent = nullptr;
        for (USCS_Node* PotentialParent : SrcNodes)
        {
            if (PotentialParent && PotentialParent->ChildNodes.Contains(SrcNode))
            {
                SrcParent = PotentialParent;
                break;
            }
        }

        if (SrcParent)
        {
            FName ParentVarName = SrcParent->GetVariableName();

            // Check if parent exists in target (either existing or newly created)
            USCS_Node* TgtParent = nullptr;

            // Look in existing target nodes
            for (USCS_Node* TgtNode : TgtNodes)
            {
                if (TgtNode && TgtNode->GetVariableName() == ParentVarName)
                {
                    TgtParent = TgtNode;
                    break;
                }
            }

            // Look in newly created nodes
            if (!TgtParent)
            {
                USCS_Node** NewParentPtr = NewNodeMap.Find(ParentVarName);
                if (NewParentPtr)
                {
                    TgtParent = *NewParentPtr;
                }
            }

            if (TgtParent)
            {
                TgtParent->AddChildNode(NewNode);
            }
            else
            {
                // Parent not found — attach to default scene root
                TgtSCS->AddNode(NewNode);
            }
        }
        else
        {
            // No parent in source — it's a root-level node or attached to native component
            if (SrcNode->ParentComponentOrVariableName != NAME_None)
            {
                // Attached to a native component (e.g. CollisionCylinder, CharacterMesh0)
                NewNode->ParentComponentOrVariableName = SrcNode->ParentComponentOrVariableName;
                NewNode->ParentComponentOwnerClassName = SrcNode->ParentComponentOwnerClassName;
                TgtSCS->AddNode(NewNode);
            }
            else
            {
                TgtSCS->AddNode(NewNode);
            }
        }
    }

    if (Count > 0)
    {
        TargetBP->Modify();
    }

    return Count;
}

// -----------------------------------------------------------------------
// SCS component reading
// -----------------------------------------------------------------------

TArray<FBPComponentInfo> UBlueprintIntrospectionBFL::GetSCSComponents(UBlueprint* Blueprint)
{
    TArray<FBPComponentInfo> Result;
    if (!Blueprint || !Blueprint->SimpleConstructionScript) return Result;

    const TArray<USCS_Node*>& AllNodes = Blueprint->SimpleConstructionScript->GetAllNodes();

    for (USCS_Node* Node : AllNodes)
    {
        if (!Node || !Node->ComponentTemplate) continue;

        FBPComponentInfo Info;
        Info.VariableName = Node->GetVariableName().ToString();
        Info.ComponentClass = Node->ComponentTemplate->GetClass()->GetName();

        // Parent
        if (Node->ParentComponentOrVariableName != NAME_None)
        {
            Info.ParentVariableName = Node->ParentComponentOrVariableName.ToString();
        }
        else
        {
            // Check if it's a root node or attached to another SCS node
            for (USCS_Node* PotentialParent : AllNodes)
            {
                if (PotentialParent && PotentialParent->ChildNodes.Contains(Node))
                {
                    Info.ParentVariableName = PotentialParent->GetVariableName().ToString();
                    break;
                }
            }
        }

        // SkeletalMeshComponent
        if (USkeletalMeshComponent* SkelComp = Cast<USkeletalMeshComponent>(Node->ComponentTemplate))
        {
            if (USkeletalMesh* Mesh = SkelComp->GetSkeletalMeshAsset())
            {
                Info.MeshAssetPath = Mesh->GetPathName();
            }
            if (SkelComp->AnimClass)
            {
                Info.AnimClassPath = SkelComp->AnimClass->GetPathName();
            }
            // Material overrides
            for (int32 i = 0; i < SkelComp->OverrideMaterials.Num(); ++i)
            {
                if (SkelComp->OverrideMaterials[i])
                {
                    Info.MaterialOverrides.Add(FString::Printf(TEXT("[%d] %s"), i, *SkelComp->OverrideMaterials[i]->GetPathName()));
                }
            }
        }

        // GroomComponent (accessed generically to avoid hard dependency on HairStrandsCore)
        if (Node->ComponentTemplate->GetClass()->GetName().Contains(TEXT("GroomComponent")))
        {
            // Use reflection to get GroomAsset
            if (FProperty* GroomProp = Node->ComponentTemplate->GetClass()->FindPropertyByName(TEXT("GroomAsset")))
            {
                if (FObjectProperty* ObjProp = CastField<FObjectProperty>(GroomProp))
                {
                    UObject* GroomAsset = ObjProp->GetObjectPropertyValue_InContainer(Node->ComponentTemplate);
                    if (GroomAsset)
                    {
                        Info.GroomAssetPath = GroomAsset->GetPathName();
                    }
                }
            }
            // Use reflection to get BindingAsset
            if (FProperty* BindingProp = Node->ComponentTemplate->GetClass()->FindPropertyByName(TEXT("BindingAsset")))
            {
                if (FObjectProperty* ObjProp = CastField<FObjectProperty>(BindingProp))
                {
                    UObject* BindingAsset = ObjProp->GetObjectPropertyValue_InContainer(Node->ComponentTemplate);
                    if (BindingAsset)
                    {
                        Info.GroomBindingPath = BindingAsset->GetPathName();
                    }
                }
            }
        }

        // Read transform from SceneComponent templates
        if (USceneComponent* SceneComp = Cast<USceneComponent>(Node->ComponentTemplate))
        {
            Info.RelativeLocation = SceneComp->GetRelativeLocation();
            Info.RelativeRotation = SceneComp->GetRelativeRotation();
            Info.RelativeScale = SceneComp->GetRelativeScale3D();
        }

        // SpringArm-specific properties
        if (Node->ComponentTemplate->GetClass()->GetName().Contains(TEXT("SpringArmComponent")))
        {
            if (FProperty* ArmProp = Node->ComponentTemplate->GetClass()->FindPropertyByName(TEXT("TargetArmLength")))
            {
                if (FFloatProperty* FloatProp = CastField<FFloatProperty>(ArmProp))
                {
                    Info.ArmLength = FloatProp->GetPropertyValue_InContainer(Node->ComponentTemplate);
                }
            }
            if (FProperty* PawnRotProp = Node->ComponentTemplate->GetClass()->FindPropertyByName(TEXT("bUsePawnControlRotation")))
            {
                if (FBoolProperty* BoolProp = CastField<FBoolProperty>(PawnRotProp))
                {
                    Info.bUsePawnControlRotation = BoolProp->GetPropertyValue_InContainer(Node->ComponentTemplate);
                }
            }
        }

        Result.Add(Info);
    }

    return Result;
}

FString UBlueprintIntrospectionBFL::GetComponentMeshPath(UBlueprint* Blueprint, const FString& ComponentName)
{
    USCS_Node* Node = FindSCSNode(Blueprint, ComponentName);
    if (!Node || !Node->ComponentTemplate) return FString();

    if (USkeletalMeshComponent* SkelComp = Cast<USkeletalMeshComponent>(Node->ComponentTemplate))
    {
        if (USkeletalMesh* Mesh = SkelComp->GetSkeletalMeshAsset())
        {
            return Mesh->GetPathName();
        }
    }
    return FString();
}

// -----------------------------------------------------------------------
// SCS component writing
// -----------------------------------------------------------------------

bool UBlueprintIntrospectionBFL::SetComponentSkeletalMesh(UBlueprint* Blueprint, const FString& ComponentName, USkeletalMesh* NewMesh)
{
    USCS_Node* Node = FindSCSNode(Blueprint, ComponentName);
    if (!Node || !Node->ComponentTemplate) return false;

    USkeletalMeshComponent* SkelComp = Cast<USkeletalMeshComponent>(Node->ComponentTemplate);
    if (!SkelComp) return false;

    SkelComp->SetSkeletalMeshAsset(NewMesh);
    Blueprint->Modify();
    return true;
}

bool UBlueprintIntrospectionBFL::SetComponentGroomAsset(UBlueprint* Blueprint, const FString& ComponentName, UObject* NewGroomAsset)
{
    USCS_Node* Node = FindSCSNode(Blueprint, ComponentName);
    if (!Node || !Node->ComponentTemplate) return false;

    FProperty* GroomProp = Node->ComponentTemplate->GetClass()->FindPropertyByName(TEXT("GroomAsset"));
    if (!GroomProp) return false;

    FObjectProperty* ObjProp = CastField<FObjectProperty>(GroomProp);
    if (!ObjProp) return false;

    ObjProp->SetObjectPropertyValue_InContainer(Node->ComponentTemplate, NewGroomAsset);
    Blueprint->Modify();
    return true;
}

bool UBlueprintIntrospectionBFL::SetComponentGroomBinding(UBlueprint* Blueprint, const FString& ComponentName, UObject* NewBinding)
{
    USCS_Node* Node = FindSCSNode(Blueprint, ComponentName);
    if (!Node || !Node->ComponentTemplate) return false;

    FProperty* BindingProp = Node->ComponentTemplate->GetClass()->FindPropertyByName(TEXT("BindingAsset"));
    if (!BindingProp) return false;

    FObjectProperty* ObjProp = CastField<FObjectProperty>(BindingProp);
    if (!ObjProp) return false;

    ObjProp->SetObjectPropertyValue_InContainer(Node->ComponentTemplate, NewBinding);
    Blueprint->Modify();
    return true;
}

bool UBlueprintIntrospectionBFL::SetComponentMaterialOverride(UBlueprint* Blueprint, const FString& ComponentName, int32 SlotIndex, UMaterialInterface* NewMaterial)
{
    USCS_Node* Node = FindSCSNode(Blueprint, ComponentName);
    if (!Node || !Node->ComponentTemplate) return false;

    USkeletalMeshComponent* SkelComp = Cast<USkeletalMeshComponent>(Node->ComponentTemplate);
    if (!SkelComp) return false;

    if (SlotIndex < 0) return false;

    // Grow the array if needed
    while (SkelComp->OverrideMaterials.Num() <= SlotIndex)
    {
        SkelComp->OverrideMaterials.Add(nullptr);
    }

    SkelComp->OverrideMaterials[SlotIndex] = NewMaterial;
    Blueprint->Modify();
    return true;
}

bool UBlueprintIntrospectionBFL::SetComponentAnimClass(UBlueprint* Blueprint, const FString& ComponentName, UClass* NewAnimClass)
{
    USCS_Node* Node = FindSCSNode(Blueprint, ComponentName);
    if (!Node || !Node->ComponentTemplate) return false;

    USkeletalMeshComponent* SkelComp = Cast<USkeletalMeshComponent>(Node->ComponentTemplate);
    if (!SkelComp) return false;

    SkelComp->AnimClass = NewAnimClass;
    Blueprint->Modify();
    return true;
}

bool UBlueprintIntrospectionBFL::SetSCSNodeParent(UBlueprint* Blueprint, const FString& ChildName, const FString& ParentName)
{
    if (!Blueprint || !Blueprint->SimpleConstructionScript) return false;

    USCS_Node* ChildNode = FindSCSNode(Blueprint, ChildName);
    USCS_Node* ParentNode = FindSCSNode(Blueprint, ParentName);

    if (!ChildNode) return false;
    if (!ParentNode) return false;

    USimpleConstructionScript* SCS = Blueprint->SimpleConstructionScript;

    // Remove child from its current parent
    for (USCS_Node* Node : SCS->GetAllNodes())
    {
        if (Node && Node->ChildNodes.Contains(ChildNode))
        {
            Node->ChildNodes.Remove(ChildNode);
            break;
        }
    }
    // Also remove from root nodes if it's there
    SCS->RemoveNode(ChildNode, /*bMarkAsModified=*/false);

    // Add as child of new parent
    ParentNode->AddChildNode(ChildNode);

    Blueprint->Modify();
    return true;
}

bool UBlueprintIntrospectionBFL::AttachSCSNodeToNative(UBlueprint* Blueprint, const FString& ChildName, const FString& NativeComponentName)
{
    if (!Blueprint || !Blueprint->SimpleConstructionScript) return false;
    if (!Blueprint->GeneratedClass) return false;

    USCS_Node* ChildNode = FindSCSNode(Blueprint, ChildName);
    if (!ChildNode) return false;

    USimpleConstructionScript* SCS = Blueprint->SimpleConstructionScript;

    // Find the native component on the CDO
    UObject* CDO = Blueprint->GeneratedClass->GetDefaultObject();
    if (!CDO) return false;

    USceneComponent* NativeComp = nullptr;
    AActor* ActorCDO = Cast<AActor>(CDO);
    if (ActorCDO)
    {
        TArray<UActorComponent*> Components;
        ActorCDO->GetComponents(Components);
        for (UActorComponent* Comp : Components)
        {
            if (Comp && Comp->GetFName() == FName(*NativeComponentName))
            {
                NativeComp = Cast<USceneComponent>(Comp);
                break;
            }
        }
    }

    if (!NativeComp) return false;

    // Remove child from its current parent (SCS node or root list)
    const TArray<USCS_Node*> AllNodes = SCS->GetAllNodes();
    for (USCS_Node* Node : AllNodes)
    {
        if (Node && Node != ChildNode && Node->ChildNodes.Contains(ChildNode))
        {
            Node->ChildNodes.Remove(ChildNode);
            break;
        }
    }
    SCS->RemoveNode(ChildNode, /*bMarkAsModified=*/false);

    // Use the proper SetParent(USceneComponent*) which sets bIsParentComponentNative=true
    ChildNode->SetParent(NativeComp);
    SCS->AddNode(ChildNode);

    Blueprint->Modify();
    return true;
}

bool UBlueprintIntrospectionBFL::SetComponentTransform(UBlueprint* Blueprint, const FString& ComponentName,
    FVector Location, FRotator Rotation, FVector Scale)
{
    USCS_Node* Node = FindSCSNode(Blueprint, ComponentName);
    if (!Node || !Node->ComponentTemplate) return false;

    USceneComponent* SceneComp = Cast<USceneComponent>(Node->ComponentTemplate);
    if (!SceneComp) return false;

    SceneComp->SetRelativeLocation_Direct(Location);
    SceneComp->SetRelativeRotation_Direct(Rotation);
    SceneComp->SetRelativeScale3D_Direct(Scale);
    Blueprint->Modify();
    return true;
}

bool UBlueprintIntrospectionBFL::SetComponentTransformExplicit(UBlueprint* Blueprint, const FString& ComponentName,
    FVector Location, float Pitch, float Yaw, float Roll, FVector Scale)
{
    USCS_Node* Node = FindSCSNode(Blueprint, ComponentName);
    if (!Node || !Node->ComponentTemplate) return false;

    USceneComponent* SceneComp = Cast<USceneComponent>(Node->ComponentTemplate);
    if (!SceneComp) return false;

    SceneComp->SetRelativeLocation_Direct(Location);
    SceneComp->SetRelativeRotation_Direct(FRotator(Pitch, Yaw, Roll));
    SceneComp->SetRelativeScale3D_Direct(Scale);
    Blueprint->Modify();
    return true;
}

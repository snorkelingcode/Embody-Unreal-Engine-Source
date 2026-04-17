#pragma once

#include "CoreMinimal.h"
#include "AnimGraphNode_Base.h"
#include "AnimNode_ProceduralBoneDriver.h"
#include "AnimGraphNode_ProceduralBoneDriver.generated.h"

UCLASS()
class EMBODYEDITOR_API UAnimGraphNode_ProceduralBoneDriver : public UAnimGraphNode_Base
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Settings)
	FAnimNode_ProceduralBoneDriver Node;

	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override
	{
		return FText::FromString(TEXT("Procedural Bone Driver"));
	}

	virtual FText GetTooltipText() const override
	{
		return FText::FromString(TEXT("Applies procedural rotation offsets to multiple bones from a TMap. One node drives the entire skeleton."));
	}

	virtual FString GetNodeCategory() const override
	{
		return TEXT("Procedural Animation");
	}
};

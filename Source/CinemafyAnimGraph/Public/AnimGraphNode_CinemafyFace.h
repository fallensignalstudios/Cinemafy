#pragma once

#include "CoreMinimal.h"
#include "AnimGraphNode_Base.h"
#include "AnimNode_CinemafyFace.h"
#include "AnimGraphNode_CinemafyFace.generated.h"

/** Anim Blueprint node for FAnimNode_CinemafyFace. Lives in the face Anim BP, before Output Pose. */
UCLASS()
class CINEMAFYANIMGRAPH_API UAnimGraphNode_CinemafyFace : public UAnimGraphNode_Base
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Settings")
	FAnimNode_CinemafyFace Node;

	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetTooltipText() const override;
	virtual FString GetNodeCategory() const override;
	virtual FLinearColor GetNodeTitleColor() const override;
};

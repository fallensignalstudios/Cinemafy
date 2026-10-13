#include "AnimGraphNode_CinemafyFace.h"

#define LOCTEXT_NAMESPACE "CinemafyAnimGraph"

FText UAnimGraphNode_CinemafyFace::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("Title", "Cinemafy Face");
}

FText UAnimGraphNode_CinemafyFace::GetTooltipText() const
{
	return LOCTEXT("Tooltip",
		"Processes a MetaHuman facial performance before RigLogic: region gain, jitter cleanup, flesh dynamics, "
		"emotions, breathing, face life and pupil response. Also applies Cinemafy Gaze and Listener. "
		"Settings come from the character's Cinemafy Face component.");
}

FString UAnimGraphNode_CinemafyFace::GetNodeCategory() const
{
	return TEXT("Cinemafy");
}

FLinearColor UAnimGraphNode_CinemafyFace::GetNodeTitleColor() const
{
	return FLinearColor(1.f, 0.62f, 0.15f);
}

#undef LOCTEXT_NAMESPACE

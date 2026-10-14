#include "CinemafyCameraOffsetComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"

UCinemafyCameraOffsetComponent::UCinemafyCameraOffsetComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
	bTickInEditor = true;
}

bool UCinemafyCameraOffsetComponent::IsPrimary() const
{
	const AActor* Owner = GetOwner();
	return Owner && Owner->FindComponentByClass<UCinemafyCameraOffsetComponent>() == this;
}

void UCinemafyCameraOffsetComponent::OnRegister()
{
	Super::OnRegister();

	Camera = GetOwner() ? GetOwner()->FindComponentByClass<UCameraComponent>() : nullptr;
	CurrentOffset = FTransform::Identity;
	if (Camera.IsValid() && IsPrimary())
	{
		BaseRelativeTransform = Camera->GetRelativeTransform();
	}
}

void UCinemafyCameraOffsetComponent::OnUnregister()
{
	CurrentOffset = FTransform::Identity;
	if (Camera.IsValid() && IsPrimary())
	{
		Camera->SetRelativeTransform(BaseRelativeTransform);
	}
	Camera.Reset();
	Super::OnUnregister();
}

void UCinemafyCameraOffsetComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!Camera.IsValid())
	{
		return;
	}

	const UWorld* World = GetWorld();
	const bool bIsEditorPreview = World && !World->IsGameWorld();
	CurrentOffset = (bIsEditorPreview && !bPreviewInEditor) ? FTransform::Identity : EvaluateOffset(DeltaTime);

	// Every offset component re-applies the whole stack, so whichever ticks last this frame
	// leaves the camera with everyone's fresh offsets.
	ApplyStack();
}

void UCinemafyCameraOffsetComponent::ApplyStack() const
{
	const AActor* Owner = GetOwner();
	const UCinemafyCameraOffsetComponent* Primary = Owner ? Owner->FindComponentByClass<UCinemafyCameraOffsetComponent>() : nullptr;
	if (!Primary || !Camera.IsValid())
	{
		return;
	}

	TInlineComponentArray<UCinemafyCameraOffsetComponent*> Stack(Owner);
	Stack.StableSort([](const UCinemafyCameraOffsetComponent& A, const UCinemafyCameraOffsetComponent& B)
	{
		return A.GetStackOrder() < B.GetStackOrder();
	});

	// Each offset is in the camera's local space as left by the previous layer: Total = Shake * Move * Base.
	FTransform Total = Primary->BaseRelativeTransform;
	for (const UCinemafyCameraOffsetComponent* Layer : Stack)
	{
		if (Layer->IsActive())
		{
			Total = Layer->CurrentOffset * Total;
		}
	}
	Camera->SetRelativeTransform(Total);
}

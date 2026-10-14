#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CinemafyCameraOffsetComponent.generated.h"

class UCameraComponent;

/**
 * Base for components that nudge a camera (moves, shake, ...). Each one produces a local-space
 * offset; all offsets on an actor are stacked in StackOrder onto the camera component's rest
 * transform. Working on the camera component (not the actor) means they layer on top of
 * Sequencer keys, rails and cranes instead of fighting them.
 */
UCLASS(Abstract)
class CINEMAFY_API UCinemafyCameraOffsetComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCinemafyCameraOffsetComponent();

	/** Preview in the editor viewport (Sequencer scrubbing, etc). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Offset")
	bool bPreviewInEditor = true;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Lower applies first (closer to the rest pose). Moves before shake, so shake rides on the move. */
	virtual int32 GetStackOrder() const { return 0; }

	const FTransform& GetCurrentOffset() const { return CurrentOffset; }

protected:
	virtual void OnRegister() override;
	virtual void OnUnregister() override;

	/** Produce this frame's local-space offset (X forward, Y right, Z up). */
	virtual FTransform EvaluateOffset(float DeltaTime) { return FTransform::Identity; }

	UCameraComponent* GetCamera() const { return Camera.Get(); }

private:
	/** The first offset component on the actor owns the camera's rest transform. */
	bool IsPrimary() const;
	void ApplyStack() const;

	TWeakObjectPtr<UCameraComponent> Camera;
	FTransform BaseRelativeTransform;
	FTransform CurrentOffset;
};

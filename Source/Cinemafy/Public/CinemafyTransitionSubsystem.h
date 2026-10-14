#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Camera/CameraTypes.h"
#include "CinemafyTypes.h"
#include "CinemafyTransitionSubsystem.generated.h"

class SCinemafyTransition;
class USceneCaptureComponent2D;
class UTextureRenderTarget2D;
struct FSlateBrush;

/**
 * Plays wipes, dissolves and fades over the game view. Call it right before cutting: the outgoing
 * camera keeps rendering into a texture for the length of the transition, and the overlay reveals the
 * new shot behind it. The Scene Director drives this for directed scenes; you can call it from anywhere.
 */
UCLASS()
class CINEMAFY_API UCinemafyTransitionSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	/** Transition away from OutgoingCamera (null = from black). Cut to the new camera right after calling this. */
	UFUNCTION(BlueprintCallable, Category = "Cinemafy|Transitions")
	void PlayTransition(ECinemafyTransition Type, float Duration, ECinemafyWipeDirection Direction, AActor* OutgoingCamera);

	/** Same, from a fixed view (e.g. the gameplay camera right before a cutscene starts). */
	void PlayTransitionFromView(ECinemafyTransition Type, float Duration, ECinemafyWipeDirection Direction, const FMinimalViewInfo& View);

	UFUNCTION(BlueprintPure, Category = "Cinemafy|Transitions")
	bool IsTransitioning() const { return TimeLeft > 0.f; }

	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual void Deinitialize() override;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	void Start(ECinemafyTransition Type, float Duration, ECinemafyWipeDirection Direction, bool bCaptureOutgoing);
	void UpdateCapture();
	void EnsureWidget();
	void StopCapture();

	UPROPERTY(Transient)
	TObjectPtr<USceneCaptureComponent2D> Capture;

	UPROPERTY(Transient)
	TObjectPtr<UTextureRenderTarget2D> RenderTarget;

	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> OutgoingCamera;

	TOptional<FMinimalViewInfo> FixedView;
	TSharedPtr<SCinemafyTransition> Widget;
	TSharedPtr<FSlateBrush> OutgoingBrush;
	float Duration = 0.7f;
	float TimeLeft = 0.f;
	int32 FramesSinceStart = 0;
	int32 DumpCount = 0;
	void DumpOutgoing();
};

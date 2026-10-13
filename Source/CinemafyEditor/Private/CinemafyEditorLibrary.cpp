#include "CinemafyEditorLibrary.h"
#include "CinemafyModule.h"
#include "CinemafyCameraLibrary.h"
#include "CinemafyFramingLibrary.h"
#include "CinemafyCameraFeelComponent.h"
#include "CinemafyFocusComponent.h"
#include "CinemafyGazeComponent.h"
#include "CineCameraActor.h"
#include "CineCameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Editor.h"
#include "Selection.h"
#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "CinemafyEditor"

namespace
{
	template <typename TComponent>
	TComponent* FindOrAddComponent(AActor* Actor, const TCHAR* BaseName)
	{
		if (TComponent* Existing = Actor->FindComponentByClass<TComponent>())
		{
			Existing->Modify();
			return Existing;
		}

		Actor->Modify();
		TComponent* Component = NewObject<TComponent>(Actor, MakeUniqueObjectName(Actor, TComponent::StaticClass(), BaseName), RF_Transactional);
		Actor->AddInstanceComponent(Component);
		Component->RegisterComponent();
		return Component;
	}

	TArray<UCineCameraComponent*> GetSelectedCameras()
	{
		TArray<UCineCameraComponent*> Cameras;
		for (AActor* Actor : UCinemafyEditorLibrary::GetSelectedActorsInOrder())
		{
			if (UCineCameraComponent* Camera = UCinemafyCameraLibrary::FindCineCamera(Actor))
			{
				Cameras.Add(Camera);
			}
		}
		return Cameras;
	}

	void WarnNothingSelected(const TCHAR* What)
	{
		UE_LOG(LogCinemafy, Warning, TEXT("Cinemafy: select at least one %s first."), What);
	}
}

TArray<AActor*> UCinemafyEditorLibrary::GetSelectedActorsInOrder()
{
	TArray<AActor*> Actors;
	if (GEditor)
	{
		GEditor->GetSelectedActors()->GetSelectedObjects<AActor>(Actors);
	}
	return Actors;
}

ACineCameraActor* UCinemafyEditorLibrary::CreateShot(const FCinemafyShotRequest& Request)
{
	FCinemafyShotSolution Solution;
	if (!GEditor || !UCinemafyFramingLibrary::SolveShot(Request, Solution))
	{
		UE_LOG(LogCinemafy, Warning, TEXT("Cinemafy: couldn't solve that shot. Two-shots and over-the-shoulders need a subject and a counterpart."));
		return nullptr;
	}

	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World)
	{
		return nullptr;
	}

	const FScopedTransaction Transaction(LOCTEXT("CreateShot", "Cinemafy: Create Shot"));

	FActorSpawnParameters Params;
	Params.ObjectFlags |= RF_Transactional;
	ACineCameraActor* CameraActor = World->SpawnActor<ACineCameraActor>(
		Solution.CameraTransform.GetLocation(), Solution.CameraTransform.Rotator(), Params);
	if (!CameraActor)
	{
		return nullptr;
	}

	const FString ShotName = StaticEnum<ECinemafyShotType>()->GetNameStringByValue(static_cast<int64>(Request.ShotType));
	CameraActor->SetActorLabel(FString::Printf(TEXT("CAM_%s_%s"), *ShotName, *Request.Subject->GetActorLabel()));

	UCineCameraComponent* Camera = CameraActor->GetCineCameraComponent();
	UCinemafyCameraLibrary::ApplySensor(Camera, Request.Sensor);
	UCinemafyCameraLibrary::ApplyPrime(Camera, Solution.Lens, 2.8f);

	UCinemafyFocusComponent* Focus = FindOrAddComponent<UCinemafyFocusComponent>(CameraActor, TEXT("CinemafyFocus"));
	Focus->FocusTarget = Request.Subject;

	GEditor->SelectNone(false, true);
	GEditor->SelectActor(CameraActor, true, true);
	return CameraActor;
}

ACineCameraActor* UCinemafyEditorLibrary::CreateShotFromSelection(ECinemafyShotType ShotType, bool bFlipSide)
{
	const TArray<AActor*> Selected = GetSelectedActorsInOrder();
	if (Selected.Num() == 0)
	{
		WarnNothingSelected(TEXT("character"));
		return nullptr;
	}

	FCinemafyShotRequest Request;
	Request.ShotType = ShotType;
	Request.Subject = Selected[0];
	Request.Counterpart = Selected.Num() > 1 ? Selected[1] : nullptr;
	Request.bFlipSide = bFlipSide;
	return CreateShot(Request);
}

int32 UCinemafyEditorLibrary::ApplyCameraFeelToSelection(ECinemafyCameraFeel Feel)
{
	const FScopedTransaction Transaction(LOCTEXT("ApplyFeel", "Cinemafy: Camera Feel"));
	int32 Count = 0;
	for (AActor* Actor : GetSelectedActorsInOrder())
	{
		if (!UCinemafyCameraLibrary::FindCineCamera(Actor))
		{
			continue;
		}

		UCinemafyCameraFeelComponent* FeelComp = FindOrAddComponent<UCinemafyCameraFeelComponent>(Actor, TEXT("CinemafyFeel"));
		FeelComp->Feel = Feel;
		// Different cameras shouldn't wobble identically.
		FeelComp->Seed = static_cast<int32>(GetTypeHash(Actor->GetFName()) % 1000);
		++Count;
	}

	if (Count == 0)
	{
		WarnNothingSelected(TEXT("Cine Camera"));
	}
	return Count;
}

int32 UCinemafyEditorLibrary::ApplyLookToSelection(ECinemafyLook Look, float Strength)
{
	const FScopedTransaction Transaction(LOCTEXT("ApplyLook", "Cinemafy: Apply Look"));
	const TArray<UCineCameraComponent*> Cameras = GetSelectedCameras();
	for (UCineCameraComponent* Camera : Cameras)
	{
		Camera->Modify();
		UCinemafyCameraLibrary::ApplyLook(Camera, Look, Strength);
	}

	if (Cameras.Num() == 0)
	{
		WarnNothingSelected(TEXT("Cine Camera"));
	}
	return Cameras.Num();
}

int32 UCinemafyEditorLibrary::SetLetterboxOnSelection(float AspectRatio)
{
	const FScopedTransaction Transaction(LOCTEXT("Letterbox", "Cinemafy: Letterbox"));
	const TArray<UCineCameraComponent*> Cameras = GetSelectedCameras();
	for (UCineCameraComponent* Camera : Cameras)
	{
		Camera->Modify();
		UCinemafyCameraLibrary::SetLetterbox(Camera, AspectRatio);
	}

	if (Cameras.Num() == 0)
	{
		WarnNothingSelected(TEXT("Cine Camera"));
	}
	return Cameras.Num();
}

int32 UCinemafyEditorLibrary::ApplyPrimeToSelection(ECinemafyPrime Prime, float FStop)
{
	const FScopedTransaction Transaction(LOCTEXT("ApplyPrime", "Cinemafy: Lens"));
	const TArray<UCineCameraComponent*> Cameras = GetSelectedCameras();
	for (UCineCameraComponent* Camera : Cameras)
	{
		Camera->Modify();
		UCinemafyCameraLibrary::ApplyPrime(Camera, Prime, FStop);
	}

	if (Cameras.Num() == 0)
	{
		WarnNothingSelected(TEXT("Cine Camera"));
	}
	return Cameras.Num();
}

int32 UCinemafyEditorLibrary::AddGazeToSelection()
{
	const FScopedTransaction Transaction(LOCTEXT("AddGaze", "Cinemafy: Add Gaze"));
	int32 Count = 0;
	for (AActor* Actor : GetSelectedActorsInOrder())
	{
		if (Actor->FindComponentByClass<USkeletalMeshComponent>())
		{
			FindOrAddComponent<UCinemafyGazeComponent>(Actor, TEXT("CinemafyGaze"));
			++Count;
		}
	}

	if (Count == 0)
	{
		WarnNothingSelected(TEXT("character (with a skeletal mesh)"));
	}
	return Count;
}

#undef LOCTEXT_NAMESPACE

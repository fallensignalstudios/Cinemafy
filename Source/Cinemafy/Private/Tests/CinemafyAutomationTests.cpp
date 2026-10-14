// Run in the editor: Tools > Session Frontend > Automation, filter "Cinemafy".

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "CinemafyCameraLibrary.h"
#include "CinemafyCameraMoveComponent.h"
#include "CinemafyDirectorLibrary.h"
#include "CinemafyFramingLibrary.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

namespace CinemafyTests
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;

	/** A throwaway world with two stand-in actors, cleaned up on scope exit. */
	struct FTestScene
	{
		UWorld* World = nullptr;
		AActor* Kara = nullptr;
		AActor* Voss = nullptr;

		FTestScene()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false);
			Kara = World->SpawnActor<AActor>();
			Voss = World->SpawnActor<AActor>();
		}

		~FTestScene()
		{
			if (World)
			{
				World->DestroyWorld(false);
			}
		}

		TArray<FCinemafyLine> MakeConversation(int32 NumLines) const
		{
			TArray<FCinemafyLine> Lines;
			for (int32 Index = 0; Index < NumLines; ++Index)
			{
				FCinemafyLine Line;
				Line.Speaker = Index % 2 == 0 ? Kara : Voss;
				Line.Listener = Index % 2 == 0 ? Voss : Kara;
				Line.Duration = 3.f;
				Line.PauseAfter = 0.5f;
				Line.Intensity = FMath::Clamp(Index / static_cast<float>(FMath::Max(NumLines - 1, 1)), 0.f, 1.f);
				Lines.Add(Line);
			}
			return Lines;
		}
	};

	float AverageShotLength(const TArray<FCinemafyPlannedShot>& Plan)
	{
		float Total = 0.f;
		for (const FCinemafyPlannedShot& Shot : Plan)
		{
			Total += Shot.Duration;
		}
		return Plan.Num() > 0 ? Total / Plan.Num() : 0.f;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCinemafyLensTest, "Cinemafy.Lens.PrimesAndSnapping", CinemafyTests::Flags)
bool FCinemafyLensTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("50mm prime"), UCinemafyCameraLibrary::GetPrimeFocalLength(ECinemafyPrime::Prime50), 50.f);
	TestEqual(TEXT("52mm snaps to 50"), static_cast<int32>(UCinemafyCameraLibrary::SnapToPrime(52.f)), static_cast<int32>(ECinemafyPrime::Prime50));
	TestEqual(TEXT("30mm snaps to 35 (closer in log space)"), static_cast<int32>(UCinemafyCameraLibrary::SnapToPrime(30.f)), static_cast<int32>(ECinemafyPrime::Prime35));
	TestEqual(TEXT("500mm snaps to the longest prime"), static_cast<int32>(UCinemafyCameraLibrary::SnapToPrime(500.f)), static_cast<int32>(ECinemafyPrime::Prime135));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCinemafyEaseTest, "Cinemafy.CameraMove.Easing", CinemafyTests::Flags)
bool FCinemafyEaseTest::RunTest(const FString& Parameters)
{
	for (ECinemafyEase Ease : { ECinemafyEase::Linear, ECinemafyEase::EaseInOut, ECinemafyEase::OperatorSettle })
	{
		TestTrue(TEXT("Starts at 0"), FMath::IsNearlyZero(UCinemafyCameraMoveComponent::ApplyEase(Ease, 0.f), 1e-4f));
		TestTrue(TEXT("Ends at 1"), FMath::IsNearlyEqual(UCinemafyCameraMoveComponent::ApplyEase(Ease, 1.f), 1.f, 1e-4f));
	}

	float MaxValue = 0.f;
	for (int32 Step = 0; Step <= 100; ++Step)
	{
		MaxValue = FMath::Max(MaxValue, UCinemafyCameraMoveComponent::ApplyEase(ECinemafyEase::OperatorSettle, Step / 100.f));
	}
	TestTrue(TEXT("Operator settle overshoots a little"), MaxValue > 1.005f);
	TestTrue(TEXT("...but not much"), MaxValue < 1.05f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCinemafyShotSizeTest, "Cinemafy.Director.ShotSizesAreOrdered", CinemafyTests::Flags)
bool FCinemafyShotSizeTest::RunTest(const FString& Parameters)
{
	const ECinemafyShotType Order[] = { ECinemafyShotType::WideTwoShot, ECinemafyShotType::MediumTwoShot, ECinemafyShotType::MediumShot,
		ECinemafyShotType::MediumCloseUp, ECinemafyShotType::CloseUp, ECinemafyShotType::ExtremeCloseUp };
	for (int32 Index = 1; Index < static_cast<int32>(UE_ARRAY_COUNT(Order)); ++Index)
	{
		TestTrue(TEXT("Each shot is tighter than the last"),
			UCinemafyDirectorLibrary::GetShotSize(Order[Index]) > UCinemafyDirectorLibrary::GetShotSize(Order[Index - 1]));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCinemafyPlannerTest, "Cinemafy.Director.PlannerFollowsTheRules", CinemafyTests::Flags)
bool FCinemafyPlannerTest::RunTest(const FString& Parameters)
{
	CinemafyTests::FTestScene Scene;
	const TArray<FCinemafyLine> Lines = Scene.MakeConversation(8);
	const FCinemafyDirectorStyle Style = UCinemafyDirectorLibrary::GetPresetStyle(ECinemafyDirectorPreset::ClassicHollywood);

	const TArray<FCinemafyPlannedShot> Plan = UCinemafyDirectorLibrary::PlanScene(Lines, Style, 7);
	const TArray<FCinemafyPlannedShot> Again = UCinemafyDirectorLibrary::PlanScene(Lines, Style, 7);

	TestTrue(TEXT("Plans something"), Plan.Num() >= Lines.Num());
	TestEqual(TEXT("Same seed, same edit"), Plan.Num(), Again.Num());
	for (int32 Index = 0; Index < FMath::Min(Plan.Num(), Again.Num()); ++Index)
	{
		TestEqual(TEXT("Same seed, same shots"), static_cast<int32>(Plan[Index].ShotType), static_cast<int32>(Again[Index].ShotType));
	}

	TestEqual(TEXT("Classic opens on an establishing wide"), static_cast<int32>(Plan[0].ShotType), static_cast<int32>(ECinemafyShotType::WideTwoShot));

	float Covered = 0.f;
	float Expected = 0.f;
	for (const FCinemafyLine& Line : Lines)
	{
		Expected += Line.Duration + Line.PauseAfter;
	}
	for (int32 Index = 0; Index < Plan.Num(); ++Index)
	{
		Covered += Plan[Index].Duration;
		if (Index > 0)
		{
			const FCinemafyPlannedShot& Previous = Plan[Index - 1];
			const FCinemafyPlannedShot& Current = Plan[Index];
			TestTrue(TEXT("Shots are back to back"), FMath::IsNearlyEqual(Previous.StartTime + Previous.Duration, Current.StartTime, 0.01f));
			if (Previous.Subject == Current.Subject)
			{
				TestNotEqual(TEXT("No jump cuts: same person never gets the same framing twice in a row"),
					static_cast<int32>(Previous.ShotType), static_cast<int32>(Current.ShotType));
			}
		}
	}
	TestTrue(TEXT("The edit covers the whole scene"), FMath::IsNearlyEqual(Covered, Expected, 0.05f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCinemafyMonologueTest, "Cinemafy.Director.MonologueNeedsNoCounterpart", CinemafyTests::Flags)
bool FCinemafyMonologueTest::RunTest(const FString& Parameters)
{
	CinemafyTests::FTestScene Scene;
	TArray<FCinemafyLine> Lines = Scene.MakeConversation(5);
	for (FCinemafyLine& Line : Lines)
	{
		Line.Speaker = Scene.Kara;
		Line.Listener = nullptr;
	}

	const TArray<FCinemafyPlannedShot> Plan = UCinemafyDirectorLibrary::PlanScene(Lines, UCinemafyDirectorLibrary::GetPresetStyle(ECinemafyDirectorPreset::Epic), 3);
	for (const FCinemafyPlannedShot& Shot : Plan)
	{
		TestFalse(TEXT("No two-shots or OTS without someone to share the frame"), UCinemafyFramingLibrary::ShotNeedsCounterpart(Shot.ShotType));
		TestFalse(TEXT("No reaction shots without a listener"), Shot.bIsReaction);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCinemafyPacingTest, "Cinemafy.Director.StylesPaceDifferently", CinemafyTests::Flags)
bool FCinemafyPacingTest::RunTest(const FString& Parameters)
{
	CinemafyTests::FTestScene Scene;
	TArray<FCinemafyLine> Lines = Scene.MakeConversation(10);
	for (FCinemafyLine& Line : Lines)
	{
		Line.Duration = 8.f;
	}

	const float Kinetic = CinemafyTests::AverageShotLength(UCinemafyDirectorLibrary::PlanScene(Lines, UCinemafyDirectorLibrary::GetPresetStyle(ECinemafyDirectorPreset::KineticAction), 1));
	const float SlowBurn = CinemafyTests::AverageShotLength(UCinemafyDirectorLibrary::PlanScene(Lines, UCinemafyDirectorLibrary::GetPresetStyle(ECinemafyDirectorPreset::SlowBurn), 1));
	TestTrue(TEXT("Kinetic Action cuts faster than Slow Burn"), Kinetic < SlowBurn);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCinemafyGroupTest, "Cinemafy.Director.GroupScenesOpenOnTheGroup", CinemafyTests::Flags)
bool FCinemafyGroupTest::RunTest(const FString& Parameters)
{
	CinemafyTests::FTestScene Scene;
	AActor* Third = Scene.World->SpawnActor<AActor>();
	TArray<FCinemafyLine> Lines = Scene.MakeConversation(6);
	Lines[2].Speaker = Third;
	Lines[3].Listener = Third;

	const TArray<FCinemafyPlannedShot> Plan = UCinemafyDirectorLibrary::PlanScene(Lines, UCinemafyDirectorLibrary::GetPresetStyle(ECinemafyDirectorPreset::ClassicHollywood), 11);
	TestEqual(TEXT("Three-hander opens on the group"), static_cast<int32>(Plan[0].ShotType), static_cast<int32>(ECinemafyShotType::GroupShot));
	TestTrue(TEXT("Group shot knows who else is in it"), Plan[0].Group.Num() >= 1);

	const TArray<FCinemafyPlannedShot> TwoHander = UCinemafyDirectorLibrary::PlanScene(Scene.MakeConversation(6),
		UCinemafyDirectorLibrary::GetPresetStyle(ECinemafyDirectorPreset::ClassicHollywood), 11);
	for (const FCinemafyPlannedShot& Shot : TwoHander)
	{
		TestNotEqual(TEXT("No group shots with only two people"), static_cast<int32>(Shot.ShotType), static_cast<int32>(ECinemafyShotType::GroupShot));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCinemafyTransitionTest, "Cinemafy.Director.TransitionsAndHolds", CinemafyTests::Flags)
bool FCinemafyTransitionTest::RunTest(const FString& Parameters)
{
	CinemafyTests::FTestScene Scene;
	TArray<FCinemafyLine> Lines = Scene.MakeConversation(8);
	Lines[4].bSceneBreak = true;

	const FCinemafyDirectorStyle SpaceOpera = UCinemafyDirectorLibrary::GetPresetStyle(ECinemafyDirectorPreset::SpaceOpera);
	const TArray<FCinemafyPlannedShot> Plan = UCinemafyDirectorLibrary::PlanScene(Lines, SpaceOpera, 5);

	TestEqual(TEXT("Space Opera opens with a wipe"), static_cast<int32>(Plan[0].TransitionIn), static_cast<int32>(ECinemafyTransition::Wipe));
	const FCinemafyPlannedShot* BreakShot = Plan.FindByPredicate([](const FCinemafyPlannedShot& Shot) { return Shot.LineIndex == 4 && Shot.bLineStart; });
	TestTrue(TEXT("The scene break gets a shot"), BreakShot != nullptr);
	if (BreakShot)
	{
		TestEqual(TEXT("...and it wipes in"), static_cast<int32>(BreakShot->TransitionIn), static_cast<int32>(ECinemafyTransition::Wipe));
	}
	TestEqual(TEXT("Space Opera closes with a wipe"), static_cast<int32>(Plan.Last().TransitionOut), static_cast<int32>(ECinemafyTransition::Wipe));
	for (const FCinemafyPlannedShot& Shot : Plan)
	{
		TestNotEqual(TEXT("Wipes always have a real direction"), static_cast<int32>(Shot.WipeDirection), static_cast<int32>(ECinemafyWipeDirection::Alternate));
		for (const FCinemafyRackEvent& Rack : Shot.RackEvents)
		{
			TestTrue(TEXT("Rack focus only inside the shot"), Rack.Time > 0.f && Rack.Time < Shot.Duration);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCinemafyActionTest, "Cinemafy.Director.ActionCutsOnImpacts", CinemafyTests::Flags)
bool FCinemafyActionTest::RunTest(const FString& Parameters)
{
	CinemafyTests::FTestScene Scene;
	TArray<FCinemafyLine> Lines = Scene.MakeConversation(1);
	Lines[0].bAction = true;
	Lines[0].Duration = 6.f;
	Lines[0].PauseAfter = 0.f;
	Lines[0].ImpactTimes = { 1.5f, 3.0f, 4.5f };

	const TArray<FCinemafyPlannedShot> Plan = UCinemafyDirectorLibrary::PlanScene(Lines,
		UCinemafyDirectorLibrary::GetPresetStyle(ECinemafyDirectorPreset::KineticAction), 9);
	TestEqual(TEXT("One shot per stretch between impacts"), Plan.Num(), 4);
	for (int32 Index = 1; Index < Plan.Num(); ++Index)
	{
		const float Cut = Plan[Index].StartTime;
		TestTrue(TEXT("Cuts land just before an impact"), Lines[0].ImpactTimes.ContainsByPredicate([Cut](float Impact) { return FMath::IsNearlyEqual(Impact - 0.08f, Cut, 0.01f); }));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCinemafyWalkTest, "Cinemafy.Director.WalkAndTalkTracks", CinemafyTests::Flags)
bool FCinemafyWalkTest::RunTest(const FString& Parameters)
{
	CinemafyTests::FTestScene Scene;
	TArray<FCinemafyLine> Lines = Scene.MakeConversation(3);
	for (FCinemafyLine& Line : Lines)
	{
		Line.bMoving = true;
	}
	FCinemafyDirectorStyle Style = UCinemafyDirectorLibrary::GetPresetStyle(ECinemafyDirectorPreset::ClassicHollywood);
	Style.bEstablishingShot = false;
	for (const FCinemafyPlannedShot& Shot : UCinemafyDirectorLibrary::PlanScene(Lines, Style, 2))
	{
		TestTrue(TEXT("Walking lines get tracking coverage"),
			Shot.ShotType == ECinemafyShotType::TrackingLead || Shot.ShotType == ECinemafyShotType::TrackingFollow);
		TestFalse(TEXT("No push-ins while walking"), Shot.bMove);
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

// Run in the editor: Tools > Session Frontend > Automation, filter "Cinemafy".

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "CinemafyCameraLibrary.h"
#include "CinemafyChoreographyLibrary.h"
#include "CinemafyScript.h"
#include "CinemafyPrevis.h"
#include "CinemafyStory.h"
#include "CinemafyMood.h"
#include "CinemafyExtras.h"
#include "GameFramework/Pawn.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "CinemafyCameraMoveComponent.h"
#include "CinemafyDirectorLibrary.h"
#include "CinemafyFramingLibrary.h"
#include "CinemafySubtitleSubsystem.h"
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCinemafyBeatTest, "Cinemafy.Director.CutsLandOnTheBeat", CinemafyTests::Flags)
bool FCinemafyBeatTest::RunTest(const FString& Parameters)
{
	CinemafyTests::FTestScene Scene;
	const TArray<FCinemafyLine> Lines = Scene.MakeConversation(6);
	// Classic pacing: shots comfortably longer than a beat, so every cut can reach one.
	TArray<FCinemafyPlannedShot> Plan = UCinemafyDirectorLibrary::PlanScene(Lines,
		UCinemafyDirectorLibrary::GetPresetStyle(ECinemafyDirectorPreset::ClassicHollywood), 4);

	TArray<float> Beats;
	for (float Beat = 0.f; Beat < 60.f; Beat += 0.5f) // 120 BPM
	{
		Beats.Add(Beat);
	}
	const float Total = Plan.Last().StartTime + Plan.Last().Duration;
	UCinemafyDirectorLibrary::SnapPlanToBeats(Plan, Lines, Beats);

	for (int32 Index = 1; Index < Plan.Num(); ++Index)
	{
		const float Cut = Plan[Index].StartTime;
		const float Offset = FMath::Abs(Cut - FMath::RoundToFloat(Cut * 2.f) / 2.f);
		TestTrue(TEXT("Every cut is on a beat (no dialogue audio to protect here)"), Offset < 0.01f);
		TestTrue(TEXT("Shots stay back to back"), FMath::IsNearlyEqual(Plan[Index - 1].StartTime + Plan[Index - 1].Duration, Cut, 0.001f));
		TestTrue(TEXT("No shot gets crushed"), Plan[Index - 1].Duration >= 0.499f);
	}
	TestTrue(TEXT("The scene keeps its length"), FMath::IsNearlyEqual(Plan.Last().StartTime + Plan.Last().Duration, Total, 0.001f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCinemafyLockTest, "Cinemafy.Director.LockedShotsSurviveRerolls", CinemafyTests::Flags)
bool FCinemafyLockTest::RunTest(const FString& Parameters)
{
	CinemafyTests::FTestScene Scene;
	const TArray<FCinemafyLine> Lines = Scene.MakeConversation(6);
	const FCinemafyDirectorStyle Style = UCinemafyDirectorLibrary::GetPresetStyle(ECinemafyDirectorPreset::ClassicHollywood);

	TArray<FCinemafyPlannedShot> First = UCinemafyDirectorLibrary::PlanScene(Lines, Style, 1);
	const int32 LockIndex = First.Num() / 2;
	TestTrue(TEXT("Alternate swap works"), UCinemafyDirectorLibrary::SwapForAlternate(First, LockIndex, Lines, Style));
	const FCinemafyPlannedShot Locked = First[LockIndex];

	TArray<FCinemafyPlannedShot> Reroll = UCinemafyDirectorLibrary::PlanScene(Lines, Style, 99);
	UCinemafyDirectorLibrary::ApplyLockedShots(Reroll, { Locked });

	const FCinemafyPlannedShot* Kept = Reroll.FindByPredicate([&Locked](const FCinemafyPlannedShot& Shot)
	{
		return Shot.bLocked && FMath::IsNearlyEqual(Shot.StartTime, Locked.StartTime) && Shot.ShotType == Locked.ShotType;
	});
	TestTrue(TEXT("The locked shot is still there after a re-roll"), Kept != nullptr);
	for (int32 Index = 1; Index < Reroll.Num(); ++Index)
	{
		TestTrue(TEXT("No gaps or overlaps around the locked shot"),
			FMath::IsNearlyEqual(Reroll[Index - 1].StartTime + Reroll[Index - 1].Duration, Reroll[Index].StartTime, 0.01f));
	}

	TestTrue(TEXT("Nudging a cut works"), UCinemafyDirectorLibrary::NudgeCut(Reroll, 1, 0.25f));
	TestTrue(TEXT("...and keeps shots back to back"),
		FMath::IsNearlyEqual(Reroll[0].StartTime + Reroll[0].Duration, Reroll[1].StartTime, 0.001f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCinemafySubtitleTest, "Cinemafy.Subtitles.SplitsLongLinesReadably", CinemafyTests::Flags)
bool FCinemafySubtitleTest::RunTest(const FString& Parameters)
{
	const FString Short = TEXT("You weren't easy to find.");
	TestEqual(TEXT("Short lines stay whole"), UCinemafySubtitleSubsystem::SplitIntoChunks(Short, 84).Num(), 1);

	const FString Long = TEXT("The fleet is scattered across three systems. If we move now, before they regroup, we might still have a chance, but only if the shields hold.");
	const TArray<FString> Chunks = UCinemafySubtitleSubsystem::SplitIntoChunks(Long, 84);
	TestTrue(TEXT("Long lines split"), Chunks.Num() >= 2);
	FString Rejoined;
	for (const FString& Chunk : Chunks)
	{
		TestTrue(TEXT("Every chunk fits"), Chunk.Len() <= 84);
		Rejoined += (Rejoined.IsEmpty() ? TEXT("") : TEXT(" ")) + Chunk;
	}
	TestEqual(TEXT("Nothing is lost"), Rejoined, Long);
	TestTrue(TEXT("The first split falls at the end of a sentence"), Chunks[0].EndsWith(TEXT(".")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCinemafyBrainTest, "Cinemafy.Director.BrainReadsMoments", CinemafyTests::Flags)
bool FCinemafyBrainTest::RunTest(const FString& Parameters)
{
	CinemafyTests::FTestScene Scene;
	AActor* Third = Scene.World->SpawnActor<AActor>();
	TArray<FCinemafyLine> Lines = Scene.MakeConversation(6);
	for (FCinemafyLine& Line : Lines)
	{
		Line.Intensity = 0.3f;
	}
	Lines[2].Intensity = 0.9f;          // reveal
	Lines[3].PauseAfter = 2.f;          // silent beat
	Lines[4].Speaker = Third;           // someone new joins
	Lines[4].Listener = Scene.Kara;
	Lines[5].bMoving = true;            // they walk

	UCinemafyDirectorLibrary::ReadMoments(Lines);
	TestEqual(TEXT("Big intensity jump reads as a reveal"), static_cast<int32>(Lines[2].Moment), static_cast<int32>(ECinemafyMoment::Reveal));
	TestEqual(TEXT("Long pause reads as a silent beat"), static_cast<int32>(Lines[3].Moment), static_cast<int32>(ECinemafyMoment::SilentBeat));
	TestEqual(TEXT("A new speaker reads as a new exchange"), static_cast<int32>(Lines[4].Moment), static_cast<int32>(ECinemafyMoment::NewExchange));
	TestEqual(TEXT("Walking reads as walk & talk"), static_cast<int32>(Lines[5].Moment), static_cast<int32>(ECinemafyMoment::WalkAndTalk));

	// Silent beats are held: the pause is covered by exactly one reaction shot.
	const TArray<FCinemafyPlannedShot> Plan = UCinemafyDirectorLibrary::PlanScene(Lines,
		UCinemafyDirectorLibrary::GetPresetStyle(ECinemafyDirectorPreset::ClassicHollywood), 21);
	float LineStart = 0.f;
	for (int32 Index = 0; Index < 3; ++Index)
	{
		LineStart += Lines[Index].Duration + Lines[Index].PauseAfter;
	}
	const float PauseStart = LineStart + Lines[3].Duration;
	const float PauseEnd = PauseStart + Lines[3].PauseAfter;
	int32 ShotsInPause = 0;
	for (const FCinemafyPlannedShot& Shot : Plan)
	{
		if (Shot.StartTime > PauseStart + 0.01f && Shot.StartTime < PauseEnd - 0.01f)
		{
			++ShotsInPause;
		}
	}
	TestTrue(TEXT("No busy cutting inside a silent beat"), ShotsInPause <= 1);

	// Audio that's all hits and no talk is an action beat.
	TArray<FCinemafyLine> Fight = Scene.MakeConversation(1);
	Fight[0].Analysis.bValid = true;
	Fight[0].Analysis.ActiveRatio = 0.2f;
	Fight[0].Analysis.LoudnessDb = -30.f;
	Fight[0].Analysis.PeakDb = -8.f;
	UCinemafyDirectorLibrary::ReadMoments(Fight);
	TestTrue(TEXT("Hits without words become action"), Fight[0].bAction && Fight[0].Moment == ECinemafyMoment::Action);

	TestFalse(TEXT("The decision report says something"), UCinemafyDirectorLibrary::SummarizeDecisions(Plan, Lines).IsEmpty());
	return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCinemafyChoreoPromptTest, "Cinemafy.Choreography.ReadsPrompts", CinemafyTests::Flags)
bool FCinemafyChoreoPromptTest::RunTest(const FString& Parameters)
{
	using Lib = UCinemafyChoreographyLibrary;

	FCinemafyPromptReading Walk = Lib::ReadPrompt(TEXT("walk"));
	TestEqual(TEXT("walk moves"), Walk.Kind, ECinemafyActionKind::Move);
	TestTrue(TEXT("walk sets a gait"), Walk.bHasGait && Walk.Gait == ECinemafyGait::Walk);

	TestEqual(TEXT("sprint is a sprinting move"), Lib::ReadPrompt(TEXT("sprint")).Gait, ECinemafyGait::Sprint);
	TestEqual(TEXT("sneak crouches"), Lib::ReadPrompt(TEXT("sneak")).Gait, ECinemafyGait::Crouch);
	TestEqual(TEXT("vault traverses"), Lib::ReadPrompt(TEXT("vault")).Kind, ECinemafyActionKind::Traverse);
	TestEqual(TEXT("climb traverses"), Lib::ReadPrompt(TEXT("climb")).Kind, ECinemafyActionKind::Traverse);
	TestEqual(TEXT("sprint vault is still a vault"), Lib::ReadPrompt(TEXT("sprint vault")).Kind, ECinemafyActionKind::Traverse);

	FCinemafyPromptReading Sword = Lib::ReadPrompt(TEXT("sword attack"));
	TestEqual(TEXT("sword attack attacks"), Sword.Kind, ECinemafyActionKind::Attack);
	TestEqual(TEXT("...with a sword"), Sword.Weapon, FString(TEXT("sword")));
	TestTrue(TEXT("...looking for attack animations"), Sword.Keywords.Num() > 0 && Sword.Keywords[0].StartsWith(TEXT("attack")));

	TestEqual(TEXT("combo chains three"), Lib::ReadPrompt(TEXT("combo")).Repeat, 3);
	TestEqual(TEXT("attack x3 chains three"), Lib::ReadPrompt(TEXT("attack x3")).Repeat, 3);
	TestTrue(TEXT("shoot is ranged"), Lib::ReadPrompt(TEXT("shoot")).bRanged);

	FCinemafyPromptReading Taunt = Lib::ReadPrompt(TEXT("taunt"));
	TestEqual(TEXT("taunt performs"), Taunt.Kind, ECinemafyActionKind::Perform);
	TestTrue(TEXT("...a taunt"), Taunt.Keywords.Num() > 0 && Taunt.Keywords[0].StartsWith(TEXT("taunt")));

	FCinemafyPromptReading Wait = Lib::ReadPrompt(TEXT("wait 2"));
	TestEqual(TEXT("wait waits"), Wait.Kind, ECinemafyActionKind::Wait);
	TestEqual(TEXT("...two seconds"), Wait.Seconds, 2.f);

	FCinemafyPromptReading Pistols = Lib::ReadPrompt(TEXT("two pistols"));
	TestEqual(TEXT("two pistols is an equip"), Pistols.Kind, ECinemafyActionKind::Equip);
	TestEqual(TEXT("...of both pistols"), Pistols.Weapon, FString(TEXT("pistols")));
	TestEqual(TEXT("holster goes unarmed"), Lib::ReadPrompt(TEXT("holster")).Weapon, FString(TEXT("unarmed")));
	TestEqual(TEXT("look looks"), Lib::ReadPrompt(TEXT("look")).Kind, ECinemafyActionKind::Look);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCinemafyChoreoPickTest, "Cinemafy.Choreography.PicksAnimationsByName", CinemafyTests::Flags)
bool FCinemafyChoreoPickTest::RunTest(const FString& Parameters)
{
	using Lib = UCinemafyChoreographyLibrary;

	const TArray<FString> Tokens = Lib::TokenizeName(TEXT("/Game/Anims/AM_Katana_Attack_Combo01"));
	TestTrue(TEXT("Tokens split on CamelCase, separators and digits"),
		Tokens.Contains(TEXT("katana")) && Tokens.Contains(TEXT("attack")) && Tokens.Contains(TEXT("combo")) && Tokens.Contains(TEXT("01")));
	TestTrue(TEXT("CamelCase pairs are kept whole too"), Lib::TokenizeName(TEXT("PistolOneHanded")).Contains(TEXT("onehanded")));

	const FCinemafyPromptReading Sword = Lib::ReadPrompt(TEXT("sword attack"));
	const float Katana = Lib::ScoreAnimation(Sword, Lib::TokenizeName(TEXT("/Game/CombatMaster/Katana/AM_Katana_Attack_01")));
	const float Pistol = Lib::ScoreAnimation(Sword, Lib::TokenizeName(TEXT("/Game/Guns/AM_Pistol_Attack_01")));
	TestTrue(TEXT("A sword attack prefers the katana"), Katana > Pistol);
	TestTrue(TEXT("Locomotion is never an attack"), Lib::ScoreAnimation(Sword, Lib::TokenizeName(TEXT("Sword_Walk_Loop"))) < 0.f);
	TestTrue(TEXT("A hit reaction is never an attack"), Lib::ScoreAnimation(Sword, Lib::TokenizeName(TEXT("Sword_HitReact_Front"))) < 0.f);

	TArray<FCinemafyMoveCandidate> Candidates;
	for (const TCHAR* Name : { TEXT("AS_Single_Attack"), TEXT("AS_Combo_Attack_01_03"), TEXT("AS_Combo_Attack_01_01"), TEXT("AS_Combo_Attack_01_02"), TEXT("AS_Combo_Attack_01_04") })
	{
		FCinemafyMoveCandidate& Candidate = Candidates.AddDefaulted_GetRef();
		Candidate.Name = Name;
		Candidate.Path = FSoftObjectPath(FString::Printf(TEXT("/Game/Anims/%s.%s"), Name, Name));
		Candidate.Tokens = Lib::TokenizeName(Name);
	}
	const TArray<int32> Combo = Lib::PickAnimations(Lib::ReadPrompt(TEXT("combo")), Candidates, 3, {});
	TestEqual(TEXT("A combo picks three"), Combo.Num(), 3);
	if (Combo.Num() == 3)
	{
		TestEqual(TEXT("...in order from the first hit"), Candidates[Combo[0]].Name, FString(TEXT("AS_Combo_Attack_01_01")));
		TestEqual(TEXT("...then the second"), Candidates[Combo[1]].Name, FString(TEXT("AS_Combo_Attack_01_02")));
		TestEqual(TEXT("...then the third"), Candidates[Combo[2]].Name, FString(TEXT("AS_Combo_Attack_01_03")));
	}

	// Folder words count, but less than the asset's own name, and never against it.
	const FCinemafyPromptReading Jump = Lib::ReadPrompt(TEXT("jump"));
	TestTrue(TEXT("A dash in a Jump folder is not a jump"), Lib::ScoreAnimation(Jump, Lib::TokenizeName(TEXT("/Game/Anims/Jump/MM_Dash"))) < 0.f);
	TestTrue(TEXT("A clip named jump is"), Lib::ScoreAnimation(Jump, Lib::TokenizeName(TEXT("/Game/Anims/Jump/MM_Jump_Start"))) > 0.f);
	TestTrue(TEXT("The name beats the folder"),
		Lib::ScoreAnimation(Sword, Lib::TokenizeName(TEXT("/Game/Sword/AM_Attack_01"))) < Lib::ScoreAnimation(Sword, Lib::TokenizeName(TEXT("/Game/Anims/AM_Sword_Attack_01"))));

	TestTrue(TEXT("Two pistols means the two-handed pistol overlay"),
		Lib::ScoreOverlayName(TEXT("pistols"), TEXT("PistolTwoHanded")) > Lib::ScoreOverlayName(TEXT("pistols"), TEXT("PistolOneHanded")));
	TestTrue(TEXT("A sword isn't a rifle"), Lib::ScoreOverlayName(TEXT("sword"), TEXT("Rifle")) <= 0.f);
	TestTrue(TEXT("Blueprint enum paths are trimmed"), Lib::ScoreOverlayName(TEXT("rifle"), TEXT("Enum_OverlayPose::Rifle")) > 5.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCinemafyChoreoLinesTest, "Cinemafy.Choreography.TakeBecomesDirectorLines", CinemafyTests::Flags)
bool FCinemafyChoreoLinesTest::RunTest(const FString& Parameters)
{
	CinemafyTests::FTestScene Scene;
	APawn* Enemy = Scene.World->SpawnActor<APawn>();

	auto MakeBeat = [](const TCHAR* Prompt, float Start, float End, TArray<float> Impacts, AActor* Target)
	{
		FCinemafyChoreoBeat Beat;
		Beat.Prompt = Prompt;
		Beat.Reading = UCinemafyChoreographyLibrary::ReadPrompt(Prompt);
		Beat.StartTime = Start;
		Beat.EndTime = End;
		Beat.ImpactTimes = Impacts;
		Beat.Target = Target;
		return Beat;
	};

	TArray<FCinemafyChoreoBeat> Beats;
	Beats.Add(MakeBeat(TEXT("walk"), 0.5f, 3.f, {}, nullptr));
	Beats.Add(MakeBeat(TEXT("vault"), 3.f, 4.5f, { 3.6f }, nullptr));
	Beats.Add(MakeBeat(TEXT("sword attack"), 4.5f, 7.f, { 5.4f }, Enemy));
	Beats.Add(MakeBeat(TEXT("taunt"), 7.f, 9.f, {}, nullptr));

	const TArray<FCinemafyLine> Lines = UCinemafyChoreographyLibrary::BuildDirectorLines(Beats, Scene.Kara, 10.f);
	TestEqual(TEXT("One line per beat"), Lines.Num(), 4);
	if (Lines.Num() != 4)
	{
		return true;
	}

	float Total = 0.f;
	for (const FCinemafyLine& Line : Lines)
	{
		Total += Line.Duration + Line.PauseAfter;
		TestFalse(TEXT("Timed to the take, not to audio"), Line.bUseAudioDuration);
	}
	TestTrue(TEXT("The lines cover the whole take"), FMath::IsNearlyEqual(Total, 10.f, 0.01f));
	TestTrue(TEXT("Walking is moving"), Lines[0].bMoving);
	TestTrue(TEXT("A vault is action"), Lines[1].bAction);
	TestTrue(TEXT("...cut on the take-off"), Lines[1].ImpactTimes.Num() == 1 && FMath::IsNearlyEqual(Lines[1].ImpactTimes[0], 0.6f, 0.01f));
	TestTrue(TEXT("The attack is action on its impact"), Lines[2].bAction && Lines[2].ImpactTimes.Num() == 1 && FMath::IsNearlyEqual(Lines[2].ImpactTimes[0], 0.9f, 0.01f));
	TestTrue(TEXT("The enemy is the listener (for over-the-shoulders)"), Lines[2].Listener == Enemy);
	TestTrue(TEXT("Everything is the performer's"), Lines[3].Speaker == Scene.Kara);
	return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCinemafyChoreoArrivalTest, "Cinemafy.Choreography.ArrivalPolicies", CinemafyTests::Flags)
bool FCinemafyChoreoArrivalTest::RunTest(const FString& Parameters)
{
	auto Beat = [](const TCHAR* Prompt)
	{
		FCinemafyChoreoBeat B;
		B.Prompt = Prompt;
		B.Reading = UCinemafyChoreographyLibrary::ReadPrompt(Prompt);
		return B;
	};
	TArray<FCinemafyChoreoBeat> Beats = { Beat(TEXT("walk")), Beat(TEXT("vault")), Beat(TEXT("sprint")), Beat(TEXT("taunt")), Beat(TEXT("run")) };

	using Lib = UCinemafyChoreographyLibrary;
	TestEqual(TEXT("A walk mark before a vault is a waypoint (no idle)"), Lib::ResolveArrival(Beats, 0), ECinemafyArrival::PassThrough);
	TestEqual(TEXT("A vault is entered moving"), Lib::ResolveArrival(Beats, 1), ECinemafyArrival::EnterAction);
	TestEqual(TEXT("A sprint before a taunt comes to a real stop"), Lib::ResolveArrival(Beats, 2), ECinemafyArrival::StopAndFace);
	TestEqual(TEXT("The last move stops"), Lib::ResolveArrival(Beats, 4), ECinemafyArrival::StopAndFace);

	Beats[0].Arrival = ECinemafyArrival::StopAndFace;
	TestEqual(TEXT("An explicit arrival wins"), Lib::ResolveArrival(Beats, 0), ECinemafyArrival::StopAndFace);
	return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCinemafyChoreoReactionTest, "Cinemafy.Choreography.TargetsReact", CinemafyTests::Flags)
bool FCinemafyChoreoReactionTest::RunTest(const FString& Parameters)
{
	using Lib = UCinemafyChoreographyLibrary;

	TestEqual(TEXT("A sword attack makes the target flinch"), Lib::ReadPrompt(TEXT("sword attack")).Reaction, ECinemafyHitReaction::Hit);
	TestEqual(TEXT("kill means death"), Lib::ReadPrompt(TEXT("kill")).Reaction, ECinemafyHitReaction::Death);
	TestEqual(TEXT("...and is still an attack"), Lib::ReadPrompt(TEXT("kill")).Kind, ECinemafyActionKind::Attack);
	TestEqual(TEXT("A heavy attack knocks down"), Lib::ReadPrompt(TEXT("heavy attack")).Reaction, ECinemafyHitReaction::Knockdown);
	TestTrue(TEXT("...and still looks for attacks (heavy only refines)"), Lib::ReadPrompt(TEXT("heavy attack")).Keywords[0].StartsWith(TEXT("attack")));
	TestEqual(TEXT("attack blocked is blocked"), Lib::ReadPrompt(TEXT("attack blocked")).Reaction, ECinemafyHitReaction::Block);
	TestEqual(TEXT("attack dodged is dodged"), Lib::ReadPrompt(TEXT("attack dodged")).Reaction, ECinemafyHitReaction::Dodge);
	TestEqual(TEXT("A reaction word doesn't search attack names"), Lib::ReadPrompt(TEXT("attack blocked")).Keywords.Num(), 1);

	TestEqual(TEXT("Straight ahead is the front"), Lib::ReactionDirection(10.f), FString(TEXT("front")));
	TestEqual(TEXT("Behind is the back"), Lib::ReactionDirection(-170.f), FString(TEXT("back")));
	TestEqual(TEXT("Positive yaw is the right"), Lib::ReactionDirection(90.f), FString(TEXT("right")));
	TestEqual(TEXT("Negative yaw is the left"), Lib::ReactionDirection(-90.f), FString(TEXT("left")));

	const FCinemafyPromptReading FromBehind = Lib::MakeReactionReading(ECinemafyHitReaction::Hit, 180.f);
	const float Back = Lib::ScoreAnimation(FromBehind, Lib::TokenizeName(TEXT("/Game/Combat/Hit/AM_HitReact_Back")));
	const float Front = Lib::ScoreAnimation(FromBehind, Lib::TokenizeName(TEXT("/Game/Combat/Hit/AM_HitReact_Front")));
	TestTrue(TEXT("A blow from behind prefers a back hit react"), Back > Front);
	TestTrue(TEXT("...but a front one still qualifies"), Front > 0.f);
	TestTrue(TEXT("An attack is never a reaction"), Lib::ScoreAnimation(FromBehind, Lib::TokenizeName(TEXT("/Game/Combat/AM_Sword_Attack_01"))) < 0.f);
	TestTrue(TEXT("A death reaction finds deaths"),
		Lib::ScoreAnimation(Lib::MakeReactionReading(ECinemafyHitReaction::Death, 0.f), Lib::TokenizeName(TEXT("/Game/Combat/Die/AM_Death_Front"))) > 0.f);
	return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCinemafyDialogueCastTest, "Cinemafy.Director.CastsDialogueByTone", CinemafyTests::Flags)
bool FCinemafyDialogueCastTest::RunTest(const FString& Parameters)
{
	using Lib = UCinemafyChoreographyLibrary;
	using Dir = UCinemafyDirectorLibrary;

	auto Line = [](ECinemafyEmotion Emotion, float Intensity)
	{
		FCinemafyLine Out;
		Out.bSetEmotion = Emotion != ECinemafyEmotion::Neutral;
		Out.Emotion = Emotion;
		Out.Intensity = Intensity;
		return Out;
	};
	auto Score = [](const FCinemafyLine& L, bool bSpeaking, const TCHAR* Path) { return Dir::ScoreDialogueAnimation(L, bSpeaking, Lib::TokenizeName(Path)); };

	const TCHAR* Furious = TEXT("/Game/Dialogue/Vol1/IP/Furious/ANIM_IP_Dialogue_furious_talk_low");
	const TCHAR* Arguing = TEXT("/Game/CinemafyLibrary/Arguing/05_AA_Arguing_Standing_type01_Male_Female");
	const TCHAR* Sad = TEXT("/Game/Dialogue/Vol1/IP/Sad/ANIM_IP_Dialogue_Sad_talk_01");
	const TCHAR* Laugh = TEXT("/Game/Dialogue/Vol1/IP/Laugh/ANIM_IP_Dialogue_laugh_talk_low_01");
	const TCHAR* Calm = TEXT("/Game/Dialogue/Vol2/IP/Standing/ANIM_IP_Talk_Calm");
	const TCHAR* Listen = TEXT("/Game/Dialogue/Vol1/IP/Listening/ANIM_IP_Dialogue_listening_idle_01");
	const TCHAR* Shocked = TEXT("/Game/Dialogue/Vol2/IP/Standing/ANIM_IP_React_Shocked");

	const FCinemafyLine Angry = Line(ECinemafyEmotion::Angry, 0.9f);
	TestTrue(TEXT("An angry line plays furious over calm"), Score(Angry, true, Furious) > Score(Angry, true, Calm));
	TestTrue(TEXT("...and can argue"), Score(Angry, true, Arguing) > 0.f);
	TestTrue(TEXT("...but never laughs"), Score(Angry, true, Laugh) < Score(Angry, true, Furious));

	const FCinemafyLine Sorrow = Line(ECinemafyEmotion::Sad, 0.3f);
	TestTrue(TEXT("A sad line plays sad"), Score(Sorrow, true, Sad) > Score(Sorrow, true, Laugh) && Score(Sorrow, true, Sad) > Score(Sorrow, true, Furious));
	TestTrue(TEXT("...over merely serious"), Score(Sorrow, true, Sad) > Score(Sorrow, true, TEXT("/Game/Dialogue/Vol1/IP/Serious/ANIM_IP_Dialogue_Serious_gesture_03")));

	const FCinemafyLine Plain = Line(ECinemafyEmotion::Neutral, 0.3f);
	TestTrue(TEXT("A plain line talks calmly"), Score(Plain, true, Calm) > Score(Plain, true, Furious));
	TestTrue(TEXT("Arguing needs a heated line"), Score(Plain, true, Arguing) <= 0.f);

	TestTrue(TEXT("A listener listens"), Score(Plain, false, Listen) > 0.f);
	TestTrue(TEXT("...and doesn't talk"), Score(Plain, false, Calm) < 0.f);
	TestTrue(TEXT("A speaker doesn't just listen"), Score(Plain, true, Listen) < 0.f);
	FCinemafyLine Reveal = Plain;
	Reveal.Moment = ECinemafyMoment::Reveal;
	TestTrue(TEXT("A reveal shocks the listener"), Score(Reveal, false, Shocked) > Score(Reveal, false, Listen));

	TestTrue(TEXT("Sitting clips need a seat"), Score(Plain, true, TEXT("/Game/Dialogue/Vol2/IP/Sitting/ANIM_IP_SittingChair_Talk_ArmsOpen")) < 0.f);
	TestTrue(TEXT("...and the arguing pack's too"), Score(Angry, true, TEXT("/Game/CinemafyLibrary/Arguing/01_AA_Arguing_Siting_type01_Male_Female")) < 0.f);
	TestTrue(TEXT("Walking talk is for walk & talk"), Score(Plain, true, TEXT("/Game/Dialogue/Vol2/IP/Walking/ANIM_IP_Walk_TalkLeft")) < 0.f);
	TestTrue(TEXT("Root-motion copies are skipped"), Score(Plain, true, TEXT("/Game/Dialogue/Vol2/RM/Standing/ANIM_IP_Talk_Calm")) < 0.f);
	TestTrue(TEXT("Fights aren't dialogue"), Score(Angry, true, TEXT("/Game/Combat/AM_Punch_Attack_01")) < 0.f);

	FCinemafyLine Shouted;
	Shouted.Intensity = 0.7f;
	Shouted.Text = FText::FromString(TEXT("Get off my bridge!"));
	TestEqual(TEXT("A heated, shouted line reads angry"), Dir::ReadLineTone(Shouted), ECinemafyEmotion::Angry);

	// Picking: the best fit, varied, away from what was just used.
	TArray<FCinemafyMoveCandidate> Candidates;
	for (const TCHAR* Path : { Furious, Sad, Laugh, Calm, Listen })
	{
		FCinemafyMoveCandidate& C = Candidates.AddDefaulted_GetRef();
		C.Path = FSoftObjectPath(FString(Path) + TEXT(".") + FPaths::GetBaseFilename(Path));
		C.Name = FPaths::GetBaseFilename(Path);
		C.Tokens = Lib::TokenizeName(Path);
	}
	TestEqual(TEXT("An angry speaker is cast furious"), Dir::PickDialogueAnimation(Angry, true, Candidates, 3, {}), 0);
	TestEqual(TEXT("A sad speaker is cast sad"), Dir::PickDialogueAnimation(Sorrow, true, Candidates, 3, {}), 1);
	TestEqual(TEXT("The listener is cast listening"), Dir::PickDialogueAnimation(Plain, false, Candidates, 3, {}), 4);
	TestEqual(TEXT("Nothing fits a listener's reveal from these: still listening"), Dir::PickDialogueAnimation(Reveal, false, Candidates, 3, {}), 4);

	// Deaths: a deaths pack's names ("Deaths_Shot_In_Chest") are deaths too.
	const FCinemafyPromptReading Death = Lib::MakeReactionReading(ECinemafyHitReaction::Death, 0.f);
	TestTrue(TEXT("A deaths-pack clip is a death"), Lib::ScoreAnimation(Death, Lib::TokenizeName(TEXT("/Game/CinemafyLibrary/Deaths/Deaths_Shot_In_Chest"))) > 0.f);
	TestTrue(TEXT("...a chest shot is a death from the front"),
		Lib::ScoreAnimation(Death, Lib::TokenizeName(TEXT("/Game/CinemafyLibrary/Deaths/Deaths_Shot_In_Chest")))
		> Lib::ScoreAnimation(Death, Lib::TokenizeName(TEXT("/Game/CinemafyLibrary/Deaths/Deaths_Hit_Slow_Bleedout"))));
	TestTrue(TEXT("...but never a plain hit reaction"),
		Lib::ScoreAnimation(Lib::MakeReactionReading(ECinemafyHitReaction::Hit, 0.f), Lib::TokenizeName(TEXT("/Game/CinemafyLibrary/Deaths/Deaths_Hit_Left_Chest_Head"))) < 0.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCinemafyTraversalJudgeTest, "Cinemafy.Choreography.JudgesObstaclesBeforeRecording", CinemafyTests::Flags)
bool FCinemafyTraversalJudgeTest::RunTest(const FString& Parameters)
{
	using Lib = UCinemafyChoreographyLibrary;
	const FCinemafyTraversalEnvelope Envelope; // GASP-like defaults
	auto Judge = [&Envelope](float Height, float Depth, float Headroom, bool bLandingClear, float Angle = 0.f, const FCinemafyTraversalEnvelope* Override = nullptr)
	{
		FCinemafyTraversalCheck Check;
		Check.Height = Height;
		Check.Depth = Depth;
		Check.Headroom = Headroom;
		Check.ApproachAngle = Angle;
		Lib::JudgeTraversal(Check, Override ? *Override : Envelope, bLandingClear, 35.f);
		return Check;
	};

	TestEqual(TEXT("A kerb is stepped over"), Judge(20.f, 30.f, 250.f, true).Verdict, ECinemafyTraversalVerdict::StepOver);
	TestEqual(TEXT("A waist-high crate with room beyond is vaulted"), Judge(100.f, 50.f, 250.f, true).Verdict, ECinemafyTraversalVerdict::Vault);
	TestEqual(TEXT("A low wall with no room beyond, but wide enough to stand on, is mantled"), Judge(100.f, 75.f, 250.f, false).Verdict, ECinemafyTraversalVerdict::Mantle);
	const FCinemafyTraversalCheck Cramped = Judge(100.f, 40.f, 250.f, false);
	TestEqual(TEXT("A thin rail against a wall: no room to land"), Cramped.Verdict, ECinemafyTraversalVerdict::NoRoomToLand);
	TestFalse(TEXT("...and that's a no"), Cramped.bPossible);
	TestTrue(TEXT("...and it says so"), Cramped.Reason.Contains(TEXT("no room to land")));
	const FCinemafyTraversalCheck Tall = Judge(310.f, 400.f, 250.f, false);
	TestEqual(TEXT("A 3 m ledge is too tall"), Tall.Verdict, ECinemafyTraversalVerdict::TooTall);
	TestTrue(TEXT("...with the numbers"), Tall.Reason.Contains(TEXT("310")) && Tall.Reason.Contains(TEXT("275")));
	TestEqual(TEXT("A platform is climbed onto"), Judge(180.f, 400.f, 250.f, false).Verdict, ECinemafyTraversalVerdict::Mantle);
	TestEqual(TEXT("Under a low ceiling there's no headroom"), Judge(180.f, 400.f, 50.f, false).Verdict, ECinemafyTraversalVerdict::NoHeadroom);
	TestEqual(TEXT("Coming in at a glancing angle is refused"), Judge(100.f, 50.f, 250.f, true, 70.f).Verdict, ECinemafyTraversalVerdict::BadAngle);

	FCinemafyTraversalEnvelope NoSystem;
	NoSystem.bCanTraverse = false;
	TestEqual(TEXT("Without traversal, a low box is jumped"), Judge(50.f, 50.f, 250.f, true, 0.f, &NoSystem).Verdict, ECinemafyTraversalVerdict::Jump);
	TestEqual(TEXT("...and a crate is out of reach"), Judge(100.f, 50.f, 250.f, true, 0.f, &NoSystem).Verdict, ECinemafyTraversalVerdict::CannotTraverse);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCinemafyChoreoDialogueTest, "Cinemafy.Choreography.LinesAndActionInOneScene", CinemafyTests::Flags)
bool FCinemafyChoreoDialogueTest::RunTest(const FString& Parameters)
{
	using Lib = UCinemafyChoreographyLibrary;

	// Prompts: quoted words are a line; speech words make it a Say beat; "answer" waits for the other line.
	const FCinemafyPromptReading WalkTalk = Lib::ReadPrompt(TEXT("walk \"Stand down, Admiral\""));
	TestEqual(TEXT("walk \"...\" still walks"), WalkTalk.Kind, ECinemafyActionKind::Move);
	TestEqual(TEXT("...saying the line"), WalkTalk.Line, FString(TEXT("Stand down, Admiral")));
	const FCinemafyPromptReading Bare = Lib::ReadPrompt(TEXT("\"Make me.\""));
	TestEqual(TEXT("Just a quote is a Say beat"), Bare.Kind, ECinemafyActionKind::Say);
	const FCinemafyPromptReading Answer = Lib::ReadPrompt(TEXT("answer \"Make me.\""));
	TestEqual(TEXT("An answer is said"), Answer.Kind, ECinemafyActionKind::Say);
	TestTrue(TEXT("...after the other line"), Answer.bAfterLine);
	const FCinemafyPromptReading Draw = Lib::ReadPrompt(TEXT("sword after"));
	TestEqual(TEXT("\"sword after\" draws"), Draw.Kind, ECinemafyActionKind::Equip);
	TestTrue(TEXT("...once they've finished talking"), Draw.bAfterLine);
	TestTrue(TEXT("The quote doesn't search animation names"), !Lib::ReadPrompt(TEXT("taunt \"Come on then\"")).Keywords.Contains(TEXT("come")));

	// The take: walk-and-talk, a pause, then the draw and the strike.
	CinemafyTests::FTestScene Scene;
	APawn* Thane = Scene.World->SpawnActor<APawn>();
	auto MakeBeat = [](const TCHAR* Prompt, float Start, float End, AActor* Target)
	{
		FCinemafyChoreoBeat Beat;
		Beat.Prompt = Prompt;
		Beat.Reading = UCinemafyChoreographyLibrary::ReadPrompt(Prompt);
		Beat.StartTime = Start;
		Beat.EndTime = End;
		Beat.Target = Target;
		return Beat;
	};
	TArray<FCinemafyChoreoBeat> Beats;
	Beats.Add(MakeBeat(TEXT("walk \"Stand down, Admiral\""), 0.f, 4.f, nullptr));
	Beats.Last().Listener = Thane;
	Beats.Last().LineStartTime = 0.f;
	Beats.Last().LineDuration = 2.5f;
	Beats.Add(MakeBeat(TEXT("look"), 4.f, 5.f, Thane));
	Beats.Add(MakeBeat(TEXT("sword after"), 7.f, 8.f, nullptr));
	Beats.Add(MakeBeat(TEXT("sword attack"), 8.f, 10.f, Thane));
	Beats.Last().ImpactTimes = { 8.6f };

	const TArray<FCinemafyLine> Lines = Lib::BuildDirectorLines(Beats, Scene.Kara, 10.5f);
	TestTrue(TEXT("At least the line, the beats and the strike"), Lines.Num() >= 3);
	if (Lines.Num() < 3)
	{
		return true;
	}
	TestEqual(TEXT("The walk carries the words"), Lines[0].Text.ToString(), FString(TEXT("Stand down, Admiral")));
	TestEqual(TEXT("...as walk-and-talk"), Lines[0].Moment, ECinemafyMoment::WalkAndTalk);
	TestTrue(TEXT("...said to Thane"), Lines[0].Listener == Thane);
	TestTrue(TEXT("The strike is still action"), Lines.Last().bAction && Lines.Last().ImpactTimes.Num() == 1);
	float Total = 0.f;
	for (const FCinemafyLine& Line : Lines)
	{
		Total += Line.Duration + Line.PauseAfter;
	}
	TestTrue(TEXT("One scene: the lines cover the whole take"), FMath::IsNearlyEqual(Total, 10.5f, 0.01f));

	// An answer that starts after its beat began is cut on its first word.
	TArray<FCinemafyChoreoBeat> Reply;
	Reply.Add(MakeBeat(TEXT("answer \"Make me.\""), 0.5f, 6.f, Scene.Kara));
	Reply.Last().LineStartTime = 3.f;
	Reply.Last().LineDuration = 1.2f;
	const TArray<FCinemafyLine> ReplyLines = Lib::BuildDirectorLines(Reply, Thane, 6.f);
	TestEqual(TEXT("A quiet listen, then the answer"), ReplyLines.Num(), 2);
	if (ReplyLines.Num() == 2)
	{
		TestTrue(TEXT("...which starts on its first word"), FMath::IsNearlyEqual(ReplyLines[0].Duration, 3.f, 0.01f));
		TestEqual(TEXT("...with its words"), ReplyLines[1].Text.ToString(), FString(TEXT("Make me.")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCinemafyFightCoverageTest, "Cinemafy.Director.WatchesTheFight", CinemafyTests::Flags)
bool FCinemafyFightCoverageTest::RunTest(const FString& Parameters)
{
	CinemafyTests::FTestScene Scene;
	TArray<FCinemafyLine> Lines = Scene.MakeConversation(1);
	Lines[0].bAction = true;
	Lines[0].Duration = 6.f;
	Lines[0].PauseAfter = 0.f;
	Lines[0].ImpactTimes = { 1.5f, 3.0f, 4.5f };
	Lines[0].bLethal = true;

	for (const ECinemafyDirectorPreset Preset : { ECinemafyDirectorPreset::KineticAction, ECinemafyDirectorPreset::ClassicHollywood })
	{
		const FCinemafyDirectorStyle Style = UCinemafyDirectorLibrary::GetPresetStyle(Preset);
		for (int32 Seed = 1; Seed <= 6; ++Seed)
		{
			const TArray<FCinemafyPlannedShot> Plan = UCinemafyDirectorLibrary::PlanScene(Lines, Style, Seed);
			int32 Ramps = 0;
			for (const FCinemafyPlannedShot& Shot : Plan)
			{
				const bool bHasImpact = Lines[0].ImpactTimes.ContainsByPredicate([&Shot](float I) { return I >= Shot.StartTime && I < Shot.StartTime + Shot.Duration; });
				if (bHasImpact)
				{
					const bool bBothSides = Shot.ShotType == ECinemafyShotType::OverTheShoulder || Shot.ShotType == ECinemafyShotType::MediumTwoShot
						|| Shot.ShotType == ECinemafyShotType::WideTwoShot;
					TestTrue(FString::Printf(TEXT("Seed %d: every hit shows both fighters (%s)"), Seed, *Shot.Reason), bBothSides);
				}
				Ramps += Shot.bSlowMo ? 1 : 0;
				if (Shot.bSlowMo)
				{
					TestTrue(TEXT("The ramp is on the killing blow"), FMath::IsNearlyEqual(Shot.StartTime + Shot.SlowMoTime, 4.5f, 0.01f));
				}
			}
			if (Style.SlowMoChance > 0.f)
			{
				TestEqual(FString::Printf(TEXT("Seed %d: exactly one ramp, on the kill"), Seed), Ramps, 1);
			}
		}
	}

	// Gentle styles ease into it; action styles drop hard.
	const TArray<FCinemafyPlannedShot> Kinetic = UCinemafyDirectorLibrary::PlanScene(Lines, UCinemafyDirectorLibrary::GetPresetStyle(ECinemafyDirectorPreset::KineticAction), 3);
	const FCinemafyPlannedShot* KineticRamp = Kinetic.FindByPredicate([](const FCinemafyPlannedShot& Shot) { return Shot.bSlowMo; });
	TestTrue(TEXT("An action style ramps hard"), KineticRamp && KineticRamp->SlowMoRate <= 0.35f);

	// A shoulder in the foreground is framing; a shoulder over the face is a ruined shot.
	const FVector Camera(0.f, 0.f, 160.f);
	const FVector Face(400.f, 0.f, 160.f);
	TestTrue(TEXT("Head straight in line covers the face"),
		UCinemafyFramingLibrary::IsFaceCoveredByForeground(Camera, Face, FVector(120.f, 0.f, 160.f), FVector(0.f, 1.f, 0.f)));
	TestFalse(TEXT("A shoulder to the side frames it"),
		UCinemafyFramingLibrary::IsFaceCoveredByForeground(Camera, Face, FVector(120.f, -60.f, 160.f), FVector(0.f, 1.f, 0.f)));
	TestFalse(TEXT("Someone behind the subject can't cover them"),
		UCinemafyFramingLibrary::IsFaceCoveredByForeground(Camera, Face, FVector(500.f, 0.f, 160.f), FVector(0.f, 1.f, 0.f)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCinemafyTraversalRouteTest, "Cinemafy.Choreography.PlansGapsDropsAndRoutes", CinemafyTests::Flags)
bool FCinemafyTraversalRouteTest::RunTest(const FString& Parameters)
{
	using Lib = UCinemafyChoreographyLibrary;
	const FCinemafyTraversalEnvelope Envelope;
	auto Gap = [&Envelope](float Width, float Drop)
	{
		FCinemafyTraversalCheck Check;
		Check.GapWidth = Width;
		Check.DropHeight = Drop;
		Lib::JudgeGap(Check, Envelope);
		return Check;
	};
	TestEqual(TEXT("A 1.5 m gap is jumped"), Gap(150.f, 0.f).Verdict, ECinemafyTraversalVerdict::Gap);
	TestEqual(TEXT("A 4 m gap is too wide"), Gap(400.f, 0.f).Verdict, ECinemafyTraversalVerdict::TooFar);
	TestEqual(TEXT("A far side 1 m up can't be jumped to"), Gap(150.f, -100.f).Verdict, ECinemafyTraversalVerdict::TooTall);
	TestEqual(TEXT("A 2 m ledge is a drop"), Gap(0.f, 200.f).Verdict, ECinemafyTraversalVerdict::Drop);
	TestTrue(TEXT("...a 3 m one says it rolls"), Gap(0.f, 300.f).Reason.Contains(TEXT("roll")));
	TestEqual(TEXT("A 10 m ledge is too high"), Gap(0.f, 1000.f).Verdict, ECinemafyTraversalVerdict::TooHighToDrop);

	// Routes: a run-up straight at the takeoff, never behind the start, and out the far side.
	FCinemafyTraversalCheck Vault;
	Vault.Verdict = ECinemafyTraversalVerdict::Vault;
	Vault.bPossible = true;
	Vault.TakeoffPoint = FVector(500.f, 0.f, 0.f);
	Vault.FrontPoint = FVector(500.f, 0.f, 40.f);
	Vault.FarPoint = FVector(550.f, 0.f, 100.f);
	Vault.LandingPoint = FVector(640.f, 0.f, 0.f);
	Lib::PlanTraversalRoute(Vault, FVector::ZeroVector, Envelope);
	TestTrue(TEXT("The run-up starts a vault's run-up length before the takeoff"), FMath::IsNearlyEqual(Vault.ApproachPoint.X, 500.f - Envelope.VaultRunUp, 1.f));
	TestTrue(TEXT("...and exits on the landing"), Vault.ExitPoint.Equals(FVector(640.f, 0.f, 0.f), 1.f));
	FCinemafyTraversalCheck Close = Vault;
	Lib::PlanTraversalRoute(Close, FVector(450.f, 0.f, 0.f), Envelope);
	TestTrue(TEXT("Starting close, the run-up never goes behind the performer"), Close.ApproachPoint.X >= 449.f);
	FCinemafyTraversalCheck Mantle = Vault;
	Mantle.Verdict = ECinemafyTraversalVerdict::Mantle;
	Mantle.Depth = 300.f;
	Lib::PlanTraversalRoute(Mantle, FVector::ZeroVector, Envelope);
	TestTrue(TEXT("A mantle ends up on top"), FMath::IsNearlyEqual(Mantle.ExitPoint.Z, 100.f, 1.f));

	// Transitions: draw before the strike, brake out of a sprint, roll out of a big landing.
	CinemafyTests::FTestScene Scene;
	APawn* Thane = Scene.World->SpawnActor<APawn>();
	auto MakeBeat = [](const TCHAR* Prompt, AActor* Target = nullptr)
	{
		FCinemafyChoreoBeat Beat;
		Beat.Prompt = Prompt;
		Beat.Reading = Lib::ReadPrompt(Prompt);
		Beat.Target = Target;
		return Beat;
	};
	TArray<FCinemafyChoreoBeat> Beats;
	Beats.Add(MakeBeat(TEXT("sprint")));
	Beats.Add(MakeBeat(TEXT("jump")));
	Beats.Last().TraversalCheck = Gap(200.f, 320.f);
	Beats.Last().TraversalCheck.bPossible = true;
	Beats.Add(MakeBeat(TEXT("run")));
	Beats.Add(MakeBeat(TEXT("sword attack"), Thane));
	Beats.Add(MakeBeat(TEXT("sword attack"), Thane));
	Beats.Add(MakeBeat(TEXT("shoot"), Thane));
	Lib::PlanTransitions(Beats, FString(), Envelope);
	TestTrue(TEXT("Sprint into a jump: brakes first"), Beats[1].Transitions.bBrakeIn);
	TestEqual(TEXT("...and rolls out of the 3 m landing (it keeps running)"), Beats[1].Transitions.Recovery, ECinemafyRecovery::Roll);
	TestEqual(TEXT("The first sword attack draws the sword"), Beats[3].Transitions.DrawWeapon, FString(TEXT("sword")));
	TestTrue(TEXT("...the second doesn't (it's in hand)"), Beats[4].Transitions.DrawWeapon.IsEmpty());
	TestEqual(TEXT("Shooting with a sword in hand draws a sidearm"), Beats[5].Transitions.DrawWeapon, FString(TEXT("pistol")));
	TestFalse(TEXT("No brake from a run"), Beats[3].Transitions.bBrakeIn);

	TArray<FCinemafyChoreoBeat> Armed;
	Armed.Add(MakeBeat(TEXT("sword attack"), Thane));
	Lib::PlanTransitions(Armed, TEXT("sword"), Envelope);
	TestTrue(TEXT("Starting with the sword out: nothing to draw"), Armed[0].Transitions.DrawWeapon.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCinemafyScriptBreakdownTest, "Cinemafy.Script.BreaksDownActsChaptersScenes", CinemafyTests::Flags)
bool FCinemafyScriptBreakdownTest::RunTest(const FString& Parameters)
{
	using Script = UCinemafyScriptLibrary;
	FString Setting, Location, Time;
	TestTrue(TEXT("A scene heading"), Script::ParseHeading(TEXT("INT. BRIDGE - NIGHT"), Setting, Location, Time));
	TestEqual(TEXT("...its place"), Location, FString(TEXT("BRIDGE")));
	TestEqual(TEXT("...its time"), Time, FString(TEXT("NIGHT")));
	TestTrue(TEXT("En dashes and INT/EXT"), Script::ParseHeading(TEXT("EXT./INT. CAR \u2013 DAY"), Setting, Location, Time) && Setting == TEXT("INT/EXT") && Time == TEXT("DAY"));
	TestFalse(TEXT("Not a heading"), Script::ParseHeading(TEXT("Interesting idea."), Setting, Location, Time));

	// A screenplay with marked acts: cues, parentheticals, transitions, continuity.
	const FString Screenplay = TEXT(
		"Title: The Bridge\n"
		"Author: Test\n"
		"\n"
		"# ACT ONE\n"
		"\n"
		"INT. BRIDGE - NIGHT\n"
		"\n"
		"Kade walks to the console. She draws her pistol.\n"
		"\n"
		"KADE\n"
		"Stand down, Admiral.\n"
		"\n"
		"THANE\n"
		"(coldly)\n"
		"Make me.\n"
		"\n"
		"Thane is wounded. He limps away.\n"
		"\n"
		"FADE OUT.\n"
		"\n"
		"# ACT TWO\n"
		"\n"
		"EXT. HANGAR - DAY\n"
		"\n"
		"THANE\n"
		"You'll regret this.\n"
		"\n"
		"Kade kills Thane.\n"
		"\n"
		"INT. CORRIDOR - CONTINUOUS\n"
		"\n"
		"THANE\n"
		"Still here.\n");
	FString Title, Report;
	const TArray<FCinemafyScriptAct> Acts = Script::BreakDownScript(Screenplay, true, 5, Title, Report);
	TestEqual(TEXT("The title page"), Title, FString(TEXT("The Bridge")));
	TestEqual(TEXT("Two marked acts"), Acts.Num(), 2);
	if (Acts.Num() == 2 && Acts[0].Chapters.Num() > 0 && Acts[0].Chapters[0].Scenes.Num() > 0 && Acts[1].Chapters.Num() > 0 && Acts[1].Chapters.Last().Scenes.Num() > 0)
	{
		TestFalse(TEXT("...not inferred"), Acts[0].bInferred);
		const FCinemafyScriptScene& Bridge = Acts[0].Chapters[0].Scenes[0];
		TestEqual(TEXT("Scene ids are act.chapter.scene"), Bridge.Id, FString(TEXT("1.1.1")));
		TestEqual(TEXT("Two lines on the bridge"), Bridge.DialogueLines, 2);
		TestTrue(TEXT("Kade and Thane are in it"), Bridge.Characters.Contains(TEXT("KADE")) && Bridge.Characters.Contains(TEXT("THANE")));
		const FCinemafyScriptLine* Make = Bridge.Lines.FindByPredicate([](const FCinemafyScriptLine& L) { return L.Text == TEXT("Make me."); });
		TestTrue(TEXT("Parentheticals are how it's said"), Make && Make->Direction == TEXT("(coldly)"));
		TestTrue(TEXT("Kade picks up her pistol"), Bridge.Changes.ContainsByPredicate([](const FCinemafyContinuityFact& F) { return F.Character == TEXT("KADE") && F.Category == TEXT("Carrying") && F.Fact.Contains(TEXT("pistol")); }));

		const FCinemafyScriptScene& Hangar = Acts[1].Chapters[0].Scenes[0];
		TestTrue(TEXT("Thane carries his wound into the hangar"), Hangar.CarriedIn.ContainsByPredicate([](const FCinemafyContinuityFact& F) { return F.Character == TEXT("THANE") && F.Category == TEXT("Injury"); }));
		TestTrue(TEXT("...and Kade her pistol"), Hangar.CarriedIn.ContainsByPredicate([](const FCinemafyContinuityFact& F) { return F.Character == TEXT("KADE") && F.Fact.Contains(TEXT("pistol")); }));

		const FCinemafyScriptScene& Corridor = Acts[1].Chapters.Last().Scenes.Last();
		TestTrue(TEXT("The dead don't talk: flagged"), Corridor.Warnings.ContainsByPredicate([](const FString& W) { return W.Contains(TEXT("THANE")) && W.Contains(TEXT("died")); }));
	}

	// A game script: mission markers, bracketed scenes, NAME: lines, no blank lines.
	const FString Game = TEXT(
		"MISSION 1: Blackout\n"
		"[Scene: Hangar]\n"
		"KADE: We go in quiet.\n"
		"VOSS: Quiet's my middle name.\n"
		"[Scene: Corridor]\n"
		"KADE: Lights.\n"
		"MISSION 2: Fallout\n"
		"[Scene: Reactor]\n"
		"VOSS: It's going to blow!\n");
	const TArray<FCinemafyScriptAct> GameActs = Script::BreakDownScript(Game, true, 5, Title, Report);
	TestEqual(TEXT("A short game script is one act"), GameActs.Num(), 1);
	if (GameActs.Num() == 1)
	{
		TestEqual(TEXT("Two missions are two chapters"), GameActs[0].Chapters.Num(), 2);
		if (GameActs[0].Chapters.Num() == 2)
		{
			TestEqual(TEXT("...the first has two scenes"), GameActs[0].Chapters[0].Scenes.Num(), 2);
			TestTrue(TEXT("...named by the script"), GameActs[0].Chapters[0].Title.Contains(TEXT("Blackout")));
			const FCinemafyScriptScene& Hangar = GameActs[0].Chapters[0].Scenes[0];
			TestEqual(TEXT("NAME: lines are dialogue"), Hangar.DialogueLines, 2);
			TestTrue(TEXT("A director with those lines (reworded on the day) maps to it"),
				Script::ScoreLinesAgainstScene({ TEXT("We go in quiet"), TEXT("Quiet is my middle name") }, Hangar) >= 0.99f);
			TestTrue(TEXT("...and not to another scene"), Script::ScoreLinesAgainstScene({ TEXT("We go in quiet") }, GameActs[0].Chapters[1].Scenes[0]) < 0.5f);
		}
	}

	// No act markers: three acts placed on the turns near the quarter marks.
	FString Long;
	for (int32 Index = 0; Index < 12; ++Index)
	{
		if (Index == 3)
		{
			Long += TEXT("FADE OUT.\n\n");
		}
		Long += FString::Printf(TEXT("INT. ROOM %d - %s\n\nKADE: Hello there, friend number %d.\n\n"), Index + 1, Index == 9 ? TEXT("YEARS LATER") : TEXT("DAY"), Index);
	}
	const TArray<FCinemafyScriptAct> Inferred = Script::BreakDownScript(Long, true, 5, Title, Report);
	TestEqual(TEXT("Three inferred acts"), Inferred.Num(), 3);
	if (Inferred.Num() == 3 && Inferred[1].Chapters.Num() > 0 && Inferred[2].Chapters.Num() > 0)
	{
		TestTrue(TEXT("...inferred"), Inferred[1].bInferred);
		TestEqual(TEXT("Act Two opens after the fade out"), Inferred[1].Chapters[0].Scenes[0].Number, 4);
		TestEqual(TEXT("Act Three opens on the time jump"), Inferred[2].Chapters[0].Scenes[0].Number, 10);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCinemafyPrevisStagingTest, "Cinemafy.Previs.StagesScenesFromTheScript", CinemafyTests::Flags)
bool FCinemafyPrevisStagingTest::RunTest(const FString& Parameters)
{
	const FString Script = TEXT(
		"INT. BRIDGE - NIGHT\n"
		"\n"
		"Kade steps off the lift. She draws her pistol.\n"
		"\n"
		"KADE\n"
		"Stand down, Admiral.\n"
		"\n"
		"ADMIRAL THANE\n"
		"Make me.\n"
		"\n"
		"Kade walks to the console. Thane turns to Kade.\n"
		"\n"
		"KADE\n"
		"Last chance.\n"
		"\n"
		"Thane vaults the railing, Kade shoots Thane.\n");
	FString Title, Report;
	const TArray<FCinemafyScriptAct> Acts = UCinemafyScriptLibrary::BreakDownScript(Script, true, 5, Title, Report);
	if (!TestTrue(TEXT("One scene"), Acts.Num() == 1 && Acts[0].Chapters.Num() == 1 && Acts[0].Chapters[0].Scenes.Num() == 1))
	{
		return true;
	}
	TArray<FString> NotStaged;
	const TArray<FCinemafyStagedBeat> Beats = UCinemafyPrevisLibrary::StageScene(Acts[0].Chapters[0].Scenes[0], NotStaged);
	auto Find = [&Beats](const TCHAR* Who, const TCHAR* Prompt) { return Beats.FindByPredicate([Who, Prompt](const FCinemafyStagedBeat& B) { return B.Character == Who && B.Prompt == Prompt; }); };

	const FCinemafyStagedBeat* Steps = Find(TEXT("KADE"), TEXT("walk"));
	TestTrue(TEXT("Kade steps off the lift: a walk to the lift"), Steps && Steps->Place == TEXT("lift"));
	TestNotNull(TEXT("She draws her pistol (pronoun -> Kade)"), Find(TEXT("KADE"), TEXT("pistol")));
	const FCinemafyStagedBeat* Answer = Find(TEXT("ADMIRAL THANE"), TEXT("say"));
	TestTrue(TEXT("Thane's answer waits for Kade's first line"), Answer && Answer->WaitFor == TEXT("KADE") && Answer->WaitForLineNumber == 1 && Answer->Target == TEXT("KADE"));
	const FCinemafyStagedBeat* Console = Beats.FindByPredicate([](const FCinemafyStagedBeat& B) { return B.Character == TEXT("KADE") && B.Prompt == TEXT("walk") && B.Place == TEXT("console"); });
	TestTrue(TEXT("Walking to the console waits for Thane's line"), Console && Console->WaitFor == TEXT("ADMIRAL THANE"));
	TestTrue(TEXT("...and carries her next line (walk and talk)"), Console && Console->Line == TEXT("Last chance."));
	const FCinemafyStagedBeat* Turn = Find(TEXT("ADMIRAL THANE"), TEXT("look"));
	TestTrue(TEXT("Thane turns to Kade: a look at her"), Turn && Turn->Target == TEXT("KADE"));
	const FCinemafyStagedBeat* Vault = Find(TEXT("ADMIRAL THANE"), TEXT("vault"));
	TestTrue(TEXT("Thane vaults the railing"), Vault && Vault->Place == TEXT("railing"));
	const FCinemafyStagedBeat* Shot = Beats.FindByPredicate([](const FCinemafyStagedBeat& B) { return B.Character == TEXT("KADE") && B.Prompt == TEXT("shoot"); });
	TestTrue(TEXT("Kade shoots Thane"), Shot && Shot->Target == TEXT("ADMIRAL THANE"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCinemafyStoryFlowTest, "Cinemafy.Story.ChoicesDivertsAndFlags", CinemafyTests::Flags)
bool FCinemafyStoryFlowTest::RunTest(const FString& Parameters)
{
	const FString Script = TEXT(
		"INT. BRIDGE - NIGHT\n"
		"\n"
		"KADE: Stand down.\n"
		"\n"
		"* [Spare him] -> MED BAY ~ spared_thane\n"
		"* {has_pistol} Shoot him -> #3\n"
		"\n"
		"INT. CORRIDOR - NIGHT\n"
		"\n"
		"KADE: Where is he?\n"
		"-> END\n"
		"\n"
		"INT. HANGAR - DAY\n"
		"\n"
		"KADE: It's done.\n"
		"\n"
		"INT. MED BAY - DAY\n"
		"? spared_thane\n"
		"~ thane_alive\n"
		"\n"
		"THANE: You hesitated.\n");
	UCinemafyScriptProject* Project = NewObject<UCinemafyScriptProject>();
	Project->BreakDown(Script);
	TArray<FCinemafyScriptScene*> Scenes = Project->GetAllScenes();
	if (!TestEqual(TEXT("Four scenes"), Scenes.Num(), 4))
	{
		return true;
	}
	const FCinemafyScriptScene& Bridge = *Scenes[0];
	TestEqual(TEXT("Two choices on the bridge"), Bridge.Choices.Num(), 2);
	if (Bridge.Choices.Num() == 2)
	{
		TestEqual(TEXT("...the words"), Bridge.Choices[0].Text, FString(TEXT("Spare him")));
		TestEqual(TEXT("...going to the med bay (by place)"), Bridge.Choices[0].TargetSceneId, Scenes[3]->Id);
		TestTrue(TEXT("...setting a flag"), Bridge.Choices[0].SetFlags.Contains(TEXT("spared_thane")));
		TestTrue(TEXT("A conditional choice"), Bridge.Choices[1].RequireFlags.Contains(TEXT("has_pistol")));
		TestEqual(TEXT("...going to scene #3"), Bridge.Choices[1].TargetSceneId, Scenes[2]->Id);
	}
	TestEqual(TEXT("A divert to the end"), Scenes[1]->DivertSceneId, FString(TEXT("END")));
	TestTrue(TEXT("Scene conditions and flags"), Scenes[3]->RequireFlags.Contains(TEXT("spared_thane")) && Scenes[3]->SetFlags.Contains(TEXT("thane_alive")));
	TestEqual(TEXT("Choice lines aren't dialogue"), Bridge.DialogueLines, 1);

	TArray<FCinemafyScriptChoice> Offered;
	TSet<FName> NoFlags;
	UCinemafyStorySubsystem::NextSceneAfter(Project, Bridge.Id, NoFlags, Offered);
	TestEqual(TEXT("Without a pistol, only one choice is offered"), Offered.Num(), 1);
	TSet<FName> Armed = { FName(TEXT("has_pistol")) };
	UCinemafyStorySubsystem::NextSceneAfter(Project, Bridge.Id, Armed, Offered);
	TestEqual(TEXT("With one, both are"), Offered.Num(), 2);
	TestEqual(TEXT("The hangar leads on past the med bay when Thane wasn't spared"), UCinemafyStorySubsystem::NextSceneAfter(Project, Scenes[2]->Id, NoFlags, Offered), FString(TEXT("END")));
	TSet<FName> Spared = { FName(TEXT("spared_thane")) };
	TestEqual(TEXT("...and into it when he was"), UCinemafyStorySubsystem::NextSceneAfter(Project, Scenes[2]->Id, Spared, Offered), Scenes[3]->Id);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCinemafyMoodTest, "Cinemafy.Story.ReadsMoodAndExtras", CinemafyTests::Flags)
bool FCinemafyMoodTest::RunTest(const FString& Parameters)
{
	FCinemafyScriptScene Fight;
	Fight.Heading = TEXT("EXT. HANGAR DECK - NIGHT");
	Fight.Setting = TEXT("EXT");
	Fight.TimeOfDay = TEXT("NIGHT");
	Fight.bHasAction = true;
	FCinemafyScriptLine Action;
	Action.Text = TEXT("They fight in the rain. Kade slashes, Thane attacks.");
	Fight.Lines.Add(Action);
	const FCinemafySceneMood FightMood = UCinemafyMoodLibrary::ReadSceneMood(Fight, ECinemafyTimeOfDay::Unknown);
	TestEqual(TEXT("A night fight is night"), FightMood.TimeOfDay, ECinemafyTimeOfDay::Night);
	TestEqual(TEXT("...and action"), FightMood.Mood, FString(TEXT("action")));
	TestEqual(TEXT("...cut kinetic"), FightMood.DirectorStyle, ECinemafyDirectorPreset::KineticAction);
	TestTrue(TEXT("...in the rain"), FightMood.Weather.Contains(TEXT("rain")));
	TestTrue(TEXT("...exterior"), FightMood.bExterior);

	FCinemafyScriptScene Grief;
	Grief.Heading = TEXT("INT. MED BAY - CONTINUOUS");
	Grief.TimeOfDay = TEXT("CONTINUOUS");
	FCinemafyScriptLine Tears;
	Tears.Text = TEXT("Kade sits by the bed. Tears. She mourns him.");
	Grief.Lines.Add(Tears);
	const FCinemafySceneMood GriefMood = UCinemafyMoodLibrary::ReadSceneMood(Grief, ECinemafyTimeOfDay::Dusk);
	TestEqual(TEXT("CONTINUOUS keeps the time of day"), GriefMood.TimeOfDay, ECinemafyTimeOfDay::Dusk);
	TestEqual(TEXT("Grief reads sad"), GriefMood.Mood, FString(TEXT("sad")));
	TestEqual(TEXT("...a sad dusk is lyrical"), GriefMood.DirectorStyle, ECinemafyDirectorPreset::GoldenHourLyrical);

	FString Place;
	TestEqual(TEXT("Crew works at consoles"), ACinemafyExtras::ReadDirection(TEXT("crew works at consoles"), Place), ECinemafyExtrasBehavior::Work);
	TestEqual(TEXT("...at the console marks"), Place, FString(TEXT("console")));
	TestEqual(TEXT("Marines patrol"), ACinemafyExtras::ReadDirection(TEXT("marines patrol the corridor"), Place), ECinemafyExtrasBehavior::Patrol);
	TestEqual(TEXT("Two officers talk"), ACinemafyExtras::ReadDirection(TEXT("two officers talk"), Place), ECinemafyExtrasBehavior::Talk);
	TestEqual(TEXT("A crowd mills about"), ACinemafyExtras::ReadDirection(TEXT("crowd mills about"), Place), ECinemafyExtrasBehavior::Wander);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCinemafyDeliveryTest, "Cinemafy.Script.ParentheticalsDriveTheDelivery", CinemafyTests::Flags)
bool FCinemafyDeliveryTest::RunTest(const FString& Parameters)
{
	using Script = UCinemafyScriptLibrary;
	const FCinemafyDelivery Cold = Script::ReadDelivery(TEXT("(coldly)"), TEXT("Make me."));
	TestTrue(TEXT("(coldly) is contempt"), Cold.bHasEmotion && Cold.Emotion == ECinemafyEmotion::Contempt);
	const FCinemafyDelivery Shout = Script::ReadDelivery(TEXT("(shouting)"), TEXT("Get down"));
	TestTrue(TEXT("(shouting) is angry and loud"), Shout.Emotion == ECinemafyEmotion::Angry && Shout.Intensity >= 0.85f && Shout.Volume >= 1.f);
	const FCinemafyDelivery Whisper = Script::ReadDelivery(TEXT("(whispering)"), TEXT("They're here."));
	TestTrue(TEXT("(whispering) is quiet and slow"), Whisper.bWhisper && Whisper.Volume < 0.5f && Whisper.Rate < 1.f);
	TestFalse(TEXT("...and leaves the feeling to the scene"), Whisper.bHasEmotion);
	const FCinemafyDelivery Tears = Script::ReadDelivery(TEXT("(through tears)"), TEXT("He's gone."));
	TestEqual(TEXT("(through tears) is sad"), Tears.Emotion, ECinemafyEmotion::Sad);
	const FCinemafyDelivery Caps = Script::ReadDelivery(FString(), TEXT("GET OFF MY BRIDGE"));
	TestTrue(TEXT("A line in capitals is shouted"), Caps.bHasEmotion && Caps.Emotion == ECinemafyEmotion::Angry);
	TestFalse(TEXT("A plain line has no direction"), Script::ReadDelivery(FString(), TEXT("Stand down, Admiral.")).bHasEmotion);
	TestFalse(TEXT("V.O. isn't a feeling"), Script::ReadDelivery(TEXT("(V.O.)"), TEXT("You always did hesitate.")).bHasEmotion);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCinemafyRevisionTest, "Cinemafy.Script.RevisionsFindWhatChanged", CinemafyTests::Flags)
bool FCinemafyRevisionTest::RunTest(const FString& Parameters)
{
	const FString Draft1 = TEXT(
		"INT. BRIDGE - NIGHT\n\n"
		"KADE\nStand down, Admiral.\n\n"
		"THANE\nMake me.\n\n"
		"EXT. HANGAR - DAY\n\n"
		"THANE\nYou'll regret this.\n");
	const FString Draft2 = TEXT(
		"INT. BRIDGE - NIGHT\n\n"
		"KADE\nStand down, Thane. Now.\n\n"
		"THANE\nMake me.\n\n"
		"EXT. HANGAR - DAY\n\n"
		"THANE\nYou'll regret this.\n\n"
		"INT. BRIG - NIGHT\n\n"
		"KADE\nComfortable?\n");
	UCinemafyScriptProject* Project = NewObject<UCinemafyScriptProject>(GetTransientPackage());
	Project->bInferActs = false;
	Project->BreakDown(Draft1);
	TestTrue(TEXT("A first import is no revision"), Project->RevisionReport.IsEmpty());
	TArray<FCinemafyScriptScene*> Scenes = Project->GetAllScenes();
	if (!TestEqual(TEXT("Two scenes"), Scenes.Num(), 2))
	{
		return false;
	}
	// The bridge is staged, and Kade's line was recorded.
	const FSoftObjectPath Recorded(TEXT("/Game/VO/Kade_Bridge.Kade_Bridge"));
	Scenes[0]->StagedHash = Scenes[0]->ContentHash;
	Scenes[0]->Status = ECinemafySceneStatus::Recorded;
	for (FCinemafyScriptLine& Line : Scenes[0]->Lines)
	{
		if (Line.Kind == ECinemafyScriptLineKind::Dialogue)
		{
			Line.Audio = TSoftObjectPtr<USoundBase>(Recorded);
		}
	}
	TestFalse(TEXT("Staged from these pages: up to date"), Scenes[0]->IsOutOfDate());

	Project->BreakDown(Draft2);
	Scenes = Project->GetAllScenes();
	if (!TestEqual(TEXT("Three scenes after the revision"), Scenes.Num(), 3))
	{
		return false;
	}
	const FCinemafyScriptScene& Bridge = *Scenes[0];
	TestEqual(TEXT("The bridge was rewritten"), static_cast<int32>(Bridge.Revision), static_cast<int32>(ECinemafySceneRevision::Rewritten));
	TestTrue(TEXT("...so its staging is out of date"), Bridge.IsOutOfDate());
	TestEqual(TEXT("...and its production work is kept"), static_cast<int32>(Bridge.Status), static_cast<int32>(ECinemafySceneStatus::Recorded));
	const FCinemafyScriptLine* Kade = Bridge.Lines.FindByPredicate([](const FCinemafyScriptLine& L) { return L.Speaker == TEXT("KADE"); });
	TestTrue(TEXT("Kade's rewritten line needs re-recording"), Kade && Kade->bNeedsRerecord && Kade->PreviousText == TEXT("Stand down, Admiral."));
	const FCinemafyScriptLine* Thane = Bridge.Lines.FindByPredicate([](const FCinemafyScriptLine& L) { return L.Speaker == TEXT("THANE"); });
	TestTrue(TEXT("Thane's unchanged line keeps its recording"), Thane && !Thane->bNeedsRerecord && Thane->Audio.ToSoftObjectPath() == Recorded);
	TestEqual(TEXT("The hangar didn't change"), static_cast<int32>(Scenes[1]->Revision), static_cast<int32>(ECinemafySceneRevision::Unchanged));
	TestFalse(TEXT("...and was never staged, so it isn't out of date"), Scenes[1]->IsOutOfDate());
	TestEqual(TEXT("The brig is new"), static_cast<int32>(Scenes[2]->Revision), static_cast<int32>(ECinemafySceneRevision::New));
	TestTrue(TEXT("The report counts it all"), Project->RevisionReport.Contains(TEXT("1 rewritten, 1 new, 0 removed, 1 line(s) to re-record")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCinemafyGoldenSampleTest, "Cinemafy.Golden.StagesTheSampleScript", CinemafyTests::Flags)
bool FCinemafyGoldenSampleTest::RunTest(const FString& Parameters)
{
	// The sample the tutorial imports, end to end: breakdown, continuity, staging, delivery.
	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("Cinemafy"));
	FString Text;
	if (!Plugin.IsValid() || !FFileHelper::LoadFileToString(Text, *(Plugin->GetBaseDir() / TEXT("Docs/samples/TheBridge.fountain"))))
	{
		AddWarning(TEXT("Docs/samples/TheBridge.fountain isn't next to the plugin: golden test skipped."));
		return true;
	}
	FString Title, Report;
	const TArray<FCinemafyScriptAct> Acts = UCinemafyScriptLibrary::BreakDownScript(Text, true, 5, Title, Report);
	TestEqual(TEXT("The sample's title"), Title, FString(TEXT("The Bridge")));
	TArray<const FCinemafyScriptScene*> Scenes;
	for (const FCinemafyScriptAct& Act : Acts)
	{
		for (const FCinemafyScriptChapter& Chapter : Act.Chapters)
		{
			for (const FCinemafyScriptScene& Scene : Chapter.Scenes)
			{
				Scenes.Add(&Scene);
			}
		}
	}
	TestEqual(TEXT("Two acts"), Acts.Num(), 2);
	if (!TestEqual(TEXT("Five scenes"), Scenes.Num(), 5))
	{
		return true;
	}

	TArray<TArray<FCinemafyStagedBeat>> Staged;
	for (const FCinemafyScriptScene* Scene : Scenes)
	{
		TArray<FString> NotStaged;
		Staged.Add(UCinemafyPrevisLibrary::StageScene(*Scene, NotStaged));
		TestTrue(FString::Printf(TEXT("Scene %s stages something"), *Scene->Id), Staged.Last().Num() > 0);
	}
	auto Find = [](const TArray<FCinemafyStagedBeat>& Beats, const TCHAR* Who, TFunction<bool(const FCinemafyStagedBeat&)> Test)
	{
		return Beats.FindByPredicate([Who, &Test](const FCinemafyStagedBeat& B) { return B.Character == Who && Test(B); });
	};

	// The bridge: Kade off the lift, gun drawn; Thane answers without turning, then turns.
	const FCinemafyStagedBeat* Lift = Find(Staged[0], TEXT("KADE"), [](const FCinemafyStagedBeat& B) { return B.Prompt == TEXT("walk") && B.Place == TEXT("lift"); });
	TestNotNull(TEXT("Kade steps off the lift"), Lift);
	TestNotNull(TEXT("Kade draws her pistol"), Find(Staged[0], TEXT("KADE"), [](const FCinemafyStagedBeat& B) { return B.Prompt == TEXT("pistol"); }));
	const FCinemafyStagedBeat* MakeMe = Find(Staged[0], TEXT("ADMIRAL THANE"), [](const FCinemafyStagedBeat& B) { return B.Line == TEXT("Make me."); });
	TestTrue(TEXT("Thane's answer waits for Kade and keeps its direction"), MakeMe && MakeMe->WaitFor == TEXT("KADE") && MakeMe->LineDirection == TEXT("(not turning)"));

	// The hangar fight: the vault over the crate, and the kill aimed at Thane.
	const FCinemafyStagedBeat* Vault = Find(Staged[2], TEXT("KADE"), [](const FCinemafyStagedBeat& B) { return B.Prompt == TEXT("vault"); });
	TestTrue(TEXT("Kade vaults the cargo crate"), Vault && Vault->Place.Contains(TEXT("crate")));
	TestNotNull(TEXT("Kade's kill is aimed at Thane"), Find(Staged[2], TEXT("KADE"), [](const FCinemafyStagedBeat& B) { return B.Target == TEXT("ADMIRAL THANE") && B.Line.IsEmpty(); }));

	// Continuity: the pistol comes along; the med bay puts it away.
	TestTrue(TEXT("Kade carries her pistol into the hangar"), Scenes[2]->CarriedIn.ContainsByPredicate([](const FCinemafyContinuityFact& F) { return F.Character == TEXT("KADE") && F.Fact.Contains(TEXT("pistol")); }));
	TestNotNull(TEXT("She holsters it in the med bay"), Find(Staged[3], TEXT("KADE"), [](const FCinemafyStagedBeat& B) { return B.Prompt.Contains(TEXT("holster")); }));

	// Every line reads as a delivery without surprises, and the parenthetical one is still a calm read.
	for (const FCinemafyScriptScene* Scene : Scenes)
	{
		for (const FCinemafyScriptLine& Line : Scene->Lines)
		{
			if (Line.Kind == ECinemafyScriptLineKind::Dialogue)
			{
				const FCinemafyDelivery Delivery = UCinemafyScriptLibrary::ReadDelivery(Line.Direction, Line.Text);
				TestTrue(FString::Printf(TEXT("A sane read of \"%s\""), *Line.Text), Delivery.Rate > 0.f && Delivery.Volume > 0.f && Delivery.Intensity >= 0.f && Delivery.Intensity <= 1.f);
			}
		}
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

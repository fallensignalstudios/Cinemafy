// Run in the editor: Tools > Session Frontend > Automation, filter "Cinemafy".

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "CinemafyCameraLibrary.h"
#include "CinemafyChoreographyLibrary.h"
#include "GameFramework/Pawn.h"
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

	TestEqual(TEXT("A sword attack makes the target flinch"), Lib::ReadPrompt(TEXT("sword attack")).Reaction, ECinemafyReaction::Hit);
	TestEqual(TEXT("kill means death"), Lib::ReadPrompt(TEXT("kill")).Reaction, ECinemafyReaction::Death);
	TestEqual(TEXT("...and is still an attack"), Lib::ReadPrompt(TEXT("kill")).Kind, ECinemafyActionKind::Attack);
	TestEqual(TEXT("A heavy attack knocks down"), Lib::ReadPrompt(TEXT("heavy attack")).Reaction, ECinemafyReaction::Knockdown);
	TestTrue(TEXT("...and still looks for attacks (heavy only refines)"), Lib::ReadPrompt(TEXT("heavy attack")).Keywords[0].StartsWith(TEXT("attack")));
	TestEqual(TEXT("attack blocked is blocked"), Lib::ReadPrompt(TEXT("attack blocked")).Reaction, ECinemafyReaction::Block);
	TestEqual(TEXT("attack dodged is dodged"), Lib::ReadPrompt(TEXT("attack dodged")).Reaction, ECinemafyReaction::Dodge);
	TestEqual(TEXT("A reaction word doesn't search attack names"), Lib::ReadPrompt(TEXT("attack blocked")).Keywords.Num(), 1);

	TestEqual(TEXT("Straight ahead is the front"), Lib::ReactionDirection(10.f), FString(TEXT("front")));
	TestEqual(TEXT("Behind is the back"), Lib::ReactionDirection(-170.f), FString(TEXT("back")));
	TestEqual(TEXT("Positive yaw is the right"), Lib::ReactionDirection(90.f), FString(TEXT("right")));
	TestEqual(TEXT("Negative yaw is the left"), Lib::ReactionDirection(-90.f), FString(TEXT("left")));

	const FCinemafyPromptReading FromBehind = Lib::MakeReactionReading(ECinemafyReaction::Hit, 180.f);
	const float Back = Lib::ScoreAnimation(FromBehind, Lib::TokenizeName(TEXT("/Game/Combat/Hit/AM_HitReact_Back")));
	const float Front = Lib::ScoreAnimation(FromBehind, Lib::TokenizeName(TEXT("/Game/Combat/Hit/AM_HitReact_Front")));
	TestTrue(TEXT("A blow from behind prefers a back hit react"), Back > Front);
	TestTrue(TEXT("...but a front one still qualifies"), Front > 0.f);
	TestTrue(TEXT("An attack is never a reaction"), Lib::ScoreAnimation(FromBehind, Lib::TokenizeName(TEXT("/Game/Combat/AM_Sword_Attack_01"))) < 0.f);
	TestTrue(TEXT("A death reaction finds deaths"),
		Lib::ScoreAnimation(Lib::MakeReactionReading(ECinemafyReaction::Death, 0.f), Lib::TokenizeName(TEXT("/Game/Combat/Die/AM_Death_Front"))) > 0.f);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CinemafyPreflight.generated.h"

class UCinemafyScriptProject;

/** How much a pre-flight finding matters. */
UENUM(BlueprintType)
enum class ECinemafyCheckSeverity : uint8
{
	Info     UMETA(DisplayName = "Info"),
	Warning  UMETA(DisplayName = "Warning"),
	Blocker  UMETA(DisplayName = "Blocker")
};

/** What a pre-flight finding can do about itself. */
UENUM(BlueprintType)
enum class ECinemafyFix : uint8
{
	None                   UMETA(DisplayName = "None"),
	BuildCastAndSets       UMETA(DisplayName = "Build Cast & Sets"),
	OpenProject            UMETA(DisplayName = "Open the Script Project"),
	OpenLevel              UMETA(DisplayName = "Open the Set's Level"),
	PlaceMark              UMETA(DisplayName = "Place a Mark"),
	AddNavMesh             UMETA(DisplayName = "Add a Nav Mesh"),
	ScratchVoices          UMETA(DisplayName = "Scratch Voices"),
	UpdateChanged          UMETA(DisplayName = "Update Changed Scenes"),
	CastTemplateCharacter  UMETA(DisplayName = "Cast a Template Character")
};

/** One thing the pre-flight check found, and how to fix it. */
USTRUCT(BlueprintType)
struct CINEMAFYEDITOR_API FCinemafyCheck
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Check")
	FString SceneId;

	UPROPERTY(BlueprintReadOnly, Category = "Check")
	ECinemafyCheckSeverity Severity = ECinemafyCheckSeverity::Info;

	UPROPERTY(BlueprintReadOnly, Category = "Check")
	FString Problem;

	UPROPERTY(BlueprintReadOnly, Category = "Check")
	ECinemafyFix Fix = ECinemafyFix::None;

	/** What the fix works on (a mark's name, a level's package). */
	UPROPERTY(BlueprintReadOnly, Category = "Check")
	FString FixArg;

	/** The fix button's words. */
	UPROPERTY(BlueprintReadOnly, Category = "Check")
	FString FixLabel;
};

/**
 * The pre-flight check: everything a scene needs before Previs (cast on set or castable, marks for the places the
 * script names, a nav mesh to walk on, voices, the right level, the pages it was staged from), with a fix for most.
 */
UCLASS()
class CINEMAFYEDITOR_API UCinemafyPreflightLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** What one scene needs in the open level before it can be staged. */
	UFUNCTION(BlueprintCallable, Category = "Cinemafy|Preflight")
	static TArray<FCinemafyCheck> CheckScene(UCinemafyScriptProject* Project, const FString& SceneId);

	/** The whole project: cast, sets, voices, revisions, notes, and the scenes staged in the open level. */
	UFUNCTION(BlueprintCallable, Category = "Cinemafy|Preflight")
	static TArray<FCinemafyCheck> CheckProject(UCinemafyScriptProject* Project);

	/** Runs a finding's fix. True when something was done. */
	UFUNCTION(BlueprintCallable, Category = "Cinemafy|Preflight")
	static bool ApplyFix(UCinemafyScriptProject* Project, const FCinemafyCheck& Check);

	/** Runs every fix that needs no decision (everything but opening another level). Returns how many ran. */
	UFUNCTION(BlueprintCallable, Category = "Cinemafy|Preflight")
	static int32 FixAll(UCinemafyScriptProject* Project, const TArray<FCinemafyCheck>& Checks);

	/** The findings as text, worst first. */
	UFUNCTION(BlueprintPure, Category = "Cinemafy|Preflight")
	static FString FormatChecks(const TArray<FCinemafyCheck>& Checks);

	/**
	 * The pre-flight window: what's about to happen, what the check found (with Fix buttons), and Go / Cancel.
	 * SceneId empty checks the whole project. GoLabel empty shows only Close. Returns true to go ahead.
	 */
	static bool ShowPreflight(UCinemafyScriptProject* Project, const FString& SceneId, const FText& Title, const FText& WhatHappens, const FText& GoLabel);

	/** Previs from the UI: says what it will change, runs the pre-flight check, then stages (and records) and shows the report. */
	static void PrevisWithPreflight(UCinemafyScriptProject* Project, const FString& SceneId, bool bRecord);
};

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CinemafyTutorial.generated.h"

class UCinemafyScriptProject;

/**
 * The first-run tutorial: a step-by-step walk from a script to a cut scene, with do-it-for-me buttons. It opens
 * on a project's first editor session and stays off once it's finished or dismissed, in unattended / commandlet
 * runs, and as soon as an AI agent drives Cinemafy over MCP (the toolset reports every tool call).
 */
UCLASS()
class CINEMAFYEDITOR_API UCinemafyTutorialLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Opens the tutorial (from the Hub, or by hand), whatever the settings say. */
	UFUNCTION(BlueprintCallable, Category = "Cinemafy|Tutorial")
	static void ShowTutorial();

	/** An automated session (MCP tool calls): close the tutorial and never open it on its own again. */
	UFUNCTION(BlueprintCallable, Category = "Cinemafy|Tutorial")
	static void NoteAutomationSession();

	/** Marks the tutorial finished (or turns it back on for the next session). */
	UFUNCTION(BlueprintCallable, Category = "Cinemafy|Tutorial")
	static void SetTutorialDone(bool bDone);

	/** True when the tutorial should open on its own: first time, a person at the keyboard, no automation seen. */
	UFUNCTION(BlueprintPure, Category = "Cinemafy|Tutorial")
	static bool ShouldAutoShow();

	/** Imports the plugin's sample script (Docs/samples/TheBridge.fountain) and opens it on the Story Board. */
	UFUNCTION(BlueprintCallable, Category = "Cinemafy|Tutorial")
	static UCinemafyScriptProject* ImportSampleScript();

	/**
	 * Builds a little set for the sample in front of the viewport: a floor, a cargo crate, and Cinemafy Marks named
	 * the way the sample script talks (lift, console, corridor, start marks), and gives the cast a character class
	 * if one of the usual template characters exists. Returns what it built and what's still needed.
	 */
	UFUNCTION(BlueprintCallable, Category = "Cinemafy|Tutorial")
	static FString BuildSampleSet(UCinemafyScriptProject* Project);

	/** Called once the editor is up: opens the tutorial after a moment if it should. */
	static void ScheduleAutoShow();
	static void CloseTutorial();
};

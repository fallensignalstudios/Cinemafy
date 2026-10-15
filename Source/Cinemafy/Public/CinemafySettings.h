#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "CinemafyProduction.h"
#include "CinemafySettings.generated.h"

class UCinemafyMoveLibrary;
class UCinemafyPerformerProfile;
class ACharacter;
class USoundBase;
class UAnimSequenceBase;

/** Two clips made to play together: a couple lying cuddled, an embrace. The lead plays on the first person named. */
USTRUCT(BlueprintType)
struct CINEMAFY_API FCinemafyPosePair
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose")
	ECinemafyPose Pose = ECinemafyPose::Lie;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose")
	TSoftObjectPtr<UAnimSequenceBase> Lead;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose")
	TSoftObjectPtr<UAnimSequenceBase> Partner;
};

/** Project-wide Cinemafy settings (Project Settings > Plugins > Cinemafy). */
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "Cinemafy"))
class CINEMAFY_API UCinemafySettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/**
	 * The animation vocabulary the Choreographer and the Scene Director use when a choreography doesn't name its
	 * own Move Library. Empty = read the whole project by asset name.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Animation")
	TSoftObjectPtr<UCinemafyMoveLibrary> DefaultMoveLibrary;

	/**
	 * Animation folders to prefer when reading the project by name: a clip under one of these folders scores this
	 * much extra (the longest matching folder counts). Lets a pack win over look-alike clips, e.g. a deaths pack
	 * over the mannequin's stock deaths. Negative values push a folder back.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Animation")
	TMap<FString, float> FolderBias;

	/**
	 * Which performer profile each kind of character uses (the most specific class wins). A choreography's own
	 * Profile overrides this.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Performers")
	TMap<TSoftClassPtr<ACharacter>, TSoftObjectPtr<UCinemafyPerformerProfile>> PerformerProfiles;

	/**
	 * Temp score by mood (calm, tense, action, sad, warm, romantic, triumphant, eerie, alarm). Scenes staged from the
	 * script get the music for their mood when their director has none.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Story")
	TMap<FString, TSoftObjectPtr<USoundBase>> MoodMusic;

	/**
	 * Foley laid under recorded takes, by event: impact (a strike lands), takeoff, land, draw, holster, roll,
	 * body_fall (a death), footstep. Sounds you leave out are skipped.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Story")
	TMap<FString, TSoftObjectPtr<USoundBase>> FoleySounds;

	/**
	 * The clip a character holds for each pose the blocking asks for (a lying idle, a seated idle). Empty = Cinemafy
	 * looks for one by name ("lie", "lying", "sleep"; "sit", "seated"; "kneel"; "lean"). With none, people lying down
	 * are laid flat as a stand-in, and the rest stay on their feet (the report says which).
	 */
	UPROPERTY(config, EditAnywhere, Category = "Staging")
	TMap<ECinemafyPose, TSoftObjectPtr<UAnimSequenceBase>> PoseAnimations;

	/** Clips made for two people at once (cuddling on a bed, an embrace): used when blocking puts two people together. */
	UPROPERTY(config, EditAnywhere, Category = "Staging")
	TArray<FCinemafyPosePair> TogetherAnimations;

	/** The FolderBias for a package path ("/Game/Pack/Anim"): 0 when no folder matches. */
	static float GetFolderBias(const FString& PackagePath);

	virtual FName GetCategoryName() const override { return TEXT("Plugins"); }

	/** The default Move Library, loaded, or null. */
	static const UCinemafyMoveLibrary* GetDefaultMoveLibrary();
};

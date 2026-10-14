#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "CinemafySettings.generated.h"

class UCinemafyMoveLibrary;

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

	/** The FolderBias for a package path ("/Game/Pack/Anim"): 0 when no folder matches. */
	static float GetFolderBias(const FString& PackagePath);

	virtual FName GetCategoryName() const override { return TEXT("Plugins"); }

	/** The default Move Library, loaded, or null. */
	static const UCinemafyMoveLibrary* GetDefaultMoveLibrary();
};

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "UObject/Interface.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CinemafyProduction.generated.h"

class ACharacter;
class UStaticMesh;
class UMaterialInterface;
class USoundBase;
class UBillboardComponent;
class UTextRenderComponent;
class UCinemafyPerformerProfile;
struct FCinemafyContinuityFact;

/**
 * A named spot on a set: "console", "lift", "window". Previs reads "Kade walks to the console" and sends her
 * here. Place them with Tools > Cinemafy (or the tutorial), name them the way the script talks.
 */
UCLASS(meta = (DisplayName = "Cinemafy Mark"))
class CINEMAFY_API ACinemafyMark : public AActor
{
	GENERATED_BODY()

public:
	ACinemafyMark();

	/** What the script calls this place ("console", "the lift", "Thane's chair"). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mark")
	FString MarkName = TEXT("mark");

	/** Which set it belongs to (a script location like "FLAGSHIP BRIDGE"); empty = any set in this level. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mark")
	FString SetName;

	/** Where a character can start the scene ("start" marks are used first when previs places the cast). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mark")
	bool bStartMark = false;

	virtual void OnConstruction(const FTransform& Transform) override;

private:
#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TObjectPtr<UBillboardComponent> Sprite;

	UPROPERTY()
	TObjectPtr<UTextRenderComponent> Label;
#endif
};

/** How a continuity fact shows on a character: a prop in hand, a bloodied material, a tag your Blueprint reads. */
USTRUCT(BlueprintType)
struct CINEMAFY_API FCinemafyContinuityLook
{
	GENERATED_BODY()

	/** Injury, Carrying, Wearing, Dead... (empty = any). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look")
	FString Category;

	/** Matches facts containing this ("pistol", "wounded", "uniform"). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look")
	FString FactContains;

	/** A prop to attach (pistol, saber, bandage). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look")
	TSoftObjectPtr<UStaticMesh> AttachMesh;

	/** Socket or bone on the body mesh, e.g. hand_r. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look")
	FName Socket = TEXT("hand_r");

	/** A material to put on the body (blood, dirt, a different uniform). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look")
	TSoftObjectPtr<UMaterialInterface> Material;

	/** Which material slot (by index) gets it; -1 = every slot. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look")
	int32 MaterialSlot = -1;

	/** Added to the actor's tags while the fact holds (your Blueprint can read it). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look")
	FName ActorTag;

	/** Keep the performer to a walk (a limp, exhaustion). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look")
	bool bWalkOnly = false;
};

/** One character of the script, cast. */
USTRUCT(BlueprintType)
struct CINEMAFY_API FCinemafyCastMember
{
	GENERATED_BODY()

	/** As the script writes it ("ADMIRAL THANE"). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cast")
	FString ScriptName;

	/** The character already in the level (actor label). Previs uses it first. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cast")
	FString ActorLabel;

	/** Or the character to spawn when the scene's level has none (a MetaHuman / GASP Blueprint). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cast")
	TSoftClassPtr<ACharacter> CharacterClass;

	/** How to drive them (gait, traversal, overlays). Empty = Project Settings by class, else auto-detect. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cast")
	TSoftObjectPtr<UCinemafyPerformerProfile> Profile;

	/** What they hold when their first scene starts ("pistol", "sword"; empty = nothing). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cast")
	FString StartingWeapon;

	/** The player in a game: scenes bind the player's own pawn (gear and all) in their place. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cast")
	bool bIsPlayer = false;

	/** Scratch voice: a system voice name (macOS "Samantha", Windows "Microsoft David"...). Empty = default. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cast|Voice")
	FString ScratchVoice;

	/** Scratch speaking rate, 1 = normal. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cast|Voice", meta = (ClampMin = "0.5", ClampMax = "2.0"))
	float ScratchRate = 1.f;

	/** How continuity shows on them (props, materials, tags), matched against each scene's carry-ins. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cast", meta = (TitleProperty = "{Category} {FactContains}"))
	TArray<FCinemafyContinuityLook> Looks;
};

/** One location of the script, built. */
USTRUCT(BlueprintType)
struct CINEMAFY_API FCinemafySet
{
	GENERATED_BODY()

	/** As the script writes it ("FLAGSHIP BRIDGE"). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Set")
	FString ScriptLocation;

	/** The level (or sub-level) it's built in. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Set")
	TSoftObjectPtr<UWorld> Level;

	/** Room tone / ambience laid under its scenes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Set")
	TSoftObjectPtr<USoundBase> Ambience;

	/** Marks placed for it (filled when Cast & Sets is refreshed in its level). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Set")
	TArray<FString> Marks;
};

UINTERFACE(BlueprintType, MinimalAPI)
class UCinemafyContinuityReceiver : public UInterface
{
	GENERATED_BODY()
};

/** Implement on a character Blueprint to show continuity your own way (swap a costume, play a limp layer). */
class CINEMAFY_API ICinemafyContinuityReceiver
{
	GENERATED_BODY()

public:
	/** A fact that holds going into the scene: Category ("Injury"), Fact ("wounded"), since which scene. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Cinemafy|Continuity")
	void ApplyContinuity(const FString& Category, const FString& Fact, const FString& SinceScene);

	/** Back to a clean slate before a scene's facts are applied. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Cinemafy|Continuity")
	void ClearContinuity();
};

/** Casting, sets and continuity looks at runtime. */
UCLASS()
class CINEMAFY_API UCinemafyProductionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Removes what continuity added (props, materials, tags), then applies the facts through the cast member's looks. */
	static void ApplyContinuityToActor(AActor* Actor, const FCinemafyCastMember& Member, const TArray<FCinemafyContinuityFact>& Facts);

	/** Removes what ApplyContinuityToActor added. */
	UFUNCTION(BlueprintCallable, Category = "Cinemafy|Continuity")
	static void ClearContinuityFromActor(AActor* Actor);

	/**
	 * The mark a phrase means ("the console", "lift"), in a set if given: exact name, then a name containing it,
	 * then it containing the name. Null if none.
	 */
	UFUNCTION(BlueprintCallable, Category = "Cinemafy|Production", meta = (WorldContext = "WorldContextObject"))
	static ACinemafyMark* FindMark(const UObject* WorldContextObject, const FString& Phrase, const FString& SetName);

	/** Tag a continuity look adds for "walk only" (choreographies keep tagged performers to a walk). */
	static const FName WalkOnlyTag;
	/** Tag on everything continuity adds (props, the actor), so it can be cleared. */
	static const FName ContinuityTag;
};

#include "CinemafyChoreography.h"
#include "CinemafyModule.h"
#include "CinemafyPerformerProfile.h"
#include "CinemafyProduction.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "Animation/AnimSequenceBase.h"
#include "CinemafySubtitleSubsystem.h"
#include "Components/AudioComponent.h"
#include "Components/CapsuleComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "MotionWarpingComponent.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "UObject/UnrealType.h"
#include "EngineUtils.h"

FCinemafyChoreoEditorRequest ACinemafyChoreography::OnEditorRequest;
FCinemafyChoreoPerformanceEvent ACinemafyChoreography::OnPerformanceEvent;

namespace CinemafyRig
{
	/** The character, its components and its anim instance: everywhere gait / traversal / overlay hooks tend to live. */
	TArray<UObject*> GetRigObjects(ACharacter* Character)
	{
		TArray<UObject*> Objects;
		if (!Character)
		{
			return Objects;
		}
		Objects.Add(Character);
		for (UActorComponent* Component : Character->GetComponents())
		{
			Objects.Add(Component);
		}
		if (USkeletalMeshComponent* Mesh = Character->GetMesh())
		{
			if (UAnimInstance* Anim = Mesh->GetAnimInstance())
			{
				Objects.Add(Anim);
			}
		}
		return Objects;
	}

	int32 CountInputs(const UFunction* Function)
	{
		int32 Inputs = 0;
		for (TFieldIterator<FProperty> It(Function); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
		{
			if (!It->HasAnyPropertyFlags(CPF_ReturnParm) && (!It->HasAnyPropertyFlags(CPF_OutParm) || It->HasAnyPropertyFlags(CPF_ReferenceParm)))
			{
				++Inputs;
			}
		}
		return Inputs;
	}

	FProperty* FirstInput(const UFunction* Function)
	{
		for (TFieldIterator<FProperty> It(Function); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
		{
			if (!It->HasAnyPropertyFlags(CPF_ReturnParm) && (!It->HasAnyPropertyFlags(CPF_OutParm) || It->HasAnyPropertyFlags(CPF_ReferenceParm)))
			{
				return *It;
			}
		}
		return nullptr;
	}

	/** Finds a function on any rig object: by exact name if given, else by a name test on the lower-case name. */
	bool FindFunction(const TArray<UObject*>& Objects, FName Exact, TFunctionRef<bool(const FString&, const UFunction*)> Test,
		UObject*& OutObject, UFunction*& OutFunction)
	{
		for (UObject* Object : Objects)
		{
			if (!Object)
			{
				continue;
			}
			if (!Exact.IsNone())
			{
				if (UFunction* Function = Object->FindFunction(Exact))
				{
					OutObject = Object;
					OutFunction = Function;
					return true;
				}
				continue;
			}
			for (TFieldIterator<UFunction> It(Object->GetClass(), EFieldIteratorFlags::IncludeSuper); It; ++It)
			{
				if (Test(It->GetName().ToLower(), *It))
				{
					OutObject = Object;
					OutFunction = *It;
					return true;
				}
			}
		}
		return false;
	}

	/** Owns a zero-initialized parameter block for a UFunction. */
	struct FParams
	{
		UFunction* Function;
		uint8* Memory;

		explicit FParams(UFunction* InFunction) : Function(InFunction)
		{
			const int32 Size = FMath::Max<int32>(Function->ParmsSize, 1);
			Memory = static_cast<uint8*>(FMemory::Malloc(Size, Function->GetMinAlignment()));
			FMemory::Memzero(Memory, Size);
			for (TFieldIterator<FProperty> It(Function); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
			{
				It->InitializeValue_InContainer(Memory);
			}
		}

		~FParams()
		{
			for (TFieldIterator<FProperty> It(Function); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
			{
				It->DestroyValue_InContainer(Memory);
			}
			FMemory::Free(Memory);
		}
	};

	void CallWithBool(UObject* Object, UFunction* Function, bool bValue)
	{
		if (!Object || !Function)
		{
			return;
		}
		FParams Params(Function);
		if (FBoolProperty* Bool = CastField<FBoolProperty>(FirstInput(Function)))
		{
			Bool->SetPropertyValue_InContainer(Params.Memory, bValue);
		}
		Object->ProcessEvent(Function, Params.Memory);
	}

	/** Fills a struct input from a no-argument function on the same object that returns that struct (e.g. GetTraversalCheckInputs). */
	bool FillStructFromProvider(UObject* Object, FStructProperty* Input, void* Destination)
	{
		for (TFieldIterator<UFunction> It(Object->GetClass(), EFieldIteratorFlags::IncludeSuper); It; ++It)
		{
			UFunction* Provider = *It;
			if (CountInputs(Provider) != 0)
			{
				continue;
			}
			for (TFieldIterator<FProperty> P(Provider); P && P->HasAnyPropertyFlags(CPF_Parm); ++P)
			{
				FStructProperty* Output = CastField<FStructProperty>(*P);
				if (Output && Output->Struct == Input->Struct && P->HasAnyPropertyFlags(CPF_ReturnParm | CPF_OutParm))
				{
					FParams Params(Provider);
					Object->ProcessEvent(Provider, Params.Memory);
					Input->Struct->CopyScriptStruct(Destination, Output->ContainerPtrToValuePtr<void>(Params.Memory));
					return true;
				}
			}
		}
		return false;
	}

	/** Calls a function with defaulted (or provider-filled) inputs and reads its bool result. */
	bool CallAndReadSuccess(UObject* Object, UFunction* Function)
	{
		FParams Params(Function);
		for (TFieldIterator<FProperty> It(Function); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
		{
			const bool bInput = !It->HasAnyPropertyFlags(CPF_ReturnParm) && (!It->HasAnyPropertyFlags(CPF_OutParm) || It->HasAnyPropertyFlags(CPF_ReferenceParm));
			if (FStructProperty* Struct = CastField<FStructProperty>(*It); Struct && bInput)
			{
				FillStructFromProvider(Object, Struct, Struct->ContainerPtrToValuePtr<void>(Params.Memory));
			}
		}

		Object->ProcessEvent(Function, Params.Memory);

		bool bSucceeded = true;
		for (TFieldIterator<FProperty> It(Function); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
		{
			FBoolProperty* Bool = CastField<FBoolProperty>(*It);
			if (!Bool || !It->HasAnyPropertyFlags(CPF_ReturnParm | CPF_OutParm))
			{
				continue;
			}
			const bool bValue = Bool->GetPropertyValue_InContainer(Params.Memory);
			const FString Name = It->GetName().ToLower();
			// GASP returns "Traversal Check Failed?"; others return success.
			bSucceeded = Name.Contains(TEXT("fail")) ? !bValue : bValue;
		}
		return bSucceeded;
	}

	/** A bool property (top level or one struct deep, e.g. CharacterInputState.WantsToSprint) whose name contains Needle. */
	TFunction<void(bool)> FindBoolSetter(UObject* Object, const FString& Needle, FString* OutPath = nullptr)
	{
		for (TFieldIterator<FProperty> It(Object->GetClass()); It; ++It)
		{
			if (FBoolProperty* Bool = CastField<FBoolProperty>(*It); Bool && It->GetName().ToLower().Contains(Needle))
			{
				if (OutPath)
				{
					*OutPath = It->GetName();
				}
				TWeakObjectPtr<UObject> Weak = Object;
				return [Weak, Bool](bool bValue) { if (UObject* O = Weak.Get()) { Bool->SetPropertyValue_InContainer(O, bValue); } };
			}
			if (FStructProperty* Struct = CastField<FStructProperty>(*It))
			{
				for (TFieldIterator<FProperty> Inner(Struct->Struct); Inner; ++Inner)
				{
					if (FBoolProperty* Bool = CastField<FBoolProperty>(*Inner); Bool && Inner->GetName().ToLower().Contains(Needle))
					{
						if (OutPath)
						{
							// Blueprint struct members carry a "_N_GUID" suffix; the readable part is enough to find it again.
							FString Member = Inner->GetName();
							int32 Underscore = INDEX_NONE;
							if (Member.FindChar(TEXT('_'), Underscore))
							{
								Member.LeftInline(Underscore);
							}
							*OutPath = It->GetName() + TEXT(".") + Member;
						}
						TWeakObjectPtr<UObject> Weak = Object;
						return [Weak, Struct, Bool](bool bValue)
						{
							if (UObject* O = Weak.Get())
							{
								Bool->SetPropertyValue_InContainer(Struct->ContainerPtrToValuePtr<void>(O), bValue);
							}
						};
					}
				}
			}
		}
		return nullptr;
	}

	/** A property named Segment, or Segment plus a Blueprint "_N_GUID" suffix. */
	bool NameMatches(const FProperty* Property, const FString& Segment)
	{
		const FString Name = Property->GetName();
		return Name.Equals(Segment, ESearchCase::IgnoreCase) || Name.StartsWith(Segment + TEXT("_"), ESearchCase::IgnoreCase);
	}

	/** The bool at "Member" or "Struct.Member" on an object, as a setter; null if it isn't there. */
	TFunction<void(bool)> BoolSetterFromPath(UObject* Object, const FString& Path)
	{
		FString Outer, Inner;
		const bool bNested = Path.Split(TEXT("."), &Outer, &Inner);
		for (TFieldIterator<FProperty> It(Object->GetClass()); It; ++It)
		{
			if (!bNested)
			{
				if (FBoolProperty* Bool = CastField<FBoolProperty>(*It); Bool && NameMatches(Bool, Path))
				{
					TWeakObjectPtr<UObject> Weak = Object;
					return [Weak, Bool](bool bValue) { if (UObject* O = Weak.Get()) { Bool->SetPropertyValue_InContainer(O, bValue); } };
				}
				continue;
			}
			FStructProperty* Struct = CastField<FStructProperty>(*It);
			if (!Struct || !NameMatches(Struct, Outer))
			{
				continue;
			}
			for (TFieldIterator<FProperty> Member(Struct->Struct); Member; ++Member)
			{
				if (FBoolProperty* Bool = CastField<FBoolProperty>(*Member); Bool && NameMatches(Bool, Inner))
				{
					TWeakObjectPtr<UObject> Weak = Object;
					return [Weak, Struct, Bool](bool bValue)
					{
						if (UObject* O = Weak.Get())
						{
							Bool->SetPropertyValue_InContainer(Struct->ContainerPtrToValuePtr<void>(O), bValue);
						}
					};
				}
			}
		}
		return nullptr;
	}

	UEnum* GetEnum(const FProperty* Property)
	{
		if (const FEnumProperty* EnumProp = CastField<FEnumProperty>(Property))
		{
			return EnumProp->GetEnum();
		}
		if (const FByteProperty* ByteProp = CastField<FByteProperty>(Property))
		{
			return ByteProp->Enum;
		}
		return nullptr;
	}

	void SetEnumValue(const FProperty* Property, void* ValuePtr, int64 Value)
	{
		if (const FEnumProperty* EnumProp = CastField<FEnumProperty>(Property))
		{
			EnumProp->GetUnderlyingProperty()->SetIntPropertyValue(ValuePtr, Value);
		}
		else if (const FByteProperty* ByteProp = CastField<FByteProperty>(Property))
		{
			ByteProp->SetIntPropertyValue(ValuePtr, Value);
		}
	}

	/** The enum value that best fits a weapon word, matching display names (Blueprint enums) and code names. */
	bool BestEnumValue(const UEnum* Enum, const FString& Weapon, int64& OutValue, float& OutScore)
	{
		OutScore = 0.f;
		for (int32 Index = 0; Index < Enum->NumEnums() - 1; ++Index)
		{
			const float Score = FMath::Max(
				UCinemafyChoreographyLibrary::ScoreOverlayName(Weapon, Enum->GetDisplayNameTextByIndex(Index).ToString().Replace(TEXT(" "), TEXT(""))),
				UCinemafyChoreographyLibrary::ScoreOverlayName(Weapon, Enum->GetNameStringByIndex(Index)));
			if (Score > OutScore)
			{
				OutScore = Score;
				OutValue = Enum->GetValueByIndex(Index);
			}
		}
		return OutScore >= 3.5f;
	}
}

ACinemafyChoreography::ACinemafyChoreography()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void ACinemafyChoreography::Choreograph()       { OnEditorRequest.Broadcast(this, ECinemafyChoreoRequest::Choreograph); }
void ACinemafyChoreography::PerformAndRecord()  { OnEditorRequest.Broadcast(this, ECinemafyChoreoRequest::PerformAndRecord); }
void ACinemafyChoreography::DirectShots()       { OnEditorRequest.Broadcast(this, ECinemafyChoreoRequest::DirectShots); }
void ACinemafyChoreography::SaveWiringAsProfile() { OnEditorRequest.Broadcast(this, ECinemafyChoreoRequest::SaveProfile); }
void ACinemafyChoreography::CheckWiring()       { OnEditorRequest.Broadcast(this, ECinemafyChoreoRequest::CheckWiring); }

void ACinemafyChoreography::ReadPrompts()
{
	Modify();
	Candidates.Reset();
	UsedAnimations.Reset();
	for (int32 Index = 0; Index < Beats.Num(); ++Index)
	{
		ResolveBeat(Index);
	}
	RefreshLabels();
}

void ACinemafyChoreography::EnsureCandidates()
{
	const USkeleton* Skeleton = nullptr;
	if (Performer && Performer->GetMesh() && Performer->GetMesh()->GetSkeletalMeshAsset())
	{
		Skeleton = Performer->GetMesh()->GetSkeletalMeshAsset()->GetSkeleton();
	}

	TArray<FString> Folders;
	for (const FDirectoryPath& Folder : SearchFolders)
	{
		Folders.Add(Folder.Path);
	}
	const FString Key = GetPathNameSafe(Skeleton) + TEXT("|") + GetPathNameSafe(GetMoveLibrary()) + TEXT("|") + FString::Join(Folders, TEXT(","));
	if (Key == CandidatesSkeleton && Candidates.Num() > 0)
	{
		return;
	}
	CandidatesSkeleton = Key;
	Candidates = UCinemafyChoreographyLibrary::GatherCandidates(GetMoveLibrary(), Folders, Skeleton);
	UE_LOG(LogCinemafy, Log, TEXT("Cinemafy Choreography: %d animation(s) available for %s."), Candidates.Num(), *GetNameSafe(Performer));
}

void ACinemafyChoreography::ResolveBeat(int32 BeatIndex)
{
	if (!Beats.IsValidIndex(BeatIndex))
	{
		return;
	}

	FCinemafyChoreoBeat& Beat = Beats[BeatIndex];
	Beat.Reading = UCinemafyChoreographyLibrary::ReadPrompt(Beat.Prompt);
	Beat.ChosenAnimations.Reset();

	const ECinemafyActionKind Kind = Beat.Reading.Kind;
	Beat.TraversalCheck = Kind == ECinemafyActionKind::Traverse ? MeasureTraversal(BeatIndex) : FCinemafyTraversalCheck();

	if (Beat.AnimationOverride)
	{
		Beat.ChosenAnimations.Add(Beat.AnimationOverride);
		return;
	}

	if (Kind == ECinemafyActionKind::Traverse && !Beat.TraversalCheck.bPossible && Beat.TraversalCheck.Verdict != ECinemafyTraversalVerdict::NotChecked
		&& Beat.TraversalCheck.Verdict != ECinemafyTraversalVerdict::NoObstacle)
	{
		UE_LOG(LogCinemafy, Warning, TEXT("Cinemafy Choreography: beat %d \"%s\" won't work: %s"), BeatIndex + 1, *Beat.Prompt, *Beat.TraversalCheck.Reason);
	}
	if (Kind != ECinemafyActionKind::Attack && Kind != ECinemafyActionKind::Perform && Kind != ECinemafyActionKind::Equip
		&& Kind != ECinemafyActionKind::Traverse)
	{
		return;
	}

	EnsureCandidates();
	const TArray<int32> Picks = UCinemafyChoreographyLibrary::PickAnimations(Beat.Reading, Candidates, Beat.Reading.Repeat, UsedAnimations);
	for (int32 Pick : Picks)
	{
		if (UAnimSequenceBase* Anim = Cast<UAnimSequenceBase>(Candidates[Pick].Path.TryLoad()))
		{
			Beat.ChosenAnimations.Add(Anim);
			UsedAnimations.Add(Candidates[Pick].Path);
		}
	}

	if (Beat.ChosenAnimations.Num() == 0 && Kind != ECinemafyActionKind::Traverse && Kind != ECinemafyActionKind::Equip)
	{
		UE_LOG(LogCinemafy, Warning, TEXT("Cinemafy Choreography: nothing in the library fits \"%s\" for %s. Add a Move Library entry with that word, or set Animation Override."),
			*Beat.Prompt, *GetNameSafe(Performer));
	}
}

FCinemafyTraversalCheck ACinemafyChoreography::MeasureTraversal(int32 BeatIndex, const FVector* FromOverride)
{
	FCinemafyTraversalCheck Check;
	if (!Beats.IsValidIndex(BeatIndex) || !GetWorld())
	{
		return Check;
	}

	// Where the run-up starts: the last mark the performer walks to, else where it stands.
	FVector From = Performer ? Performer->GetActorLocation() : GetActorLocation();
	if (FromOverride)
	{
		From = *FromOverride;
	}
	else
	{
		for (int32 Index = BeatIndex - 1; Index >= 0; --Index)
		{
			const FCinemafyChoreoBeat& Previous = Beats[Index];
			const ECinemafyActionKind PreviousKind = Previous.Reading.Kind;
			if (PreviousKind != ECinemafyActionKind::Look && PreviousKind != ECinemafyActionKind::Wait)
			{
				// After a vault or climb the performer is where the route came out, not where the obstacle was clicked.
				const bool bRouted = PreviousKind == ECinemafyActionKind::Traverse && Previous.TraversalCheck.bPossible
					&& !Previous.TraversalCheck.ExitPoint.IsNearlyZero();
				From = bRouted ? Previous.TraversalCheck.ExitPoint : GetBeatWorldLocation(Index);
				break;
			}
		}
	}

	float Radius = 35.f;
	float HalfHeight = 90.f;
	TArray<AActor*> Ignore;
	Ignore.Add(this);
	if (Performer)
	{
		Ignore.Add(Performer);
		if (const UCapsuleComponent* Capsule = Performer->GetCapsuleComponent())
		{
			Radius = Capsule->GetScaledCapsuleRadius();
			HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
		}
	}
	for (const FCinemafyChoreoBeat& Other : Beats)
	{
		// Other performers aren't scenery.
		if (Cast<APawn>(Other.Target))
		{
			Ignore.AddUnique(Other.Target);
		}
	}

	return UCinemafyChoreographyLibrary::CheckTraversal(this, From, GetBeatWorldLocation(BeatIndex), GetTraversalEnvelope(), Radius, HalfHeight, Ignore);
}

int32 ACinemafyChoreography::AddBeat(const FString& Prompt, FVector WorldLocation, AActor* Target)
{
	Modify();
	FCinemafyChoreoBeat& Beat = Beats.AddDefaulted_GetRef();
	Beat.Prompt = Prompt;
	Beat.Location = GetActorTransform().InverseTransformPosition(WorldLocation);
	Beat.Target = Target;
	const int32 Index = Beats.Num() - 1;
	ResolveBeat(Index);
	RefreshLabels();
	return Index;
}

FVector ACinemafyChoreography::GetBeatWorldLocation(int32 BeatIndex) const
{
	return Beats.IsValidIndex(BeatIndex) ? GetActorTransform().TransformPosition(Beats[BeatIndex].Location) : GetActorLocation();
}

TArray<FCinemafyLine> ACinemafyChoreography::BuildDirectorLines() const
{
	return UCinemafyChoreographyLibrary::BuildDirectorLines(Beats, Performer, TakeDuration);
}

void ACinemafyChoreography::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	RefreshLabels();
}

#if WITH_EDITOR
void ACinemafyChoreography::PostEditChangeProperty(FPropertyChangedEvent& Event)
{
	Super::PostEditChangeProperty(Event);

	const FName Name = Event.GetMemberPropertyName();
	if (Name == GET_MEMBER_NAME_CHECKED(ACinemafyChoreography, Beats) || Name == GET_MEMBER_NAME_CHECKED(ACinemafyChoreography, Performer)
		|| Name == GET_MEMBER_NAME_CHECKED(ACinemafyChoreography, MoveLibrary) || Name == GET_MEMBER_NAME_CHECKED(ACinemafyChoreography, SearchFolders))
	{
		UsedAnimations.Reset();
		for (int32 Index = 0; Index < Beats.Num(); ++Index)
		{
			ResolveBeat(Index);
		}
	}
	RefreshLabels();
}
#endif

void ACinemafyChoreography::BeginPlay()
{
	Super::BeginPlay();
	if (bPerformOnBeginPlay || bPerformOnNextPlay)
	{
		Perform();
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// Wiring
// ---------------------------------------------------------------------------------------------------------------------

const UCinemafyPerformerProfile* ACinemafyChoreography::GetProfile() const
{
	return UCinemafyPerformerProfile::ResolveFor(Performer, Profile);
}

const UCinemafyMoveLibrary* ACinemafyChoreography::GetMoveLibrary() const
{
	if (MoveLibrary)
	{
		return MoveLibrary;
	}
	const UCinemafyPerformerProfile* P = GetProfile();
	return P ? P->MoveLibrary.LoadSynchronous() : nullptr;
}

TArray<FName> ACinemafyChoreography::GetSkipRecordingComponents() const
{
	TArray<FName> Skip = SkipRecordingComponents;
	if (const UCinemafyPerformerProfile* P = GetProfile())
	{
		for (const FName& Name : P->SkipRecordingComponents)
		{
			Skip.AddUnique(Name);
		}
	}
	return Skip;
}

FCinemafyTraversalEnvelope ACinemafyChoreography::GetTraversalEnvelope()
{
	if (const UCinemafyPerformerProfile* P = GetProfile())
	{
		return P->Traversal;
	}
	// No profile: assume GASP-like limits if a traversal function can be found, else jumping only.
	if (!Rig.Traverse && Performer)
	{
		BuildRig();
	}
	FCinemafyTraversalEnvelope Envelope;
	Envelope.bCanTraverse = static_cast<bool>(Rig.Traverse);
	return Envelope;
}

void ACinemafyChoreography::RefreshWiring()
{
	if (Performer)
	{
		BuildRig();
	}
}

void ACinemafyChoreography::BuildRig()
{
	Rig = FCinemafyPerformerRig();
	DetectedWiring = FCinemafyDetectedWiring();
	TArray<FString> Report;
	const TArray<UObject*> Objects = CinemafyRig::GetRigObjects(Performer);

	const UCinemafyPerformerProfile* P = GetProfile();
	const bool bGuess = !P || P->bAutoDetectMissing;
	Report.Add(P ? FString::Printf(TEXT("Profile %s%s"), *P->GetName(), P->bAutoDetectMissing ? TEXT(" (+ auto-detect)") : TEXT(""))
		: FString(TEXT("No profile: guessed by name (Save Wiring As Profile to keep it)")));

	// A choreography's own Wiring overrides beat the profile's.
	auto Pick = [](FName Override, FName FromProfile) { return !Override.IsNone() ? Override : FromProfile; };
	auto FindExact = [&Objects](FName Name, UObject*& OutObject, UFunction*& OutFunction)
	{
		return CinemafyRig::FindFunction(Objects, Name, [](const FString&, const UFunction*) { return false; }, OutObject, OutFunction);
	};

	// --- Gait ---
	auto GaitSetter = [&](FName Function, const FString& Property, const TCHAR* Needle, FName& OutFunction, FString& OutProperty, const TCHAR* Label)
		-> TFunction<void(bool)>
	{
		UObject* Object = nullptr;
		UFunction* Found = nullptr;
		if (!Function.IsNone())
		{
			if (FindExact(Function, Object, Found) && CinemafyRig::CountInputs(Found) == 1 && CastField<FBoolProperty>(CinemafyRig::FirstInput(Found)))
			{
				OutFunction = Function;
				TWeakObjectPtr<UObject> Weak = Object;
				return [Weak, Found](bool bValue) { CinemafyRig::CallWithBool(Weak.Get(), Found, bValue); };
			}
			Report.Add(FString::Printf(TEXT("%s: MISSING %s(bool)"), Label, *Function.ToString()));
			return nullptr;
		}
		if (!Property.IsEmpty())
		{
			for (UObject* Candidate : Objects)
			{
				if (TFunction<void(bool)> Setter = Candidate ? CinemafyRig::BoolSetterFromPath(Candidate, Property) : nullptr)
				{
					OutProperty = Property;
					return Setter;
				}
			}
			Report.Add(FString::Printf(TEXT("%s: MISSING property %s"), Label, *Property));
			return nullptr;
		}
		if (!bGuess)
		{
			return nullptr;
		}
		const FString Wanted = FString(TEXT("setwantsto")) + Needle;
		if (CinemafyRig::FindFunction(Objects, NAME_None, [&Wanted](const FString& Name, const UFunction* F)
			{
				return Name == Wanted && CinemafyRig::CountInputs(F) == 1 && CastField<FBoolProperty>(CinemafyRig::FirstInput(F));
			}, Object, Found))
		{
			OutFunction = Found->GetFName();
			TWeakObjectPtr<UObject> Weak = Object;
			return [Weak, Found](bool bValue) { CinemafyRig::CallWithBool(Weak.Get(), Found, bValue); };
		}
		for (UObject* Candidate : Objects)
		{
			if (TFunction<void(bool)> Setter = Candidate ? CinemafyRig::FindBoolSetter(Candidate, FString(TEXT("wantsto")) + Needle, &OutProperty) : nullptr)
			{
				return Setter;
			}
		}
		return nullptr;
	};

	Rig.SetSprint = GaitSetter(Pick(SprintFunction, P ? P->SprintFunction : NAME_None), P ? P->SprintProperty : FString(), TEXT("sprint"),
		DetectedWiring.SprintFunction, DetectedWiring.SprintProperty, TEXT("Sprint"));
	Rig.SetWalk = GaitSetter(Pick(WalkFunction, P ? P->WalkFunction : NAME_None), P ? P->WalkProperty : FString(), TEXT("walk"),
		DetectedWiring.WalkFunction, DetectedWiring.WalkProperty, TEXT("Walk"));
	Report.Add(FString::Printf(TEXT("Gait: %s"), Rig.SetSprint || Rig.SetWalk ? TEXT("intent flags") : TEXT("speed fallback")));

	// --- Traversal ---
	{
		const FName Wanted = Pick(TraversalFunction, P ? P->TraversalFunction : NAME_None);
		UObject* Object = nullptr;
		UFunction* Function = nullptr;
		bool bFound = false;
		if (!Wanted.IsNone())
		{
			bFound = FindExact(Wanted, Object, Function);
			if (!bFound)
			{
				Report.Add(FString::Printf(TEXT("Traversal: MISSING %s (vaults and climbs will fall back to a jump)"), *Wanted.ToString()));
			}
		}
		else if (bGuess)
		{
			bFound = CinemafyRig::FindFunction(Objects, NAME_None, [](const FString& Name, const UFunction*)
			{
				return Name.StartsWith(TEXT("try")) && Name.Contains(TEXT("traversal"));
			}, Object, Function);
		}
		if (bFound)
		{
			DetectedWiring.TraversalFunction = Function->GetFName();
			TWeakObjectPtr<UObject> Weak = Object;
			Rig.Traverse = [Weak, Function]() { UObject* O = Weak.Get(); return O && CinemafyRig::CallAndReadSuccess(O, Function); };
			Report.Add(FString::Printf(TEXT("Traversal: %s.%s"), *Object->GetClass()->GetName(), *Function->GetName()));
		}
		else if (Wanted.IsNone())
		{
			Report.Add(TEXT("Traversal: none (vaults and climbs fall back to a jump or a picked animation)"));
		}
	}

	// --- Overlay ---
	{
		struct FOverlayHook { TWeakObjectPtr<UObject> Object; UFunction* Function = nullptr; FProperty* Property = nullptr; };
		TArray<FOverlayHook> Hooks;
		const FName WantedFunction = Pick(OverlayFunction, P ? P->OverlayFunction : NAME_None);
		const FString WantedProperty = P ? P->OverlayProperty : FString();
		for (UObject* Object : Objects)
		{
			if (!Object)
			{
				continue;
			}
			for (TFieldIterator<UFunction> It(Object->GetClass(), EFieldIteratorFlags::IncludeSuper); It; ++It)
			{
				const FString Name = It->GetName().ToLower();
				const bool bNameMatches = !WantedFunction.IsNone() ? It->GetFName() == WantedFunction
					: (bGuess && WantedProperty.IsEmpty() && Name.StartsWith(TEXT("set")) && Name.Contains(TEXT("overlay")));
				if (bNameMatches && CinemafyRig::CountInputs(*It) == 1 && CinemafyRig::GetEnum(CinemafyRig::FirstInput(*It)))
				{
					Hooks.Add({ Object, *It, nullptr });
					DetectedWiring.OverlayFunction = It->GetFName();
				}
			}
			if (WantedFunction.IsNone())
			{
				for (TFieldIterator<FProperty> It(Object->GetClass()); It; ++It)
				{
					const bool bNameMatches = !WantedProperty.IsEmpty() ? CinemafyRig::NameMatches(*It, WantedProperty)
						: (bGuess && It->GetName().ToLower().Contains(TEXT("overlay")));
					if (bNameMatches && CinemafyRig::GetEnum(*It))
					{
						Hooks.Add({ Object, nullptr, *It });
						if (DetectedWiring.OverlayProperty.IsEmpty())
						{
							DetectedWiring.OverlayProperty = It->GetName();
						}
					}
				}
			}
		}

		if (Hooks.Num() > 0)
		{
			const TMap<FString, FString> Values = P ? P->OverlayValues : TMap<FString, FString>();
			Rig.SetOverlay = [Hooks, Values](const FString& Weapon) -> bool
			{
				int32 BestHook = INDEX_NONE;
				int64 BestValue = 0;
				float BestScore = 0.f;
				for (int32 Index = 0; Index < Hooks.Num(); ++Index)
				{
					const FProperty* Prop = Hooks[Index].Function ? CinemafyRig::FirstInput(Hooks[Index].Function) : Hooks[Index].Property;
					const UEnum* Enum = CinemafyRig::GetEnum(Prop);
					// The profile's explicit mapping first ("pistols" -> PistolTwoHanded), then the name match.
					if (const FString* Explicit = Values.Find(Weapon))
					{
						for (int32 E = 0; Enum && E < Enum->NumEnums() - 1; ++E)
						{
							if (Enum->GetNameStringByIndex(E).Equals(*Explicit, ESearchCase::IgnoreCase)
								|| Enum->GetDisplayNameTextByIndex(E).ToString().Replace(TEXT(" "), TEXT("")).Equals(Explicit->Replace(TEXT(" "), TEXT("")), ESearchCase::IgnoreCase))
							{
								BestHook = Index;
								BestValue = Enum->GetValueByIndex(E);
								BestScore = 100.f;
							}
						}
						continue;
					}
					int64 Value = 0;
					float Score = 0.f;
					if (CinemafyRig::BestEnumValue(Enum, Weapon, Value, Score) && Score > BestScore)
					{
						BestScore = Score;
						BestValue = Value;
						BestHook = Index;
					}
				}
				if (BestHook == INDEX_NONE)
				{
					return false;
				}
				const FOverlayHook& Hook = Hooks[BestHook];
				UObject* Object = Hook.Object.Get();
				if (!Object)
				{
					return false;
				}
				if (Hook.Function)
				{
					CinemafyRig::FParams Params(Hook.Function);
					FProperty* Input = CinemafyRig::FirstInput(Hook.Function);
					CinemafyRig::SetEnumValue(Input, Input->ContainerPtrToValuePtr<void>(Params.Memory), BestValue);
					Object->ProcessEvent(Hook.Function, Params.Memory);
				}
				else
				{
					CinemafyRig::SetEnumValue(Hook.Property, Hook.Property->ContainerPtrToValuePtr<void>(Object), BestValue);
				}
				return true;
			};
			Report.Add(FString::Printf(TEXT("Overlay: %d hook(s)"), Hooks.Num()));
		}
		else if (!WantedFunction.IsNone() || !WantedProperty.IsEmpty())
		{
			Report.Add(FString::Printf(TEXT("Overlay: MISSING %s"), !WantedFunction.IsNone() ? *WantedFunction.ToString() : *WantedProperty));
		}
		else
		{
			Report.Add(TEXT("Overlay: none"));
		}
	}

	// --- Crouch: the character's own functions, else ACharacter::Crouch ---
	{
		UObject* StartObject = nullptr; UFunction* Start = nullptr;
		UObject* StopObject = nullptr; UFunction* Stop = nullptr;
		const FName WantedStart = P ? P->CrouchFunction : NAME_None;
		const FName WantedStop = P ? P->UncrouchFunction : NAME_None;
		if (!WantedStart.IsNone() && !FindExact(WantedStart, StartObject, Start))
		{
			Report.Add(FString::Printf(TEXT("Crouch: MISSING %s (using ACharacter::Crouch)"), *WantedStart.ToString()));
		}
		else if (WantedStart.IsNone() && bGuess)
		{
			CinemafyRig::FindFunction(Objects, NAME_None, [](const FString& Name, const UFunction* F) { return Name == TEXT("startcrouch") && CinemafyRig::CountInputs(F) == 0; }, StartObject, Start);
		}
		if (!WantedStop.IsNone() && !FindExact(WantedStop, StopObject, Stop))
		{
			Report.Add(FString::Printf(TEXT("Uncrouch: MISSING %s (using ACharacter::UnCrouch)"), *WantedStop.ToString()));
		}
		else if (WantedStop.IsNone() && bGuess)
		{
			CinemafyRig::FindFunction(Objects, NAME_None, [](const FString& Name, const UFunction* F) { return Name == TEXT("stopcrouch") && CinemafyRig::CountInputs(F) == 0; }, StopObject, Stop);
		}
		DetectedWiring.CrouchFunction = Start ? Start->GetFName() : NAME_None;
		DetectedWiring.UncrouchFunction = Stop ? Stop->GetFName() : NAME_None;

		TWeakObjectPtr<ACharacter> Weak = Performer;
		TWeakObjectPtr<UObject> WeakStart = StartObject, WeakStop = StopObject;
		Rig.SetCrouch = [Weak, WeakStart, Start, WeakStop, Stop](bool bCrouch)
		{
			ACharacter* Character = Weak.Get();
			if (!Character)
			{
				return;
			}
			UObject* Object = bCrouch ? WeakStart.Get() : WeakStop.Get();
			UFunction* Function = bCrouch ? Start : Stop;
			if (Object && Function)
			{
				CinemafyRig::FParams Params(Function);
				Object->ProcessEvent(Function, Params.Memory);
			}
			else if (bCrouch)
			{
				Character->Crouch();
			}
			else
			{
				Character->UnCrouch();
			}
		};
	}

	EffectiveFallbackSpeeds = P ? P->FallbackSpeeds : FallbackSpeeds;
	// Wiring runs again with every beat: say so once, and again only when it changes.
	const FString NewReport = FString::Join(Report, TEXT("; "));
	if (NewReport != WiringReport)
	{
		WiringReport = NewReport;
		UE_LOG(LogCinemafy, Log, TEXT("Cinemafy Choreography wiring for %s: %s"), *GetNameSafe(Performer), *WiringReport);
	}
}

FString UCinemafyPerformerProfile::Validate(const ACharacter* Character) const
{
	if (!Character)
	{
		return TEXT("No character to check.");
	}
	const TArray<UObject*> Objects = CinemafyRig::GetRigObjects(const_cast<ACharacter*>(Character));
	TArray<FString> Lines;
	auto CheckFunction = [&](const TCHAR* Label, FName Name)
	{
		if (Name.IsNone())
		{
			return;
		}
		UObject* Object = nullptr;
		UFunction* Function = nullptr;
		const bool bFound = CinemafyRig::FindFunction(Objects, Name, [](const FString&, const UFunction*) { return false; }, Object, Function);
		Lines.Add(FString::Printf(TEXT("%s %s: %s"), Label, *Name.ToString(), bFound ? TEXT("ok") : TEXT("MISSING")));
	};
	auto CheckBool = [&](const TCHAR* Label, const FString& Path)
	{
		if (Path.IsEmpty())
		{
			return;
		}
		bool bFound = false;
		for (UObject* Object : Objects)
		{
			bFound |= Object && static_cast<bool>(CinemafyRig::BoolSetterFromPath(Object, Path));
		}
		Lines.Add(FString::Printf(TEXT("%s %s: %s"), Label, *Path, bFound ? TEXT("ok") : TEXT("MISSING")));
	};
	CheckFunction(TEXT("Sprint"), SprintFunction);
	CheckBool(TEXT("Sprint"), SprintProperty);
	CheckFunction(TEXT("Walk"), WalkFunction);
	CheckBool(TEXT("Walk"), WalkProperty);
	CheckFunction(TEXT("Crouch"), CrouchFunction);
	CheckFunction(TEXT("Uncrouch"), UncrouchFunction);
	CheckFunction(TEXT("Traversal"), TraversalFunction);
	CheckFunction(TEXT("Overlay"), OverlayFunction);
	if (!OverlayProperty.IsEmpty())
	{
		bool bFound = false;
		for (UObject* Object : Objects)
		{
			if (!Object)
			{
				continue;
			}
			for (TFieldIterator<FProperty> It(Object->GetClass()); It; ++It)
			{
				bFound |= CinemafyRig::NameMatches(*It, OverlayProperty) && CinemafyRig::GetEnum(*It) != nullptr;
			}
		}
		Lines.Add(FString::Printf(TEXT("Overlay %s: %s"), *OverlayProperty, bFound ? TEXT("ok") : TEXT("MISSING")));
	}
	if (Lines.Num() == 0)
	{
		Lines.Add(TEXT("Nothing named: speeds, Jump and ACharacter::Crouch only."));
	}
	return FString::Join(Lines, TEXT("\n"));
}

void ACinemafyChoreography::ApplyGait(ECinemafyGait Gait)
{
	if (!Performer)
	{
		return;
	}
	// Continuity: a limping or exhausted character (a "walk only" look) doesn't run.
	if ((Gait == ECinemafyGait::Run || Gait == ECinemafyGait::Sprint) && Performer->Tags.Contains(UCinemafyProductionLibrary::WalkOnlyTag))
	{
		Gait = ECinemafyGait::Walk;
	}

	const bool bWantCrouch = Gait == ECinemafyGait::Crouch;
	if (bWantCrouch != bCrouched && Rig.SetCrouch)
	{
		Rig.SetCrouch(bWantCrouch);
		bCrouched = bWantCrouch;
	}

	if (Rig.SetSprint)
	{
		Rig.SetSprint(Gait == ECinemafyGait::Sprint);
	}
	if (Rig.SetWalk)
	{
		Rig.SetWalk(Gait == ECinemafyGait::Walk || Gait == ECinemafyGait::Crouch);
	}
	if (!Rig.SetSprint && !Rig.SetWalk)
	{
		if (UCharacterMovementComponent* Movement = Performer->GetCharacterMovement())
		{
			Movement->MaxWalkSpeed = static_cast<float>(Gait == ECinemafyGait::Sprint ? EffectiveFallbackSpeeds.Z : Gait == ECinemafyGait::Run ? EffectiveFallbackSpeeds.Y : EffectiveFallbackSpeeds.X);
		}
	}
	CurrentGait = Gait;
}

bool ACinemafyChoreography::ApplyOverlay(const FString& Weapon)
{
	if (Weapon.IsEmpty() || !Rig.SetOverlay)
	{
		return false;
	}
	const bool bApplied = Rig.SetOverlay(Weapon);
	if (!bApplied)
	{
		UE_LOG(LogCinemafy, Log, TEXT("Cinemafy Choreography: no overlay state on %s fits \"%s\"."), *GetNameSafe(Performer), *Weapon);
	}
	return bApplied;
}

bool ACinemafyChoreography::TryTraverse()
{
	return Rig.Traverse && Rig.Traverse();
}

// ---------------------------------------------------------------------------------------------------------------------
// Performing
// ---------------------------------------------------------------------------------------------------------------------

void ACinemafyChoreography::Perform()
{
	UWorld* World = GetWorld();
	if (!World || !World->IsGameWorld())
	{
		UE_LOG(LogCinemafy, Warning, TEXT("Cinemafy Choreography: Perform runs in play (use Perform & Record in the editor)."));
		return;
	}
	if (!Performer || Beats.Num() == 0)
	{
		UE_LOG(LogCinemafy, Warning, TEXT("Cinemafy Choreography %s: needs a Performer and at least one beat."), *GetName());
		return;
	}

	// Beats resolved in the editor come across with their picks; resolve anything that wasn't.
	for (int32 Index = 0; Index < Beats.Num(); ++Index)
	{
		FCinemafyChoreoBeat& Beat = Beats[Index];
		if (Beat.Reading.Summary.IsEmpty())
		{
			ResolveBeat(Index);
		}
		if (!Beat.Id.IsValid())
		{
			Beat.Id = FGuid::NewGuid();
		}
		Beat.StartTime = -1.f;
		Beat.EndTime = -1.f;
		Beat.ImpactTimes.Reset();
		Beat.Events.Reset();
		Beat.LineStartTime = -1.f;
		Beat.LineDuration = 0.f;
		Beat.Outcome = ECinemafyBeatOutcome::NotRun;
		Beat.OutcomeReason.Reset();
	}
	PendingContacts.Reset();
	PendingReactions.Reset();
	ReactionMontages.Reset();
	LineBeat = INDEX_NONE;
	LineEndTime = -1.f;
	LineAudio.Reset();
	LastAddressed.Reset();
	bWaitingForCue = false;
	CurrentWeapon = StartingWeapon == TEXT("unarmed") ? FString() : StartingWeapon;
	RefreshTransitions();
	HeldPoses.Reset();

	// Characters we act on who have no choreography of their own in this take: we drive their reactions and they're
	// recorded with us.
	Partners.Reset();
	for (const FCinemafyChoreoBeat& Beat : Beats)
	{
		ACharacter* Target = Cast<ACharacter>(Beat.Target);
		if (!Target || Target == Performer || Beat.Reading.Kind != ECinemafyActionKind::Attack || !Beat.bTargetReacts)
		{
			continue;
		}
		bool bPerformsThemselves = false;
		for (TActorIterator<ACinemafyChoreography> It(World); It; ++It)
		{
			if (*It != this && It->Performer == Target && (It->bPerformOnNextPlay || It->bPerformOnBeginPlay || It->IsPerforming()))
			{
				bPerformsThemselves = true;
				break;
			}
		}
		if (!bPerformsThemselves)
		{
			Partners.AddUnique(Target);
		}
	}
	PartnerTransforms.Reset();
	PartnerTransforms.SetNum(Partners.Num());

	// Remember what we change so it can all be put back.
	UCharacterMovementComponent* Movement = Performer->GetCharacterMovement();
	SavedMaxWalkSpeed = Movement ? Movement->MaxWalkSpeed : -1.f;
	bStartedCrouched = Performer->bIsCrouched;
	SavedPlayer.Reset();
	SpawnedController.Reset();

	// The performer needs a controller for movement input to land; a player stops driving it for the take.
	if (APlayerController* PC = Cast<APlayerController>(Performer->GetController()))
	{
		SavedPlayer = PC;
		PC->UnPossess();
		PC->SetViewTarget(Performer);
	}
	if (!Performer->GetController())
	{
		Performer->SpawnDefaultController();
		SpawnedController = Performer->GetController();
	}

	BuildRig();
	bCrouched = bStartedCrouched;
	ApplyGait(ECinemafyGait::Walk);

	bPerforming = true;
	PerformTime = 0.f;
	LeadInLeft = LeadInSeconds;
	TailLeft = -1.f;
	CurrentBeat = 0;
	Phase = EPhase::Start;
	RecordedTimes.Reset();
	RecordedTransforms.Reset();
	TakeDuration = 0.f;

	OnPerformanceEvent.Broadcast(this, true);
}

void ACinemafyChoreography::StopPerforming()
{
	if (bPerforming)
	{
		// Anything still running is cut off: say so rather than leave it looking unrun.
		if (Beats.IsValidIndex(CurrentBeat) && Phase != EPhase::Start)
		{
			FinishBeat(ECinemafyBeatOutcome::TimedOut, TEXT("Performance stopped before this beat finished"));
		}
		FinishPerformance();
	}
}

void ACinemafyChoreography::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// When the whole world is going away, don't hand pawns between controllers that are being torn down.
	bWorldEnding = EndPlayReason == EEndPlayReason::EndPlayInEditor || EndPlayReason == EEndPlayReason::Quit
		|| EndPlayReason == EEndPlayReason::LevelTransition;
	StopPerforming();
	Super::EndPlay(EndPlayReason);
}

void ACinemafyChoreography::FinishPerformance()
{
	bPerforming = false;
	TakeDuration = PerformTime;
	PendingContacts.Reset();
	PendingReactions.Reset();

	// Cut points for the director: what was seen to take off, and strikes that actually landed.
	for (FCinemafyChoreoBeat& Beat : Beats)
	{
		Beat.ImpactTimes.Reset();
		for (const FCinemafyPerformanceEvent& Event : Beat.Events)
		{
			if (Event.Kind == ECinemafyPerformanceEventKind::Contact || Event.Kind == ECinemafyPerformanceEventKind::Takeoff)
			{
				Beat.ImpactTimes.Add(Event.Time);
			}
		}
		Beat.ImpactTimes.Sort();
	}
	RestorePerformer();
	OnPerformanceEvent.Broadcast(this, false);
	OnPerformanceFinished.Broadcast();
}

void ACinemafyChoreography::RestorePerformer()
{
	if (!Performer)
	{
		return;
	}
	if (Rig.SetSprint)
	{
		Rig.SetSprint(false);
	}
	if (Rig.SetWalk)
	{
		Rig.SetWalk(false);
	}
	if (bCrouched != bStartedCrouched && Rig.SetCrouch)
	{
		Rig.SetCrouch(bStartedCrouched);
		bCrouched = bStartedCrouched;
	}
	if (UCharacterMovementComponent* Movement = Performer->GetCharacterMovement(); Movement && SavedMaxWalkSpeed >= 0.f)
	{
		Movement->MaxWalkSpeed = SavedMaxWalkSpeed;
	}
	if (UMotionWarpingComponent* Warping = Performer->FindComponentByClass<UMotionWarpingComponent>())
	{
		for (const TCHAR* Name : { TEXT("AttackTarget"), TEXT("Target"), TEXT("Warp"), TEXT("MeleeTarget") })
		{
			Warping->RemoveWarpTarget(Name);
		}
	}
	// Hand the character back to whoever had it.
	APlayerController* PC = SavedPlayer.Get();
	if (PC && !bWorldEnding)
	{
		if (AController* Spawned = SpawnedController.Get())
		{
			Spawned->UnPossess();
			Spawned->Destroy();
		}
		PC->Possess(Performer);
	}
	SavedPlayer.Reset();
	SpawnedController.Reset();
}

void ACinemafyChoreography::AddEvent(ECinemafyPerformanceEventKind Kind, bool bEstimated, const FString& Note, float AtTime)
{
	if (!Beats.IsValidIndex(CurrentBeat) || !Performer)
	{
		return;
	}
	FCinemafyPerformanceEvent& Event = Beats[CurrentBeat].Events.AddDefaulted_GetRef();
	Event.Kind = Kind;
	Event.Time = AtTime >= 0.f ? AtTime : PerformTime;
	Event.Location = Performer->GetActorLocation();
	Event.bEstimated = bEstimated;
	Event.Note = Note;
}

void ACinemafyChoreography::UpdatePendingContacts()
{
	USkeletalMeshComponent* Mesh = Performer ? Performer->GetMesh() : nullptr;
	UAnimInstance* Anim = Mesh ? Mesh->GetAnimInstance() : nullptr;
	for (int32 Index = PendingContacts.Num() - 1; Index >= 0; --Index)
	{
		const FCinemafyPendingContact& Pending = PendingContacts[Index];
		if (PerformTime < Pending.Time)
		{
			continue;
		}
		UAnimMontage* Montage = Pending.Montage.Get();
		if (Montage && Anim && Anim->Montage_IsPlaying(Montage) && Beats.IsValidIndex(Pending.BeatIndex))
		{
			FCinemafyPerformanceEvent& Event = Beats[Pending.BeatIndex].Events.AddDefaulted_GetRef();
			Event.Kind = ECinemafyPerformanceEventKind::Contact;
			Event.Time = Pending.Time;
			Event.Location = Performer->GetActorLocation();
			Event.bEstimated = Pending.bEstimated;
			Event.Note = Pending.Note;
		}
		// Otherwise the swing was interrupted before it landed: no contact.
		PendingContacts.RemoveAt(Index);
	}
}

void ACinemafyChoreography::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!bPerforming)
	{
		UWorld* World = GetWorld();
		if (bShowPath && World && !World->IsGameWorld())
		{
			DrawPath();
		}
		return;
	}

	if (!Performer)
	{
		FinishPerformance();
		return;
	}

	PerformTime += DeltaSeconds;
	RecordedTimes.Add(PerformTime);
	RecordedTransforms.Add(Performer->GetActorTransform());
	for (int32 Index = 0; Index < Partners.Num(); ++Index)
	{
		TArray<FTransform>& Samples = PartnerTransforms[Index];
		Samples.Add(Partners[Index] ? Partners[Index]->GetActorTransform() : (Samples.Num() > 0 ? Samples.Last() : FTransform::Identity));
	}
	UpdatePendingContacts();
	UpdateReactions(DeltaSeconds);
	UpdateLine();

	if (LeadInLeft > 0.f)
	{
		LeadInLeft -= DeltaSeconds;
		return;
	}

	if (CurrentBeat >= Beats.Num())
	{
		if (TailLeft < 0.f)
		{
			TailLeft = TailSeconds;
		}
		TailLeft -= DeltaSeconds;
		if (TailLeft <= 0.f && !IsAnimationBusy() && PendingContacts.Num() == 0 && PendingReactions.Num() == 0 && LineEndTime < 0.f)
		{
			FinishPerformance();
		}
		return;
	}

	TickBeat(DeltaSeconds);
}

void ACinemafyChoreography::RefreshTransitions()
{
	UCinemafyChoreographyLibrary::PlanTransitions(Beats, StartingWeapon, GetTraversalEnvelope());
}

UAnimSequenceBase* ACinemafyChoreography::PickTransitionClip(const FCinemafyPromptReading& Reading)
{
	EnsureCandidates();
	const TArray<int32> Picks = UCinemafyChoreographyLibrary::PickAnimations(Reading, Candidates, 1, UsedAnimations);
	return Picks.Num() > 0 ? Cast<UAnimSequenceBase>(Candidates[Picks[0]].Path.TryLoad()) : nullptr;
}

float ACinemafyChoreography::GetLineSeconds(const FCinemafyChoreoBeat& Beat) const
{
	if (Beat.DialogueAudio)
	{
		const float AudioSeconds = Beat.DialogueAudio->GetDuration();
		if (AudioSeconds > 0.f && AudioSeconds < INDEFINITELY_LOOPING_DURATION)
		{
			return AudioSeconds;
		}
	}
	TArray<FString> Words;
	Beat.GetLineText().ToString().ParseIntoArrayWS(Words);
	// Words at the speaking pace, plus a beat for the breath and the punctuation.
	return FMath::Max(1.f, Words.Num() / FMath::Max(WordsPerSecond, 0.5f) + 0.4f);
}

AActor* ACinemafyChoreography::GetLineListener(int32 BeatIndex) const
{
	if (!Beats.IsValidIndex(BeatIndex))
	{
		return nullptr;
	}
	const FCinemafyChoreoBeat& Beat = Beats[BeatIndex];
	if (Beat.Listener && Beat.Listener != Performer)
	{
		return Beat.Listener;
	}
	if (Cast<APawn>(Beat.Target) && Beat.Target != Performer)
	{
		return Beat.Target;
	}
	if (AActor* Last = LastAddressed.Get())
	{
		return Last;
	}
	// Before the take (or with nobody addressed yet): the nearest earlier beat that names someone.
	for (int32 Index = BeatIndex - 1; Index >= 0; --Index)
	{
		const FCinemafyChoreoBeat& Earlier = Beats[Index];
		if (Earlier.Listener && Earlier.Listener != Performer)
		{
			return Earlier.Listener;
		}
		if (Cast<APawn>(Earlier.Target) && Earlier.Target != Performer)
		{
			return Earlier.Target;
		}
	}
	return nullptr;
}

AActor* ACinemafyChoreography::GetCueActor(int32 BeatIndex) const
{
	if (!Beats.IsValidIndex(BeatIndex))
	{
		return nullptr;
	}
	const FCinemafyChoreoBeat& Beat = Beats[BeatIndex];
	if (Beat.WaitForLineFrom && Beat.WaitForLineFrom != Performer)
	{
		return Beat.WaitForLineFrom;
	}
	return Beat.Reading.bAfterLine ? GetLineListener(BeatIndex) : nullptr;
}

namespace CinemafyLines
{
	/** When each character's latest line ends (world seconds). Shared by every choreography in the take. */
	TMap<TWeakObjectPtr<const AActor>, double>& SpokenLineEnds()
	{
		static TMap<TWeakObjectPtr<const AActor>, double> Ends;
		return Ends;
	}

	/** How many lines each character has started in the take. */
	TMap<TWeakObjectPtr<const AActor>, int32>& SpokenLineCounts()
	{
		static TMap<TWeakObjectPtr<const AActor>, int32> Counts;
		return Counts;
	}
}

bool ACinemafyChoreography::IsCueReady() const
{
	const AActor* Cue = GetCueActor(CurrentBeat);
	const UWorld* World = GetWorld();
	if (!Cue || !World)
	{
		return true;
	}
	const double* End = CinemafyLines::SpokenLineEnds().Find(TWeakObjectPtr<const AActor>(Cue));
	const int32 Number = Beats.IsValidIndex(CurrentBeat) ? Beats[CurrentBeat].WaitForLineNumber : 0;
	if (Number > 0)
	{
		// A specific line: done once they've finished that many lines (however long ago that was).
		const int32 Started = CinemafyLines::SpokenLineCounts().FindRef(TWeakObjectPtr<const AActor>(Cue));
		const int32 Finished = Started - (End && World->GetTimeSeconds() < *End ? 1 : 0);
		return Finished >= Number;
	}
	// A line that was still running when we started waiting counts: wait for its last word.
	return End && *End > CueAfterWorldTime - 0.25 && World->GetTimeSeconds() >= *End;
}

void ACinemafyChoreography::StartLine()
{
	if (!Beats.IsValidIndex(CurrentBeat) || !Performer)
	{
		return;
	}
	// A new line cuts off whatever this performer was still saying.
	if (LineEndTime >= 0.f)
	{
		if (UAudioComponent* Previous = LineAudio.Get())
		{
			Previous->Stop();
		}
		LineEndTime = PerformTime;
		UpdateLine();
	}

	FCinemafyChoreoBeat& Beat = Beats[CurrentBeat];
	const float Seconds = GetLineSeconds(Beat);
	const FText Text = Beat.GetLineText();
	Beat.LineStartTime = PerformTime;
	Beat.LineDuration = Seconds;
	LineBeat = CurrentBeat;
	LineEndTime = PerformTime + Seconds;
	if (AActor* Listener = GetLineListener(CurrentBeat))
	{
		LastAddressed = Listener;
	}

	// A heard line (an announcement, a radio) comes from nowhere in particular and carries the voice's name.
	const bool bHeard = !Beat.HeardFrom.IsEmpty();
	if (Beat.DialogueAudio)
	{
		LineAudio = bHeard ? UGameplayStatics::SpawnSound2D(this, Beat.DialogueAudio) : UGameplayStatics::SpawnSoundAttached(Beat.DialogueAudio, Performer->GetRootComponent());
	}
	UWorld* World = GetWorld();
	if (bShowSubtitlesWhilePerforming && !Text.IsEmpty() && World)
	{
		if (UCinemafySubtitleSubsystem* Subtitles = World->GetSubsystem<UCinemafySubtitleSubsystem>())
		{
			Subtitles->ShowSubtitle(FText::FromString(bHeard ? Beat.HeardFrom : Performer->GetActorNameOrLabel()), Text, Seconds);
		}
	}
	if (World)
	{
		CinemafyLines::SpokenLineEnds().Add(TWeakObjectPtr<const AActor>(Performer.Get()), World->GetTimeSeconds() + Seconds);
		CinemafyLines::SpokenLineCounts().FindOrAdd(TWeakObjectPtr<const AActor>(Performer.Get())) += 1;
	}
	AddEvent(ECinemafyPerformanceEventKind::LineStart, Beat.DialogueAudio == nullptr, Text.ToString());
}

void ACinemafyChoreography::UpdateLine()
{
	if (LineEndTime < 0.f || PerformTime < LineEndTime)
	{
		return;
	}
	if (Beats.IsValidIndex(LineBeat) && Performer)
	{
		// Logged on the beat that spoke it, even if the performer has moved on.
		FCinemafyPerformanceEvent& Event = Beats[LineBeat].Events.AddDefaulted_GetRef();
		Event.Kind = ECinemafyPerformanceEventKind::LineEnd;
		Event.Time = PerformTime;
		Event.Location = Performer->GetActorLocation();
		Event.bEstimated = Beats[LineBeat].DialogueAudio == nullptr;
		Beats[LineBeat].LineDuration = PerformTime - Beats[LineBeat].LineStartTime;
	}
	LineAudio.Reset();
	LineBeat = INDEX_NONE;
	LineEndTime = -1.f;
}

FVector ACinemafyChoreography::GetTargetPoint(const FCinemafyChoreoBeat& Beat, int32 BeatIndex) const
{
	if (Beat.Target && (Beat.Reading.Kind == ECinemafyActionKind::Attack || Beat.Reading.Kind == ECinemafyActionKind::Look
		|| Beat.Reading.Kind == ECinemafyActionKind::Perform) && Beat.Target->IsA<APawn>())
	{
		return Beat.Target->GetActorLocation();
	}
	return GetBeatWorldLocation(BeatIndex);
}

void ACinemafyChoreography::StartBeat()
{
	FCinemafyChoreoBeat& Beat = Beats[CurrentBeat];
	const FCinemafyPromptReading& Reading = Beat.Reading;
	Beat.StartTime = PerformTime;
	BeatTime = 0.f;
	PhaseTime = 0.f;
	bTraversalStarted = false;
	bSawTraversal = false;
	TraverseRetry = 0.f;
	ActDoneDelay = 0.f;
	ActiveMontage.Reset();
	AnimQueue = Beat.ChosenAnimations;
	StuckCheckLocation = Performer->GetActorLocation();
	StuckTimer = 0.f;
	ShortfallReason.Reset();
	FallbackReason.Reset();
	bBraked = false;
	bRecovering = false;
	DrawPendingWeapon.Reset();
	TransitionClip = nullptr;
	CurrentArrival = UCinemafyChoreographyLibrary::ResolveArrival(Beats, CurrentBeat);
	AddEvent(ECinemafyPerformanceEventKind::BeatStart, false, Beat.Prompt);

	// Dialogue: wait for the other character's line if asked, then speak (the line runs alongside the beat).
	bWaitingForCue = GetCueActor(CurrentBeat) != nullptr;
	CueAfterWorldTime = GetWorld()->GetTimeSeconds();
	CueWaitTime = 0.f;
	if (!bWaitingForCue && Beat.HasLine())
	{
		StartLine();
	}

	const FVector Here = Performer->GetActorLocation();
	const FVector Point = GetBeatWorldLocation(CurrentBeat);
	const FVector TargetPoint = GetTargetPoint(Beat, CurrentBeat);
	const float Distance = FVector::Dist2D(Here, TargetPoint);

	// Gait: what was asked for; otherwise keep moving the way we were (attacks close long gaps at a run).
	if (Reading.bHasGait)
	{
		ApplyGait(Reading.Gait);
	}
	else if (Reading.Kind == ECinemafyActionKind::Attack && !Reading.bRanged && Distance > 600.f && CurrentGait == ECinemafyGait::Walk)
	{
		ApplyGait(ECinemafyGait::Run);
	}

	const FString& DrawWeapon = Beat.Transitions.DrawWeapon;
	if (!DrawWeapon.IsEmpty() && DrawWeapon != CurrentWeapon && Beat.ChosenAnimations.Num() > 0)
	{
		// The strike needs a weapon that isn't in hand. A long way out, it comes out on the move (overlay only);
		// close in, it's a proper draw right before the strike.
		if (Distance > 600.f)
		{
			if (ApplyOverlay(DrawWeapon))
			{
				CurrentWeapon = DrawWeapon;
				AddEvent(ECinemafyPerformanceEventKind::Transition, false, FString::Printf(TEXT("draws the %s on the move"), *DrawWeapon));
			}
			else
			{
				FallbackReason = FString::Printf(TEXT("No overlay state fits \"%s\"; performed in the current stance"), *DrawWeapon);
			}
		}
		else
		{
			DrawPendingWeapon = DrawWeapon;
		}
	}
	else if (!Reading.Weapon.IsEmpty())
	{
		if (ApplyOverlay(Reading.Weapon))
		{
			CurrentWeapon = Reading.Weapon == TEXT("unarmed") ? FString() : Reading.Weapon;
		}
		else if (Reading.Kind != ECinemafyActionKind::Equip)
		{
			FallbackReason = FString::Printf(TEXT("No overlay state fits \"%s\"; performed in the current stance"), *Reading.Weapon);
		}
	}
	if (Reading.Kind == ECinemafyActionKind::Equip && Reading.Keywords.ContainsByPredicate([](const FString& Word) { return Word.StartsWith(TEXT("holster")); }))
	{
		CurrentWeapon.Reset();
	}

	// Route.
	PathPoints.Reset();
	PathIndex = 0;
	if (Reading.Kind == ECinemafyActionKind::Move && bUseNavMesh)
	{
		if (UNavigationPath* Path = UNavigationSystemV1::FindPathToLocationSynchronously(this, Here, Point))
		{
			if (Path->IsValid() && Path->PathPoints.Num() > 1 && Path->GetPathLength() < FVector::Dist(Here, Point) * 3.f)
			{
				for (int32 Index = 1; Index < Path->PathPoints.Num(); ++Index)
				{
					PathPoints.Add(Path->PathPoints[Index]);
				}
			}
		}
	}
	if (PathPoints.Num() == 0)
	{
		PathPoints.Add(Point);
	}

	// Measure the obstacle again from where the performer actually is: never fake a vault the character can't do.
	bStepOverOnly = false;
	PrecheckFailReason.Reset();
	if (Reading.Kind == ECinemafyActionKind::Traverse)
	{
		Beat.TraversalCheck = MeasureTraversal(CurrentBeat, &Here);
		RefreshTransitions(); // The brake and the landing depend on what was just measured.
		const ECinemafyTraversalVerdict Verdict = Beat.TraversalCheck.Verdict;
		if (Verdict == ECinemafyTraversalVerdict::StepOver)
		{
			bStepOverOnly = true;
		}
		else if (!Beat.TraversalCheck.bPossible && Verdict != ECinemafyTraversalVerdict::NotChecked && Verdict != ECinemafyTraversalVerdict::NoObstacle)
		{
			PrecheckFailReason = TEXT("Didn't try: ") + Beat.TraversalCheck.Reason;
		}
		else if (Beat.TraversalCheck.bPossible && !Beat.TraversalCheck.TakeoffPoint.IsNearlyZero())
		{
			// The planned route: get to the run-up start (around things, on the navmesh), then go straight at it.
			PathPoints.Reset();
			PathIndex = 0;
			const FVector RunUpStart = Beat.TraversalCheck.ApproachPoint;
			if (FVector::Dist2D(Here, RunUpStart) > 60.f)
			{
				if (bUseNavMesh)
				{
					if (UNavigationPath* Path = UNavigationSystemV1::FindPathToLocationSynchronously(this, Here, RunUpStart))
					{
						if (Path->IsValid() && Path->PathPoints.Num() > 2 && Path->GetPathLength() < FVector::Dist(Here, RunUpStart) * 3.f)
						{
							for (int32 Index = 1; Index < Path->PathPoints.Num() - 1; ++Index)
							{
								PathPoints.Add(Path->PathPoints[Index]);
							}
						}
					}
				}
				PathPoints.Add(RunUpStart);
			}
			// Vaults and gaps want speed: walking into one is a mantle, or a short jump.
			const bool bWantsSpeed = Verdict == ECinemafyTraversalVerdict::Vault || Verdict == ECinemafyTraversalVerdict::Gap;
			if (bWantsSpeed && !Reading.bHasGait && CurrentGait == ECinemafyGait::Walk)
			{
				ApplyGait(ECinemafyGait::Run);
			}
		}
	}

	bool bApproach = false;
	switch (Reading.Kind)
	{
	case ECinemafyActionKind::Move:
	case ECinemafyActionKind::Traverse:
		bApproach = true;
		break;
	case ECinemafyActionKind::Attack:
		bApproach = !Reading.bRanged && (Cast<APawn>(Beat.Target) ? Distance > MeleeRange + 60.f : FVector::Dist2D(Here, Point) > 150.f);
		break;
	case ECinemafyActionKind::Look:
		bApproach = false;
		break;
	case ECinemafyActionKind::Say:
		// Clicked on the listener = say it from here; clicked on the floor = walk there first.
		bApproach = !Cast<APawn>(Beat.Target) && FVector::Dist2D(Here, Point) > 150.f;
		break;
	default:
		bApproach = FVector::Dist2D(Here, Point) > 150.f;
		break;
	}

	Phase = bApproach ? EPhase::Approach : (Reading.Kind == ECinemafyActionKind::Wait ? EPhase::Act : EPhase::Face);
	if (Phase == EPhase::Act)
	{
		ActDoneDelay = Reading.Seconds > 0.f ? Reading.Seconds : DefaultWaitSeconds;
	}

	UE_LOG(LogCinemafy, Verbose, TEXT("Cinemafy Choreography: beat %d \"%s\" -> %s"), CurrentBeat + 1, *Beat.Prompt, *Reading.Summary);
}

void ACinemafyChoreography::FinishBeat(ECinemafyBeatOutcome Outcome, const FString& Reason)
{
	if (Beats.IsValidIndex(CurrentBeat))
	{
		FCinemafyChoreoBeat& Beat = Beats[CurrentBeat];

		// A clean finish after a short approach or a missing overlay is a fallback, not a success.
		FString FinalReason = Reason;
		if (Outcome == ECinemafyBeatOutcome::Succeeded && !(ShortfallReason.IsEmpty() && FallbackReason.IsEmpty()))
		{
			Outcome = ECinemafyBeatOutcome::Fallback;
			FinalReason = !ShortfallReason.IsEmpty() ? ShortfallReason : FallbackReason;
		}

		Beat.EndTime = PerformTime;
		Beat.Outcome = Outcome;
		Beat.OutcomeReason = FinalReason;
		if (Outcome == ECinemafyBeatOutcome::Succeeded || Outcome == ECinemafyBeatOutcome::Fallback)
		{
			AddEvent(ECinemafyPerformanceEventKind::ActionEnd);
		}
		if (Outcome != ECinemafyBeatOutcome::Succeeded)
		{
			UE_LOG(LogCinemafy, Warning, TEXT("Cinemafy Choreography: beat %d \"%s\" %s: %s"), CurrentBeat + 1, *Beat.Prompt,
				*StaticEnum<ECinemafyBeatOutcome>()->GetDisplayNameTextByValue(static_cast<int64>(Outcome)).ToString(), *FinalReason);
		}
	}
	++CurrentBeat;
	Phase = EPhase::Start;
}

void ACinemafyChoreography::TickBeat(float DeltaSeconds)
{
	if (Phase == EPhase::Start)
	{
		StartBeat();
		if (!PrecheckFailReason.IsEmpty())
		{
			FinishBeat(ECinemafyBeatOutcome::Failed, PrecheckFailReason);
			PrecheckFailReason.Reset();
			return;
		}
	}

	FCinemafyChoreoBeat& Beat = Beats[CurrentBeat];
	const FCinemafyPromptReading& Reading = Beat.Reading;

	if (bWaitingForCue)
	{
		CueWaitTime += DeltaSeconds;
		if (AActor* Listener = GetLineListener(CurrentBeat))
		{
			FaceToward(Listener->GetActorLocation(), DeltaSeconds);
		}
		if (!IsCueReady())
		{
			if (CueWaitTime <= CueTimeout)
			{
				return;
			}
			// A cue that never comes doesn't take this beat's own line down with it: say so, and go ahead.
			AddEvent(ECinemafyPerformanceEventKind::BeatStart, false, FString::Printf(TEXT("waited %.0fs for %s's line, which never came (are they in the take, and do they have a line?): went ahead"),
				CueTimeout, *GetNameSafe(GetCueActor(CurrentBeat))));
			UE_LOG(LogCinemafy, Warning, TEXT("Cinemafy: %s waited %.0fs for %s's line, which never came; went ahead."), *GetNameSafe(Performer), CueTimeout, *GetNameSafe(GetCueActor(CurrentBeat)));
		}
		bWaitingForCue = false;
		AddEvent(ECinemafyPerformanceEventKind::BeatStart, false, FString::Printf(TEXT("cue from %s"), *GetNameSafe(GetCueActor(CurrentBeat))));
		if (Beat.HasLine())
		{
			StartLine();
		}
	}

	BeatTime += DeltaSeconds;
	PhaseTime += DeltaSeconds;

	if (BeatTime > BeatTimeout)
	{
		FinishBeat(ECinemafyBeatOutcome::TimedOut, FString::Printf(TEXT("Still not done after %.0fs (phase %d)"), BeatTimeout, static_cast<int32>(Phase)));
		return;
	}

	const FVector Here = Performer->GetActorLocation();
	const FVector Point = GetBeatWorldLocation(CurrentBeat);

	switch (Phase)
	{
	case EPhase::Approach:
	{
		// Blocked: report it honestly instead of running into the wall forever (or pretending we arrived).
		StuckTimer += DeltaSeconds;
		if (StuckTimer > 1.2f)
		{
			const bool bStuck = FVector::Dist2D(StuckCheckLocation, Here) < 20.f && !IsTraversing();
			StuckCheckLocation = Here;
			StuckTimer = 0.f;
			if (bStuck && Reading.Kind != ECinemafyActionKind::Traverse)
			{
				const FString Reason = FString::Printf(TEXT("Blocked %.0f cm short of the mark"), FVector::Dist2D(Here, Point));
				if (Reading.Kind == ECinemafyActionKind::Move)
				{
					FinishBeat(ECinemafyBeatOutcome::Blocked, Reason);
				}
				else
				{
					// It can still act from here, but that's a fallback.
					ShortfallReason = Reason + TEXT("; acted from there");
					Phase = EPhase::Face;
					PhaseTime = 0.f;
				}
				return;
			}
		}

		// Out of a full sprint into a jump, a climb or a strike: ease off first (a gather step).
		if (Beat.Transitions.bBrakeIn && !bBraked && CurrentGait == ECinemafyGait::Sprint)
		{
			const FCinemafyTraversalCheck& Check = Beat.TraversalCheck;
			FVector Commit = Point;
			if (Reading.Kind == ECinemafyActionKind::Traverse && Check.bPossible && !Check.TakeoffPoint.IsNearlyZero())
			{
				Commit = Check.TakeoffPoint;
			}
			else if (Reading.Kind == ECinemafyActionKind::Attack && Beat.Target)
			{
				Commit = Beat.Target->GetActorLocation();
			}
			if (FVector::Dist2D(Here, Commit) < Beat.Transitions.BrakeDistance)
			{
				ApplyGait(ECinemafyGait::Run);
				bBraked = true;
				AddEvent(ECinemafyPerformanceEventKind::Transition, false, TEXT("brakes out of the sprint"));
			}
		}

		if (Reading.Kind == ECinemafyActionKind::Traverse && bStepOverOnly)
		{
			// Ankle-high: just walk across it.
			SteerTo(Point, 10.f);
			if (FVector::Dist2D(Here, Point) < 60.f)
			{
				FinishBeat(ECinemafyBeatOutcome::Succeeded, Beat.TraversalCheck.Reason);
			}
			return;
		}
		if (Reading.Kind == ECinemafyActionKind::Traverse)
		{
			const FCinemafyTraversalCheck& Check = Beat.TraversalCheck;
			const bool bRouted = Check.bPossible && !Check.TakeoffPoint.IsNearlyZero();

			// First the run-up start (passing through, no stop), then straight across the face.
			if (bRouted && PathPoints.IsValidIndex(PathIndex))
			{
				if (SteerTo(PathPoints[PathIndex], 45.f))
				{
					++PathIndex;
				}
				return;
			}

			const FVector Takeoff = bRouted ? Check.TakeoffPoint : Point;
			FVector Across = bRouted && !Check.ExitPoint.IsNearlyZero() ? (Check.ExitPoint - Check.TakeoffPoint).GetSafeNormal2D() : FVector::ZeroVector;
			if (Across.IsNearlyZero())
			{
				Across = (Takeoff - Here).GetSafeNormal2D();
			}
			const float Distance = FVector::Dist2D(Here, Takeoff);
			// Aim at a point well past the takeoff on the crossing line, so any sideways offset closes before the jump.
			SteerTo(Takeoff + Across * 300.f, 10.f);

			if (bRouted && (Check.Verdict == ECinemafyTraversalVerdict::Gap || Check.Verdict == ECinemafyTraversalVerdict::Drop))
			{
				// Leave from the edge: a jump for gaps (and drops the prompt asked to jump), a step off for the rest.
				const float Lead = Performer->GetVelocity().Size2D() * 0.1f + 20.f;
				const bool bAtEdge = Distance < Lead || FVector::DotProduct(Here - Takeoff, Across) > -5.f;
				if (bAtEdge)
				{
					const bool bJump = Check.Verdict == ECinemafyTraversalVerdict::Gap
						|| Reading.Keywords.ContainsByPredicate([](const FString& Word) { return Word == TEXT("jump") || Word == TEXT("leap") || Word == TEXT("hop"); });
					if (bJump)
					{
						Performer->Jump();
					}
					AddEvent(ECinemafyPerformanceEventKind::ActionStart, false, bJump ? TEXT("jumps from the edge") : TEXT("steps off the edge"));
					Phase = EPhase::Act;
					PhaseTime = 0.f;
					bTraversalStarted = true;
				}
				return;
			}

			if (Distance < TraversalRange && Rig.Traverse)
			{
				TraverseRetry -= DeltaSeconds;
				if (TraverseRetry <= 0.f)
				{
					TraverseRetry = 0.1f;
					bTraversalStarted = TryTraverse();
				}
			}
			if (!bTraversalStarted && Distance < 70.f)
			{
				// The traversal system didn't take it (or there isn't one). Only an approved stand-in, and it's labeled as one.
				const FString Why = Rig.Traverse ? TEXT("The character's traversal didn't accept this obstacle (height, depth or approach)")
					: TEXT("The performer has no traversal function (set Traversal Function under Wiring)");
				if (PlayNextAnimation())
				{
					FallbackReason = Why + FString::Printf(TEXT("; played %s instead"), *GetNameSafe(ActiveMontage.Get()));
				}
				else
				{
					Performer->Jump();
					FallbackReason = Why + TEXT("; jumped instead");
				}
				bTraversalStarted = true;
			}
			if (bTraversalStarted)
			{
				AddEvent(ECinemafyPerformanceEventKind::ActionStart, false, TEXT("traversal requested"));
				Phase = EPhase::Act;
				PhaseTime = 0.f;
			}
			return;
		}

		FVector Destination = PathPoints.IsValidIndex(PathIndex) ? PathPoints[PathIndex] : Point;
		const bool bLastPoint = PathIndex >= PathPoints.Num() - 1;
		const bool bMeleeTarget = Reading.Kind == ECinemafyActionKind::Attack && Cast<APawn>(Beat.Target);
		float Radius = ArrivalRadius;
		if (bMeleeTarget)
		{
			Destination = Beat.Target->GetActorLocation();
			const ACharacter* TargetCharacter = Cast<ACharacter>(Beat.Target);
			Radius = MeleeRange + (TargetCharacter && TargetCharacter->GetCapsuleComponent() ? TargetCharacter->GetCapsuleComponent()->GetScaledCapsuleRadius() : 40.f);
		}
		else if (!bLastPoint)
		{
			Radius = 80.f;
		}
		else if (CurrentArrival == ECinemafyArrival::PassThrough || CurrentArrival == ECinemafyArrival::EnterAction)
		{
			Radius = CornerRadius;
		}

		if (SteerTo(Destination, Radius))
		{
			if (!bLastPoint && !bMeleeTarget)
			{
				++PathIndex;
			}
			else if (Reading.Kind == ECinemafyActionKind::Move)
			{
				if (CurrentArrival == ECinemafyArrival::StopAndFace)
				{
					// Let the stop play out before anything else starts (no taunting mid-skid).
					Phase = EPhase::Settle;
					PhaseTime = 0.f;
				}
				else
				{
					AddEvent(ECinemafyPerformanceEventKind::Arrived, false, TEXT("passing through"));
					FinishBeat(ECinemafyBeatOutcome::Succeeded);
				}
			}
			else
			{
				AddEvent(ECinemafyPerformanceEventKind::Arrived);
				Phase = EPhase::Face;
				PhaseTime = 0.f;
			}
		}
		return;
	}

	case EPhase::Settle:
	{
		if (Performer->GetVelocity().Size2D() < 25.f || PhaseTime > 1.5f)
		{
			AddEvent(ECinemafyPerformanceEventKind::Arrived, false, FString::Printf(TEXT("stopped %.0f cm from the mark"), FVector::Dist2D(Here, Point)));
			FinishBeat(ECinemafyBeatOutcome::Succeeded);
		}
		return;
	}

	case EPhase::Face:
	{
		bool bFaced = true;
		if (Reading.Kind == ECinemafyActionKind::Say)
		{
			AActor* Listener = GetLineListener(CurrentBeat);
			bFaced = !Listener || FaceToward(Listener->GetActorLocation(), DeltaSeconds);
		}
		else if (Beat.Target || Reading.Kind == ECinemafyActionKind::Look || Reading.Kind == ECinemafyActionKind::Attack)
		{
			bFaced = FaceToward(GetTargetPoint(Beat, CurrentBeat), DeltaSeconds);
		}
		if (bFaced || PhaseTime > 0.6f)
		{
			Phase = EPhase::Act;
			PhaseTime = 0.f;
			switch (Reading.Kind)
			{
			case ECinemafyActionKind::Look:
				ActDoneDelay = Reading.Seconds > 0.f ? Reading.Seconds : 1.f;
				break;
			case ECinemafyActionKind::Say:
				// Hold for the rest of the line (it may have started during the walk over).
				ActDoneDelay = LineEndTime >= 0.f && LineBeat == CurrentBeat ? FMath::Max(LineEndTime - PerformTime, 0.05f) : 0.05f;
				break;
			case ECinemafyActionKind::Attack:
			case ECinemafyActionKind::Perform:
			case ECinemafyActionKind::Equip:
				if (!DrawPendingWeapon.IsEmpty() && AnimQueue.Num() > 0)
				{
					// Draw first, then the strike: the weapon comes out before it's used.
					const FString Weapon = DrawPendingWeapon;
					DrawPendingWeapon.Reset();
					if (ApplyOverlay(Weapon))
					{
						CurrentWeapon = Weapon;
					}
					else
					{
						FallbackReason = FString::Printf(TEXT("No overlay state fits \"%s\"; performed in the current stance"), *Weapon);
					}
					if (UAnimSequenceBase* DrawClip = PickTransitionClip(UCinemafyChoreographyLibrary::ReadPrompt(TEXT("draw ") + Weapon)))
					{
						AnimQueue.Insert(DrawClip, 0);
						TransitionClip = DrawClip;
					}
					AddEvent(ECinemafyPerformanceEventKind::Transition, false, FString::Printf(TEXT("draws the %s%s"), *Weapon,
						TransitionClip ? *(TEXT(" (") + GetNameSafe(TransitionClip) + TEXT(")")) : TEXT("")));
				}
				if (PlayNextAnimation())
				{
					AddEvent(ECinemafyPerformanceEventKind::ActionStart, false, GetNameSafe(ActiveMontage.Get()));
				}
				else if (Reading.Kind == ECinemafyActionKind::Equip && Rig.SetOverlay)
				{
					// The overlay switch is the whole equip (no draw animation in the library).
					ActDoneDelay = 0.5f;
				}
				else
				{
					FinishBeat(ECinemafyBeatOutcome::Failed, Reading.Kind == ECinemafyActionKind::Equip
						? FString::Printf(TEXT("No overlay hook and no draw animation for \"%s\""), *Reading.Weapon)
						: FString::Printf(TEXT("Nothing in the library fits \"%s\" for this skeleton (add a Move Library entry or an Animation Override)"), *Beat.Prompt));
				}
				break;
			default:
				ActDoneDelay = Reading.Seconds > 0.f ? Reading.Seconds : DefaultWaitSeconds;
				break;
			}
		}
		return;
	}

	case EPhase::Act:
	{
		if (Reading.Kind == ECinemafyActionKind::Traverse)
		{
			if (bRecovering)
			{
				// Rolling keeps the momentum; a recovery stands its ground.
				if (Beat.Transitions.Recovery != ECinemafyRecovery::Recover)
				{
					SteerTo(Here + Performer->GetActorForwardVector() * 300.f, 10.f, 0.6f);
				}
				if (!IsAnimationBusy())
				{
					FinishBeat(ECinemafyBeatOutcome::Succeeded);
				}
				return;
			}

			// Keep the input going so the landing flows straight into the next move.
			SteerTo(Here + Performer->GetActorForwardVector() * 300.f, 10.f, 0.6f);
			const bool bTraversingNow = IsTraversing();
			if (bTraversingNow && !bSawTraversal)
			{
				AddEvent(ECinemafyPerformanceEventKind::Takeoff, false, FallbackReason.IsEmpty() ? TEXT("traversal") : TEXT("fallback"));
				AirPeakZ = Here.Z;
			}
			if (bTraversingNow)
			{
				AirPeakZ = FMath::Max(AirPeakZ, Here.Z);
			}
			bSawTraversal |= bTraversingNow;
			if (PhaseTime > 0.35f && !bSawTraversal && FVector::Dist2D(Here, Point) >= 70.f)
			{
				// The traversal call returned without starting anything: keep approaching.
				bTraversalStarted = false;
				Phase = EPhase::Approach;
				PhaseTime = 0.f;
			}
			else if (PhaseTime > 0.3f && bSawTraversal && !bTraversingNow)
			{
				AddEvent(ECinemafyPerformanceEventKind::Landing);

				// A big landing ends in a roll or a recovery: planned from the measured drop, or seen in the take.
				const float Fell = AirPeakZ - Here.Z;
				ECinemafyRecovery Recovery = Beat.Transitions.Recovery;
				if (Recovery == ECinemafyRecovery::None && Beat.bAutoTransitions && Fell > GetTraversalEnvelope().RecoveryDropHeight)
				{
					Recovery = Performer->GetVelocity().Size2D() > 200.f ? ECinemafyRecovery::Roll : ECinemafyRecovery::Recover;
					Beat.Transitions.Recovery = Recovery;
				}
				if (Recovery != ECinemafyRecovery::None)
				{
					FCinemafyPromptReading Landing;
					Landing.Kind = ECinemafyActionKind::Traverse;
					Landing.Keywords = Recovery == ECinemafyRecovery::Roll
						? TArray<FString>{ TEXT("roll"), TEXT("land|landing|recovery") }
						: TArray<FString>{ TEXT("land|landing|recover|recovery"), TEXT("hard|heavy") };
					UAnimSequenceBase* Clip = PickTransitionClip(Landing);
					if (!Clip && Recovery == ECinemafyRecovery::Recover)
					{
						Landing.Keywords = { TEXT("recover|recovery|getup") };
						Clip = PickTransitionClip(Landing);
					}
					if (Clip)
					{
						AnimQueue = { Clip };
						TransitionClip = Clip;
						if (PlayNextAnimation())
						{
							bRecovering = true;
							AddEvent(ECinemafyPerformanceEventKind::Transition, false, FString::Printf(TEXT("%s out of a %.0f cm fall (%s)"),
								Recovery == ECinemafyRecovery::Roll ? TEXT("rolls") : TEXT("recovers"), Fell, *GetNameSafe(Clip)));
							return;
						}
					}
					FallbackReason = FString::Printf(TEXT("Landed from %.0f cm with no %s clip (add one with \"%s\" in its name)"), Fell,
						Recovery == ECinemafyRecovery::Roll ? TEXT("roll") : TEXT("landing recovery"), Recovery == ECinemafyRecovery::Roll ? TEXT("roll") : TEXT("land"));
				}
				FinishBeat(ECinemafyBeatOutcome::Succeeded);
			}
			else if (PhaseTime > 5.f)
			{
				FinishBeat(ECinemafyBeatOutcome::TimedOut, bSawTraversal ? TEXT("Traversal never landed") : TEXT("Traversal never started"));
			}
			return;
		}

		if (Reading.Kind == ECinemafyActionKind::Look)
		{
			FaceToward(GetTargetPoint(Beat, CurrentBeat), DeltaSeconds);
		}
		else if (Reading.Kind == ECinemafyActionKind::Say)
		{
			if (AActor* Listener = GetLineListener(CurrentBeat))
			{
				FaceToward(Listener->GetActorLocation(), DeltaSeconds);
			}
		}

		if (ActiveMontage.IsValid())
		{
			if (!IsAnimationBusy() && !PlayNextAnimation())
			{
				FinishBeat(ECinemafyBeatOutcome::Succeeded);
			}
			return;
		}

		ActDoneDelay -= DeltaSeconds;
		if (ActDoneDelay <= 0.f)
		{
			FinishBeat(ECinemafyBeatOutcome::Succeeded);
		}
		return;
	}

	default:
		return;
	}
}

bool ACinemafyChoreography::SteerTo(const FVector& Destination, float Radius, float InputScale)
{
	const FVector Delta = (Destination - Performer->GetActorLocation()) * FVector(1.f, 1.f, 0.f);
	const float Distance = Delta.Size();
	if (Distance <= Radius)
	{
		return true;
	}
	Performer->AddMovementInput(Delta / Distance, InputScale);
	return false;
}

bool ACinemafyChoreography::FaceToward(const FVector& Point, float DeltaSeconds)
{
	const FVector Direction = (Point - Performer->GetActorLocation()).GetSafeNormal2D();
	if (Direction.IsNearlyZero())
	{
		return true;
	}
	const float Desired = Direction.Rotation().Yaw;
	const float Current = Performer->GetActorRotation().Yaw;
	const float NewYaw = FMath::FixedTurn(Current, Desired, TurnSpeed * DeltaSeconds);
	Performer->SetActorRotation(FRotator(0.f, NewYaw, 0.f));
	return FMath::Abs(FRotator::NormalizeAxis(Desired - NewYaw)) < 5.f;
}

bool ACinemafyChoreography::PlayNextAnimation()
{
	USkeletalMeshComponent* Mesh = Performer ? Performer->GetMesh() : nullptr;
	UAnimInstance* Anim = Mesh ? Mesh->GetAnimInstance() : nullptr;
	if (!Anim || AnimQueue.Num() == 0)
	{
		return false;
	}

	FCinemafyChoreoBeat& Beat = Beats[CurrentBeat];
	UAnimSequenceBase* Clip = AnimQueue[0];
	AnimQueue.RemoveAt(0);
	if (!Clip)
	{
		return PlayNextAnimation();
	}

	ActivePlayRate = FMath::Max(Beat.PlayRate, 0.1f);

	// Line the strike up for montages with warp windows. The root target is where the attacker should stand
	// (in front of the target, facing it) - not the target's own position, or root motion walks into them.
	if (APawn* TargetPawn = Cast<APawn>(Beat.Target))
	{
		if (UMotionWarpingComponent* Warping = Performer->FindComponentByClass<UMotionWarpingComponent>())
		{
			const FVector TargetLocation = TargetPawn->GetActorLocation();
			const FVector Toward = (TargetLocation - Performer->GetActorLocation()).GetSafeNormal2D();
			const ACharacter* TargetCharacter = Cast<ACharacter>(TargetPawn);
			const float Standoff = MeleeRange + (TargetCharacter && TargetCharacter->GetCapsuleComponent() ? TargetCharacter->GetCapsuleComponent()->GetScaledCapsuleRadius() : 40.f);
			const FVector Stand = TargetLocation - Toward * Standoff;
			const FVector RootLocation(Stand.X, Stand.Y, Performer->GetActorLocation().Z);
			for (const TCHAR* Name : { TEXT("AttackTarget"), TEXT("Target"), TEXT("Warp"), TEXT("MeleeTarget") })
			{
				Warping->AddOrUpdateWarpTargetFromLocationAndRotation(Name, RootLocation, Toward.Rotation());
			}
		}
	}

	UAnimMontage* Montage = Cast<UAnimMontage>(Clip);
	if (Montage)
	{
		if (Anim->Montage_Play(Montage, ActivePlayRate) <= 0.f)
		{
			return false;
		}
	}
	else
	{
		Montage = Anim->PlaySlotAnimationAsDynamicMontage(Clip, TEXT("DefaultSlot"), 0.2f, 0.25f, ActivePlayRate, 1);
		if (!Montage)
		{
			return false;
		}
	}
	ActiveMontage = Montage;
	UsedAnimations.Add(FSoftObjectPath(Clip));

	// When does it land? An authored hit/impact/contact notify is taken as the contact. A damage or trace
	// *window* only brackets it, and with nothing at all it's a labeled guess at 40%.
	if (Beat.Reading.Kind == ECinemafyActionKind::Attack && Clip != TransitionClip)
	{
		float Contact = Clip->GetPlayLength() * 0.4f;
		bool bEstimated = true;
		FString Note = TEXT("estimated at 40% (no hit notify)");
		for (const FAnimNotifyEvent& Notify : Clip->Notifies)
		{
			const FString Name = Notify.NotifyName.ToString().ToLower() + (Notify.Notify ? Notify.Notify->GetClass()->GetName().ToLower() : FString())
				+ (Notify.NotifyStateClass ? Notify.NotifyStateClass->GetClass()->GetName().ToLower() : FString());
			if (Name.Contains(TEXT("hit")) || Name.Contains(TEXT("impact")) || Name.Contains(TEXT("contact")))
			{
				Contact = Notify.GetTriggerTime();
				bEstimated = false;
				Note = Notify.NotifyName.ToString();
				break;
			}
			if (bEstimated && (Name.Contains(TEXT("damage")) || Name.Contains(TEXT("trace")) || Name.Contains(TEXT("window"))))
			{
				Contact = Notify.GetTriggerTime() + Notify.GetDuration() * 0.4f;
				Note = FString::Printf(TEXT("estimated inside %s"), *Notify.NotifyName.ToString());
			}
		}
		FCinemafyPendingContact& Pending = PendingContacts.AddDefaulted_GetRef();
		Pending.BeatIndex = CurrentBeat;
		Pending.Time = PerformTime + Contact / ActivePlayRate;
		Pending.bEstimated = bEstimated;
		Pending.Montage = Montage;
		Pending.Note = Clip->GetName() + TEXT(": ") + Note;

		// The target answers on the same moment: a combo's last strike gets the big reaction.
		ScheduleReaction(CurrentBeat, Montage, Pending.Time, AnimQueue.Num() == 0);
	}
	return true;
}

bool ACinemafyChoreography::IsAnimationBusy() const
{
	UAnimMontage* Montage = ActiveMontage.Get();
	USkeletalMeshComponent* Mesh = Performer ? Performer->GetMesh() : nullptr;
	UAnimInstance* Anim = Mesh ? Mesh->GetAnimInstance() : nullptr;
	if (!Montage || !Anim || !Anim->Montage_IsPlaying(Montage))
	{
		return false;
	}
	// Hand over as the montage starts blending out, so the next thing overlaps the tail.
	const float Remaining = (Montage->GetPlayLength() - Anim->Montage_GetPosition(Montage)) / ActivePlayRate;
	return Remaining > Montage->GetDefaultBlendOutTime();
}

bool ACinemafyChoreography::IsTraversing() const
{
	USkeletalMeshComponent* Mesh = Performer ? Performer->GetMesh() : nullptr;
	UAnimInstance* Anim = Mesh ? Mesh->GetAnimInstance() : nullptr;
	const UCharacterMovementComponent* Movement = Performer ? Performer->GetCharacterMovement() : nullptr;
	return (Anim && Anim->IsAnyMontagePlaying())
		|| (Movement && (Movement->MovementMode == MOVE_Flying || Movement->MovementMode == MOVE_Falling));
}

// ---------------------------------------------------------------------------------------------------------------------
// Interactions: the target answers a strike, its impact frame on the attacker's contact
// ---------------------------------------------------------------------------------------------------------------------

namespace
{
	/** bHoldLastFrame: the character stays in the clip's final pose (a fall) instead of blending back to idle. */
	UAnimMontage* PlayClipOn(ACharacter* Character, UAnimSequenceBase* Clip, float BlendIn, bool bHoldLastFrame = false)
	{
		USkeletalMeshComponent* Mesh = Character ? Character->GetMesh() : nullptr;
		UAnimInstance* Anim = Mesh ? Mesh->GetAnimInstance() : nullptr;
		if (!Anim || !Clip)
		{
			return nullptr;
		}
		if (UAnimMontage* Montage = Cast<UAnimMontage>(Clip))
		{
			// A montage asset is shared, so it isn't modified; UpdateReactions pauses it before it blends out.
			return Anim->Montage_Play(Montage, 1.f) > 0.f ? Montage : nullptr;
		}
		UAnimMontage* Dynamic = Anim->PlaySlotAnimationAsDynamicMontage(Clip, TEXT("DefaultSlot"), BlendIn, bHoldLastFrame ? 0.f : 0.25f, 1.f, 1);
		if (Dynamic && bHoldLastFrame)
		{
			// The engine's way to hold a montage's last frame: never auto-blend out. The playing instance copies the flag
			// when play starts, so set it there too (this montage is ours, made just now).
			Dynamic->bEnableAutoBlendOut = false;
			if (FAnimMontageInstance* Instance = Anim->GetActiveInstanceForMontage(Dynamic))
			{
				Instance->bEnableAutoBlendOut = false;
			}
		}
		return Dynamic;
	}

	/** How long before the strike lands a reaction has to start: its own hit notify, else a sensible lead for the kind. */
	float ReactionLead(const UAnimSequenceBase* Clip, ECinemafyHitReaction Reaction)
	{
		if (Clip)
		{
			for (const FAnimNotifyEvent& Notify : Clip->Notifies)
			{
				const FString Name = Notify.NotifyName.ToString().ToLower();
				if (Name.Contains(TEXT("hit")) || Name.Contains(TEXT("impact")) || Name.Contains(TEXT("contact")))
				{
					return Notify.GetTriggerTime();
				}
			}
		}
		switch (Reaction)
		{
		case ECinemafyHitReaction::Block: return 0.25f;  // the guard comes up before the blow
		case ECinemafyHitReaction::Dodge: return 0.3f;   // and the dodge starts before it
		default:                       return 0.06f;  // hit, knockdown and death react on the blow
		}
	}
}

bool ACinemafyChoreography::IsDowned(const ACharacter* Character) const
{
	return HeldPoses.ContainsByPredicate([Character](const TPair<TWeakObjectPtr<ACharacter>, TWeakObjectPtr<UAnimMontage>>& Held) { return Held.Key.Get() == Character; });
}

UAnimSequenceBase* ACinemafyChoreography::PickReaction(ACharacter* Defender, ECinemafyHitReaction Reaction, float RelativeYaw, float& OutLead)
{
	OutLead = ReactionLead(nullptr, Reaction);
	const USkeletalMeshComponent* Mesh = Defender ? Defender->GetMesh() : nullptr;
	const USkeleton* Skeleton = Mesh && Mesh->GetSkeletalMeshAsset() ? Mesh->GetSkeletalMeshAsset()->GetSkeleton() : nullptr;
	if (!Skeleton)
	{
		return nullptr;
	}

	TArray<FString> Folders;
	for (const FDirectoryPath& Folder : SearchFolders)
	{
		Folders.Add(Folder.Path);
	}
	// Defenders can be on a different skeleton from the performer, so each skeleton gets its own catalog.
	TArray<FCinemafyMoveCandidate>& Catalog = PartnerCandidates.FindOrAdd(GetPathNameSafe(Skeleton));
	if (Catalog.Num() == 0)
	{
		Catalog = UCinemafyChoreographyLibrary::GatherCandidates(GetMoveLibrary(), Folders, Skeleton);
	}

	for (const ECinemafyHitReaction Try : { Reaction, ECinemafyHitReaction::Hit })
	{
		const FCinemafyPromptReading Reading = UCinemafyChoreographyLibrary::MakeReactionReading(Try, RelativeYaw);
		const TArray<int32> Picks = UCinemafyChoreographyLibrary::PickAnimations(Reading, Catalog, 1, UsedAnimations);
		if (Picks.Num() > 0)
		{
			if (UAnimSequenceBase* Clip = Cast<UAnimSequenceBase>(Catalog[Picks[0]].Path.TryLoad()))
			{
				UsedAnimations.Add(Catalog[Picks[0]].Path);
				OutLead = ReactionLead(Clip, Try);
				if (Try != Reaction)
				{
					UE_LOG(LogCinemafy, Log, TEXT("Cinemafy Choreography: no %s reaction for %s; using a hit react."),
						*StaticEnum<ECinemafyHitReaction>()->GetDisplayNameTextByValue(static_cast<int64>(Reaction)).ToString(), *GetNameSafe(Defender));
				}
				return Clip;
			}
		}
		if (Try == ECinemafyHitReaction::Hit || Reaction == ECinemafyHitReaction::Block || Reaction == ECinemafyHitReaction::Dodge)
		{
			break; // a block or a dodge never quietly turns into a hit
		}
	}
	return nullptr;
}

void ACinemafyChoreography::ScheduleReaction(int32 BeatIndex, UAnimMontage* AttackMontage, float ContactTime, bool bFinalStrike)
{
	if (!Beats.IsValidIndex(BeatIndex) || !Performer)
	{
		return;
	}
	FCinemafyChoreoBeat& Beat = Beats[BeatIndex];
	ACharacter* Defender = Cast<ACharacter>(Beat.Target);
	if (!Beat.bTargetReacts || !Defender || Defender == Performer || IsDowned(Defender))
	{
		return;
	}

	ECinemafyHitReaction Reaction = Beat.bOverrideReaction ? Beat.Reaction : Beat.Reading.Reaction;
	if (Reaction == ECinemafyHitReaction::None)
	{
		return;
	}
	// Every strike before a combo's last one gets a hit react (a block stays a block all the way through).
	if (!bFinalStrike && Reaction != ECinemafyHitReaction::Block)
	{
		Reaction = ECinemafyHitReaction::Hit;
	}

	// Where the strike comes from at contact. A defender facing roughly toward it turns to take it head-on.
	const FVector ToAttacker = (Performer->GetActorLocation() - Defender->GetActorLocation()).GetSafeNormal2D();
	float RelativeYaw = FRotator::NormalizeAxis(ToAttacker.Rotation().Yaw - Defender->GetActorRotation().Yaw);
	if (FMath::Abs(RelativeYaw) <= DefenderTurnLimit)
	{
		RelativeYaw = 0.f;
	}

	float Lead = 0.f;
	UAnimSequenceBase* Clip = nullptr;
	if (Beat.ReactionOverride)
	{
		Clip = Beat.ReactionOverride;
		Lead = ReactionLead(Clip, Reaction);
	}
	else
	{
		Clip = PickReaction(Defender, Reaction, RelativeYaw, Lead);
	}
	if (!Clip)
	{
		const FString Why = FString::Printf(TEXT("No %s reaction animation fits %s's skeleton; the target didn't react"),
			*StaticEnum<ECinemafyHitReaction>()->GetDisplayNameTextByValue(static_cast<int64>(Reaction)).ToString(), *GetNameSafe(Defender));
		if (BeatIndex == CurrentBeat && FallbackReason.IsEmpty())
		{
			FallbackReason = Why;
		}
		UE_LOG(LogCinemafy, Warning, TEXT("Cinemafy Choreography: %s."), *Why);
		return;
	}

	// Hold the target still for the exchange, so they don't wander out of the blow.
	if (AController* Controller = Defender->GetController())
	{
		Controller->StopMovement();
	}

	FCinemafyPendingReaction& Pending = PendingReactions.AddDefaulted_GetRef();
	Pending.BeatIndex = BeatIndex;
	Pending.Defender = Defender;
	Pending.ContactTime = ContactTime;
	Pending.StartTime = FMath::Max(PerformTime, ContactTime - Lead);
	Pending.Clip = Clip;
	Pending.AttackMontage = AttackMontage;
	Pending.Reaction = Reaction;
}

void ACinemafyChoreography::UpdateReactions(float DeltaSeconds)
{
	// Downed defenders hold the last frame of their fall.
	for (const TPair<TWeakObjectPtr<ACharacter>, TWeakObjectPtr<UAnimMontage>>& Held : HeldPoses)
	{
		ACharacter* Character = Held.Key.Get();
		UAnimMontage* Montage = Held.Value.Get();
		UAnimInstance* Anim = Character && Character->GetMesh() ? Character->GetMesh()->GetAnimInstance() : nullptr;
		// Pause before the montage's own blend-out starts (by then the fall is already easing back to idle).
		const float HoldAt = Montage ? Montage->GetPlayLength() - FMath::Max(Montage->BlendOut.GetBlendTime(), 0.05f) : 0.f;
		if (Anim && Montage && Montage->bEnableAutoBlendOut && Anim->Montage_IsPlaying(Montage) && Anim->Montage_GetPosition(Montage) >= HoldAt)
		{
			Anim->Montage_Pause(Montage);
		}
	}

	for (int32 Index = PendingReactions.Num() - 1; Index >= 0; --Index)
	{
		const FCinemafyPendingReaction Pending = PendingReactions[Index];
		ACharacter* Defender = Pending.Defender.Get();
		if (!Defender || !Beats.IsValidIndex(Pending.BeatIndex))
		{
			PendingReactions.RemoveAt(Index);
			continue;
		}
		FCinemafyChoreoBeat& Beat = Beats[Pending.BeatIndex];

		// During the wind-up, a defender facing the blow turns into it.
		const FVector ToAttacker = (Performer->GetActorLocation() - Defender->GetActorLocation()).GetSafeNormal2D();
		const float Yaw = FRotator::NormalizeAxis(ToAttacker.Rotation().Yaw - Defender->GetActorRotation().Yaw);
		if (FMath::Abs(Yaw) <= DefenderTurnLimit && FMath::Abs(Yaw) > 2.f)
		{
			const float NewYaw = FMath::FixedTurn(Defender->GetActorRotation().Yaw, ToAttacker.Rotation().Yaw, TurnSpeed * DeltaSeconds);
			Defender->SetActorRotation(FRotator(0.f, NewYaw, 0.f));
		}
		if (PerformTime < Pending.StartTime)
		{
			continue;
		}
		PendingReactions.RemoveAt(Index);

		auto Note = [&Beat, this, Defender](const FString& Text)
		{
			FCinemafyPerformanceEvent& Event = Beat.Events.AddDefaulted_GetRef();
			Event.Kind = ECinemafyPerformanceEventKind::Reaction;
			Event.Time = PerformTime;
			Event.Location = Defender->GetActorLocation();
			Event.Note = Text;
		};

		// The swing was cut off before it could land, so there's nothing to react to.
		USkeletalMeshComponent* AttackerMesh = Performer->GetMesh();
		UAnimInstance* AttackerAnim = AttackerMesh ? AttackerMesh->GetAnimInstance() : nullptr;
		if (!Pending.AttackMontage.IsValid() || !AttackerAnim || !AttackerAnim->Montage_IsPlaying(Pending.AttackMontage.Get()))
		{
			continue;
		}

		// A defender in the middle of their own action isn't overridden (but our own reactions can be cut into).
		UAnimInstance* DefenderAnim = Defender->GetMesh() ? Defender->GetMesh()->GetAnimInstance() : nullptr;
		UAnimMontage* Busy = DefenderAnim ? DefenderAnim->GetCurrentActiveMontage() : nullptr;
		if (Busy && !ReactionMontages.Contains(Busy))
		{
			Note(FString::Printf(TEXT("%s was busy (%s); no reaction"), *GetNameSafe(Defender), *GetNameSafe(Busy)));
			continue;
		}

		const bool bGoesDown = Pending.Reaction == ECinemafyHitReaction::Death || Pending.Reaction == ECinemafyHitReaction::Knockdown;
		UAnimMontage* Played = PlayClipOn(Defender, Pending.Clip.Get(), Pending.Reaction == ECinemafyHitReaction::Block ? 0.15f : 0.06f, bGoesDown);
		if (!Played)
		{
			Note(FString::Printf(TEXT("%s couldn't play %s"), *GetNameSafe(Defender), *GetNameSafe(Pending.Clip.Get())));
			continue;
		}
		ReactionMontages.Add(Played);
		if (bGoesDown)
		{
			HeldPoses.Add({ Defender, Played });
		}
		Note(FString::Printf(TEXT("%s: %s (impact %.2fs in, on the contact)"), *GetNameSafe(Defender), *GetNameSafe(Pending.Clip.Get()),
			Pending.ContactTime - Pending.StartTime));
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// Editor display
// ---------------------------------------------------------------------------------------------------------------------

namespace
{
	FColor ColorForKind(ECinemafyActionKind Kind)
	{
		switch (Kind)
		{
		case ECinemafyActionKind::Move:     return FColor(90, 200, 255);
		case ECinemafyActionKind::Traverse: return FColor(255, 200, 60);
		case ECinemafyActionKind::Attack:   return FColor(255, 70, 70);
		case ECinemafyActionKind::Perform:  return FColor(200, 120, 255);
		case ECinemafyActionKind::Equip:    return FColor(255, 150, 60);
		case ECinemafyActionKind::Look:     return FColor(140, 255, 140);
		case ECinemafyActionKind::Say:      return FColor(255, 240, 150);
		default:                            return FColor(200, 200, 200);
		}
	}
}

void ACinemafyChoreography::RefreshLabels()
{
	UWorld* World = GetWorld();
	if (!World || World->IsGameWorld())
	{
		return;
	}

	for (UTextRenderComponent* Label : Labels)
	{
		if (Label)
		{
			Label->DestroyComponent();
		}
	}
	Labels.Reset();

	if (!bShowPath)
	{
		return;
	}
	RefreshTransitions();

	for (int32 Index = 0; Index < Beats.Num(); ++Index)
	{
		const FCinemafyChoreoBeat& Beat = Beats[Index];
		UTextRenderComponent* Label = NewObject<UTextRenderComponent>(this, NAME_None, RF_Transient | RF_TextExportTransient);
		Label->SetupAttachment(RootComponent);
		Label->bIsEditorOnly = true;
		Label->SetHiddenInGame(true);
		Label->SetHorizontalAlignment(EHTA_Center);
		Label->SetWorldSize(22.f);
		// After a take, problems show right at the beat: red for blocked / failed / timed out, amber for fallbacks.
		const bool bProblem = Beat.Outcome == ECinemafyBeatOutcome::Blocked || Beat.Outcome == ECinemafyBeatOutcome::Failed || Beat.Outcome == ECinemafyBeatOutcome::TimedOut;
		const bool bFallback = Beat.Outcome == ECinemafyBeatOutcome::Fallback;
		FString Text = FString::Printf(TEXT("%d  %s"), Index + 1, *Beat.Prompt);
		if (bProblem || bFallback)
		{
			Text += FString::Printf(TEXT("\n%s: %s"), *StaticEnum<ECinemafyBeatOutcome>()->GetDisplayNameTextByValue(static_cast<int64>(Beat.Outcome)).ToString(), *Beat.OutcomeReason);
		}
		// Before a take, traversal beats say what they measured: red if it can't work, so you know before you hit record.
		const FCinemafyTraversalCheck& Check = Beat.TraversalCheck;
		// Nothing measured (no obstacle found, or no run-up) isn't a refusal: the take still tries it, so only warn in text.
		const bool bWontWork = Beat.Reading.Kind == ECinemafyActionKind::Traverse && Check.Verdict != ECinemafyTraversalVerdict::NotChecked
			&& Check.Verdict != ECinemafyTraversalVerdict::NoObstacle && !Check.bPossible;
		if (Beat.Reading.Kind == ECinemafyActionKind::Traverse && Check.Verdict != ECinemafyTraversalVerdict::NotChecked && !bProblem && !bFallback)
		{
			Text += TEXT("\n") + Check.Reason;
		}
		if (!Beat.Transitions.Summary.IsEmpty())
		{
			Text += TEXT("\n+ ") + Beat.Transitions.Summary;
		}
		Label->SetTextRenderColor(bProblem || bWontWork ? FColor(255, 60, 60) : bFallback ? FColor(255, 170, 40) : ColorForKind(Beat.Reading.Kind));
		Label->SetText(FText::FromString(Text));
		Label->RegisterComponent();
		Label->SetWorldLocation(GetBeatWorldLocation(Index) + FVector(0.f, 0.f, 60.f));
		Labels.Add(Label);
	}
}

void ACinemafyChoreography::DrawPath() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FVector Previous = Performer ? Performer->GetActorLocation() : GetActorLocation();
	for (int32 Index = 0; Index < Beats.Num(); ++Index)
	{
		const FCinemafyChoreoBeat& Beat = Beats[Index];
		const FVector Point = GetBeatWorldLocation(Index);
		const FColor Color = ColorForKind(Beat.Reading.Kind);
		const bool bRoutedTraversal = Beat.Reading.Kind == ECinemafyActionKind::Traverse && Beat.TraversalCheck.bPossible
			&& !Beat.TraversalCheck.TakeoffPoint.IsNearlyZero();
		if (bRoutedTraversal)
		{
			// The route below draws the run-up and the crossing; the path carries on from where it comes out.
			DrawDebugLine(World, Previous + FVector(0, 0, 5), Beat.TraversalCheck.ApproachPoint + FVector(0, 0, 5), Color, false, -1.f, SDPG_Foreground, 3.f);
			Previous = Beat.TraversalCheck.ExitPoint.IsNearlyZero() ? Point : Beat.TraversalCheck.ExitPoint;
		}
		else if (Beat.Reading.Kind != ECinemafyActionKind::Look)
		{
			DrawDebugLine(World, Previous + FVector(0, 0, 5), Point + FVector(0, 0, 5), Color, false, -1.f, SDPG_Foreground, 3.f);
			Previous = Point;
		}
		else
		{
			DrawDebugLine(World, Previous + FVector(0, 0, 150), Point, Color, false, -1.f, SDPG_Foreground, 1.f);
		}
		DrawDebugSphere(World, Point, 18.f, 10, Color, false, -1.f, SDPG_Foreground, 2.f);
		const FCinemafyTraversalCheck& Check = Beat.TraversalCheck;
		if (Beat.Reading.Kind == ECinemafyActionKind::Traverse && Check.Verdict != ECinemafyTraversalVerdict::NotChecked
			&& Check.Verdict != ECinemafyTraversalVerdict::NoObstacle)
		{
			// The measured obstacle: front face, top, far edge and landing, green if it works, red if not.
			const FColor CheckColor = Check.bPossible ? FColor(60, 230, 90) : FColor(255, 60, 60);
			const FVector FrontTop(Check.FrontPoint.X, Check.FrontPoint.Y, Check.FarPoint.Z);
			const FVector FrontFloor(Check.FrontPoint.X, Check.FrontPoint.Y, Check.FarPoint.Z - Check.Height);
			DrawDebugLine(World, FrontFloor, FrontTop, CheckColor, false, -1.f, SDPG_Foreground, 2.f);
			if (Check.Depth > 0.f)
			{
				DrawDebugLine(World, FrontTop, Check.FarPoint, CheckColor, false, -1.f, SDPG_Foreground, 2.f);
			}
			if (!Check.LandingPoint.IsNearlyZero())
			{
				DrawDebugLine(World, Check.FarPoint, Check.LandingPoint, CheckColor, false, -1.f, SDPG_Foreground, 1.f);
				DrawDebugCircle(World, Check.LandingPoint + FVector(0, 0, 2), 35.f, 16, CheckColor, false, -1.f, SDPG_Foreground, 1.5f, FVector(1, 0, 0), FVector(0, 1, 0), false);
			}
			if (Check.bPossible && !Check.TakeoffPoint.IsNearlyZero())
			{
				// The planned route: to the run-up start, the straight run at the takeoff, and out the other side.
				const FColor RouteColor(120, 220, 255);
				const FVector Lift(0, 0, 8);
				DrawDebugLine(World, Check.ApproachPoint + Lift, Check.TakeoffPoint + Lift, RouteColor, false, -1.f, SDPG_Foreground, 2.5f);
				DrawDebugDirectionalArrow(World, Check.TakeoffPoint + Lift, (Check.ExitPoint.IsNearlyZero() ? Check.FarPoint : Check.ExitPoint) + Lift,
					25.f, RouteColor, false, -1.f, SDPG_Foreground, 1.5f);
				DrawDebugCircle(World, Check.ApproachPoint + FVector(0, 0, 3), 22.f, 12, RouteColor, false, -1.f, SDPG_Foreground, 1.5f, FVector(1, 0, 0), FVector(0, 1, 0), false);
			}
		}
		if (Beat.Target)
		{
			DrawDebugLine(World, Point, Beat.Target->GetActorLocation(), Color, false, -1.f, SDPG_World, 1.f);
		}
	}

	// Labels face the camera.
	if (World->ViewLocationsRenderedLastFrame.Num() > 0)
	{
		const FVector View = World->ViewLocationsRenderedLastFrame[0];
		for (UTextRenderComponent* Label : Labels)
		{
			if (Label)
			{
				const FVector ToView = (View - Label->GetComponentLocation()).GetSafeNormal2D();
				Label->SetWorldRotation(ToView.Rotation());
			}
		}
	}
}

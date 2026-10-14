#include "CinemafyChoreography.h"
#include "CinemafyModule.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "Animation/AnimSequenceBase.h"
#include "Components/CapsuleComponent.h"
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
	TFunction<void(bool)> FindBoolSetter(UObject* Object, const FString& Needle)
	{
		for (TFieldIterator<FProperty> It(Object->GetClass()); It; ++It)
		{
			if (FBoolProperty* Bool = CastField<FBoolProperty>(*It); Bool && It->GetName().ToLower().Contains(Needle))
			{
				TWeakObjectPtr<UObject> Weak = Object;
				return [Weak, Bool](bool bValue) { if (UObject* O = Weak.Get()) { Bool->SetPropertyValue_InContainer(O, bValue); } };
			}
			if (FStructProperty* Struct = CastField<FStructProperty>(*It))
			{
				for (TFieldIterator<FProperty> Inner(Struct->Struct); Inner; ++Inner)
				{
					if (FBoolProperty* Bool = CastField<FBoolProperty>(*Inner); Bool && Inner->GetName().ToLower().Contains(Needle))
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
	const FString Key = GetPathNameSafe(Skeleton) + TEXT("|") + GetPathNameSafe(MoveLibrary) + TEXT("|") + FString::Join(Folders, TEXT(","));
	if (Key == CandidatesSkeleton && Candidates.Num() > 0)
	{
		return;
	}
	CandidatesSkeleton = Key;
	Candidates = UCinemafyChoreographyLibrary::GatherCandidates(MoveLibrary, Folders, Skeleton);
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

	if (Beat.AnimationOverride)
	{
		Beat.ChosenAnimations.Add(Beat.AnimationOverride);
		return;
	}

	const ECinemafyActionKind Kind = Beat.Reading.Kind;
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

void ACinemafyChoreography::BuildRig()
{
	Rig = FCinemafyPerformerRig();
	TArray<FString> Report;
	const TArray<UObject*> Objects = CinemafyRig::GetRigObjects(Performer);

	auto BoolSetter = [&Objects](FName Exact, const TCHAR* Needle) -> TFunction<void(bool)>
	{
		UObject* Object = nullptr;
		UFunction* Function = nullptr;
		const FString Wanted = FString(TEXT("setwantsto")) + Needle;
		if (CinemafyRig::FindFunction(Objects, Exact, [&Wanted](const FString& Name, const UFunction* F)
			{
				return Name == Wanted && CinemafyRig::CountInputs(F) == 1 && CastField<FBoolProperty>(CinemafyRig::FirstInput(F));
			}, Object, Function))
		{
			TWeakObjectPtr<UObject> Weak = Object;
			return [Weak, Function](bool bValue) { CinemafyRig::CallWithBool(Weak.Get(), Function, bValue); };
		}
		for (UObject* Candidate : Objects)
		{
			if (TFunction<void(bool)> Setter = CinemafyRig::FindBoolSetter(Candidate, FString(TEXT("wantsto")) + Needle))
			{
				return Setter;
			}
		}
		return nullptr;
	};

	Rig.SetSprint = BoolSetter(SprintFunction, TEXT("sprint"));
	Rig.SetWalk = BoolSetter(WalkFunction, TEXT("walk"));
	Report.Add(FString::Printf(TEXT("Gait: %s"), Rig.SetSprint || Rig.SetWalk ? TEXT("wants-to flags") : TEXT("speed fallback")));

	// Traversal (GASP: TryTraversalAction, fed by GetTraversalCheckInputs).
	{
		UObject* Object = nullptr;
		UFunction* Function = nullptr;
		if (CinemafyRig::FindFunction(Objects, TraversalFunction, [](const FString& Name, const UFunction*)
			{
				return Name.StartsWith(TEXT("try")) && Name.Contains(TEXT("traversal"));
			}, Object, Function))
		{
			TWeakObjectPtr<UObject> Weak = Object;
			Rig.Traverse = [Weak, Function]() { UObject* O = Weak.Get(); return O && CinemafyRig::CallAndReadSuccess(O, Function); };
			Report.Add(FString::Printf(TEXT("Traversal: %s.%s"), *Object->GetClass()->GetName(), *Function->GetName()));
		}
		else
		{
			Report.Add(TEXT("Traversal: none found (falls back to Jump / a picked vault animation)"));
		}
	}

	// Overlay: a SetXxxOverlayXxx(enum) function, or an enum property with "overlay" in its name.
	{
		struct FOverlayHook { TWeakObjectPtr<UObject> Object; UFunction* Function = nullptr; FProperty* Property = nullptr; };
		TArray<FOverlayHook> Hooks;
		for (UObject* Object : Objects)
		{
			if (!Object)
			{
				continue;
			}
			for (TFieldIterator<UFunction> It(Object->GetClass(), EFieldIteratorFlags::IncludeSuper); It; ++It)
			{
				const FString Name = It->GetName().ToLower();
				const bool bNameMatches = !OverlayFunction.IsNone() ? It->GetFName() == OverlayFunction : (Name.StartsWith(TEXT("set")) && Name.Contains(TEXT("overlay")));
				if (bNameMatches && CinemafyRig::CountInputs(*It) == 1 && CinemafyRig::GetEnum(CinemafyRig::FirstInput(*It)))
				{
					Hooks.Add({ Object, *It, nullptr });
				}
			}
			if (OverlayFunction.IsNone())
			{
				for (TFieldIterator<FProperty> It(Object->GetClass()); It; ++It)
				{
					if (It->GetName().ToLower().Contains(TEXT("overlay")) && CinemafyRig::GetEnum(*It))
					{
						Hooks.Add({ Object, nullptr, *It });
					}
				}
			}
		}

		if (Hooks.Num() > 0)
		{
			Rig.SetOverlay = [Hooks](const FString& Weapon) -> bool
			{
				// Try every hook and use the one whose enum fits the weapon best.
				int32 BestHook = INDEX_NONE;
				int64 BestValue = 0;
				float BestScore = 0.f;
				for (int32 Index = 0; Index < Hooks.Num(); ++Index)
				{
					const FProperty* Prop = Hooks[Index].Function ? CinemafyRig::FirstInput(Hooks[Index].Function) : Hooks[Index].Property;
					int64 Value = 0;
					float Score = 0.f;
					if (CinemafyRig::BestEnumValue(CinemafyRig::GetEnum(Prop), Weapon, Value, Score) && Score > BestScore)
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
		else
		{
			Report.Add(TEXT("Overlay: none found"));
		}
	}

	// Crouch: the character's own StartCrouch/StopCrouch if it has them, else ACharacter::Crouch.
	{
		UObject* StartObject = nullptr; UFunction* Start = nullptr;
		UObject* StopObject = nullptr; UFunction* Stop = nullptr;
		CinemafyRig::FindFunction(Objects, NAME_None, [](const FString& Name, const UFunction* F) { return Name == TEXT("startcrouch") && CinemafyRig::CountInputs(F) == 0; }, StartObject, Start);
		CinemafyRig::FindFunction(Objects, NAME_None, [](const FString& Name, const UFunction* F) { return Name == TEXT("stopcrouch") && CinemafyRig::CountInputs(F) == 0; }, StopObject, Stop);
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

	WiringReport = FString::Join(Report, TEXT("; "));
	UE_LOG(LogCinemafy, Log, TEXT("Cinemafy Choreography wiring for %s: %s"), *GetNameSafe(Performer), *WiringReport);
}

void ACinemafyChoreography::ApplyGait(ECinemafyGait Gait)
{
	if (!Performer)
	{
		return;
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
			Movement->MaxWalkSpeed = static_cast<float>(Gait == ECinemafyGait::Sprint ? FallbackSpeeds.Z : Gait == ECinemafyGait::Run ? FallbackSpeeds.Y : FallbackSpeeds.X);
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
		if (Beats[Index].Reading.Summary.IsEmpty())
		{
			ResolveBeat(Index);
		}
		Beats[Index].StartTime = -1.f;
		Beats[Index].EndTime = -1.f;
		Beats[Index].ImpactTimes.Reset();
		Beats[Index].bFailed = false;
	}

	// The performer needs a controller for movement input to land; a player stops driving it for the take.
	if (APlayerController* PC = Cast<APlayerController>(Performer->GetController()))
	{
		PC->UnPossess();
		PC->SetViewTarget(Performer);
	}
	if (!Performer->GetController())
	{
		Performer->SpawnDefaultController();
		SpawnedController = Performer->GetController();
	}

	BuildRig();
	bCrouched = Performer->bIsCrouched;
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
		FinishPerformance();
	}
}

void ACinemafyChoreography::FinishPerformance()
{
	bPerforming = false;
	TakeDuration = PerformTime;
	OnPerformanceEvent.Broadcast(this, false);
	OnPerformanceFinished.Broadcast();
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
		if (TailLeft <= 0.f && !IsAnimationBusy())
		{
			FinishPerformance();
		}
		return;
	}

	TickBeat(DeltaSeconds);
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

	if (!Reading.Weapon.IsEmpty())
	{
		ApplyOverlay(Reading.Weapon);
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

void ACinemafyChoreography::FinishBeat(bool bFailed)
{
	if (Beats.IsValidIndex(CurrentBeat))
	{
		Beats[CurrentBeat].EndTime = PerformTime;
		Beats[CurrentBeat].bFailed |= bFailed;
		if (bFailed)
		{
			UE_LOG(LogCinemafy, Warning, TEXT("Cinemafy Choreography: beat %d \"%s\" didn't finish cleanly."), CurrentBeat + 1, *Beats[CurrentBeat].Prompt);
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
	}

	FCinemafyChoreoBeat& Beat = Beats[CurrentBeat];
	const FCinemafyPromptReading& Reading = Beat.Reading;
	BeatTime += DeltaSeconds;
	PhaseTime += DeltaSeconds;

	if (BeatTime > BeatTimeout)
	{
		FinishBeat(true);
		return;
	}

	const FVector Here = Performer->GetActorLocation();
	const FVector Point = GetBeatWorldLocation(CurrentBeat);

	switch (Phase)
	{
	case EPhase::Approach:
	{
		// Stuck against something: call it arrived rather than run into the wall forever.
		StuckTimer += DeltaSeconds;
		if (StuckTimer > 1.2f)
		{
			const bool bStuck = FVector::Dist2D(StuckCheckLocation, Here) < 20.f && !IsTraversing();
			StuckCheckLocation = Here;
			StuckTimer = 0.f;
			if (bStuck && Reading.Kind != ECinemafyActionKind::Traverse)
			{
				UE_LOG(LogCinemafy, Log, TEXT("Cinemafy Choreography: %s is blocked on beat %d; carrying on from here."), *GetNameSafe(Performer), CurrentBeat + 1);
				if (Reading.Kind == ECinemafyActionKind::Move)
				{
					FinishBeat(false);
				}
				else
				{
					Phase = EPhase::Face;
					PhaseTime = 0.f;
				}
				return;
			}
		}

		if (Reading.Kind == ECinemafyActionKind::Traverse)
		{
			const float Distance = FVector::Dist2D(Here, Point);
			// Keep running at it; ask the character to traverse as soon as it's in range.
			const FVector Beyond = Point + (Point - Here).GetSafeNormal2D() * 300.f;
			SteerTo(Beyond, 10.f);
			if (Distance < TraversalRange)
			{
				TraverseRetry -= DeltaSeconds;
				if (TraverseRetry <= 0.f)
				{
					TraverseRetry = 0.1f;
					if (TryTraverse())
					{
						bTraversalStarted = true;
					}
				}
			}
			if (!bTraversalStarted && Distance < 70.f)
			{
				// No traversal system (or it said no): a picked vault animation, else a jump.
				if (!PlayNextAnimation())
				{
					Performer->Jump();
				}
				bTraversalStarted = true;
			}
			if (bTraversalStarted)
			{
				Beat.ImpactTimes.Add(PerformTime);
				Phase = EPhase::Act;
				PhaseTime = 0.f;
			}
			return;
		}

		float Radius = ArrivalRadius;
		FVector Destination = PathPoints.IsValidIndex(PathIndex) ? PathPoints[PathIndex] : Point;
		const bool bLastPoint = PathIndex >= PathPoints.Num() - 1;
		if (Reading.Kind == ECinemafyActionKind::Attack && Cast<APawn>(Beat.Target))
		{
			Destination = Beat.Target->GetActorLocation();
			const ACharacter* TargetCharacter = Cast<ACharacter>(Beat.Target);
			Radius = MeleeRange + (TargetCharacter && TargetCharacter->GetCapsuleComponent() ? TargetCharacter->GetCapsuleComponent()->GetScaledCapsuleRadius() : 40.f);
		}
		else if (!bLastPoint)
		{
			Radius = 80.f;
		}
		else if (Reading.Kind == ECinemafyActionKind::Move && Beats.IsValidIndex(CurrentBeat + 1))
		{
			const ECinemafyActionKind Next = Beats[CurrentBeat + 1].Reading.Kind;
			if (Next == ECinemafyActionKind::Move || Next == ECinemafyActionKind::Traverse)
			{
				Radius = CornerRadius;
			}
		}

		if (SteerTo(Destination, Radius))
		{
			if (!bLastPoint && !(Reading.Kind == ECinemafyActionKind::Attack && Cast<APawn>(Beat.Target)))
			{
				++PathIndex;
			}
			else if (Reading.Kind == ECinemafyActionKind::Move)
			{
				FinishBeat(false);
			}
			else
			{
				Phase = EPhase::Face;
				PhaseTime = 0.f;
			}
		}
		return;
	}

	case EPhase::Face:
	{
		bool bFaced = true;
		if (Beat.Target || Reading.Kind == ECinemafyActionKind::Look || Reading.Kind == ECinemafyActionKind::Attack)
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
			case ECinemafyActionKind::Attack:
			case ECinemafyActionKind::Perform:
			case ECinemafyActionKind::Equip:
				if (!PlayNextAnimation())
				{
					// Equip without an animation is just the overlay change; anything else had nothing to play.
					ActDoneDelay = Reading.Kind == ECinemafyActionKind::Equip ? 0.5f : 0.8f;
					Beat.bFailed = Reading.Kind != ECinemafyActionKind::Equip;
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
			// Keep the input going so the landing flows straight into the next move.
			SteerTo(Here + Performer->GetActorForwardVector() * 300.f, 10.f, 0.6f);
			bSawTraversal |= IsTraversing();
			if (PhaseTime > 0.35f && !bSawTraversal && FVector::Dist2D(Here, Point) >= 70.f)
			{
				// The traversal call said yes but nothing happened (no result pin): keep approaching.
				Beat.ImpactTimes.Pop();
				bTraversalStarted = false;
				Phase = EPhase::Approach;
				PhaseTime = 0.f;
			}
			else if (PhaseTime > 0.3f && bSawTraversal && !IsTraversing())
			{
				FinishBeat(false);
			}
			else if (PhaseTime > 5.f)
			{
				FinishBeat(true);
			}
			return;
		}

		if (Reading.Kind == ECinemafyActionKind::Look)
		{
			FaceToward(GetTargetPoint(Beat, CurrentBeat), DeltaSeconds);
		}

		if (ActiveMontage.IsValid())
		{
			if (!IsAnimationBusy() && !PlayNextAnimation())
			{
				FinishBeat(false);
			}
			return;
		}

		ActDoneDelay -= DeltaSeconds;
		if (ActDoneDelay <= 0.f)
		{
			FinishBeat(false);
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

	// Line the strike up with the target for montages that warp (motion warping windows).
	if (Beat.Target)
	{
		if (UMotionWarpingComponent* Warping = Performer->FindComponentByClass<UMotionWarpingComponent>())
		{
			const FVector TargetLocation = Beat.Target->GetActorLocation();
			const FRotator Facing = (TargetLocation - Performer->GetActorLocation()).GetSafeNormal2D().Rotation();
			for (const TCHAR* Name : { TEXT("AttackTarget"), TEXT("Target"), TEXT("Warp"), TEXT("MeleeTarget") })
			{
				Warping->AddOrUpdateWarpTargetFromLocationAndRotation(Name, TargetLocation, Facing);
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

	// When does it land? A notify named like a hit, else about 40% in.
	float Impact = Clip->GetPlayLength() * 0.4f;
	for (const FAnimNotifyEvent& Notify : Clip->Notifies)
	{
		const FString Name = Notify.NotifyName.ToString().ToLower() + (Notify.Notify ? Notify.Notify->GetClass()->GetName().ToLower() : FString())
			+ (Notify.NotifyStateClass ? Notify.NotifyStateClass->GetClass()->GetName().ToLower() : FString());
		if (Name.Contains(TEXT("hit")) || Name.Contains(TEXT("impact")) || Name.Contains(TEXT("damage")) || Name.Contains(TEXT("trace")) || Name.Contains(TEXT("window")))
		{
			Impact = Notify.GetTriggerTime();
			break;
		}
	}
	if (Beat.Reading.Kind == ECinemafyActionKind::Attack)
	{
		Beat.ImpactTimes.Add(PerformTime + Impact / ActivePlayRate);
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

	for (int32 Index = 0; Index < Beats.Num(); ++Index)
	{
		const FCinemafyChoreoBeat& Beat = Beats[Index];
		UTextRenderComponent* Label = NewObject<UTextRenderComponent>(this, NAME_None, RF_Transient | RF_TextExportTransient);
		Label->SetupAttachment(RootComponent);
		Label->bIsEditorOnly = true;
		Label->SetHiddenInGame(true);
		Label->SetHorizontalAlignment(EHTA_Center);
		Label->SetWorldSize(22.f);
		Label->SetTextRenderColor(ColorForKind(Beat.Reading.Kind));
		Label->SetText(FText::FromString(FString::Printf(TEXT("%d  %s"), Index + 1, *Beat.Prompt)));
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
		if (Beat.Reading.Kind != ECinemafyActionKind::Look)
		{
			DrawDebugLine(World, Previous + FVector(0, 0, 5), Point + FVector(0, 0, 5), Color, false, -1.f, SDPG_Foreground, 3.f);
			Previous = Point;
		}
		else
		{
			DrawDebugLine(World, Previous + FVector(0, 0, 150), Point, Color, false, -1.f, SDPG_Foreground, 1.f);
		}
		DrawDebugSphere(World, Point, 18.f, 10, Color, false, -1.f, SDPG_Foreground, 2.f);
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

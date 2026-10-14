#include "CinemafyStoryLibrary.h"
#include "CinemafyModule.h"
#include "CinemafyProduction.h"
#include "CinemafyScript.h"
#include "CinemafySceneDirector.h"
#include "Animation/AnimSequenceBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "Editor.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Modules/ModuleManager.h"
#include "Sound/SoundWave.h"

namespace CinemafyLipSync
{
	/** The speaker's face: the cast entry's Face Mesh, else the mesh with eye bones on their actor in the level. */
	USkeletalMesh* FaceFor(const UCinemafyScriptProject* Project, const FString& Speaker, UWorld* World)
	{
		if (const FCinemafyCastMember* Member = Project->FindCastMember(Speaker))
		{
			if (USkeletalMesh* Mesh = Member->FaceMesh.LoadSynchronous())
			{
				return Mesh;
			}
		}
		AActor* Actor = World ? UCinemafyStoryLibrary::FindCastActor(Project, Speaker, World) : nullptr;
		if (!Actor)
		{
			return nullptr;
		}
		TInlineComponentArray<USkeletalMeshComponent*> Meshes(Actor);
		for (const USkeletalMeshComponent* Mesh : Meshes)
		{
			if (Mesh->GetBoneIndex(Cinemafy::LeftEyeBone) != INDEX_NONE)
			{
				return Mesh->GetSkeletalMeshAsset();
			}
		}
		return nullptr;
	}
}

bool UCinemafyStoryLibrary::IsLipSyncAvailable()
{
	for (const TCHAR* Path : { TEXT("/Script/MetaHumanPerformance.MetaHumanPerformance"), TEXT("/Script/MetaHumanAnimator.MetaHumanPerformance") })
	{
		if (FindObject<UClass>(nullptr, Path))
		{
			return true;
		}
	}
	return false;
}

TArray<FCinemafyLipSyncJob> UCinemafyStoryLibrary::GetLipSyncJobs(UCinemafyScriptProject* Project, const FString& SceneId, bool bReplaceExisting)
{
	TArray<FCinemafyLipSyncJob> Jobs;
	if (!Project)
	{
		return Jobs;
	}
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	TMap<FString, USkeletalMesh*> Faces;
	TSet<FString> Faceless;
	for (const FCinemafyScriptScene* Scene : Project->GetAllScenes())
	{
		if ((!SceneId.IsEmpty() && Scene->Id != SceneId) || Scene->Status == ECinemafySceneStatus::Cut)
		{
			continue;
		}
		for (const FCinemafyScriptLine& Line : Scene->Lines)
		{
			if (Line.Kind != ECinemafyScriptLineKind::Dialogue || Line.Audio.IsNull() || (Line.HasCurrentFace() && !bReplaceExisting))
			{
				continue;
			}
			if (!Cast<USoundWave>(Line.Audio.LoadSynchronous()))
			{
				continue;
			}
			USkeletalMesh** Known = Faces.Find(Line.Speaker);
			USkeletalMesh* Face = Known ? *Known : Faces.Add(Line.Speaker, CinemafyLipSync::FaceFor(Project, Line.Speaker, World));
			if (!Face)
			{
				Faceless.Add(Line.Speaker);
				continue;
			}
			FCinemafyLipSyncJob& Job = Jobs.AddDefaulted_GetRef();
			Job.LineId = Line.LineId;
			Job.SceneId = Scene->Id;
			Job.Speaker = Line.Speaker;
			Job.Text = Line.Text;
			Job.AudioPath = Line.Audio.ToSoftObjectPath().ToString();
			Job.FaceMeshPath = FSoftObjectPath(Face).ToString();
			Job.PackagePath = FString::Printf(TEXT("/Game/Cinemafy/LipSync/SC_%s"), *Scene->Id.Replace(TEXT("."), TEXT("_")));
			Job.AssetName = TEXT("Face_") + Line.LineId;
		}
	}
	for (const FString& Speaker : Faceless)
	{
		UE_LOG(LogCinemafy, Warning, TEXT("Cinemafy lip-sync: no face for %s (put their MetaHuman in the level, or set Face Mesh on their cast entry). Their lines keep the Talk curves."), *Speaker);
	}
	return Jobs;
}

bool UCinemafyStoryLibrary::AssignLipSync(UCinemafyScriptProject* Project, const FString& LineId, UAnimSequenceBase* Animation)
{
	if (!Project || !Animation || LineId.IsEmpty())
	{
		return false;
	}
	for (FCinemafyScriptScene* Scene : Project->GetAllScenes())
	{
		for (FCinemafyScriptLine& Line : Scene->Lines)
		{
			if (Line.LineId != LineId)
			{
				continue;
			}
			Project->Modify();
			Line.FaceAnimation = Animation;
			Line.FaceMadeFrom = Line.Audio.ToSoftObjectPath();
			Project->MarkPackageDirty();

			// Directors already in the level that speak this line get the face too.
			if (UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr)
			{
				for (TActorIterator<ACinemafySceneDirector> It(World); It; ++It)
				{
					for (FCinemafyLine& Spoken : It->Lines)
					{
						if (Spoken.Text.ToString().Equals(Line.Text, ESearchCase::IgnoreCase) && (!It->ScriptProject || It->ScriptProject == Project))
						{
							It->Modify();
							Spoken.FaceAnimation = Animation;
						}
					}
				}
			}
			return true;
		}
	}
	return false;
}

FString UCinemafyStoryLibrary::GenerateLipSync(UCinemafyScriptProject* Project, const FString& SceneId, bool bReplaceExisting)
{
	if (!Project || !GEditor)
	{
		return TEXT("No script project.");
	}
	if (!FModuleManager::Get().IsModuleLoaded(TEXT("PythonScriptPlugin")))
	{
		return TEXT("Lip-sync runs through the Python Editor Script Plugin: enable it (and the MetaHuman plugin), then try again. Faces keep the Talk curves meanwhile.");
	}
	if (!IsLipSyncAvailable())
	{
		return TEXT("MetaHuman's audio-driven animation isn't available: enable the MetaHuman plugin (MetaHuman Animator). Faces keep the Talk curves meanwhile.");
	}
	const int32 Count = GetLipSyncJobs(Project, SceneId, bReplaceExisting).Num();
	if (Count == 0)
	{
		return TEXT("Every voiced line already has a face (lines need audio first: Scratch Voices or recorded voice-over), or no speaker has a MetaHuman face in the level.");
	}
	const FString Command = FString::Printf(TEXT("py import unreal, cinemafy_lipsync; unreal.log(cinemafy_lipsync.generate(unreal.load_asset('%s'), '%s', %s))"),
		*Project->GetPathName(), *SceneId.Replace(TEXT("'"), TEXT("")), bReplaceExisting ? TEXT("True") : TEXT("False"));
	GEditor->Exec(GEditor->GetEditorWorldContext().World(), *Command);
	return FString::Printf(TEXT("Lip-sync started for %d line(s): MetaHuman solves them one by one in the background (progress in the Output Log). Direct the scene again once they're in."), Count);
}

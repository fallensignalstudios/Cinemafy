#include "CinemafyStoryLibrary.h"
#include "CinemafyModule.h"
#include "CinemafySceneDirector.h"
#include "CinemafyTypes.h"
#include "CineCameraActor.h"
#include "CineCameraComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Editor.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "ImageUtils.h"
#include "LevelSequence.h"
#include "Misc/Crc.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "MoviePipelineDeferredPasses.h"
#include "MoviePipelineImageSequenceOutput.h"
#include "MoviePipelineOutputSetting.h"
#include "MoviePipelinePIEExecutor.h"
#include "MoviePipelinePrimaryConfig.h"
#include "MoviePipelineQueue.h"
#include "MoviePipelineQueueSubsystem.h"
#include "ScopedTransaction.h"
#include "Serialization/BufferArchive.h"
#include "Sound/SoundBase.h"

#define LOCTEXT_NAMESPACE "CinemafyPaperwork"

namespace CinemafyPaperwork
{
	FString ProjectFolderName(const UCinemafyScriptProject* Project)
	{
		return FPaths::MakeValidFileName(Project->Title.IsEmpty() ? Project->GetName() : Project->Title).Replace(TEXT(" "), TEXT("_"));
	}

	FString Csv(const FString& Value)
	{
		return TEXT("\"") + Value.Replace(TEXT("\""), TEXT("\"\"")).Replace(TEXT("\n"), TEXT(" ")) + TEXT("\"");
	}

	FString Html(const FString& Value)
	{
		return Value.Replace(TEXT("&"), TEXT("&amp;")).Replace(TEXT("<"), TEXT("&lt;")).Replace(TEXT(">"), TEXT("&gt;"));
	}

	FString SrtTime(float Seconds)
	{
		const int32 Millis = FMath::Max(0, FMath::RoundToInt(Seconds * 1000.f));
		return FString::Printf(TEXT("%02d:%02d:%02d,%03d"), Millis / 3600000, (Millis / 60000) % 60, (Millis / 1000) % 60, Millis % 1000);
	}

	/** A line's length: its audio, else its words at speaking pace. */
	float LineSeconds(const FCinemafyScriptLine& Line)
	{
		if (USoundBase* Sound = Line.Audio.LoadSynchronous())
		{
			const float Duration = Sound->GetDuration();
			if (Duration > 0.f && Duration < INDEFINITELY_LOOPING_DURATION)
			{
				return Duration;
			}
		}
		TArray<FString> Words;
		Line.Text.ParseIntoArrayWS(Words);
		return FMath::Max(1.f, Words.Num() / 2.6f + 0.4f);
	}

	/** One frame from a camera's point of view, saved as a PNG. */
	bool CaptureFrame(UWorld* World, ACineCameraActor* Camera, const FString& Path)
	{
		if (!World || !Camera)
		{
			return false;
		}
		UTextureRenderTarget2D* Target = NewObject<UTextureRenderTarget2D>();
		Target->InitCustomFormat(640, 360, PF_B8G8R8A8, false);
		Target->TargetGamma = 2.2f;
		USceneCaptureComponent2D* Capture = NewObject<USceneCaptureComponent2D>(Camera);
		Capture->bCaptureEveryFrame = false;
		Capture->bCaptureOnMovement = false;
		Capture->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
		Capture->TextureTarget = Target;
		if (const UCineCameraComponent* Lens = Camera->GetCineCameraComponent())
		{
			Capture->FOVAngle = Lens->GetHorizontalFieldOfView();
		}
		Capture->RegisterComponentWithWorld(World);
		Capture->SetWorldTransform(Camera->GetCineCameraComponent() ? Camera->GetCineCameraComponent()->GetComponentTransform() : Camera->GetActorTransform());
		Capture->CaptureScene();
		FBufferArchive Png;
		const bool bSaved = FImageUtils::ExportRenderTarget2DAsPNG(Target, Png) && FFileHelper::SaveArrayToFile(Png, *Path);
		Capture->DestroyComponent();
		return bSaved;
	}
}

int32 UCinemafyStoryLibrary::RenderDailies(UCinemafyScriptProject* Project, const FString& SceneId)
{
	UMoviePipelineQueueSubsystem* Queues = GEditor ? GEditor->GetEditorSubsystem<UMoviePipelineQueueSubsystem>() : nullptr;
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!Project || !Queues || !World)
	{
		return 0;
	}
	if (Queues->IsRendering())
	{
		UE_LOG(LogCinemafy, Warning, TEXT("Cinemafy: a render is already running; dailies not queued."));
		return 0;
	}
	UMoviePipelineQueue* Queue = Queues->GetQueue();
	Queue->DeleteAllJobs();

	const FString Root = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Cinemafy") / TEXT("Dailies") / CinemafyPaperwork::ProjectFolderName(Project));
	int32 Jobs = 0;
	Project->Modify();
	for (FCinemafyScriptScene* Scene : Project->GetAllScenes())
	{
		const bool bWanted = SceneId.IsEmpty()
			? (Scene->Status == ECinemafySceneStatus::Recorded || Scene->Status == ECinemafySceneStatus::Directed || Scene->Status == ECinemafySceneStatus::Final)
			: Scene->Id == SceneId;
		if (!bWanted || Scene->Sequence.IsNull())
		{
			continue;
		}
		UMoviePipelineExecutorJob* Job = Queue->AllocateNewJob(UMoviePipelineExecutorJob::StaticClass());
		Job->JobName = FString::Printf(TEXT("%s %s"), *Scene->Id, *Scene->Heading);
		Job->Sequence = Scene->Sequence.ToSoftObjectPath();
		Job->Map = !Scene->Level.IsNull() ? Scene->Level.ToSoftObjectPath() : FSoftObjectPath(World);

		UMoviePipelinePrimaryConfig* Config = Job->GetConfiguration();
		Config->FindOrAddSettingByClass(UMoviePipelineDeferredPassBase::StaticClass());
		Config->FindOrAddSettingByClass(UMoviePipelineImageSequenceOutput_JPG::StaticClass());
		if (UMoviePipelineOutputSetting* Output = Cast<UMoviePipelineOutputSetting>(Config->FindOrAddSettingByClass(UMoviePipelineOutputSetting::StaticClass())))
		{
			Scene->DailiesFolder = Root / Scene->Id.Replace(TEXT("."), TEXT("_"));
			Output->OutputDirectory.Path = Scene->DailiesFolder;
			Output->OutputResolution = FIntPoint(1280, 720);
			Output->FileNameFormat = TEXT("{sequence_name}.{frame_number}");
		}
		++Jobs;
	}
	if (Jobs > 0)
	{
		Queues->RenderQueueWithExecutor(UMoviePipelinePIEExecutor::StaticClass());
		UE_LOG(LogCinemafy, Log, TEXT("Cinemafy: rendering %d daily(ies) to %s."), Jobs, *Root);
	}
	return Jobs;
}

bool UCinemafyStoryLibrary::AddReviewNote(UCinemafyScriptProject* Project, const FString& SceneId, const FString& Text, float AtSeconds)
{
	FCinemafyScriptScene* Scene = Project ? Project->FindScene(SceneId) : nullptr;
	if (!Scene || Text.IsEmpty())
	{
		return false;
	}
	const FScopedTransaction Transaction(LOCTEXT("ReviewNote", "Cinemafy: Review Note"));
	Project->Modify();
	FCinemafyReviewNote& Note = Scene->ReviewNotes.AddDefaulted_GetRef();
	Note.Author = FPlatformProcess::UserName();
	Note.Date = FDateTime::Now().ToString(TEXT("%Y-%m-%d %H:%M"));
	Note.AtSeconds = AtSeconds;
	Note.Text = Text;
	Project->MarkPackageDirty();
	return true;
}

FString UCinemafyStoryLibrary::ExportPaperwork(UCinemafyScriptProject* Project)
{
	using namespace CinemafyPaperwork;
	if (!Project)
	{
		return FString();
	}
	const FString Folder = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Cinemafy") / TEXT("Paperwork") / ProjectFolderName(Project));
	IFileManager::Get().MakeDirectory(*Folder, true);
	const FString Title = Project->Title.IsEmpty() ? Project->GetName() : Project->Title;
	const TArray<FCinemafyScriptScene*> Scenes = Project->GetAllScenes();

	// 1) Voice-over recording scripts, one per character: every line in order, with what it answers.
	TMap<FString, TArray<FString>> Booth;
	TMap<FString, TArray<FString>> BoothCsv;
	for (const FCinemafyScriptScene* Scene : Scenes)
	{
		FString Cue;
		int32 Number = 0;
		for (const FCinemafyScriptLine& Line : Scene->Lines)
		{
			if (Line.Kind != ECinemafyScriptLineKind::Dialogue)
			{
				continue;
			}
			++Number;
			TArray<FString>& Script = Booth.FindOrAdd(Line.Speaker);
			if (Script.Num() == 0)
			{
				Script.Add(FString::Printf(TEXT("# %s: voice-over script for %s\n"), *Title, *Line.Speaker));
				Script.Add(TEXT("| File | Scene | Cue (what it answers) | Direction | Line |"));
				Script.Add(TEXT("| --- | --- | --- | --- | --- |"));
				BoothCsv.FindOrAdd(Line.Speaker).Add(TEXT("File,Scene,LineNumber,Cue,Direction,Line,Status"));
			}
			const FString File = Line.LineId + TEXT(".wav");
			Script.Add(FString::Printf(TEXT("| %s | %s %s | %s | %s | %s |"), *File, *Scene->Id, *Scene->Heading.Replace(TEXT("|"), TEXT("/")),
				*Cue.Replace(TEXT("|"), TEXT("/")), *Line.Direction, *Line.Text.Replace(TEXT("|"), TEXT("/"))));
			BoothCsv.FindOrAdd(Line.Speaker).Add(FString::Printf(TEXT("%s,%s,%d,%s,%s,%s,%s"), *Csv(File), *Csv(Scene->Id), Number, *Csv(Cue), *Csv(Line.Direction), *Csv(Line.Text),
				*Csv(Line.Audio.IsNull() ? TEXT("to record") : (Line.bScratchAudio ? TEXT("scratch only") : TEXT("recorded")))));
			Cue = FString::Printf(TEXT("%s: %s"), *Line.Speaker, *Line.Text.Left(60));
		}
	}
	for (const TPair<FString, TArray<FString>>& Pair : Booth)
	{
		const FString Name = FPaths::MakeValidFileName(Pair.Key).Replace(TEXT(" "), TEXT("_"));
		FFileHelper::SaveStringToFile(FString::Join(Pair.Value, TEXT("\n")), *(Folder / FString::Printf(TEXT("VO_%s.md"), *Name)), FFileHelper::EEncodingOptions::ForceUTF8);
		FFileHelper::SaveStringToFile(FString::Join(BoothCsv[Pair.Key], TEXT("\n")), *(Folder / FString::Printf(TEXT("VO_%s.csv"), *Name)), FFileHelper::EEncodingOptions::ForceUTF8);
	}

	// 2) Subtitles for the whole story, scene after scene (timed by audio where there is some, else estimated).
	{
		TArray<FString> Srt;
		float Time = 0.f;
		int32 Index = 0;
		for (const FCinemafyScriptScene* Scene : Scenes)
		{
			if (Scene->Status == ECinemafySceneStatus::Cut)
			{
				continue;
			}
			const float SceneStart = Time;
			for (const FCinemafyScriptLine& Line : Scene->Lines)
			{
				if (Line.Kind != ECinemafyScriptLineKind::Dialogue)
				{
					continue;
				}
				const float Length = LineSeconds(Line);
				Srt.Add(FString::Printf(TEXT("%d\n%s --> %s\n%s: %s\n"), ++Index, *SrtTime(Time), *SrtTime(Time + Length), *Line.Speaker, *Line.Text));
				Time += Length + 0.5f;
			}
			Time = FMath::Max(Time, SceneStart + Scene->EstimatedSeconds);
		}
		FFileHelper::SaveStringToFile(FString::Join(Srt, TEXT("\n")), *(Folder / TEXT("Subtitles_Story.srt")), FFileHelper::EEncodingOptions::ForceUTF8);
	}

	// 3) Localization: an Unreal String Table CSV keyed by line id.
	{
		TArray<FString> Rows = { TEXT("Key,SourceString,Comment") };
		for (const FCinemafyScriptScene* Scene : Scenes)
		{
			for (const FCinemafyScriptLine& Line : Scene->Lines)
			{
				if (Line.Kind == ECinemafyScriptLineKind::Dialogue)
				{
					Rows.Add(FString::Printf(TEXT("%s,%s,%s"), *Csv(Line.LineId), *Csv(Line.Text),
						*Csv(FString::Printf(TEXT("%s | %s | %s %s"), *Line.Speaker, *Scene->Id, *Scene->Heading, *Line.Direction))));
				}
			}
			for (const FCinemafyScriptChoice& Choice : Scene->Choices)
			{
				Rows.Add(FString::Printf(TEXT("%s,%s,%s"), *Csv(FString::Printf(TEXT("CHOICE_%s_%08X"), *Scene->Id.Replace(TEXT("."), TEXT("_")), FCrc::StrCrc32(*Choice.Text))),
					*Csv(Choice.Text), *Csv(FString::Printf(TEXT("Choice | %s"), *Scene->Id))));
			}
		}
		FFileHelper::SaveStringToFile(FString::Join(Rows, TEXT("\n")), *(Folder / TEXT("Localization_StringTable.csv")), FFileHelper::EEncodingOptions::ForceUTF8);
	}

	// 4) Shot lists and 5) the storyboard, from the level's directors.
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	TArray<FString> Shots = { TEXT("Scene,Shot,Start,Duration,Type,Subject,Lens,Move,Reason") };
	TArray<FString> Board;
	Board.Add(FString::Printf(TEXT("<!doctype html><html><head><meta charset=\"utf-8\"><title>%s storyboard</title><style>")
		TEXT("body{font-family:sans-serif;background:#16161a;color:#eee;margin:24px}h2{color:#ff9e26;margin-top:32px}")
		TEXT(".grid{display:flex;flex-wrap:wrap;gap:14px}.shot{width:320px;background:#24242a;border-radius:6px;overflow:hidden}")
		TEXT(".shot img{width:320px;height:180px;object-fit:cover;background:#000;display:block}.shot div{padding:8px;font-size:13px}")
		TEXT(".why{color:#aaa;font-size:12px}</style></head><body><h1>%s</h1>"), *Html(Title), *Html(Title)));
	const FString FrameFolder = Folder / TEXT("Storyboard");
	IFileManager::Get().MakeDirectory(*FrameFolder, true);
	if (World)
	{
		const UEnum* ShotEnum = StaticEnum<ECinemafyShotType>();
		const UEnum* LensEnum = StaticEnum<ECinemafyPrime>();
		const UEnum* MoveEnum = StaticEnum<ECinemafyCameraMove>();
		for (const FCinemafyScriptScene* Scene : Scenes)
		{
			ACinemafySceneDirector* Director = nullptr;
			for (TActorIterator<ACinemafySceneDirector> It(World); It; ++It)
			{
				if (It->ScriptProject == Project && It->ScriptSceneId == Scene->Id)
				{
					Director = *It;
					break;
				}
			}
			if (!Director || Director->LastPlan.Num() == 0)
			{
				continue;
			}
			Board.Add(FString::Printf(TEXT("<h2>%s %s</h2><div class=\"grid\">"), *Html(Scene->Id), *Html(Scene->Heading)));
			for (int32 Index = 0; Index < Director->LastPlan.Num(); ++Index)
			{
				const FCinemafyPlannedShot& Shot = Director->LastPlan[Index];
				const FString Type = ShotEnum->GetDisplayNameTextByValue(static_cast<int64>(Shot.ShotType)).ToString();
				const FString Lens = LensEnum->GetDisplayNameTextByValue(static_cast<int64>(Shot.Lens)).ToString();
				const FString Move = Shot.bMove ? MoveEnum->GetDisplayNameTextByValue(static_cast<int64>(Shot.Move)).ToString() : FString(TEXT("static"));
				const FString Subject = Shot.Subject ? Shot.Subject->GetActorLabel() : FString();
				Shots.Add(FString::Printf(TEXT("%s,%d,%.2f,%.2f,%s,%s,%s,%s,%s"), *Csv(Scene->Id), Index + 1, Shot.StartTime, Shot.Duration, *Csv(Type), *Csv(Subject), *Csv(Lens), *Csv(Move), *Csv(Shot.Reason)));

				const FString Frame = FString::Printf(TEXT("%s_%02d.png"), *Scene->Id.Replace(TEXT("."), TEXT("_")), Index + 1);
				ACineCameraActor* Camera = Director->LastPlanCameras.IsValidIndex(Index) ? Director->LastPlanCameras[Index].Get() : nullptr;
				const bool bFrame = CaptureFrame(World, Camera, FrameFolder / Frame);
				Board.Add(FString::Printf(TEXT("<div class=\"shot\">%s<div><b>%d. %s</b> on %s | %s | %.1fs | %s<div class=\"why\">%s</div></div></div>"),
					bFrame ? *FString::Printf(TEXT("<img src=\"Storyboard/%s\">"), *Frame) : TEXT("<img>"),
					Index + 1, *Html(Type), *Html(Subject), *Html(Lens), Shot.Duration, *Html(Move), *Html(Shot.Reason)));
			}
			Board.Add(TEXT("</div>"));
		}
	}
	Board.Add(TEXT("</body></html>"));
	FFileHelper::SaveStringToFile(FString::Join(Shots, TEXT("\n")), *(Folder / TEXT("ShotList.csv")), FFileHelper::EEncodingOptions::ForceUTF8);
	FFileHelper::SaveStringToFile(FString::Join(Board, TEXT("\n")), *(Folder / TEXT("Storyboard.html")), FFileHelper::EEncodingOptions::ForceUTF8);
	FFileHelper::SaveStringToFile(Project->BuildChecklist(), *(Folder / TEXT("Checklist.md")), FFileHelper::EEncodingOptions::ForceUTF8);
	UE_LOG(LogCinemafy, Log, TEXT("Cinemafy: paperwork for %s written to %s."), *Title, *Folder);
	return Folder;
}

#undef LOCTEXT_NAMESPACE

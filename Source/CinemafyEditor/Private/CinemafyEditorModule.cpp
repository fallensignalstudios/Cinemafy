#include "CinemafyEditorModule.h"
#include "CinemafyEditorLibrary.h"
#include "CinemafyTypes.h"
#include "ToolMenus.h"
#include "Framework/Commands/UIAction.h"
#include "Textures/SlateIcon.h"
#include "Modules/ModuleManager.h"

#define LOCTEXT_NAMESPACE "CinemafyEditor"

namespace
{
	/** Adds one menu entry per (non-hidden) enum value. */
	template <typename TEnum, typename TFunc>
	void AddEnumEntries(FToolMenuSection& Section, TFunc&& OnPicked)
	{
		const UEnum* Enum = StaticEnum<TEnum>();
		for (int32 Index = 0; Index < Enum->NumEnums() - 1; ++Index)
		{
			if (Enum->HasMetaData(TEXT("Hidden"), Index))
			{
				continue;
			}

			const TEnum Value = static_cast<TEnum>(Enum->GetValueByIndex(Index));
			Section.AddMenuEntry(
				FName(*Enum->GetNameStringByIndex(Index)),
				Enum->GetDisplayNameTextByIndex(Index),
				Enum->GetToolTipTextByIndex(Index),
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateLambda([Value, OnPicked]() { OnPicked(Value); })));
		}
	}

	void FillShotsMenu(UToolMenu* Menu)
	{
		FToolMenuSection& Section = Menu->AddSection("Shots", LOCTEXT("ShotsSection", "Select subject, then counterpart"));
		AddEnumEntries<ECinemafyShotType>(Section, [](ECinemafyShotType Type) { UCinemafyEditorLibrary::CreateShotFromSelection(Type, false); });

		FToolMenuSection& Flipped = Menu->AddSection("ShotsFlipped", LOCTEXT("ShotsFlippedSection", "Other side of the line"));
		Flipped.AddSubMenu("Flipped", LOCTEXT("Flipped", "Flipped Side"), LOCTEXT("FlippedTip", "Same shots, from the other side of the line of action."),
			FNewToolMenuDelegate::CreateLambda([](UToolMenu* SubMenu)
			{
				FToolMenuSection& S = SubMenu->AddSection("Shots");
				AddEnumEntries<ECinemafyShotType>(S, [](ECinemafyShotType Type) { UCinemafyEditorLibrary::CreateShotFromSelection(Type, true); });
			}));
	}

	void FillLensMenu(UToolMenu* Menu)
	{
		FToolMenuSection& Section = Menu->AddSection("Lens", LOCTEXT("LensSection", "Prime (f/2.8)"));
		AddEnumEntries<ECinemafyPrime>(Section, [](ECinemafyPrime Prime) { UCinemafyEditorLibrary::ApplyPrimeToSelection(Prime, 2.8f); });
	}

	void FillFeelMenu(UToolMenu* Menu)
	{
		FToolMenuSection& Section = Menu->AddSection("Feel", LOCTEXT("FeelSection", "Camera Feel"));
		AddEnumEntries<ECinemafyCameraFeel>(Section, [](ECinemafyCameraFeel Feel) { UCinemafyEditorLibrary::ApplyCameraFeelToSelection(Feel); });
	}

	void FillMoveMenu(UToolMenu* Menu)
	{
		FToolMenuSection& Section = Menu->AddSection("Move", LOCTEXT("MoveSection", "Key Progress 0 -> 1 in Sequencer"));
		AddEnumEntries<ECinemafyCameraMove>(Section, [](ECinemafyCameraMove Move) { UCinemafyEditorLibrary::ApplyCameraMoveToSelection(Move); });
	}

	void FillLightMenu(UToolMenu* Menu)
	{
		FToolMenuSection& Section = Menu->AddSection("Light", LOCTEXT("LightSection", "Select a character (+ camera), or Light Rigs to restyle"));
		AddEnumEntries<ECinemafyLightingStyle>(Section, [](ECinemafyLightingStyle Style) { UCinemafyEditorLibrary::ApplyLightingStyleToSelection(Style); });
	}

	void FillLookMenu(UToolMenu* Menu)
	{
		FToolMenuSection& Section = Menu->AddSection("Look", LOCTEXT("LookSection", "Look"));
		AddEnumEntries<ECinemafyLook>(Section, [](ECinemafyLook Look) { UCinemafyEditorLibrary::ApplyLookToSelection(Look, 1.f); });

		FToolMenuSection& Letterbox = Menu->AddSection("Letterbox", LOCTEXT("LetterboxSection", "Letterbox"));
		Letterbox.AddMenuEntry("Scope", LOCTEXT("Scope", "2.39:1 Scope"), FText::GetEmpty(), FSlateIcon(),
			FUIAction(FExecuteAction::CreateLambda([]() { UCinemafyEditorLibrary::SetLetterboxOnSelection(2.39f); })));
		Letterbox.AddMenuEntry("Flat", LOCTEXT("Flat", "1.85:1 Flat"), FText::GetEmpty(), FSlateIcon(),
			FUIAction(FExecuteAction::CreateLambda([]() { UCinemafyEditorLibrary::SetLetterboxOnSelection(1.85f); })));
		Letterbox.AddMenuEntry("NoLetterbox", LOCTEXT("NoLetterbox", "None (16:9)"), FText::GetEmpty(), FSlateIcon(),
			FUIAction(FExecuteAction::CreateLambda([]() { UCinemafyEditorLibrary::SetLetterboxOnSelection(0.f); })));
	}
}

void FCinemafyEditorModule::StartupModule()
{
	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FCinemafyEditorModule::RegisterMenus));
}

void FCinemafyEditorModule::ShutdownModule()
{
	UToolMenus::UnRegisterStartupCallback(this);
	UToolMenus::UnregisterOwner(this);
}

void FCinemafyEditorModule::RegisterMenus()
{
	FToolMenuOwnerScoped OwnerScoped(this);

	UToolMenu* ToolsMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Tools");
	FToolMenuSection& Section = ToolsMenu->AddSection("Cinemafy", LOCTEXT("CinemafySection", "Cinemafy"));

	Section.AddSubMenu("CinemafyShots", LOCTEXT("ShotsMenu", "Create Shot"),
		LOCTEXT("ShotsMenuTip", "Spawn a framed, eye-focused camera. Select the subject first, then who they're talking to."),
		FNewToolMenuDelegate::CreateStatic(&FillShotsMenu));

	Section.AddSubMenu("CinemafyLens", LOCTEXT("LensMenu", "Lens Kit"),
		LOCTEXT("LensMenuTip", "Put a cinema prime on the selected cameras."),
		FNewToolMenuDelegate::CreateStatic(&FillLensMenu));

	Section.AddSubMenu("CinemafyFeel", LOCTEXT("FeelMenu", "Camera Feel"),
		LOCTEXT("FeelMenuTip", "Give the selected cameras an operator's touch."),
		FNewToolMenuDelegate::CreateStatic(&FillFeelMenu));

	Section.AddSubMenu("CinemafyMove", LOCTEXT("MoveMenu", "Camera Move"),
		LOCTEXT("MoveMenuTip", "Give the selected cameras an eased operator move: push, truck, pedestal, crane or orbit."),
		FNewToolMenuDelegate::CreateStatic(&FillMoveMenu));

	Section.AddSubMenu("CinemafyLight", LOCTEXT("LightMenu", "Light Rig"),
		LOCTEXT("LightMenuTip", "Light the selected character for the selected camera: key, fill, rim and eye light that follow them."),
		FNewToolMenuDelegate::CreateStatic(&FillLightMenu));

	Section.AddSubMenu("CinemafyLook", LOCTEXT("LookMenu", "Look & Letterbox"),
		LOCTEXT("LookMenuTip", "Grade and frame the selected cameras."),
		FNewToolMenuDelegate::CreateStatic(&FillLookMenu));

	Section.AddMenuEntry("CinemafyAddToSequence", LOCTEXT("AddToSequenceMenu", "Add Cameras to Sequence"),
		LOCTEXT("AddToSequenceMenuTip", "Add the selected cameras to the open Level Sequence and cut to them at the playhead."),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateLambda([]() { UCinemafyEditorLibrary::AddSelectedCamerasToSequence(); })));

	Section.AddMenuEntry("CinemafyLineCheck", LOCTEXT("LineCheckMenu", "Check 180-Degree Line"),
		LOCTEXT("LineCheckMenuTip", "Select two characters and your cameras. Flags cameras across the line of action."),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateLambda([]() { UCinemafyEditorLibrary::CheckLineOfActionForSelection(); })));

	Section.AddMenuEntry("CinemafyGaze", LOCTEXT("GazeMenu", "Add Gaze to Characters"),
		LOCTEXT("GazeMenuTip", "Add living eyes (saccades, head follow, blinks) to the selected characters."),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateLambda([]() { UCinemafyEditorLibrary::AddGazeToSelection(); })));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FCinemafyEditorModule, CinemafyEditor)

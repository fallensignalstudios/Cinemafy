using UnrealBuildTool;

public class CinemafyEditor : ModuleRules
{
	public CinemafyEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"CinematicCamera",
			"Cinemafy"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"UnrealEd",
			"ToolMenus",
			"LevelSequence",
			"LevelSequenceEditor",
			"MovieScene",
			"MovieSceneTracks",
			"Slate",
			"SlateCore"
		});
	}
}

using UnrealBuildTool;

public class Cinemafy : ModuleRules
{
	public Cinemafy(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"CinematicCamera",
			"LevelSequence",
			"MovieScene",
			"MovieSceneTracks"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Slate",
			"SlateCore",
			"AssetRegistry",
			"NavigationSystem",
			"MotionWarping",
			"DeveloperSettings",
			"InputCore",
			"AIModule"
		});
	}
}

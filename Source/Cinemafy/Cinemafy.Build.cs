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
			"CinematicCamera"
		});
	}
}

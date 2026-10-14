using UnrealBuildTool;

public class CinemafyAnimGraph : ModuleRules
{
	public CinemafyAnimGraph(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"AnimGraph",
			"BlueprintGraph",
			"Cinemafy"
		});
	}
}

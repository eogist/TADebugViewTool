using UnrealBuildTool;

public class TADebugViewTool : ModuleRules
{
	public TADebugViewTool(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"CoreUObject",
			"Engine",
			"UnrealEd",
			"LevelEditor",
			"Slate",
			"SlateCore",
			"ToolMenus",
			"InputCore",
			"Projects",
			"HTTP",
			"Json",
			"JsonUtilities"
		});
	}
}

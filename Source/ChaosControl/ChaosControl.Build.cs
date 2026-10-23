using UnrealBuildTool;

public class ChaosControl : ModuleRules
{
	public ChaosControl(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// Lets files in subfolders (e.g. Tests/) include the module's headers by name.
		PrivateIncludePaths.Add(ModuleDirectory);

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"AIModule",
			"RenderCore",
			"Slate",
			"SlateCore"
		});
	}
}

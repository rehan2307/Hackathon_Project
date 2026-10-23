using UnrealBuildTool;

public class ChaosControlTarget : TargetRules
{
	public ChaosControlTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("ChaosControl");
	}
}

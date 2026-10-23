using UnrealBuildTool;

public class ChaosControlEditorTarget : TargetRules
{
	public ChaosControlEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("ChaosControl");
	}
}

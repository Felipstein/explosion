using UnrealBuildTool;

public class HUDLabEditorTarget : TargetRules
{
	public HUDLabEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("HUDLab");
	}
}

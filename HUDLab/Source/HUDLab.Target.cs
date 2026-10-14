using UnrealBuildTool;

public class HUDLabTarget : TargetRules
{
	public HUDLabTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("HUDLab");
	}
}

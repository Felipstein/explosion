using UnrealBuildTool;

public class BlastLabTarget : TargetRules
{
	public BlastLabTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.AddRange(new string[] { "BlastLabShaders", "BlastLab" });
	}
}

using UnrealBuildTool;

public class RTSGameTarget : TargetRules
{
	public RTSGameTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.AddRange(new string[] { "SimCore", "BlastFx", "RTSGame" });
	}
}

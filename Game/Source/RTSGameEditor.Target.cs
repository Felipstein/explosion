using UnrealBuildTool;

public class RTSGameEditorTarget : TargetRules
{
	public RTSGameEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.AddRange(new string[] { "SimCore", "RTSShaders", "BlastFx", "RTSGame" });
	}
}

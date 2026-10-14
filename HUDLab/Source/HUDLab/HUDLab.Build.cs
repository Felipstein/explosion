using UnrealBuildTool;

// O laboratório: os estados de teste do HUD (os 5 da planta) e a foto automática.
public class HUDLab : ModuleRules
{
	public HUDLab(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "RTSHud" });
		PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });
	}
}

using UnrealBuildTool;

// As peças do mundo que o tanque derruba, no desenho (docs/WORLDFORGE.md §12.8): os fios soltos. Feito no WorldForge.
public class RTSWorldProps : ModuleRules
{
	public RTSWorldProps(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine" });
	}
}

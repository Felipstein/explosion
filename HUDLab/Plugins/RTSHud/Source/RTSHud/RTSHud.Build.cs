using UnrealBuildTool;

// O HUD do jogo como plugin: só desenho, sem saber da simulação (docs/concept/hud/DECISOES.md).
public class RTSHud : ModuleRules
{
	public RTSHud(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "SlateCore" });
		PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "RenderCore", "ImageCore" });
	}
}

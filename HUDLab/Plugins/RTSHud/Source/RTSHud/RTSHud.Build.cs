using UnrealBuildTool;

// O HUD do jogo como plugin: só desenho, sem saber da simulação (docs/concept/hud/DECISOES.md).
public class RTSHud : ModuleRules
{
	public RTSHud(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "SlateCore" });
		// Json: a árvore de poderes do general (Game/Config/RTS/GeneralTrees.json), lida pelo laboratório e pelo jogo do mesmo jeito
		PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "RenderCore", "ImageCore", "Json" });
	}
}

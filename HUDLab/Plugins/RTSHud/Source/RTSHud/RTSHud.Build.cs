using UnrealBuildTool;

// O HUD do jogo como plugin: só desenho, sem saber da simulação (docs/concept/hud/DECISOES.md).
public class RTSHud : ModuleRules
{
	public RTSHud(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		// (06/11) sem unity build: cada desenho (Painter*.cpp) tem os seus ajudantes num namespace anônimo (Hex, Lit...); juntos num
		// arquivo só eles se chocam (C2084). A Unreal só junta os que não estão mudados no git, então quebrou depois do commit a380f19
		bUseUnity = false;
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "SlateCore" });
		// Json: a árvore de poderes do general (Game/Config/RTS/GeneralTrees.json), lida pelo laboratório e pelo jogo do mesmo jeito
		PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "RenderCore", "ImageCore", "Json" });
	}
}

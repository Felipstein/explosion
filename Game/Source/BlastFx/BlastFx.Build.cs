using UnrealBuildTool;

// As explosões volumétricas do BlastLab no jogo (cópia do volume e da gravação,
// com um diretor próprio no lugar do simulador). As gravações e os materiais vêm
// do BlastLab por link físico (Game/Tools/game.ps1 worlds).
public class BlastFx : ModuleRules
{
	public BlastFx(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "RTSShaders", "RenderCore", "RHI", "Renderer" });
	}
}

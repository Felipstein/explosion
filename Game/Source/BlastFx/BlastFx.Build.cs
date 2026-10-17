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
		// os destroços da morte de tanque (BlastDebris: malha procedural) e o chão deles (BlastFxGround: o Landscape), 14/10
		PrivateDependencyModuleNames.AddRange(new string[] { "ProceduralMeshComponent", "Landscape", "RTSSunShadow", "RTSDustField" });   // (16/10 noite: a consulta do mapa do sol pela CPU, a prova da sombra; a poeira em volume, que faz sombra no chão)
	}
}

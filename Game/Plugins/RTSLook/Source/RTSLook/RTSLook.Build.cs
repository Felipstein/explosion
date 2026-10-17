using UnrealBuildTool;

// O look do jogo (sessão de gráficos): a receita de luz por ambiente e o diretor que monta
// sol, céu, névoa e pós a partir dela. Não depende do módulo do jogo: a câmera entra pela
// interface ILookCamera (docs/GRAFICOS.md §8).
public class RTSLook : ModuleRules
{
	public RTSLook(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine" });
		// (17/10) o especular da areia vai pro material do Landscape (ALandscapeProxy)
		PrivateDependencyModuleNames.AddRange(new string[] { "Landscape" });
	}
}

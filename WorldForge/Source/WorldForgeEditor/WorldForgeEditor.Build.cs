using UnrealBuildTool;

// Módulo de editor: ferramentas que o Python chama (Tools/*.py) pra montar
// mundos "de verdade" no editor a partir do gerador — Landscape da Unreal,
// camadas de material e espalhamento — que não dá pra criar em tempo de jogo.
public class WorldForgeEditor : ModuleRules
{
	public WorldForgeEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "Landscape" });
		PrivateDependencyModuleNames.AddRange(new string[] { "UnrealEd", "WorldForge" });
	}
}

using UnrealBuildTool;

// Módulo do simulador: os sistemas de explosão em tempo de execução.
public class BlastLab : ModuleRules
{
	public BlastLab(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "Slate", "SlateCore" });
		// painel em Slate e entrada do simulador
		PrivateDependencyModuleNames.AddRange(new string[] { "InputCore", "RenderCore", "RHI", "BlastLabShaders",
			"Json", "PhysicsCore",        // receitas das armas (Config/BlastRecipes.json) e o material físico do chão
			"Landscape",                 // o terreno dos campos feitos das fatias do WorldForge (BlastLabGround)
			"ProceduralMeshComponent" }); // os destroços da morte de tanque: chapas e rastros (BlastDebris)
	}
}

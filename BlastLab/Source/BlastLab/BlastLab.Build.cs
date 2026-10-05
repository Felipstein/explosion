using UnrealBuildTool;

// Módulo do simulador: os sistemas de explosão em tempo de execução.
public class BlastLab : ModuleRules
{
	public BlastLab(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine" });
		// ExplosionDemo: toca a sequência da explosão em laço na janela do simulador
		PrivateDependencyModuleNames.AddRange(new string[] { "LevelSequence", "MovieScene" });
	}
}

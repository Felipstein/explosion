using UnrealBuildTool;

// Módulo do simulador: os sistemas de explosão em tempo de execução.
public class BlastLab : ModuleRules
{
	public BlastLab(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "Slate", "SlateCore" });
		// painel em Slate e entrada do simulador
		PrivateDependencyModuleNames.AddRange(new string[] { "InputCore", "RenderCore", "RHI", "BlastLabShaders" });
	}
}

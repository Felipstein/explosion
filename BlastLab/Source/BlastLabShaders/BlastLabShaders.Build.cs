using UnrealBuildTool;

// Shaders do simulador (a fumaça do campo em compute shaders). Carrega em
// PostConfigInit, como a Unreal exige pra registrar a pasta /BlastLab dos .usf.
public class BlastLabShaders : ModuleRules
{
	public BlastLabShaders(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "RenderCore", "RHI" });
		PrivateDependencyModuleNames.AddRange(new string[] { "Projects",
			"Renderer", "Json" });       // o raymarch das explosões: o passo depois do TSR (BlastRaymarch.cpp) e o cabeçalho da gravação
	}
}

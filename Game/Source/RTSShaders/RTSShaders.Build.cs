using UnrealBuildTool;

// Shaders do jogo (a passada da névoa de guerra). Carrega em PostConfigInit, como a Unreal
// exige pra registrar a pasta /RTSGame dos .usf e os shaders globais.
public class RTSShaders : ModuleRules
{
	public RTSShaders(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine" });
		PrivateDependencyModuleNames.AddRange(new string[] { "RenderCore", "RHI", "Renderer", "Projects" });
	}
}

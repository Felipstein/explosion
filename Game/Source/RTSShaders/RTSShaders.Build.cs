using UnrealBuildTool;

// Shaders do jogo (a passada da névoa de guerra e o raymarch da Carga Pesada, cópia do BlastLab). Carrega em PostConfigInit, como a Unreal
// exige pra registrar a pasta /RTSGame dos .usf e os shaders globais.
public class RTSShaders : ModuleRules
{
	public RTSShaders(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine" });
		PublicDependencyModuleNames.AddRange(new string[] { "RenderCore", "RHI", "Renderer" });
		PrivateDependencyModuleNames.AddRange(new string[] { "Projects", "Json", "RTSSunShadowShaders", "RTSDustField" });   // a parte de desenho do mapa do sol comum (16/10, a Arte; carrega cedo como este)
	}
}

using UnrealBuildTool;

// A parte de desenho do mapa do sol, carregada cedo (PostConfigInit): a pasta dos shaders (/Plugin/RTSSunShadow, o
// SunShadow.ush), os parâmetros e o mapa que a thread de desenho vê. Quem tem shader global depende só deste; o
// subsistema que monta o mapa (UObject) fica no RTSSunShadow, que carrega depois
public class RTSSunShadowShaders : ModuleRules
{
	public RTSSunShadowShaders(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "RenderCore", "RHI" });
		PrivateDependencyModuleNames.AddRange(new string[] { "Projects" });
	}
}

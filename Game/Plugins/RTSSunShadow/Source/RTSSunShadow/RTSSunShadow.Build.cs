using UnrealBuildTool;

// O mapa de sombra do sol de todas as fumaças e poeiras (o subsistema e os parâmetros dos shaders que leem ele)
public class RTSSunShadow : ModuleRules
{
	public RTSSunShadow(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "RenderCore", "RHI", "RTSSunShadowShaders" });
		PrivateDependencyModuleNames.AddRange(new string[] { "Landscape" });
	}
}

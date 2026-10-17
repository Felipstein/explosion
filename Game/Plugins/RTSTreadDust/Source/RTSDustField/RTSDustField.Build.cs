using UnrealBuildTool;

// O campo da poeira em volume (RTSDustField.h): os shaders globais e os passes que o raymarch das explosões chama. Carrega
// cedo (PostConfigInit), como todo módulo com shader global; não tem UObject
public class RTSDustField : ModuleRules
{
	public RTSDustField(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "RenderCore", "RHI" });
		PrivateDependencyModuleNames.AddRange(new string[] { "CoreUObject", "Engine", "Renderer", "Projects", "Json", "RTSSunShadowShaders" });
	}
}

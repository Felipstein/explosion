using UnrealBuildTool;

// O laboratório de efeitos: o fogo da carcaça em cartões 6-way (FXWreckFire), a mesma
// lógica do laboratório no navegador (FXLab/web/fxfire.js), e a câmera com o painel.
public class FXLab : ModuleRules
{
	public FXLab(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore" });
		PrivateDependencyModuleNames.AddRange(new string[] { "ProceduralMeshComponent", "Slate", "SlateCore", "Json", "RHI", "RenderCore" });
	}
}

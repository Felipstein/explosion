using UnrealBuildTool;

// Módulo do gerador de mundos. O núcleo (Generation/) só depende de Core:
// é o mesmo código que o jogo vai usar pra gerar o mapa a partir da semente.
// O resto mostra o mundo gerado: terreno, água, objetos, câmera RTS e painel.
public class WorldForge : ModuleRules
{
	public WorldForge(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "RTSLook" });   // a luz do Look (Plugins/RTSLook = link pro Game/Plugins/RTSLook)
		PrivateDependencyModuleNames.AddRange(new string[] {
			"InputCore", "Slate", "SlateCore", "ProceduralMeshComponent",
			"MeshDescription", "StaticMeshDescription", "ImageWrapper", "Json", "RenderCore", "RHI",
			"Landscape"   // o A/B de parâmetro do chão na qualidade real (-ForgeLandscapeParam)
		});
	}
}

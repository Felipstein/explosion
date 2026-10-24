using UnrealBuildTool;

// O jogo na Unreal: liga a simulação (SimCore) ao desenho, à câmera, ao HUD e às telas.
// Nada daqui decide a partida; só lê o SimCore e manda comandos pra ele.
public class RTSGame : ModuleRules
{
	public RTSGame(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "SimCore", "BlastFx", "RTSLook" });
		PrivateDependencyModuleNames.AddRange(new string[] {
			"Slate", "SlateCore", "MoviePlayer", "Landscape", "Json", "Projects", "RTSShaders", "RenderCore", "RHI", "Sockets", "Networking", "ImageWrapper", "RTSHud", "RTSTreadDust", "ProceduralMeshComponent", "RTSWorldProps", "ProceduralMeshComponent", "RTSSunShadow"
		});
	}
}

using System.IO;
using UnrealBuildTool;

// A poeira das lagartas como plugin: só desenho, sem saber da simulação (a ponte manda os veículos).
public class RTSTreadDust : ModuleRules
{
	public RTSTreadDust(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "ProceduralMeshComponent" });
		PrivateDependencyModuleNames.AddRange(new string[] { "Json", "Projects" });
		// o perfil e o atlas (JSON) vão junto no jogo empacotado
		RuntimeDependencies.Add(Path.Combine(PluginDirectory, "Resources", "*.json"));
	}
}

using System.IO;
using UnrealBuildTool;

// As telas do jogo como páginas HTML no navegador embutido (CEF, o módulo WebBrowser da Unreal): o mesmo caminho das interfaces
// em HTML dos jogos grandes (o Gameface da Coherent nos jogos de Unreal, o cliente do League e o launcher da Epic no CEF).
// As páginas vão junto no pacote como arquivos soltos (Resources/UI), servidas por um esquema próprio (https://rts.ui/).
public class RTSShell : ModuleRules
{
	public RTSShell(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine" });
		PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore", "InputCore", "WebBrowser", "Projects" });
		RuntimeDependencies.Add(Path.Combine(PluginDirectory, "Resources", "UI", "..."), StagedFileType.NonUFS);
	}
}

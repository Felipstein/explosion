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
		PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore", "InputCore", "WebBrowser", "Projects", "RHI", "Json", "UMG", "RenderCore", "ApplicationCore" });
		RuntimeDependencies.Add(Path.Combine(PluginDirectory, "Resources", "UI", "..."), StagedFileType.NonUFS);
		// (06/11) as telas nativas leem do disco as fontes do mockup, as imagens do menu e da sala e os losangos dos mapas: vão no pacote
		// como arquivos soltos (antes só ia o Resources/UI das páginas, e o pacote sairia com as telas sem letra nem imagem)
		RuntimeDependencies.Add(Path.Combine(PluginDirectory, "Resources", "Fonts", "..."), StagedFileType.NonUFS);
		RuntimeDependencies.Add(Path.Combine(PluginDirectory, "Resources", "Menu", "..."), StagedFileType.NonUFS);
		RuntimeDependencies.Add(Path.Combine(PluginDirectory, "Resources", "Skirmish", "..."), StagedFileType.NonUFS);
	}
}

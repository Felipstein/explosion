using UnrealBuildTool;

// Módulo de editor: ferramentas de importação que o Python chama
// (Tools/*.py), como a criação do volume animado a partir de uma explosão
// gravada.
public class BlastLabEditor : ModuleRules
{
	public BlastLabEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine" });
		PrivateDependencyModuleNames.AddRange(new string[] { "UnrealEd", "AssetRegistry" });
	}
}

using UnrealBuildTool;

// A simulação da partida (o "juiz"): C++ puro, sem nada da Unreal além do Core
// pro registro do módulo (SimCoreModule.cpp). Os mesmos arquivos compilam fora
// da Unreal nos testes (Game/Tools/simtest.ps1).
public class SimCore : ModuleRules
{
	public SimCore(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.NoPCHs;
		bUseUnity = false;
		// sem exceções, como o resto da Unreal: o STL compila com _HAS_EXCEPTIONS=0 (VCToolChain.cs)
		// a simulação é inteira (ponto fixo), não depende disto; é só o cinto de segurança
		// caso algum float escape pra cá (docs/JOGO.md §3.2 item 2)
		FPSemantics = FPSemanticsMode.Precise;
		PublicIncludePaths.Add(ModuleDirectory + "/Public");
		PrivateDependencyModuleNames.Add("Core");
	}
}

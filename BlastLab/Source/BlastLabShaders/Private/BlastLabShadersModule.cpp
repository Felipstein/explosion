#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "ShaderCore.h"

/** Registra BlastLab/Shaders como /BlastLab (BattleSmoke.usf). */
class FBlastLabShadersModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		AddShaderSourceDirectoryMapping(TEXT("/BlastLab"),
			FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("Shaders"))));
	}
};

IMPLEMENT_MODULE(FBlastLabShadersModule, BlastLabShaders)

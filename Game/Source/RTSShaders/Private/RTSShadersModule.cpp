#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "RTSFogPass.h"
#include "ShaderCore.h"

/** Registra Game/Shaders como /RTSGame (FogOfWar.usf). */
class FRTSShadersModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		AddShaderSourceDirectoryMapping(TEXT("/RTSGame"),
			FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("Shaders"))));
	}

	virtual void ShutdownModule() override
	{
		RTSFogPass::Disable();
	}
};

IMPLEMENT_MODULE(FRTSShadersModule, RTSShaders)

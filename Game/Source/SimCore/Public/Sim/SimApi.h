#pragma once

// Exportação da DLL do SimCore. Na Unreal (editor, cada módulo é uma DLL) o
// UnrealBuildTool define SIMCORE_API como DLLEXPORT/DLLIMPORT; o SimCore não
// inclui os cabeçalhos da Unreal (é C++ puro), então os dois vêm daqui, com o
// mesmo texto da Unreal (redefinição idêntica não dá aviso). Nos testes fora da
// Unreal (Game/Tools/simtest.ps1) tudo compila junto e a marca some.
#if defined(_WIN32)
	#ifndef DLLEXPORT
		#define DLLEXPORT __declspec(dllexport)
	#endif
	#ifndef DLLIMPORT
		#define DLLIMPORT __declspec(dllimport)
	#endif
#else
	#ifndef DLLEXPORT
		#define DLLEXPORT __attribute__((visibility("default")))
	#endif
	#ifndef DLLIMPORT
		#define DLLIMPORT __attribute__((visibility("default")))
	#endif
#endif

#ifndef SIMCORE_API
	#define SIMCORE_API
#endif

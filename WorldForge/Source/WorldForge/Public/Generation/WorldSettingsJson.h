#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Generation/WorldMap.h"

/**
 * Os controles da vila no JSON dos controles (o app, a lista de mapas, o
 * "qualidade real" e o editor leem e gravam pelo mesmo par de funções):
 *   "villages": 0|1, "keepout": [[x, y, raio], ...] (m), "sand": 1|2|3 (a areia do acabamento)
 */
namespace WorldSettingsJson
{
	inline void WriteVillage(FJsonObject& J, const FWorldGenSettings& S)
	{
		J.SetNumberField(TEXT("villages"), S.Villages);
		J.SetNumberField(TEXT("relief_type"), S.ReliefType);
		J.SetNumberField(TEXT("sand"), S.Sand);
		if (S.KeepOut.Num())
		{
			TArray<TSharedPtr<FJsonValue>> List;
			for (const FVector3f& K : S.KeepOut)
			{
				List.Add(MakeShared<FJsonValueArray>(TArray<TSharedPtr<FJsonValue>>{
					MakeShared<FJsonValueNumber>(K.X), MakeShared<FJsonValueNumber>(K.Y), MakeShared<FJsonValueNumber>(K.Z) }));
			}
			J.SetArrayField(TEXT("keepout"), List);
		}
	}

	inline void ReadVillage(const FJsonObject& J, FWorldGenSettings& S)
	{
		S.Villages = J.HasField(TEXT("villages")) ? int32(J.GetNumberField(TEXT("villages"))) : 0;
		S.ReliefType = J.HasField(TEXT("relief_type")) ? int32(J.GetNumberField(TEXT("relief_type"))) : 0;
		S.Sand = J.HasField(TEXT("sand")) ? FMath::Clamp(int32(J.GetNumberField(TEXT("sand"))), 1, 3) : 3;
		S.KeepOut.Reset();
		const TArray<TSharedPtr<FJsonValue>>* List = nullptr;
		if (J.TryGetArrayField(TEXT("keepout"), List))
		{
			for (const TSharedPtr<FJsonValue>& V : *List)
			{
				const TArray<TSharedPtr<FJsonValue>>& E = V->AsArray();
				if (E.Num() == 3) S.KeepOut.Add(FVector3f(float(E[0]->AsNumber()), float(E[1]->AsNumber()), float(E[2]->AsNumber())));
			}
		}
	}

	/** Texto fixo da vila pra chave da prévia guardada. */
	inline FString VillageKey(const FWorldGenSettings& S)
	{
		FString K = FString::Printf(TEXT("v%d r%d"), S.Villages, S.ReliefType);
		if (S.Sand != 3) K += FString::Printf(TEXT(" s%d"), S.Sand);   // (a de duna, o padrão, não muda a chave dos mapas de antes)
		for (const FVector3f& P : S.KeepOut) K += FString::Printf(TEXT(";%.1f,%.1f,%.1f"), P.X, P.Y, P.Z);
		return K;
	}
}

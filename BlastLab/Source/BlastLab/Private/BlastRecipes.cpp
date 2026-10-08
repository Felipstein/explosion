#include "BlastRecipes.h"

#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	TMap<FString, FBlastRecipe> Recipes;
	TMap<FString, FBlastSurface> Surfaces;
	FDateTime LoadedStamp;
	bool bLoaded = false;

	void ReadNumber(const TSharedPtr<FJsonObject>& O, const TCHAR* Key, float& Out)
	{
		double V;
		if (O->TryGetNumberField(Key, V))
		{
			Out = float(V);
		}
	}

	void ReadInt(const TSharedPtr<FJsonObject>& O, const TCHAR* Key, int32& Out)
	{
		double V;
		if (O->TryGetNumberField(Key, V))
		{
			Out = FMath::RoundToInt32(V);
		}
	}

	void ReadRange(const TSharedPtr<FJsonObject>& O, const TCHAR* Key, FBlastRange& Out)
	{
		const TArray<TSharedPtr<FJsonValue>>* A;
		if (O->TryGetArrayField(Key, A) && A->Num() == 2)
		{
			Out.Min = float((*A)[0]->AsNumber());
			Out.Max = float((*A)[1]->AsNumber());
		}
	}

	void ReadColor(const TSharedPtr<FJsonObject>& O, const TCHAR* Key, FLinearColor& Out)
	{
		const TArray<TSharedPtr<FJsonValue>>* A;
		if (O->TryGetArrayField(Key, A) && A->Num() >= 3)
		{
			Out.R = float((*A)[0]->AsNumber());
			Out.G = float((*A)[1]->AsNumber());
			Out.B = float((*A)[2]->AsNumber());
			if (A->Num() >= 4)
			{
				Out.A = float((*A)[3]->AsNumber());
			}
		}
	}

	FBlastRecipe ParseRecipe(const TSharedPtr<FJsonObject>& O, const FBlastRecipe& Base)
	{
		FBlastRecipe R = Base;
		ReadNumber(O, TEXT("flashMs"), R.FlashMs);
		ReadNumber(O, TEXT("flashRadiusM"), R.FlashRadiusM);
		ReadNumber(O, TEXT("flashRadiance"), R.FlashRadiance);
		ReadNumber(O, TEXT("ringDelayMs"), R.RingDelayMs);
		ReadNumber(O, TEXT("ringMs"), R.RingMs);
		ReadNumber(O, TEXT("ringRadiusM"), R.RingRadiusM);
		ReadNumber(O, TEXT("ringOpacity"), R.RingOpacity);
		ReadInt(O, TEXT("debris"), R.Debris);
		ReadRange(O, TEXT("debrisSpeed"), R.DebrisSpeed);
		ReadRange(O, TEXT("debrisElevDeg"), R.DebrisElevDeg);
		ReadRange(O, TEXT("debrisSizeM"), R.DebrisSizeM);
		ReadInt(O, TEXT("clods"), R.Clods);
		ReadRange(O, TEXT("clodSpeed"), R.ClodSpeed);
		ReadRange(O, TEXT("clodElevDeg"), R.ClodElevDeg);
		ReadRange(O, TEXT("clodSizeM"), R.ClodSizeM);
		ReadInt(O, TEXT("sparks"), R.Sparks);
		ReadRange(O, TEXT("sparkSpeed"), R.SparkSpeed);
		ReadRange(O, TEXT("sparkElevDeg"), R.SparkElevDeg);
		ReadRange(O, TEXT("sparkLifeMs"), R.SparkLifeMs);
		ReadNumber(O, TEXT("terminalSpeed"), R.TerminalSpeed);
		ReadNumber(O, TEXT("restS"), R.RestS);
		ReadNumber(O, TEXT("minPixels"), R.MinPixels);
		ReadNumber(O, TEXT("scorchRadiusM"), R.ScorchRadiusM);
		ReadNumber(O, TEXT("scorchMs"), R.ScorchMs);
		ReadNumber(O, TEXT("scorchLifeS"), R.ScorchLifeS);
		return R;
	}

	FDateTime Stamp()
	{
		return IFileManager::Get().GetTimeStamp(*FBlastRecipes::FilePath());
	}

	void Load()
	{
		bLoaded = true;
		LoadedStamp = Stamp();
		FString Text;
		if (!FFileHelper::LoadFileToString(Text, *FBlastRecipes::FilePath()))
		{
			UE_LOG(LogTemp, Warning, TEXT("BlastLab: sem %s, usando as receitas padrão"), *FBlastRecipes::FilePath());
			return;
		}
		TSharedPtr<FJsonObject> Root;
		if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root.IsValid())
		{
			// arquivo no meio de uma edição: fica a última receita boa
			UE_LOG(LogTemp, Warning, TEXT("BlastLab: %s com erro de JSON, mantendo as receitas anteriores"), *FBlastRecipes::FilePath());
			return;
		}
		TMap<FString, FBlastRecipe> NewRecipes;
		const FBlastRecipe Base;
		const TSharedPtr<FJsonObject>* Weapons;
		if (Root->TryGetObjectField(TEXT("weapons"), Weapons))
		{
			for (const auto& It : (*Weapons)->Values)
			{
				const TSharedPtr<FJsonObject>* O;
				if (It.Value->TryGetObject(O))
				{
					NewRecipes.Add(FString(It.Key), ParseRecipe(*O, Base));
				}
			}
		}
		TMap<FString, FBlastSurface> NewSurfaces;
		const TSharedPtr<FJsonObject>* Grounds;
		if (Root->TryGetObjectField(TEXT("surfaces"), Grounds))
		{
			for (const auto& It : (*Grounds)->Values)
			{
				const TSharedPtr<FJsonObject>* O;
				if (It.Value->TryGetObject(O))
				{
					FBlastSurface S;
					ReadColor(*O, TEXT("clodColor"), S.ClodColor);
					ReadColor(*O, TEXT("ringColor"), S.RingColor);
					ReadColor(*O, TEXT("scorchColor"), S.ScorchColor);
					ReadNumber(*O, TEXT("clods"), S.Clods);
					ReadNumber(*O, TEXT("ring"), S.Ring);
					NewSurfaces.Add(FString(It.Key), S);
				}
			}
		}
		Recipes = MoveTemp(NewRecipes);
		Surfaces = MoveTemp(NewSurfaces);
		UE_LOG(LogTemp, Display, TEXT("BlastLab: receitas lidas (%d armas, %d chãos)"), Recipes.Num(), Surfaces.Num());
	}
}

FString FBlastRecipes::FilePath()
{
	return FPaths::ConvertRelativePathToFull(FPaths::ProjectConfigDir() / TEXT("BlastRecipes.json"));
}

bool FBlastRecipes::Poll()
{
	if (!bLoaded || Stamp() != LoadedStamp)
	{
		Load();
		return true;
	}
	return false;
}

const FBlastRecipe& FBlastRecipes::Get(const FString& Weapon)
{
	if (!bLoaded)
	{
		Load();
	}
	static const FBlastRecipe Default;
	const FBlastRecipe* R = Recipes.Find(Weapon);
	return R ? *R : Default;
}

const FBlastSurface& FBlastRecipes::Surface(const FString& PhysicalMaterialName)
{
	if (!bLoaded)
	{
		Load();
	}
	FString Key = PhysicalMaterialName;
	Key.RemoveFromStart(TEXT("PM_"));
	Key = Key.ToLower();
	if (const FBlastSurface* S = Surfaces.Find(Key))
	{
		return *S;
	}
	static const FBlastSurface Default;
	const FBlastSurface* Earth = Surfaces.Find(TEXT("terra"));
	return Earth ? *Earth : Default;
}

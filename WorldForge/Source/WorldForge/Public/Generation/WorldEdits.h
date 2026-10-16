#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

/**
 * As edições à mão de um mapa, por cima do que o gerador faz (plano em
 * docs/WORLDFORGE.md §11). O arquivo do mapa guarda o que entra no gerador
 * (semente, controles) e estes deltas; o gerado não é guardado.
 *
 * Objeto apagado: o tipo do espalhamento e o ponto sorteado (m). Ao gerar de
 * novo, o mesmo sorteio cai no mesmo ponto e é pulado; a busca tem tolerância
 * de 10 cm (como o Data Override do PCG da Unreal 5.8, chave espacial). Se os
 * controles mudarem, o ponto pode não existir mais: a edição fica "sem alvo"
 * (contada, nunca descartada calada).
 */
struct FWorldEdits
{
	struct FDeleted
	{
		FString Kind;
		FVector2f P = FVector2f::ZeroVector;
	};
	TArray<FDeleted> Deleted;

	/** Pintura do chão: por célula de 1 m (o canto, em m), a camada (0–5, na
	 *  ordem das camadas do bioma) e quanto (0–255). Entra por cima dos pesos
	 *  gerados (FWorldDetail::DesertWeights), então vale pra vista, pro sorteio
	 *  do espalhamento e pro Landscape do "qualidade real". */
	struct FPaint
	{
		uint8 Layer = 0;
		uint8 Amount = 0;
	};
	TMap<FIntPoint, FPaint> Paint;

	/** Altura à mão: o desvio (m) somado ao relevo do gerador, por vértice da
	 *  grade do mapa (I, J; 2 m). Entra antes de tudo (FWorldDetail::Setup):
	 *  encosta, rocha, água e estrada se ajustam juntas. */
	TMap<FIntPoint, float> Height;

	/** Estrada à mão: os pontos de controle (m) e o tipo. Entra na rede do
	 *  gerador (FWorldDetail::BuildRoads), que escava, aterra e entronca. */
	struct FRoadEdit
	{
		TArray<FVector2f> Points;
		bool bPaved = false;
		/** A ponta solta se desfaz devagar, como a do gerador que entra na base (a
		 *  que veio de uma estrada do gerador guarda isso); senão termina em ~4 m. */
		bool bFade[2] = { false, false };
		/** O acabamento do asfalto (16/10, o Felipe: a laje aprovada "mas lembre-se que tinha mais de uma variação" e
		 *  "também não descartei a original que é colada no chão"): 0 automático (o desgaste por trecho), 1 colada no
		 *  chão (sem laje), 2 inteira, 3 gasta, 4 destruída. FWorldDetail::FRoad::Style. */
		int32 Style = 0;
	};
	TArray<FRoadEdit> Roads;
	/** Estradas do gerador tiradas à mão: um ponto do traçado (m); a estrada do
	 *  gerador que passa a até 3 m dele sai. */
	TArray<FVector2f> RemovedRoads;

	/** Peça movida à mão: o tipo e o ponto sorteado (a mesma chave do apagado),
	 *  o lugar novo (m) e o giro somado (graus). O sorteio põe a peça no lugar
	 *  novo e assenta de novo (altura, inclinação) com as regras dela. */
	struct FMoved
	{
		FString Kind;
		FVector2f From = FVector2f::ZeroVector;
		FVector2f To = FVector2f::ZeroVector;
		float Yaw = 0.0f;
	};
	TArray<FMoved> Moved;

	bool IsEmpty() const { return Deleted.Num() == 0 && Paint.Num() == 0 && Height.Num() == 0 && Roads.Num() == 0 && RemovedRoads.Num() == 0 && Moved.Num() == 0; }

	/** Esta peça foi movida? (mesmo tipo, ponto sorteado a até 10 cm) */
	const FMoved* FindMoved(const FString& Kind, FVector2f From) const
	{
		const int32 I = MovedIndex(Kind, From);
		return I != INDEX_NONE ? &Moved[I] : nullptr;
	}
	int32 MovedIndex(const FString& Kind, FVector2f From) const
	{
		if (Moved.Num() == 0)
		{
			return INDEX_NONE;
		}
		const FIntPoint C = CellOf(From);
		for (int32 DJ = -1; DJ <= 1; ++DJ)
		{
			for (int32 DI = -1; DI <= 1; ++DI)
			{
				TArray<int32> Found;
				MoveGrid.MultiFind(FIntPoint(C.X + DI, C.Y + DJ), Found);
				for (const int32 I : Found)
				{
					if (FVector2f::DistSquared(Moved[I].From, From) < 0.01f && Moved[I].Kind == Kind) return I;
				}
			}
		}
		return INDEX_NONE;
	}
	/** Grava o lugar novo (troca o que já havia da mesma peça). */
	void SetMoved(const FMoved& M)
	{
		const int32 I = MovedIndex(M.Kind, M.From);
		if (I != INDEX_NONE) { Moved[I] = M; return; }
		Moved.Add(M);
		MoveGrid.Add(CellOf(M.From), Moved.Num() - 1);
	}
	void RemoveMoved(const FString& Kind, FVector2f From)
	{
		const int32 I = MovedIndex(Kind, From);
		if (I != INDEX_NONE)
		{
			Moved.RemoveAt(I);
			Reindex();
		}
	}

	/** Quanto (0–1) e qual camada a pintura pede em P (bilinear entre as células da mesma camada). */
	float PaintAt(FVector2f P, int32& OutLayer) const
	{
		OutLayer = -1;
		if (Paint.Num() == 0)
		{
			return 0.0f;
		}
		const float X = P.X - 0.5f, Y = P.Y - 0.5f;
		const int32 X0 = FMath::FloorToInt(X), Y0 = FMath::FloorToInt(Y);
		const float FX = X - X0, FY = Y - Y0;
		const FPaint* C[4] = { Paint.Find(FIntPoint(X0, Y0)), Paint.Find(FIntPoint(X0 + 1, Y0)), Paint.Find(FIntPoint(X0, Y0 + 1)), Paint.Find(FIntPoint(X0 + 1, Y0 + 1)) };
		// a camada da célula mais forte manda; as outras entram só se forem a mesma
		int32 Layer = -1, BestA = 0;
		for (const FPaint* K : C) if (K && K->Amount > BestA) { BestA = K->Amount; Layer = K->Layer; }
		if (Layer < 0)
		{
			return 0.0f;
		}
		auto A = [Layer](const FPaint* K) { return K && K->Layer == Layer ? K->Amount / 255.0f : 0.0f; };
		OutLayer = Layer;
		return FMath::Lerp(FMath::Lerp(A(C[0]), A(C[1]), FX), FMath::Lerp(A(C[2]), A(C[3]), FX), FY);
	}

	/** Pincelada: disco de raio R (m) na camada, com a borda suave; guarda o que
	 *  havia antes (pra desfazer) em OutPrev (só a primeira vez de cada célula). */
	void PaintDisc(FVector2f Center, float R, int32 Layer, float Strength, TMap<FIntPoint, FPaint>& OutPrev)
	{
		const int32 X0 = FMath::FloorToInt(Center.X - R), X1 = FMath::FloorToInt(Center.X + R);
		const int32 Y0 = FMath::FloorToInt(Center.Y - R), Y1 = FMath::FloorToInt(Center.Y + R);
		for (int32 Y = Y0; Y <= Y1; ++Y)
		{
			for (int32 X = X0; X <= X1; ++X)
			{
				const float D = FVector2f::Distance(FVector2f(X + 0.5f, Y + 0.5f), Center);
				const float W = Strength * (1.0f - FMath::SmoothStep(0.6f * R, R, D));
				if (W <= 0.004f) continue;
				const FIntPoint Key(X, Y);
				FPaint* Cur = Paint.Find(Key);
				if (!OutPrev.Contains(Key)) OutPrev.Add(Key, Cur ? *Cur : FPaint());
				const uint8 Want = uint8(FMath::Clamp(W, 0.0f, 1.0f) * 255.0f + 0.5f);
				if (!Cur) { Paint.Add(Key, { uint8(Layer), Want }); continue; }
				if (Cur->Layer == Layer) Cur->Amount = FMath::Max(Cur->Amount, Want);
				else if (Want >= Cur->Amount) *Cur = { uint8(Layer), Want };
			}
		}
	}

	/** Volta as células de uma pincelada (desfazer). */
	void Unpaint(const TMap<FIntPoint, FPaint>& Prev)
	{
		for (const TPair<FIntPoint, FPaint>& K : Prev)
		{
			if (K.Value.Amount == 0) Paint.Remove(K.Key);
			else Paint.Add(K.Key, K.Value);
		}
	}

	/** Refaz o índice espacial (depois de mudar Deleted). */
	void Reindex()
	{
		Grid.Reset();
		for (int32 I = 0; I < Deleted.Num(); ++I)
		{
			Grid.Add(CellOf(Deleted[I].P), I);
		}
		MoveGrid.Reset();
		for (int32 I = 0; I < Moved.Num(); ++I)
		{
			MoveGrid.Add(CellOf(Moved[I].From), I);
		}
	}

	/** Este sorteio foi apagado à mão? (mesmo tipo, a até 10 cm) */
	bool IsDeleted(const FString& Kind, FVector2f P) const
	{
		if (Deleted.Num() == 0)
		{
			return false;
		}
		const FIntPoint C = CellOf(P);
		for (int32 DJ = -1; DJ <= 1; ++DJ)
		{
			for (int32 DI = -1; DI <= 1; ++DI)
			{
				TArray<int32> Found;
				Grid.MultiFind(FIntPoint(C.X + DI, C.Y + DJ), Found);
				for (const int32 I : Found)
				{
					if (FVector2f::DistSquared(Deleted[I].P, P) < 0.01f && Deleted[I].Kind == Kind)
					{
						return true;
					}
				}
			}
		}
		return false;
	}

	void Add(const FString& Kind, FVector2f P)
	{
		Deleted.Add({ Kind, P });
		Grid.Add(CellOf(P), Deleted.Num() - 1);
	}

	void RemoveLast()
	{
		if (Deleted.Num())
		{
			Deleted.Pop();
			Reindex();
		}
	}

	/** JSON: {"deleted": [[kind, x, y], ...]} */
	FString ToJson() const
	{
		FString S = TEXT("{\n \"deleted\": [");
		for (int32 I = 0; I < Deleted.Num(); ++I)
		{
			S += FString::Printf(TEXT("%s\n  [\"%s\", %.3f, %.3f]"), I ? TEXT(",") : TEXT(""), *Deleted[I].Kind, Deleted[I].P.X, Deleted[I].P.Y);
		}
		S += TEXT("\n ],\n \"paint\": [");
		int32 N = 0;
		for (const TPair<FIntPoint, FPaint>& K : Paint)
		{
			S += FString::Printf(TEXT("%s\n  [%d, %d, %d, %d]"), N++ ? TEXT(",") : TEXT(""), K.Key.X, K.Key.Y, K.Value.Layer, K.Value.Amount);
		}
		S += TEXT("\n ],\n \"height\": [");
		N = 0;
		for (const TPair<FIntPoint, float>& K : Height)
		{
			S += FString::Printf(TEXT("%s\n  [%d, %d, %.3f]"), N++ ? TEXT(",") : TEXT(""), K.Key.X, K.Key.Y, K.Value);
		}
		S += TEXT("\n ],\n \"roads\": [");
		for (int32 R = 0; R < Roads.Num(); ++R)
		{
			S += FString::Printf(TEXT("%s\n  {\"paved\": %s, \"style\": %d, \"fade\": [%s, %s], \"points\": ["), R ? TEXT(",") : TEXT(""), Roads[R].bPaved ? TEXT("true") : TEXT("false"),
				Roads[R].Style, Roads[R].bFade[0] ? TEXT("true") : TEXT("false"), Roads[R].bFade[1] ? TEXT("true") : TEXT("false"));
			for (int32 K = 0; K < Roads[R].Points.Num(); ++K)
			{
				S += FString::Printf(TEXT("%s[%.2f, %.2f]"), K ? TEXT(", ") : TEXT(""), Roads[R].Points[K].X, Roads[R].Points[K].Y);
			}
			S += TEXT("]}");
		}
		S += TEXT("\n ],\n \"removed_roads\": [");
		for (int32 R = 0; R < RemovedRoads.Num(); ++R)
		{
			S += FString::Printf(TEXT("%s[%.2f, %.2f]"), R ? TEXT(", ") : TEXT(""), RemovedRoads[R].X, RemovedRoads[R].Y);
		}
		S += TEXT("],\n \"moved\": [");
		for (int32 I = 0; I < Moved.Num(); ++I)
		{
			const FMoved& M = Moved[I];
			S += FString::Printf(TEXT("%s\n  [\"%s\", %.3f, %.3f, %.3f, %.3f, %.1f]"), I ? TEXT(",") : TEXT(""), *M.Kind, M.From.X, M.From.Y, M.To.X, M.To.Y, M.Yaw);
		}
		S += TEXT("\n ]\n}\n");
		return S;
	}

	/** Lê o JSON de ToJson (falso se não der). */
	bool FromJson(const FString& Text)
	{
		TSharedPtr<FJsonObject> J;
		if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), J) || !J.IsValid())
		{
			return false;
		}
		Deleted.Reset();
		const TArray<TSharedPtr<FJsonValue>>* List = nullptr;
		if (J->TryGetArrayField(TEXT("deleted"), List))
		{
			for (const TSharedPtr<FJsonValue>& V : *List)
			{
				const TArray<TSharedPtr<FJsonValue>>& E = V->AsArray();
				if (E.Num() == 3)
				{
					Deleted.Add({ E[0]->AsString(), FVector2f(float(E[1]->AsNumber()), float(E[2]->AsNumber())) });
				}
			}
		}
		Paint.Reset();
		if (J->TryGetArrayField(TEXT("paint"), List))
		{
			for (const TSharedPtr<FJsonValue>& V : *List)
			{
				const TArray<TSharedPtr<FJsonValue>>& E = V->AsArray();
				if (E.Num() == 4)
				{
					Paint.Add(FIntPoint(int32(E[0]->AsNumber()), int32(E[1]->AsNumber())), { uint8(E[2]->AsNumber()), uint8(E[3]->AsNumber()) });
				}
			}
		}
		Height.Reset();
		if (J->TryGetArrayField(TEXT("height"), List))
		{
			for (const TSharedPtr<FJsonValue>& V : *List)
			{
				const TArray<TSharedPtr<FJsonValue>>& E = V->AsArray();
				if (E.Num() == 3)
				{
					Height.Add(FIntPoint(int32(E[0]->AsNumber()), int32(E[1]->AsNumber())), float(E[2]->AsNumber()));
				}
			}
		}
		Roads.Reset();
		if (J->TryGetArrayField(TEXT("roads"), List))
		{
			for (const TSharedPtr<FJsonValue>& V : *List)
			{
				const TSharedPtr<FJsonObject> O = V->AsObject();
				FRoadEdit& R = Roads.AddDefaulted_GetRef();
				R.bPaved = O->GetBoolField(TEXT("paved"));
				R.Style = O->HasField(TEXT("style")) ? FMath::Clamp(int32(O->GetNumberField(TEXT("style"))), 0, 4) : 0;
				const TArray<TSharedPtr<FJsonValue>>* Fade;
				if (O->TryGetArrayField(TEXT("fade"), Fade) && Fade->Num() == 2)
				{
					R.bFade[0] = (*Fade)[0]->AsBool();
					R.bFade[1] = (*Fade)[1]->AsBool();
				}
				for (const TSharedPtr<FJsonValue>& Pt : O->GetArrayField(TEXT("points")))
				{
					const TArray<TSharedPtr<FJsonValue>>& E = Pt->AsArray();
					if (E.Num() == 2) R.Points.Add(FVector2f(float(E[0]->AsNumber()), float(E[1]->AsNumber())));
				}
			}
		}
		Moved.Reset();
		if (J->TryGetArrayField(TEXT("moved"), List))
		{
			for (const TSharedPtr<FJsonValue>& V : *List)
			{
				const TArray<TSharedPtr<FJsonValue>>& E = V->AsArray();
				if (E.Num() == 6)
				{
					Moved.Add({ E[0]->AsString(), FVector2f(float(E[1]->AsNumber()), float(E[2]->AsNumber())),
						FVector2f(float(E[3]->AsNumber()), float(E[4]->AsNumber())), float(E[5]->AsNumber()) });
				}
			}
		}
		RemovedRoads.Reset();
		if (J->TryGetArrayField(TEXT("removed_roads"), List))
		{
			for (const TSharedPtr<FJsonValue>& V : *List)
			{
				const TArray<TSharedPtr<FJsonValue>>& E = V->AsArray();
				if (E.Num() == 2) RemovedRoads.Add(FVector2f(float(E[0]->AsNumber()), float(E[1]->AsNumber())));
			}
		}
		Reindex();
		return true;
	}

private:
	static FIntPoint CellOf(FVector2f P) { return FIntPoint(FMath::FloorToInt(P.X), FMath::FloorToInt(P.Y)); }
	TMultiMap<FIntPoint, int32> Grid;
	TMultiMap<FIntPoint, int32> MoveGrid;
};

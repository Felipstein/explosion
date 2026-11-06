#include "RTSHudTypes.h"

#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

// (04/11) A árvore de poderes do general: o arquivo que docs/poderes/tela/export_game.py gera dos .js da sessão das superarmas.

bool FRTSHudGeneral::LoadTree(const FString& JsonPath, const FString& Faction)
{
	FString Text;
	TSharedPtr<FJsonObject> Root;
	if (!FFileHelper::LoadFileToString(Text, *JsonPath) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("RTSHud: a árvore do general não abriu: %s"), *JsonPath);
		return false;
	}
	const TArray<TSharedPtr<FJsonValue>>* Pts = nullptr;
	if (Root->TryGetArrayField(TEXT("points_per_rank"), Pts))
	{
		for (int32 I = 0; I < 5 && I < Pts->Num(); ++I) PointsPerRank[I] = int32((*Pts)[I]->AsNumber());
	}
	const TSharedPtr<FJsonObject>* Facs = nullptr;
	const TSharedPtr<FJsonObject>* Fac = nullptr;
	if (!Root->TryGetObjectField(TEXT("factions"), Facs) || !((*Facs)->TryGetObjectField(Faction, Fac) || (*Facs)->TryGetObjectField(TEXT("usa"), Fac)))
	{
		return false;
	}
	Rows = int32((*Fac)->GetNumberField(TEXT("rows")));
	const TArray<TSharedPtr<FJsonValue>>* Ranks = nullptr;
	if ((*Fac)->TryGetArrayField(TEXT("ranks"), Ranks))
	{
		for (int32 I = 0; I < 5 && I < Ranks->Num(); ++I)
		{
			const TArray<TSharedPtr<FJsonValue>>& Pair = (*Ranks)[I]->AsArray();
			RankAbbr[I] = Pair.Num() > 0 ? Pair[0]->AsString() : FString();
			RankName[I] = Pair.Num() > 1 ? Pair[1]->AsString() : FString();
		}
	}
	Nodes.Reset();
	const TArray<TSharedPtr<FJsonValue>>* List = nullptr;
	if ((*Fac)->TryGetArrayField(TEXT("nodes"), List))
	{
		for (const TSharedPtr<FJsonValue>& V : *List)
		{
			const TSharedPtr<FJsonObject> O = V->AsObject();
			FRTSHudGenNode N;
			N.Id = FName(*O->GetStringField(TEXT("id")));
			N.Name = O->GetStringField(TEXT("name"));
			N.Level = O->GetStringField(TEXT("level"));
			N.Icon = FName(*O->GetStringField(TEXT("icon")));
			N.Rank = FMath::Clamp(int32(O->GetNumberField(TEXT("rank"))), 1, 5);
			N.Row = int32(O->GetNumberField(TEXT("row")));
			N.Cost = int32(O->GetNumberField(TEXT("cost")));
			N.Cooldown = O->GetStringField(TEXT("cooldown"));
			N.Desc = O->GetStringField(TEXT("desc"));
			const FString Req = O->GetStringField(TEXT("req"));
			N.Req = Req.IsEmpty() ? NAME_None : FName(*Req);
			Rows = FMath::Max(Rows, N.Row + 1);
			Nodes.Add(N);
		}
	}
	return Nodes.Num() > 0;
}

int32 FRTSHudGeneral::PointsLeftFor(int32 InRank) const
{
	int32 Sum = 0;
	for (int32 I = 0; I < FMath::Clamp(InRank, 0, 5); ++I) Sum += PointsPerRank[I];
	for (const FRTSHudGenNode& N : Nodes)
	{
		if (Owned.Contains(N.Id)) Sum -= N.Cost;
	}
	return FMath::Max(0, Sum);
}

bool FRTSHudGeneral::CanBuy(FName Id) const
{
	const FRTSHudGenNode* N = Nodes.FindByPredicate([&](const FRTSHudGenNode& X) { return X.Id == Id; });
	return N && !Owned.Contains(Id) && !Soon.Contains(Id) && Rank >= N->Rank && (N->Req.IsNone() || Owned.Contains(N->Req)) && PointsLeftFor(Rank) >= N->Cost;
}

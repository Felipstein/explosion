#include "RTSHudKit.h"

#include "Engine/Texture2D.h"
#include "HAL/FileManager.h"
#include "ImageCore.h"
#include "ImageUtils.h"
#include "TextureResource.h"

FRTSHudKit::FRTSHudKit(const FString& InKitDir, const FString& InExtraDir)
	: KitDir(InKitDir), ExtraDir(InExtraDir)
{
}

FString FRTSHudKit::Resolve(FName Name) const
{
	const FString S = Name.ToString();
	FString Prefix, Rest;
	if (!S.Split(TEXT(":"), &Prefix, &Rest))
	{
		Prefix = S;
	}
	if (Prefix == TEXT("skin")) return KitDir / TEXT("skin") / Rest + TEXT(".png");
	if (Prefix == TEXT("icon")) return KitDir / TEXT("icons") / Rest + TEXT(".png");
	if (Prefix == TEXT("btn")) return KitDir / TEXT("photos/buttons") / Rest + TEXT(".png");
	if (Prefix == TEXT("card")) return KitDir / TEXT("photos/card") / Rest + TEXT(".png");
	if (Prefix == TEXT("emblem")) return ExtraDir / FString::Printf(TEXT("emblem_%s.png"), *Rest);
	if (Prefix == TEXT("minimap")) return ExtraDir / TEXT("minimap.png");
	if (Prefix == TEXT("bg")) return ExtraDir / TEXT("background.png");
	return FString();
}

UTexture2D* FRTSHudKit::Get(FName Name)
{
	if (Name.IsNone())
	{
		return nullptr;
	}
	if (const TStrongObjectPtr<UTexture2D>* Found = Cache.Find(Name))
	{
		return Found->Get();
	}
	if (Missing.Contains(Name))
	{
		return nullptr;
	}
	const FString Path = Resolve(Name);
	UTexture2D* Tex = (!Path.IsEmpty() && IFileManager::Get().FileExists(*Path)) ? LoadPngWithMips(Path) : nullptr;
	if (!Tex)
	{
		UE_LOG(LogTemp, Warning, TEXT("RTSHud: imagem que falta: %s (%s)"), *Name.ToString(), *Path);
		Missing.Add(Name);
		return nullptr;
	}
	Cache.Add(Name, TStrongObjectPtr<UTexture2D>(Tex));
	return Tex;
}

// O PNG com a cadeia de mipmaps: a pele do HUD sai bem menor que a textura (uma moldura de 198 px vira
// um botão de 86), e sem mips a redução serrilha. A média de cada 2×2 pesa a cor pelo alfa, pra borda
// recortada não puxar a cor do fundo transparente.
UTexture2D* FRTSHudKit::LoadPngWithMips(const FString& Path)
{
	FImage Loaded;
	if (!FImageUtils::LoadImage(*Path, Loaded))
	{
		return nullptr;
	}
	FImage Img;
	Loaded.CopyTo(Img, ERawImageFormat::BGRA8, EGammaSpace::sRGB);
	TArray<TArray64<FColor>> Levels;
	{
		const TArrayView64<FColor> View = Img.AsBGRA8();
		Levels.Emplace(View.GetData(), View.Num());
	}
	int32 W = Img.SizeX, H = Img.SizeY;
	while (W > 1 || H > 1)
	{
		const int32 NW = FMath::Max(1, W / 2), NH = FMath::Max(1, H / 2);
		TArray64<FColor> Next;
		Next.SetNumUninitialized(int64(NW) * NH);
		const TArray64<FColor>& Prev = Levels.Last();
		for (int32 Y = 0; Y < NH; ++Y)
		{
			for (int32 X = 0; X < NW; ++X)
			{
				float R = 0, G = 0, B = 0, A = 0, Rs = 0, Gs = 0, Bs = 0;
				for (int32 K = 0; K < 4; ++K)
				{
					const int32 SXp = FMath::Min(2 * X + (K & 1), W - 1), SYp = FMath::Min(2 * Y + (K >> 1), H - 1);
					const FColor& C = Prev[int64(SYp) * W + SXp];
					R += C.R * C.A; G += C.G * C.A; B += C.B * C.A; A += C.A;
					Rs += C.R; Gs += C.G; Bs += C.B;
				}
				Next[int64(Y) * NW + X] = A > 0.0f
					? FColor(uint8(R / A + 0.5f), uint8(G / A + 0.5f), uint8(B / A + 0.5f), uint8(A / 4 + 0.5f))
					: FColor(uint8(Rs / 4), uint8(Gs / 4), uint8(Bs / 4), 0);
			}
		}
		Levels.Add(MoveTemp(Next));
		W = NW; H = NH;
	}

	UTexture2D* Tex = UTexture2D::CreateTransient(Img.SizeX, Img.SizeY, PF_B8G8R8A8);
	if (!Tex)
	{
		return nullptr;
	}
	FTexturePlatformData* PD = Tex->GetPlatformData();
	{
		FTexture2DMipMap& M = PD->Mips[0];
		void* D = M.BulkData.Lock(LOCK_READ_WRITE);
		FMemory::Memcpy(D, Levels[0].GetData(), Levels[0].Num() * sizeof(FColor));
		M.BulkData.Unlock();
	}
	W = Img.SizeX; H = Img.SizeY;
	for (int32 L = 1; L < Levels.Num(); ++L)
	{
		W = FMath::Max(1, W / 2); H = FMath::Max(1, H / 2);
		FTexture2DMipMap* M = new FTexture2DMipMap();
		M->SizeX = W; M->SizeY = H; M->SizeZ = 1;
		PD->Mips.Add(M);
		M->BulkData.Lock(LOCK_READ_WRITE);
		void* D = M->BulkData.Realloc(Levels[L].Num() * sizeof(FColor));
		FMemory::Memcpy(D, Levels[L].GetData(), Levels[L].Num() * sizeof(FColor));
		M->BulkData.Unlock();
	}
	Tex->SRGB = true;
	Tex->Filter = TF_Trilinear;
	Tex->NeverStream = true;
	Tex->UpdateResource();
	return Tex;
}

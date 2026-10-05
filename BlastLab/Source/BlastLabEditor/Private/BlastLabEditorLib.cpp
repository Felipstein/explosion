#include "BlastLabEditorLib.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "SparseVolumeTexture/SparseVolumeTexture.h"
#include "SparseVolumeTexture/SparseVolumeTextureData.h"
#include "Engine/VolumeTexture.h"
#include "UObject/Package.h"

UAnimatedSparseVolumeTexture* UBlastLabEditorLib::CreateAnimatedVolume(const FString& Folder, const FString& AssetPath,
	FIntVector Resolution, int32 NumFrames, float WorldSizeCm, FString& Error)
{
	const int64 NumVoxels = int64(Resolution.X) * Resolution.Y * Resolution.Z;
	if (NumVoxels <= 0 || NumFrames <= 0)
	{
		Error = TEXT("resolução ou número de quadros inválido");
		return nullptr;
	}
	if (FindPackage(nullptr, *AssetPath) || FPackageName::DoesPackageExist(AssetPath))
	{
		Error = TEXT("o asset já existe: apague antes");
		return nullptr;
	}

	UPackage* Package = CreatePackage(*AssetPath);
	const FString Name = FPackageName::GetLongPackageAssetName(AssetPath);
	UAnimatedSparseVolumeTexture* Volume = NewObject<UAnimatedSparseVolumeTexture>(Package, *Name, RF_Public | RF_Standalone);
	if (!Volume->BeginInitialize(NumFrames))
	{
		Error = TEXT("BeginInitialize falhou");
		return nullptr;
	}

	// 1 voxel = tamanho/resolução cm: o volume aparece no tamanho real
	FTransform FrameTransform = FTransform::Identity;
	FrameTransform.SetScale3D(FVector(WorldSizeCm) / FVector(Resolution));

	TArray64<uint8> A, B;
	for (int32 Frame = 0; Frame < NumFrames; ++Frame)
	{
		const FString FileA = FPaths::Combine(Folder, FString::Printf(TEXT("f%03d_a.bin"), Frame));
		const FString FileB = FPaths::Combine(Folder, FString::Printf(TEXT("f%03d_b.bin"), Frame));
		if (!FFileHelper::LoadFileToArray(A, *FileA) || !FFileHelper::LoadFileToArray(B, *FileB)
			|| A.Num() != NumVoxels * 4 || B.Num() != NumVoxels * 8)
		{
			Error = FString::Printf(TEXT("quadro %d ausente ou com tamanho errado (%s)"), Frame, *FileA);
			return nullptr;
		}

		// A: fuligem, temperatura, poeira (códigos do WebGL), RGBA8 · B: combustível +
		// velocidade em m/s, meia precisão (o zero tem que ser parado de verdade).
		// Fundo zero: blocos inteiros vazios não são guardados (o volume fica esparso).
		UE::SVT::FTextureDataCreateInfo Info;
		Info.VirtualVolumeAABBMin = FIntVector3::ZeroValue;
		Info.VirtualVolumeAABBMax = FIntVector3(Resolution.X, Resolution.Y, Resolution.Z);
		Info.AttributesFormats[0] = PF_R8G8B8A8;
		Info.AttributesFormats[1] = PF_FloatRGBA;
		Info.FallbackValues[0] = FVector4f(0, 0, 0, 0);
		Info.FallbackValues[1] = FVector4f(0, 0, 0, 0);

		UE::SVT::FTextureData Data{};
		if (!Data.CreateFromDense(Info, TArrayView64<uint8>(A.GetData(), A.Num()), TArrayView64<uint8>(B.GetData(), B.Num())))
		{
			Error = FString::Printf(TEXT("CreateFromDense falhou no quadro %d"), Frame);
			return nullptr;
		}
		if (!Volume->AppendFrame(Data, FrameTransform))
		{
			Error = FString::Printf(TEXT("AppendFrame falhou no quadro %d"), Frame);
			return nullptr;
		}
	}

	if (!Volume->EndInitialize())
	{
		Error = TEXT("EndInitialize falhou");
		return nullptr;
	}
	Volume->ConditionalPostLoad();
	FAssetRegistryModule::AssetCreated(Volume);
	Package->MarkPackageDirty();
	return Volume;
}

UVolumeTexture* UBlastLabEditorLib::CreateVolumeTexture(const FString& File, const FString& AssetPath,
	FIntVector Resolution, FString& Error)
{
	const int64 NumVoxels = int64(Resolution.X) * Resolution.Y * Resolution.Z;
	TArray64<uint8> Data;
	if (NumVoxels <= 0 || !FFileHelper::LoadFileToArray(Data, *File) || Data.Num() != NumVoxels * 4)
	{
		Error = FString::Printf(TEXT("arquivo ausente ou com tamanho errado: %s"), *File);
		return nullptr;
	}
	if (FindPackage(nullptr, *AssetPath) || FPackageName::DoesPackageExist(AssetPath))
	{
		Error = TEXT("o asset já existe: apague antes");
		return nullptr;
	}

	UPackage* Package = CreatePackage(*AssetPath);
	const FString Name = FPackageName::GetLongPackageAssetName(AssetPath);
	UVolumeTexture* Texture = NewObject<UVolumeTexture>(Package, *Name, RF_Public | RF_Standalone);
	Texture->Source.Init(Resolution.X, Resolution.Y, Resolution.Z, 1, TSF_BGRA8, Data.GetData());
	Texture->SRGB = false;                                        // dado, não cor
	Texture->CompressionSettings = TC_VectorDisplacementmap;     // RGBA8 sem compressão
	Texture->MipGenSettings = TMGS_NoMipmaps;
	Texture->AddressMode = TA_Wrap;                              // o ruído ladrilha
	Texture->PostEditChange();
	FAssetRegistryModule::AssetCreated(Texture);
	Package->MarkPackageDirty();
	return Texture;
}

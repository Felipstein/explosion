#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "BlastLabEditorLib.generated.h"

class UAnimatedSparseVolumeTexture;
class UVolumeTexture;

UCLASS()
class BLASTLABEDITOR_API UBlastLabEditorLib : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Cria um Sparse Volume Texture animado a partir de quadros densos em
	 * disco (Tools/convert_bake.py): Folder/fNNN_a.bin (RGBA8) e fNNN_b.bin
	 * (RGBA de meia precisão), na ordem densa da Unreal (x mais rápido, depois
	 * y, depois z). Segue o mesmo caminho do Niagara Baker da Epic
	 * (NiagaraBakerRendererOutputSparseVolumeTexture.cpp): BeginInitialize →
	 * CreateFromDense + AppendFrame por quadro → EndInitialize.
	 *
	 * @param Folder      pasta com os quadros
	 * @param AssetPath   pacote de destino, ex. /Game/Explosions/SVT_HeavyCharge_v0 (não pode existir)
	 * @param Resolution  voxels em x, y, z
	 * @param NumFrames   quantos quadros ler
	 * @param WorldSizeCm aresta do domínio em cm (a escala do quadro faz 1 voxel = tamanho/resolução)
	 * @param Error       motivo, quando falha
	 * @return o asset (ainda não salvo) ou nullptr
	 */
	UFUNCTION(BlueprintCallable, Category = "BlastLab")
	static UAnimatedSparseVolumeTexture* CreateAnimatedVolume(const FString& Folder, const FString& AssetPath,
		FIntVector Resolution, int32 NumFrames, float WorldSizeCm, FString& Error);

	/**
	 * Cria uma textura de volume (UVolumeTexture) RGBA8 sem compressão, que se
	 * repete nas bordas, a partir de um arquivo bruto BGRA8 na ordem densa
	 * (x mais rápido). Usada pro ruído de detalhe das explosões, que precisa
	 * ladrilhar no espaço.
	 */
	UFUNCTION(BlueprintCallable, Category = "BlastLab")
	static UVolumeTexture* CreateVolumeTexture(const FString& File, const FString& AssetPath,
		FIntVector Resolution, FString& Error);
};

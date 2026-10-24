#pragma once

#include "CoreMinimal.h"
#include "RenderGraphDefinitions.h"
#include "RenderGraphResources.h"
#include "ShaderParameterMacros.h"

class FRDGBuilder;
class FSceneView;

/**
 * O campo da poeira das lagartas como VOLUME (16/10 noite, o redesenho: docs/POEIRA_DAS_LAGARTAS.md; a regra do
 * CLAUDE.md: toda fumaça e poeira tem volume, recebe a sombra do mapa, projeta sombra e é acesa pelo fogo; um sistema
 * só). A simulação da poeira (a da v26, no ARTSTreadDust) manda os puffs a cada quadro; aqui eles viram densidade num
 * campo esparso em volta do foco (tijolos de 2 m, voxels de 0,125 m), com a MESMA gravação do EmberGen das folhas
 * aprovadas e a erosão do BlastLab, e a luz de cada célula (0,5 m): a sombra da própria poeira até o sol, a do mapa
 * (RTSSunShadow), o céu e as luzes das explosões e do fogo. Quem marcha é o raymarch das explosões (o BlastRaymarch):
 * ele chama AddPasses no passe dele e soma a poeira no MarchJoint pelas funções do DustField.ush.
 */

/** Um puff (um cartão da v26): o mesmo lugar, tamanho, quadro da gravação, sumiço e cor. */
struct FRTSDustPuff
{
	FVector PosM = FVector::ZeroVector;   // m, mundo: o pé da nuvem (o chão da gravação)
	float Scale = 1.0f;                   // m por m da gravação (o lado do cartão ÷ 4,8)
	float Yaw = 0.0f;                     // rad
	bool bMirror = false;
	float Frame = 0.0f;                   // o quadro da gravação (fracionário)
	float Sigma = 0.0f;                   // σ por unidade de densidade da gravação (1/m)
	float Erode = 0.0f;                   // a erosão do BlastLab (E)
	FLinearColor Albedo = FLinearColor(0.6f, 0.5f, 0.4f);   // o albedo de espalhamento simples (rgb, linear)
};

/** Uma luz da cena (o RTSSceneLights): posição relativa à câmera (m), raio (m), cor × candela ÷ 128 000. */
struct FRTSDustLight
{
	FVector3f PosCamRelM = FVector3f::ZeroVector;
	float RadiusM = 0.0f;
	FVector3f Color = FVector3f::ZeroVector;
};

/** Os parâmetros do raymarch que lê o campo (os nomes batem com o DustField.ush, RTS_DUST_FIELD_GLOBALS 1). */
BEGIN_SHADER_PARAMETER_STRUCT(FRTSDustFieldParameters, RTSDUSTFIELD_API)
	SHADER_PARAMETER_RDG_TEXTURE(Texture3D<uint>, RTSDustPT)
	SHADER_PARAMETER_RDG_TEXTURE(Texture3D<uint>, RTSDustPTCoarse)
	SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float>, RTSDustAtlas)
	SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float4>, RTSDustColor)
	SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float4>, RTSDustLightA)
	SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float4>, RTSDustLightB)
	SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float>, RTSDustNoise)
	SHADER_PARAMETER_SAMPLER(SamplerState, RTSDustSampler)
	SHADER_PARAMETER_SAMPLER(SamplerState, RTSDustNoiseSampler)
	SHADER_PARAMETER(FVector3f, RTSDustCamToWindowM)
	SHADER_PARAMETER(FIntVector, RTSDustWindowBricks)
	SHADER_PARAMETER(FVector3f, RTSDustAtlasInvSize)
	SHADER_PARAMETER(FVector3f, RTSDustLightInvSize)
	SHADER_PARAMETER(float, RTSDustDetail)
	SHADER_PARAMETER(float, RTSDustTime)
	SHADER_PARAMETER(float, RTSDustValid)
	SHADER_PARAMETER(FVector3f, RTSDustSunDirW)
	SHADER_PARAMETER(float, RTSDustVoxM)
	SHADER_PARAMETER(int32, RTSDustVoxStore)
END_SHADER_PARAMETER_STRUCT()

namespace RTSDustField
{
	/** Na thread do jogo: carrega a gravação (Resources/dust_kick_v2.rtsdust) uma vez; falso se faltar o arquivo. */
	RTSDUSTFIELD_API bool LoadKick(const FString& Path);
	/** A gravação carregou (a poeira usa o volume; senão fica nos cartões). */
	RTSDUSTFIELD_API bool IsKickLoaded();
	/** A duração da gravação em quadros (o mesmo índice do atlas das folhas). */
	RTSDUSTFIELD_API int32 KickFrames();
	/** A coluna densa do quadro da gravação (Σ ρ²·dz no p95 das colunas, m na escala 1): × σ do puff = a profundidade
	 *  óptica do miolo (17/10, o nascer do puff com a opacidade do cartão). */
	RTSDUSTFIELD_API float KickColumn(float Frame);
	/** O quadro (fracionário) da gravação no tempo de cozimento BakeT (s), pelos tempos dela; −1 se ela não tem os tempos.
	 *  A gravação de 30 quadros por segundo (22/10) tem outro número de quadros que a folha dos cartões. */
	RTSDUSTFIELD_API float KickFrameAt(double BakeT);
	/**
	 * Na thread do jogo, a cada quadro: os puffs, o ponto que a câmera olha (m, mundo: o centro da janela), a direção
	 * pro sol, o relógio (s) e o detalhe fino da marcha.
	 */
	RTSDUSTFIELD_API void SubmitFrame(const FVector& FocusM, const FVector& ToSun, double TimeS, float Detail, TArray<FRTSDustPuff>&& Puffs);
	/**
	 * Na thread de desenho (o passe do raymarch): monta o campo deste quadro e preenche Out. Falso sem poeira (o Out vem
	 * válido, com RTSDustValid 0). A caixa da poeira em m relativos à câmera, pra juntar com as das explosões.
	 */
	RTSDUSTFIELD_API bool AddPasses(FRDGBuilder& GraphBuilder, const FSceneView& View, TConstArrayView<FRTSDustLight> Lights,
		FRTSDustFieldParameters& Out, FVector3f& OutBoxMinM, FVector3f& OutBoxMaxM);
	/** Só o FXLab: liga a extensão da vista de depuração (rts.DustField.Debug); na thread do jogo, depois do motor. */
	RTSDUSTFIELD_API void EnsureDebugView();
	/** A vista de depuração ligada (pra quem junta as luzes dela não gastar à toa). */
	RTSDUSTFIELD_API bool IsDebugViewOn();
	/** Só o FXLab: as luzes da vista de depuração (aqui PosCamRelM leva a posição no MUNDO, em m). */
	RTSDUSTFIELD_API void SetDebugLights(TArray<FRTSDustLight>&& LightsWorldM);
	/** O último quadro mandado: tijolos, puffs, tijolos refeitos, tijolos que não couberam. */
	RTSDUSTFIELD_API FIntVector4 GetStats();
}

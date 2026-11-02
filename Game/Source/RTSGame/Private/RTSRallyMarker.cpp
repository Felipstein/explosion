#include "RTSRallyMarker.h"

#include "CanvasItem.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Canvas.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GlobalRenderResources.h"
#include "Materials/MaterialInterface.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "RTSMapBaker.h"
#include "RTSMatchSubsystem.h"
#include "RTSPlayerController.h"
#include "RTSUnitActor.h"
#include "TextureResource.h"
#include "UObject/StrongObjectPtr.h"

namespace
{
	// as medidas do mestre aprovado (o quadro de 1600 × 900; S = a altura da tela / 900, a mesma escala da barra de vida)
	constexpr float CasingPx = 8.0f;          // o contorno quase preto (a B: com o fino de 6,5 px o amarelo, o laranja e o rosa sumiam na areia)
	constexpr float CorePx = 4.0f;            // o miolo: a cor do jogador 35% pro branco
	constexpr float ChevronEveryPx = 90.0f;
	constexpr float ChevronCasingPx = 7.0f;
	constexpr float ChevronPx = 2.8f;         // a seta: a cor do jogador 80% pro branco
	constexpr float EdgeMarginPx = 28.0f;     // a ficha da borda, com o ponto fora da tela
	const FColor CasingColor(4, 6, 10, 230);
	const FColor Ink(243, 241, 236);
	// a reta rente ao chão (a mesma altura da linha da fila), amostrada no relevo a cada 3 m
	constexpr float LineLiftCm = 30.0f;
	constexpr float SampleCm = 300.0f;
	// a bandeira (cm), na proporção do quadro aprovado contra o tanque na tela: o mastro ~0,75 da largura dele, o pano ~0,45
	constexpr float MastCm = 420.0f;
	constexpr float MastDiameterCm = 20.0f;
	constexpr float ClothLengthCm = 200.0f;
	constexpr float ClothHeightCm = 125.0f;
	constexpr float PlateRadiusCm = 55.0f;   // (a 1ª prova: com 80 cm a placa cinza parecia uma tampa de bueiro)
	const TCHAR* CylinderPath = TEXT("/Engine/BasicShapes/Cylinder.Cylinder");
	const TCHAR* SpherePath = TEXT("/Engine/BasicShapes/Sphere.Sphere");
	const TCHAR* ClothPath = TEXT("/Game/Structures/Flag/SM_FlagCloth/StaticMeshes/SM_FlagCloth.SM_FlagCloth");
	const TCHAR* DarkMaterialPath = TEXT("/Game/Structures/Materials/MI_Dark.MI_Dark");
	const TCHAR* PlayerMaterialPath = TEXT("/Game/Structures/Materials/MI_Player.MI_Player");
	const TCHAR* MetalMaterialPath = TEXT("/Game/Structures/Materials/MI_RuWhite.MI_RuWhite");   // o mastro claro do mestre (o MI_UsMetal saía quase preto)

	/** Mistura em sRGB (como no quadro aprovado) e devolve a cor linear do Canvas. */
	FLinearColor Mix(const FColor& A, const FColor& B, float T)
	{
		const auto L = [T](uint8 X, uint8 Y) { return uint8(FMath::RoundToInt(FMath::Lerp(float(X), float(Y), T))); };
		return FLinearColor(FColor(L(A.R, B.R), L(A.G, B.G), L(A.B, B.B)));
	}

	/** (02/11, a 1ª prova: "continua mal feito") a borda lisa do mestre em SVG: a linha grossa e o polígono do Canvas saem
	 *  serrilhados, então o traço é uma faixa de triângulos com a textura do perfil (branca, o alfa caindo em ~1 px de tela). A
	 *  faixa e o disco (as pontas, as fichas) têm a largura + 1 px, com o alfa 0,5 na borda certa. Um par por largura, feito uma vez. */
	UTexture2D* SoftTexture(float WidthPx, bool bDisc)
	{
		static TMap<int32, TStrongObjectPtr<UTexture2D>> Cache;
		const int32 Key = FMath::RoundToInt32(WidthPx * 4.0f) * 2 + (bDisc ? 1 : 0);
		if (const TStrongObjectPtr<UTexture2D>* Have = Cache.Find(Key))
		{
			return Have->Get();
		}
		constexpr int32 N = 64;
		const int32 Rows = bDisc ? N : 1;
		UTexture2D* T = UTexture2D::CreateTransient(N, Rows, PF_B8G8R8A8);
		if (!T)
		{
			return nullptr;
		}
		T->Filter = TF_Bilinear;
		T->AddressX = TA_Clamp;
		T->AddressY = TA_Clamp;
		T->SRGB = true;
		const float Edge = 1.0f / FMath::Max(1.0f, WidthPx + 1.0f);   // um pixel de tela na escala da textura
		FTexture2DMipMap& Mip = T->GetPlatformData()->Mips[0];
		FColor* Px = static_cast<FColor*>(Mip.BulkData.Lock(LOCK_READ_WRITE));
		for (int32 Y = 0; Y < Rows; ++Y)
		{
			for (int32 X = 0; X < N; ++X)
			{
				const float U = (X + 0.5f) / N - 0.5f;
				const float V = bDisc ? (Y + 0.5f) / N - 0.5f : 0.0f;
				const float R = bDisc ? FMath::Sqrt(U * U + V * V) : FMath::Abs(U);
				Px[Y * N + X] = FColor(255, 255, 255, uint8(FMath::RoundToInt(255.0f * FMath::Clamp((0.5f - R) / Edge, 0.0f, 1.0f))));
			}
		}
		Mip.BulkData.Unlock();
		T->UpdateResource();
		Cache.Add(Key, TStrongObjectPtr<UTexture2D>(T));
		return T;
	}

	void AddQuad(TArray<FCanvasUVTri>& Tris, const FVector2D& A, const FVector2D& B, const FVector2D& C, const FVector2D& D,
		const FVector2D& UA, const FVector2D& UB, const FVector2D& UC, const FVector2D& UD, const FLinearColor& Color)
	{
		// A-B um lado, C-D o outro (A frente de C)
		FCanvasUVTri T;
		T.V0_Pos = A; T.V0_UV = UA; T.V0_Color = Color;
		T.V1_Pos = B; T.V1_UV = UB; T.V1_Color = Color;
		T.V2_Pos = C; T.V2_UV = UC; T.V2_Color = Color;
		Tris.Add(T);
		T.V0_Pos = C; T.V0_UV = UC;
		T.V1_Pos = B; T.V1_UV = UB;
		T.V2_Pos = D; T.V2_UV = UD;
		Tris.Add(T);
	}

	void Flush(UCanvas* Canvas, TArray<FCanvasUVTri>& Tris, UTexture2D* Texture)
	{
		if (Tris.Num() && Texture && Texture->GetResource())
		{
			FCanvasTriangleItem Item(Tris, Texture->GetResource());
			Item.BlendMode = SE_BLEND_Translucent;
			Canvas->DrawItem(Item);
		}
		Tris.Reset();
	}

	/** O traço da polilinha: a faixa contínua (as juntas em esquadria, sem sobrepor) e as pontas redondas, liso como o SVG. */
	void Stroke(UCanvas* Canvas, const TArray<FVector2D>& Pts, float Width, const FLinearColor& Color)
	{
		if (Pts.Num() < 2)
		{
			return;
		}
		const float Half = (Width + 1.0f) * 0.5f;
		TArray<FVector2D> L, R;
		for (int32 K = 0; K < Pts.Num(); ++K)
		{
			const FVector2D In = (K > 0 ? Pts[K] - Pts[K - 1] : Pts[1] - Pts[0]).GetSafeNormal();
			const FVector2D Out = K + 1 < Pts.Num() ? (Pts[K + 1] - Pts[K]).GetSafeNormal() : In;
			FVector2D T = (In + Out).GetSafeNormal();
			if (T.IsNearlyZero())
			{
				T = Out;
			}
			const FVector2D N(-T.Y, T.X);
			const float Miter = Half / FMath::Max(0.5f, float(FVector2D::DotProduct(N, FVector2D(-In.Y, In.X))));
			L.Add(Pts[K] + N * Miter);
			R.Add(Pts[K] - N * Miter);
		}
		TArray<FCanvasUVTri> Tris;
		const FVector2D U0(0.0f, 0.5f), U1(1.0f, 0.5f);
		for (int32 K = 1; K < Pts.Num(); ++K)
		{
			AddQuad(Tris, L[K - 1], R[K - 1], L[K], R[K], U0, U1, U0, U1, Color);
		}
		Flush(Canvas, Tris, SoftTexture(Width, false));
		// as pontas: meio disco além de cada ponta
		const auto Cap = [&](const FVector2D& E, const FVector2D& D)
		{
			const FVector2D N(-D.Y, D.X);
			AddQuad(Tris, E + N * Half, E - N * Half, E + (D + N) * Half, E + (D - N) * Half,
				FVector2D(1.0f, 0.5f), FVector2D(0.0f, 0.5f), FVector2D(1.0f, 1.0f), FVector2D(0.0f, 1.0f), Color);
		};
		Cap(Pts[0], (Pts[0] - Pts[1]).GetSafeNormal());
		Cap(Pts.Last(), (Pts.Last() - Pts[Pts.Num() - 2]).GetSafeNormal());
		Flush(Canvas, Tris, SoftTexture(Width, true));
	}

	void Disc(UCanvas* Canvas, const FVector2D& At, float Radius, const FLinearColor& Color)
	{
		const float Half = Radius + 0.5f;
		TArray<FCanvasUVTri> Tris;
		AddQuad(Tris, At + FVector2D(-Half, -Half), At + FVector2D(Half, -Half), At + FVector2D(-Half, Half), At + FVector2D(Half, Half),
			FVector2D(0.0f, 0.0f), FVector2D(1.0f, 0.0f), FVector2D(0.0f, 1.0f), FVector2D(1.0f, 1.0f), Color);
		Flush(Canvas, Tris, SoftTexture(Radius * 2.0f, true));
	}

	void Triangle(UCanvas* Canvas, const FVector2D& A, const FVector2D& B, const FVector2D& C, const FLinearColor& Color)
	{
		TArray<FCanvasUVTri> Tris;
		FCanvasUVTri T;
		T.V0_Pos = A; T.V1_Pos = B; T.V2_Pos = C;
		T.V0_Color = T.V1_Color = T.V2_Color = Color;
		Tris.Add(T);
		FCanvasTriangleItem Item(Tris, GWhiteTexture);
		Item.BlendMode = SE_BLEND_Translucent;
		Canvas->DrawItem(Item);
	}

	/** Onde o raio que sai de A na direção D deixa o retângulo [Lo, Hi] (A dentro), em unidades de D. */
	float ExitT(const FVector2D& A, const FVector2D& D, const FVector2D& Lo, const FVector2D& Hi)
	{
		float T = TNumericLimits<float>::Max();
		if (D.X > UE_KINDA_SMALL_NUMBER) { T = FMath::Min(T, float((Hi.X - A.X) / D.X)); }
		if (D.X < -UE_KINDA_SMALL_NUMBER) { T = FMath::Min(T, float((Lo.X - A.X) / D.X)); }
		if (D.Y > UE_KINDA_SMALL_NUMBER) { T = FMath::Min(T, float((Hi.Y - A.Y) / D.Y)); }
		if (D.Y < -UE_KINDA_SMALL_NUMBER) { T = FMath::Min(T, float((Lo.Y - A.Y) / D.Y)); }
		return T;
	}

	/** A reta sai da borda da planta, na direção do ponto (nunca atravessa o prédio); sem o modelo, do centro. */
	FVector2D DoorCm(const URTSMatchSubsystem* Match, const FRTSUnitState& U)
	{
		const ARTSUnitActor* View = Match->ViewOf(U.Id);
		const FBox2D Foot = View ? View->GetFootprintCm() : FBox2D(ForceInit);
		if (!View || !Foot.bIsValid)
		{
			return U.PosCm;
		}
		const FTransform T = View->GetActorTransform();
		const FVector L = T.InverseTransformPosition(FVector(U.RallyCm.X, U.RallyCm.Y, T.GetLocation().Z));
		const FVector2D C = Foot.GetCenter();
		const FVector2D D = FVector2D(L.X, L.Y) - C;
		const float Out = FMath::Min(1.0f, ExitT(C, D, Foot.Min, Foot.Max));
		const FVector W = T.TransformPosition(FVector(C.X + D.X * Out, C.Y + D.Y * Out, 0.0));
		return FVector2D(W.X, W.Y);
	}

	/** A ficha da borda (o ponto fora da tela): o disco do time com a bandeirinha e a seta pra fora, na direção da reta. */
	void EdgeToken(UCanvas* Canvas, const FVector2D& P, const FVector2D& Dir, float S, const FColor& Team)
	{
		const FColor Dark(uint8(Team.R * 0.42f), uint8(Team.G * 0.42f), uint8(Team.B * 0.42f));
		Disc(Canvas, P, 13.0f * S, FLinearColor(0.0f, 0.0f, 0.0f, 0.7f));
		Disc(Canvas, P, 12.3f * S, FLinearColor(Team));
		Disc(Canvas, P, 10.7f * S, FLinearColor(Dark));
		Stroke(Canvas, { P + FVector2D(-3.0f, 6.0f) * S, P + FVector2D(-3.0f, -7.0f) * S }, 1.8f * S, FLinearColor(Ink));
		Triangle(Canvas, P + FVector2D(-2.2f, -7.0f) * S, P + FVector2D(7.0f, -4.2f) * S, P + FVector2D(-2.2f, -1.4f) * S, FLinearColor(Ink));
		const FVector2D Side(-Dir.Y, Dir.X);
		const auto At = [&](float A, float B) { return P + (Dir * A + Side * B) * S; };
		Triangle(Canvas, At(13.4f, -7.6f), At(25.4f, 0.0f), At(13.4f, 7.6f), FLinearColor(CasingColor));
		Triangle(Canvas, At(15.0f, -5.5f), At(23.0f, 0.0f), At(15.0f, 5.5f), Mix(Team, FColor::White, 0.5f));
	}

	/** A reta da porta ao ponto, rente ao chão, com as setas; com o ponto fora da tela, até a borda e a ficha lá. */
	void DrawRoute(UCanvas* Canvas, ARTSPlayerController* PC, const URTSMatchSubsystem* Match, const FRTSBakedMap* Map, const FRTSUnitState& U, const FColor& Team, float S)
	{
		const FVector2D From = DoorCm(Match, U);
		const float LenCm = float(FVector2D::Distance(From, U.RallyCm));
		const FVector2D W(Canvas->SizeX, Canvas->SizeY);
		const FVector2D Lo(EdgeMarginPx * S, EdgeMarginPx * S), Hi = W - Lo;
		const auto Inside = [&](const FVector2D& P) { return P.X >= Lo.X && P.Y >= Lo.Y && P.X <= Hi.X && P.Y <= Hi.Y; };
		TArray<FVector2D> Pts;
		if (LenCm > PlateRadiusCm * 2.0f)
		{
			// termina na borda da placa da bandeira (a reta não passa por cima do pé do mastro)
			const FVector2D To = U.RallyCm - (U.RallyCm - From).GetSafeNormal() * PlateRadiusCm;
			const int32 N = FMath::Clamp(FMath::CeilToInt32(LenCm / SampleCm), 1, 160);
			for (int32 K = 0; K <= N; ++K)
			{
				const FVector2D P2 = FMath::Lerp(From, To, double(K) / N);
				FVector2D Screen;
				if (!PC->ProjectWorldLocationToScreen(FVector(P2.X, P2.Y, Map->GroundZCm(P2) + LineLiftCm), Screen))
				{
					break;
				}
				Pts.Add(Screen);
			}
		}
		FVector2D Rally;
		const bool bRallyProjected = PC->ProjectWorldLocationToScreen(FVector(U.RallyCm.X, U.RallyCm.Y, Map->GroundZCm(U.RallyCm)), Rally);
		const bool bOffScreen = !bRallyProjected || !Inside(Rally);
		bool bToken = false;
		FVector2D TokenAt, TokenDir;
		if (bOffScreen)
		{
			// a reta vai até onde sai da tela (com a margem) e a ficha fica lá; sem nenhum pedaço na tela, a ficha vai pra borda na
			// direção do ponto, vista do meio da tela
			int32 Last = Pts.Num() - 1;
			while (Last >= 0 && !Inside(Pts[Last]))
			{
				--Last;
			}
			if (Last >= 0 && Last + 1 < Pts.Num())
			{
				const FVector2D D = Pts[Last + 1] - Pts[Last];
				TokenAt = Pts[Last] + D * FMath::Min(1.0f, ExitT(Pts[Last], D, Lo, Hi));
				TokenDir = D.GetSafeNormal();
				Pts.SetNum(Last + 1);
				Pts.Add(TokenAt);
				bToken = true;
			}
			else if (bRallyProjected)
			{
				const FVector2D Mid = W * 0.5f;
				TokenDir = (Rally - Mid).GetSafeNormal();
				TokenAt = Mid + TokenDir * ExitT(Mid, TokenDir, Lo, Hi);
				Pts.Reset();
				bToken = !TokenDir.IsNearlyZero();
			}
		}
		if (Pts.Num() >= 2)
		{
			Stroke(Canvas, Pts, CasingPx * S, FLinearColor(CasingColor));
			Stroke(Canvas, Pts, CorePx * S, Mix(Team, FColor::White, 0.35f));
			// as setas a cada ~90 px da reta na tela, no sentido da ida
			TArray<float> Acc;
			Acc.Add(0.0f);
			for (int32 K = 1; K < Pts.Num(); ++K)
			{
				Acc.Add(Acc.Last() + float(FVector2D::Distance(Pts[K - 1], Pts[K])));
			}
			const int32 Count = FMath::FloorToInt32(Acc.Last() / (ChevronEveryPx * S));
			const FLinearColor Light = Mix(Team, FColor::White, 0.8f);
			for (int32 C = 1; C < Count; ++C)
			{
				const float Along = Acc.Last() * float(C) / float(Count);
				int32 J = 0;
				while (J + 2 < Acc.Num() && Acc[J + 1] < Along)
				{
					++J;
				}
				const FVector2D D = (Pts[J + 1] - Pts[J]).GetSafeNormal();
				const float T = Acc[J + 1] > Acc[J] ? (Along - Acc[J]) / (Acc[J + 1] - Acc[J]) : 0.0f;
				const FVector2D P = FMath::Lerp(Pts[J], Pts[J + 1], double(T));
				const FVector2D Side(-D.Y, D.X);
				const TArray<FVector2D> Arms = { P + (D * -6.0f + Side * -7.0f) * S, P + D * 2.0f * S, P + (D * -6.0f + Side * 7.0f) * S };
				Stroke(Canvas, Arms, ChevronCasingPx * S, FLinearColor(CasingColor));
				Stroke(Canvas, Arms, ChevronPx * S, Light);
			}
		}
		if (bToken)
		{
			EdgeToken(Canvas, TokenAt, TokenDir, S, Team);
		}
	}

	/** Uma bandeira por prédio (só a da seleção aparece), viva enquanto o prédio existir. */
	struct FFlag
	{
		TWeakObjectPtr<AActor> Actor;
		TWeakObjectPtr<UStaticMeshComponent> Cloth;
		float SentYaw = -100.0f;
		float SentLift = -1.0f;
		float SentRate = -1.0f;
		float Offset = 0.0f;
		uint64 SeenFrame = 0;
	};

	struct FFlags
	{
		TWeakObjectPtr<UWorld> World;
		TMap<uint32, FFlag> ById;
		TSet<uint32> RallySent;   // os prédios que o -RTSRallyAt já marcou
	};

	FFlags& Flags()
	{
		static FFlags F;
		return F;
	}

	UStaticMeshComponent* AddPart(AActor* A, UStaticMesh* Mesh, UMaterialInterface* Material, const FVector& At, const FVector& Scale, const FLinearColor& Team)
	{
		UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(A);
		C->SetupAttachment(A->GetRootComponent());
		C->SetStaticMesh(Mesh);
		C->SetRelativeLocation(At);
		C->SetRelativeScale3D(Scale);
		if (Material)
		{
			for (int32 K = 0; K < FMath::Max(1, C->GetNumMaterials()); ++K)
			{
				C->SetMaterial(K, Material);
			}
		}
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetGenerateOverlapEvents(false);
		C->SetReceivesDecals(false);
		C->SetCanEverAffectNavigation(false);
		C->SetMobility(EComponentMobility::Movable);
		C->RegisterComponent();
		C->SetCustomPrimitiveDataVector4(0, FVector4(Team.R, Team.G, Team.B, 1.0f));   // a cor do jogador (os materiais dos prédios)
		return C;
	}

	FFlag SpawnFlag(UWorld* World, const FLinearColor& Team)
	{
		FFlag F;
		UStaticMesh* Cylinder = LoadObject<UStaticMesh>(nullptr, CylinderPath);
		UStaticMesh* Sphere = LoadObject<UStaticMesh>(nullptr, SpherePath);
		UStaticMesh* Cloth = LoadObject<UStaticMesh>(nullptr, ClothPath);
		if (!Cylinder || !Sphere || !Cloth)
		{
			static bool bWarned = false;
			UE_CLOG(!bWarned, LogTemp, Warning, TEXT("RTS: a bandeira do ponto de encontro sem as malhas (%s)"), !Cloth ? ClothPath : CylinderPath);
			bWarned = true;
			return F;
		}
		FActorSpawnParameters Params;
		Params.ObjectFlags |= RF_Transient;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AActor* A = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, Params);
		if (!A)
		{
			return F;
		}
		USceneComponent* Root = NewObject<USceneComponent>(A, TEXT("RallyFlag"));
		Root->SetMobility(EComponentMobility::Movable);
		A->SetRootComponent(Root);
		Root->RegisterComponent();
		UMaterialInterface* Dark = LoadObject<UMaterialInterface>(nullptr, DarkMaterialPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
		UMaterialInterface* Player = LoadObject<UMaterialInterface>(nullptr, PlayerMaterialPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
		UMaterialInterface* Metal = LoadObject<UMaterialInterface>(nullptr, MetalMaterialPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
		// a placa redonda pregada no chão (a borda escura e o miolo do time), o mastro de metal com a bola e o pano no topo
		// (as formas da engine têm 1 m e o pivô no meio)
		const float Plate = PlateRadiusCm * 2.0f / 100.0f;
		AddPart(A, Cylinder, Dark, FVector(0.0, 0.0, 4.0), FVector(Plate, Plate, 0.08), Team);
		AddPart(A, Cylinder, Player, FVector(0.0, 0.0, 8.0), FVector(Plate * 0.68f, Plate * 0.68f, 0.08), Team);
		AddPart(A, Cylinder, Metal, FVector(0.0, 0.0, MastCm * 0.5f), FVector(MastDiameterCm / 100.0f, MastDiameterCm / 100.0f, MastCm / 100.0f), Team);
		AddPart(A, Sphere, Metal, FVector(0.0, 0.0, MastCm + 14.0f), FVector(0.4), Team);
		UStaticMeshComponent* C = AddPart(A, Cloth, nullptr, FVector(0.0, 0.0, MastCm - 12.0f), FVector::OneVector, Team);
		C->SetBoundsScale(3.0f);   // (o pano sai da caixa da malha pelo vento: sem isso some na borda da tela)
		C->SetCustomPrimitiveDataFloat(5, ClothLengthCm);
		C->SetCustomPrimitiveDataFloat(6, ClothHeightCm);
		C->SetCustomPrimitiveDataFloat(11, 0.0f);   // sem emblema: o pano liso na cor do jogador
		// (a 1ª prova: o pano no sol saía (130, 205, 233), lavado, contra o 59B2FF do mestre: o azul no ombro do tonemapper) o tecido
		// tingido mais fundo, a mesma cor com menos luz e mais saturação
		const auto Dye = [](float V) { return 0.62f * FMath::Pow(V, 1.3f); };
		C->SetCustomPrimitiveDataVector4(0, FVector4(Dye(Team.R), Dye(Team.G), Dye(Team.B), 1.0f));
		F.Actor = A;
		F.Cloth = C;
		return F;
	}

	/** O vento do mapa no pano, só quando muda (a mesma conta do RTSUnitActor::SendFlagWind: a fase segue sem pular). */
	void SendWind(FFlag& F, const URTSMatchSubsystem* Match, float Now)
	{
		UStaticMeshComponent* C = F.Cloth.Get();
		if (!C)
		{
			return;
		}
		const float Yaw = FMath::DegreesToRadians(Match->GetWindYawDeg());
		const float Speed = Match->GetWindSpeedMs();
		// (a 1ª prova: no vento fraco o pano caía e a bandeira não lia) o marcador sempre tremula: no mínimo 0,6 esticado
		const float Lift = FMath::Clamp(Speed / 7.0f, 0.6f, 1.0f);
		const float Rate = 0.6f + 0.32f * Speed;
		if (FMath::Abs(FMath::FindDeltaAngleRadians(F.SentYaw, Yaw)) < FMath::DegreesToRadians(1.5f) && FMath::Abs(F.SentLift - Lift) < 0.02f
			&& FMath::Abs(F.SentRate - Rate) < 0.04f)
		{
			return;
		}
		if (F.SentRate >= 0.0f)
		{
			F.Offset = FMath::Fmod(Now * F.SentRate + F.Offset - Now * Rate, 1000.0f);
		}
		C->SetCustomPrimitiveDataFloat(7, Yaw);
		C->SetCustomPrimitiveDataFloat(8, Lift);
		C->SetCustomPrimitiveDataFloat(9, Rate);
		C->SetCustomPrimitiveDataFloat(10, F.Offset);
		F.SentYaw = Yaw;
		F.SentLift = Lift;
		F.SentRate = Rate;
	}

	/** -RTSRallyAt=X,Y (m): o ponto de encontro dos prédios selecionados, uma vez cada (a prova sem o clique). */
	void RallyForTest(ARTSPlayerController* PC, URTSMatchSubsystem* Match, FFlags& F)
	{
		FString Arg, X, Y;
		if (!FParse::Value(FCommandLine::Get(), TEXT("-RTSRallyAt="), Arg, false) || !Arg.Split(TEXT(","), &X, &Y))
		{
			return;
		}
		TArray<uint32> Fresh;
		for (const uint32 Id : PC->GetSelection())
		{
			FRTSUnitState U;
			if (!F.RallySent.Contains(Id) && Match->GetUnit(Id, U) && U.bStructure && U.Owner == Match->LocalPlayer())
			{
				Fresh.Add(Id);
				F.RallySent.Add(Id);
			}
		}
		if (Fresh.Num())
		{
			Match->OrderRally(Fresh, FVector2D(FCString::Atof(*X) * 100.0, FCString::Atof(*Y) * 100.0));
			UE_LOG(LogTemp, Log, TEXT("RTS ponto de encontro (teste): %d prédios pra (%s, %s) m"), Fresh.Num(), *X, *Y);
		}
	}
}

void RTSRallyMarker::Draw(UCanvas* Canvas, ARTSPlayerController* PC, URTSMatchSubsystem* Match)
{
	UWorld* World = PC ? PC->GetWorld() : nullptr;
	const FRTSBakedMap* Map = Match ? Match->BakedMap() : nullptr;
	if (!Canvas || !World || !PC || !Map)
	{
		return;
	}
	FFlags& F = Flags();
	if (F.World.Get() != World)
	{
		F.ById.Reset();
		F.RallySent.Reset();
		F.World = World;
	}
	RallyForTest(PC, Match, F);
	const float S = FMath::Max(0.5f, Canvas->SizeY / 900.0f);
	for (const uint32 Id : PC->GetSelection())
	{
		FRTSUnitState U;
		if (!Match->GetUnit(Id, U) || !U.bStructure || !U.bHasRally || U.Owner != Match->LocalPlayer())
		{
			continue;
		}
		const FLinearColor TeamLinear = URTSMatchSubsystem::PlayerColor(U.Owner);
		DrawRoute(Canvas, PC, Match, Map, U, TeamLinear.ToFColor(true), S);
		FFlag& Flag = F.ById.FindOrAdd(Id);
		if (!Flag.Actor.IsValid())
		{
			Flag = SpawnFlag(World, TeamLinear);
		}
		if (AActor* A = Flag.Actor.Get())
		{
			const FVector At(U.RallyCm.X, U.RallyCm.Y, Map->GroundZCm(U.RallyCm) - 2.0f);
			if (!A->GetActorLocation().Equals(At, 0.5))
			{
				A->SetActorLocation(At);
			}
			if (A->IsHidden())
			{
				A->SetActorHiddenInGame(false);
			}
			SendWind(Flag, Match, float(World->GetTimeSeconds()));
		}
		Flag.SeenFrame = GFrameCounter;
	}
	// as bandeiras fora da seleção somem; a do prédio que acabou vai embora
	for (auto It = F.ById.CreateIterator(); It; ++It)
	{
		if (It->Value.SeenFrame == GFrameCounter)
		{
			continue;
		}
		AActor* A = It->Value.Actor.Get();
		FRTSUnitState U;
		if (!A || !Match->GetUnit(It->Key, U))
		{
			if (A)
			{
				A->Destroy();
			}
			It.RemoveCurrent();
			continue;
		}
		if (!A->IsHidden())
		{
			A->SetActorHiddenInGame(true);
		}
	}
}

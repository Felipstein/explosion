#include "WorldForgeMeshes.h"

#include "Engine/StaticMesh.h"
#include "Generation/WorldNoise.h"
#include "MeshDescription.h"
#include "StaticMeshAttributes.h"

namespace WorldForgeMeshes
{
	/** Triângulos facetados (normal por face), em centímetros. */
	struct FBuilder
	{
		TArray<FVector3f> P;
		TArray<FVector3f> N;
		TArray<FVector4f> C;

		/**
		 * Frente do triângulo na Unreal: normal = (C − A) × (B − A)
		 * (KismetProceduralMeshLibrary::CalculateTangentsForMesh). Com Out
		 * (direção pra fora), a ordem é corrigida se vier invertida.
		 */
		void Tri(FVector3f A, FVector3f B, FVector3f Cc, const FVector4f& Color, const FVector3f& Out)
		{
			FVector3f Nrm = ((Cc - A) ^ (B - A));
			if ((Nrm | Out) < 0.0f)
			{
				Swap(B, Cc);
				Nrm = -Nrm;
			}
			Nrm = Nrm.GetSafeNormal();
			P.Append({ A, B, Cc });
			N.Append({ Nrm, Nrm, Nrm });
			C.Append({ Color, Color, Color });
		}

		void TriFromCenter(const FVector3f& A, const FVector3f& B, const FVector3f& Cc, const FVector4f& Color, const FVector3f& Center)
		{
			Tri(A, B, Cc, Color, (A + B + Cc) / 3.0f - Center);
		}

		/** Prisma de Sides lados (cilindro facetado) de Z0 a Z1, raio R0 embaixo e R1 em cima. */
		void Frustum(FVector3f Base, float R0, float R1, float Height, int32 Sides, const FVector4f& Color, float Phase = 0.0f, bool bCaps = true)
		{
			const FVector3f Top = Base + FVector3f(0, 0, Height);
			for (int32 S = 0; S < Sides; ++S)
			{
				const float A0 = Phase + 2.0f * PI * S / Sides, A1 = Phase + 2.0f * PI * (S + 1) / Sides;
				const FVector3f D0(FMath::Cos(A0), FMath::Sin(A0), 0.0f), D1(FMath::Cos(A1), FMath::Sin(A1), 0.0f);
				const FVector3f B0 = Base + D0 * R0, B1 = Base + D1 * R0;
				const FVector3f T0 = Top + D0 * R1, T1 = Top + D1 * R1;
				const FVector3f Out = (D0 + D1).GetSafeNormal();
				if (R1 > 0.01f)
				{
					Tri(B0, B1, T1, Color, Out);
					Tri(B0, T1, T0, Color, Out);
				}
				else
				{
					Tri(B0, B1, Top, Color, Out + FVector3f(0, 0, R0 / FMath::Max(Height, 1.0f)));
				}
				if (bCaps)
				{
					Tri(Base, B1, B0, Color, FVector3f(0, 0, -1));
					if (R1 > 0.01f)
					{
						Tri(Top, T0, T1, Color, FVector3f(0, 0, 1));
					}
				}
			}
		}

		/** Bolha: icosaedro subdividido, com o raio perturbado pelo hash da direção. */
		void Blob(FVector3f Center, FVector3f Radii, float Bumps, uint32 Seed, const FVector4f& Color, int32 Subdiv = 1)
		{
			TArray<FVector3f> V;
			TArray<FIntVector> F;
			Icosphere(Subdiv, V, F);
			for (FVector3f& D : V)
			{
				const float H = WorldNoise::Gradient(D.X * 2.1f + D.Z * 0.7f, D.Y * 2.1f - D.Z * 1.3f, Seed);
				const float R = 1.0f + Bumps * H;
				D = Center + FVector3f(D.X * Radii.X, D.Y * Radii.Y, D.Z * Radii.Z) * R;
			}
			for (const FIntVector& T : F)
			{
				TriFromCenter(V[T.X], V[T.Y], V[T.Z], Color, Center);
			}
		}

		static void Icosphere(int32 Subdiv, TArray<FVector3f>& V, TArray<FIntVector>& F)
		{
			const float T = (1.0f + FMath::Sqrt(5.0f)) * 0.5f;
			V = { {-1, T, 0}, {1, T, 0}, {-1, -T, 0}, {1, -T, 0}, {0, -1, T}, {0, 1, T},
				  {0, -1, -T}, {0, 1, -T}, {T, 0, -1}, {T, 0, 1}, {-T, 0, -1}, {-T, 0, 1} };
			for (FVector3f& X : V) X.Normalize();
			F = { {0, 11, 5}, {0, 5, 1}, {0, 1, 7}, {0, 7, 10}, {0, 10, 11}, {1, 5, 9}, {5, 11, 4}, {11, 10, 2}, {10, 7, 6}, {7, 1, 8},
				  {3, 9, 4}, {3, 4, 2}, {3, 2, 6}, {3, 6, 8}, {3, 8, 9}, {4, 9, 5}, {2, 4, 11}, {6, 2, 10}, {8, 6, 7}, {9, 8, 1} };
			for (int32 S = 0; S < Subdiv; ++S)
			{
				TMap<uint64, int32> Mid;
				auto Middle = [&](int32 A, int32 B)
				{
					const uint64 Key = (uint64(FMath::Min(A, B)) << 32) | uint64(FMath::Max(A, B));
					if (const int32* Found = Mid.Find(Key)) return *Found;
					const int32 I = V.Add(((V[A] + V[B]) * 0.5f).GetSafeNormal());
					Mid.Add(Key, I);
					return I;
				};
				TArray<FIntVector> NF;
				for (const FIntVector& Tr : F)
				{
					const int32 A = Middle(Tr.X, Tr.Y), B = Middle(Tr.Y, Tr.Z), Cc = Middle(Tr.Z, Tr.X);
					NF.Append({ {Tr.X, A, Cc}, {Tr.Y, B, A}, {Tr.Z, Cc, B}, {A, B, Cc} });
				}
				F = MoveTemp(NF);
			}
		}
	};

	static FVector4f Col(float R, float G, float B, float Glow = 0.0f) { return FVector4f(R, G, B, Glow); }

	static void Conifer(FBuilder& M, WorldNoise::FRng& Rng)
	{
		M.Frustum(FVector3f(0, 0, -50), 28, 18, 330, 6, Col(0.16f, 0.10f, 0.06f));
		const int32 Tiers = 3 + Rng.Int(2);
		float Z = 170.0f + Rng.Range(-20, 30);
		float R = 270.0f + Rng.Range(-30, 40);
		for (int32 K = 0; K < Tiers; ++K)
		{
			const float Hgt = 430.0f - 40.0f * K + Rng.Range(-30, 30);
			const float Shade = 0.85f + 0.08f * K + Rng.Range(-0.05f, 0.05f);
			const FVector3f Off(Rng.Range(-15, 15), Rng.Range(-15, 15), 0);
			M.Frustum(FVector3f(0, 0, Z) + Off, R, 0.0f, Hgt, 8, Col(0.045f * Shade, 0.10f * Shade, 0.04f * Shade), Rng.Range(0, 1));
			Z += Hgt * 0.52f;
			R *= 0.74f;
		}
	}

	static void Broadleaf(FBuilder& M, WorldNoise::FRng& Rng, uint32 Variant)
	{
		M.Frustum(FVector3f(0, 0, -50), 34, 22, 420, 6, Col(0.15f, 0.10f, 0.06f));
		const int32 Blobs = 3 + Rng.Int(2);
		for (int32 K = 0; K < Blobs; ++K)
		{
			const float A = 2.0f * PI * K / Blobs + Rng.Range(0, 1);
			const float D = K == 0 ? 0.0f : Rng.Range(90, 150);
			const FVector3f C(FMath::Cos(A) * D, FMath::Sin(A) * D, 520.0f + Rng.Range(-40, 90) + (K == 0 ? 80.0f : 0.0f));
			const float R = Rng.Range(190, 260);
			const float Shade = Rng.Range(0.85f, 1.1f);
			M.Blob(C, FVector3f(R, R, R * 0.85f), 0.12f, Variant * 31u + K, Col(0.06f * Shade, 0.13f * Shade, 0.035f * Shade));
		}
	}

	static void Rock(FBuilder& M, WorldNoise::FRng& Rng, uint32 Variant)
	{
		const FVector3f Radii(Rng.Range(110, 150), Rng.Range(80, 110), Rng.Range(55, 80));
		const float G = Rng.Range(0.17f, 0.24f);
		M.Blob(FVector3f(0, 0, 10), Radii, 0.28f, Variant * 7u + 3u, Col(G, G * 0.96f, G * 0.92f), 1);
		if (Rng.Float() < 0.6f)   // pedra menor encostada
		{
			const float A = Rng.Range(0, 2 * PI);
			M.Blob(FVector3f(FMath::Cos(A) * Radii.X, FMath::Sin(A) * Radii.Y, 0) * 0.9f, Radii * 0.45f, 0.3f, Variant * 13u + 5u, Col(G * 0.9f, G * 0.88f, G * 0.85f), 1);
		}
	}

	/** Prisma hexagonal com ponta (cristal), inclinado. */
	static void Shard(FBuilder& M, FVector3f Base, FVector3f Axis, float Radius, float Length, const FVector4f& Color)
	{
		Axis.Normalize();
		const FVector3f U = (FMath::Abs(Axis.Z) < 0.9f ? (Axis ^ FVector3f(0, 0, 1)) : (Axis ^ FVector3f(1, 0, 0))).GetSafeNormal();
		const FVector3f V = Axis ^ U;
		const FVector3f Top = Base + Axis * Length;
		const FVector3f Tip = Top + Axis * (Radius * 1.8f);
		for (int32 S = 0; S < 6; ++S)
		{
			const float A0 = 2.0f * PI * S / 6, A1 = 2.0f * PI * (S + 1) / 6;
			const FVector3f D0 = U * FMath::Cos(A0) + V * FMath::Sin(A0), D1 = U * FMath::Cos(A1) + V * FMath::Sin(A1);
			const FVector3f Out = D0 + D1;
			M.Tri(Base + D0 * Radius, Base + D1 * Radius, Top + D1 * Radius, Color, Out);
			M.Tri(Base + D0 * Radius, Top + D1 * Radius, Top + D0 * Radius, Color, Out);
			M.Tri(Top + D0 * Radius, Top + D1 * Radius, Tip, Color, Out + Axis);
		}
	}

	static void Crystal(FBuilder& M, WorldNoise::FRng& Rng)
	{
		const int32 Count = 5 + Rng.Int(4);
		for (int32 K = 0; K < Count; ++K)
		{
			const float A = Rng.Range(0, 2 * PI);
			const float D = K == 0 ? 0.0f : Rng.Range(40, 110);
			const float Tilt = K == 0 ? Rng.Range(0, 0.15f) : Rng.Range(0.25f, 0.7f);
			const FVector3f Dir(FMath::Cos(A), FMath::Sin(A), 0);
			const FVector3f Axis = (FVector3f(0, 0, 1) + Dir * Tilt).GetSafeNormal();
			const float Len = (K == 0 ? Rng.Range(180, 260) : Rng.Range(70, 170));
			const float Hue = Rng.Range(-0.04f, 0.04f);
			Shard(M, Dir * D + FVector3f(0, 0, -30), Axis, Rng.Range(18, 32), Len, Col(0.12f + Hue, 0.85f, 0.25f - Hue, 1.0f));
		}
		// pedrinhas escuras na base do aglomerado
		M.Blob(FVector3f(0, 0, -10), FVector3f(110, 100, 35), 0.25f, 99u, Col(0.08f, 0.08f, 0.07f), 1);
	}

	static void BasePad(FBuilder& M)
	{
		// laje de concreto octogonal (36 m) com a faixa da cor do jogador e um farol
		const FVector4f Concrete = Col(0.30f, 0.30f, 0.28f);
		M.Frustum(FVector3f(0, 0, -150), 1850, 1780, 180, 8, Concrete, PI / 8, true);
		for (int32 S = 0; S < 8; ++S)
		{
			const float A0 = PI / 8 + 2.0f * PI * S / 8, A1 = PI / 8 + 2.0f * PI * (S + 1) / 8;
			const FVector3f D0(FMath::Cos(A0), FMath::Sin(A0), 0), D1(FMath::Cos(A1), FMath::Sin(A1), 0);
			const float Z = 32.0f;
			const FVector4f Stripe = Col(1, 1, 1, 1);
			M.Tri(D0 * 1500 + FVector3f(0, 0, Z), D1 * 1500 + FVector3f(0, 0, Z), D1 * 1620 + FVector3f(0, 0, Z), Stripe, FVector3f(0, 0, 1));
			M.Tri(D0 * 1500 + FVector3f(0, 0, Z), D1 * 1620 + FVector3f(0, 0, Z), D0 * 1620 + FVector3f(0, 0, Z), Stripe, FVector3f(0, 0, 1));
		}
		M.Frustum(FVector3f(0, 0, 30), 60, 45, 700, 6, Col(1, 1, 1, 1));
	}

	UStaticMesh* Build(EKind Kind, uint32 Variant, UMaterialInterface* Material, UObject* Outer)
	{
		FBuilder M;
		WorldNoise::FRng Rng(Variant * 2654435761u + uint32(Kind) * 97u);
		switch (Kind)
		{
		case EKind::Conifer: Conifer(M, Rng); break;
		case EKind::Broadleaf: Broadleaf(M, Rng, Variant); break;
		case EKind::Rock: Rock(M, Rng, Variant); break;
		case EKind::Crystal: Crystal(M, Rng); break;
		case EKind::BasePad: BasePad(M); break;
		default: break;
		}

		FMeshDescription Desc;
		FStaticMeshAttributes Attr(Desc);
		Attr.Register();
		const FPolygonGroupID Group = Desc.CreatePolygonGroup();
		Attr.GetPolygonGroupMaterialSlotNames()[Group] = TEXT("Prop");
		auto Normals = Attr.GetVertexInstanceNormals();
		auto Tangents = Attr.GetVertexInstanceTangents();
		auto Signs = Attr.GetVertexInstanceBinormalSigns();
		auto Colors = Attr.GetVertexInstanceColors();
		auto UVs = Attr.GetVertexInstanceUVs();
		auto Positions = Attr.GetVertexPositions();
		Desc.ReserveNewVertices(M.P.Num());
		Desc.ReserveNewVertexInstances(M.P.Num());
		Desc.ReserveNewTriangles(M.P.Num() / 3);
		for (int32 T = 0; T < M.P.Num(); T += 3)
		{
			FVertexInstanceID Inst[3];
			for (int32 K = 0; K < 3; ++K)
			{
				const FVertexID V = Desc.CreateVertex();
				Positions[V] = M.P[T + K];
				Inst[K] = Desc.CreateVertexInstance(V);
				const FVector3f Nrm = M.N[T + K];
				const FVector3f Tan = (FMath::Abs(Nrm.Z) < 0.95f ? (FVector3f(0, 0, 1) ^ Nrm) : FVector3f(1, 0, 0)).GetSafeNormal();
				Normals[Inst[K]] = Nrm;
				Tangents[Inst[K]] = Tan;
				Signs[Inst[K]] = 1.0f;
				// a cor do vértice da Unreal é guardada linear e convertida pra sRGB
				// 8 bits; o material lê de volta linear
				Colors[Inst[K]] = M.C[T + K];
				UVs[Inst[K]] = FVector2f(M.P[T + K].X, M.P[T + K].Y) / 100.0f;
			}
			Desc.CreateTriangle(Group, Inst);
		}

		UStaticMesh* Mesh = NewObject<UStaticMesh>(Outer, NAME_None, RF_Transient);
		Mesh->GetStaticMaterials().Add(FStaticMaterial(Material, TEXT("Prop"), TEXT("Prop")));
		UStaticMesh::FBuildMeshDescriptionsParams Params;
		Params.bFastBuild = true;                 // caminho de runtime (sem DDC/editor)
		Params.bCommitMeshDescription = false;
		Params.bMarkPackageDirty = false;
		Params.bBuildSimpleCollision = false;
		Mesh->BuildFromMeshDescriptions({ &Desc }, Params);
		return Mesh;
	}
}

#include "RTSLooseTurret.h"

#include "HAL/PlatformTime.h"

// As constantes do solver num namespace com nome, não no anônimo: no unity build a UBT junta vários .cpp do módulo num só, e o
// anônimo de todos vira um (o MaxStepsPerFrame daqui e o do RTSMatchSubsystem.cpp, no mesmo blob, davam o C2374, redefinição)
namespace TurretSolver
{
	constexpr float GravityCmS2 = 980.0f;
	/** O passo fixo: a 15 m/s cada passo anda 12,5 cm, menos que a meia espessura da torre (~60 cm). */
	constexpr float StepS = 1.0f / 120.0f;
	/** No máximo 8 passos por quadro (1/15 s): num tranco, câmera lenta em vez de passos grandes que atravessam o chão. */
	constexpr int32 MaxStepsPerFrame = 8;
	// o solver do Box2D v2.4.1 (b2_common.h): 8 iterações de velocidade e 3 de posição, β 0,2 (b2_baumgarte), a folga de 0,5 cm
	// (b2_linearSlop) e no máximo 20 cm de correção por iteração (b2_maxLinearCorrection)
	constexpr int32 VelocityIterations = 8;
	constexpr int32 PositionIterations = 3;
	constexpr float Baumgarte = 0.2f;
	constexpr float LinearSlopCm = 0.5f;
	constexpr float MaxCorrectionCm = 20.0f;
	/** O contato especulativo: 2 cm (4× a folga, o b2_speculativeDistance do Box2D v3) mais o que o ponto anda no passo, pra
	 *  nenhum ponto atravessar o chão num passo a qualquer velocidade (no protótipo, sem ele ela parava 1,7–2,0 cm no ar). */
	constexpr float SpeculativeCm = 2.0f;
	/** Sem quique abaixo de 1 m/s (b2_velocityThreshold): o pulo seria < e²v²/2g ≈ 5 mm, menos de 1 pixel a 100 m. */
	constexpr float RestitutionThresholdCmS = 100.0f;
	/** A restituição cai com a velocidade da batida: e = Rn / (1 + (v/K)²), com K = 30 pés/s = 9,144 m/s, a velocidade em que Rn
	 *  cai à metade (Pfeiffer & Bowen 1989, o CRSP; a "scale Rn by velocity" do RocFall). Na areia a 13 m/s, e ≈ 0,10; a 3 m/s,
	 *  0,27 (o leito de grãos absorve mais quanto mais forte a batida: arXiv 1810.01459). Com Rn fixo o pulo mediano ia de 0,27 a
	 *  0,98 m no protótipo. */
	constexpr float RestitutionHalfSpeedCmS = 914.4f;
	/** Contra aço (o casco, as outras unidades, outra torre): Rn 0,6 (aço-aço em batida lenta), com a mesma escala. */
	constexpr float SteelRestitution = 0.6f;
	/** O atrito: aço rugoso contra areia seca tende ao ângulo de atrito da areia, δ → φ ≈ 31° (Potyondy 1961), μ = tan 31° ≈ 0,6.
	 *  Ela fica parada até ~31° e desliza na face de avalanche da duna (~34°, o ângulo de repouso), como na vida real. A Unreal
	 *  usa 0,7 por padrão; no protótipo, μ de 0,45 a 0,75 quase não muda o tempo até parar. */
	constexpr float Friction = 0.6f;
	// dormir, encostada (Box2D: b2_linearSleepTolerance 1 cm/s, b2_angularSleepTolerance 2°/s, b2_timeToSleep 0,5 s; a Unreal
	// usa 1 cm/s, 0,05 rad/s e 4 quadros)
	constexpr float SleepLinearCmS = 1.0f;
	constexpr float SleepAngularRadS = 2.0f * UE_PI / 180.0f;
	constexpr float SleepS = 0.5f;
	/** O teto: 10 s de voo e ela congela onde está (o protótipo dormiu em 3–9 s; uma face de avalanche longa pode chegar nele). */
	constexpr float CapS = 10.0f;
	/** O tubo do cano: 7% da torre (o tubo do M256 tem 1.175 kg; a torre do T-72 é citada com 12 t, as de 12–20 t dão 6–10%). */
	constexpr float BarrelMassShare = 0.07f;
	/** O raio do tubo: 3/4 da meia caixa da malha do cano (a caixa pega o evacuador, 16,5 cm; o tubo tem 11–13 cm). */
	constexpr float BarrelRadiusShare = 0.75f;
	/** A busca do chão supõe normais com z ≥ 0,5 (rampas até 60°): mais íngreme que isso, o ponto só é pego depois de entrar. */
	constexpr float MinNormalZ = 0.5f;
	// as batidas que viram pó e marca: acima do limiar do quique, a 0,25 s uma da outra, no máximo 4 por torre (o protótipo dá
	// ~10 batidas > 1 m/s por queda, em rajadas)
	constexpr float ImpactSpacingS = 0.25f;
	constexpr int32 MaxImpacts = 4;
	// (23/10, a areia do acerto) as raspadas: deslizando ou rolando a mais de 1 m/s no plano, um ponto a cada 0,5 m de contato com o chão
	// (o mesmo passo da garra da lagarta), no máximo 12 por torre
	constexpr float ScrapeMinCmS = 100.0f;
	constexpr float ScrapeStepCm = 50.0f;
	constexpr int32 MaxScrapes = 12;
	/** Um obstáculo arma quando nenhum ponto está a menos disto dele. */
	constexpr float ArmMarginCm = 5.0f;

	static float ScaledRestitution(float Rn, float ApproachCmS)
	{
		const float U = ApproachCmS / RestitutionHalfSpeedCmS;
		return Rn / (1.0f + U * U);
	}

	static FVector AnyPerpendicular(const FVector& N)
	{
		return FVector::CrossProduct(N, FMath::Abs(N.X) < 0.9 ? FVector::ForwardVector : FVector::RightVector).GetSafeNormal();
	}

	/** O quanto a caixa mexeu entre duas leituras (cm): o maior deslocamento entre os 8 cantos (pega o andar, o giro e a troca de
	 *  caixa, como o casco que perde a torre). */
	static double MaxCornerShiftCm(const FTransform& FrameA, const FBox& BoxA, const FTransform& FrameB, const FBox& BoxB)
	{
		double Most = 0.0;
		for (int32 C = 0; C < 8; ++C)
		{
			const FVector LocalA((C & 1) ? BoxA.Max.X : BoxA.Min.X, (C & 2) ? BoxA.Max.Y : BoxA.Min.Y, (C & 4) ? BoxA.Max.Z : BoxA.Min.Z);
			const FVector LocalB((C & 1) ? BoxB.Max.X : BoxB.Min.X, (C & 2) ? BoxB.Max.Y : BoxB.Min.Y, (C & 4) ? BoxB.Max.Z : BoxB.Min.Z);
			Most = FMath::Max(Most, FVector::Dist(FrameA.TransformPositionNoScale(LocalA), FrameB.TransformPositionNoScale(LocalB)));
		}
		return Most;
	}
}

void FRTSLooseTurret::Init(const FRTSLooseTurretShape& Shape)
{
	const double S = FMath::Max(double(Shape.Scale), 0.01);
	const FVector BMin = Shape.Body.Min * S;
	const FVector BMax = Shape.Body.Max * S;
	const FVector BCenter = (BMin + BMax) * 0.5;
	const FVector BSize = BMax - BMin;
	const bool bBarrel = Shape.Barrel.IsValid != 0;
	FVector RMin = FVector::ZeroVector;
	FVector RMax = FVector::ZeroVector;
	FVector RCenter = FVector::ZeroVector;
	double RodLen = 0.0;
	double RodR = 0.0;
	if (bBarrel)
	{
		RMin = Shape.Barrel.Min * S;
		RMax = Shape.Barrel.Max * S;
		RCenter = (RMin + RMax) * 0.5;
		RodLen = RMax.X - RMin.X;
		RodR = TurretSolver::BarrelRadiusShare * 0.5 * FMath::Min(RMax.Y - RMin.Y, RMax.Z - RMin.Z);
	}
	// a massa por forma (o padrão da Unreal e do PhysX): o corpo como caixa de densidade uniforme e o tubo como bastão
	const double MRod = bBarrel ? double(TurretSolver::BarrelMassShare) : 0.0;
	const double MBody = 1.0 - MRod;
	const FVector Com = BCenter * MBody + RCenter * MRod;
	// a inércia por massa nos eixos das malhas (a caixa: (b² + c²)/12; o bastão: r²/2 no eixo e L²/12 + r²/4 nos outros), levada
	// pro centro pelos eixos paralelos (só a diagonal: os produtos do cano fora do centro são < 1% dela). No Battle Tank: o
	// centro 18 cm à frente do anel e K = (1,12; 3,40; 4,29) m² (rolar, cambalhota, girar); o cano aumenta a cambalhota em ~70%
	const auto Parallel = [](const FVector& I0, const FVector& D, double M)
	{
		return FVector(M * (I0.X + D.Y * D.Y + D.Z * D.Z), M * (I0.Y + D.X * D.X + D.Z * D.Z), M * (I0.Z + D.X * D.X + D.Y * D.Y));
	};
	FVector KSum = Parallel(FVector(BSize.Y * BSize.Y + BSize.Z * BSize.Z, BSize.X * BSize.X + BSize.Z * BSize.Z,
		BSize.X * BSize.X + BSize.Y * BSize.Y) / 12.0, BCenter - Com, MBody);
	if (bBarrel)
	{
		const double Across = RodLen * RodLen / 12.0 + RodR * RodR / 4.0;
		KSum += Parallel(FVector(RodR * RodR / 2.0, Across, Across), RCenter - Com, MRod);
	}
	K = KSum.ComponentMax(FVector(1.0));
	InvK = FVector(1.0 / K.X, 1.0 / K.Y, 1.0 / K.Z);
	ComLocal = Com / S;
	BodyBox = FBox(BMin - Com, BMax - Com);
	// os pontos de contato: a grade 3×3×3 da caixa sem o centro (os cantos, os meios das arestas e os centros das faces),
	// espaçados ≤ 2,6 m como a grade de 2 m do mapa (uma crista entre dois cantos não atravessa a face)
	Points.Reset();
	for (int32 Ix = 0; Ix < 3; ++Ix)
	{
		for (int32 Iy = 0; Iy < 3; ++Iy)
		{
			for (int32 Iz = 0; Iz < 3; ++Iz)
			{
				if (Ix == 1 && Iy == 1 && Iz == 1)
				{
					continue;
				}
				FPoint& Pt = Points.AddDefaulted_GetRef();
				Pt.P = FVector(BMin.X + BSize.X * 0.5 * Ix, BMin.Y + BSize.Y * 0.5 * Iy, BMin.Z + BSize.Z * 0.5 * Iz) - Com;
			}
		}
	}
	// o cano: uma esfera a 3/4 do tubo e outra na boca (a ponta da esfera na ponta da malha)
	Muzzle = FVector::ZeroVector;
	if (bBarrel)
	{
		FPoint& Mid = Points.AddDefaulted_GetRef();
		Mid.P = FVector(RMin.X + 0.75 * RodLen, RCenter.Y, RCenter.Z) - Com;
		Mid.RadiusCm = float(RodR);
		Muzzle = FVector(RMax.X - RodR, RCenter.Y, RCenter.Z) - Com;
		FPoint& Tip = Points.AddDefaulted_GetRef();
		Tip.P = Muzzle;
		Tip.RadiusCm = float(RodR);
	}
	BoundRadiusCm = 0.0f;
	for (const FPoint& Pt : Points)
	{
		BoundRadiusCm = FMath::Max(BoundRadiusCm, float(Pt.P.Size()) + Pt.RadiusCm);
	}
	PointStates.Init(FPointState(), Points.Num());
	TouchingNow.Init(false, Points.Num());
	Contacts.Reserve(Points.Num());
}

void FRTSLooseTurret::Launch(const FVector& InX, const FQuat& InQ, const FVector& InV, const FVector& InW, float DelayS)
{
	X = PrevX = InX;
	Q = PrevQ = InQ.GetNormalized();
	V = FVector::ZeroVector;
	L = FVector::ZeroVector;
	UpdateAxes();
	LaunchV = InV;
	LaunchL = InertiaMul(InW);
	Delay = FMath::Max(DelayS, 0.0f);
	Age = FlightS = Accum = StillS = 0.0f;
	LastHitS = -1.0e9f;
	CapAtS = TurretSolver::CapS;
	ImpactsMade = 0;
	ScrapesMade = 0;
	ScrapePathCm = 0.0f;
	bLaunched = bAsleep = bCapped = bWasOnObstacle = false;
	for (FPointState& State : PointStates)
	{
		State = FPointState();
	}
	Impacts.Reset();
	Scrapes.Reset();
	TouchedObstacles.Reset();
	Supports.Reset();
	FirstTouchS = -1.0f;
	Hits = ObstacleTouches = Wakes = Steps = 0;
	MaxHitCmS = MaxObstacleHitCmS = 0.0f;
	SolveSeconds = 0.0;
}

void FRTSLooseTurret::Advance(float DeltaSeconds, const IRTSLooseTurretGround& Ground, TArrayView<FRTSTurretObstacle> Obstacles)
{
	if (bAsleep || DeltaSeconds <= 0.0f || Points.IsEmpty())
	{
		return;
	}
	const double T0 = FPlatformTime::Seconds();
	Accum += DeltaSeconds;
	int32 Taken = 0;
	while (Accum >= TurretSolver::StepS && Taken < TurretSolver::MaxStepsPerFrame && !bAsleep)
	{
		Accum -= TurretSolver::StepS;
		++Taken;
		PrevX = X;
		PrevQ = Q;
		Step(Ground, Obstacles);
	}
	Accum = FMath::Min(Accum, TurretSolver::StepS);
	if (bAsleep)
	{
		PrevX = X;
		PrevQ = Q;
		Accum = 0.0f;
	}
	SolveSeconds += FPlatformTime::Seconds() - T0;
}

FVector FRTSLooseTurret::ShownLocation() const
{
	const double A = bAsleep ? 1.0 : FMath::Clamp(double(Accum / TurretSolver::StepS), 0.0, 1.0);
	return PrevX + (X - PrevX) * A;
}

FQuat FRTSLooseTurret::ShownRotation() const
{
	const float A = bAsleep ? 1.0f : FMath::Clamp(Accum / TurretSolver::StepS, 0.0f, 1.0f);
	return FQuat::Slerp(PrevQ, Q, A);
}

void FRTSLooseTurret::UpdateAxes()
{
	Ax = Q.GetAxisX();
	Ay = Q.GetAxisY();
	Az = Q.GetAxisZ();
}

FVector FRTSLooseTurret::InvInertiaMul(const FVector& A) const
{
	return Ax * (InvK.X * FVector::DotProduct(Ax, A)) + Ay * (InvK.Y * FVector::DotProduct(Ay, A)) + Az * (InvK.Z * FVector::DotProduct(Az, A));
}

FVector FRTSLooseTurret::InertiaMul(const FVector& A) const
{
	return Ax * (K.X * FVector::DotProduct(Ax, A)) + Ay * (K.Y * FVector::DotProduct(Ay, A)) + Az * (K.Z * FVector::DotProduct(Az, A));
}

float FRTSLooseTurret::EffectiveInv(const FVector& R, const FVector& D) const
{
	return 1.0f + float(FVector::DotProduct(D, FVector::CrossProduct(InvInertiaMul(FVector::CrossProduct(R, D)), R)));
}

void FRTSLooseTurret::ApplyImpulse(const FVector& P, const FVector& R)
{
	V += P;
	L += FVector::CrossProduct(R, P);
}

bool FRTSLooseTurret::GroundSeparation(const IRTSLooseTurretGround& Ground, const FVector& P, float RadiusCm, float MarginCm, float& OutSep, FVector& OutN)
{
	// a distância ao plano tangente do chão sob o ponto: (z − h)·n.z − raio. A busca rápida (só a altura) descarta o ponto que
	// não chega a Margin nem numa rampa de 60°
	const FVector2D At(P.X, P.Y);
	const float Above = float(P.Z) - Ground.HeightCm(At);
	if (Above * TurretSolver::MinNormalZ - RadiusCm >= MarginCm)
	{
		return false;
	}
	OutN = Ground.Normal(At);
	OutSep = Above * float(OutN.Z) - RadiusCm;
	return true;
}

bool FRTSLooseTurret::BoxPenetration(const FRTSTurretObstacle& O, const FVector& P, float RadiusCm, float& OutDepth, FVector& OutN)
{
	const FVector Lp = O.Frame.InverseTransformPositionNoScale(P);
	double Best = TNumericLimits<double>::Max();
	int32 Axis = 0;
	double Sign = 1.0;
	for (int32 A = 0; A < 3; ++A)
	{
		const double Lo = O.Box.Min[A] - RadiusCm;
		const double Hi = O.Box.Max[A] + RadiusCm;
		if (Lp[A] <= Lo || Lp[A] >= Hi)
		{
			return false;
		}
		if (Lp[A] - Lo < Best)
		{
			Best = Lp[A] - Lo;
			Axis = A;
			Sign = -1.0;
		}
		if (Hi - Lp[A] < Best)
		{
			Best = Hi - Lp[A];
			Axis = A;
			Sign = 1.0;
		}
	}
	FVector Local = FVector::ZeroVector;
	Local[Axis] = Sign;
	OutN = O.Frame.TransformVectorNoScale(Local);
	OutDepth = float(Best);
	return true;
}

void FRTSLooseTurret::Step(const IRTSLooseTurretGround& Ground, TArrayView<FRTSTurretObstacle> Obstacles)
{
	Age += TurretSolver::StepS;
	if (!bLaunched)
	{
		if (Age < Delay)
		{
			return;   // a munição estourou; a torre sai 2 quadros depois do clarão (a receita aprovada do BlastLab v2)
		}
		bLaunched = true;
		V = LaunchV;
		L = LaunchL;
	}
	FlightS += TurretSolver::StepS;
	++Steps;
	V.Z -= TurretSolver::GravityCmS2 * TurretSolver::StepS;   // (o arrasto do ar é ~0,3% do peso: fica de fora)
	const FVector VBefore = V;

	// os obstáculos perto (as esferas se tocam): só eles entram nos testes dos pontos; o longe arma na hora (nada dentro dele).
	// Um obstáculo arma quando nenhum ponto está dentro dele (a torre nasce encostada no próprio casco)
	TArray<int32, TInlineAllocator<16>> Close;
	for (int32 Ob = 0; Ob < Obstacles.Num(); ++Ob)
	{
		FRTSTurretObstacle& O = Obstacles[Ob];
		if (!O.bActive)
		{
			continue;
		}
		const double Reach = BoundRadiusCm + O.Box.GetExtent().Size() + TurretSolver::ArmMarginCm + 50.0;
		if (FVector::DistSquared(X, O.Frame.TransformPositionNoScale(O.Box.GetCenter())) > Reach * Reach)
		{
			O.bArmed = true;
			continue;
		}
		Close.Add(Ob);
		if (O.bArmed)
		{
			continue;
		}
		bool bInside = false;
		for (const FPoint& Pt : Points)
		{
			float Depth = 0.0f;
			FVector Dummy;
			if (BoxPenetration(O, X + Q.RotateVector(Pt.P), Pt.RadiusCm + TurretSolver::ArmMarginCm, Depth, Dummy))
			{
				bInside = true;
				break;
			}
		}
		O.bArmed = !bInside;
	}

	// os contatos na pose atual: o chão (com a margem especulativa) ou, longe dele, a caixa de um obstáculo armado
	Contacts.Reset();
	const FVector W0 = InvInertiaMul(L);
	for (int32 I = 0; I < Points.Num(); ++I)
	{
		const FPoint& Pt = Points[I];
		FPointState& State = PointStates[I];
		TouchingNow[I] = false;
		const FVector R = Q.RotateVector(Pt.P);
		const FVector At = X + R;
		const FVector Vp = V + FVector::CrossProduct(W0, R);
		const float Margin = TurretSolver::SpeculativeCm + float(Vp.Size()) * TurretSolver::StepS;
		int32 Kind = -1;
		float Sep = 0.0f;
		float Rn = 0.0f;
		FVector N = FVector::UpVector;
		if (GroundSeparation(Ground, At, Pt.RadiusCm, Margin, Sep, N) && Sep < Margin)
		{
			Kind = 0;
			Rn = Ground.Restitution(FVector2D(At.X, At.Y));
		}
		else
		{
			for (const int32 Ob : Close)
			{
				float Depth = 0.0f;
				if (Obstacles[Ob].bArmed && BoxPenetration(Obstacles[Ob], At, Pt.RadiusCm, Depth, N))
				{
					Kind = 1 + Ob;
					Sep = -Depth;
					Rn = TurretSolver::SteelRestitution;
					break;
				}
			}
		}
		if (Kind < 0)
		{
			State = FPointState();
			continue;
		}
		if (State.Kind != Kind)
		{
			State.Acc = FVector::ZeroVector;   // trocou de superfície: sem partida quente
			State.Kind = Kind;
		}
		FContact& C = Contacts.AddDefaulted_GetRef();
		C.Index = I;
		C.Kind = Kind;
		C.R = R;
		C.At = At;
		C.N = N;
		C.T1 = TurretSolver::AnyPerpendicular(N);
		C.T2 = FVector::CrossProduct(N, C.T1);
		C.Kn = EffectiveInv(R, N);
		C.K1 = EffectiveInv(R, C.T1);
		C.K2 = EffectiveInv(R, C.T2);
		C.Vn0 = float(FVector::DotProduct(Vp, N));
		// o alvo da velocidade normal: o quique (se o ponto encosta neste passo e vem a mais de 1 m/s); acima do chão e sem
		// encostar, só chegar até ele (o especulativo); dentro, parar (a posição sai pela correção NGS)
		const float Approach = -C.Vn0;
		const bool bTouches = Sep <= 0.0f || Approach * TurretSolver::StepS > Sep;
		if (bTouches && Approach > TurretSolver::RestitutionThresholdCmS)
		{
			C.Bias = TurretSolver::ScaledRestitution(Rn, Approach) * Approach;
		}
		else if (Sep > 0.0f)
		{
			C.Bias = -Sep / TurretSolver::StepS;
		}
	}

	// a partida quente: os impulsos do passo anterior de cada ponto (os pontos são fixos no corpo)
	for (const FContact& C : Contacts)
	{
		const FVector& Acc = PointStates[C.Index].Acc;
		ApplyImpulse(C.N * Acc.X + C.T1 * Acc.Y + C.T2 * Acc.Z, C.R);
	}
	// os impulsos sequenciais: o atrito primeiro (preso no disco μ·normal acumulado), depois o normal (≥ 0)
	for (int32 It = 0; It < TurretSolver::VelocityIterations; ++It)
	{
		for (const FContact& C : Contacts)
		{
			FVector& Acc = PointStates[C.Index].Acc;
			FVector Vp = V + FVector::CrossProduct(InvInertiaMul(L), C.R);
			const FVector2D OldT(Acc.Y, Acc.Z);
			FVector2D NewT = OldT + FVector2D(-FVector::DotProduct(Vp, C.T1) / C.K1, -FVector::DotProduct(Vp, C.T2) / C.K2);
			const double MaxT = TurretSolver::Friction * Acc.X;
			if (NewT.SizeSquared() > MaxT * MaxT)
			{
				NewT = NewT.GetSafeNormal() * MaxT;
			}
			const FVector2D DeltaT = NewT - OldT;
			Acc.Y = NewT.X;
			Acc.Z = NewT.Y;
			ApplyImpulse(C.T1 * DeltaT.X + C.T2 * DeltaT.Y, C.R);
			Vp = V + FVector::CrossProduct(InvInertiaMul(L), C.R);
			const double OldN = Acc.X;
			Acc.X = FMath::Max(OldN - (FVector::DotProduct(Vp, C.N) - C.Bias) / C.Kn, 0.0);
			ApplyImpulse(C.N * (Acc.X - OldN), C.R);
		}
	}

	// integra: x pela velocidade, q pelo giro (ω = K⁻¹ no mundo · ℓ, na pose do começo do passo)
	X += V * TurretSolver::StepS;
	const FVector W1 = InvInertiaMul(L);
	const double Spin = W1.Size();
	if (Spin > 1.0e-9)
	{
		Q = FQuat(W1 / Spin, Spin * TurretSolver::StepS) * Q;
		Q.Normalize();
		UpdateAxes();
	}

	// a posição (NGS, Box2D v2.4): empurra pra fora o que entrou além da folga, ponto a ponto
	for (int32 It = 0; It < TurretSolver::PositionIterations; ++It)
	{
		for (const FPoint& Pt : Points)
		{
			const FVector R = Q.RotateVector(Pt.P);
			const FVector At = X + R;
			float Sep = 0.0f;
			FVector N = FVector::UpVector;
			bool bIn = GroundSeparation(Ground, At, Pt.RadiusCm, 0.0f, Sep, N) && Sep < 0.0f;
			if (!bIn)
			{
				for (const int32 Ob : Close)
				{
					float Depth = 0.0f;
					if (Obstacles[Ob].bArmed && BoxPenetration(Obstacles[Ob], At, Pt.RadiusCm, Depth, N))
					{
						Sep = -Depth;
						bIn = true;
						break;
					}
				}
			}
			if (!bIn)
			{
				continue;
			}
			const float Correction = FMath::Clamp(TurretSolver::Baumgarte * (Sep + TurretSolver::LinearSlopCm), -TurretSolver::MaxCorrectionCm, 0.0f);
			if (Correction >= 0.0f)
			{
				continue;
			}
			const double Push = -Correction / EffectiveInv(R, N);
			X += N * Push;
			const FVector Turn = InvInertiaMul(FVector::CrossProduct(R, N * Push));
			const double Angle = Turn.Size();
			if (Angle > 1.0e-12)
			{
				Q = FQuat(Turn / Angle, Angle) * Q;
				Q.Normalize();
				UpdateAxes();
			}
		}
	}

	// o que tocou de verdade (impulso normal > 0): a batida no chão (o pó e a marca), o encosto num obstáculo (e quais, pro
	// acordar) e o dormir
	float Hard = 0.0f;
	float ObstacleHard = 0.0f;
	FVector Sum = FVector::ZeroVector;
	double WSum = 0.0;
	bool bTouch = false;
	bool bOnObstacle = false;
	TouchedObstacles.Reset();
	for (const FContact& C : Contacts)
	{
		const double An = PointStates[C.Index].Acc.X;
		if (An <= 0.0)
		{
			continue;
		}
		bTouch = true;
		TouchingNow[C.Index] = true;
		if (C.Kind == 0)
		{
			Hard = FMath::Max(Hard, -C.Vn0);
			Sum += C.At * An;
			WSum += An;
		}
		else
		{
			bOnObstacle = true;
			ObstacleHard = FMath::Max(ObstacleHard, -C.Vn0);
			TouchedObstacles.AddUnique(C.Kind - 1);
		}
	}
	if (bOnObstacle && !bWasOnObstacle)
	{
		++ObstacleTouches;
	}
	bWasOnObstacle = bOnObstacle;
	MaxObstacleHitCmS = FMath::Max(MaxObstacleHitCmS, ObstacleHard);
	if (WSum > 0.0 && FirstTouchS < 0.0f)
	{
		FirstTouchS = FlightS;
		FirstTouchAt = X;
	}
	if (Hard > TurretSolver::RestitutionThresholdCmS && FlightS - LastHitS >= TurretSolver::ImpactSpacingS)
	{
		++Hits;
		MaxHitCmS = FMath::Max(MaxHitCmS, Hard);
		LastHitS = FlightS;
		if (ImpactsMade < TurretSolver::MaxImpacts)
		{
			++ImpactsMade;
			const FVector Center = Sum / WSum;
			float Spread = 0.0f;
			for (const FContact& C : Contacts)
			{
				if (C.Kind == 0 && PointStates[C.Index].Acc.X > 0.0)
				{
					Spread = FMath::Max(Spread, float(FVector::Dist2D(C.At, Center)));
				}
			}
			FRTSTurretImpact& Hit = Impacts.AddDefaulted_GetRef();
			Hit.AtCm = Center;
			Hit.SlideDir = FVector2D(VBefore.X, VBefore.Y).GetSafeNormal();
			Hit.NormalSpeedCmS = Hard;
			Hit.SpreadCm = Spread;
			Hit.TangentSpeedCmS = float(FVector2D(VBefore.X, VBefore.Y).Size());
		}
	}
	// (23/10, a areia do acerto) a raspada: os pontos que encostam no chão andando no plano (a velocidade de cada ponto depois do passo, sem
	// a parte da normal), pesados pelo impulso; um ponto a cada 0,5 m de caminho a mais de 1 m/s. Só lê o estado: a física não muda
	if (WSum > 0.0 && ScrapesMade < TurretSolver::MaxScrapes)
	{
		const FVector Wn = InvInertiaMul(L);
		FVector Slide = FVector::ZeroVector;
		for (const FContact& C : Contacts)
		{
			const double An = PointStates[C.Index].Acc.X;
			if (C.Kind == 0 && An > 0.0)
			{
				const FVector Vp = V + FVector::CrossProduct(Wn, C.R);
				Slide += (Vp - C.N * FVector::DotProduct(Vp, C.N)) * An;
			}
		}
		Slide /= WSum;
		const float Speed = float(FVector2D(Slide.X, Slide.Y).Size());
		if (Speed > TurretSolver::ScrapeMinCmS)
		{
			ScrapePathCm += Speed * TurretSolver::StepS;
			if (ScrapePathCm >= TurretSolver::ScrapeStepCm)
			{
				ScrapePathCm = 0.0f;
				FRTSTurretScrape& Sc = Scrapes.AddDefaulted_GetRef();
				Sc.AtCm = Sum / WSum;
				Sc.Dir = FVector2D(Slide.X, Slide.Y).GetSafeNormal();
				Sc.SpeedCmS = Speed;
				Sc.Index = ScrapesMade++;
			}
		}
	}
	const bool bStill = V.SizeSquared() < FMath::Square(TurretSolver::SleepLinearCmS) && InvInertiaMul(L).SizeSquared() < FMath::Square(TurretSolver::SleepAngularRadS);
	StillS = (bTouch && bStill) ? StillS + TurretSolver::StepS : 0.0f;
	if (StillS >= TurretSolver::SleepS)
	{
		GoToSleep(Ground, Obstacles, false);
	}
	else if (FlightS >= CapAtS)
	{
		GoToSleep(Ground, Obstacles, true);
	}
}

void FRTSLooseTurret::GoToSleep(const IRTSLooseTurretGround& Ground, TArrayView<FRTSTurretObstacle> Obstacles, bool bByCap)
{
	bAsleep = true;
	bCapped = bByCap;
	RestS = FlightS;
	V = FVector::ZeroVector;
	L = FVector::ZeroVector;
	// os obstáculos que a seguram (impulso normal no último passo) e a caixa de cada um agora: se um mexer ou sumir, ela acorda
	// (WakeIfSupportMoved)
	Supports.Reset();
	for (const int32 Ob : TouchedObstacles)
	{
		if (Obstacles.IsValidIndex(Ob) && Obstacles[Ob].bActive)
		{
			FSupport& S = Supports.AddDefaulted_GetRef();
			S.Index = Ob;
			S.Frame = Obstacles[Ob].Frame;
			S.Box = Obstacles[Ob].Box;
		}
	}
	// a folga do ponto mais baixo (a separação de verdade, pela normal do chão) e o menor chão sob os pontos encostados (o
	// afundar da carcaça desce a torre até o topo dela ficar abaixo dele)
	float Gap = TNumericLimits<float>::Max();
	float Lowest = TNumericLimits<float>::Max();
	for (const FPoint& Pt : Points)
	{
		const FVector At = X + Q.RotateVector(Pt.P);
		const FVector2D At2(At.X, At.Y);
		const float H = Ground.HeightCm(At2);
		const float Sep = (float(At.Z) - H) * float(Ground.Normal(At2).Z) - Pt.RadiusCm;
		Gap = FMath::Min(Gap, Sep);
		if (Sep < TurretSolver::SpeculativeCm)
		{
			Lowest = FMath::Min(Lowest, H);
		}
	}
	RestGapCm = Gap;
	RestGroundZ = Lowest < TNumericLimits<float>::Max() ? Lowest : Ground.HeightCm(FVector2D(X.X, X.Y));
}

void FRTSLooseTurret::Wake()
{
	bAsleep = false;
	bCapped = false;
	StillS = 0.0f;
	Accum = 0.0f;
	PrevX = X;
	PrevQ = Q;
	Supports.Reset();
	// um teto novo a partir daqui: o apoio que some depois do teto não deixa ela congelar no ar de novo no passo seguinte
	CapAtS = FlightS + TurretSolver::CapS;
	++Wakes;
}

bool FRTSLooseTurret::WakeIfSupportMoved(TArrayView<const FRTSTurretObstacle> Obstacles)
{
	if (!bAsleep || Supports.IsEmpty())
	{
		return false;
	}
	for (const FSupport& S : Supports)
	{
		const FRTSTurretObstacle* O = Obstacles.IsValidIndex(S.Index) ? &Obstacles[S.Index] : nullptr;
		// parado dentro da folga do solver (0,5 cm): o contato continua o mesmo
		if (O && O->bActive && O->Box.IsValid
			&& TurretSolver::MaxCornerShiftCm(S.Frame, S.Box, O->Frame, O->Box) <= TurretSolver::LinearSlopCm)
		{
			continue;
		}
		Wake();
		return true;
	}
	return false;
}

void FRTSLooseTurret::GetDebugPoints(TArray<FVector>& OutPoints, TArray<bool>& OutTouching) const
{
	OutPoints.Reset(Points.Num());
	for (const FPoint& Pt : Points)
	{
		OutPoints.Add(X + Q.RotateVector(Pt.P));
	}
	OutTouching = TouchingNow;
}

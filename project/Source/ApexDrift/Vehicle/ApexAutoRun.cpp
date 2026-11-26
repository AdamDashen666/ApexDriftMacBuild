#include "Vehicle/ApexAutoRun.h"
#include "Vehicle/ApexDriftVehiclePawn.h"
#include "Vehicle/ApexCarRegistry.h"
#include "Track/ApexTrackSplineActor.h"
#include "Gameplay/ApexDriftScoreComponent.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SplineComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "Engine/World.h"

namespace
{
	constexpr float LatAccel = 22.0f;        // 过弯侧向加速度 m/s2
	constexpr float BrakeAccel = 15.0f;
	constexpr float SpinTime = 1.40f;

	float Smooth01(float X)
	{
		X = FMath::Clamp(X, 0.0f, 1.0f);
		return X * X * (3.0f - 2.0f * X);
	}
	// 漂移角：弯越急角度越大
	float AngleForRadius(float R)
	{
		static const float Rr[] = { 14.f, 30.f, 60.f, 110.f, 180.f, 280.f };
		static const float Aa[] = { 58.f, 54.f, 47.f, 40.f, 33.f, 26.f };
		if (R <= Rr[0]) return Aa[0];
		for (int32 i = 1; i < UE_ARRAY_COUNT(Rr); ++i)
		{
			if (R < Rr[i]) return FMath::Lerp(Aa[i - 1], Aa[i], (R - Rr[i - 1]) / (Rr[i] - Rr[i - 1]));
		}
		return Aa[UE_ARRAY_COUNT(Aa) - 1];
	}
	float WrapDeg(float A) { return FMath::FindDeltaAngleDegrees(0.0f, A); }
}

// --------------------------------------------------------------------------- 计划
void FApexAutoRun::BuildLine(const AApexTrackSplineActor* Track, float StartDist, float StartLat)
{
	const USplineComponent* Sp = Track->TrackSpline;
	const float LenCm = Sp->GetSplineLength();
	LapLen = LenCm * 0.01f;
	const int32 N = FMath::Max(64, FMath::RoundToInt(LapLen / Ds));
	const float StepCm = LenCm / N;
	TArray<FVector2D> C, R;
	C.SetNum(N);
	R.SetNum(N);
	for (int32 i = 0; i < N; ++i)
	{
		const float D = FMath::Fmod(StartDist + i * StepCm, LenCm);
		const FVector P = Sp->GetLocationAtDistanceAlongSpline(D, ESplineCoordinateSpace::World);
		FVector Dir = Sp->GetDirectionAtDistanceAlongSpline(D, ESplineCoordinateSpace::World);
		Dir.Z = 0.0f;
		Dir.Normalize();
		C[i] = FVector2D(P.X, P.Y) * 0.01;
		R[i] = FVector2D(-Dir.Y, Dir.X);
	}
	// 实测每处路面边缘（有的图比配置的路宽窄，外面是高起的地形），横着时车头车尾也不出路面
	TArray<float> Lo, Hi;
	{
		UWorld* World = Track->GetWorld();
		TArray<float> Edge[2];
		Edge[0].Init(7.5f, N);
		Edge[1].Init(7.5f, N);
		FCollisionQueryParams Qp(SCENE_QUERY_STAT(ApexAutoEdge), true);
		for (TActorIterator<AApexDriftVehiclePawn> It(World); It; ++It) Qp.AddIgnoredActor(*It);
		for (int32 i = 0; i < N; i += 2)
		{
			const float D = FMath::Fmod(StartDist + i * StepCm, LenCm);
			const float Z = Sp->GetLocationAtDistanceAlongSpline(D, ESplineCoordinateSpace::World).Z;
			for (int32 Side = 0; Side < 2; ++Side)
			{
				const float Sg = Side ? 1.0f : -1.0f;
				float Last = 2.5f;
				FString Blk;
				for (float L = 3.0f; L <= 7.6f; L += 0.45f)
				{
					const FVector2D P = (C[i] + R[i] * (Sg * L)) * 100.0;
					FHitResult Hit;
					if (!World->LineTraceSingleByObjectType(Hit, FVector(P.X, P.Y, Z + 220.0f), FVector(P.X, P.Y, Z - 400.0f), FCollisionObjectQueryParams(ECC_WorldStatic), Qp)) break;
					const UStaticMeshComponent* Smc = Cast<UStaticMeshComponent>(Hit.GetComponent());
					if (!Smc || !Smc->GetStaticMesh() || !Smc->GetStaticMesh()->GetName().Contains(TEXT("Road")))
					{
						Blk = FString::Printf(TEXT("%s 高出 %.0fcm"), Smc && Smc->GetStaticMesh() ? *Smc->GetStaticMesh()->GetName() : TEXT("?"), Hit.ImpactPoint.Z - Z);
						break;
					}
					Last = L;
				}
				if (Last < 4.6f) Log.Add(FString::Printf(TEXT("窄 s=%.0f 侧%.0f 半宽 %.1f %s"), i * StepCm * 0.01f, Sg, Last, *Blk));
				Edge[Side][i] = Last;
				Edge[Side][(i + 1) % N] = Last;
			}
		}
		Lo.SetNum(N);
		Hi.SetNum(N);
		float MinHalf = 99.0f;
		for (int32 i = 0; i < N; ++i)
		{
			float L = 99.0f, H = 99.0f;
			for (int32 k = -3; k <= 3; ++k)
			{
				L = FMath::Min(L, Edge[0][((i + k) % N + N) % N]);
				H = FMath::Min(H, Edge[1][((i + k) % N + N) % N]);
			}
			MinHalf = FMath::Min3(MinHalf, L, H);
			Lo[i] = -FMath::Clamp(L - 2.75f, -1.0f, 3.1f);
			Hi[i] = FMath::Clamp(H - 2.75f, -1.0f, 3.1f);
			if (Lo[i] > Hi[i]) Lo[i] = Hi[i] = 0.5f * (Lo[i] + Hi[i]);
		}
		Log.Add(FString::Printf(TEXT("路面最窄半宽 %.1fm"), MinHalf));
	}
	// 走线：在路宽内多尺度“拉直”，自然得到外-内-外
	TArray<float> Off;
	Off.Init(0.0f, N);
	auto At = [&](int32 i) { const int32 k = ((i % N) + N) % N; return C[k] + R[k] * Off[k]; };
	static const int32 Scales[] = { 24, 16, 10, 6, 4, 2 };
	for (int32 K : Scales)
	{
		for (int32 It = 0; It < 160; ++It)
		{
			for (int32 i = 0; i < N; ++i)
			{
				const FVector2D Mid = (At(i - K) + At(i + K)) * 0.5;
				Off[i] = FMath::Clamp(Off[i] + 0.55f * static_cast<float>(FVector2D::DotProduct(Mid - At(i), R[i])), Lo[i], Hi[i]);
			}
		}
	}
	for (int32 i = 0; i < N; ++i) Off[i] = FMath::Clamp(Off[i], Lo[i], Hi[i]);
	TArray<float> Sm = Off;
	for (int32 It = 0; It < 30; ++It)
	{
		for (int32 i = 0; i < N; ++i) Sm[i] = 0.25f * Off[(i + N - 1) % N] + 0.5f * Off[i] + 0.25f * Off[(i + 1) % N];
		Swap(Sm, Off);
	}

	// 一圈 + 120m 余量（过线后还按计划走，免得在线上急停）
	const int32 M = N + 60;
	Q.SetNum(M);
	Center.SetNum(M);
	RightDir.SetNum(M);
	for (int32 i = 0; i < M; ++i)
	{
		const int32 k = i % N;
		const float Blend = (i < N / 2) ? Smooth01(i * Ds / 110.0f) : 1.0f;   // 从发车格并到走线上
		Center[i] = C[k];
		RightDir[i] = R[k];
		Q[i] = C[k] + R[k] * FMath::Lerp(StartLat, Off[k], Blend);
	}
	Kappa.SetNumZeroed(M);
	const int32 H = 4;
	for (int32 i = 0; i < M; ++i)
	{
		const int32 A = FMath::Max(0, i - H), B = FMath::Min(M - 1, i + H);
		const FVector2D U = Q[i] - Q[A], W = Q[B] - Q[i];
		const double Lu = U.Size(), Lw = W.Size();
		if (Lu < 0.5 || Lw < 0.5) continue;
		const double Cross = U.X * W.Y - U.Y * W.X, Dot = U.X * W.X + U.Y * W.Y;
		Kappa[i] = static_cast<float>(FMath::Atan2(Cross, Dot) / (0.5 * (Lu + Lw)));
	}
	TArray<float> Ks = Kappa;
	for (int32 It = 0; It < 4; ++It)
	{
		for (int32 i = 1; i + 1 < M; ++i) Ks[i] = 0.25f * Kappa[i - 1] + 0.5f * Kappa[i] + 0.25f * Kappa[i + 1];
		Kappa = Ks;
	}
}

void FApexAutoRun::BuildSpeed(const FApexCarInfo& Info)
{
	const int32 M = Q.Num();
	V.SetNum(M);
	const float Top = Info.TopKmh / 3.6f * 0.985f;
	for (int32 i = 0; i < M; ++i)
	{
		V[i] = FMath::Min(Top, FMath::Sqrt(LatAccel * Info.Grip / FMath::Max(FMath::Abs(Kappa[i]), 1e-5f)));
	}
	for (int32 i = M - 2; i >= 0; --i)
	{
		const float D = (Q[i + 1] - Q[i]).Size();
		V[i] = FMath::Min(V[i], FMath::Sqrt(V[i + 1] * V[i + 1] + 2.0f * BrakeAccel * D));
	}
	V[0] = 0.5f;
	for (int32 i = 1; i < M; ++i)
	{
		const float D = (Q[i] - Q[i - 1]).Size();
		// 侧向用掉的抓地越多，能给的加速度越少
		const float Use = FMath::Clamp(V[i - 1] * V[i - 1] * FMath::Abs(Kappa[i]) / (LatAccel * Info.Grip), 0.0f, 1.0f);
		const float A = AApexDriftVehiclePawn::TargetAccel(V[i - 1] * 3.6f, Info) * FMath::Lerp(0.95f, 0.45f, Use * Use);
		V[i] = FMath::Min(V[i], FMath::Sqrt(V[i - 1] * V[i - 1] + 2.0f * FMath::Max(A, 0.2f) * D));
	}
	T.SetNum(M);
	T[0] = 0.0f;
	for (int32 i = 1; i < M; ++i)
	{
		T[i] = T[i - 1] + (Q[i] - Q[i - 1]).Size() / FMath::Max(0.5f * (V[i] + V[i - 1]), 1.5f);
	}
}

float FApexAutoRun::SampleTime(const TArray<float>& A, float Time) const
{
	if (A.Num() == 0) return 0.0f;
	const float X = FMath::Clamp(Time / Dtp, 0.0f, static_cast<float>(A.Num() - 1));
	const int32 i = FMath::Min(FMath::FloorToInt(X), A.Num() - 2);
	return FMath::Lerp(A[i], A[i + 1], X - i);
}

void FApexAutoRun::BuildDrift(const FApexCarInfo& Info)
{
	const int32 M = Q.Num();
	const int32 Nt = FMath::CeilToInt(T[M - 1] / Dtp) + 2;
	TArray<float> Kt, Vt;
	Kt.SetNum(Nt);
	Vt.SetNum(Nt);
	{
		int32 i = 0;
		for (int32 t = 0; t < Nt; ++t)
		{
			const float Time = t * Dtp;
			while (i + 2 < M && T[i + 1] < Time) ++i;
			const float F = FMath::Clamp((Time - T[i]) / FMath::Max(T[i + 1] - T[i], 1e-3f), 0.0f, 1.0f);
			Kt[t] = FMath::Lerp(Kappa[i], Kappa[i + 1], F);
			Vt[t] = FMath::Lerp(V[i], V[i + 1], F);
		}
	}

	// ---- 弯：侧向需求够大、持续够久
	struct FSeg { int32 A, B; float Sign, Angle; };
	TArray<FSeg> Segs;
	for (int32 t = 0; t < Nt;)
	{
		const float S = FMath::Sign(Kt[t]);
		auto On = [&](int32 k) { return Vt[k] > 9.0f && Vt[k] * Vt[k] * FMath::Abs(Kt[k]) > 5.0f && FMath::Sign(Kt[k]) == S; };
		if (!On(t)) { ++t; continue; }
		int32 e = t;
		float PeakK = 0.0f;
		while (e < Nt && On(e)) { PeakK = FMath::Max(PeakK, FMath::Abs(Kt[e])); ++e; }
		if ((e - t) * Dtp > 0.35f) Segs.Add({ t, e, S, AngleForRadius(1.0f / FMath::Max(PeakK, 1e-4f)) });
		t = e;
	}
	// 同向、间隔短的并成一个弯，连着漂过去
	for (int32 s = 0; s + 1 < Segs.Num();)
	{
		if (Segs[s].Sign == Segs[s + 1].Sign && (Segs[s + 1].A - Segs[s].B) * Dtp < 1.3f)
		{
			Segs[s].B = Segs[s + 1].B;
			Segs[s].Angle = FMath::Max(Segs[s].Angle, Segs[s + 1].Angle);
			Segs.RemoveAt(s + 1);
		}
		else ++s;
	}
	Stats.Corners = Segs.Num();

	BasePsi.Init(0.0f, Nt);
	Corner.Init(0.0f, Nt);
	const int32 Lead = FMath::RoundToInt(0.55f / Dtp), Tail = FMath::RoundToInt(0.18f / Dtp);
	for (int32 s = 0; s < Segs.Num(); ++s)
	{
		FSeg& G = Segs[s];
		int32 A = G.A - Lead, B = G.B + Tail;
		if (s > 0) A = FMath::Max(A, (Segs[s - 1].B + G.A) / 2);
		if (s + 1 < Segs.Num()) B = FMath::Min(B, (G.B + Segs[s + 1].A) / 2);
		A = FMath::Max(A, 0);
		B = FMath::Min(B, Nt);
		float PeakK = 1e-4f;
		for (int32 t = FMath::Max(G.A, 0); t < FMath::Min(G.B, Nt); ++t) PeakK = FMath::Max(PeakK, FMath::Abs(Kt[t]));
		for (int32 t = A; t < B; ++t)
		{
			const float Local = (FMath::Sign(Kt[t]) == G.Sign) ? FMath::Clamp(FMath::Abs(Kt[t]) / PeakK, 0.0f, 1.0f) : 0.0f;
			BasePsi[t] = G.Sign * G.Angle * FMath::Lerp(0.72f, 1.0f, FMath::Sqrt(Local));
			Corner[t] = G.Sign;
		}
		G.A = A;
		G.B = B;
	}

	// ---- 360：直道后的入弯优先做“360 入弯”，长直道中段、长弯中段补一个
	Spins.Reset();
	const float MinGap = 5.0f;
	auto CanPlace = [&](float Ts) -> bool
	{
		for (const FSpin& S : Spins) if (FMath::Abs(S.Start - Ts) < MinGap) return false;
		const int32 a = FMath::RoundToInt(Ts / Dtp), b = FMath::RoundToInt((Ts + SpinTime) / Dtp);
		if (a < FMath::RoundToInt(4.0f / Dtp) || b >= Nt - 2) return false;
		for (int32 t = a; t <= b; ++t)
		{
			if (Vt[t] < 17.0f) return false;
			if (Vt[t] * Vt[t] * FMath::Abs(Kt[t]) > 24.0f) return false;
		}
		return true;
	};
	for (int32 s = 0; s < Segs.Num(); ++s)
	{
		const float GapBefore = (Segs[s].A - (s > 0 ? Segs[s - 1].B : 0)) * Dtp;
		if (GapBefore > 1.9f)
		{
			// 旋转在弯心之前收完，直接落进漂移角
			const float End = Segs[s].A * Dtp + 0.45f;
			if (CanPlace(End - SpinTime)) Spins.Add({ End - SpinTime, SpinTime, Segs[s].Sign, true });
		}
	}
	for (int32 s = 0; s <= Segs.Num(); ++s)
	{
		const float G0 = (s > 0 ? Segs[s - 1].B : 0) * Dtp, G1 = (s < Segs.Num() ? Segs[s].A : Nt - 1) * Dtp;
		if (G1 - G0 > 6.0f)
		{
			const float Mid = FMath::Lerp(G0, G1, 0.42f);
			if (CanPlace(Mid)) Spins.Add({ Mid, SpinTime, s < Segs.Num() ? Segs[s].Sign : 1.0f, false });
		}
	}
	for (const FSeg& G : Segs)
	{
		if ((G.B - G.A) * Dtp > 3.2f)
		{
			const float Mid = FMath::Lerp(G.A, G.B, 0.46f) * Dtp - 0.5f * SpinTime;
			if (CanPlace(Mid)) Spins.Add({ Mid, SpinTime, G.Sign, false });
		}
	}
	Spins.Sort([](const FSpin& L, const FSpin& R) { return L.Start < R.Start; });
	Stats.Spins = Spins.Num();

	// 钟摆：直道后的入弯先往反方向一点（360 入弯的不做）
	for (int32 s = 0; s < Segs.Num(); ++s)
	{
		const float GapBefore = (Segs[s].A - (s > 0 ? Segs[s - 1].B : 0)) * Dtp;
		bool bSpun = false;
		for (const FSpin& S : Spins) if (FMath::Abs(S.Start + S.Dur - Segs[s].A * Dtp) < 2.2f) bSpun = true;
		if (GapBefore > 1.5f && !bSpun && Vt[FMath::Max(Segs[s].A, 0)] > 22.0f)
		{
			const int32 A = FMath::Max(0, Segs[s].A - FMath::RoundToInt(0.5f / Dtp));
			for (int32 t = A; t < Segs[s].A; ++t) BasePsi[t] = -Segs[s].Sign * 15.0f;
		}
	}
	// 时间上平滑（零相位高斯）
	{
		const float Sig = 0.17f / Dtp;
		const int32 R = FMath::CeilToInt(Sig * 3.0f);
		TArray<float> W, Out;
		float Sum = 0.0f;
		for (int32 k = -R; k <= R; ++k) { W.Add(FMath::Exp(-0.5f * FMath::Square(k / Sig))); Sum += W.Last(); }
		Out.SetNum(Nt);
		for (int32 t = 0; t < Nt; ++t)
		{
			float A = 0.0f;
			for (int32 k = -R; k <= R; ++k) A += W[k + R] * BasePsi[FMath::Clamp(t + k, 0, Nt - 1)];
			Out[t] = A / Sum;
		}
		BasePsi = Out;
	}
	PsiPlan = BasePsi;
	for (const FSpin& S : Spins)
	{
		for (int32 t = 0; t < Nt; ++t) PsiPlan[t] += S.Dir * 360.0f * Smooth01((t * Dtp - S.Start) / S.Dur);
	}
}

bool FApexAutoRun::Prepare(const AApexTrackSplineActor* Track, const AApexDriftVehiclePawn& Car, const FApexCarInfo& Info)
{
	bActive = bFinished = bSpinning = bCmdValid = bReady = false;
	Stats = FStats();
	Log.Reset();
	if (!Track || !Track->TrackSpline || Track->TrackSpline->GetSplineLength() < 20000.0f)
	{
		return false;
	}
	FTransform Xf;
	float Dist = 0.0f, Lat = 0.0f;
	Track->FindClosestTrackTransform(Car.GetActorLocation(), Xf, Dist, Lat);
	BuildLine(Track, Dist, FMath::Clamp(Lat * 0.01f, -5.0f, 5.0f));
	BuildSpeed(Info);
	BuildDrift(Info);
	const int32 LapI = FMath::Min(FMath::RoundToInt(LapLen / Ds), T.Num() - 1);
	Stats.PlannedLap = T[LapI];
	Idx = 0;
	Elapsed = 0.0f;
	StuckT = 0.0f;
	LogT = 0.0f;
	SpinIdx = 0;
	SumV = 0.0;
	DriftT = 0.0;
	YawCmd = 0.0f;
	Gear = 1;
	Psi = 0.0f;
	AirT = 0.0f;
	bReady = true;
	Ei = 0.0f;
	OffT = 0.0f;
	Along = 0.0f;
	NextSpinIn = 1e6f;
	Log.Add(FString::Printf(TEXT("计划：全长 %.0fm 预计 %.1fs 弯 %d 个 360 共 %d 个"), LapLen, Stats.PlannedLap, Stats.Corners, Stats.Spins));
	for (const FSpin& S : Spins) Log.Add(FString::Printf(TEXT("  360 @ %.1fs 方向 %.0f %s"), S.Start, S.Dir, S.bEntry ? TEXT("入弯") : TEXT("")));
	return true;
}

void FApexAutoRun::NotifyHit(bool bWall, const FString& What)
{
	if (!bActive) return;
	if (bWall) ++Stats.WallHits;
	if (Log.Num() < 600) Log.Add(FString::Printf(TEXT("  t=%.2f s=%.0f lat=%.1f psi=%.0f %s"), Elapsed, Along, LatFromCenter, Psi, *What));
}

FVector2D FApexAutoRun::PathPoint(float Ahead, float Lat, FVector2D& OutDir) const
{
	const float X = FMath::Clamp((Along + Ahead) / Ds, 0.0f, static_cast<float>(Center.Num() - 2));
	const int32 i = FMath::FloorToInt(X);
	const FVector2D C = FMath::Lerp(Center[i], Center[i + 1], X - i);
	const FVector2D R = FMath::Lerp(RightDir[i], RightDir[i + 1], X - i);
	OutDir = FVector2D(R.Y, -R.X);
	return (C + R * Lat) * 100.0;
}

// --------------------------------------------------------------------------- 执行
void FApexAutoRun::Drive(AApexDriftVehiclePawn& Car, float Dt, bool bGrounded)
{
	USkeletalMeshComponent* Body = Car.GetMesh();
	UChaosWheeledVehicleMovementComponent* Move = Cast<UChaosWheeledVehicleMovementComponent>(Car.GetVehicleMovement());
	if (!bActive || !Body || !Move)
	{
		return;
	}
	Elapsed += Dt;

	const FVector Loc3 = Car.GetActorLocation();
	const FVector2D Pos(Loc3.X * 0.01, Loc3.Y * 0.01);
	const FVector Vel3 = Body->GetPhysicsLinearVelocity();
	const FVector2D Vel(Vel3.X * 0.01, Vel3.Y * 0.01);
	const float Speed = static_cast<float>(Vel.Size());

	// ---- 走到哪了（只往前找）
	{
		double Best = 1e18;
		int32 BestI = Idx;
		for (int32 i = FMath::Max(0, Idx - 3); i < FMath::Min(Q.Num() - 1, Idx + 40); ++i)
		{
			const double D = (Q[i] - Pos).SizeSquared();
			if (D < Best) { Best = D; BestI = i; }
		}
		Idx = BestI;
	}
	const int32 I1 = FMath::Min(Idx + 1, Q.Num() - 1);
	const int32 I0 = FMath::Max(I1 - 1, 0);
	const FVector2D SegV = Q[I1] - Q[I0];
	const double SegL = FMath::Max(SegV.Size(), 0.1);
	const float Frac = static_cast<float>(FMath::Clamp(FVector2D::DotProduct(Pos - Q[I0], SegV) / (SegL * SegL), -1.0, 2.0));
	const FVector2D Tan = SegV / SegL;
	const FVector2D Rt(-Tan.Y, Tan.X);
	const float E = static_cast<float>(FVector2D::DotProduct(Pos - Q[I0], Rt));       // 右正
	const float S = (I0 + Frac) * Ds;
	Along = S;
	auto Lerp1 = [&](const TArray<float>& A, float X) { const float Xi = FMath::Clamp(X, 0.0f, static_cast<float>(A.Num() - 1) - 0.001f); const int32 k = FMath::FloorToInt(Xi); return FMath::Lerp(A[k], A[k + 1], Xi - k); };
	const float Time = Lerp1(T, S / Ds);
	const float PathYaw = FMath::RadiansToDegrees(static_cast<float>(FMath::Atan2(Tan.Y, Tan.X)));
	LatFromCenter = static_cast<float>(FVector2D::DotProduct(Pos - Center[I0], RightDir[I0]));
	CornerDir = SampleTime(Corner, Time);

	const float VelYaw = Speed > 2.0f ? FMath::RadiansToDegrees(static_cast<float>(FMath::Atan2(Vel.Y, Vel.X))) : Car.GetActorRotation().Yaw;
	// 单帧的磕碰直接盖过去；方向或车速持续对不上（真的撞住了）才重新对齐
	const bool bOff = FMath::Abs(FMath::FindDeltaAngleDegrees(CmdYaw, VelYaw)) > 25.0f || FMath::Abs(CmdV - Speed) > 9.0f;
	OffT = bOff ? OffT + Dt : FMath::Max(0.0f, OffT - 2.0f * Dt);
	if (!bCmdValid || AirT > 0.6f || OffT > 0.25f)
	{
		CmdYaw = VelYaw;
		CmdV = Speed;
		OffT = 0.0f;
		bCmdValid = true;
	}

	// ---- 速度：跟计划，受加减速能力限制
	const float Want = Lerp1(V, (S + CmdV * 0.06f + 1.0f) / Ds);
	const FApexCarInfo& Info = Car.CarInfo();
	const float Acc = AApexDriftVehiclePawn::TargetAccel(CmdV * 3.6f, Info) * 1.05f + 0.5f;
	const float PrevV = CmdV;
	CmdV = FMath::FInterpConstantTo(CmdV, FMath::Max(Want, 3.0f), Dt, Want > CmdV ? Acc : BrakeAccel * 1.25f);
	const float LongAcc = (CmdV - PrevV) / FMath::Max(Dt, 1e-4f);

	// ---- 方向：曲率前馈 + 对走线偏差的临界阻尼修正
	const float Kff = Lerp1(Kappa, (S + CmdV * 0.5f * Dt) / Ds);
	const float Phi = FMath::DegreesToRadians(FMath::FindDeltaAngleDegrees(PathYaw, CmdYaw));
	const float Wn = 2.3f;
	const float Vm = FMath::Max(CmdV, 4.0f);
	Ei = FMath::Clamp(Ei + E * Dt, -0.8f, 0.8f);
	float Omega = Kff * CmdV - Wn * Wn * (E + 0.7f * Ei) / Vm - 2.0f * Wn * Phi;                     // rad/s
	const float MaxOmega = 30.0f / Vm;
	Omega = FMath::Clamp(Omega, -MaxOmega, MaxOmega);

	// ---- 车头：计划角（含 360）
	const float HeadYaw = Car.GetActorRotation().Yaw;
	const float Rate = CmdV / FMath::Max(Lerp1(V, S / Ds), 2.0f);
	const float PsiT = SampleTime(PsiPlan, Time);
	const float PsiDot = (SampleTime(PsiPlan, Time + 0.03f) - SampleTime(PsiPlan, Time - 0.03f)) / 0.06f * FMath::Clamp(Rate, 0.3f, 1.3f);
	Psi = FMath::FindDeltaAngleDegrees(CmdYaw, HeadYaw);
	const float PsiErr = WrapDeg(PsiT - Psi);
	bSpinning = false;
	NextSpinIn = 1e6f;
	for (const FSpin& Sp : Spins)
	{
		if (Time > Sp.Start && Time < Sp.Start + Sp.Dur) bSpinning = true;
		if (Sp.Start > Time) NextSpinIn = FMath::Min(NextSpinIn, Sp.Start - Time);
	}
	const float AbsPsi = FMath::Abs(Psi);

	// 过坎、压路肩的短暂离地也继续控制（拖久了才放手）
	AirT = bGrounded ? 0.0f : AirT + Dt;
	if (!bGrounded) ++Stats.AirFrames;
	if (AirT < 0.6f)
	{
		const float Low = FMath::Clamp((Speed - 3.0f) / 5.0f, 0.0f, 1.0f);             // 起步几米交给正常物理
		if (Low > 0.0f)
		{
			CmdYaw = WrapDeg(CmdYaw + FMath::RadiansToDegrees(Omega) * Dt);
			const float Yr = FMath::DegreesToRadians(CmdYaw);
			const FVector2D NewV = FVector2D(FMath::Cos(Yr), FMath::Sin(Yr)) * CmdV;
			const FVector2D Out = FMath::Lerp(Vel, NewV, Low);
			Body->SetPhysicsLinearVelocity(FVector(Out.X * 100.0, Out.Y * 100.0, Vel3.Z));

			const FVector Up = Car.GetActorUpVector();
			FVector Av = Body->GetPhysicsAngularVelocityInDegrees();
			const float Yaw = FVector::DotProduct(Av, Up);
			const float Wanted = FMath::Clamp(FMath::RadiansToDegrees(Omega) + PsiDot + 7.0f * PsiErr, -620.0f, 620.0f);
			YawCmd = FMath::FInterpConstantTo(YawCmd, Wanted, Dt, 3200.0f);
			Av += Up * ((YawCmd - Yaw) * Low);
			// 横着滑和旋转时压住侧倾
			const FVector Fwd = Car.GetActorForwardVector();
			const float RollRate = FVector::DotProduct(Av, Fwd);
			Av -= Fwd * (RollRate * FMath::Clamp(AbsPsi / 60.0f, 0.0f, 1.0f) * FMath::Min(1.0f, 4.0f * Dt));
			Body->SetPhysicsAngularVelocityInDegrees(Av);
		}
	}
	Body->AddForce(-Car.GetActorUpVector() * FMath::Min(1.5f + 0.0025f * Speed * Speed, 11.0f) * 100.0f * FMath::Max(500.0f, Body->GetMass()));

	// ---- 输入只管表现：前轮指向、油门声浪、刹车灯、轮子打滑
	const bool bBraking = LongAcc < -4.0f;
	const bool bSliding = AbsPsi > 12.0f;
	const float SteerDeg = AbsPsi < 100.0f ? FMath::Clamp(-Psi + FMath::RadiansToDegrees(FMath::Atan(2.9f * Kff)), -46.0f, 46.0f) : 0.0f;
	Car.AutoApplyInputs(bBraking && !bSpinning ? 0.15f : 1.0f, (bBraking && !bSpinning) ? FMath::Clamp(-LongAcc / 14.0f, 0.2f, 1.0f) : 0.0f, SteerDeg, false,
		bSpinning ? 0.45f : FMath::Lerp(3.0f, 0.85f, FMath::Clamp((AbsPsi - 4.0f) / 14.0f, 0.0f, 1.0f)),
		bSpinning ? 0.40f : FMath::Lerp(3.6f, 0.75f, FMath::Clamp((AbsPsi - 4.0f) / 14.0f, 0.0f, 1.0f)));
	{
		// 自己按车速选挡，旋转时锁住，避免倒着走时自动换到倒挡
		static const float UpKmh[] = { 0.f, 78.f, 118.f, 162.f, 208.f, 262.f, 1e6f };
		const float Kmh = CmdV * 3.6f;
		if (!bSpinning)
		{
			while (Gear < 6 && Kmh > UpKmh[Gear]) ++Gear;
			while (Gear > 1 && Kmh < UpKmh[Gear - 1] - 14.0f) --Gear;
		}
		if (Move->GetTargetGear() != Gear) Move->SetTargetGear(Gear, bSpinning || Move->GetCurrentGear() < 1);
	}

	// ---- 统计
	SumV += Speed * Dt;
	if (bSliding) DriftT += Dt;
	Stats.LapTime = Elapsed;
	Stats.AvgKmh = static_cast<float>(SumV / FMath::Max(Elapsed, 0.1f)) * 3.6f;
	Stats.MaxKmh = FMath::Max(Stats.MaxKmh, Speed * 3.6f);
	Stats.DriftPct = static_cast<float>(DriftT / FMath::Max(Elapsed, 0.1f)) * 100.0f;
	if (Speed > 8.0f)
	{
		Stats.MaxLatErr = FMath::Max(Stats.MaxLatErr, FMath::Abs(E));
		Stats.MaxOffCenter = FMath::Max(Stats.MaxOffCenter, FMath::Abs(LatFromCenter));
		if (bGrounded) Stats.WorstPsiErr = FMath::Max(Stats.WorstPsiErr, FMath::Abs(PsiErr));
		if (!bSpinning) Stats.MaxAngle = FMath::Max(Stats.MaxAngle, AbsPsi);
	}
	if (SpinIdx < Spins.Num() && Time > Spins[SpinIdx].Start + Spins[SpinIdx].Dur)
	{
		const bool bOk = FMath::Abs(PsiErr) < 25.0f;
		if (bOk) ++Stats.SpinsDone;
		Log.Add(FString::Printf(TEXT("360 #%d 完成 t=%.1f 角误差 %.1f v=%.0f"), SpinIdx, Elapsed, PsiErr, Speed * 3.6f));
		if (bOk && Car.DriftScoreComp) Car.DriftScoreComp->AwardSpin();
		++SpinIdx;
	}
	LogT += Dt;
	if (LogT >= ((FMath::Abs(E) > 0.8f || !bGrounded || FMath::Abs(PsiErr) > 8.0f) ? 0.05f : 0.5f))
	{
		LogT = 0.0f;
		Log.Add(FString::Printf(TEXT("  t=%.1f s=%.0f v=%.0f/%.0f e=%.2f lat=%.1f psi=%.0f/%.0f gear=%d rpm=%.0f%s%s"), Elapsed, S, Speed * 3.6f, Want * 3.6f, E, LatFromCenter, Psi, WrapDeg(PsiT),
			Move->GetCurrentGear(), Move->GetEngineRotationSpeed(), bSpinning ? TEXT(" SPIN") : TEXT(""), bGrounded ? TEXT("") : TEXT(" AIR")));
	}

	// ---- 结束
	if (S >= LapLen)
	{
		bFinished = true;
		bActive = false;
		Log.Add(TEXT("完成一圈"));
		return;
	}
	StuckT = (Elapsed > 4.0f && Speed < 3.0f) ? StuckT + Dt : 0.0f;
	if (StuckT > 2.5f)
	{
		bActive = false;
		Log.Add(TEXT("卡住，中断"));
	}
}

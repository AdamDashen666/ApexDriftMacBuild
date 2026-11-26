#include "Core/ApexBenchSubsystem.h"
#include "EngineUtils.h"
#include "Track/ApexPropField.h"
#include "Core/ApexDriftPlayerController.h"
#include "Core/ApexMapRegistry.h"
#include "Vehicle/ApexDriftVehiclePawn.h"
#include "Track/ApexTrackSplineActor.h"
#include "Components/SplineComponent.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "ChaosVehicleWheel.h"
#include "DynamicRHI.h"
#include "RHI.h"
#include "RenderTimer.h"
#include "RenderCore.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "HAL/FileManager.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
#include "DynamicResolutionState.h"

bool UApexBenchSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return FParse::Param(FCommandLine::Get(), TEXT("ApexBench")) && Super::ShouldCreateSubsystem(Outer);
}

void UApexBenchSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (!InWorld.IsGameWorld())
	{
		return;
	}
	bEnabled = true;
	bDrift = FParse::Param(FCommandLine::Get(), TEXT("ApexBenchDrift"));
	bShotUI = !FParse::Param(FCommandLine::Get(), TEXT("ApexBenchNoUI"));
	bPad = FParse::Param(FCommandLine::Get(), TEXT("ApexBenchPad"));
	bAuto = FParse::Param(FCommandLine::Get(), TEXT("ApexBenchAuto"));
	bKeys = FParse::Param(FCommandLine::Get(), TEXT("ApexBenchKeys"));
	bMonkey = FParse::Param(FCommandLine::Get(), TEXT("ApexBenchMonkey"));
	FParse::Value(FCommandLine::Get(), TEXT("ApexBenchSecs="), MaxSecs);
	FParse::Value(FCommandLine::Get(), TEXT("ApexBenchShots="), ShotInterval);
	FParse::Value(FCommandLine::Get(), TEXT("ApexBenchSpeed="), CruiseKmh);
	if (!FParse::Value(FCommandLine::Get(), TEXT("ApexBenchLabel="), Label))
	{
		Label = FApexMapRegistry::GetMaps()[FApexMapRegistry::FindIndexForWorld(&InWorld)].Id.ToString();
	}
	PhaseStart = FPlatformTime::Seconds();
	IFileManager::Get().MakeDirectory(*(FPaths::ProjectSavedDir() / TEXT("Bench")), true);
}

void UApexBenchSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	AApexDriftPlayerController* PC = Cast<AApexDriftPlayerController>(GetWorld()->GetFirstPlayerController());
	if (!PC || !PC->GetVehiclePawn())
	{
		return;
	}
	const double Now = FPlatformTime::Seconds();
	PC->bExternalDrive = true;

	if (Phase == 0)
	{
		// 预热：等流送与 PSO 缓存，菜单环视画面本身也算预热
		if (Now - PhaseStart > 7.0)
		{
			if (bAuto) PC->StartAutoRun(); else PC->StartRaceSession(EApexGameModeType::FreeDrift);
			if (bPad)
			{
				AStaticMeshActor* Floor = GetWorld()->SpawnActor<AStaticMeshActor>(FVector(0.0f, 0.0f, 150000.0f), FRotator::ZeroRotator);
				Floor->SetMobility(EComponentMobility::Movable);
				Floor->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
				Floor->SetActorScale3D(FVector(4000.0f, 4000.0f, 1.0f));
				Floor->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));
				PC->GetVehiclePawn()->SetActorTransform(FTransform(FRotator::ZeroRotator, FVector(-150000.0f, 0.0f, 150120.0f)), false, nullptr, ETeleportType::TeleportPhysics);
			}
			Phase = 1;
			PhaseStart = Now;
		}
		return;
	}
	if (Phase == 1)
	{
		if (PC->GetGameState() == EApexGameState::Playing)
		{
			Phase = 2;
			PhaseStart = Now;
			AutoStart = PC->GetVehiclePawn()->GetActorLocation();
			if (UChaosWheeledVehicleMovementComponent* Move = Cast<UChaosWheeledVehicleMovementComponent>(PC->GetVehiclePawn()->GetVehicleMovement()))
			{
				FString Off;
				for (int32 i = 0; i < Move->Wheels.Num(); ++i) Off += FString::Printf(TEXT("%.2f "), Move->Wheels[i]->GetSuspensionOffset());
				FHitResult Hit;
				const FVector Loc = PC->GetVehiclePawn()->GetActorLocation();
				GetWorld()->LineTraceSingleByChannel(Hit, Loc + FVector(0, 0, 100), Loc - FVector(0, 0, 300), ECC_WorldStatic);
				UE_LOG(LogTemp, Display, TEXT("[ApexBench] 静止悬挂偏移 %s 车身原点离地 %.2f"), *Off, Loc.Z - Hit.ImpactPoint.Z);
			}
			NextShot = 2.0f;
		}
		return;
	}
	if (Phase != 2)
	{
		return;
	}

	Elapsed += DeltaTime;
	if (bPad)
	{
		static const bool bSpin = FParse::Param(FCommandLine::Get(), TEXT("ApexBenchSpin"));
		if (bSpin) DriveSpin(DeltaTime); else if (bKeys) DriveKeys(DeltaTime); else DrivePad(DeltaTime);
		return;
	}
	static const bool bRamps = FParse::Param(FCommandLine::Get(), TEXT("ApexBenchRamps"));
	if (bRamps)
	{
		DriveRamps(DeltaTime);
		return;
	}
	if (bAuto)
	{
		AApexDriftVehiclePawn* Car = PC->GetVehiclePawn();
		if (Car->IsAutoRunning()) { bAutoSeen = true; AutoDoneT = 0.0f; }
		else if (bAutoSeen && (AutoDoneT += DeltaTime) > 1.2f)
		{
			const FApexAutoRun::FStats& A = Car->GetAutoRun().GetStats();
			TArray<FString> Out;
			Out.Add(FString::Printf(TEXT("%s 车%d 完成=%d 用时 %.2fs（计划 %.2f） 平均 %.0fkm/h 最高 %.0f 漂移占比 %.0f%% 弯 %d 360 %d/%d 最大漂移角 %.0f 走线偏差 %.2fm 离中心最远 %.2fm 角误差 %.1f 撞墙 %d 离地帧 %d 复位后 1.2s：车速 %.1f 离赛道起点 %.1fm"),
				*Label, Car->GetCarIndex(), Car->GetAutoRun().IsFinished() ? 1 : 0, A.LapTime, A.PlannedLap, A.AvgKmh, A.MaxKmh, A.DriftPct, A.Corners, A.SpinsDone, A.Spins, A.MaxAngle, A.MaxLatErr, A.MaxOffCenter, A.WorstPsiErr, A.WallHits, A.AirFrames, Car->GetTelemetry().SpeedKmh,
				PC->GetTrackActor() ? FVector::Dist2D(Car->GetActorLocation(), AutoStart) * 0.01f : -1.0f));
			Out.Append(Car->GetAutoRun().GetLog());
			FFileHelper::SaveStringArrayToFile(Out, *FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / FString::Printf(TEXT("Bench/%s_auto.txt"), *Label)), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
			Finish();
			Phase = 3;
			return;
		}
	}
	else
	Drive(DeltaTime);

	// ---- 采样（截图后 0.6 秒内的帧不计） ----
	ShotCooldown = FMath::Max(0.0f, ShotCooldown - DeltaTime);
	const float Ms = static_cast<float>(FApp::GetDeltaTime() * 1000.0);
	if (ShotCooldown <= 0.0f && Elapsed > 1.0f)
	{
		const float Gpu = FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles(0));
		FrameMs.Add(Ms);
		GpuMs.Add(Gpu);
		GameMs.Add(FPlatformTime::ToMilliseconds(GGameThreadTime));
		RenderMs.Add(FPlatformTime::ToMilliseconds(GRenderThreadTime));
		FDynamicResolutionStateInfos Infos;
		GEngine->GetDynamicResolutionCurrentStateInfos(Infos);
		ResFrac.Add(Infos.ResolutionFractionApproximations[GDynamicPrimaryResolutionFraction]);
		SecAccum += Ms;
		SecGpu += Gpu;
		SecWorst = FMath::Max(SecWorst, Ms);
		++SecFrames;
		if (SecAccum >= 1000.0f)
		{
			SecondLog.Add(FString::Printf(TEXT("{\"t\":%.1f,\"dist\":%.0f,\"fps\":%.1f,\"worstMs\":%.1f,\"gpuMs\":%.2f,\"speed\":%.0f,\"res\":%.2f}"),
				Elapsed, LastDist * 0.01f, SecFrames * 1000.0f / SecAccum, SecWorst, SecGpu / SecFrames, PC->GetVehiclePawn()->GetTelemetry().SpeedKmh, ResFrac.Last()));
			SecAccum = 0.0f; SecGpu = 0.0f; SecWorst = 0.0f; SecFrames = 0;
		}
	}

	if (ShotInterval > 0.0f && Elapsed >= NextShot)
	{
		NextShot += ShotInterval;
		ShotCooldown = 0.6f;
		FScreenshotRequest::RequestScreenshot(FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / FString::Printf(TEXT("Bench/%s_%02d.png"), *Label, ShotIndex++)), bShotUI, false);
		static const bool bCamCycle = FParse::Param(FCommandLine::Get(), TEXT("ApexBenchCamCycle"));   // 每次截图后换一个机位
		if (bCamCycle && PC->GetVehiclePawn()) PC->GetVehiclePawn()->CycleCameraMode();
	}

	const float TrackLen = PC->GetTrackActor() ? PC->GetTrackActor()->GetTrackLength() : 0.0f;
	if (Elapsed >= MaxSecs || (TrackLen > 0.0f && Travelled > TrackLen * 1.01f))
	{
		Finish();
		Phase = 3;
	}
}

void UApexBenchSubsystem::Drive(float DeltaTime)
{
	AApexDriftPlayerController* PC = Cast<AApexDriftPlayerController>(GetWorld()->GetFirstPlayerController());
	AApexDriftVehiclePawn* Car = PC ? PC->GetVehiclePawn() : nullptr;
	AApexTrackSplineActor* Track = PC ? PC->GetTrackActor() : nullptr;
	if (!Car || !Track || !Track->TrackSpline)
	{
		return;
	}
	const FApexDriftTelemetry& Tel = Car->GetTelemetry();
	USplineComponent* Spline = Track->TrackSpline;
	const float Len = FMath::Max(1000.0f, Spline->GetSplineLength());

	FTransform Xf;
	float Dist = 0.0f, Lat = 0.0f;
	Track->FindClosestTrackTransform(Car->GetActorLocation(), Xf, Dist, Lat);
	if (StartDist < 0.0f)
	{
		StartDist = Dist;
		LastDist = Dist;
	}
	float D = Dist - LastDist;
	if (D < -Len * 0.5f) D += Len;
	if (D > Len * 0.5f) D -= Len;
	if (FMath::Abs(D) < 6000.0f) Travelled += D;
	LastDist = Dist;

	auto DirAt = [&](float Ahead) -> FVector
	{
		FVector V = Spline->GetDirectionAtDistanceAlongSpline(FMath::Fmod(Dist + Ahead, Len), ESplineCoordinateSpace::World);
		V.Z = 0.0f;
		return V.GetSafeNormal();
	};
	auto PosAt = [&](float Ahead) -> FVector
	{
		return Spline->GetLocationAtDistanceAlongSpline(FMath::Fmod(Dist + Ahead, Len), ESplineCoordinateSpace::World);
	};

	const float SpeedCms = FMath::Max(Tel.SpeedKmh / 0.036f, 100.0f);
	// 前方每一段曲率允许的过弯速度，按刹车能力折算到当前位置（弯中 1.25g、刹车 1.2g）
	float AllowedCms = 1e6f;
	for (float A = 300.0f; A < 32000.0f; A += 900.0f)
	{
		const float C = FMath::Acos(FMath::Clamp(FVector::DotProduct(DirAt(A), DirAt(A + 1200.0f)), -1.0f, 1.0f)) / 1200.0f;
		const float Vc2 = (C > 1e-6f) ? 1230.0f / C : 1e12f;
		AllowedCms = FMath::Min(AllowedCms, FMath::Sqrt(Vc2 + 2.0f * 1180.0f * FMath::Max(0.0f, A - 600.0f)));
	}
	const float TargetKmh = FMath::Min(CruiseKmh, AllowedCms * 0.036f);

	// 纯跟踪转向
	const float LookAhead = FMath::Clamp(SpeedCms * 0.55f, 900.0f, 4200.0f);
	const FVector ToTarget = (PosAt(LookAhead) - Car->GetActorLocation()).GetSafeNormal2D();
	// 以速度方向为参照，漂移时同样成立
	FVector Ref = Car->GetVelocity().GetSafeNormal2D();
	if (Tel.SpeedKmh < 12.0f || !Car->IsDriftActive()) Ref = Car->GetActorForwardVector().GetSafeNormal2D();
	const float Err = FMath::RadiansToDegrees(FMath::Atan2(FVector::CrossProduct(Ref, ToTarget).Z, FVector::DotProduct(Ref, ToTarget)));
	float Steer = FMath::Clamp(Err / FMath::Lerp(14.0f, 9.0f, FMath::Clamp((Tel.SpeedKmh - 110.0f) / 150.0f, 0.0f, 1.0f)), -1.0f, 1.0f);

	float Throttle = FMath::Clamp((TargetKmh - Tel.SpeedKmh) * 0.09f + 0.22f, 0.0f, 1.0f);
	float Brake = FMath::Clamp((Tel.SpeedKmh - TargetKmh - 6.0f) * 0.045f, 0.0f, 1.0f);
	bool bHand = false;

	CornerCooldown = FMath::Max(0.0f, CornerCooldown - DeltaTime);
	if (bDrift)
	{
		const float TurnAhead = FMath::RadiansToDegrees(FMath::Atan2(FVector::CrossProduct(DirAt(0.0f), DirAt(2600.0f)).Z, FVector::DotProduct(DirAt(0.0f), DirAt(2600.0f))));
		if (!Car->IsDriftActive() && FMath::Abs(TurnAhead) > 22.0f && Tel.SpeedKmh > 45.0f && CornerCooldown <= 0.0f)
		{
			HandbrakeTimer = 0.40f;
			CornerCooldown = 2.5f;
		}
		if (HandbrakeTimer > 0.0f)
		{
			HandbrakeTimer -= DeltaTime;
			bHand = true;
			Steer = FMath::Sign(TurnAhead) * 0.9f;
			Throttle = 0.6f;
			Brake = 0.0f;
		}
		else if (Car->IsDriftActive())
		{
			Throttle = FMath::Max(Throttle, 0.75f);
			Brake = 0.0f;
		}
	}

	static const bool bStraight = FParse::Param(FCommandLine::Get(), TEXT("ApexBenchStraight"));   // 真实路面只按油门
	if (bStraight)
	{
		Steer = 0.0f; Throttle = 1.0f; Brake = 0.0f; bHand = false;
		PadLogTimer += DeltaTime;
		if (PadLogTimer >= 0.4f)
		{
			PadLogTimer = 0.0f;
			UE_LOG(LogTemp, Display, TEXT("[ApexStraight] t=%.1f v=%.0f lat=%.0f slip=%.2f yaw=%.1f roll=%.2f pitch=%.2f headErr=%.2f"), Elapsed, Tel.SpeedKmh, Lat, Tel.SlipAngleDeg,
				Car->GetMesh()->GetPhysicsAngularVelocityInDegrees().Z, Car->GetActorRotation().Roll, Car->GetActorRotation().Pitch,
				FMath::FindDeltaAngleDegrees(Xf.GetRotation().Rotator().Yaw, Car->GetActorRotation().Yaw));
		}
	}
	if (bMonkey)
	{
		MonkeyHold -= DeltaTime;
		if (MonkeyHold <= 0.0f)
		{
			MonkeySteer = FMath::Abs(Steer) > 0.30f ? FMath::Sign(Steer) : 0.0f;
			MonkeyHold = FMath::FRandRange(0.08f, 0.22f);
			if (FMath::FRand() < 0.07f) { MonkeySteer = FMath::RandBool() ? 1.0f : -1.0f; MonkeyHold = FMath::FRandRange(0.35f, 0.7f); }
		}
		MonkeyHandNext -= DeltaTime;
		if (MonkeyHandNext <= 0.0f && Tel.SpeedKmh > 60.0f) { MonkeyHand = FMath::FRandRange(0.2f, 0.5f); MonkeyHandNext = FMath::FRandRange(4.0f, 9.0f); }
		MonkeyHand -= DeltaTime;
		Steer = MonkeySteer;
		bHand = MonkeyHand > 0.0f;
		Throttle = (Tel.SpeedKmh > TargetKmh + 45.0f) ? 0.0f : 1.0f;
		Brake = (Tel.SpeedKmh > TargetKmh + 45.0f) ? 1.0f : 0.0f;

		const bool bD = Car->IsDriftActive();
		if (bD && CurDrift <= 0.0f) ++DriftRuns;
		CurDrift = bD ? CurDrift + DeltaTime : 0.0f;
		MaxDriftRun = FMath::Max(MaxDriftRun, CurDrift);
		const bool bCrooked = !bHand && FMath::Abs(Tel.SlipAngleDeg) > 14.0f && Tel.SpeedKmh > 30.0f && (Steer == 0.0f || Steer * Tel.SlipAngleDeg > 0.0f);
		CurCrooked = bCrooked ? CurCrooked + DeltaTime : 0.0f;
		MaxCrooked = FMath::Max(MaxCrooked, CurCrooked);
	}
	Car->SetThrottleInput(Throttle);
	Car->SetBrakeInput(Brake);
	Car->SetSteeringInput(Steer);
	Car->SetHandbrakeInput(bHand);
	ShiftCooldown = FMath::Max(0.0f, ShiftCooldown - DeltaTime);
	if (Tel.bManualGearbox && ShiftCooldown <= 0.0f && Tel.Gear > 0)
	{
		if (Tel.EngineRPM > Tel.MaxRPM * 0.90f) { Car->ShiftUp(); ShiftCooldown = 0.4f; }
		else if (Tel.EngineRPM < Tel.MaxRPM * 0.42f && Tel.Gear > 1) { Car->ShiftDown(); ShiftCooldown = 0.4f; }
	}

	// ---- 操控统计 ----
	MaxSpeed = FMath::Max(MaxSpeed, Tel.SpeedKmh);
	const float AbsSlip = FMath::Abs(Tel.SlipAngleDeg);
	if (Tel.SpeedKmh > 25.0f)
	{
		if (Car->IsDriftActive())
		{
			MaxSlipDrift = FMath::Max(MaxSlipDrift, AbsSlip);
			DriftSeconds += DeltaTime;
		}
		else
		{
			MaxSlipGrip = FMath::Max(MaxSlipGrip, AbsSlip);
		}
	}
	const bool bBackwards = FVector::DotProduct(Car->GetActorForwardVector(), DirAt(0.0f)) < -0.2f;
	if (bBackwards && !bSpinning) ++Spins;
	bSpinning = bBackwards;
	if (FMath::Abs(Lat) > MaxLateral)
	{
		MaxLateral = FMath::Abs(Lat);
		if (MaxLateral > 950.0f) UE_LOG(LogTemp, Display, TEXT("[ApexBench] lat=%.0f dist=%.0f v=%.0f pos=%s up=%.2f"), Lat, Dist * 0.01f, Tel.SpeedKmh, *Car->GetActorLocation().ToCompactString(), Car->GetActorUpVector().Z);
	}
	if (FMath::Abs(Lat) > Track->TrackWidthCm * 0.5f) OffTrackSeconds += DeltaTime;

	StuckTimer = (Tel.SpeedKmh < 6.0f || Car->GetActorUpVector().Z < 0.3f || bBackwards) ? StuckTimer + DeltaTime : 0.0f;
	if (StuckTimer > 2.5f)
	{
		StuckTimer = 0.0f;
		++Resets;
		Car->ResetToTrack();
	}
}

void UApexBenchSubsystem::DrivePad(float DeltaTime)
{
	AApexDriftPlayerController* PC = Cast<AApexDriftPlayerController>(GetWorld()->GetFirstPlayerController());
	AApexDriftVehiclePawn* Car = PC ? PC->GetVehiclePawn() : nullptr;
	if (!Car) return;
	const float T = Elapsed;
	float Thr = 0.0f, Brk = 0.0f, Str = 0.0f;
	bool bHand = false;
	const TCHAR* Stage = TEXT("");
	if (T < 4.5f)        { Stage = TEXT("加速"); Thr = 1.0f; }
	else if (T < 4.85f)  { Stage = TEXT("手刹入弯"); Thr = 0.6f; Str = 1.0f; bHand = true; }
	else if (T < 9.5f)   { Stage = TEXT("右漂保持"); Thr = 1.0f; Str = 0.3f; }
	else if (T < 11.5f)  { Stage = TEXT("反打换向"); Thr = 1.0f; Str = -1.0f; }
	else if (T < 14.5f)  { Stage = TEXT("左漂保持"); Thr = 1.0f; Str = -0.3f; }
	else if (T < 17.0f)  { Stage = TEXT("带油回正"); Thr = 1.0f; }
	else if (T < 20.0f)  { Stage = TEXT("抓地加速"); Thr = 1.0f; }
	else if (T < 23.0f)  { Stage = TEXT("满舵右"); Thr = 0.7f; Str = 1.0f; }
	else if (T < 26.0f)  { Stage = TEXT("满舵左"); Thr = 0.7f; Str = -1.0f; }
	else if (T < 29.0f)  { Stage = TEXT("全力刹车"); Brk = 1.0f; }
	else { Finish(); Phase = 3; return; }
	Car->SetThrottleInput(Thr);
	Car->SetBrakeInput(Brk);
	Car->SetSteeringInput(Str);
	Car->SetHandbrakeInput(bHand);
	PadLogTimer += DeltaTime;
	if (PadLogTimer >= 0.25f)
	{
		PadLogTimer = 0.0f;
		const FApexDriftTelemetry& Tel = Car->GetTelemetry();
		const FVector L = Car->GetActorLocation();
		PadLog.Add(FString::Printf(TEXT("%5.2f %s v=%.0f gear=%d slip=%.1f drift=%d yawRate=%.0f pos=(%.0f,%.0f) up=%.2f"), T, Stage, Tel.SpeedKmh, Tel.Gear, Tel.SlipAngleDeg,
			Car->IsDriftActive() ? 1 : 0, Car->GetMesh()->GetPhysicsAngularVelocityInDegrees().Z, L.X * 0.01f, L.Y * 0.01f, Car->GetActorUpVector().Z));
	}
}

namespace
{
	struct FKeySeg { const TCHAR* Name; float Dur; float Thr; float Brk; float Str; bool bHand; float YawKick; bool bReset; bool bMeasure; };
	// 键盘玩家：W 基本不松，方向只有 -1/0/+1
	const FKeySeg GKeySegs[] = {
		{ TEXT("A 起步加速"), 6.0f, 1, 0, 0, false, 0, true, false },
		{ TEXT("A 手刹+右"), 0.35f, 1, 0, 1, true, 0, false, false },
		{ TEXT("A 右打漂过弯"), 1.6f, 1, 0, 1, false, 0, false, false },
		{ TEXT("A 反打想拉直(1秒)"), 1.0f, 1, 0, -1, false, 0, false, false },
		{ TEXT("A 松方向"), 2.5f, 1, 0, 0, false, 0, false, true },

		{ TEXT("B 起步加速"), 6.0f, 1, 0, 0, false, 0, true, false },
		{ TEXT("B 手刹+右"), 0.35f, 1, 0, 1, true, 0, false, false },
		{ TEXT("B 右打"), 1.2f, 1, 0, 1, false, 0, false, false },
		{ TEXT("B 左右乱点1"), 0.35f, 1, 0, -1, false, 0, false, false },
		{ TEXT("B 左右乱点2"), 0.35f, 1, 0, 1, false, 0, false, false },
		{ TEXT("B 左右乱点3"), 0.35f, 1, 0, -1, false, 0, false, false },
		{ TEXT("B 左右乱点4"), 0.35f, 1, 0, 1, false, 0, false, false },
		{ TEXT("B 左右乱点5"), 0.35f, 1, 0, -1, false, 0, false, false },
		{ TEXT("B 左右乱点6"), 0.35f, 1, 0, 1, false, 0, false, false },
		{ TEXT("B 松方向"), 2.5f, 1, 0, 0, false, 0, false, true },

		{ TEXT("C 起步"), 2.6f, 1, 0, 0, false, 0, true, false },
		{ TEXT("C 低速满油满舵过弯"), 1.6f, 1, 0, 1, false, 0, false, false },
		{ TEXT("C 松方向"), 2.5f, 1, 0, 0, false, 0, false, true },

		{ TEXT("D 高速加速"), 9.0f, 1, 0, 0, false, 0, true, false },
		{ TEXT("D 被撞出横摆"), 0.05f, 1, 0, 0, false, 150, false, false },
		{ TEXT("D 反打救车"), 0.8f, 1, 0, -1, false, 0, false, false },
		{ TEXT("D 松方向"), 2.5f, 1, 0, 0, false, 0, false, true },

		{ TEXT("E 起步"), 5.0f, 1, 0, 0, false, 0, true, false },
		{ TEXT("E 手刹+右"), 0.35f, 1, 0, 1, true, 0, false, false },
		{ TEXT("E 右打持续漂"), 3.0f, 1, 0, 1, false, 0, false, false },
		{ TEXT("E 直接松方向"), 2.5f, 1, 0, 0, false, 0, false, true },

		{ TEXT("F 起步"), 5.0f, 1, 0, 0, false, 0, true, false },
		{ TEXT("F 手刹+右"), 0.35f, 1, 0, 1, true, 0, false, false },
		{ TEXT("F 右漂"), 1.5f, 1, 0, 1, false, 0, false, false },
		{ TEXT("F 手刹+左换向"), 0.35f, 1, 0, -1, true, 0, false, false },
		{ TEXT("F 左漂"), 2.0f, 1, 0, -1, false, 0, false, false },
		{ TEXT("F 松方向"), 2.5f, 1, 0, 0, false, 0, false, true },

		{ TEXT("S60 起步"), 2.2f, 1, 0, 0, false, 0, true, false },
		{ TEXT("S60 点右0.25s"), 0.25f, 1, 0, 1, false, 0, false, false },
		{ TEXT("S60 松"), 2.0f, 1, 0, 0, false, 0, false, true },
		{ TEXT("S60 半舵右1s"), 1.0f, 1, 0, 0.4f, false, 0, false, false },
		{ TEXT("S60 松2"), 2.0f, 1, 0, 0, false, 0, false, true },
		{ TEXT("S120 起步"), 4.0f, 1, 0, 0, false, 0, true, false },
		{ TEXT("S120 满舵右1s"), 1.0f, 0.6f, 0, 1, false, 0, false, false },
		{ TEXT("S120 松"), 2.0f, 1, 0, 0, false, 0, false, true },
		{ TEXT("S180 起步"), 6.0f, 1, 0, 0, false, 0, true, false },
		{ TEXT("S180 点右0.25s"), 0.25f, 1, 0, 1, false, 0, false, false },
		{ TEXT("S180 点左0.25s"), 0.25f, 1, 0, -1, false, 0, false, false },
		{ TEXT("S180 松"), 2.0f, 1, 0, 0, false, 0, false, true },
		{ TEXT("S180 满舵右1.5s"), 1.5f, 1, 0, 1, false, 0, false, false },
		{ TEXT("S180 松2"), 2.5f, 1, 0, 0, false, 0, false, true },
		{ TEXT("S 带刹车入弯"), 1.2f, 0, 0.6f, 1, false, 0, false, false },
		{ TEXT("S 带刹车松"), 2.0f, 0, 0, 0, false, 0, false, true },
		{ TEXT("G 全油门直线"), 26.0f, 1, 0, 0, false, 0, true, false },
		{ TEXT("G 高速满舵右"), 2.5f, 1, 0, 1, false, 0, false, false },
		{ TEXT("G 回正"), 1.5f, 1, 0, 0, false, 0, false, true },
		{ TEXT("G 全力刹车"), 8.0f, 0, 1, 0, false, 0, false, false },
	};
}

void UApexBenchSubsystem::DriveSpin(float DeltaTime)
{
	// 每组：起步到目标速度、手刹+满舵按住 Hold 秒、松手直行 3 秒
	struct FSpinCase { float Kmh; float Hold; float Dir; };
	static const FSpinCase Cases[] = { { 90.f, 0.35f, 1 }, { 90.f, 0.8f, 1 }, { 120.f, 1.2f, 1 }, { 120.f, 2.0f, -1 }, { 160.f, 1.2f, -1 }, { 200.f, 1.2f, 1 }, { 120.f, 4.0f, 1 } };
	AApexDriftPlayerController* PC = Cast<AApexDriftPlayerController>(GetWorld()->GetFirstPlayerController());
	AApexDriftVehiclePawn* Car = PC ? PC->GetVehiclePawn() : nullptr;
	if (!Car) return;
	const int32 Idx = KeySeg / 3, Stage = KeySeg % 3;
	if (Idx >= UE_ARRAY_COUNT(Cases)) { Finish(); Phase = 3; return; }
	const FSpinCase& C = Cases[Idx];
	const FApexDriftTelemetry& Tel = Car->GetTelemetry();
	if (KeySegT <= 0.0f && Stage == 0)
	{
		Car->ResetToTrack();
		Car->SetActorTransform(FTransform(FRotator::ZeroRotator, FVector(-150000.0f, 0.0f, 150120.0f)), false, nullptr, ETeleportType::TeleportPhysics);
		Car->GetMesh()->SetPhysicsLinearVelocity(FVector::ZeroVector);
		Car->GetMesh()->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
	}
	KeySegT += DeltaTime;
	const FVector Vel = Car->GetVelocity();
	const float Psi = Vel.Size2D() > 200.0f ? FMath::FindDeltaAngleDegrees(FMath::RadiansToDegrees(FMath::Atan2(Vel.Y, Vel.X)), Car->GetActorRotation().Yaw) : 0.0f;
	if (Stage == 0) { SpinAccum = 0.0f; SpinPrev = Psi; SpinPeak = 0.0f; SpinMinV = 1e6f; }
	else
	{
		SpinAccum += FMath::FindDeltaAngleDegrees(SpinPrev, Psi);
		SpinPrev = Psi;
		SpinPeak = FMath::Max(SpinPeak, FMath::Abs(SpinAccum));
		SpinMinV = FMath::Min(SpinMinV, Tel.SpeedKmh);
	}
	Car->SetThrottleInput(1.0f);
	Car->SetBrakeInput(0.0f);
	Car->SetSteeringInput(Stage == 1 ? C.Dir : 0.0f);
	Car->SetHandbrakeInput(Stage == 1);
	PadLogTimer += DeltaTime;
	if (Stage > 0 && PadLogTimer >= 0.2f)
	{
		PadLogTimer = 0.0f;
		PadLog.Add(FString::Printf(TEXT("  #%d stage%d t=%.2f v=%.0f psi=%.0f 累计=%.0f yaw=%.0f"), Idx, Stage, KeySegT, Tel.SpeedKmh, Psi, SpinAccum, Car->GetMesh()->GetPhysicsAngularVelocityInDegrees().Z));
	}
	if (ShotInterval > 0.0f && Idx == 2 && Stage > 0 && Elapsed >= NextShot)
	{
		NextShot = Elapsed + ShotInterval;
		FScreenshotRequest::RequestScreenshot(FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / FString::Printf(TEXT("Bench/%s_%02d.png"), *Label, ShotIndex++)), bShotUI, false);
	}
	const bool bNext = (Stage == 0) ? (KeySegT > 0.6f && (Tel.SpeedKmh >= C.Kmh || KeySegT > 9.0f)) : (Stage == 1 ? KeySegT >= C.Hold : KeySegT >= 3.0f);
	if (bNext)
	{
		if (Stage == 2)
		{
			PadLog.Add(FString::Printf(TEXT("=== %.0fkm/h 手刹+满舵按住%.2fs：车头最多转过 %.0f°，最终 %.0f°，最低车速 %.0f，结束 v=%.0f slip=%.1f"), C.Kmh, C.Hold, SpinPeak, SpinAccum, SpinMinV, Tel.SpeedKmh, Tel.SlipAngleDeg));
		}
		++KeySeg;
		KeySegT = 0.0f;
	}
}

void UApexBenchSubsystem::DriveRamps(float DeltaTime)
{
	struct FRampCase { const TCHAR* Name; float X, Y, Yaw, Kmh, Secs, Steer; };
	static const FRampCase Cases[] = {
		{ TEXT("08度跳台"), -190, 330, 0, 160, 5.0f, 0 }, { TEXT("14度跳台"), -190, 300, 0, 160, 5.5f, 0 }, { TEXT("20度跳台"), -190, 270, 0, 150, 6.0f, 0 }, { TEXT("28度跳台"), -190, 240, 0, 140, 6.5f, 0 },
		{ TEXT("桌面跳台"), -190, 210, 0, 120, 5.5f, 0 }, { TEXT("断桥"), -190, 180, 0, 110, 5.5f, 0 }, { TEXT("波浪路"), -190, 150, 0, 110, 6.0f, 0 }, { TEXT("登台"), -190, 120, 0, 110, 6.0f, 0 },
		{ TEXT("侧倾坡"), -150, 90, 0, 90, 7.0f, 0 }, { TEXT("回程22度"), 300, 255, 180, 130, 5.5f, 0 }, { TEXT("回程双峰"), 300, 225, 180, 90, 5.5f, 0 }, { TEXT("二次起跳"), 300, 195, 180, 110, 6.0f, 0 },
		{ TEXT("超级跳台200"), -260, -265, 0, 200, 8.0f, 0 }, { TEXT("超级跳台150"), -260, -265, 0, 150, 8.0f, 0 }, { TEXT("金字塔"), -112, -170, 90, 90, 5.0f, 0 }, { TEXT("立交桥"), 78, -110, 90, 100, 6.0f, 0 },
		{ TEXT("半管斜冲"), -110, -196, 9, 100, 5.0f, 0 }, { TEXT("贴墙弯"), 190, -190, 35, 110, 5.0f, 0.45f }, { TEXT("碗池贴边"), -90, -190, 180, 80, 9.0f, 0.0f }, { TEXT("横坡"), 200, -262, 0, 110, 5.0f, 0 }, { TEXT("土包场"), 150, -60, 0, 80, 5.0f, 0 }, { TEXT("螺旋塔"), 60, -178, 0, 60, 22.0f, 0 },
		{ TEXT("撞木箱墙"), 60, -10, 0, 140, 4.5f, 0 }, { TEXT("撞泡沫墙"), 100, 28, 0, 140, 4.5f, 0 }, { TEXT("撞油桶"), 92, 52, 0, 110, 4.0f, 0 }, { TEXT("打保龄球瓶"), 60, 0, 0, 170, 5.0f, 0 }, { TEXT("绕桩直冲"), -290, -14, 0, 130, 5.0f, 0 },
	};
	AApexDriftPlayerController* PC = Cast<AApexDriftPlayerController>(GetWorld()->GetFirstPlayerController());
	AApexDriftVehiclePawn* Car = PC ? PC->GetVehiclePawn() : nullptr;
	if (!Car) return;
	AApexPropField* Field = nullptr;
	for (TActorIterator<AApexPropField> It(GetWorld()); It; ++It) { Field = *It; break; }
	static int32 Only = -2;
	if (Only == -2)
	{
		Only = -1;
		FParse::Value(FCommandLine::Get(), TEXT("ApexRampOnly="), Only);
		if (Only >= 0) KeySeg = Only;
	}
	if (KeySeg >= UE_ARRAY_COUNT(Cases) || (Only >= 0 && KeySeg > Only)) { Finish(); Phase = 3; return; }
	const FRampCase& C = Cases[KeySeg];
	const FApexDriftTelemetry& Tel = Car->GetTelemetry();
	const FRotator Rot(0.0f, C.Yaw, 0.0f);
	const FVector Start(C.X * 100.0f, C.Y * 100.0f, 45.0f);
	if (KeySegT <= 0.0f)
	{
		if (Field) Field->ResetAll();
		Car->ResetToTrack();
		Car->SetActorTransform(FTransform(Rot, Start), false, nullptr, ETeleportType::TeleportPhysics);
		Car->GetMesh()->SetPhysicsLinearVelocity(Rot.Vector() * C.Kmh / 0.036f * 0.85f);
		Car->GetMesh()->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
		RampAir = 0.0f; RampMaxZ = 0.0f; RampMinUp = 1.0f; RampLat = 0.0f; RampShot = 0; RampWorstMs = 0.0f;
	}
	KeySegT += DeltaTime;
	RampWorstMs = KeySegT > 0.5f ? FMath::Max(RampWorstMs, (float)FApp::GetDeltaTime() * 1000.0f) : 0.0f;
	float Steer = C.Steer;
	const FVector P = Car->GetActorLocation();
	const FVector Rel = Rot.UnrotateVector(P - Start);
	if (FCString::Strcmp(C.Name, TEXT("螺旋塔")) == 0)
	{
		// 绕塔心 28m 半径开上去
		const FVector2D D = FVector2D(P.X, P.Y) - FVector2D(17000.0f, -15000.0f);
		if (P.X > 16500.0f || KeySegT > 6.0f)
		{
			const FVector2D Tan = FVector2D(-D.Y, D.X).GetSafeNormal();
			const float Want = FMath::RadiansToDegrees(FMath::Atan2(Tan.Y, Tan.X)) + FMath::Clamp((D.Size() - 2800.0f) * 0.05f, -20.0f, 20.0f);
			const bool bOut = P.Z > 1690.0f && D.Y > 2000.0f;
			Steer = FMath::Clamp(FMath::FindDeltaAngleDegrees(Car->GetActorRotation().Yaw, bOut ? 180.0f : Want) / 12.0f + (bOut ? 0.0f : 0.35f), -1.0f, 1.0f);
		}
	}
	else if (FCString::Strcmp(C.Name, TEXT("碗池贴边")) == 0 && P.X < -17000.0f)
	{
		Steer = 0.75f;
	}
	else if (C.Steer == 0.0f && Car->GetTelemetry().SpeedKmh > 20.0f)
	{
		Steer = FMath::Clamp(FMath::FindDeltaAngleDegrees(Car->GetActorRotation().Yaw, C.Yaw - FMath::Clamp(Rel.Y * 0.02f, -6.0f, 6.0f)) / 14.0f, -0.6f, 0.6f);
	}
	Car->SetThrottleInput(Tel.SpeedKmh < C.Kmh ? 1.0f : 0.25f);
	Car->SetBrakeInput(0.0f);
	Car->SetSteeringInput(Steer);
	Car->SetHandbrakeInput(false);
	int32 Ground = 0;
	if (UChaosWheeledVehicleMovementComponent* Move = Cast<UChaosWheeledVehicleMovementComponent>(Car->GetVehicleMovement()))
	{
		for (int32 i = 0; i < Move->GetNumWheels(); ++i) Ground += Move->GetWheelState(i).bInContact ? 1 : 0;
	}
	if (Ground < 2 && KeySegT > 0.4f) RampAir += DeltaTime;
	RampMaxZ = FMath::Max(RampMaxZ, (float)P.Z * 0.01f);
	if (KeySegT > 0.4f) RampMinUp = FMath::Min(RampMinUp, (float)Car->GetActorUpVector().Z);
	RampLat = Rel.Y * 0.01f;
	static const bool bShots = ShotInterval > 0.0f;
	if (bShots && RampShot < 2 && KeySegT > (RampShot == 0 ? C.Secs * 0.42f : C.Secs * 0.72f))
	{
		FScreenshotRequest::RequestScreenshot(FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / FString::Printf(TEXT("Bench/%s_%02d_%d.png"), *Label, KeySeg, RampShot)), bShotUI, false);
		++RampShot;
	}
	PadLogTimer += DeltaTime;
	if (PadLogTimer >= 0.25f)
	{
		PadLogTimer = 0.0f;
		PadLog.Add(FString::Printf(TEXT("  %5.2f %s (%.0f,%.0f,%.1f) v=%.0f up=%.2f pitch=%.0f roll=%.0f 轮%d slip=%.1f"), KeySegT, C.Name, P.X * 0.01f, P.Y * 0.01f, P.Z * 0.01f, Tel.SpeedKmh, Car->GetActorUpVector().Z, Car->GetActorRotation().Pitch, Car->GetActorRotation().Roll, Ground, Tel.SlipAngleDeg));
	}
	if (KeySegT >= C.Secs)
	{
		PadLog.Add(FString::Printf(TEXT("=== %-12s 腾空 %.2fs 最高 %.1fm 最小竖直 %.2f 结束 v=%.0f 竖直 %.2f 偏航误差 %.1f 横偏 %.1fm 轮着地 %d 撞动道具 %d 最差帧 %.1fms"), C.Name, RampAir, RampMaxZ, RampMinUp, Tel.SpeedKmh, Car->GetActorUpVector().Z,
			FMath::FindDeltaAngleDegrees(C.Yaw, Car->GetActorRotation().Yaw), RampLat, Ground, Field ? Field->CountMoved() : 0, RampWorstMs));
		++KeySeg;
		KeySegT = 0.0f;
	}
}

void UApexBenchSubsystem::DriveKeys(float DeltaTime)
{
	AApexDriftPlayerController* PC = Cast<AApexDriftPlayerController>(GetWorld()->GetFirstPlayerController());
	AApexDriftVehiclePawn* Car = PC ? PC->GetVehiclePawn() : nullptr;
	if (!Car) return;
	if (KeySeg >= UE_ARRAY_COUNT(GKeySegs)) { Finish(); Phase = 3; return; }
	const FKeySeg& S = GKeySegs[KeySeg];
	const FApexDriftTelemetry& Tel = Car->GetTelemetry();
	if (KeySegT <= 0.0f)
	{
		if (S.bReset)
		{
			Car->ResetToTrack();
			Car->SetActorTransform(FTransform(FRotator::ZeroRotator, FVector(-150000.0f, 0.0f, 150120.0f)), false, nullptr, ETeleportType::TeleportPhysics);
			Car->GetMesh()->SetPhysicsLinearVelocity(FVector::ZeroVector);
			Car->GetMesh()->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
		}
		if (S.YawKick != 0.0f) Car->GetMesh()->SetPhysicsAngularVelocityInDegrees(FVector(0, 0, S.YawKick));
		KeyStraightT = -1.0f;
		KeyDriftT = 0.0f;
		KeyMaxSlip = 0.0f;
	}
	KeySegT += DeltaTime;
	Car->SetThrottleInput(S.Thr);
	Car->SetBrakeInput(S.Brk);
	Car->SetSteeringInput(S.Str);
	Car->SetHandbrakeInput(S.bHand);
	const float YawRate = Car->GetMesh()->GetPhysicsAngularVelocityInDegrees().Z;
	if (Car->IsDriftActive()) KeyDriftT += DeltaTime;
	KeyMaxSlip = FMath::Max(KeyMaxSlip, FMath::Abs(Tel.SlipAngleDeg));
	if (S.bMeasure)
	{
		const bool bStraight = !Car->IsDriftActive() && FMath::Abs(Tel.SlipAngleDeg) < 3.0f && FMath::Abs(YawRate) < 4.0f;
		if (bStraight && KeyStraightT < 0.0f) KeyStraightT = KeySegT;
		if (!bStraight) KeyStraightT = -1.0f;
	}
	PadLogTimer += DeltaTime;
	if (PadLogTimer >= 0.25f)
	{
		PadLogTimer = 0.0f;
		PadLog.Add(FString::Printf(TEXT("  %5.2f %s v=%.0f gear=%d rpm=%.0f slip=%.1f drift=%d yaw=%.0f"), KeySegT, S.Name, Tel.SpeedKmh, Tel.Gear, Tel.EngineRPM, Tel.SlipAngleDeg, Car->IsDriftActive() ? 1 : 0, YawRate));
	}
	if (KeySegT >= S.Dur)
	{
		if (S.bMeasure)
		{
			PadLog.Add(FString::Printf(TEXT("=== %s：%s，本段漂移 %.2fs，结束时 v=%.0f slip=%.1f drift=%d"), S.Name,
				KeyStraightT >= 0.0f ? *FString::Printf(TEXT("%.2fs 拉直"), KeyStraightT) : TEXT("没能拉直"), KeyDriftT, Tel.SpeedKmh, Tel.SlipAngleDeg, Car->IsDriftActive() ? 1 : 0));
		}
		else if (!S.bReset) PadLog.Add(FString::Printf(TEXT("  .. %s 最大侧滑 %.1f 度 v=%.0f"), S.Name, KeyMaxSlip, Tel.SpeedKmh));
		++KeySeg;
		KeySegT = 0.0f;
	}
}

void UApexBenchSubsystem::Finish()
{
	if (bPad || PadLog.Num() > 0)
	{
		FFileHelper::SaveStringArrayToFile(PadLog, *FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / FString::Printf(TEXT("Bench/%s_pad.txt"), *Label)), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FPlatformMisc::RequestExit(false);
		return;
	}
	if (bMonkey)
	{
		const FString Line = FString::Printf(TEXT("%s 漂移次数=%d 最长连续漂移=%.2fs 漂移总时长=%.1fs/%.0fs 松手或反打后仍歪着的最长时间=%.2fs 复位=%d 掉头=%d"), *Label, DriftRuns, MaxDriftRun, DriftSeconds, Elapsed, MaxCrooked, Resets, Spins);
		FFileHelper::SaveStringToFile(Line, *FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / FString::Printf(TEXT("Bench/%s_monkey.txt"), *Label)), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	}
	auto Avg = [](const TArray<float>& A) { double S = 0.0; for (float V : A) S += V; return A.Num() ? static_cast<float>(S / A.Num()) : 0.0f; };
	auto Pct = [](TArray<float> A, float P) { if (!A.Num()) return 0.0f; A.Sort(); return A[FMath::Clamp(FMath::FloorToInt(P * (A.Num() - 1)), 0, A.Num() - 1)]; };

	TArray<float> Sorted = FrameMs;
	Sorted.Sort([](float L, float R) { return L > R; });
	const int32 N1 = FMath::Max(1, Sorted.Num() / 100);
	double Worst1 = 0.0;
	for (int32 i = 0; i < N1 && i < Sorted.Num(); ++i) Worst1 += Sorted[i];
	const float Low1Fps = Worst1 > 0.0 ? static_cast<float>(1000.0 * N1 / Worst1) : 0.0f;
	int32 Over833 = 0, Over20 = 0;
	for (float V : FrameMs) { if (V > 8.34f) ++Over833; if (V > 20.0f) ++Over20; }

	FIntPoint Size(0, 0);
	if (GEngine && GEngine->GameViewport && GEngine->GameViewport->Viewport) Size = GEngine->GameViewport->Viewport->GetSizeXY();

	const float AvgMs = Avg(FrameMs);
	FString Json = FString::Printf(TEXT("{\n\"label\":\"%s\",\"viewport\":\"%dx%d\",\"seconds\":%.1f,\"frames\":%d,\n")
		TEXT("\"avgFps\":%.1f,\"low1Fps\":%.1f,\"p50Ms\":%.2f,\"p95Ms\":%.2f,\"p99Ms\":%.2f,\"maxMs\":%.1f,\n")
		TEXT("\"framesOver8.34ms\":%.4f,\"framesOver20ms\":%d,\n")
		TEXT("\"avgGpuMs\":%.2f,\"p95GpuMs\":%.2f,\"avgGameThreadMs\":%.2f,\"avgRenderThreadMs\":%.2f,\"avgResFraction\":%.3f,\"minResFraction\":%.3f,\n")
		TEXT("\"travelledM\":%.0f,\"maxSpeedKmh\":%.1f,\"resets\":%d,\"spins\":%d,\"maxSlipGripDeg\":%.1f,\"maxSlipDriftDeg\":%.1f,\"driftSeconds\":%.1f,\"maxLateralCm\":%.0f,\"offTrackSeconds\":%.1f,\"driftScore\":%d,\n\"series\":[\n"),
		*Label, Size.X, Size.Y, Elapsed, FrameMs.Num(),
		AvgMs > 0.0f ? 1000.0f / AvgMs : 0.0f, Low1Fps, Pct(FrameMs, 0.5f), Pct(FrameMs, 0.95f), Pct(FrameMs, 0.99f), Sorted.Num() ? Sorted[0] : 0.0f,
		FrameMs.Num() ? static_cast<float>(Over833) / FrameMs.Num() : 0.0f, Over20,
		Avg(GpuMs), Pct(GpuMs, 0.95f), Avg(GameMs), Avg(RenderMs), Avg(ResFrac), Pct(ResFrac, 0.0f),
		Travelled * 0.01f, MaxSpeed, Resets, Spins, MaxSlipGrip, MaxSlipDrift, DriftSeconds, MaxLateral, OffTrackSeconds,
		Cast<AApexDriftPlayerController>(GetWorld()->GetFirstPlayerController())->GetVehiclePawn()->GetTelemetry().TotalScore);
	Json += FString::Join(SecondLog, TEXT(",\n"));
	Json += TEXT("\n]}\n");
	const FString Path = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / FString::Printf(TEXT("Bench/%s.json"), *Label));
	FFileHelper::SaveStringToFile(Json, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	UE_LOG(LogTemp, Display, TEXT("[ApexBench] %s avg=%.1f low1=%.1f -> %s"), *Label, AvgMs > 0.0f ? 1000.0f / AvgMs : 0.0f, Low1Fps, *Path);
	FPlatformMisc::RequestExit(false);
}

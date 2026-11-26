#include "Gameplay/ApexDriftScoreComponent.h"
#include "Track/ApexTrackSplineActor.h"
#include "Components/SplineComponent.h"

UApexDriftScoreComponent::UApexDriftScoreComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UApexDriftScoreComponent::ResetSession(EApexGameModeType ModeType, AApexTrackSplineActor* InTrackActor)
{
	GameModeType = ModeType;
	TrackActor = InTrackActor;

	bIsDrifting = false;
	bInClippingZone = false;
	bPassedHalfTrack = false;
	bSessionFinished = false;
	bWrongWay = false;

	CurrentDriftFloat = 0.0f;
	DriftAccumInChain = 0.0f;
	TotalScore = 0;
	BestSingleDrift = 0;
	PeakAngleDeg = 0.0f;
	DriftMultiplier = 1.0f;
	MaxComboMultiplier = 1.0f;
	PrevMultiplierStep = 1;

	TransitionTimer = 0.0f;
	ActiveDriftTime = 0.0f;
	WallTapCooldown = 0.0f;

	CurrentLap = 1;
	TotalLaps = 3;
	CurrentLapTime = 0.0f;
	BestLapTime = 0.0f;
	SessionTime = 0.0f;
	SessionTimeRemaining = (ModeType == EApexGameModeType::DriftAttack3Min) ? 180.0f : 0.0f;

	LastTrackDistance = 0.0f;
	if (TrackActor && GetOwner())
	{
		FTransform Dummy;
		float Lateral = 0.0f;
		TrackActor->FindClosestTrackTransform(GetOwner()->GetActorLocation(), Dummy, LastTrackDistance, Lateral);
	}

	ActiveZoneName.Empty();
	CalloutText.Empty();
	CalloutTimer = 0.0f;
}

void UApexDriftScoreComponent::SetCallout(const FString& Text, float Duration)
{
	CalloutText = Text;
	CalloutTimer = Duration;
}

void UApexDriftScoreComponent::UpdateScoring(float DeltaSeconds, float SpeedKmh, float SlipAngleDeg, bool bGrounded, const FVector& VehiclePos, bool bActiveGameplay)
{
	if (CalloutTimer > 0.0f)
	{
		CalloutTimer = FMath::Max(0.0f, CalloutTimer - DeltaSeconds);
	}
	if (WallTapCooldown > 0.0f)
	{
		WallTapCooldown = FMath::Max(0.0f, WallTapCooldown - DeltaSeconds);
	}

	if (!bActiveGameplay || bSessionFinished)
	{
		return;
	}

	// Mode timers & Lap progression
	CurrentLapTime += DeltaSeconds;
	SessionTime += DeltaSeconds;
	if (GameModeType == EApexGameModeType::DriftAttack3Min)
	{
		SessionTimeRemaining = FMath::Max(0.0f, SessionTimeRemaining - DeltaSeconds);
		if (SessionTimeRemaining <= 0.0f)
		{
			BankCurrentDrift();
			bSessionFinished = true;
			return;
		}
	}

	// Track progress, drift zones, wrong-way & lap detection
	bInClippingZone = false;
	ActiveZoneName.Empty();
	if (TrackActor && TrackActor->TrackSpline)
	{
		FTransform TrackXf;
		float Dist = 0.0f;
		float Lateral = 0.0f;
		TrackActor->FindClosestTrackTransform(VehiclePos, TrackXf, Dist, Lateral);
		const float TrackLen = FMath::Max(1000.0f, TrackActor->GetTrackLength());

		// 逆行按行进方向判断：车头甩过去、360 时不算逆行
		if (GetOwner() && SpeedKmh > 18.0f)
		{
			const float Dot = FVector::DotProduct(GetOwner()->GetVelocity().GetSafeNormal2D(), TrackXf.GetRotation().GetForwardVector().GetSafeNormal2D());
			bWrongWay = (Dot < -0.35f) && !TrackActor->bFreeRoam;
		}
		else
		{
			bWrongWay = false;
		}

		// 自由场地里离开外环：这一圈作废，也不算漂移区
		const bool bOffLoop = TrackActor->bFreeRoam && FMath::Abs(Lateral) > TrackActor->TrackWidthCm * 0.5f + 400.0f;
		if (bOffLoop)
		{
			bPassedHalfTrack = false;
			LastTrackDistance = Dist;
		}

		// Midpoint checkpoint for valid lap count
		const float Progress = Dist / TrackLen;
		if (!bOffLoop && Progress > 0.40f && Progress < 0.70f)
		{
			bPassedHalfTrack = true;
		}

		// Detect crossing start/finish line forward
		if (bPassedHalfTrack && LastTrackDistance > TrackLen * 0.75f && Dist < TrackLen * 0.25f)
		{
			bPassedHalfTrack = false;
			if (CurrentLapTime > 15.0f)
			{
				if (BestLapTime <= 0.0f || CurrentLapTime < BestLapTime)
				{
					BestLapTime = CurrentLapTime;
					SetCallout(TEXT("BEST LAP! 刷新最快圈速"), 2.2f);
				}
				CurrentLapTime = 0.0f;
				if (GameModeType == EApexGameModeType::Circuit3Laps && CurrentLap >= TotalLaps)
				{
					BankCurrentDrift();
					bSessionFinished = true;
					return;
				}
				CurrentLap++;
			}
		}
		LastTrackDistance = Dist;

		FApexDriftZoneInfo Zone;
		if (!bOffLoop && TrackActor->EvaluateDriftZone(Dist, VehiclePos, Zone, bInClippingZone))
		{
			ActiveZoneName = Zone.ZoneName;
		}
	}

	const float AbsAngle = FMath::Abs(SlipAngleDeg);
	const bool bQualifies = bGrounded && !bWrongWay && SpeedKmh >= 24.0f && AbsAngle >= 12.5f;

	if (bQualifies)
	{
		if (!bIsDrifting)
		{
			bIsDrifting = true;
			CurrentDriftFloat = 0.0f;
			DriftAccumInChain = 0.0f;
			DriftMultiplier = 1.0f;
			PrevMultiplierStep = 1;
			ActiveDriftTime = 0.0f;
		}
		TransitionTimer = 0.0f;
		ActiveDriftTime += DeltaSeconds;
		PeakAngleDeg = FMath::Max(PeakAngleDeg, AbsAngle);

		// Physical drift rate scales naturally with angle (12..65 deg) & speed
		const float AngleFactor = FMath::Clamp((AbsAngle - 10.0f) / 38.0f, 0.25f, 1.65f);
		const float SpeedFactor = FMath::Clamp(SpeedKmh / 85.0f, 0.35f, 2.1f);
		const float ZoneFactor = ActiveZoneName.IsEmpty() ? 1.0f : (bInClippingZone ? 1.65f : 1.25f);
		const float BasePtsPerSec = 520.0f * AngleFactor * SpeedFactor * ZoneFactor;

		DriftAccumInChain += BasePtsPerSec * DeltaSeconds;
		CurrentDriftFloat += BasePtsPerSec * DriftMultiplier * DeltaSeconds;

		// Smooth multiplier progression up to 5.0x
		const float TargetMult = FMath::Clamp(1.0f + FMath::FloorToFloat(DriftAccumInChain / 950.0f) * 0.5f, 1.0f, 5.0f);
		if (TargetMult > DriftMultiplier)
		{
			DriftMultiplier = TargetMult;
			MaxComboMultiplier = FMath::Max(MaxComboMultiplier, DriftMultiplier);
			const int32 MultStep = FMath::RoundToInt(DriftMultiplier * 2.0f);
			if (MultStep > PrevMultiplierStep)
			{
				PrevMultiplierStep = MultStep;
				OnDriftAudioCue.Broadcast(FName("ComboUp"), DriftMultiplier);
				if (DriftMultiplier >= 4.0f) SetCallout(TEXT("APEX MASTER! 极限连漂"), 1.6f);
				else if (bInClippingZone) SetCallout(TEXT("CLIPPING ZONE! 完美贴弯"), 1.4f);
				else if (AbsAngle >= 38.0f) SetCallout(TEXT("GREAT ANGLE! 大角度横滑"), 1.4f);
			}
		}
		else if (bInClippingZone && CalloutTimer <= 0.05f)
		{
			SetCallout(TEXT("CLIPPING ZONE x1.65"), 1.0f);
		}
	}
	else if (bIsDrifting)
	{
		TransitionTimer += DeltaSeconds;
		if (TransitionTimer >= TransitionGraceDuration)
		{
			BankCurrentDrift();
		}
	}
}

void UApexDriftScoreComponent::NotifyWallImpact(float ImpactSpeedCmS)
{
	if (!bIsDrifting && ImpactSpeedCmS < 300.0f)
	{
		return;
	}

	if (ImpactSpeedCmS >= 850.0f)
	{
		ForceCancelDrift(TEXT("CRASH! 碰撞导致连击中断"));
	}
	else if (bIsDrifting && ImpactSpeedCmS >= 180.0f && ImpactSpeedCmS < 680.0f && WallTapCooldown <= 0.0f)
	{
		WallTapCooldown = 2.0f;
		CurrentDriftFloat += 500.0f * DriftMultiplier;
		SetCallout(TEXT("WALL TAP +500! 擦墙炫技"), 1.5f);
		OnDriftAudioCue.Broadcast(FName("ComboUp"), DriftMultiplier);
	}
}

void UApexDriftScoreComponent::AwardSpin()
{
	if (bIsDrifting)
	{
		CurrentDriftFloat += 1500.0f * DriftMultiplier;
	}
	else
	{
		TotalScore += 1500;
	}
	SetCallout(TEXT("360 SPIN! 旋转漂移"), 1.8f);
	OnDriftAudioCue.Broadcast(FName("ComboUp"), FMath::Max(DriftMultiplier, 3.0f));
}

void UApexDriftScoreComponent::BankCurrentDrift()
{
	if (!bIsDrifting)
	{
		return;
	}
	const int32 Banked = FMath::RoundToInt(CurrentDriftFloat);
	if (Banked >= 100)
	{
		TotalScore += Banked;
		BestSingleDrift = FMath::Max(BestSingleDrift, Banked);
		SetCallout(FString::Printf(TEXT("DRIFT BANKED +%d"), Banked), 1.8f);
		OnDriftAudioCue.Broadcast(FName("Bank"), static_cast<float>(Banked));
	}
	bIsDrifting = false;
	CurrentDriftFloat = 0.0f;
	DriftAccumInChain = 0.0f;
	DriftMultiplier = 1.0f;
	TransitionTimer = 0.0f;
}

void UApexDriftScoreComponent::ForceCancelDrift(const FString& ReasonText)
{
	if (bIsDrifting && CurrentDriftFloat >= 120.0f)
	{
		SetCallout(ReasonText, 1.8f);
		OnDriftAudioCue.Broadcast(FName("ComboLost"), 0.0f);
	}
	bIsDrifting = false;
	CurrentDriftFloat = 0.0f;
	DriftAccumInChain = 0.0f;
	DriftMultiplier = 1.0f;
	TransitionTimer = 0.0f;
}

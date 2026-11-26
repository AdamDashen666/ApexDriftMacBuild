#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/ApexDriftTypes.h"
#include "ApexDriftScoreComponent.generated.h"

class AApexTrackSplineActor;

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnDriftAudioCue, FName /*CueName*/, float /*Param*/);

UCLASS(ClassGroup = (Gameplay), meta = (BlueprintSpawnableComponent))
class APEXDRIFT_API UApexDriftScoreComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UApexDriftScoreComponent();

	FOnDriftAudioCue OnDriftAudioCue;

	void ResetSession(EApexGameModeType ModeType, AApexTrackSplineActor* InTrackActor);
	void UpdateScoring(float DeltaSeconds, float SpeedKmh, float SlipAngleDeg, bool bGrounded, const FVector& VehiclePos, bool bActiveGameplay);

	void NotifyWallImpact(float ImpactSpeedCmS);
	void BankCurrentDrift();
	/** 完成一个 360：加分并提示 */
	void AwardSpin();
	void ShowCallout(const FString& Text, float Duration) { SetCallout(Text, Duration); }
	void ForceCancelDrift(const FString& ReasonText);

	bool IsDrifting() const { return bIsDrifting; }
	int32 GetCurrentDriftScore() const { return FMath::RoundToInt(CurrentDriftFloat); }
	int32 GetTotalScore() const { return TotalScore; }
	int32 GetBestSingleDrift() const { return BestSingleDrift; }
	float GetPeakAngleDeg() const { return PeakAngleDeg; }
	float GetDriftMultiplier() const { return DriftMultiplier; }
	float GetMaxComboMultiplier() const { return MaxComboMultiplier; }
	float GetTransitionGraceNormalized() const { return bIsDrifting && TransitionTimer > 0.0f ? FMath::Clamp(1.0f - TransitionTimer / TransitionGraceDuration, 0.0f, 1.0f) : (bIsDrifting ? 1.0f : 0.0f); }
	bool IsInClippingZone() const { return bInClippingZone; }
	const FString& GetActiveZoneName() const { return ActiveZoneName; }
	const FString& GetCalloutText() const { return CalloutText; }
	float GetCalloutAlpha() const { return FMath::Clamp(CalloutTimer / 1.4f, 0.0f, 1.0f); }

	int32 GetCurrentLap() const { return CurrentLap; }
	int32 GetTotalLaps() const { return TotalLaps; }
	float GetCurrentLapTime() const { return CurrentLapTime; }
	float GetBestLapTime() const { return BestLapTime; }
	float GetSessionTimeRemaining() const { return SessionTimeRemaining; }
	bool IsSessionFinished() const { return bSessionFinished; }
	bool IsWrongWay() const { return bWrongWay; }

private:
	void SetCallout(const FString& Text, float Duration = 1.6f);

	UPROPERTY()
	TObjectPtr<AApexTrackSplineActor> TrackActor;

	EApexGameModeType GameModeType = EApexGameModeType::FreeDrift;

	bool bIsDrifting = false;
	bool bInClippingZone = false;
	bool bPassedHalfTrack = false;
	bool bSessionFinished = false;
	bool bWrongWay = false;

	float CurrentDriftFloat = 0.0f;
	float DriftAccumInChain = 0.0f;
	int32 TotalScore = 0;
	int32 BestSingleDrift = 0;
	float PeakAngleDeg = 0.0f;
	float DriftMultiplier = 1.0f;
	float MaxComboMultiplier = 1.0f;
	int32 PrevMultiplierStep = 1;

	float TransitionTimer = 0.0f;
	static constexpr float TransitionGraceDuration = 2.4f;
	float ActiveDriftTime = 0.0f;
	float WallTapCooldown = 0.0f;

	float LastTrackDistance = 0.0f;
	int32 CurrentLap = 1;
	int32 TotalLaps = 3;
	float CurrentLapTime = 0.0f;
	float BestLapTime = 0.0f;
	float SessionTimeRemaining = 180.0f;

	FString ActiveZoneName;
	FString CalloutText;
	float CalloutTimer = 0.0f;
};

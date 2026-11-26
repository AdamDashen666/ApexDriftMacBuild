#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ApexVehicleAudioComponent.generated.h"

class UAudioComponent;
class USoundWave;

USTRUCT()
struct FApexEngineBand
{
	GENERATED_BODY()

	float CenterRPM = 1000.0f;

	UPROPERTY()
	TObjectPtr<UAudioComponent> Comp = nullptr;
};

UCLASS(ClassGroup = (Audio), meta = (BlueprintSpawnableComponent))
class APEXDRIFT_API UApexVehicleAudioComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UApexVehicleAudioComponent();

	void UpdateVehicleAudio(
		float DeltaSeconds,
		float EngineRPM,
		float ThrottleInput,
		float SpeedKmh,
		float TireSkidIntensity,
		float RoadWetness,
		bool bOnCurb,
		float TurboBoostBar,
		bool bIsPaused);

	// 各车的发动机音高与音量倍率
	float EnginePitch = 1.0f;
	float EngineGain = 1.0f;

	void TriggerBOVFlutter();
	void TriggerExhaustBackfire();
	void TriggerImpactSound(float ImpactSpeedCmS);
	void PlayUISound(FName CueName);

	void ApplyVolumeSettings(float InMaster, float InEngine, float InTire, float InSfx);

protected:
	virtual void BeginPlay() override;

private:
	USoundWave* LoadSound(const TCHAR* AssetPath, bool bLooping = false);
	UAudioComponent* CreateLoopAudio(USoundWave* Wave, USceneComponent* AttachTarget, float InitialVol = 0.0f);

	UPROPERTY()
	TArray<FApexEngineBand> OnLoadBands;

	UPROPERTY()
	TArray<FApexEngineBand> OffLoadBands;

	UPROPERTY()
	TObjectPtr<UAudioComponent> TurboSpoolAudio;
	UPROPERTY()
	TObjectPtr<UAudioComponent> TireDryAudio;
	UPROPERTY()
	TObjectPtr<UAudioComponent> TireWetAudio;
	UPROPERTY()
	TObjectPtr<UAudioComponent> CurbRumbleAudio;
	UPROPERTY()
	TObjectPtr<UAudioComponent> WindRushAudio;
	UPROPERTY()
	TObjectPtr<UAudioComponent> AmbientHarborAudio;

	UPROPERTY()
	TArray<TObjectPtr<USoundWave>> BOVSounds;
	UPROPERTY()
	TArray<TObjectPtr<USoundWave>> ExhaustPopSounds;
	UPROPERTY()
	TObjectPtr<USoundWave> ImpactHeavySound;
	UPROPERTY()
	TObjectPtr<USoundWave> ImpactGrazeSound;

	UPROPERTY()
	TMap<FName, TObjectPtr<USoundWave>> UISounds;

	float MasterVol = 0.90f;
	float EngineVol = 0.95f;
	float TireVol = 0.85f;
	float SfxVol = 0.85f;
	float SmoothedLoad = 0.0f;
	float SmoothedTire = 0.0f;
	float PopCooldown = 0.0f;
	float ImpactCooldown = 0.0f;
};

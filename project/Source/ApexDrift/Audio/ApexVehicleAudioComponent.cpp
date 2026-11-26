#include "Audio/ApexVehicleAudioComponent.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundWave.h"
#include "Kismet/GameplayStatics.h"

UApexVehicleAudioComponent::UApexVehicleAudioComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

USoundWave* UApexVehicleAudioComponent::LoadSound(const TCHAR* AssetPath, bool bLooping)
{
	USoundWave* Wave = Cast<USoundWave>(StaticLoadObject(USoundWave::StaticClass(), nullptr, AssetPath));
	if (Wave && bLooping)
	{
		Wave->bLooping = true;
	}
	return Wave;
}

UAudioComponent* UApexVehicleAudioComponent::CreateLoopAudio(USoundWave* Wave, USceneComponent* AttachTarget, float InitialVol)
{
	if (!Wave || !AttachTarget)
	{
		return nullptr;
	}
	UAudioComponent* AC = NewObject<UAudioComponent>(GetOwner());
	if (!AC)
	{
		return nullptr;
	}
	AC->bAutoActivate = false;
	AC->bAllowSpatialization = false;
	AC->bIsUISound = false;
	AC->SetSound(Wave);
	AC->SetVolumeMultiplier(InitialVol);
	AC->AttachToComponent(AttachTarget, FAttachmentTransformRules::KeepRelativeTransform);
	AC->RegisterComponent();
	AC->Play();
	return AC;
}

void UApexVehicleAudioComponent::BeginPlay()
{
	Super::BeginPlay();

	AActor* Owner = GetOwner();
	USceneComponent* Root = Owner ? Owner->GetRootComponent() : nullptr;
	if (!Root)
	{
		return;
	}

	const TPair<float, const TCHAR*> OnConfigs[] = {
		{1000.0f, TEXT("/Game/Audio/SFX_Engine_On_1000.SFX_Engine_On_1000")},
		{2500.0f, TEXT("/Game/Audio/SFX_Engine_On_2500.SFX_Engine_On_2500")},
		{4500.0f, TEXT("/Game/Audio/SFX_Engine_On_4500.SFX_Engine_On_4500")},
		{6500.0f, TEXT("/Game/Audio/SFX_Engine_On_6500.SFX_Engine_On_6500")},
		{8000.0f, TEXT("/Game/Audio/SFX_Engine_On_8000.SFX_Engine_On_8000")}
	};
	for (const auto& Cfg : OnConfigs)
	{
		if (UAudioComponent* AC = CreateLoopAudio(LoadSound(Cfg.Value, true), Root, 0.0f))
		{
			OnLoadBands.Add({Cfg.Key, AC});
		}
	}

	const TPair<float, const TCHAR*> OffConfigs[] = {
		{2000.0f, TEXT("/Game/Audio/SFX_Engine_Off_2000.SFX_Engine_Off_2000")},
		{5000.0f, TEXT("/Game/Audio/SFX_Engine_Off_5000.SFX_Engine_Off_5000")}
	};
	for (const auto& Cfg : OffConfigs)
	{
		if (UAudioComponent* AC = CreateLoopAudio(LoadSound(Cfg.Value, true), Root, 0.0f))
		{
			OffLoadBands.Add({Cfg.Key, AC});
		}
	}

	TurboSpoolAudio = CreateLoopAudio(LoadSound(TEXT("/Game/Audio/SFX_Turbo_Spool.SFX_Turbo_Spool"), true), Root, 0.0f);
	TireDryAudio = CreateLoopAudio(LoadSound(TEXT("/Game/Audio/SFX_Tire_Screech_Dry.SFX_Tire_Screech_Dry"), true), Root, 0.0f);
	TireWetAudio = CreateLoopAudio(LoadSound(TEXT("/Game/Audio/SFX_Tire_Spray_Wet.SFX_Tire_Spray_Wet"), true), Root, 0.0f);
	CurbRumbleAudio = CreateLoopAudio(LoadSound(TEXT("/Game/Audio/SFX_Curb_Rumble.SFX_Curb_Rumble"), true), Root, 0.0f);
	WindRushAudio = CreateLoopAudio(LoadSound(TEXT("/Game/Audio/SFX_Wind_Rush.SFX_Wind_Rush"), true), Root, 0.0f);
	AmbientHarborAudio = CreateLoopAudio(LoadSound(TEXT("/Game/Audio/SFX_Ambient_Harbor.SFX_Ambient_Harbor"), true), Root, 0.18f);

	if (USoundWave* B1 = LoadSound(TEXT("/Game/Audio/SFX_Turbo_BOV_01.SFX_Turbo_BOV_01"))) BOVSounds.Add(B1);
	if (USoundWave* B2 = LoadSound(TEXT("/Game/Audio/SFX_Turbo_BOV_02.SFX_Turbo_BOV_02"))) BOVSounds.Add(B2);
	if (USoundWave* P1 = LoadSound(TEXT("/Game/Audio/SFX_Exhaust_Pop_01.SFX_Exhaust_Pop_01"))) ExhaustPopSounds.Add(P1);
	if (USoundWave* P2 = LoadSound(TEXT("/Game/Audio/SFX_Exhaust_Pop_02.SFX_Exhaust_Pop_02"))) ExhaustPopSounds.Add(P2);
	if (USoundWave* P3 = LoadSound(TEXT("/Game/Audio/SFX_Exhaust_Pop_03.SFX_Exhaust_Pop_03"))) ExhaustPopSounds.Add(P3);

	ImpactHeavySound = LoadSound(TEXT("/Game/Audio/SFX_Impact_Heavy.SFX_Impact_Heavy"));
	ImpactGrazeSound = LoadSound(TEXT("/Game/Audio/SFX_Impact_Graze.SFX_Impact_Graze"));

	UISounds.Add(FName("Hover"), LoadSound(TEXT("/Game/Audio/SFX_UI_Hover.SFX_UI_Hover")));
	UISounds.Add(FName("Click"), LoadSound(TEXT("/Game/Audio/SFX_UI_Click.SFX_UI_Click")));
	UISounds.Add(FName("Bank"), LoadSound(TEXT("/Game/Audio/SFX_UI_DriftBank.SFX_UI_DriftBank")));
	UISounds.Add(FName("ComboUp"), LoadSound(TEXT("/Game/Audio/SFX_UI_ComboUp.SFX_UI_ComboUp")));
	UISounds.Add(FName("ComboLost"), LoadSound(TEXT("/Game/Audio/SFX_UI_ComboLost.SFX_UI_ComboLost")));
	UISounds.Add(FName("Countdown"), LoadSound(TEXT("/Game/Audio/SFX_UI_Countdown.SFX_UI_Countdown")));
	UISounds.Add(FName("Go"), LoadSound(TEXT("/Game/Audio/SFX_UI_Go.SFX_UI_Go")));
}

void UApexVehicleAudioComponent::ApplyVolumeSettings(float InMaster, float InEngine, float InTire, float InSfx)
{
	MasterVol = FMath::Clamp(InMaster, 0.0f, 1.0f);
	EngineVol = FMath::Clamp(InEngine, 0.0f, 1.0f);
	TireVol = FMath::Clamp(InTire, 0.0f, 1.0f);
	SfxVol = FMath::Clamp(InSfx, 0.0f, 1.0f);
}

void UApexVehicleAudioComponent::UpdateVehicleAudio(
	float DeltaSeconds,
	float EngineRPM,
	float ThrottleInput,
	float SpeedKmh,
	float TireSkidIntensity,
	float RoadWetness,
	bool bOnCurb,
	float TurboBoostBar,
	bool bIsPaused)
{
	PopCooldown = FMath::Max(0.0f, PopCooldown - DeltaSeconds);
	ImpactCooldown = FMath::Max(0.0f, ImpactCooldown - DeltaSeconds);

	const float PauseMul = bIsPaused ? 0.0f : 1.0f;
	const float RPM = FMath::Clamp(EngineRPM, 750.0f, 8400.0f);
	SmoothedLoad = FMath::FInterpTo(SmoothedLoad, FMath::Clamp(ThrottleInput, 0.0f, 1.0f), DeltaSeconds, 8.0f);
	SmoothedTire = FMath::FInterpTo(SmoothedTire, FMath::Clamp(TireSkidIntensity, 0.0f, 1.0f), DeltaSeconds, 11.0f);

	const float EngMaster = MasterVol * EngineVol * PauseMul;

	// Multi-band crossfade across On-Load engine recordings
	for (int32 i = 0; i < OnLoadBands.Num(); ++i)
	{
		UAudioComponent* AC = OnLoadBands[i].Comp;
		if (!AC) continue;
		const float C = OnLoadBands[i].CenterRPM;
		const float PrevC = (i > 0) ? OnLoadBands[i - 1].CenterRPM : 0.0f;
		const float NextC = (i + 1 < OnLoadBands.Num()) ? OnLoadBands[i + 1].CenterRPM : 12000.0f;

		float Weight = 0.0f;
		if (RPM <= C)
		{
			Weight = (i == 0) ? 1.0f : FMath::Clamp((RPM - PrevC) / FMath::Max(1.0f, C - PrevC), 0.0f, 1.0f);
		}
		else
		{
			Weight = (i + 1 == OnLoadBands.Num()) ? 1.0f : FMath::Clamp((NextC - RPM) / FMath::Max(1.0f, NextC - C), 0.0f, 1.0f);
		}

		const float LoadMix = FMath::Lerp(0.35f, 1.0f, SmoothedLoad);
		const float Pitch = FMath::Clamp(RPM / C, 0.52f, 1.85f) * EnginePitch;
		AC->SetPitchMultiplier(Pitch);
		AC->SetVolumeMultiplier(Weight * LoadMix * EngMaster * EngineGain * 0.72f);
	}

	// Off-load decel bands fill overrun rumble when throttle lifts
	for (int32 i = 0; i < OffLoadBands.Num(); ++i)
	{
		UAudioComponent* AC = OffLoadBands[i].Comp;
		if (!AC) continue;
		const float C = OffLoadBands[i].CenterRPM;
		float Weight = (i == 0)
			? FMath::Clamp(1.0f - (RPM - 2000.0f) / 3000.0f, 0.0f, 1.0f)
			: FMath::Clamp((RPM - 2000.0f) / 3000.0f, 0.0f, 1.0f);
		const float OffMix = FMath::Lerp(0.65f, 0.12f, SmoothedLoad);
		AC->SetPitchMultiplier(FMath::Clamp(RPM / C, 0.55f, 1.80f) * EnginePitch);
		AC->SetVolumeMultiplier(Weight * OffMix * EngMaster * EngineGain * 0.55f);
	}

	if (TurboSpoolAudio)
	{
		const float BoostNorm = FMath::Clamp(TurboBoostBar / 1.4f, 0.0f, 1.0f);
		TurboSpoolAudio->SetPitchMultiplier(0.75f + BoostNorm * 0.55f);
		TurboSpoolAudio->SetVolumeMultiplier(BoostNorm * EngMaster * 0.28f);
	}

	const float TireMaster = MasterVol * TireVol * PauseMul;
	if (TireDryAudio)
	{
		const float DryMix = FMath::Clamp(1.0f - RoadWetness * 0.55f, 0.25f, 1.0f);
		TireDryAudio->SetPitchMultiplier(0.88f + SmoothedTire * 0.28f);
		TireDryAudio->SetVolumeMultiplier(SmoothedTire * DryMix * TireMaster * 0.65f);
	}
	if (TireWetAudio)
	{
		const float WetMix = FMath::Clamp(RoadWetness, 0.0f, 1.0f);
		const float SpeedSpray = FMath::Clamp(SpeedKmh / 140.0f, 0.0f, 1.0f) * 0.25f;
		const float Level = FMath::Max(SmoothedTire * 0.85f, SpeedSpray) * WetMix;
		TireWetAudio->SetPitchMultiplier(0.92f + FMath::Clamp(SpeedKmh / 180.0f, 0.0f, 0.35f));
		TireWetAudio->SetVolumeMultiplier(Level * TireMaster * 0.55f);
	}
	if (CurbRumbleAudio)
	{
		const float CurbTarget = (bOnCurb && SpeedKmh > 15.0f) ? FMath::Clamp(SpeedKmh / 90.0f, 0.2f, 1.0f) : 0.0f;
		CurbRumbleAudio->SetPitchMultiplier(0.8f + FMath::Clamp(SpeedKmh / 120.0f, 0.0f, 0.8f));
		CurbRumbleAudio->SetVolumeMultiplier(CurbTarget * MasterVol * SfxVol * PauseMul * 0.55f);
	}
	if (WindRushAudio)
	{
		const float WindNorm = FMath::Clamp((SpeedKmh - 45.0f) / 190.0f, 0.0f, 1.0f);
		WindRushAudio->SetPitchMultiplier(0.85f + WindNorm * 0.45f);
		WindRushAudio->SetVolumeMultiplier(WindNorm * WindNorm * MasterVol * SfxVol * PauseMul * 0.48f);
	}
	if (AmbientHarborAudio)
	{
		AmbientHarborAudio->SetVolumeMultiplier(MasterVol * SfxVol * (bIsPaused ? 0.08f : 0.22f));
	}
}

void UApexVehicleAudioComponent::TriggerBOVFlutter()
{
	if (BOVSounds.Num() > 0 && GetWorld())
	{
		USoundWave* Wave = BOVSounds[FMath::RandRange(0, BOVSounds.Num() - 1)];
		if (Wave)
		{
			UGameplayStatics::PlaySound2D(GetWorld(), Wave, MasterVol * EngineVol * 0.52f, FMath::RandRange(0.95f, 1.08f));
		}
	}
}

void UApexVehicleAudioComponent::TriggerExhaustBackfire()
{
	if (PopCooldown <= 0.0f && ExhaustPopSounds.Num() > 0 && GetWorld())
	{
		PopCooldown = 0.22f;
		USoundWave* Wave = ExhaustPopSounds[FMath::RandRange(0, ExhaustPopSounds.Num() - 1)];
		if (Wave)
		{
			UGameplayStatics::PlaySound2D(GetWorld(), Wave, MasterVol * EngineVol * 0.75f, FMath::RandRange(0.92f, 1.12f));
		}
	}
}

void UApexVehicleAudioComponent::TriggerImpactSound(float ImpactSpeedCmS)
{
	if (ImpactCooldown > 0.0f || !GetWorld())
	{
		return;
	}
	ImpactCooldown = 0.28f;
	const bool bHeavy = ImpactSpeedCmS >= 650.0f;
	USoundWave* Wave = bHeavy ? ImpactHeavySound : ImpactGrazeSound;
	if (Wave)
	{
		const float Vol = FMath::Clamp(ImpactSpeedCmS / 1200.0f, 0.25f, 1.0f) * MasterVol * SfxVol * 0.85f;
		UGameplayStatics::PlaySound2D(GetWorld(), Wave, Vol, FMath::RandRange(0.94f, 1.06f));
	}
}

void UApexVehicleAudioComponent::PlayUISound(FName CueName)
{
	if (!GetWorld())
	{
		return;
	}
	if (TObjectPtr<USoundWave>* Found = UISounds.Find(CueName))
	{
		if (*Found)
		{
			UGameplayStatics::PlaySound2D(GetWorld(), *Found, MasterVol * SfxVol * 0.70f, 1.0f);
		}
	}
}

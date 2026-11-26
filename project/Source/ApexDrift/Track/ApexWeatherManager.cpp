#include "Track/ApexWeatherManager.h"
#include "Components/LocalLightComponent.h"
#include "Core/ApexMapRegistry.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/PostProcessVolume.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/PointLightComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialParameterCollection.h"
#include "Materials/MaterialParameterCollectionInstance.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"
#include "EngineUtils.h"
#include "Engine/TextureCube.h"
#include "Engine/Texture2D.h"

AApexWeatherManager::AApexWeatherManager()
{
	PrimaryActorTick.bCanEverTick = true;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	// 花瓣/红叶体积：一张合并网格 + 顶点动画材质，跟随相机，单次绘制
	PetalVolume = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PetalVolume"));
	PetalVolume->SetupAttachment(Root);
	PetalVolume->SetUsingAbsoluteLocation(true);
	PetalVolume->SetUsingAbsoluteRotation(true);
	PetalVolume->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PetalVolume->SetCastShadow(false);
	PetalVolume->bVisibleInRayTracing = false;
	PetalVolume->bAffectDistanceFieldLighting = false;
	PetalVolume->bAffectDynamicIndirectLighting = false;
	PetalVolume->SetBoundsScale(4.0f);
	PetalVolume->SetVisibility(false);
}

FApexWeatherSetup AApexWeatherManager::GetSetup(FName InMapId, int32 Index)
{
	FApexWeatherSetup S;

	// 各 HDRI 的太阳方位在 Tools/prepare_sky_hdri.py 里测出，SkyYawDeg 使其与方向光一致
	if (InMapId == TEXT("FujiSakura"))
	{
		switch (Index)
		{
		case 1: // 金色黄昏
			S.SunRot = FRotator(-9.0f, 82.0f, 0.0f);
			S.SunColor = FLinearColor(1.0f, 0.62f, 0.36f);
			S.SunIntensity = 7.0f;
			S.SkyTexture = TEXT("/Game/Art/Sky/T_Sky_FujiSunset.T_Sky_FujiSunset");
			S.SkyCube = TEXT("/Game/Art/Sky/T_SkyCube_FujiSunset.T_SkyCube_FujiSunset");
			S.SkyBrightness = 1.0f;
			S.SkyLightIntensity = 1.15f;
			S.FogColor = FLinearColor(0.46f, 0.40f, 0.52f);
			S.SunFogColor = FLinearColor(1.0f, 0.52f, 0.26f);
			S.FogDensity = 0.0013f; S.FogFalloff = 0.022f;
			S.Wetness = 0.0f;
			S.TrackLights = 0.55f;
			S.Exposure = 0.15f; S.ExpMin = -0.4f; S.ExpMax = 1.2f;
			S.Saturation = 1.06f; S.Contrast = 1.04f; S.WhiteTemp = 6300.0f; S.Bloom = 0.5f;
			S.Petals = 0.85f;
			break;
		case 2: // 雨后初霁
			S.SunRot = FRotator(-46.0f, -68.0f, 0.0f);
			S.SunColor = FLinearColor(1.0f, 0.95f, 0.87f);
			S.SunIntensity = 6.5f;
			S.SkyTexture = TEXT("/Game/Art/Sky/T_Sky_FujiCloudy.T_Sky_FujiCloudy");
			S.SkyCube = TEXT("/Game/Art/Sky/T_SkyCube_FujiCloudy.T_SkyCube_FujiCloudy");
			S.SkyBrightness = 1.0f;
			S.SkyLightIntensity = 1.35f;
			S.FogColor = FLinearColor(0.56f, 0.64f, 0.74f);
			S.SunFogColor = FLinearColor(0.60f, 0.52f, 0.44f);
			S.FogDensity = 0.0020f; S.FogFalloff = 0.03f;
			S.Wetness = 0.80f; S.Puddles = 0.62f;
			S.TrackLights = 0.2f;
			S.Exposure = 0.1f; S.ExpMin = -0.2f; S.ExpMax = 1.4f;
			S.Saturation = 1.05f; S.Contrast = 1.03f; S.WhiteTemp = 6600.0f;
			S.Petals = 0.45f;
			break;
		default: // 春日晴空
			S.SunRot = FRotator(-47.0f, -80.0f, 0.0f);
			S.SunColor = FLinearColor(1.0f, 0.955f, 0.885f);
			S.SunIntensity = 10.0f;
			S.SkyTexture = TEXT("/Game/Art/Sky/T_Sky_FujiClear.T_Sky_FujiClear");
			S.SkyCube = TEXT("/Game/Art/Sky/T_SkyCube_FujiClear.T_SkyCube_FujiClear");
			S.SkyBrightness = 1.0f;
			S.SkyLightIntensity = 1.2f;
			S.FogColor = FLinearColor(0.62f, 0.78f, 1.0f);
			S.SunFogColor = FLinearColor(0.50f, 0.44f, 0.34f);
			S.FogDensity = 0.0024f; S.FogFalloff = 0.022f;
			S.Exposure = 0.0f; S.ExpMin = 0.0f; S.ExpMax = 1.4f;
			S.Saturation = 1.08f; S.Contrast = 1.03f;
			S.Petals = 1.0f;
			break;
		}
		return S;
	}

	if (InMapId == TEXT("MomijiTouge"))
	{
		switch (Index)
		{
		case 1: // 山谷晨雾
			S.SunRot = FRotator(-20.0f, 40.0f, 0.0f);
			S.SunColor = FLinearColor(1.0f, 0.86f, 0.68f);
			S.SunIntensity = 7.0f;
			S.SkyTexture = TEXT("/Game/Art/Sky/T_Sky_MomijiMist.T_Sky_MomijiMist");
			S.SkyCube = TEXT("/Game/Art/Sky/T_SkyCube_MomijiMist.T_SkyCube_MomijiMist");
			S.SkyLightIntensity = 1.2f;
			S.FogColor = FLinearColor(0.86f, 0.89f, 0.94f);
			S.SunFogColor = FLinearColor(1.1f, 0.82f, 0.52f);
			S.FogDensity = 0.020f; S.FogFalloff = 0.05f; S.FogMaxOpacity = 0.93f;
			S.Wetness = 0.35f; S.Puddles = 0.15f;
			S.TrackLights = 0.35f;
			S.Exposure = 0.1f; S.ExpMin = -0.2f; S.ExpMax = 1.4f;
			S.Saturation = 1.05f; S.Contrast = 1.04f; S.WhiteTemp = 6400.0f; S.Bloom = 0.5f;
			S.Petals = 0.45f;
			break;
		case 2: // 夕照红叶
			S.SunRot = FRotator(-12.0f, -150.0f, 0.0f);
			S.SunColor = FLinearColor(1.0f, 0.62f, 0.34f);
			S.SunIntensity = 9.0f;
			S.SkyTexture = TEXT("/Game/Art/Sky/T_Sky_MomijiSunset.T_Sky_MomijiSunset");
			S.SkyCube = TEXT("/Game/Art/Sky/T_SkyCube_MomijiSunset.T_SkyCube_MomijiSunset");
			S.SkyLightIntensity = 1.35f;
			S.FogColor = FLinearColor(0.44f, 0.40f, 0.48f);
			S.SunFogColor = FLinearColor(1.0f, 0.50f, 0.22f);
			S.FogDensity = 0.0016f;
			S.TrackLights = 0.6f;
			S.Exposure = 0.5f; S.ExpMin = -0.6f; S.ExpMax = 1.2f;
			S.Saturation = 1.06f; S.Contrast = 1.05f; S.Bloom = 0.5f;
			S.Petals = 0.8f;
			break;
		default: // 秋日暖阳
			S.SunRot = FRotator(-36.0f, 30.0f, 0.0f);
			S.SunColor = FLinearColor(1.0f, 0.93f, 0.82f);
			S.SunIntensity = 10.0f;
			S.SkyTexture = TEXT("/Game/Art/Sky/T_Sky_MomijiClear.T_Sky_MomijiClear");
			S.SkyCube = TEXT("/Game/Art/Sky/T_SkyCube_MomijiClear.T_SkyCube_MomijiClear");
			S.SkyLightIntensity = 1.15f;
			S.FogColor = FLinearColor(0.48f, 0.58f, 0.74f);
			S.SunFogColor = FLinearColor(0.62f, 0.48f, 0.32f);
			S.FogDensity = 0.0011f;
			S.Exposure = 0.0f; S.ExpMin = 0.0f; S.ExpMax = 1.4f;
			S.Saturation = 1.08f; S.Contrast = 1.04f;
			S.Petals = 0.7f;
			break;
		}
		return S;
	}

	if (InMapId == TEXT("TrainingPark"))
	{
		// 湖畔训练场：借用富士的天空；夜间用黑潮的夜空和场边灯塔
		if (Index == 2)
		{
			S = GetSetup(TEXT("KuroshioCircuit"), 1);
			S.Wetness = 0.0f; S.Puddles = 0.0f;
			S.SkyLightIntensity = 1.6f;
			S.FogDensity = 0.0008f;
			S.Exposure = 0.35f;
			S.FloodScale = 5.0f;    // 60m 高的灯阵
			S.ExpMax = 1.6f;
			S.Petals = 0.0f;
			return S;
		}
		S = GetSetup(TEXT("FujiSakura"), Index);
		S.Petals = 0.35f;
		S.TrackLights = Index == 1 ? 0.5f : 0.0f;
		S.FloodScale = 1.2f;
		return S;
	}

	// 黑潮港湾
	switch (Index)
	{
	case 1: // 午夜霓虹
		S.SunRot = FRotator(-28.0f, -45.0f, 0.0f);
		S.SunColor = FLinearColor(0.35f, 0.56f, 1.0f);
		S.SunIntensity = 0.42f;
		S.SkyTexture = TEXT("/Game/Art/Sky/T_Sky_KuroNight.T_Sky_KuroNight");
		S.SkyCube = TEXT("/Game/Art/Sky/T_SkyCube_KuroNight.T_SkyCube_KuroNight");
		S.SkyBrightness = 0.6f;
		S.SkyLightIntensity = 1.3f;
		S.FogColor = FLinearColor(0.05f, 0.11f, 0.26f);
		S.SunFogColor = FLinearColor(0.02f, 0.03f, 0.06f);
		S.FogDensity = 0.0005f;
		S.Wetness = 0.72f; S.Puddles = 0.62f;
		S.TrackLights = 1.0f; S.bNight = true;
		S.Exposure = 0.35f; S.ExpMin = -0.2f; S.ExpMax = 2.0f;
		S.Saturation = 1.05f;
		break;
	case 2: // 海岸晴日
		S.SunRot = FRotator(-40.0f, -110.0f, 0.0f);
		S.SunColor = FLinearColor(1.0f, 0.96f, 0.90f);
		S.SunIntensity = 10.0f;
		S.SkyTexture = TEXT("/Game/Art/Sky/T_Sky_FujiClear.T_Sky_FujiClear");
		S.SkyCube = TEXT("/Game/Art/Sky/T_SkyCube_FujiClear.T_SkyCube_FujiClear");
		S.SkyLightIntensity = 1.2f;
		S.FogColor = FLinearColor(0.46f, 0.60f, 0.82f);
		S.SunFogColor = FLinearColor(0.50f, 0.44f, 0.34f);
		S.FogDensity = 0.0006f;
		S.Exposure = 0.0f; S.ExpMin = 0.0f; S.ExpMax = 1.4f;
		S.Saturation = 1.06f; S.Contrast = 1.03f;
		break;
	default: // 黄金日落
		S.SunRot = FRotator(-16.0f, -135.0f, 0.0f);
		S.SunColor = FLinearColor(1.0f, 0.80f, 0.56f);
		S.SunIntensity = 9.5f;
		S.SkyTexture = TEXT("/Game/Art/Sky/T_Sky_KuroSunset.T_Sky_KuroSunset");
		S.SkyCube = TEXT("/Game/Art/Sky/T_SkyCube_KuroSunset.T_SkyCube_KuroSunset");
		S.SkyLightIntensity = 1.3f;
		S.FogColor = FLinearColor(0.50f, 0.44f, 0.46f);
		S.SunFogColor = FLinearColor(1.0f, 0.56f, 0.28f);
		S.FogDensity = 0.0006f;
		S.Wetness = 0.55f; S.Puddles = 0.5f;
		S.TrackLights = 0.2f;
		S.Exposure = 0.05f; S.ExpMin = -0.2f; S.ExpMax = 1.6f;
		S.Saturation = 1.05f;
		break;
	}
	return S;
}

void AApexWeatherManager::BeginPlay()
{
	Super::BeginPlay();
	DiscoverSceneActors();
	ApplyWeatherIndex(CurrentWeatherIndex);
}

void AApexWeatherManager::DiscoverSceneActors()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	if (!SunLight)
	{
		for (TActorIterator<ADirectionalLight> It(World); It; ++It) { SunLight = *It; break; }
	}
	if (!SkyLightActor)
	{
		for (TActorIterator<ASkyLight> It(World); It; ++It) { SkyLightActor = *It; break; }
	}
	if (!SkyAtmosphereActor)
	{
		for (TActorIterator<ASkyAtmosphere> It(World); It; ++It) { SkyAtmosphereActor = *It; break; }
	}
	if (!HeightFog)
	{
		for (TActorIterator<AExponentialHeightFog> It(World); It; ++It) { HeightFog = *It; break; }
	}
	if (!PostProcessVolume)
	{
		for (TActorIterator<APostProcessVolume> It(World); It; ++It) { PostProcessVolume = *It; break; }
	}
	if (!SkyDomeActor)
	{
		for (TActorIterator<AStaticMeshActor> It(World); It; ++It)
		{
			if (It->ActorHasTag(FName("ApexSkyDome"))) { SkyDomeActor = *It; break; }
		}
	}
	if (!EnvironmentMPC)
	{
		EnvironmentMPC = Cast<UMaterialParameterCollection>(StaticLoadObject(
			UMaterialParameterCollection::StaticClass(), nullptr, TEXT("/Game/Art/Materials/MPC_Environment.MPC_Environment")));
	}
}

void AApexWeatherManager::ApplyWeatherIndex(int32 Index)
{
	DiscoverSceneActors();
	CurrentWeatherIndex = FMath::Clamp(Index, 0, 2);
	const FApexWeatherSetup S = GetSetup(MapId, CurrentWeatherIndex);
	VisualWetness = S.Wetness;

	if (SunLight)
	{
		if (UDirectionalLightComponent* DLC = Cast<UDirectionalLightComponent>(SunLight->GetLightComponent()))
		{
			DLC->SetMobility(EComponentMobility::Movable);
			DLC->SetLightColor(S.SunColor);
			DLC->SetIntensity(S.SunIntensity);
			DLC->SetAtmosphereSunLight(false);
			DLC->SetLightSourceAngle(0.7f);
		}
		SunLight->SetActorRotation(S.SunRot);
	}

	// 天空由 HDRI 天穹提供；大气组件只保留给夜景/无天穹关卡
	if (SkyDomeActor && SkyDomeActor->GetStaticMeshComponent())
	{
		UStaticMeshComponent* SMC = SkyDomeActor->GetStaticMeshComponent();
		if (!SkyDomeMID)
		{
			if (UMaterialInterface* Base = SMC->GetMaterial(0))
			{
				SkyDomeMID = UMaterialInstanceDynamic::Create(Base, this);
				SMC->SetMaterial(0, SkyDomeMID);
			}
		}
		if (SkyDomeMID)
		{
			UTexture* Sky = S.SkyTexture ? Cast<UTexture>(StaticLoadObject(UTexture::StaticClass(), nullptr, S.SkyTexture, nullptr, LOAD_NoWarn | LOAD_Quiet)) : nullptr;
			if (Sky)
			{
				SkyDomeMID->SetTextureParameterValue(FName("SkyTex"), Sky);
			}
			// HDRI 太阳固定在贴图中央方向（预处理时已转正到 +X），天穹直接按太阳方位旋转
			const float SunYaw = S.SunRot.Yaw + 180.0f;
			SkyDomeActor->SetActorRotation(FRotator(0.0f, SunYaw + S.SkyYawDeg, 0.0f));
			SkyDomeMID->SetScalarParameterValue(FName("Brightness"), S.SkyBrightness);
			SkyDomeMID->SetVectorParameterValue(FName("HorizonFog"), S.FogColor);
		}
	}

	if (SkyLightActor)
	{
		if (USkyLightComponent* SLC = SkyLightActor->GetLightComponent())
		{
			SLC->SetMobility(EComponentMobility::Movable);
			SLC->bLowerHemisphereIsBlack = false;
			SLC->SetIntensity(S.SkyLightIntensity);
			SLC->SetLightColor(S.SkyLightTint);
			UTextureCube* HdriCube = S.SkyCube ? Cast<UTextureCube>(StaticLoadObject(UTextureCube::StaticClass(), nullptr, S.SkyCube, nullptr, LOAD_NoWarn | LOAD_Quiet)) : nullptr;
			if (HdriCube)
			{
				SLC->SourceType = ESkyLightSourceType::SLS_SpecifiedCubemap;
				SLC->bRealTimeCapture = false;
				// 引擎按“采样方向 +角度”旋转，与天穹 Actor 的旋转方向相反
				SLC->SourceCubemapAngle = -(S.SunRot.Yaw + 180.0f + S.SkyYawDeg);
				SLC->SetCubemap(HdriCube);
			}
			SLC->RecaptureSky();
		}
	}

	if (HeightFog)
	{
		if (UExponentialHeightFogComponent* HFC = HeightFog->GetComponent())
		{
			HFC->SetFogDensity(S.FogDensity);
			HFC->SetFogHeightFalloff(S.FogFalloff);
			HFC->SetFogInscatteringColor(S.FogColor * 0.55f);
			HFC->SetDirectionalInscatteringColor(S.SunFogColor * 0.35f);
			HFC->SetDirectionalInscatteringExponent(6.0f);
			HFC->SetDirectionalInscatteringStartDistance(3000.0f);
			HFC->SetStartDistance(S.FogStart);
			HFC->SetFogMaxOpacity(S.FogMaxOpacity);
		}
	}

	if (PostProcessVolume)
	{
		FPostProcessSettings& P = PostProcessVolume->Settings;
		P.bOverride_AutoExposureBias = true;          P.AutoExposureBias = S.Exposure;
		P.bOverride_AutoExposureMinBrightness = true; P.AutoExposureMinBrightness = S.ExpMin;
		P.bOverride_AutoExposureMaxBrightness = true; P.AutoExposureMaxBrightness = S.ExpMax;
		P.bOverride_AutoExposureSpeedUp = true;       P.AutoExposureSpeedUp = 3.0f;
		P.bOverride_AutoExposureSpeedDown = true;     P.AutoExposureSpeedDown = 2.0f;
		P.bOverride_ColorSaturation = true;           P.ColorSaturation = FVector4(1.0f, 1.0f, 1.0f, S.Saturation);
		P.bOverride_ColorContrast = true;             P.ColorContrast = FVector4(1.0f, 1.0f, 1.0f, S.Contrast);
		P.bOverride_WhiteTemp = true;                 P.WhiteTemp = S.WhiteTemp;
		P.bOverride_BloomIntensity = true;            P.BloomIntensity = S.Bloom;
		P.bOverride_MotionBlurAmount = true;          P.MotionBlurAmount = 0.28f;
		P.bOverride_MotionBlurMax = true;             P.MotionBlurMax = 3.0f;
		P.bOverride_VignetteIntensity = true;         P.VignetteIntensity = 0.18f;
		P.bOverride_FilmToe = true;                   P.FilmToe = 0.50f;
		P.bOverride_FilmSlope = true;                 P.FilmSlope = 0.88f;
	}

	if (EnvironmentMPC && GetWorld())
	{
		if (UMaterialParameterCollectionInstance* Inst = GetWorld()->GetParameterCollectionInstance(EnvironmentMPC))
		{
			Inst->SetScalarParameterValue(FName("WetnessAmount"), S.Wetness);
			Inst->SetScalarParameterValue(FName("PuddleIntensity"), S.Puddles);
			Inst->SetScalarParameterValue(FName("EmissiveBoost"), S.TrackLights);
		}
	}

	SetTrackStreetLightsIntensity(S.TrackLights);
	ApplyPetals(S.Petals);
}

void AApexWeatherManager::ApplyPetals(float Amount)
{
	if (!PetalVolume)
	{
		return;
	}
	if (Amount <= 0.01f || MapId == TEXT("KuroshioCircuit"))
	{
		PetalVolume->SetVisibility(false);
		return;
	}
	if (!PetalVolume->GetStaticMesh())
	{
		const bool bMaple = (MapId == TEXT("MomijiTouge"));
		const TCHAR* MeshPath = bMaple ? TEXT("/Game/VFX/SM_FallingMaple.SM_FallingMaple") : TEXT("/Game/VFX/SM_FallingPetals.SM_FallingPetals");
		const TCHAR* MatPath = bMaple ? TEXT("/Game/VFX/M_FallingMaple.M_FallingMaple") : TEXT("/Game/VFX/M_FallingPetals.M_FallingPetals");
		UStaticMesh* SM = Cast<UStaticMesh>(StaticLoadObject(UStaticMesh::StaticClass(), nullptr, MeshPath, nullptr, LOAD_NoWarn | LOAD_Quiet));
		UMaterialInterface* Mat = Cast<UMaterialInterface>(StaticLoadObject(UMaterialInterface::StaticClass(), nullptr, MatPath, nullptr, LOAD_NoWarn | LOAD_Quiet));
		if (!SM || !Mat)
		{
			return;
		}
		PetalVolume->SetStaticMesh(SM);
		PetalMID = UMaterialInstanceDynamic::Create(Mat, this);
		PetalVolume->SetMaterial(0, PetalMID);
	}
	if (PetalMID)
	{
		PetalMID->SetScalarParameterValue(FName("Amount"), Amount);
	}
	PetalVolume->SetVisibility(true);
}

void AApexWeatherManager::SetTrackStreetLightsIntensity(float IntensityMultiplier)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	int32 NumLights = 0;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor || !Actor->ActorHasTag(FName("TrackStreetLight")))
		{
			continue;
		}
		TArray<ULightComponent*> Lights;
		Actor->GetComponents<ULightComponent>(Lights);
		for (ULightComponent* LC : Lights)
		{
			if (!LC) continue;
			++NumLights;
			LC->SetMobility(EComponentMobility::Movable);
			// 统一用坎德拉：路灯约 6m 高，灯塔约 15m 高
			if (ULocalLightComponent* LL = Cast<ULocalLightComponent>(LC)) LL->SetIntensityUnits(ELightUnits::Candelas);
			const float BaseIntensity = Actor->ActorHasTag(FName("FloodTower")) ? 5000.0f * GetSetup(MapId, CurrentWeatherIndex).FloodScale : 260.0f;
			LC->SetIntensity(BaseIntensity * IntensityMultiplier);
			LC->SetVisibility(IntensityMultiplier > 0.3f);
		}
	}
	UE_LOG(LogTemp, Display, TEXT("[ApexWeather] track lights %d x %.2f"), NumLights, IntensityMultiplier);
}

void AApexWeatherManager::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	RippleTimeAccum += DeltaSeconds;
	if (EnvironmentMPC && GetWorld() && VisualWetness > 0.05f)
	{
		if (UMaterialParameterCollectionInstance* Inst = GetWorld()->GetParameterCollectionInstance(EnvironmentMPC))
		{
			Inst->SetScalarParameterValue(FName("RainRippleTime"), RippleTimeAccum);
		}
	}
	if (PetalVolume && PetalVolume->IsVisible())
	{
		if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
		{
			if (PC->PlayerCameraManager)
			{
				PetalVolume->SetWorldLocation(PC->PlayerCameraManager->GetCameraLocation());
			}
		}
	}
}

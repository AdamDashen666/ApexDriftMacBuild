#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/ApexDriftTypes.h"
#include "ApexWeatherManager.generated.h"

class ADirectionalLight;
class ASkyLight;
class ASkyAtmosphere;
class AExponentialHeightFog;
class APostProcessVolume;
class AStaticMeshActor;
class UMaterialInstanceDynamic;
class UMaterialParameterCollection;
class UStaticMeshComponent;
class UTextureCube;

/** 一种天气/时段的全部光照与后期参数（纯视觉，任何字段都不参与轮胎抓地计算） */
struct FApexWeatherSetup
{
	// 太阳
	FRotator SunRot = FRotator(-35.0f, -120.0f, 0.0f);
	FLinearColor SunColor = FLinearColor(1.0f, 0.95f, 0.88f);
	float SunIntensity = 9.0f;
	// 天空：HDRI 天穹 + 同源天光
	const TCHAR* SkyTexture = nullptr;     // 经纬展开 HDR 贴图（天穹用）
	const TCHAR* SkyCube = nullptr;        // 天光立方体贴图
	float SkyYawDeg = 0.0f;                // 让 HDRI 太阳对齐方向光
	float SkyBrightness = 1.0f;            // 天穹亮度
	float SkyLightIntensity = 1.2f;
	FLinearColor SkyLightTint = FLinearColor::White;
	// 雾与空气透视
	FLinearColor FogColor = FLinearColor(0.5f, 0.6f, 0.75f);
	FLinearColor SunFogColor = FLinearColor(0.6f, 0.45f, 0.3f);
	float FogDensity = 0.0004f;
	float FogFalloff = 0.12f;
	float FogStart = 0.0f;
	float FogMaxOpacity = 1.0f;
	// 视觉湿润度
	float Wetness = 0.0f;
	float Puddles = 0.0f;
	// 人工光
	float TrackLights = 0.0f;
	float FloodScale = 1.0f;       // 灯塔额外倍率（训练场 60m 高灯阵）
	bool bNight = false;
	// 后期
	float Exposure = 0.0f;
	float ExpMin = 0.0f;
	float ExpMax = 1.5f;
	float Saturation = 1.0f;
	float Contrast = 1.0f;
	float WhiteTemp = 6500.0f;
	float Bloom = 0.35f;
	// 飘落物（花瓣/红叶）密度 0..1
	float Petals = 0.0f;
};

UCLASS()
class APEXDRIFT_API AApexWeatherManager : public AActor
{
	GENERATED_BODY()

public:
	AApexWeatherManager();

	/** 对应 FApexMapRegistry 的地图 Id */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather")
	FName MapId = TEXT("KuroshioCircuit");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather")
	int32 CurrentWeatherIndex = 0;

	/** 仅供材质/水花音效使用的视觉湿润度 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather")
	float VisualWetness = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather")
	TObjectPtr<ADirectionalLight> SunLight;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather")
	TObjectPtr<ASkyLight> SkyLightActor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather")
	TObjectPtr<ASkyAtmosphere> SkyAtmosphereActor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather")
	TObjectPtr<AExponentialHeightFog> HeightFog;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather")
	TObjectPtr<APostProcessVolume> PostProcessVolume;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather")
	TObjectPtr<UMaterialParameterCollection> EnvironmentMPC;

	/** 天穹网格（Tag: ApexSkyDome） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather")
	TObjectPtr<AStaticMeshActor> SkyDomeActor;

	UFUNCTION(BlueprintCallable, Category = "Weather")
	void ApplyWeatherIndex(int32 Index);

	static FApexWeatherSetup GetSetup(FName InMapId, int32 Index);

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

private:
	void DiscoverSceneActors();
	void SetTrackStreetLightsIntensity(float IntensityMultiplier);
	void ApplyPetals(float Amount);

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> SkyDomeMID;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> PetalVolume;
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> PetalMID;

	float RippleTimeAccum = 0.0f;
};

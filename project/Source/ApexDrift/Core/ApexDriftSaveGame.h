#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Core/ApexDriftTypes.h"
#include "ApexDriftSaveGame.generated.h"

UCLASS()
class APEXDRIFT_API UApexDriftSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	static const FString DefaultSlotName;

	/** 每张地图（FApexMapInfo::Id）的最好成绩 */
	UPROPERTY()
	TMap<FName, FApexMapRecord> MapRecords;

	UPROPERTY(BlueprintReadWrite, Category = "Records")
	int32 BestSingleDrift = 0;

	UPROPERTY(BlueprintReadWrite, Category = "Records")
	float PeakDriftAngleDeg = 0.0f;

	UPROPERTY(BlueprintReadWrite, Category = "Records")
	float BestComboMultiplier = 1.0f;

	UPROPERTY(BlueprintReadWrite, Category = "Customization")
	int32 PaintPresetIndex = 0;

	/** 上次选择的车（FApexCarRegistry 序号） */
	UPROPERTY(BlueprintReadWrite, Category = "Customization")
	int32 VehicleIndex = 0;

	/** 每辆车自己的漆色（FApexCarInfo::Id） */
	UPROPERTY()
	TMap<FName, int32> CarPaint;

	/** 上次选择的地图（FApexMapRegistry 序号） */
	UPROPERTY(BlueprintReadWrite, Category = "Customization")
	int32 MapIndex = 0;

	/** 上次选择的模式 */
	UPROPERTY(BlueprintReadWrite, Category = "Customization")
	EApexGameModeType LastMode = EApexGameModeType::FreeDrift;

	/** 每张地图选择的天气序号 0..2 */
	UPROPERTY()
	TMap<FName, int32> MapWeather;

	UPROPERTY(BlueprintReadWrite, Category = "Settings")
	EApexGraphicsTier GraphicsTier = EApexGraphicsTier::Balanced;

	UPROPERTY(BlueprintReadWrite, Category = "Settings")
	EApexTSRMode TSRMode = EApexTSRMode::AutoDynamic;

	/** 动态分辨率的目标帧率：0=120 1=144 2=165 3=240 */
	UPROPERTY(BlueprintReadWrite, Category = "Settings")
	int32 FrameTargetIndex = 1;

	UPROPERTY(BlueprintReadWrite, Category = "Settings")
	int32 ResolutionIndex = 2; // 0: 1280x720, 1: 1920x1080, 2: 2560x1440, 3: 3840x2160

	UPROPERTY(BlueprintReadWrite, Category = "Settings")
	int32 WindowModeIndex = 1; // 0: Fullscreen, 1: Borderless, 2: Windowed

	UPROPERTY(BlueprintReadWrite, Category = "Settings")
	bool bVSyncEnabled = false;

	UPROPERTY(BlueprintReadWrite, Category = "Settings")
	bool bShowFps = false;

	UPROPERTY(BlueprintReadWrite, Category = "Settings")
	bool bMotionBlur = true;

	UPROPERTY(BlueprintReadWrite, Category = "Settings")
	EApexHandlingAssist HandlingAssist = EApexHandlingAssist::Balanced;

	UPROPERTY(BlueprintReadWrite, Category = "Settings")
	bool bManualTransmission = false;

	UPROPERTY(BlueprintReadWrite, Category = "Settings")
	float MasterVolume = 0.90f;

	UPROPERTY(BlueprintReadWrite, Category = "Settings")
	float EngineVolume = 0.95f;

	UPROPERTY(BlueprintReadWrite, Category = "Settings")
	float TireVolume = 0.85f;

	UPROPERTY(BlueprintReadWrite, Category = "Settings")
	float SfxVolume = 0.85f;

	UPROPERTY(BlueprintReadWrite, Category = "Settings")
	float BaseCameraFOV = 82.0f;

	int32 GetWeatherForMap(FName MapId) const
	{
		const int32* V = MapWeather.Find(MapId);
		return V ? FMath::Clamp(*V, 0, 2) : 0;
	}

	static UApexDriftSaveGame* LoadOrCreate();
	void SaveToDisk();
};

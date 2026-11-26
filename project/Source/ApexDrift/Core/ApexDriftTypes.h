#pragma once

#include "CoreMinimal.h"
#include "ApexDriftTypes.generated.h"

/** 一局结束时刷新了哪些纪录（CommitSessionRecord 的返回值，按位） */
namespace ApexRecordFlags
{
	enum : int32 { Score = 1, SingleDrift = 2, Lap = 4 };
}

UENUM(BlueprintType)
enum class EApexGameModeType : uint8
{
	FreeDrift       UMETA(DisplayName = "Free Drift"),
	DriftAttack3Min UMETA(DisplayName = "3-Min Drift Attack"),
	Circuit3Laps    UMETA(DisplayName = "3-Lap Time & Drift")
};

UENUM(BlueprintType)
enum class EApexGameState : uint8
{
	MainMenu  UMETA(DisplayName = "Main Menu"),
	Countdown UMETA(DisplayName = "Countdown"),
	Playing   UMETA(DisplayName = "Playing"),
	Paused    UMETA(DisplayName = "Paused"),
	Finished  UMETA(DisplayName = "Finished")
};

UENUM(BlueprintType)
enum class EApexHandlingAssist : uint8
{
	Balanced      UMETA(DisplayName = "Balanced Drift"),
	DriftPro      UMETA(DisplayName = "Drift Pro"),
	SimulationRaw UMETA(DisplayName = "Simulation Raw")
};

UENUM(BlueprintType)
enum class EApexGraphicsTier : uint8
{
	Performance UMETA(DisplayName = "Performance"),
	Balanced    UMETA(DisplayName = "Balanced"),
	Quality     UMETA(DisplayName = "Quality"),
	Cinematic   UMETA(DisplayName = "Cinematic RT")
};

UENUM(BlueprintType)
enum class EApexTSRMode : uint8
{
	AutoDynamic    UMETA(DisplayName = "Dynamic Resolution"),
	Native100      UMETA(DisplayName = "Native 100%"),
	TSRQuality     UMETA(DisplayName = "TSR Quality"),
	TSRBalanced    UMETA(DisplayName = "TSR Balanced"),
	TSRPerformance UMETA(DisplayName = "TSR Performance")
};

UENUM(BlueprintType)
enum class EApexCameraMode : uint8
{
	DynamicDriftChase UMETA(DisplayName = "Drift Chase"),
	CloseChase        UMETA(DisplayName = "Close Chase"),
	HoodCam           UMETA(DisplayName = "Hood Cam"),
	BumperCam         UMETA(DisplayName = "Bumper Cam")
};

USTRUCT(BlueprintType)
struct FApexDriftZoneInfo
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString ZoneName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float StartDistance = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float EndDistance = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector ClippingPoint = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float ClippingRadius = 900.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float ScoreMultiplier = 1.5f;
};

USTRUCT(BlueprintType)
struct FApexDriftTelemetry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	float SpeedKmh = 0.0f;

	UPROPERTY(BlueprintReadOnly)
	float EngineRPM = 900.0f;

	UPROPERTY(BlueprintReadOnly)
	float MaxRPM = 8000.0f;

	UPROPERTY(BlueprintReadOnly)
	int32 Gear = 1;

	UPROPERTY(BlueprintReadOnly)
	float TurboBoostBar = 0.0f;

	UPROPERTY(BlueprintReadOnly)
	float SlipAngleDeg = 0.0f;

	UPROPERTY(BlueprintReadOnly)
	bool bIsDrifting = false;

	UPROPERTY(BlueprintReadOnly)
	bool bHandbrakeActive = false;

	UPROPERTY(BlueprintReadOnly)
	bool bManualGearbox = false;

	UPROPERTY(BlueprintReadOnly)
	bool bHeadlightsOn = true;

	UPROPERTY(BlueprintReadOnly)
	bool bWrongWay = false;

	UPROPERTY(BlueprintReadOnly)
	EApexCameraMode CameraMode = EApexCameraMode::DynamicDriftChase;

	UPROPERTY(BlueprintReadOnly)
	int32 CurrentDriftScore = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 TotalScore = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 BestSingleDrift = 0;

	UPROPERTY(BlueprintReadOnly)
	float PeakAngleDeg = 0.0f;

	UPROPERTY(BlueprintReadOnly)
	float DriftMultiplier = 1.0f;

	UPROPERTY(BlueprintReadOnly)
	float MaxDriftCombo = 1.0f;

	UPROPERTY(BlueprintReadOnly)
	float TransitionGraceNormalized = 0.0f;

	UPROPERTY(BlueprintReadOnly)
	bool bInClippingZone = false;

	UPROPERTY(BlueprintReadOnly)
	FString ActiveZoneName;

	UPROPERTY(BlueprintReadOnly)
	FString CalloutText;

	UPROPERTY(BlueprintReadOnly)
	float CalloutAlpha = 0.0f;

	UPROPERTY(BlueprintReadOnly)
	int32 CurrentLap = 1;

	UPROPERTY(BlueprintReadOnly)
	int32 TotalLaps = 3;

	UPROPERTY(BlueprintReadOnly)
	float CurrentLapTime = 0.0f;

	UPROPERTY(BlueprintReadOnly)
	float BestLapTime = 0.0f;

	UPROPERTY(BlueprintReadOnly)
	float SessionTimeRemaining = 0.0f;

	/** 本局从发车起已跑的时间（三圈竞速结算的总用时） */
	UPROPERTY(BlueprintReadOnly)
	float SessionTime = 0.0f;

	UPROPERTY(BlueprintReadOnly)
	FVector2D PlayerMinimapPos = FVector2D(0.5f, 0.5f);

	UPROPERTY(BlueprintReadOnly)
	float PlayerHeadingRad = 0.0f;

	/** 自动漂移进行中 / 进度 / 自动运镜 */
	UPROPERTY(BlueprintReadOnly)
	bool bAutoRun = false;

	UPROPERTY(BlueprintReadOnly)
	float AutoProgress = 0.0f;

	UPROPERTY(BlueprintReadOnly)
	bool bAutoDirector = false;

	/** 平滑后的实时帧率（设置里打开“显示帧率”时 HUD 使用） */
	UPROPERTY(BlueprintReadOnly)
	float SmoothedFps = 0.0f;

	/** 0..1 油门/刹车输入，HUD 踏板条 */
	UPROPERTY(BlueprintReadOnly)
	float ThrottleInput = 0.0f;

	UPROPERTY(BlueprintReadOnly)
	float BrakeInput = 0.0f;
};

USTRUCT(BlueprintType)
struct FApexMapRecord
{
	GENERATED_BODY()

	UPROPERTY()
	int32 BestFreeDrift = 0;

	UPROPERTY()
	int32 BestAttack = 0;

	UPROPERTY()
	float BestLapSec = 0.0f;
};

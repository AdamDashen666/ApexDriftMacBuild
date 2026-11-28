#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Core/ApexDriftTypes.h"
#include "InputActionValue.h"
#include "ApexDriftPlayerController.generated.h"

class UInputMappingContext;
class UInputAction;
class AApexDriftVehiclePawn;
class AApexTrackSplineActor;
class AApexWeatherManager;
class UApexDriftSaveGame;
class UApexHudWidget;
class UApexMainMenuWidget;
class UApexPauseMenuWidget;
class UApexSettingsWidget;
class UApexFinishWidget;

UCLASS()
class APEXDRIFT_API AApexDriftPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AApexDriftPlayerController();

	UFUNCTION(BlueprintCallable, Category = "GameFlow")
	void StartRaceSession(EApexGameModeType ModeType);

	UFUNCTION(BlueprintCallable, Category = "GameFlow")
	void RestartCurrentRace();

	UFUNCTION(BlueprintCallable, Category = "GameFlow")
	void ReturnToMainMenu();
	/** 自动驾驶/验证流程接管输入时置真，停用按键轮询 */
	bool bExternalDrive = false;

	/** 自动漂移一圈：总是回到发车格从头开始 */
	UFUNCTION(BlueprintCallable, Category = "GameFlow")
	void StartAutoRun();
	/** 玩家接管：自动结束，不能继续 */
	void CancelAutoRun();
	/** 当前这一局有自动驾驶参与，不计成绩 */
	bool IsAutoSession() const { return bAutoSession; }
	bool IsAutoPending() const { return bAutoPending; }
	void StartAutoRunOnMap(int32 MapIndex);

	UFUNCTION(BlueprintCallable, Category = "GameFlow")
	void PauseGame();

	UFUNCTION(BlueprintCallable, Category = "GameFlow")
	void ResumeGame();

	UFUNCTION(BlueprintCallable, Category = "GameFlow")
	void OpenSettingsMenu();

	UFUNCTION(BlueprintCallable, Category = "GameFlow")
	void CloseSettingsMenu();

	/** 换车（FApexCarRegistry 序号），立即生效并保存 */
	UFUNCTION(BlueprintCallable, Category = "ApexDrift")
	void SetVehicleCar(int32 CarIndex);

	/** 选定漆面（FApexPaintRegistry 序号），立即生效并保存 */
	UFUNCTION(BlueprintCallable, Category = "GameFlow")
	void SetVehiclePaint(int32 PaintIndex);

	/** 选定当前地图的天气 0..2，立即生效并保存 */
	UFUNCTION(BlueprintCallable, Category = "GameFlow")
	void SetWeatherIndex(int32 WeatherIndex);

	/** 切换到另一张地图并停在主菜单（菜单背景随之变化）；同一张地图时什么也不做 */
	UFUNCTION(BlueprintCallable, Category = "GameFlow")
	void PreviewMap(int32 MapIndex);

	/** 在指定地图开始比赛；如果不是当前地图，先加载关卡再自动开始倒计时 */
	UFUNCTION(BlueprintCallable, Category = "GameFlow")
	void StartRaceOnMap(int32 MapIndex, EApexGameModeType ModeType);

	int32 GetCurrentMapIndex() const { return CurrentMapIndex; }
	int32 GetCurrentWeatherIndex() const;
	EApexGameModeType GetCurrentMode() const { return CurrentMode; }
	UApexDriftSaveGame* GetSaveData() const { return SaveData; }
	AApexDriftVehiclePawn* GetVehiclePawn() const { return VehiclePawn; }
	AApexTrackSplineActor* GetTrackActor() const { return TrackActor; }

	UFUNCTION(BlueprintCallable, Category = "GameFlow")
	void ResetVehicleToTrack();

	/** 训练场一类的自由场地 */
	bool IsFreeRoamMap() const;
	double LastResetPress = -10.0;
	float GetPlayingTime() const { return PlayingTime; }
	float PlayingTime = 0.0f;

	UFUNCTION(BlueprintCallable, Category = "GameFlow")
	void ApplyAndSaveSettings(bool bIncludeVideoMode);

	/** 改键后调用：按 ApexKeys 重建键盘映射并存档 */
	void ApplyKeyBindings();

	void PlayMenuClickSound();
	EApexGameState GetGameState() const { return CurrentGameState; }

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	virtual void OnPossess(APawn* InPawn) override;
	virtual void Tick(float DeltaSeconds) override;

private:
	void BuildProgrammaticInput();
	void RebuildKeyMappings();
	void CreateWidgets();
	void UpdateWidgetVisibility();
	void FinishRaceSession();
	/** 把当前局成绩写入存档；返回是否刷新纪录 */
	int32 CommitSessionRecord();   // 返回 ApexRecordFlags 按位组合，0 = 没破纪录
	void PlaceVehicleAtStartGrid();
	void TickVerifyGameplay(float DeltaSeconds);
	void WriteVerifyReportAndExit();

	void OnShiftUp(const FInputActionValue& Value);
	void OnShiftDown(const FInputActionValue& Value);
	void OnCycleCamera(const FInputActionValue& Value);
	void OnLookBackStart(const FInputActionValue& Value);
	void OnLookBackStop(const FInputActionValue& Value);
	void OnResetCar(const FInputActionValue& Value);
	void OnToggleLights(const FInputActionValue& Value);
	void OnTogglePause(const FInputActionValue& Value);
	void OnLookAround(const FInputActionValue& Value);

	UPROPERTY()
	TObjectPtr<UInputMappingContext> VehicleIMC;
	UPROPERTY()
	TObjectPtr<UInputAction> IA_Throttle;
	UPROPERTY()
	TObjectPtr<UInputAction> IA_Brake;
	UPROPERTY()
	TObjectPtr<UInputAction> IA_Steering;
	UPROPERTY()
	TObjectPtr<UInputAction> IA_Handbrake;
	UPROPERTY()
	TObjectPtr<UInputAction> IA_ShiftUp;
	UPROPERTY()
	TObjectPtr<UInputAction> IA_ShiftDown;
	UPROPERTY()
	TObjectPtr<UInputAction> IA_Camera;
	UPROPERTY()
	TObjectPtr<UInputAction> IA_LookBack;
	UPROPERTY()
	TObjectPtr<UInputAction> IA_Reset;
	UPROPERTY()
	TObjectPtr<UInputAction> IA_Headlights;
	UPROPERTY()
	TObjectPtr<UInputAction> IA_AutoRun;
	UPROPERTY()
	TObjectPtr<UInputAction> IA_Director;
	UPROPERTY()
	TObjectPtr<UInputAction> IA_Pause;
	UPROPERTY()
	TObjectPtr<UInputAction> IA_Look;

	UPROPERTY()
	TObjectPtr<AApexDriftVehiclePawn> VehiclePawn;
	UPROPERTY()
	TObjectPtr<AApexTrackSplineActor> TrackActor;
	UPROPERTY()
	TObjectPtr<AApexWeatherManager> WeatherManager;
	UPROPERTY()
	TObjectPtr<UApexDriftSaveGame> SaveData;

	UPROPERTY()
	TObjectPtr<UApexHudWidget> HudWidget;
	UPROPERTY()
	TObjectPtr<UApexMainMenuWidget> MainMenuWidget;
	UPROPERTY()
	TObjectPtr<UApexPauseMenuWidget> PauseMenuWidget;
	UPROPERTY()
	TObjectPtr<UApexSettingsWidget> SettingsWidget;
	UPROPERTY()
	TObjectPtr<UApexFinishWidget> FinishWidget;

	int32 CurrentMapIndex = 0;
	float SmoothedDelta = 0.0f;
	EApexGameModeType CurrentMode = EApexGameModeType::FreeDrift;
	EApexGameState CurrentGameState = EApexGameState::MainMenu;
	EApexGameState StateBeforeSettings = EApexGameState::MainMenu;
	bool bInSettingsOverlay = false;

	float CountdownTimer = 0.0f;
	int32 LastCountdownBeep = 4;
	FTransform StartGridTransform;
	bool bHasStartGrid = false;

	// Automated functional & UI screenshot verification (-ApexVerifyGameplay)
	bool bVerifyGameplayMode = false;
	void PollDrivingInput();
	void TickAutoRun();
	void OnToggleAuto(const FInputActionValue& Value);
	void OnToggleDirector(const FInputActionValue& Value);
	bool bAutoSession = false;
	bool bAutoPending = false;
	bool bAutoNeutral = false;
	bool bAutoWasRunning = false;
	void TickVerifyAuto();   // -ApexVerifyAuto：真实按键检查自动漂移的开始/暂停/接管/重来/复位/不计成绩
	void TickVerifyInput();  // -ApexVerifyInput：在进程内发真实键盘事件，检查整条输入链路
	void TickVerifyCars();   // -ApexVerifyCars：菜单里逐辆换车截图
	void TickVerifyFlow();   // -ApexVerifyFlow：按真实按钮走一遍菜单/跨图/暂停/结算并截图
	void TickVerifyPick();   // -ApexVerifyPick：主菜单逐张点地图卡片（含第 4 张湖畔训练场），再点开始比赛，确认进的是训练场
	void TickVerifyKeys();   // -ApexVerifyKeys：在设置页用真实按键改键，再开车检查新键生效、旧键失效
	int32 VerifyPhase = 0;
	double VerifyPhaseStartTime = 0.0;
	double VerifyPhaseSimTime = 0.0;
	double VerifyLastLogSimTime = -1.0;
	double VerifySustainDriftShotTaken = 0.0;
	float VerifyMaxSpeedKmh = 0.0f;
	int32 VerifyMaxGear = 0;
	float VerifyPeakSlipAngleDeg = 0.0f;
	int32 VerifyPeakCurrentDriftScore = 0;
	int32 VerifyBankedTotalScore = 0;
	float VerifyMaxCombo = 1.0f;
	bool bVerifySaveGameRoundTrip = false;
	bool bVerifyResetSucceeded = false;
	int32 VerifySkidSegments = 0;
	int32 VerifySmokeEmitted = 0;
};

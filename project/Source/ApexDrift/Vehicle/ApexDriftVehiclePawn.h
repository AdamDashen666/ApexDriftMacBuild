#pragma once

#include "CoreMinimal.h"
#include "WheeledVehiclePawn.h"
#include "Core/ApexDriftTypes.h"
#include "Vehicle/ApexAutoRun.h"
#include "ApexDriftVehiclePawn.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UStaticMeshComponent;
class USpotLightComponent;
class UPointLightComponent;
class USceneComponent;
class UMaterialInstanceDynamic;
class UApexDriftScoreComponent;
class UApexVehicleAudioComponent;
class UApexDriftVFXComponent;
class AApexTrackSplineActor;
class AApexSkidmarkManager;
class AApexWeatherManager;
class UStaticMesh;
struct FApexCarInfo;

UCLASS()
class APEXDRIFT_API AApexDriftVehiclePawn : public AWheeledVehiclePawn
{
	GENERATED_BODY()

public:
	AApexDriftVehiclePawn();

	// 主角车可视部件（物理仍由隐藏的 Chaos 骨骼网格驱动）
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle|Visuals")
	TObjectPtr<UStaticMeshComponent> ChassisMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle|Visuals")
	TObjectPtr<UStaticMeshComponent> ChassisGlassMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle|Visuals")
	TObjectPtr<UStaticMeshComponent> LightsMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle|Visuals")
	TObjectPtr<UStaticMeshComponent> GTWingMesh;

	/** 0 FL 1 FR 2 RL 3 RR：枢轴负责悬挂位移与转向，轮子只自转，卡钳不自转 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle|Visuals")
	TArray<TObjectPtr<USceneComponent>> WheelPivots;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle|Visuals")
	TArray<TObjectPtr<UStaticMeshComponent>> WheelMeshes;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle|Visuals")
	TArray<TObjectPtr<UStaticMeshComponent>> CaliperMeshes;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle|Lights")
	TObjectPtr<USpotLightComponent> HeadlightL;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle|Lights")
	TObjectPtr<USpotLightComponent> HeadlightR;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle|Lights")
	TObjectPtr<UPointLightComponent> TaillightL;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle|Lights")
	TObjectPtr<UPointLightComponent> TaillightR;

	// Cameras
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle|Camera")
	TObjectPtr<USpringArmComponent> ChaseSpringArm;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle|Camera")
	TObjectPtr<UCameraComponent> ChaseCamera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle|Camera")
	TObjectPtr<UCameraComponent> HoodCamera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle|Camera")
	TObjectPtr<UCameraComponent> BumperCamera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle|Camera")
	TObjectPtr<USpringArmComponent> ShowroomSpringArm;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle|Camera")
	TObjectPtr<UCameraComponent> ShowroomCamera;

	/** 自动漂移时的运镜相机（世界坐标） */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle|Camera")
	TObjectPtr<UCameraComponent> CineCamera;

	// Gameplay & Audio components
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle|Systems")
	TObjectPtr<UApexDriftScoreComponent> DriftScoreComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle|Systems")
	TObjectPtr<UApexVehicleAudioComponent> VehicleAudioComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle|Systems")
	TObjectPtr<UApexDriftVFXComponent> DriftVFXComp;

	// Controls & State API (called by PlayerController / GameMode / Automated Tests)
	void SetThrottleInput(float Value);
	void SetBrakeInput(float Value);
	void SetSteeringInput(float Value);
	void SetHandbrakeInput(bool bPressed);
	void ShiftUp();
	void ShiftDown();
	void CycleCameraMode();
	void SetCameraMode(EApexCameraMode NewMode);
	void SetShowroomOrbitMode(bool bEnableShowroom);
	void AddShowroomOrLookDelta(float YawDelta, float PitchDelta);
	// 截图流程用：固定展示镜头角度（停止自转）
	void DebugShowroom(float Yaw, float Pitch, float Arm, bool bFreeze) { ShowroomYaw = Yaw; ShowroomPitch = Pitch; DebugArm = Arm; bShowroomFreeze = bFreeze; }
	void SetLookBack(bool bEnableLookBack);
	void ToggleHeadlights();
	UFUNCTION(BlueprintCallable, Category = "ApexDrift")
	void SetHeadlightsEnabled(bool bEnabled);
	void ResetToTrack();
	/** 清掉漂移、辅助、滤波等操控状态 */
	void ResetHandlingState();

	UFUNCTION(BlueprintCallable, Category = "ApexDrift")
	void InitializeVisualsForPreview();

	UFUNCTION(BlueprintCallable, Category = "ApexDrift")
	void ApplyPaintPreset(int32 PresetIndex);
	/** 换车（FApexCarRegistry 序号）：换可视部件、灯光相机位置和操控参数 */
	UFUNCTION(BlueprintCallable, Category = "ApexDrift")
	void SetCar(int32 InCarIndex);
	int32 GetCarIndex() const { return CarIndex; }
	const FApexCarInfo& CarInfo() const { return Car(); }
	/** 全油门目标加速度（m/s2） */
	static float TargetAccel(float Kmh, const FApexCarInfo& Info);

	// ---- 自动漂移一圈（FApexAutoRun）
	void PrepareAutoRun();
	bool BeginAutoRun();
	void EndAutoRun();
	bool IsAutoRunning() const { return AutoRun.IsActive(); }
	void ToggleDirector() { bDirector = !bDirector; }
	FApexAutoRun& GetAutoRun() { return AutoRun; }
	/** 自动驾驶的表现输入：油门/刹车/前轮角/手刹 + 前后轮抓地 */
	void AutoApplyInputs(float Throttle, float Brake, float SteerDeg, bool bHand, float FrontGrip, float RearGrip);
	void ApplyHandlingSettings(EApexHandlingAssist Assist, bool bManual, float InBaseFOV);
	void SetControlsLocked(bool bLocked);
	void SetPausedState(bool bPaused);

	const FApexDriftTelemetry& GetTelemetry() const { return Telemetry; }
	AApexSkidmarkManager* GetSkidmarkManager() const { return SkidmarkManager; }
	int32 GetPaintPresetIndex() const { return CurrentPaintPreset; }
	bool IsDriftActive() const { return bDriftActive; }
	float GetRawSteering() const { return RawSteering; }

protected:
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION()
	void OnChassisHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

private:
	void SetupChassisPhysicsAndEngine();
	const FApexCarInfo& Car() const;
	void ApplyCarVisuals();
	void EmitExhaustFlames();
	/** 物理轮位与当前车可视轮位不同：把接地点挪到看得见的轮子下面 */
	FVector VisualContact(int32 Wheel, const FVector& PhysContact) const;
	void SetupVisualMaterials();
	void BindSceneManagers();
	void UpdateHandling(float DeltaSeconds, float SpeedKmh, float SlipAngleDeg, bool bGrounded);
	void SetAllWheelGrip(float FrontGrip, float RearGrip);
	void EndDrift();
	/** 拟真档的自由滑动（手刹甩起来以后）；返回 true 表示本帧已由它接管 */
	bool UpdateSimSlide(float Dt, float SpeedKmh, bool bGrounded, bool bForward);
	void UpdateWheelVisuals();
	void UpdateCameraSystem(float DeltaSeconds, float SpeedKmh, float SlipAngleDeg);
	void UpdateVFXAndSkidmarks(float DeltaSeconds, float& OutMaxSkid, bool& bOutOnCurb);
	void UpdateLightsAndMaterials(float DeltaSeconds, float SpeedKmh);

	UPROPERTY()
	TObjectPtr<AApexTrackSplineActor> TrackSplineActor;
	UPROPERTY()
	TObjectPtr<AApexSkidmarkManager> SkidmarkManager;
	UPROPERTY()
	TObjectPtr<AApexWeatherManager> WeatherManager;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> BodyPaintMID;
	UPROPERTY()
	TObjectPtr<class UPhysicalMaterial> AutoPhysMat;
	UPROPERTY()
	TArray<TObjectPtr<UMaterialInstanceDynamic>> TintMIDs;   // 贴图车的可染色槽
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> CaliperMID;
	UPROPERTY()
	TArray<TObjectPtr<UMaterialInstanceDynamic>> HeadLightMIDs;
	UPROPERTY()
	TArray<TObjectPtr<UMaterialInstanceDynamic>> TailLightMIDs;
	UPROPERTY()
	TArray<TObjectPtr<UMaterialInstanceDynamic>> BrakeLightMIDs;
	UPROPERTY()
	TArray<TObjectPtr<UMaterialInstanceDynamic>> ReverseLightMIDs;

	FApexDriftTelemetry Telemetry;
	EApexHandlingAssist HandlingAssist = EApexHandlingAssist::Balanced;
	EApexCameraMode CameraMode = EApexCameraMode::DynamicDriftChase;

	bool bControlsLocked = true;
	bool bPaused = false;
	bool bShowroomOrbit = true;
	bool bLookBack = false;
	bool bHeadlightsOn = true;
	bool bManualGearbox = false;

	float RawThrottle = 0.0f;
	float RawBrake = 0.0f;
	float RawSteering = 0.0f;
	bool bRawHandbrake = false;
	float PrevThrottle = 0.0f;
	int32 PrevGear = 1;

	float BaseFOV = 82.0f;
	float LookYawOffset = 0.0f;
	float ShowroomYaw = -32.0f;
	float ShowroomPitch = -5.3f;
	float DebugArm = 0.0f;
	bool bShowroomFreeze = false;
	float CurrentChaseYawOffset = 0.0f;
	float CameraYaw = 0.0f;
	float CamAccel = 0.0f;
	float CamShakeTime = 0.0f;
	bool bCameraYawValid = false;
	float TurboPressureBar = 0.0f;

	// 操控状态
	float SmoothedSteer = 0.0f;
	bool bDriftActive = false;
	float DriftDir = 0.0f;          // +1 右漂 -1 左漂
	float DriftTime = 0.0f;
	float DriftExitTimer = 0.0f;
	float DriftAngle = 0.0f;        // 当前目标侧滑角（度），收到 0 就退出漂移
	bool bSimFree = false;          // 拟真档：正在自由滑动（手刹触发，不拉回）
	float SimRate = 0.0f;           // 车头相对行进方向的角速度（度/秒）
	float SimPsi = 0.0f;            // 车头相对行进方向的夹角（度，右正）
	float SimSpin = 0.0f;           // 累计转过的角度
	float SimAirT = 0.0f;
	float ExitAssistTimer = 0.0f;   // 刚退出漂移：强稳定
	float LastPropHitTime = -10.0f;
	float WallAssistTimer = 0.0f;   // 刚擦墙：强稳定
	float PowerOverTimer = 0.0f;
	float GripBlend = 1.0f;         // 1 抓地 0 漂移
	float PrevVelYawDeg = 0.0f;
	bool bPrevVelYawValid = false;
	float PathRateDeg = 0.0f;       // 速度方向角速度
	float PrevSpeedCms = 0.0f;
	float TangentAccel = 0.0f;      // 平滑后的切向加速度 m/s2
	float TangentAssist = 0.0f;     // 切向补偿积分 m/s2
	float AppliedFrontGrip = -1.0f;
	float AppliedRearGrip = -1.0f;
	float SmokeEmitAccum = 0.0f;
	int32 CurrentPaintPreset = 0;
	int32 CarIndex = 0;
	FApexAutoRun AutoRun;
	float CamPsi = 0.0f;
	bool bDirector = true;
	int32 CineShot = 0;          // 0 追尾 1.. 运镜
	int32 CineSeq = 0;
	float CineT = 0.0f;
	float CineDur = 0.0f;
	float CineSide = 1.0f;
	float CineZ = 0.0f;
	float CineAngle = 0.0f;
	bool bCineReset = false;
	bool UpdateDirector(float Dt);
	void SetCineShot(int32 Shot, float Dur);
	bool bCarApplied = false;

	TArray<FVector> PrevWheelContacts;
	TArray<bool> PrevWheelSkidding;
};

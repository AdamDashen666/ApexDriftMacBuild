#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "ApexBenchSubsystem.generated.h"

class AApexDriftPlayerController;
class AApexTrackSplineActor;

/**
 * 自动跑分/巡航验证（仅带命令行参数时启用，正式游玩零开销）：
 *   -ApexBench            自动驾驶跑圈，统计帧时间写到 Saved/Bench/<label>.json 后退出
 *   -ApexBenchDrift       弯道主动拉手刹漂移，用来验证操控
 *   -ApexBenchSecs=90     最长采样秒数（跑完一圈也会结束）
 *   -ApexBenchShots=6     每隔多少秒截一张图（0 不截）
 *   -ApexBenchSpeed=110   直道目标车速 km/h
 *   -ApexBenchLabel=xxx   输出文件名
 *   -ApexBenchNoUI        截图不带 HUD
 *   -ApexBenchPad         固定脚本的平地操控测试
 *   -ApexBenchKeys        模拟键盘玩家的场景测试（需同时带 -ApexBenchPad）
 *   -ApexAssist=0/1/2     覆盖存档里的漂移辅助（均衡/专业/拟真）
 */
UCLASS()
class APEXDRIFT_API UApexBenchSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UApexBenchSubsystem, STATGROUP_Tickables); }
	virtual bool IsTickable() const override { return bEnabled; }

protected:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

private:
	void Drive(float DeltaTime);
	void DrivePad(float DeltaTime);
	void Finish();

	bool bEnabled = false;
	bool bDrift = false;
	bool bPad = false;
	bool bKeys = false;         // -ApexBenchKeys：模拟真人键盘操作的一组场景，量“多久能拉直”
	int32 KeySeg = 0;
	float KeySegT = 0.0f;
	float ShiftCooldown = 0.0f;
	bool bMonkey = false;       // -ApexBenchMonkey：键盘式 0/1 方向 + 随机失误 + 随手点手刹
	float MonkeySteer = 0.0f;
	float MonkeyHold = 0.0f;
	float MonkeyHand = 0.0f;
	float MonkeyHandNext = 4.0f;
	float MonkeyRandomT = -1.0f, MonkeyRandLastT = -99.0f, MonkeyWallT = -99.0f, MonkeyHandT = -99.0f;
	float MonkeySlideT = -1.0f, MonkeySlideV = 0.0f, MonkeySlideThr = 0.0f, MonkeySlideStr = 0.0f;
	TArray<FString> MonkeyEvents;   // 每次掉头前后的情况
	float CurDrift = 0.0f;
	float MaxDriftRun = 0.0f;   // 单次连续漂移最长秒数
	float CurCrooked = 0.0f;
	float MaxCrooked = 0.0f;    // 没按手刹、没打方向却还没对正的最长秒数
	int32 DriftRuns = 0;
	int32 WallHits = 0;
	float KeyStraightT = 0.0f;
	float KeyDriftT = 0.0f;
	float KeyMaxSlip = 0.0f;
	void DriveKeys(float DeltaTime);
	void DriveSpin(float DeltaTime);
	/** -ApexBenchPad -ApexBenchSim：拟真档轮胎物理（直线稳定 / 甩尾后有没有被拉回 / 倒着滑 / 油门甩尾 / 机器人保持漂移 / 倒车） */
	void DriveSim(float DeltaTime);
	int32 SimCase = 0, SimStage = 0;
	float SimT = 0.0f, SimPsiPrev = 0.0f, SimPeak = 0.0f, SimBackT = 0.0f, SimErrSum = 0.0f, SimBandT = 0.0f, SimMaxYaw = 0.0f, SimMaxPsiStraight = 0.0f;
	int32 SimErrN = 0;
	float SimPsiPrevAcc = 0.0f;
	float SimMarks[3] = { -1.0f, -1.0f, -1.0f };
	/** -ApexBenchPad -ApexBenchGrip：键盘玩家平时开车（不拉手刹）的出弯给油、弯中收油、刹车入弯、蛇行、变道、S 弯，量最大侧滑和松手后多久稳住 */
	void DriveGrip(float DeltaTime);
	int32 GripCase = 0, GripStep = -1;
	float GripT = 0.0f, GripPeak = 0.0f, GripPeakYaw = 0.0f, GripCalmT = -1.0f, GripStepPeak = 0.0f;
	FString GripSteps;
	TArray<FString> GripSummary;
	void DriveRamps(float DeltaTime);   // -ApexBenchRamps：训练场逐个坡道冲一遍，量腾空、落地、翻车，再撞道具
	float RampAir = 0.0f, RampMaxZ = 0.0f, RampMinUp = 1.0f, RampLat = 0.0f, RampMinFps = 0.0f, RampWorstMs = 0.0f;
	int32 RampShot = 0;
	bool bRace = false;         // -ApexBenchRace：三圈竞速跑完，等结算画面出来截图，记各圈用时
	float RaceDoneT = -1.0f;
	float RaceLapStart = 0.0f;
	int32 RaceLastLap = 1;
	bool bRaceShot = false;
	TArray<float> RaceLaps;
	void WriteRace(bool bFinished);
	void DriveResetR(float DeltaTime);   // -ApexBenchResetR：训练场撞散道具后用真实按键按一次 / 连按两次复位键
	int32 RStep = 0;
	float RStepT = 0.0f;
	int32 RFrames = 0;
	int32 RFails = 0;
	FVector RCrash = FVector::ZeroVector;
	TArray<FString> RLog;
	bool bAuto = false;
	bool bAutoSeen = false;
	float AutoDoneT = 0.0f;
	FVector AutoStart = FVector::ZeroVector;
	float SpinAccum = 0.0f;
	float SpinPrev = 0.0f;
	float SpinPeak = 0.0f;
	float SpinMinV = 0.0f;
	bool bShotUI = true;
	float PadLogTimer = 0.0f;
	TArray<FString> PadLog;
	float MaxSecs = 90.0f;
	float ShotInterval = 0.0f;
	float CruiseKmh = 110.0f;
	FString Label;

	int32 Phase = 0;
	double PhaseStart = 0.0;
	float Elapsed = 0.0f;
	float NextShot = 0.0f;
	float ShotCooldown = 0.0f;
	int32 ShotIndex = 0;

	float StartDist = -1.0f;
	float LastDist = 0.0f;
	float Travelled = 0.0f;
	float HandbrakeTimer = 0.0f;
	float CornerCooldown = 0.0f;
	float StuckTimer = 0.0f;

	int32 Resets = 0;
	int32 Spins = 0;
	bool bSpinning = false;
	float MaxSlipGrip = 0.0f;
	float MaxSlipDrift = 0.0f;
	float DriftSeconds = 0.0f;
	float MaxLateral = 0.0f;
	float OffTrackSeconds = 0.0f;
	float MaxSpeed = 0.0f;

	TArray<float> FrameMs;
	TArray<float> GpuMs;
	TArray<float> GameMs;
	TArray<float> RenderMs;
	TArray<float> ResFrac;
	TArray<FString> SecondLog;
	float SecAccum = 0.0f;
	int32 SecFrames = 0;
	float SecWorst = 0.0f;
	float SecGpu = 0.0f;
};

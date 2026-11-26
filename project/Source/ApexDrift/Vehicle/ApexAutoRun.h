#pragma once

#include "CoreMinimal.h"

class AApexTrackSplineActor;
class AApexDriftVehiclePawn;
struct FApexCarInfo;

/**
 * 自动漂移一圈。
 * 开始时按赛道样条一次算好整圈计划（走线、每点车速、漂移角、360 的位置，不含随机数，每次一样），
 * 运行时把速度方向锁在走线上、把车头锁在计划角度上。只从发车格开始，跑满一圈即结束；中断后没有“继续”。
 */
class FApexAutoRun
{
public:
	struct FSpin { float Start = 0.0f; float Dur = 1.35f; float Dir = 1.0f; bool bEntry = false; };
	struct FStats
	{
		float LapTime = 0.0f, AvgKmh = 0.0f, MaxKmh = 0.0f, DriftPct = 0.0f, MaxLatErr = 0.0f, MaxOffCenter = 0.0f, MaxAngle = 0.0f, WorstPsiErr = 0.0f;
		int32 Corners = 0, Spins = 0, SpinsDone = 0, WallHits = 0, AirFrames = 0;
		float PlannedLap = 0.0f;
	};

	/** 从车的当前位置（发车格）建立整圈计划（几十毫秒，放在倒计时开始时做） */
	bool Prepare(const AApexTrackSplineActor* Track, const AApexDriftVehiclePawn& Car, const FApexCarInfo& Info);
	bool Begin() { bActive = bReady; bReady = false; return bActive; }
	void Stop() { bActive = false; bReady = false; }
	bool IsActive() const { return bActive; }
	bool IsFinished() const { return bFinished; }
	bool IsSpinning() const { return bActive && bSpinning; }
	/** 车头相对行进方向的夹角（度，右正，±180） */
	float GetPsi() const { return Psi; }
	float GetPathYaw() const { return CmdYaw; }
	/** 当前位置所在弯道的方向（+1 右 -1 左 0 直道） */
	float GetCornerDir() const { return CornerDir; }
	float GetLateral() const { return LatFromCenter; }
	float GetElapsed() const { return Elapsed; }
	float GetProgress() const { return LapLen > 1.0f ? FMath::Clamp(Along / LapLen, 0.0f, 1.0f) : 0.0f; }
	/** 走线上前方 Ahead 米、离赛道中心 Lat 米处的世界坐标（厘米，Z=0）与方向 */
	FVector2D PathPoint(float Ahead, float Lat, FVector2D& OutDir) const;
	/** 距离下一个 360 还有多少秒（没有则很大） */
	float TimeToNextSpin() const { return NextSpinIn; }
	void NotifyHit(bool bWall, const FString& What);
	const FStats& GetStats() const { return Stats; }
	const TArray<FString>& GetLog() const { return Log; }

	/** 每帧由车辆 UpdateHandling 调用，代替所有操控逻辑 */
	void Drive(AApexDriftVehiclePawn& Car, float Dt, bool bGrounded);

private:
	void BuildLine(const AApexTrackSplineActor* Track, float StartDist, float StartLat);
	void BuildSpeed(const FApexCarInfo& Info);
	void BuildDrift(const FApexCarInfo& Info);
	float SampleTime(const TArray<float>& A, float T) const;

	// 沿路线（间隔 Ds 米）
	static constexpr float Ds = 2.0f;
	TArray<FVector2D> Q;          // 走线点（米）
	TArray<FVector2D> Center;     // 赛道中心
	TArray<FVector2D> RightDir;
	TArray<float> Kappa;          // 曲率 1/m，右弯为正
	TArray<float> V;              // 计划车速 m/s
	TArray<float> T;              // 到达各点的时间 s
	float LapLen = 0.0f;

	// 按时间（间隔 Dtp 秒）
	static constexpr float Dtp = 0.02f;
	TArray<float> PsiPlan;        // 目标夹角（度，含 360 的累计）
	TArray<float> BasePsi;        // 不含 360
	TArray<float> Corner;         // 弯道方向
	TArray<FSpin> Spins;

	bool bActive = false;
	bool bReady = false;
	bool bFinished = false;
	bool bSpinning = false;
	int32 Idx = 0;
	float Psi = 0.0f;
	float CmdYaw = 0.0f;
	float CmdV = 0.0f;
	bool bCmdValid = false;
	float CornerDir = 0.0f;
	float LatFromCenter = 0.0f;
	float Elapsed = 0.0f;
	float StuckT = 0.0f;
	float AirT = 0.0f;
	float Along = 0.0f;
	float NextSpinIn = 1e6f;
	float Ei = 0.0f;
	float OffT = 0.0f;
	float LogT = 0.0f;
	int32 SpinIdx = 0;
	double SumV = 0.0;
	double DriftT = 0.0;
	float YawCmd = 0.0f;
	int32 Gear = 1;
	FStats Stats;
	TArray<FString> Log;
};

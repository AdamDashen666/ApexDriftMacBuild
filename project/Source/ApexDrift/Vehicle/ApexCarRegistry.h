#pragma once

#include "CoreMinimal.h"

/** 一辆车的几何数据（厘米，车身坐标：原点在前后轴中点的地面，车头 +X、车右 +Y） */
struct FApexCarGeometry
{
	FVector Wheel[4] = { FVector(145.75, -81.77, 32.09), FVector(145.75, 81.77, 32.09), FVector(-145.75, -90.66, 34.74), FVector(-145.75, 90.66, 34.74) }; // FL FR RL RR 轮心
	float RadiusF = 32.0f;
	float RadiusR = 34.0f;
	float WidthF = 26.0f;
	float WidthR = 28.0f;
	FVector Headlight = FVector(190, 76, 55);   // 车右侧
	FVector Taillight = FVector(-183, 90, 72);  // 车右侧
	TArray<FVector> Exhausts;
	FVector HoodCam = FVector(128, 0, 98);
	FVector BumperCam = FVector(244, 0, 44);
	float Length = 457.0f;
	float Width = 200.0f;
	float Height = 108.0f;
};

/** 车辆表：外观路径、几何、性能、界面文案。换车只改可视部件与操控参数，物理底盘共用 */
struct FApexCarInfo
{
	FName Id;
	FString Name;        // 大标题（拉丁字）
	FString ClassZh;     // 类别
	FString Tagline;     // 一句话
	// 性能倍率（主角车 = 1）
	float Accel = 1.0f;
	float TopKmh = 368.0f;
	float Grip = 1.0f;       // 侧向能力
	float Agility = 1.0f;    // 打方向响应
	float Drift = 1.0f;      // 漂移角
	float SoundPitch = 1.0f;
	float SoundVolume = 1.0f;
	bool bTurbo = true;
	bool bExhaustFlame = true;
	int32 DefaultPaint = 0;
	// 界面性能条 0..1：极速 加速 抓地 漂移
	float Stats[4] = { 0.8f, 0.8f, 0.8f, 0.8f };
	FApexCarGeometry Geo;

	FString MeshPath(const TCHAR* Part) const
	{
		const FString I = Id.ToString();
		return FString::Printf(TEXT("/Game/Vehicles/%s/SM_%s_%s.SM_%s_%s"), *I, *I, Part, *I, Part);
	}
};

class APEXDRIFT_API FApexCarRegistry
{
public:
	static const TArray<FApexCarInfo>& GetCars();
	static int32 Num() { return GetCars().Num(); }
	static int32 Wrap(int32 Index)
	{
		const int32 N = Num();
		return ((Index % N) + N) % N;
	}
	static const FApexCarInfo& Get(int32 Index) { return GetCars()[Wrap(Index)]; }
};

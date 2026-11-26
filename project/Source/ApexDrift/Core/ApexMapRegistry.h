#pragma once

#include "CoreMinimal.h"

/** 一个地图可选的天气/时段 */
struct FApexWeatherInfo
{
	FString Name;      // 例：晴朗上午
	FString Detail;    // 例：能见度极佳，湖面如镜
	FName IconKey;     // Sun / Sunset / Rain / Moon / Cloud / Mist
};

/** 一张可选地图的静态信息（供 UI、关卡切换、天气表共用） */
struct FApexMapInfo
{
	FName Id;
	FString LevelPath;        // /Game/Maps/L_xxx
	FString NameZh;
	FString NameEn;
	FString Region;
	FString Tagline;
	FString ThumbnailPath;    // /Game/UI/Thumbs/T_Map_xxx.T_Map_xxx
	float LengthKm = 0.0f;
	int32 Difficulty = 1;     // 1..5
	FLinearColor Accent = FLinearColor::White;
	bool bFreeRoam = false;            // 训练场：没有逆行，复位是原地扶正
	TArray<FApexWeatherInfo> Weathers; // 固定 3 个
};

class APEXDRIFT_API FApexMapRegistry
{
public:
	static const TArray<FApexMapInfo>& GetMaps();
	static int32 NumMaps() { return GetMaps().Num(); }
	static int32 ClampIndex(int32 Index);
	/** 根据当前世界的关卡名找到地图序号，找不到返回 0 */
	static int32 FindIndexForWorld(const UWorld* World);
};

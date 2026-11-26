#pragma once

#include "CoreMinimal.h"

/** 车漆预设：UI 色块与车辆材质共用一张表 */
struct FApexPaintInfo
{
	FString NameZh;
	FLinearColor Base;        // 漆面底色（线性）
	FLinearColor Flop;        // 掠射角变色
	float Metallic = 0.4f;
	float FlakeStrength = 0.5f;
	float Roughness = 0.32f;
	FColor Swatch = FColor::White; // UI 色块（sRGB）
};

class APEXDRIFT_API FApexPaintRegistry
{
public:
	static const TArray<FApexPaintInfo>& GetPaints();
	static int32 Num() { return GetPaints().Num(); }
	static int32 Wrap(int32 Index)
	{
		const int32 N = Num();
		return ((Index % N) + N) % N;
	}
};

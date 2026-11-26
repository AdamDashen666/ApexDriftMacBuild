#include "Core/ApexPaintRegistry.h"

const TArray<FApexPaintInfo>& FApexPaintRegistry::GetPaints()
{
	static const TArray<FApexPaintInfo> Paints =
	{
		{ TEXT("富士蓝"), FLinearColor(0.014f, 0.085f, 0.380f), FLinearColor(0.030f, 0.300f, 0.780f), 0.45f, 0.70f, 0.30f, FColor(34, 92, 190) },
		{ TEXT("樱霞粉"), FLinearColor(0.620f, 0.180f, 0.270f), FLinearColor(0.900f, 0.420f, 0.520f), 0.30f, 0.60f, 0.30f, FColor(232, 140, 168) },
		{ TEXT("朱红"),   FLinearColor(0.520f, 0.020f, 0.012f), FLinearColor(0.860f, 0.090f, 0.030f), 0.18f, 0.30f, 0.32f, FColor(206, 44, 34) },
		{ TEXT("雪顶白"), FLinearColor(0.780f, 0.785f, 0.790f), FLinearColor(0.880f, 0.890f, 0.910f), 0.05f, 0.35f, 0.34f, FColor(238, 240, 242) },
		{ TEXT("液态银"), FLinearColor(0.480f, 0.500f, 0.540f), FLinearColor(0.760f, 0.790f, 0.830f), 0.82f, 0.80f, 0.28f, FColor(170, 176, 186) },
		{ TEXT("墨玉绿"), FLinearColor(0.008f, 0.080f, 0.052f), FLinearColor(0.030f, 0.260f, 0.170f), 0.42f, 0.60f, 0.30f, FColor(22, 92, 70) },
		{ TEXT("暮光金"), FLinearColor(0.560f, 0.230f, 0.035f), FLinearColor(0.920f, 0.560f, 0.140f), 0.60f, 0.75f, 0.29f, FColor(214, 146, 52) },
		{ TEXT("夜幕黑"), FLinearColor(0.008f, 0.008f, 0.010f), FLinearColor(0.040f, 0.045f, 0.060f), 0.35f, 0.65f, 0.28f, FColor(24, 25, 30) },
	};
	return Paints;
}

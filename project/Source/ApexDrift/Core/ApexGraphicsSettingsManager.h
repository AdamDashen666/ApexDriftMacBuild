#pragma once

#include "CoreMinimal.h"
#include "Core/ApexDriftTypes.h"

class UApexDriftSaveGame;

class APEXDRIFT_API FApexGraphicsSettingsManager
{
public:
	static void ApplyAllSettings(UWorld* World, const UApexDriftSaveGame* SaveData, bool bApplyVideoMode = true);
	static void ApplyGraphicsTier(UWorld* World, EApexGraphicsTier Tier);
	static void ApplyResolutionScaling(UWorld* World, EApexTSRMode Mode, int32 FrameTargetIndex);
	/** 0 = TSR，1 = NVIDIA DLSS（插件 ApexDLSS，只在 Windows）；DLSS 不可用时自动用 TSR */
	static void ApplyUpscaler(int32 UpscalerIndex, EApexTSRMode Mode);
	static bool IsDLSSAvailable();
	static void ApplyMotionBlur(bool bEnabled);
	static void ApplyVideoMode(int32 ResolutionIndex, int32 WindowModeIndex, bool bVSync);

	static FIntPoint GetResolutionFromIndex(int32 Index);
	static FString GetTierLabel(EApexGraphicsTier Tier);
	static FString GetTierDescription(EApexGraphicsTier Tier);
	static FString GetTSRLabel(EApexTSRMode Mode);
	/** 0=120 1=144 2=165 3=240 */
	static int32 GetFrameTargetFromIndex(int32 Index);
};

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
	static void ApplyMotionBlur(bool bEnabled);
	static void ApplyVideoMode(int32 ResolutionIndex, int32 WindowModeIndex, bool bVSync);

	static FIntPoint GetResolutionFromIndex(int32 Index);
	static FString GetTierLabel(EApexGraphicsTier Tier);
	static FString GetTierDescription(EApexGraphicsTier Tier);
	static FString GetTSRLabel(EApexTSRMode Mode);
	/** 0=120 1=144 2=165 3=240 */
	static int32 GetFrameTargetFromIndex(int32 Index);
};

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "Core/ApexDriftTypes.h"
#include "ApexGameInstance.generated.h"

/** 跨关卡保存“加载完就开赛”的请求，并负责加载画面 */
UCLASS()
class APEXDRIFT_API UApexGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	virtual void Init() override;
	virtual void Shutdown() override;

	bool bPendingAutoStart = false;
	bool bPendingAutoRun = false;      // 加载完后自动漂移一圈
	EApexGameModeType PendingMode = EApexGameModeType::FreeDrift;

	void TravelToMap(int32 MapIndex);

private:
	void HandlePreLoadMap(const FString& MapName);
	int32 LoadingMapIndex = 0;
};

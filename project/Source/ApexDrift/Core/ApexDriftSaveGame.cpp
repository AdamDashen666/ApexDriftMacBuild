#include "Core/ApexDriftSaveGame.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

const FString UApexDriftSaveGame::DefaultSlotName = TEXT("ApexDriftProfile_v2");

UApexDriftSaveGame* UApexDriftSaveGame::LoadOrCreate()
{
	if (UGameplayStatics::DoesSaveGameExist(DefaultSlotName, 0))
	{
		if (UApexDriftSaveGame* Loaded = Cast<UApexDriftSaveGame>(UGameplayStatics::LoadGameFromSlot(DefaultSlotName, 0)))
		{
			return Loaded;
		}
	}
	UApexDriftSaveGame* Fresh = Cast<UApexDriftSaveGame>(UGameplayStatics::CreateSaveGameObject(UApexDriftSaveGame::StaticClass()));
	if (Fresh)
	{
		Fresh->SaveToDisk();
	}
	return Fresh;
}

void UApexDriftSaveGame::SaveToDisk()
{
	// 跑分（-ApexBench）是自动驾驶的测试，不能把成绩、上次地图等写进玩家存档
	static const bool bBench = FParse::Param(FCommandLine::Get(), TEXT("ApexBench"));
	if (bBench)
	{
		return;
	}
	UGameplayStatics::SaveGameToSlot(this, DefaultSlotName, 0);
}

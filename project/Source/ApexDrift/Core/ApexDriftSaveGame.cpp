#include "Core/ApexDriftSaveGame.h"
#include "Kismet/GameplayStatics.h"

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
	UGameplayStatics::SaveGameToSlot(this, DefaultSlotName, 0);
}

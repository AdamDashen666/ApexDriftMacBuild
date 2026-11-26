#include "Core/ApexGameInstance.h"
#include "Core/ApexMapRegistry.h"
#include "UI/ApexLoadingScreen.h"
#include "Kismet/GameplayStatics.h"
#include "MoviePlayer.h"
#include "UObject/UObjectGlobals.h"

void UApexGameInstance::Init()
{
	Super::Init();
	FCoreUObjectDelegates::PreLoadMap.AddUObject(this, &UApexGameInstance::HandlePreLoadMap);
}

void UApexGameInstance::Shutdown()
{
	FCoreUObjectDelegates::PreLoadMap.RemoveAll(this);
	Super::Shutdown();
}

void UApexGameInstance::TravelToMap(int32 MapIndex)
{
	LoadingMapIndex = FApexMapRegistry::ClampIndex(MapIndex);
	const FApexMapInfo& Info = FApexMapRegistry::GetMaps()[LoadingMapIndex];
	UGameplayStatics::OpenLevel(this, FName(*Info.LevelPath));
}

void UApexGameInstance::HandlePreLoadMap(const FString& MapName)
{
	const TArray<FApexMapInfo>& Maps = FApexMapRegistry::GetMaps();
	for (int32 i = 0; i < Maps.Num(); ++i)
	{
		if (MapName.Contains(FPackageName::GetShortName(Maps[i].LevelPath)))
		{
			LoadingMapIndex = i;
			break;
		}
	}
	if (IsRunningDedicatedServer() || IsRunningCommandlet() || !IsMoviePlayerEnabled())
	{
		return;
	}
	FLoadingScreenAttributes Attr;
	Attr.bAutoCompleteWhenLoadingCompletes = true;
	Attr.bMoviesAreSkippable = false;
	Attr.bAllowEngineTick = false;
	Attr.MinimumLoadingScreenDisplayTime = 1.0f;
	Attr.WidgetLoadingScreen = MakeApexLoadingScreen(LoadingMapIndex);
	GetMoviePlayer()->SetupLoadingScreen(Attr);
}

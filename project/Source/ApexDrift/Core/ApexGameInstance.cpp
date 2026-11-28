#include "Core/ApexGameInstance.h"
#include "Core/ApexMapRegistry.h"
#include "Core/ApexNeuralRenderingSettings.h"
#include "Containers/Ticker.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UI/ApexLoadingScreen.h"
#include "Kismet/GameplayStatics.h"
#include "MoviePlayer.h"
#include "UObject/UObjectGlobals.h"

void UApexGameInstance::Init()
{
	Super::Init();
	FCoreUObjectDelegates::PreLoadMap.AddUObject(this, &UApexGameInstance::HandlePreLoadMap);
#if PLATFORM_WINDOWS
	// DLSS 5：启动时就记下 OptiScaler 本次读到的配置（之后玩家改 INI 不会混进“当前”）
	FString NrSample;
	if (FParse::Value(FCommandLine::Get(), TEXT("ApexNrSelfTest="), NrSample))
	{
		TArray<FString> Report;
		ApexNeuralRendering::RunSelfTest(NrSample, Report);
		IFileManager::Get().MakeDirectory(*(FPaths::ProjectSavedDir() / TEXT("Bench")), true);
		FFileHelper::SaveStringArrayToFile(Report, *(FPaths::ProjectSavedDir() / TEXT("Bench/nr_selftest.txt")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FPlatformMisc::RequestExit(false, TEXT("ApexNrSelfTest"));
		return;
	}
	ApexNeuralRendering::Initialize();
	if (FParse::Param(FCommandLine::Get(), TEXT("ApexNrTestRestart")))
	{
		// 测“立即重启生效”：第一次启动 5 秒后自动重启；重启出来的进程看到标记就写结果并退出
		const FString Bench = FPaths::ProjectSavedDir() / TEXT("Bench");
		const FString Marker = Bench / TEXT("nr_restart_marker.txt");
		IFileManager::Get().MakeDirectory(*Bench, true);
		if (IFileManager::Get().FileExists(*Marker))
		{
			IFileManager::Get().Delete(*Marker);
			FFileHelper::SaveStringToFile(TEXT("重启后的新进程起来了"), *(Bench / TEXT("nr_restart_ok.txt")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
			FPlatformMisc::RequestExit(false, TEXT("ApexNrTestRestart"));
			return;
		}
		FFileHelper::SaveStringToFile(TEXT("1"), *Marker);
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([](float) { ApexNeuralRendering::RestartGame(); return false; }), 5.0f);
	}
#endif
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

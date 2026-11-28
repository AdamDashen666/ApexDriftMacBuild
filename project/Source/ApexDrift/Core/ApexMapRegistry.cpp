#include "Core/ApexMapRegistry.h"
#include "Engine/World.h"
#include "Misc/PackageName.h"

const TArray<FApexMapInfo>& FApexMapRegistry::GetMaps()
{
	static const TArray<FApexMapInfo> Maps = []()
	{
		TArray<FApexMapInfo> Out;

		FApexMapInfo& Fuji = Out.AddDefaulted_GetRef();
		Fuji.Id = TEXT("FujiSakura");
		Fuji.LevelPath = TEXT("/Game/Maps/L_FujiSakura");
		Fuji.NameZh = TEXT("富士樱湖");
		Fuji.NameEn = TEXT("FUJI SAKURA LAKE");
		Fuji.Region = TEXT("日本 · 富士五湖");
		Fuji.Tagline = TEXT("满开樱花大道、朱红鸟居与湖面倒映的富士山");
		Fuji.ThumbnailPath = TEXT("/Game/UI/Thumbs/T_Map_FujiSakura.T_Map_FujiSakura");
		Fuji.LengthKm = 4.1f;
		Fuji.Difficulty = 2;
		Fuji.Accent = FLinearColor(FColor(255, 143, 177));
		Fuji.Weathers = {
			{ TEXT("春日晴空"), TEXT("能见度极佳，花瓣随风飘落"), TEXT("Sun") },
			{ TEXT("金色黄昏"), TEXT("低斜暖阳，富士山染成粉金"), TEXT("Sunset") },
			{ TEXT("雨后初霁"), TEXT("路面水光倒影，抓地不受影响"), TEXT("Rain") },
		};

		FApexMapInfo& Momiji = Out.AddDefaulted_GetRef();
		Momiji.Id = TEXT("MomijiTouge");
		Momiji.LevelPath = TEXT("/Game/Maps/L_MomijiTouge");
		Momiji.NameZh = TEXT("红枫峠");
		Momiji.NameEn = TEXT("MOMIJI TOUGE");
		Momiji.Region = TEXT("日本 · 秋之山道");
		Momiji.Tagline = TEXT("层林尽染的连续发夹弯，山谷晨雾与红叶隧道");
		Momiji.ThumbnailPath = TEXT("/Game/UI/Thumbs/T_Map_MomijiTouge.T_Map_MomijiTouge");
		Momiji.LengthKm = 4.3f;
		Momiji.Difficulty = 4;
		Momiji.Accent = FLinearColor(FColor(255, 122, 46));
		Momiji.Weathers = {
			{ TEXT("秋日暖阳"), TEXT("红叶透光，空气通透"), TEXT("Sun") },
			{ TEXT("山谷晨雾"), TEXT("薄雾与斜射晨光"), TEXT("Mist") },
			{ TEXT("夕照红叶"), TEXT("落日把山林点燃"), TEXT("Sunset") },
		};

		FApexMapInfo& Kuro = Out.AddDefaulted_GetRef();
		Kuro.Id = TEXT("KuroshioCircuit");
		Kuro.LevelPath = TEXT("/Game/Maps/L_KuroshioCircuit");
		Kuro.NameZh = TEXT("黑潮港湾");
		Kuro.NameEn = TEXT("KUROSHIO HARBOR");
		Kuro.Region = TEXT("日本 · 太平洋海岸");
		Kuro.Tagline = TEXT("海岸环道、码头集装箱与高架连弯");
		Kuro.ThumbnailPath = TEXT("/Game/UI/Thumbs/T_Map_KuroshioCircuit.T_Map_KuroshioCircuit");
		Kuro.LengthKm = 2.9f;
		Kuro.Difficulty = 3;
		Kuro.Accent = FLinearColor(FColor(64, 190, 255));
		Kuro.Weathers = {
			{ TEXT("黄金日落"), TEXT("海面与路面的金色反光"), TEXT("Sunset") },
			{ TEXT("午夜霓虹"), TEXT("雨夜灯火，抓地不受影响"), TEXT("Moon") },
			{ TEXT("海岸晴日"), TEXT("蓝天白云与干燥路面"), TEXT("Sun") },
		};

		FApexMapInfo& Train = Out.AddDefaulted_GetRef();
		Train.Id = TEXT("TrainingPark");
		Train.LevelPath = TEXT("/Game/Maps/L_TrainingPark");
		Train.NameZh = TEXT("湖畔游乐场");
		Train.NameEn = TEXT("LAKESIDE PLAYGROUND");
		Train.Region = TEXT("日本 · 富士湖畔");
		Train.Tagline = TEXT("六平方公里游乐场：倾角椭圆、巨型跳台、翻滚环、墙骑弯、越野区和上千个能撞飞的道具");
		Train.ThumbnailPath = TEXT("/Game/UI/Thumbs/T_Map_TrainingPark.T_Map_TrainingPark");
		Train.LengthKm = 3.5f;
		Train.Difficulty = 1;
		Train.bFreeRoam = true;
		Train.Accent = FLinearColor(FColor(120, 224, 90));
		Train.Weathers = {
			{ TEXT("晴空万里"), TEXT("视野开阔，能看到富士山"), TEXT("Sun") },
			{ TEXT("湖畔日落"), TEXT("低斜暖阳，场地拉出长影"), TEXT("Sunset") },
			{ TEXT("夜间灯光"), TEXT("灯塔照亮全场"), TEXT("Moon") },
		};

		return Out;
	}();
	return Maps;
}

int32 FApexMapRegistry::ClampIndex(int32 Index)
{
	return FMath::Clamp(Index, 0, GetMaps().Num() - 1);
}

int32 FApexMapRegistry::FindIndexForWorld(const UWorld* World)
{
	if (!World)
	{
		return 0;
	}
	FString MapName = World->GetMapName();
	MapName.RemoveFromStart(World->StreamingLevelsPrefix);
	const TArray<FApexMapInfo>& Maps = GetMaps();
	for (int32 i = 0; i < Maps.Num(); ++i)
	{
		if (FPackageName::GetShortName(Maps[i].LevelPath).Equals(MapName, ESearchCase::IgnoreCase))
		{
			return i;
		}
	}
	return 0;
}

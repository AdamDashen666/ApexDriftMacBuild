#include "Vehicle/ApexCarRegistry.h"

namespace
{
	// 由 Tools/gen_car_registry.py 从 RawAssets/Models/**/manifest.json 生成
	FApexCarGeometry MakeGeometry(const FName Id)
	{
		FApexCarGeometry G;
#include "Vehicle/ApexCarGeometry.inl"
		return G;
	}

	FApexCarInfo Make(const TCHAR* Id, const TCHAR* Name, const TCHAR* Class, const TCHAR* Tag,
		float Accel, float Top, float Grip, float Agility, float Drift, float Pitch, float Volume, bool bTurbo, bool bFlame, int32 Paint)
	{
		FApexCarInfo C;
		C.Id = FName(Id);
		C.Name = Name;
		C.ClassZh = Class;
		C.Tagline = Tag;
		C.Accel = Accel;
		C.TopKmh = Top;
		C.Grip = Grip;
		C.Agility = Agility;
		C.Drift = Drift;
		C.SoundPitch = Pitch;
		C.SoundVolume = Volume;
		C.bTurbo = bTurbo;
		C.bExhaustFlame = bFlame;
		C.DefaultPaint = Paint;
		C.Geo = MakeGeometry(C.Id);
		// 性能条由实际参数换算，不另填一套数字
		C.Stats[0] = FMath::Clamp((Top - 250.0f) / 135.0f, 0.1f, 1.0f);
		C.Stats[1] = FMath::Clamp((Accel - 0.70f) / 0.50f, 0.1f, 1.0f);
		C.Stats[2] = FMath::Clamp((Grip - 0.65f) / 0.48f, 0.1f, 1.0f);
		C.Stats[3] = FMath::Clamp((Drift - 0.68f) / 0.54f, 0.1f, 1.0f);
		return C;
	}
}

const TArray<FApexCarInfo>& FApexCarRegistry::GetCars()
{
	//          Id                  名称                 类别                   一句话                                     加速   极速   抓地   响应   漂移  音高  音量  涡轮  喷火  默认漆
	static const TArray<FApexCarInfo> Cars =
	{
		Make(TEXT("HeroCar"),  TEXT("APEX GT"),  TEXT("GT 赛道车"),   TEXT("碳纤宽体与大尾翼，各项均衡"),         1.00f, 368.f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, true,  true, 0),
		// 2026-11-28 换车：旧的 6 辆用户嫌丑，全部换掉；拉力车是用户点名要的真实车型改的，游戏里用虚构名字、去掉全部厂标
		Make(TEXT("Goblin"),   TEXT("GOBLIN"),   TEXT("V12 中置超跑"), TEXT("自然吸气 V12，高速稳、出弯有劲"),     1.08f, 370.f, 1.06f, 1.02f, 0.96f, 1.12f, 1.05f, false, true, 5),
		Make(TEXT("Agunta"),   TEXT("AGUNTA"),   TEXT("赛道超跑"),     TEXT("大尾翼下压力最强，过弯抓地最好"),     1.10f, 375.f, 1.10f, 1.04f, 0.92f, 1.08f, 1.00f, true,  true, 3),
		Make(TEXT("Saetta"),   TEXT("SAETTA"),   TEXT("楔形拉力车"),   TEXT("轴距短、车头灵，甩尾最活"),           0.98f, 300.f, 0.94f, 1.14f, 1.18f, 0.95f, 1.05f, false, true, 2),
		Make(TEXT("Falco"),    TEXT("FALCO"),    TEXT("四驱拉力掀背"), TEXT("四驱抓地稳，越野区最好开"),           1.04f, 290.f, 1.02f, 1.06f, 1.04f, 1.02f, 1.00f, true,  true, 3),
		Make(TEXT("Boreas"),   TEXT("BOREAS"),   TEXT("B 组拉力怪兽"), TEXT("五缸涡轮爆发力强，起步最猛"),         1.12f, 305.f, 0.98f, 1.02f, 1.10f, 0.90f, 1.15f, true,  true, 6),
		Make(TEXT("Kestrel"),  TEXT("KESTREL"),  TEXT("宽体 B 组赛车"), TEXT("超宽车身、后驱大马力，横着走最带劲"), 1.06f, 320.f, 0.94f, 0.96f, 1.18f, 0.85f, 1.12f, true,  true, 0),
	};
	return Cars;
}

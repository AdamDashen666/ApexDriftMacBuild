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
		Make(TEXT("Onyx"),     TEXT("ONYX"),     TEXT("宽体轿跑"),     TEXT("车身短、反应快，最好上手的漂移车"),   0.97f, 350.f, 0.98f, 1.12f, 1.10f, 1.04f, 1.00f, true,  true, 5),
		Make(TEXT("Ironclad"), TEXT("IRONCLAD"), TEXT("V8 肌肉车"),    TEXT("大马力后驱，角度大、尾巴活"),         1.02f, 335.f, 0.90f, 0.94f, 1.16f, 0.82f, 1.10f, false, true, 6),
		Make(TEXT("Vela"),     TEXT("VELA"),     TEXT("电动 GT"),      TEXT("起步最猛，安静顺滑"),                 1.14f, 340.f, 1.02f, 1.02f, 0.96f, 1.55f, 0.50f, false, false, 3),
		Make(TEXT("Ember"),    TEXT("EMBER"),    TEXT("原型超跑"),     TEXT("长鼻低趴，高速抓地最稳"),             1.07f, 372.f, 1.10f, 0.98f, 0.90f, 1.10f, 1.00f, true,  true, 2),
		Make(TEXT("Selva"),    TEXT("SELVA"),    TEXT("顶级超跑"),     TEXT("极速最高，适合跑圈刷时间"),           1.12f, 375.f, 1.06f, 1.06f, 0.94f, 1.16f, 1.00f, true,  true, 4),
		Make(TEXT("Rustbelt"), TEXT("RUSTBELT"), TEXT("热棒皮卡"),     TEXT("外露 V8、巨型后轮，横着走最带劲"),    1.00f, 300.f, 0.84f, 0.96f, 1.20f, 0.74f, 1.15f, false, true, 7),
	};
	return Cars;
}

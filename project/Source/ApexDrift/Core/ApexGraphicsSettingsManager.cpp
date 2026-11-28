#include "Core/ApexGraphicsSettingsManager.h"
#include "Core/ApexDriftSaveGame.h"
#include "GameFramework/GameUserSettings.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMemory.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Engine/Engine.h"

namespace
{
	void SetCVarInt(const TCHAR* Name, int32 Value)
	{
		if (IConsoleVariable* Var = IConsoleManager::Get().FindConsoleVariable(Name))
		{
			if (Var->GetInt() != Value)
			{
				Var->Set(Value, ECVF_SetByGameOverride);
			}
		}
	}

	void SetCVarFloat(const TCHAR* Name, float Value)
	{
		if (IConsoleVariable* Var = IConsoleManager::Get().FindConsoleVariable(Name))
		{
			Var->Set(Value, ECVF_SetByGameOverride);
		}
	}
}

FIntPoint FApexGraphicsSettingsManager::GetResolutionFromIndex(int32 Index)
{
#if PLATFORM_MAC
	// Mac 屏幕多是 16:10；无边框全屏直接用桌面分辨率，这几档只在窗口 / 独占全屏模式下用
	switch (Index)
	{
	case 0: return FIntPoint(1280, 800);
	case 1: return FIntPoint(1440, 900);
	case 3: return FIntPoint(2560, 1600);
	case 2:
	default:
		return FIntPoint(1920, 1200);
	}
#else
	switch (Index)
	{
	case 0: return FIntPoint(1280, 720);
	case 1: return FIntPoint(1920, 1080);
	case 3: return FIntPoint(3840, 2160);
	case 2:
	default:
		return FIntPoint(2560, 1440);
	}
#endif
}

int32 FApexGraphicsSettingsManager::GetFrameTargetFromIndex(int32 Index)
{
#if PLATFORM_MAC
	// Mac 屏幕最高 120Hz（MacBook Air 等是 60Hz）。动态分辨率按这个帧率压分辨率，
	// 目标高于屏幕刷新率只会把画面压糊，所以 Mac 用 30/60/90/120，默认档（索引 1）是 60。
	switch (Index)
	{
	case 0: return 30;
	case 2: return 90;
	case 3: return 120;
	case 1:
	default:
		return 60;
	}
#else
	switch (Index)
	{
	case 0: return 120;
	case 2: return 165;
	case 3: return 240;
	case 1:
	default:
		return 144;
	}
#endif
}

FString FApexGraphicsSettingsManager::GetTierLabel(EApexGraphicsTier Tier)
{
	switch (Tier)
	{
	case EApexGraphicsTier::Performance: return TEXT("极速");
	case EApexGraphicsTier::Balanced:    return TEXT("均衡（推荐）");
	case EApexGraphicsTier::Quality:     return TEXT("高画质");
	case EApexGraphicsTier::Cinematic:   return TEXT("光追电影级");
	default:                             return TEXT("均衡（推荐）");
	}
}

FString FApexGraphicsSettingsManager::GetTierDescription(EApexGraphicsTier Tier)
{
	switch (Tier)
	{
#if PLATFORM_MAC
	case EApexGraphicsTier::Performance: return TEXT("关闭全局光照追踪，保留高质量贴图与阴影。M1、M2 等基础款芯片推荐。");
	case EApexGraphicsTier::Cinematic:   return TEXT("开启光线追踪（M3 及以后的芯片有硬件加速，更早的芯片会很慢），用于截图与欣赏。");
#else
	case EApexGraphicsTier::Performance: return TEXT("关闭全局光照追踪，保留高质量贴图与阴影。适合 165–240Hz 竞速。");
	case EApexGraphicsTier::Cinematic:   return TEXT("硬件光线追踪全开，用于截图与欣赏，帧率会明显降低。");
#endif
	case EApexGraphicsTier::Balanced:    return TEXT("轻量全局光照 + 屏幕空间与 Lumen 混合反射，画面与流畅度的最佳平衡。");
	case EApexGraphicsTier::Quality:     return TEXT("高精度全局光照、完整 Lumen 反射和更远的植被细节。");
	default:                             return FString();
	}
}

FString FApexGraphicsSettingsManager::GetTSRLabel(EApexTSRMode Mode)
{
	switch (Mode)
	{
	case EApexTSRMode::AutoDynamic:    return TEXT("动态分辨率（自动保帧）");
	case EApexTSRMode::Native100:      return TEXT("原生 100%");
	case EApexTSRMode::TSRQuality:     return TEXT("质量 67%");
	case EApexTSRMode::TSRBalanced:    return TEXT("平衡 58%");
	case EApexTSRMode::TSRPerformance: return TEXT("性能 50%");
	default:                           return TEXT("动态分辨率（自动保帧）");
	}
}

void FApexGraphicsSettingsManager::ApplyAllSettings(UWorld* World, const UApexDriftSaveGame* SaveData, bool bApplyVideoMode)
{
	if (!SaveData)
	{
		return;
	}
	// 跑分时 -ApexTier=0..3（极速 / 均衡 / 高画质 / 光追电影级）临时换档，不改存档
	EApexGraphicsTier Tier = SaveData->GraphicsTier;
	int32 TierArg = -1;
	if (FParse::Value(FCommandLine::Get(), TEXT("ApexTier="), TierArg) && TierArg >= 0 && TierArg <= static_cast<int32>(EApexGraphicsTier::Cinematic))
	{
		Tier = static_cast<EApexGraphicsTier>(TierArg);
	}
	ApplyGraphicsTier(World, Tier);
	// -ApexRes=0..4（动态 / 原生 / 质量 / 平衡 / 性能）临时换分辨率缩放，不改存档
	EApexTSRMode ResMode = SaveData->TSRMode;
	int32 ResArg = -1;
	if (FParse::Value(FCommandLine::Get(), TEXT("ApexRes="), ResArg) && ResArg >= 0 && ResArg <= static_cast<int32>(EApexTSRMode::TSRPerformance))
	{
		ResMode = static_cast<EApexTSRMode>(ResArg);
	}
	ApplyResolutionScaling(World, ResMode, SaveData->FrameTargetIndex);
	ApplyUpscaler(SaveData->UpscalerIndex, ResMode);
	ApplyMotionBlur(SaveData->bMotionBlur);
	if (bApplyVideoMode)
	{
		ApplyVideoMode(SaveData->ResolutionIndex, SaveData->WindowModeIndex, SaveData->bVSyncEnabled);
	}
}

void FApexGraphicsSettingsManager::ApplyGraphicsTier(UWorld* World, EApexGraphicsTier Tier)
{
	// 各项独立给档：贴图始终高，阴影/GI/反射按档位走
	int32 Base = 2, Shadow = 2, GI = 1, Refl = 2, Post = 2, Foliage = 2;
	switch (Tier)
	{
	case EApexGraphicsTier::Performance: Base = 1; Shadow = 1; GI = 0; Refl = 0; Post = 1; Foliage = 1; break;
	case EApexGraphicsTier::Balanced:    Base = 2; Shadow = 2; GI = 1; Refl = 1; Post = 2; Foliage = 2; break;
	case EApexGraphicsTier::Quality:     Base = 2; Shadow = 2; GI = 2; Refl = 2; Post = 2; Foliage = 3; break;
	case EApexGraphicsTier::Cinematic:   Base = 3; Shadow = 3; GI = 3; Refl = 3; Post = 3; Foliage = 3; break;
	}

	if (UGameUserSettings* GUS = GEngine ? GEngine->GetGameUserSettings() : nullptr)
	{
		GUS->SetOverallScalabilityLevel(Base);
		GUS->SetViewDistanceQuality(FMath::Max(2, Base));
		GUS->SetAntiAliasingQuality(FMath::Max(2, Base));
		GUS->SetShadowQuality(Shadow);
		GUS->SetGlobalIlluminationQuality(GI);
		GUS->SetReflectionQuality(Refl);
		GUS->SetPostProcessingQuality(Post);
		GUS->SetTextureQuality(3);
		GUS->SetVisualEffectQuality(FMath::Max(1, Base));
		GUS->SetFoliageQuality(Foliage);
		GUS->ApplyNonResolutionSettings();
	}

	SetCVarInt(TEXT("r.AntiAliasingMethod"), 4); // TSR
	SetCVarInt(TEXT("r.Shadow.Virtual.Cache"), 1);

	const bool bCinematic = (Tier == EApexGraphicsTier::Cinematic);
	const bool bQuality = (Tier == EApexGraphicsTier::Quality);
	const bool bPerf = (Tier == EApexGraphicsTier::Performance);

	// 只有电影级才构建光追场景；其余档位走软件 Lumen（距离场 + 屏幕追踪），省掉每帧 BLAS/TLAS
	SetCVarInt(TEXT("r.RayTracing.Enable"), bCinematic ? 1 : 0);
	SetCVarInt(TEXT("r.Lumen.HardwareRayTracing"), bCinematic ? 1 : 0);
	SetCVarInt(TEXT("r.Lumen.HardwareRayTracing.LightingMode"), bCinematic ? 2 : 0);
	SetCVarInt(TEXT("r.Lumen.HardwareRayTracing.HitLighting.Allowed"), bCinematic ? 1 : 0);

	SetCVarInt(TEXT("r.Lumen.DiffuseIndirect.Allow"), bPerf ? 0 : 1);
	SetCVarInt(TEXT("r.DistanceFieldAO"), bPerf ? 0 : 1);

	// 反射：均衡档只对光滑表面做追踪，粗糙面复用 GI/天光
	SetCVarInt(TEXT("r.Lumen.Reflections.Allow"), bPerf ? 0 : 1);
	SetCVarInt(TEXT("r.Lumen.Reflections.DownsampleFactor"), bCinematic ? 1 : 2);
	SetCVarFloat(TEXT("r.Lumen.Reflections.MaxRoughnessToTrace"), bCinematic ? 0.55f : (bQuality ? 0.35f : 0.22f));
	SetCVarInt(TEXT("r.Lumen.TranslucencyReflections.FrontLayer.Enable"), bCinematic ? 1 : 0);
	SetCVarInt(TEXT("r.SSR.Quality"), bPerf ? 2 : 3);

	// 阴影：虚拟阴影贴图的光线数量
	SetCVarInt(TEXT("r.Shadow.Virtual.SMRT.RayCountDirectional"), bCinematic ? 8 : (bQuality ? 4 : 2));
	SetCVarInt(TEXT("r.Shadow.Virtual.SMRT.SamplesPerRayDirectional"), bCinematic ? 4 : 2);
	SetCVarFloat(TEXT("r.Shadow.Virtual.ResolutionLodBiasDirectional"), bCinematic ? -1.0f : (bQuality ? -0.5f : (bPerf ? 1.0f : 0.0f)));
	SetCVarFloat(TEXT("r.Shadow.Virtual.ResolutionLodBiasDirectionalMoving"), bCinematic ? -0.5f : (bQuality ? 0.0f : 1.0f));

	// 体积雾只在高画质以上开启
	SetCVarInt(TEXT("r.VolumetricFog"), (bCinematic || bQuality) ? 1 : 0);
	SetCVarInt(TEXT("r.VolumetricFog.GridPixelSize"), bCinematic ? 8 : 16);
	SetCVarInt(TEXT("r.VolumetricFog.GridSizeZ"), bCinematic ? 128 : 64);

	// Nanite 像素误差：均衡 1.2px，极速 1.8px
	SetCVarFloat(TEXT("r.Nanite.MaxPixelsPerEdge"), bCinematic ? 1.0f : (bQuality ? 1.0f : (bPerf ? 1.8f : 1.25f)));
	SetCVarFloat(TEXT("r.ViewDistanceScale"), bCinematic ? 1.5f : (bPerf ? 0.8f : 1.0f));

	// TSR：历史分辨率 100%，避免 200% 历史的额外开销
	SetCVarFloat(TEXT("r.TSR.History.ScreenPercentage"), bCinematic ? 200.0f : 100.0f);
	SetCVarInt(TEXT("r.TSR.ShadingRejection.Flickering"), bPerf ? 0 : 1);

	SetCVarInt(TEXT("r.ContactShadows"), bPerf ? 0 : 1);
#if PLATFORM_MAC
	// Mac 是统一内存，系统和显存共用：8GB 的机器再给 5GB 贴图池会严重换页
	const uint32 RamGB = FPlatformMemory::GetConstants().TotalPhysicalGB;
	SetCVarInt(TEXT("r.Streaming.PoolSize"), RamGB <= 8 ? 1800 : (RamGB <= 16 ? 3500 : 5000));
#else
	SetCVarInt(TEXT("r.Streaming.PoolSize"), 5000);
#endif
}

void FApexGraphicsSettingsManager::ApplyResolutionScaling(UWorld* World, EApexTSRMode Mode, int32 FrameTargetIndex)
{
	SetCVarInt(TEXT("r.AntiAliasingMethod"), 4);

	if (Mode == EApexTSRMode::AutoDynamic)
	{
		const float Target = static_cast<float>(GetFrameTargetFromIndex(FrameTargetIndex));
		SetCVarInt(TEXT("r.DynamicRes.OperationMode"), 2); // 始终启用
		SetCVarFloat(TEXT("r.DynamicRes.FrameTimeBudget"), 1000.0f / Target);
		SetCVarFloat(TEXT("r.DynamicRes.TargetedGPUHeadRoomPercentage"), 6.0f);
		SetCVarFloat(TEXT("r.DynamicRes.MinScreenPercentage"), 50.0f);
		SetCVarFloat(TEXT("r.DynamicRes.MaxScreenPercentage"), 100.0f);
		SetCVarInt(TEXT("r.DynamicRes.MinResolutionChangePeriod"), 12);
		SetCVarFloat(TEXT("r.ScreenPercentage"), 75.0f);
		return;
	}

	float Pct = 67.0f;
	switch (Mode)
	{
	case EApexTSRMode::Native100:      Pct = 100.0f; break;
	case EApexTSRMode::TSRQuality:     Pct = 67.0f;  break;
	case EApexTSRMode::TSRBalanced:    Pct = 58.0f;  break;
	case EApexTSRMode::TSRPerformance: Pct = 50.0f;  break;
	default: break;
	}
	SetCVarInt(TEXT("r.DynamicRes.OperationMode"), 0);
	SetCVarFloat(TEXT("r.ScreenPercentage"), Pct);
}

bool FApexGraphicsSettingsManager::IsDLSSAvailable()
{
	// 插件 ApexDLSS 初始化完会把它设成 1；Mac 版、非 NVIDIA 显卡没有这个变量或是 0
	const IConsoleVariable* Var = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ApexDLSS.Available"));
	return Var && Var->GetInt() == 1;
}

void FApexGraphicsSettingsManager::ApplyUpscaler(int32 UpscalerIndex, EApexTSRMode Mode)
{
	// 跑分时 -ApexDLSS=0/1 临时换超分，不改存档
	int32 Arg = -1;
	if (FParse::Value(FCommandLine::Get(), TEXT("ApexDLSS="), Arg))
	{
		UpscalerIndex = Arg != 0 ? 1 : 0;
	}
	const bool bDLSS = UpscalerIndex == 1 && IsDLSSAvailable();

	// 渲染分辨率仍由屏幕百分比 / 动态分辨率决定；模式只决定 DLSS 用哪套模型。
	// 动态分辨率固定用质量模式，分辨率在 50%–100% 之间变时不用重建 DLSS。
	int32 DLSSMode = 2;
	switch (Mode)
	{
	case EApexTSRMode::Native100:      DLSSMode = 1; break; // DLAA
	case EApexTSRMode::TSRQuality:     DLSSMode = 2; break;
	case EApexTSRMode::TSRBalanced:    DLSSMode = 3; break;
	case EApexTSRMode::TSRPerformance: DLSSMode = 4; break;
	default: break;
	}
	SetCVarInt(TEXT("r.ApexDLSS.Mode"), DLSSMode);
	SetCVarInt(TEXT("r.ApexDLSS.Enable"), bDLSS ? 1 : 0);
}

void FApexGraphicsSettingsManager::ApplyMotionBlur(bool bEnabled)
{
	SetCVarInt(TEXT("r.MotionBlurQuality"), bEnabled ? 3 : 0);
}

void FApexGraphicsSettingsManager::ApplyVideoMode(int32 ResolutionIndex, int32 WindowModeIndex, bool bVSync)
{
	UGameUserSettings* GUS = GEngine ? GEngine->GetGameUserSettings() : nullptr;
	if (!GUS)
	{
		return;
	}
	// 无头命令行模式不改窗口
	if (IsRunningCommandlet())
	{
		return;
	}
	const FIntPoint Res = GetResolutionFromIndex(ResolutionIndex);
	EWindowMode::Type WinMode = EWindowMode::WindowedFullscreen;
	if (WindowModeIndex == 0) WinMode = EWindowMode::Fullscreen;
	else if (WindowModeIndex == 2) WinMode = EWindowMode::Windowed;

	GUS->SetScreenResolution(Res);
	GUS->SetFullscreenMode(WinMode);
	GUS->SetVSyncEnabled(bVSync);
	GUS->SetFrameRateLimit(0.0f);
	GUS->ApplyResolutionSettings(false);
	GUS->SaveSettings();
}

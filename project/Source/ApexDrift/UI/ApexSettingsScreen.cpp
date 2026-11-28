#include "UI/ApexSettingsScreen.h"
#include "UI/ApexMenuScreens.h"
#include "Core/ApexDriftPlayerController.h"
#include "Core/ApexDriftSaveGame.h"
#include "Core/ApexGraphicsSettingsManager.h"
#include "Core/ApexKeyBindings.h"
#include "Vehicle/ApexDriftVehiclePawn.h"
#include "Audio/ApexVehicleAudioComponent.h"
#if PLATFORM_WINDOWS
#include "Core/ApexNeuralRenderingSettings.h"
#endif

using namespace ApexUI;
using FGfx = FApexGraphicsSettingsManager;

namespace
{
	constexpr float ControlW = 392.0f;
	constexpr float KeyCapW = 150.0f;
	constexpr int32 KeysTab = 3;

#if PLATFORM_WINDOWS
	// 能不能调：DLSS 可用 + OptiScaler 真的加载了 + 超分选的是 DLSS（神经渲染挂在 DLSS 之前跑）。
	// 不按显卡名判断：OptiScaler 自带的 INT8 模型 RTX 30 起就能跑，只有 NVIDIA 的模型要 RTX 50（写在说明里）
	FString NeuralRenderingDisabledWhy(const UApexDriftSaveGame& Save)
	{
		using namespace ApexNeuralRendering;
		if (!FGfx::IsDLSSAvailable())
		{
			return TEXT("需要 NVIDIA RTX 显卡（DirectX 12）");
		}
		switch (GetSettings().Status)
		{
		case EStatus::Missing: return TEXT("没有装 DLSS 5 组件（OptiScaler-NR）");
		case EStatus::ReadFailed: return TEXT("读不到 OptiScaler.ini");
		default: break;
		}
		return Save.UpscalerIndex == 1 ? FString() : FString(TEXT("将超分技术设为 NVIDIA DLSS 后可用"));
	}

	// 行内说明写清楚“现在实际是什么”：开关能即时切换时立刻生效；不能时（刚装好的第一次启动）写明重启前后各是什么
	FString NeuralRenderingEnabledDesc()
	{
		using namespace ApexNeuralRendering;
		const FSettings& S = GetSettings();
		if (S.Status == EStatus::WriteFailed) return TEXT("保存失败，请检查游戏目录权限后重试");
		if (S.bLiveToggle) return S.bTogglePending ? FString(TEXT("正在切换…")) : FString(TEXT("立即生效，画面上会提示 On / Off"));
		if (S.bEnabled != S.bEnabledAtLaunch)
		{
			return FString::Printf(TEXT("现在仍是%s，重启游戏后变为%s"), S.bEnabledAtLaunch ? TEXT("开") : TEXT("关"), S.bEnabled ? TEXT("开") : TEXT("关"));
		}
		return TEXT("这次启动改了要重启才生效");
	}

	FString NeuralRenderingPassesDesc()
	{
		using namespace ApexNeuralRendering;
		const FSettings& S = GetSettings();
		if (S.Status == EStatus::WriteFailed) return TEXT("保存失败，请检查游戏目录权限后重试");
		if (S.Passes != S.PassesAtLaunch) return FString::Printf(TEXT("现在是 %d 次，重启游戏后变为 %d 次"), S.PassesAtLaunch, S.Passes);
		return TEXT("改了要重启游戏才生效");
	}
#endif
}

void SApexSettings::Construct(const FArguments& InArgs)
{
	SApexScreen::Construct(InArgs);

	auto OnOff = [](const TCHAR* Off, const TCHAR* On) { TArray<FString> A; A.Add(Off); A.Add(On); return A; };

	// ---------------- 画面 ----------------
	{
		FPage& Pg = Pages.AddDefaulted_GetRef();
		Pg.Name = TEXT("画面");
		Pg.En = TEXT("GRAPHICS");
		Pg.Icon = EApexIcon::Display;

		FRow& Tier = Pg.Rows.AddDefaulted_GetRef();
		Tier.Name = TEXT("画质档位");
		Tier.Desc = TEXT("影响光照、阴影、反射和植被细节");
		Tier.Detail = TEXT("先选档位，再用分辨率缩放微调帧率。");
		for (int32 i = 0; i < 4; ++i)
		{
			Tier.Options.Add(FGfx::GetTierLabel(static_cast<EApexGraphicsTier>(i)));
			Tier.OptionDescs.Add(FGfx::GetTierDescription(static_cast<EApexGraphicsTier>(i)));
		}
		Tier.Get = [](const UApexDriftSaveGame& S) { return static_cast<int32>(S.GraphicsTier); };
		Tier.Set = [](UApexDriftSaveGame& S, int32 V) { S.GraphicsTier = static_cast<EApexGraphicsTier>(V); };

#if PLATFORM_WINDOWS
		FRow& Up = Pg.Rows.AddDefaulted_GetRef();
		Up.Name = TEXT("超分技术");
		Up.Desc = TEXT("用哪种技术把渲染分辨率还原到屏幕分辨率");
		Up.Detail = TEXT("NVIDIA DLSS 要 RTX 显卡，画面更稳、细节更多；分辨率缩放选“原生 100%”时就是 DLAA（只抗锯齿，不降分辨率）。显卡不支持时自动用 TSR。");
		Up.Options = { TEXT("TSR（引擎自带）"), TEXT("NVIDIA DLSS") };
		Up.OptionDescs = { TEXT("所有显卡都能用"), TEXT("RTX 显卡专用，推荐") };
		Up.Get = [](const UApexDriftSaveGame& S) { return (S.UpscalerIndex == 1 && FGfx::IsDLSSAvailable()) ? 1 : 0; };
		Up.Set = [](UApexDriftSaveGame& S, int32 V) { S.UpscalerIndex = V; };
		Up.DisabledWhy = [](const UApexDriftSaveGame&) { return FGfx::IsDLSSAvailable() ? FString() : FString(TEXT("需要 NVIDIA RTX 显卡（DirectX 12）")); };

		FRow& Nr = Pg.Rows.AddDefaulted_GetRef();
		Nr.Name = TEXT("DLSS 5 神经渲染");
		Nr.Detail = TEXT("用 AI 增强赛道的光照、材质和细节，显卡负载明显增加（RTX 5070 每次约 2.8ms）。开关立即生效，画面上会提示 On / Off；刚装好后的第一次启动要先重启一次才能即时切换。由第三方 OptiScaler-NR 提供，NVIDIA 的模型需要 RTX 50 系列。");
		Nr.Options = OnOff(TEXT("关"), TEXT("开"));
		Nr.OptionDescs = { TEXT("使用游戏原有的光照和材质"), TEXT("启用 AI 光照和材质增强") };
		Nr.DefaultChoice = 0;   // 很吃显卡，恢复默认时关掉
		Nr.Get = [](const UApexDriftSaveGame&) { return ApexNeuralRendering::GetSettings().bEnabled ? 1 : 0; };
		Nr.Set = [](UApexDriftSaveGame&, int32 V) { ApexNeuralRendering::SetEnabled(V != 0); };
		Nr.DisabledWhy = NeuralRenderingDisabledWhy;
		Nr.DynDesc = NeuralRenderingEnabledDesc;

		FRow& NrPasses = Pg.Rows.AddDefaulted_GetRef();
		NrPasses.Name = TEXT("神经渲染次数");
		NrPasses.Detail = TEXT("每帧运行 AI 增强的次数。次数越多变化越明显，显卡负载按次数成倍增加。OptiScaler 只在启动时读次数，改完点下面的“立即重启生效”。");
		NrPasses.Options = { TEXT("1 次"), TEXT("2 次"), TEXT("3 次") };
		NrPasses.OptionDescs = { TEXT("默认，显卡负载最低"), TEXT("增强两遍，显卡负载增加"), TEXT("增强三遍，显卡负载最高") };
		NrPasses.DefaultChoice = 0;
		NrPasses.Get = [](const UApexDriftSaveGame&) { return ApexNeuralRendering::GetSettings().Passes - 1; };
		NrPasses.Set = [](UApexDriftSaveGame&, int32 V) { ApexNeuralRendering::SetPasses(V + 1); };
		NrPasses.DisabledWhy = [](const UApexDriftSaveGame& Profile)
		{
			const FString Why = NeuralRenderingDisabledWhy(Profile);
			if (!Why.IsEmpty())
			{
				return Why;
			}
			return ApexNeuralRendering::GetSettings().bEnabled ? FString() : FString(TEXT("开启 DLSS 5 神经渲染后可调"));
		};
		NrPasses.DynDesc = NeuralRenderingPassesDesc;
#endif

		FRow& Tsr = Pg.Rows.AddDefaulted_GetRef();
		Tsr.Name = TEXT("分辨率缩放");
		Tsr.Desc = TEXT("降低渲染分辨率换取帧率，再超分还原");
#if PLATFORM_WINDOWS
		Tsr.Detail = TEXT("先用较低的分辨率渲染，再用 TSR 或 DLSS（见“超分技术”）还原到屏幕分辨率。");
#else
		Tsr.Detail = TEXT("先用较低的分辨率渲染，再用 TSR 还原到屏幕分辨率。");
#endif
		for (int32 i = 0; i < 5; ++i)
		{
			Tsr.Options.Add(FGfx::GetTSRLabel(static_cast<EApexTSRMode>(i)));
		}
		Tsr.OptionDescs = {
			TEXT("按目标帧率自动在 50%–100% 之间调整，推荐"),
			TEXT("不缩放，最清晰，也最吃性能"),
			TEXT("接近原生清晰度"),
			TEXT("清晰度与帧率折中"),
			TEXT("帧率最高，远处细节略软") };
		Tsr.Get = [](const UApexDriftSaveGame& S) { return static_cast<int32>(S.TSRMode); };
		Tsr.Set = [](UApexDriftSaveGame& S, int32 V) { S.TSRMode = static_cast<EApexTSRMode>(V); };

		FRow& Fps = Pg.Rows.AddDefaulted_GetRef();
		Fps.Name = TEXT("目标帧率");
		Fps.Desc = TEXT("动态分辨率会尽量保持这个帧率");
		Fps.Detail = TEXT("设为显示器刷新率或略低一些。目标越高，渲染分辨率会降得越多。");
		for (int32 i = 0; i < 4; ++i)
		{
			Fps.Options.Add(FString::Printf(TEXT("%d 帧"), FGfx::GetFrameTargetFromIndex(i)));
		}
		Fps.Get = [](const UApexDriftSaveGame& S) { return FMath::Clamp(S.FrameTargetIndex, 0, 3); };
		Fps.Set = [](UApexDriftSaveGame& S, int32 V) { S.FrameTargetIndex = V; };
		Fps.DisabledWhy = [](const UApexDriftSaveGame& S) { return S.TSRMode == EApexTSRMode::AutoDynamic ? FString() : FString(TEXT("仅在“动态分辨率”下可用")); };

		FRow& Res = Pg.Rows.AddDefaulted_GetRef();
		Res.Name = TEXT("分辨率");
		Res.Desc = TEXT("全屏和窗口模式的输出分辨率");
		Res.Detail = TEXT("无边框模式始终使用桌面分辨率，想改清晰度请用“分辨率缩放”。");
		for (int32 i = 0; i < 4; ++i)
		{
			const FIntPoint R = FGfx::GetResolutionFromIndex(i);
			Res.Options.Add(FString::Printf(TEXT("%d × %d"), R.X, R.Y));
		}
		Res.Get = [](const UApexDriftSaveGame& S) { return FMath::Clamp(S.ResolutionIndex, 0, 3); };
		Res.Set = [](UApexDriftSaveGame& S, int32 V) { S.ResolutionIndex = V; };
		Res.DisabledWhy = [](const UApexDriftSaveGame& S) { return S.WindowModeIndex == 1 ? FString(TEXT("无边框模式使用桌面分辨率")) : FString(); };
		Res.bVideo = true;

		FRow& Win = Pg.Rows.AddDefaulted_GetRef();
		Win.Name = TEXT("显示模式");
		Win.Desc = TEXT("全屏延迟最低，无边框切换最方便");
		Win.Options = { TEXT("独占全屏"), TEXT("无边框"), TEXT("窗口") };
		Win.OptionDescs = { TEXT("输入延迟最低"), TEXT("切到其它程序不黑屏，推荐"), TEXT("可拖动的普通窗口") };
		Win.Get = [](const UApexDriftSaveGame& S) { return FMath::Clamp(S.WindowModeIndex, 0, 2); };
		Win.Set = [](UApexDriftSaveGame& S, int32 V) { S.WindowModeIndex = V; };
		Win.bVideo = true;

		FRow& VS = Pg.Rows.AddDefaulted_GetRef();
		VS.Name = TEXT("垂直同步");
		VS.Desc = TEXT("消除画面撕裂，会增加一点延迟");
		VS.Detail = TEXT("显示器支持 G-SYNC / FreeSync 时建议关闭。");
		VS.Options = OnOff(TEXT("关"), TEXT("开"));
		VS.Get = [](const UApexDriftSaveGame& S) { return S.bVSyncEnabled ? 1 : 0; };
		VS.Set = [](UApexDriftSaveGame& S, int32 V) { S.bVSyncEnabled = V != 0; };
		VS.bVideo = true;

		FRow& SF = Pg.Rows.AddDefaulted_GetRef();
		SF.Name = TEXT("显示帧率");
		SF.Desc = TEXT("驾驶时在右上角显示实时帧率");
		SF.Options = OnOff(TEXT("关"), TEXT("开"));
		SF.Get = [](const UApexDriftSaveGame& S) { return S.bShowFps ? 1 : 0; };
		SF.Set = [](UApexDriftSaveGame& S, int32 V) { S.bShowFps = V != 0; };

		FRow& MB = Pg.Rows.AddDefaulted_GetRef();
		MB.Name = TEXT("动态模糊");
		MB.Desc = TEXT("高速时的画面拖影，更有速度感");
		MB.Detail = TEXT("容易晕 3D 的话建议关闭。");
		MB.Options = OnOff(TEXT("关"), TEXT("开"));
		MB.Get = [](const UApexDriftSaveGame& S) { return S.bMotionBlur ? 1 : 0; };
		MB.Set = [](UApexDriftSaveGame& S, int32 V) { S.bMotionBlur = V != 0; };
	}

	// ---------------- 操控 ----------------
	{
		FPage& Pg = Pages.AddDefaulted_GetRef();
		Pg.Name = TEXT("操控");
		Pg.En = TEXT("DRIVING");
		Pg.Icon = EApexIcon::Wheel;

		FRow& As = Pg.Rows.AddDefaulted_GetRef();
		As.Name = TEXT("漂移辅助");
		As.Desc = TEXT("决定车子多容易甩尾、会不会帮你反打");
		As.Detail = TEXT("新手从“均衡”开始。能连续漂过两个弯以后，再换“专业”追求更大的角度。");
		As.Options = { TEXT("均衡"), TEXT("专业"), TEXT("拟真") };
		As.OptionDescs = {
			TEXT("点手刹触发漂移，松方向或反打马上回正，不容易打转。"),
			TEXT("低速满油满舵也会甩尾，可用角度更大；松方向或反打同样能收住。"),
			TEXT("轮胎物理（和地平线一样）：没有任何把车头拉回的辅助，甩出去要自己反打、收油，也能一直倒着滑。") };
		As.Get = [](const UApexDriftSaveGame& S) { return static_cast<int32>(S.HandlingAssist); };
		As.Set = [](UApexDriftSaveGame& S, int32 V) { S.HandlingAssist = static_cast<EApexHandlingAssist>(V); };

		FRow& Tc = Pg.Rows.AddDefaulted_GetRef();
		Tc.Name = TEXT("牵引力控制");
		Tc.Desc = TEXT("拟真档：不让油门把后轮踩空");
		Tc.Detail = TEXT("只限油门，从不转车头。平时车滑起来（压路肩、蹭墙、收油甩尾）也照样限住空转，按着油门不会越甩越大。拉手刹甩起来以后不限，直到车身稳住：可以用油门保持漂移、做 360。关掉后任何时候全油门都能把车尾踩出去，也更容易打转。");
		Tc.Options = { TEXT("开"), TEXT("关") };
		Tc.OptionDescs = { TEXT("平时不会被油门甩尾；用手刹起漂后放开油门，可以用油门续漂"), TEXT("后轮随便空转：油门就能甩尾、保持漂移，但更容易打转") };
		Tc.Get = [](const UApexDriftSaveGame& S) { return S.bTractionControl ? 0 : 1; };
		Tc.Set = [](UApexDriftSaveGame& S, int32 V) { S.bTractionControl = V == 0; };
		Tc.DisabledWhy = [](const UApexDriftSaveGame& S) { return S.HandlingAssist == EApexHandlingAssist::SimulationRaw ? FString() : FString(TEXT("只在“拟真”档起作用")); };

		FRow& Tr = Pg.Rows.AddDefaulted_GetRef();
		Tr.Name = TEXT("变速箱");
		Tr.DynDesc = [] { return FString::Printf(TEXT("手动模式用 %s / %s（RB / LB）升降挡"), *ApexKeys::Name(EApexBind::ShiftUp), *ApexKeys::Name(EApexBind::ShiftDown)); };
		Tr.Options = { TEXT("自动"), TEXT("手动") };
		Tr.OptionDescs = { TEXT("专注方向和油门"), TEXT("自己用升挡 / 降挡键换挡（可在“按键”页改），顶到红线不升挡车速就上不去") };
		Tr.Get = [](const UApexDriftSaveGame& S) { return S.bManualTransmission ? 1 : 0; };
		Tr.Set = [](UApexDriftSaveGame& S, int32 V) { S.bManualTransmission = V != 0; };

		FRow& Fov = Pg.Rows.AddDefaulted_GetRef();
		Fov.Type = EType::Slider;
		Fov.Name = TEXT("视野");
		Fov.Desc = TEXT("数值越大看得越宽，速度感也越强");
		Fov.Detail = TEXT("默认 82 度。屏幕较小或坐得较近时可以调到 90 左右。");
		Fov.Min = 68.0f; Fov.Max = 105.0f; Fov.Step = 1.0f;
		Fov.GetF = [](const UApexDriftSaveGame& S) { return S.BaseCameraFOV; };
		Fov.SetF = [](UApexDriftSaveGame& S, float V) { S.BaseCameraFOV = FMath::RoundToFloat(V); };
		Fov.Fmt = [](float V) { return FString::Printf(TEXT("%.0f°"), V); };
	}

	// ---------------- 声音 ----------------
	{
		FPage& Pg = Pages.AddDefaulted_GetRef();
		Pg.Name = TEXT("声音");
		Pg.En = TEXT("AUDIO");
		Pg.Icon = EApexIcon::Speaker;
		auto AddVol = [&Pg](const TCHAR* Name, const TCHAR* Desc, TFunction<float(const UApexDriftSaveGame&)> G, TFunction<void(UApexDriftSaveGame&, float)> St)
		{
			FRow& R = Pg.Rows.AddDefaulted_GetRef();
			R.Type = EType::Slider;
			R.Name = Name;
			R.Desc = Desc;
			R.Min = 0.0f; R.Max = 1.0f; R.Step = 0.05f;
			R.GetF = MoveTemp(G);
			R.SetF = MoveTemp(St);
			R.Fmt = [](float V) { return FString::Printf(TEXT("%d"), FMath::RoundToInt(V * 100.0f)); };
		};
		AddVol(TEXT("主音量"), TEXT("整体音量"), [](const UApexDriftSaveGame& S) { return S.MasterVolume; }, [](UApexDriftSaveGame& S, float V) { S.MasterVolume = V; });
		AddVol(TEXT("引擎"), TEXT("发动机、涡轮与排气"), [](const UApexDriftSaveGame& S) { return S.EngineVolume; }, [](UApexDriftSaveGame& S, float V) { S.EngineVolume = V; });
		AddVol(TEXT("轮胎"), TEXT("胎噪与漂移尖叫，是判断抓地的重要线索"), [](const UApexDriftSaveGame& S) { return S.TireVolume; }, [](UApexDriftSaveGame& S, float V) { S.TireVolume = V; });
		AddVol(TEXT("音效"), TEXT("界面、得分、碰撞与环境"), [](const UApexDriftSaveGame& S) { return S.SfxVolume; }, [](UApexDriftSaveGame& S, float V) { S.SfxVolume = V; });
	}

	// ---------------- 按键 ----------------
	{
		FPage& Pg = Pages.AddDefaulted_GetRef();
		Pg.Name = TEXT("按键");
		Pg.En = TEXT("CONTROLS");
		Pg.Icon = EApexIcon::Keyboard;
		for (int32 b = 0; b < ApexKeys::Num; ++b)
		{
			const ApexKeys::FInfo& I = ApexKeys::Info(static_cast<EApexBind>(b));
			FRow& K = Pg.Rows.AddDefaulted_GetRef();
			K.Type = EType::Key;
			K.Bind = b;
			K.Name = I.Label;
			K.Desc = I.Desc;
		}
	}
}

FLinearColor SApexSettings::AccentColor() const
{
	return MapAccent(PC.IsValid() ? PC->GetCurrentMapIndex() : 0);
}

SApexSettings::FLayout SApexSettings::Layout(const FVector2f& S) const
{
	FLayout L;
	L.X0 = FMath::Max(56.0f, (S.X - 1688.0f) * 0.5f);
	L.ListX = L.X0 + 268.0f;
	L.ListW = 900.0f;
	L.DetailX = L.ListX + L.ListW + 48.0f;
	L.DetailW = FMath::Max(260.0f, S.X - L.X0 - L.DetailX);
	if (Tab == KeysTab)
	{
		L.PerCol = (Pages[Tab].Rows.Num() + 1) / 2;
		L.RowH = FMath::Min(84.0f, (S.Y - 150.0f - L.Y0) / FMath::Max(1, L.PerCol));
	}
	else if (Tab == 0)
	{
		L.RowH = FMath::Min(L.RowH, (S.Y - 146.0f - L.Y0) / FMath::Max(1, Pages[Tab].Rows.Num()));
	}
	return L;
}

FApexRect SApexSettings::RowRect(const FLayout& L, int32 i) const
{
	if (L.PerCol > 0)
	{
		const float W = (L.ListW - 16.0f) * 0.5f;
		return FApexRect(L.ListX + (i / L.PerCol) * (W + 16.0f), L.Y0 + (i % L.PerCol) * L.RowH, W, L.RowH - 8.0f);
	}
	return FApexRect(L.ListX, L.Y0 + i * L.RowH, L.ListW, L.RowH - 8.0f);
}
FApexRect SApexSettings::KeyCapRect(const FApexRect& Row) const { return FApexRect(Row.R() - 20.0f - KeyCapW, Row.CY() - 23.0f, KeyCapW, 46.0f); }
float SApexSettings::ControlWidth(const FRow& R) const { return R.Type == EType::Key ? KeyCapW + 20.0f : ControlW; }
FApexRect SApexSettings::SliderRect(const FApexRect& Row) const { return FApexRect(Row.R() - ControlW + 4.0f, Row.CY() - 20.0f, ControlW - 100.0f, 40.0f); }
FApexRect SApexSettings::ArrowRect(const FApexRect& Row, bool bRight) const
{
	const float X = bRight ? Row.R() - 20.0f - 42.0f : Row.R() - ControlW;
	return FApexRect(X, Row.CY() - 21.0f, 42.0f, 42.0f);
}

FString SApexSettings::WhyDisabled(const FRow& R) const
{
	return (R.DisabledWhy && Save.IsValid()) ? R.DisabledWhy(*Save) : FString();
}

const SApexSettings::FRow* SApexSettings::RowOf(int32 Id) const
{
	const int32 i = Id - IdRow0;
	return (Pages.IsValidIndex(Tab) && Pages[Tab].Rows.IsValidIndex(i)) ? &Pages[Tab].Rows[i] : nullptr;
}

int32 SApexSettings::DefaultFocus() const
{
	return Pages.IsValidIndex(Tab) && Pages[Tab].Rows.Num() > 0 ? IdRow0 : IdBack;
}

void SApexSettings::OnShown()
{
	TabT = 0.0f;
	LastRow = 0;
	PendingCommit = -1.0f;
	CaptureRow = -1;
	NoticeT = 0.0f;
}

void SApexSettings::OnHidden()
{
	CaptureRow = -1;
	if (PendingCommit >= 0.0f)
	{
		Commit(false);
	}
}

void SApexSettings::BuildItems(const FVector2f& S)
{
	const FLayout L = Layout(S);
	for (int32 i = 0; i < Pages.Num(); ++i)
	{
		Put(IdTab0 + i, FApexRect(L.X0 + 8.0f, L.Y0 + i * 76.0f, 224.0f, 64.0f), Tab == i);
	}
	for (int32 i = 0; i < Pages[Tab].Rows.Num(); ++i)
	{
		Put(IdRow0 + i, RowRect(L, i));
	}
	const float BY = S.Y - 130.0f;
	if (Pages[Tab].Rows.Num() > 0)
	{
		Put(IdDefaults, FApexRect(L.ListX + 8.0f, BY, 250.0f, 54.0f));
	}
#if PLATFORM_WINDOWS
	if (Tab == 0 && ApexNeuralRendering::NeedsRestart())
	{
		Put(IdRestart, FApexRect(L.ListX + 278.0f, BY, 270.0f, 54.0f));   // DLSS 5 次数等要重启才生效的改动
	}
#endif
	Put(IdBack, FApexRect(L.X0 + 8.0f, BY, 224.0f, 54.0f));
	if (FocusId >= IdRow0 && RowOf(FocusId))
	{
		LastRow = FocusId - IdRow0;
	}
	if (CaptureRow >= 0)
	{
		LastRow = CaptureRow;
	}
}

void SApexSettings::TickScreen(float Dt)
{
	TabT = FMath::Min(1.0f, TabT + Dt * 4.0f);
	NoticeT = FMath::Max(0.0f, NoticeT - Dt);
	if (PendingCommit >= 0.0f)
	{
		PendingCommit -= Dt;
		if (PendingCommit < 0.0f)
		{
			Commit(false);
		}
	}
}

void SApexSettings::SetTab(int32 T)
{
	T = FMath::Clamp(T, 0, Pages.Num() - 1);
	if (T != Tab)
	{
		CaptureRow = -1;
		NoticeT = 0.0f;
		Tab = T;
		TabT = 0.0f;
		LastRow = 0;
		Click();
	}
}

bool SApexSettings::OnShoulder(int32 Dir)
{
	SetTab((Tab + Dir + Pages.Num()) % Pages.Num());
	FocusId = DefaultFocus();
	return true;
}

void SApexSettings::Commit(bool bVideo)
{
	PendingCommit = -1.0f;
	if (PC.IsValid())
	{
		PC->ApplyAndSaveSettings(bVideo);
	}
}

void SApexSettings::LivePreview() const
{
	// 滑条拖动时只更新音量与视野，不写盘、不刷画质
	if (!PC.IsValid() || !Save.IsValid() || !PC->GetVehiclePawn())
	{
		return;
	}
	AApexDriftVehiclePawn* Pawn = PC->GetVehiclePawn();
	if (Pawn->VehicleAudioComp)
	{
		Pawn->VehicleAudioComp->ApplyVolumeSettings(Save->MasterVolume, Save->EngineVolume, Save->TireVolume, Save->SfxVolume);
	}
	Pawn->ApplyHandlingSettings(Save->HandlingAssist, Save->bManualTransmission, Save->BaseCameraFOV, Save->bTractionControl);
}

void SApexSettings::StepChoice(const FRow& R, int32 Dir)
{
	if (!Save.IsValid() || R.Type != EType::Choice || !WhyDisabled(R).IsEmpty())
	{
		return;
	}
	const int32 N = R.Options.Num();
	const int32 Cur = R.Get(*Save);
	int32 Next = Cur + Dir;
	if (N == 2)
	{
		Next = 1 - Cur; // 开关项左右都能切
	}
	Next = (Next % N + N) % N;
	if (Next != Cur)
	{
		R.Set(*Save, Next);
		Commit(R.bVideo);
	}
}

void SApexSettings::SetSlider(const FRow& R, float V)
{
	if (!Save.IsValid())
	{
		return;
	}
	V = FMath::Clamp(FMath::RoundToFloat(V / R.Step) * R.Step, R.Min, R.Max);
	if (!FMath::IsNearlyEqual(V, R.GetF(*Save), 1e-4f))
	{
		R.SetF(*Save, V);
		LivePreview();
	}
}

bool SApexSettings::OnAdjust(int32 Id, int32 Dir)
{
	const FRow* R = RowOf(Id);
	if (!R || R->Type == EType::Key)
	{
		return false;   // 按键页左右键在两列之间移动
	}
	if (R->Type == EType::Slider)
	{
		if (Save.IsValid())
		{
			SetSlider(*R, R->GetF(*Save) + R->Step * Dir);
			PendingCommit = 0.35f;
		}
	}
	else
	{
		StepChoice(*R, Dir);
	}
	return true;
}

bool SApexSettings::OnDrag(int32 Id, const FVector2f& Pos, bool bEnd)
{
	const FRow* R = RowOf(Id);
	if (!R || R->Type != EType::Slider)
	{
		return false;
	}
	const FApexRect T = SliderRect(RectOf(Id));
	if (!bDragging && Pos.X < T.X - 24.0f)
	{
		return false; // 点在文字区不拖动
	}
	const float K = FMath::Clamp((Pos.X - T.X) / FMath::Max(1.0f, T.W), 0.0f, 1.0f);
	SetSlider(*R, FMath::Lerp(R->Min, R->Max, K));
	if (bEnd)
	{
		Commit(false);
	}
	return true;
}

void SApexSettings::OnActivate(int32 Id, const FVector2f& LocalPos, bool bMouse)
{
	if (Id >= IdTab0 && Id < IdTab0 + Pages.Num())
	{
		SetTab(Id - IdTab0);
		return;
	}
	if (Id == IdBack)
	{
		OnBack();
		return;
	}
	if (Id == IdDefaults)
	{
		RestoreDefaults();
		return;
	}
	if (Id == IdRestart)
	{
#if PLATFORM_WINDOWS
		if (PendingCommit >= 0.0f)
		{
			Commit(false);
		}
		if (!ApexNeuralRendering::RestartGame())
		{
			Notice = TEXT("没能自动重启，请手动关掉游戏再打开");
			NoticeT = 4.0f;
		}
#endif
		return;
	}
	if (CaptureRow >= 0)
	{
		// 等待按键时点了别处：只取消
		StopCapture(TEXT("已取消改键"));
		return;
	}
	if (const FRow* R = RowOf(Id))
	{
		if (R->Type == EType::Key)
		{
			StartCapture(Id - IdRow0);
			return;
		}
		if (R->Type == EType::Choice)
		{
			const FApexRect Row = RectOf(Id);
			if (bMouse && LocalPos.X < Row.R() - ControlW - 12.0f)
			{
				return; // 点名称只是选中该行
			}
			const bool bLeft = bMouse && LocalPos.X < Row.R() - ControlW + ControlW * 0.36f;
			StepChoice(*R, bLeft ? -1 : 1);
		}
	}
}

bool SApexSettings::OnBack()
{
	if (CaptureRow >= 0)
	{
		StopCapture(TEXT("已取消改键"));
		return true;
	}
	if (PendingCommit >= 0.0f)
	{
		Commit(false);
	}
	if (PC.IsValid())
	{
		PC->CloseSettingsMenu();
	}
	return true;
}

void SApexSettings::RestoreDefaults()
{
	if (!Save.IsValid())
	{
		return;
	}
	if (Tab == KeysTab)
	{
		CaptureRow = -1;
		ApexKeys::ResetAll(Save.Get());
		if (PC.IsValid()) PC->ApplyKeyBindings();
		Notice = TEXT("已恢复默认键位");
		NoticeT = 2.5f;
		Click();
		return;
	}
	const UApexDriftSaveGame* Def = GetDefault<UApexDriftSaveGame>();
	bool bVideo = false;
	for (const FRow& R : Pages[Tab].Rows)
	{
		if (!WhyDisabled(R).IsEmpty())
		{
			continue;   // 不可用的行不动（例如 DLSS 5 没装 / 没选 DLSS 时，不能被“恢复默认”悄悄写进配置）
		}
		if (R.Type == EType::Slider)
		{
			R.SetF(*Save, R.GetF(*Def));
		}
		else
		{
			const int32 D = R.DefaultChoice.Get(R.Get(*Def));
			if (D != R.Get(*Save))
			{
				R.Set(*Save, D);
				bVideo |= R.bVideo;
			}
		}
	}
	Commit(bVideo);
}

void SApexSettings::PaintRow(FApexPainter& P, const FLayout& L, int32 i) const
{
	const FRow& R = Pages[Tab].Rows[i];
	const int32 Id = IdRow0 + i;
	const FApexRect B = RowRect(L, i);
	const float H = HoverOf(Id);
	const FLinearColor Accent = AccentColor();
	const FString Why = WhyDisabled(R);
	const bool bOff = !Why.IsEmpty();
	const float Old = P.GetAlpha();
	const float T = EaseOut((TabT - i * 0.05f) / 0.7f);
	P.SetAlpha(Old * T);

	// 右侧说明对应的行，即使鼠标移开也留一条弱提示
	const float Cur0 = (i == LastRow) ? 1.0f : 0.0f;
	P.SkewH(B, Paper(0.05f + 0.04f * Cur0 + 0.07f * H), Paper(0.03f + 0.02f * Cur0 + 0.05f * H), Slant);
	if (H > 0.02f || Cur0 > 0.0f)
	{
		P.Skew(FApexRect(B.X, B.Y, 6.0f, B.H), WithA(Accent, FMath::Max(H, 0.55f * Cur0)), Slant);
	}
	const float TX = B.X + 34.0f;
	const float A = bOff ? 0.42f : 1.0f;
	P.Text(R.Name, TX, B.Y + 27.0f, EApexFont::Display, 28.0f, Paper(A));
	P.TextFit(bOff ? Why : RowDesc(R), TX - 5.0f, B.Y + 56.0f, EApexFont::Body, 16.0f, B.W - ControlWidth(R) - 60.0f, bOff ? Fade(Gold(), 0.8f) : TextLow());

	if (!Save.IsValid())
	{
		P.SetAlpha(Old);
		return;
	}

	if (R.Type == EType::Key)
	{
		const EApexBind Bd = static_cast<EApexBind>(R.Bind);
		const bool bCap = CaptureRow == i;
		const bool bChanged = ApexKeys::Get(Bd) != ApexKeys::GetDefault(Bd);
		const FApexRect KR = KeyCapRect(B);
		const float Pulse = 0.5f + 0.5f * FMath::Sin(TimeNow * 6.0f);
		P.RoundRect(FApexRect(KR.X, KR.Y + 4.0f, KR.W, KR.H), 8.0f, InkDeep(0.65f));
		P.RoundRect(KR, 8.0f, bCap ? WithA(Accent, 0.55f + 0.45f * Pulse) : (bChanged ? Mix(Paper(0.95f), Accent, 0.45f) : Paper(0.95f)));
		P.TextFit(bCap ? FString(TEXT("按新键…")) : ApexKeys::Name(Bd), KR.CX(), KR.CY(), EApexFont::BodyBold, 22.0f, KR.W - 16.0f, Ink(), EApexH::Center);
	}
	else if (R.Type == EType::Choice)
	{
		const FApexRect AL = ArrowRect(B, false), AR = ArrowRect(B, true);
		const int32 Cur = R.Get(*Save);
		const bool bHoverL = !bNavMode && HoverId == Id && AL.Contains(MousePos);
		const bool bHoverR = !bNavMode && HoverId == Id && AR.Contains(MousePos);
		auto Arrow = [&](const FApexRect& Rc, EApexIcon Ic, bool bHot)
		{
			P.Skew(Rc, bHot ? Paper(0.95f) : Paper(bOff ? 0.04f : 0.10f), Slant);
			Icon(P, Ic, Rc.Center(), 18.0f, bHot ? Ink() : Paper(bOff ? 0.25f : 0.90f), 3.0f);
		};
		Arrow(AL, EApexIcon::ArrowLeft, bHoverL && !bOff);
		Arrow(AR, EApexIcon::ArrowRight, bHoverR && !bOff);
		const float MidX = (AL.R() + AR.X) * 0.5f;
		const float MidW = AR.X - AL.R() - 24.0f;
		if (R.Options.IsValidIndex(Cur))
		{
			P.TextFit(R.Options[Cur], MidX, B.CY() - 6.0f, EApexFont::BodyBold, 22.0f, MidW, Paper(A), EApexH::Center);
		}
		// 选项位置指示
		const int32 N = R.Options.Num();
		const float PW = FMath::Min(28.0f, (MidW - (N - 1) * 6.0f) / N);
		const float PX = MidX - (N * PW + (N - 1) * 6.0f) * 0.5f;
		for (int32 k = 0; k < N; ++k)
		{
			P.Skew(FApexRect(PX + k * (PW + 6.0f), B.CY() + 18.0f, PW, 4.0f), k == Cur ? (bOff ? Paper(0.35f) : Accent) : Paper(0.16f), 0.5f);
		}
	}
	else
	{
		const FApexRect T0 = SliderRect(B);
		const float V = R.GetF(*Save);
		const float K = FMath::Clamp((V - R.Min) / FMath::Max(1e-4f, R.Max - R.Min), 0.0f, 1.0f);
		const float CY = T0.CY();
		P.Skew(FApexRect(T0.X, CY - 4.0f, T0.W, 8.0f), Paper(0.16f), 0.5f);
		P.SkewH(FApexRect(T0.X, CY - 4.0f, FMath::Max(1.0f, T0.W * K), 8.0f), Mix(Accent, Paper(), 0.0f), Mix(Accent, Paper(), 0.35f), 0.5f);
		const float KX = T0.X + T0.W * K;
		const float Kn = 11.0f + 2.0f * H;
		P.Skew(FApexRect(KX - Kn * 0.5f, CY - 15.0f, Kn, 30.0f), Paper(), Slant * 1.6f);
		P.Text(R.Fmt(V), B.R() - 24.0f, CY, EApexFont::Num, 30.0f, TextHi(), EApexH::Right);
	}
	if (IsFocused(Id))
	{
		FocusRing(P, B, Slant, 3.0f);
	}
	P.SetAlpha(Old);
}

void SApexSettings::PaintDetail(FApexPainter& P, const FLayout& L, const FVector2f& S) const
{
	const FPage& Pg = Pages[Tab];
	if (!Pg.Rows.IsValidIndex(LastRow))
	{
		return;
	}
	const FRow& R = Pg.Rows[LastRow];
	const FLinearColor Accent = AccentColor();
	const float X = L.DetailX;
	const float W = L.DetailW;
	float Y = L.Y0;
	const int32 NR = L.PerCol > 0 ? L.PerCol : Pg.Rows.Num();
	P.Rect(FApexRect(X, Y, 1.0f, NR * L.RowH > 420.0f ? NR * L.RowH - 8.0f : 420.0f), Line(0.16f));
	const float TX = X + 32.0f;
	const float TW = W - 40.0f;
	Y += 24.0f;
	P.Text(R.Name, TX, Y, EApexFont::Display, 36.0f, TextHi());
	P.SkewH(FApexRect(TX + 2.0f, Y + 30.0f, 92.0f, 4.0f), Accent, WithA(Accent, 0.0f), 0.5f);
	Y += 70.0f;
	const FString Body = R.Detail.IsEmpty() ? RowDesc(R) : R.Detail;
	const int32 N = P.TextWrap(Body, TX, Y, TW, EApexFont::Body, 19.0f, 32.0f, TextMid(), 4);
	Y += N * 32.0f + 18.0f;

	if (R.Type == EType::Choice && Save.IsValid())
	{
		const int32 Cur = R.Get(*Save);
		for (int32 i = 0; i < R.Options.Num(); ++i)
		{
			const bool bCur = i == Cur;
			const bool bHasDesc = R.OptionDescs.IsValidIndex(i) && !R.OptionDescs[i].IsEmpty();
			if (bCur)
			{
				P.Skew(FApexRect(TX + 2.0f, Y - 13.0f, 6.0f, 26.0f), Accent, Slant);
			}
			else
			{
				P.Skew(FApexRect(TX + 3.0f, Y - 5.0f, 4.0f, 10.0f), Paper(0.30f), Slant);
			}
			P.Text(R.Options[i], TX + 24.0f, Y, EApexFont::BodyBold, 19.0f, bCur ? TextHi() : TextLow());
			Y += 30.0f;
			if (bHasDesc)
			{
				const int32 Lines = P.TextWrap(R.OptionDescs[i], TX + 24.0f, Y, TW - 24.0f, EApexFont::Body, 16.0f, 26.0f, bCur ? TextMid() : Paper(0.48f), 3);
				Y += Lines * 26.0f + 10.0f;
			}
			else
			{
				Y += 8.0f;
			}
		}
	}
	else if (R.Type == EType::Slider && Save.IsValid())
	{
		P.Text(R.Fmt(R.GetF(*Save)), TX - 2.0f, Y + 34.0f, EApexFont::NumHeavy, 84.0f, TextHi());
		P.Text(FString::Printf(TEXT("默认 %s"), *R.Fmt(R.GetF(*GetDefault<UApexDriftSaveGame>()))), TX, Y + 96.0f, EApexFont::Body, 17.0f, TextLow());
	}
	else if (R.Type == EType::Key)
	{
		const EApexBind Bd = static_cast<EApexBind>(R.Bind);
		const bool bCap = CaptureRow == LastRow;
		const float Pulse = 0.5f + 0.5f * FMath::Sin(TimeNow * 6.0f);
		const FApexRect Cap(TX, Y, FMath::Min(TW, 320.0f), 76.0f);
		P.RoundRect(FApexRect(Cap.X, Cap.Y + 5.0f, Cap.W, Cap.H), 10.0f, InkDeep(0.65f));
		P.RoundRect(Cap, 10.0f, bCap ? WithA(Accent, 0.55f + 0.45f * Pulse) : Paper(0.95f));
		P.TextFit(bCap ? FString(TEXT("请按下新按键")) : ApexKeys::Name(Bd), Cap.CX(), Cap.CY(), EApexFont::BodyBold, 32.0f, Cap.W - 24.0f, Ink(), EApexH::Center);
		Y += Cap.H + 34.0f;
		P.Text(FString::Printf(TEXT("默认 %s"), *ApexKeys::KeyName(ApexKeys::GetDefault(Bd))), TX, Y, EApexFont::Body, 17.0f, TextLow());
		Y += 32.0f;
		Y += P.TextWrap(FString::Printf(TEXT("固定：%s"), ApexKeys::Info(Bd).Fixed), TX, Y, TW, EApexFont::Body, 17.0f, 28.0f, TextLow(), 2) * 28.0f + 14.0f;
		Y += P.TextWrap(bCap ? FString(TEXT("按 Esc（手柄 B）取消。Esc、P、回车、Tab 和方向键有固定用途，不能用。"))
			: FString(TEXT("点这一行或按回车，再按想用的键。和别的动作重复时，两个动作的键会互换。")),
			TX, Y, TW, EApexFont::Body, 17.0f, 28.0f, TextMid(), 3) * 28.0f + 14.0f;
	}
	if (NoticeT > 0.0f)   // 改键结果、重启失败（切页时清掉）
	{
		P.TextWrap(Notice, TX, Y + 10.0f, TW, EApexFont::BodyBold, 19.0f, 30.0f, WithA(Accent, FMath::Min(1.0f, NoticeT * 2.0f)), 3);
	}
}

void SApexSettings::PaintScreen(FApexPainter& P, const FVector2f& S) const
{
	const FLayout L = Layout(S);
	const FLinearColor Accent = AccentColor();
	const float Base = P.GetAlpha();
	P.SetAlpha(Base * EaseOut(OpenT / 0.28f));
	Backdrop(P, S, 22.0f, 0.84f);

	ScreenTitle(P, L.X0, 96.0f, TEXT("设置"), TEXT("SETTINGS"), Accent, 60.0f);
	P.Text(TEXT("改动自动保存"), S.X - L.X0, 108.0f, EApexFont::Body, 17.0f, TextLow(), EApexH::Right);

	for (int32 i = 0; i < Pages.Num(); ++i)
	{
		const FApexRect R = RectOf(IdTab0 + i);
		const float Sel = SelectOf(IdTab0 + i), H = HoverOf(IdTab0 + i);
		P.Skew(R, Mix(Paper(0.04f + 0.10f * H), Paper(0.96f), Sel));
		if (Sel > 0.01f) P.Skew(FApexRect(R.X, R.Y, 8.0f, R.H), WithA(Accent, Sel));
		const FLinearColor C = Mix(Paper(0.72f + 0.28f * H), Ink(), Sel);
		Icon(P, Pages[i].Icon, FVector2f(R.X + 48.0f, R.CY()), 28.0f, C, 2.4f);
		P.Text(Pages[i].Name, R.X + 84.0f, R.CY(), EApexFont::Display, 30.0f, C);
		if (IsFocused(IdTab0 + i)) FocusRing(P, R, Slant, 5.0f);
	}

	for (int32 i = 0; i < Pages[Tab].Rows.Num(); ++i)
	{
		PaintRow(P, L, i);
	}
	PaintDetail(P, L, S);

	const FApexRect RB = RectOf(IdBack);
	GhostButton(P, RB, TEXT("返回"), EApexIcon::ArrowLeft, HoverOf(IdBack), PressOf(IdBack), 25.0f);
	if (IsFocused(IdBack)) FocusRing(P, RB);
	if (Find(IdDefaults))
	{
		const FApexRect RD = RectOf(IdDefaults);
		GhostButton(P, RD, TEXT("恢复本页默认"), EApexIcon::Restart, HoverOf(IdDefaults), PressOf(IdDefaults), 23.0f);
		if (IsFocused(IdDefaults)) FocusRing(P, RD);
	}
	if (Find(IdRestart))
	{
		const FApexRect RR = RectOf(IdRestart);
		GhostButton(P, RR, TEXT("立即重启生效"), EApexIcon::Restart, HoverOf(IdRestart), PressOf(IdRestart), 23.0f);
		if (IsFocused(IdRestart)) FocusRing(P, RR);
	}

	if (CaptureRow >= 0)
	{
		const TTuple<FString, FString, FString> Hints[] = {
			MakeTuple(FString(TEXT("任意键")), FString(TEXT("—")), FString(TEXT("绑定到这个动作")) ),
			MakeTuple(FString(TEXT("Esc")), FString(TEXT("B")), FString(TEXT("取消")) ),
		};
		HintBar(P, L.X0 + 4.0f, S.Y - 42.0f, MakeArrayView(Hints));
	}
	else
	{
		const bool bKeys = Tab == KeysTab;
		const TTuple<FString, FString, FString> Hints[] = {
			MakeTuple(FString(bKeys ? TEXT("↑|↓|←|→") : TEXT("↑|↓")), FString(TEXT("左摇杆")), FString(TEXT("选择")) ),
			bKeys ? MakeTuple(FString(TEXT("Enter")), FString(TEXT("A")), FString(TEXT("改键")) )
				: MakeTuple(FString(TEXT("←|→")), FString(TEXT("←|→")), FString(TEXT("调整")) ),
			MakeTuple(FString(TEXT("Q|E")), FString(TEXT("LB|RB")), FString(TEXT("切换分类")) ),
			MakeTuple(FString(TEXT("Esc")), FString(TEXT("B")), FString(TEXT("返回")) ),
		};
		HintBar(P, L.X0 + 4.0f, S.Y - 42.0f, MakeArrayView(Hints));
	}
	P.SetAlpha(Base);
}

void SApexSettings::StartCapture(int32 Row)
{
	CaptureRow = Row;
	LastRow = Row;
	NoticeT = 0.0f;
	Click();
}

void SApexSettings::StopCapture(const FString& Msg)
{
	CaptureRow = -1;
	Notice = Msg;
	NoticeT = Msg.IsEmpty() ? 0.0f : 2.5f;
}

bool SApexSettings::OnCaptureKey(const FKey& Key, bool bRepeat)
{
	if (CaptureRow < 0)
	{
		// 刚绑定的键还按着：吞掉它的重复，免得拿来在菜单里导航
		if (bRepeat && Key == SwallowKey) return true;
		SwallowKey = FKey();
		return false;
	}
	if (bRepeat) return true;
	if (Key == EKeys::Escape || Key == EKeys::Gamepad_FaceButton_Right || Key == EKeys::Gamepad_Special_Right)
	{
		StopCapture(TEXT("已取消改键"));
		return true;
	}
	const FRow* R = Pages.IsValidIndex(Tab) && Pages[Tab].Rows.IsValidIndex(CaptureRow) ? &Pages[Tab].Rows[CaptureRow] : nullptr;
	if (!R || R->Type != EType::Key)
	{
		CaptureRow = -1;
		return true;
	}
	FString Why;
	if (!ApexKeys::CanBind(Key, &Why))
	{
		Notice = Why;   // 继续等
		NoticeT = 2.5f;
		return true;
	}
	const EApexBind B = static_cast<EApexBind>(R->Bind);
	const ApexKeys::FInfo& I = ApexKeys::Info(B);
	SwallowKey = Key;
	if (ApexKeys::Get(B) == Key)
	{
		StopCapture(FString::Printf(TEXT("“%s”保持 %s"), I.Label, *ApexKeys::KeyName(Key)));
		return true;
	}
	const EApexBind Other = ApexKeys::Set(Save.Get(), B, Key);
	if (Other != EApexBind::Count)
	{
		StopCapture(FString::Printf(TEXT("“%s”改成 %s；“%s”换成 %s"), I.Label, *ApexKeys::KeyName(Key), ApexKeys::Info(Other).Label, *ApexKeys::Name(Other)));
	}
	else
	{
		StopCapture(FString::Printf(TEXT("“%s”改成 %s"), I.Label, *ApexKeys::KeyName(Key)));
	}
	if (PC.IsValid()) PC->ApplyKeyBindings();
	Click();
	return true;
}

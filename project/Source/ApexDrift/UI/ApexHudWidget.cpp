#include "UI/ApexHudWidget.h"
#include "UI/ApexUIStyle.h"
#include "UI/ApexUIArt.h"
#include "Core/ApexDriftPlayerController.h"
#include "Core/ApexDriftSaveGame.h"
#include "Core/ApexKeyBindings.h"
#include "Core/ApexMapRegistry.h"
#include "Track/ApexTrackSplineActor.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "Widgets/SLeafWidget.h"

using namespace ApexUI;

#if !UE_BUILD_SHIPPING
static int32 GApexHudProfile = 0;
static FAutoConsoleVariableRef CVarApexHudProfile(TEXT("apex.HudProfile"), GApexHudProfile, TEXT("1 = 每 240 帧在日志里输出一次 HUD 绘制平均耗时"));
#endif

namespace
{
	using V2 = FVector2f;

	/** 数值不变就不重新拼字符串、不重新测量 */
	struct FHudText
	{
		FString S;
		FSlateFontInfo F;
		V2 Size = V2::ZeroVector;
		EApexFont Fam = EApexFont::Num;
		float SizePx = 20.0f;
		int64 Key = MIN_int64;
		bool bInit = false;

		void Init(EApexFont InFam, float InPx, int32 Spacing = 0)
		{
			Fam = InFam;
			SizePx = InPx;
			F = Font(InFam, InPx, Spacing);
			bInit = true;
		}
		void SetStr(const FString& In)
		{
			if (!S.Equals(In, ESearchCase::CaseSensitive) || Size.X <= 0.0f)
			{
				S = In;
				Size = S.IsEmpty() ? V2::ZeroVector : Measure(S, F);
			}
		}
		template <typename Fn>
		void Set(int64 NewKey, Fn&& Make)
		{
			if (NewKey != Key)
			{
				Key = NewKey;
				S = Make();
				Size = S.IsEmpty() ? V2::ZeroVector : Measure(S, F);
			}
		}
		void Draw(FApexPainter& P, float X, float Y, const FLinearColor& C, EApexH H = EApexH::Left, EApexV V = EApexV::Middle, float ShadowA = 0.45f) const
		{
			if (S.IsEmpty() || C.A * P.GetAlpha() < 0.004f)
			{
				return;
			}
			float L = X;
			if (H == EApexH::Center) L = X - Size.X * 0.5f;
			else if (H == EApexH::Right) L = X - Size.X;
			float T = Y;
			if (V == EApexV::Middle) T = Y - FApexPainter::MiddleEm(Fam) * SizePx;
			else if (V == EApexV::Baseline) T = Y - FApexPainter::AscentEm(Fam) * SizePx;
			P.TextCached(S, F, V2(L, T), C, ShadowA);
		}
		void DrawScaled(FApexPainter& P, float CX, float CY, float K, const FLinearColor& C, float ShadowA = 0.45f) const
		{
			if (S.IsEmpty())
			{
				return;
			}
			// 以视觉中线为缩放中心
			const float T = CY - FApexPainter::MiddleEm(Fam) * SizePx;
			P.TextScaled(S, F, Size, V2(CX - Size.X * 0.5f, T), K, C, ShadowA);
		}
	};

	FLinearColor AngleColor(float AbsDeg)
	{
		if (AbsDeg >= 55.0f) return Red();
		if (AbsDeg >= 35.0f) return Orange();
		if (AbsDeg >= 12.5f) return Green();
		return Paper(0.65f);
	}
}

class SApexHudCanvas : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SApexHudCanvas) {}
	SLATE_END_ARGS()

	void Construct(const FArguments&)
	{
		EnsureLoaded();
		SetCanTick(true);
		ForceVolatile(true);
		SetVisibility(EVisibility::HitTestInvisible);
		InitTexts();
	}

	void SetData(AApexDriftPlayerController* InPC, const FApexDriftTelemetry& InTelemetry, EApexGameModeType InMode, EApexGameState InState, float InCountdownSec, const TArray<FVector2D>& InMinimapPts)
	{
		PC = InPC;
		if (KeysVer != ApexKeys::Version())
		{
			RefreshKeyTexts();
		}
		// 状态迁移
		if (InState != State)
		{
			if (InState == EApexGameState::Countdown)
			{
				HintT = 0.0f;
				GoT = -1.0f;
				ZoneT = 100.0f;
				ZoneKey.Reset();
				DispScore = 0.0f;
				DriftA = 0.0f;
			}
			if (State == EApexGameState::Countdown && InState == EApexGameState::Playing)
			{
				GoT = 0.0f;
				HintT = 0.0f;
			}
		}
		Tel = InTelemetry;
		Mode = InMode;
		State = InState;
		Countdown = InCountdownSec;
		if (MapPts.Num() != InMinimapPts.Num() || (MapPts.Num() > 0 && !FVector2D(MapPts[0]).Equals(InMinimapPts[0], 1e-4)))
		{
			CacheMinimap(InMinimapPts);
		}
		if (Tel.ActiveZoneName != ZoneKey)
		{
			ZoneKey = Tel.ActiveZoneName;
			if (!ZoneKey.IsEmpty())
			{
				FString Main, Sub;
				SplitCallout(ZoneKey, Main, Sub);
				TZone.SetStr(Main);
				TZoneSub.SetStr(Sub.IsEmpty() ? FString(TEXT("DRIFT ZONE")) : Sub);
				ZoneT = 0.0f;
			}
		}
		if (Tel.CalloutText != CalloutKey)
		{
			CalloutKey = Tel.CalloutText;
			FString Main, Sub;
			SplitCallout(CalloutKey, Main, Sub);
			TCall.SetStr(Main);
			TCallSub.SetStr(Sub);
			CalloutT = 0.0f;
			bCalloutBad = CalloutKey.Contains(TEXT("碰撞")) || CalloutKey.Contains(TEXT("CRASH"));
		}
	}

	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(1920.0f, 1080.0f); }

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override
	{
		const float Dt = FMath::Clamp(InDeltaTime, 0.0f, 0.1f);
		TimeNow += Dt;
		HintT += Dt;
		ZoneT += Dt;
		CalloutT += Dt;
		if (GoT >= 0.0f) GoT += Dt;
		const bool bDrift = Tel.bIsDrifting || Tel.CurrentDriftScore > 0;
		DriftA = Approach(DriftA, bDrift ? 1.0f : 0.0f, Dt, bDrift ? 18.0f : 6.0f);
		if (bDrift)
		{
			DispScore = (Tel.CurrentDriftScore < DispScore) ? static_cast<float>(Tel.CurrentDriftScore) : Approach(DispScore, static_cast<float>(Tel.CurrentDriftScore), Dt, 14.0f);
			if (Tel.CurrentDriftScore - DispScore < 1.0f) DispScore = static_cast<float>(Tel.CurrentDriftScore);
		}
		if (Tel.DriftMultiplier > LastMult + 0.01f) MultPop = 1.0f;
		LastMult = Tel.DriftMultiplier;
		MultPop = FMath::Max(0.0f, MultPop - Dt * 3.0f);
		DispTotal = Approach(DispTotal, static_cast<float>(Tel.TotalScore), Dt, 8.0f);
		if (FMath::Abs(Tel.TotalScore - DispTotal) < 1.0f) DispTotal = static_cast<float>(Tel.TotalScore);
		DispRpm = Approach(DispRpm, FMath::Clamp(Tel.EngineRPM / FMath::Max(1000.0f, Tel.MaxRPM), 0.0f, 1.05f), Dt, 24.0f);
		DispSlip = Approach(DispSlip, Tel.SlipAngleDeg, Dt, 20.0f);
		DispThr = Approach(DispThr, FMath::Clamp(Tel.ThrottleInput, 0.0f, 1.0f), Dt, 24.0f);
		DispBrk = Approach(DispBrk, FMath::Clamp(Tel.BrakeInput, 0.0f, 1.0f), Dt, 24.0f);
		DispBoost = Approach(DispBoost, FMath::Max(0.0f, Tel.TurboBoostBar), Dt, 12.0f);
		WrongA = Approach(WrongA, Tel.bWrongWay ? 1.0f : 0.0f, Dt, 10.0f);
	}

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override
	{
		if (State == EApexGameState::MainMenu || State == EApexGameState::Finished)
		{
			return LayerId;
		}
#if !UE_BUILD_SHIPPING
		const uint64 Cyc0 = GApexHudProfile ? FPlatformTime::Cycles64() : 0;
#endif
		const V2 L = V2(AllottedGeometry.GetLocalSize());
		const float Scale = FMath::Max(0.05f, FMath::Min(L.Y / 1080.0f, L.X / 1440.0f));
		const V2 S(L.X / Scale, L.Y / Scale);
		const FGeometry Geo = AllottedGeometry.MakeChild(FVector2f(S), FSlateLayoutTransform(Scale, FVector2f(0.0f, 0.0f)));

		const int32 Map = PC.IsValid() ? PC->GetCurrentMapIndex() : 0;
		Accent = FApexMapRegistry::GetMaps()[FApexMapRegistry::ClampIndex(Map)].Accent;

		int32 OutLayer;
		{
			FApexPainter P(Geo, OutDrawElements, LayerId, InWidgetStyle.GetColorAndOpacityTint().A);
			// 先把所有形状画进同一批，再画文字，减少批次
			PaintSpeedShapes(P, S);
			PaintInfoShapes(P, S);
			PaintMinimap(P, S);
			if (DriftA > 0.01f) PaintDriftShapes(P, S);
			PaintZoneShapes(P, S);
			PaintWrongShapes(P, S);
			PaintHintShapes(P, S);

			PaintSpeedText(P, S);
			PaintInfoText(P, S);
			if (DriftA > 0.01f) PaintDriftText(P, S);
			PaintZoneText(P, S);
			PaintCallout(P, S);
			PaintWrongText(P, S);
			PaintHintText(P, S);
			PaintAuto(P, S);
			PaintTrainHint(P, S);
			PaintFps(P, S);
			PaintCountdown(P, S);
			OutLayer = P.Finish();
		}
#if !UE_BUILD_SHIPPING
		if (GApexHudProfile)
		{
			ProfCycles += FPlatformTime::Cycles64() - Cyc0;
			if (++ProfFrames >= 240)
			{
				UE_LOG(LogTemp, Display, TEXT("[ApexHud] avg paint %.4f ms over %d frames"), FPlatformTime::ToMilliseconds64(ProfCycles) / ProfFrames, ProfFrames);
				ProfCycles = 0;
				ProfFrames = 0;
			}
		}
#endif
		return OutLayer;
	}

private:
	// =====================================================================
	// 右下：车速 / 转速 / 挡位 / 踏板 / 增压
	// =====================================================================
	static constexpr float TachA0 = 2.4435f;   // 140 度
	static constexpr float TachSpan = 4.5379f; // 260 度
	static constexpr float TachR = 150.0f;
	V2 TachC(const V2& S) const { return V2(S.X - 236.0f, S.Y - 208.0f); }

	/** 手动挡换挡提示：1 升挡 -1 降挡 0 无 */
	int32 ShiftHint() const
	{
		if (!Tel.bManualGearbox || Tel.Gear <= 0) return 0;
		if (Tel.Gear < 6 && DispRpm > 0.90f) return 1;
		if (Tel.Gear > 1 && DispRpm < 0.30f && Tel.ThrottleInput > 0.4f) return -1;
		return 0;
	}

	void PaintSpeedShapes(FApexPainter& P, const V2& S) const
	{
		const V2 C = TachC(S);
		P.CircleGrad(C, 300.0f, InkDeep(0.55f), InkDeep(0.0f));
		P.Circle(C, TachR + 38.0f, Ink(0.44f));

		constexpr float Red0 = 0.86f;
		P.Arc(C, TachR, 7.0f, TachA0, TachA0 + TachSpan * Red0, Paper(0.20f));
		P.Arc(C, TachR, 7.0f, TachA0 + TachSpan * (Red0 + 0.006f), TachA0 + TachSpan, Fade(Red(), 0.55f));
		const float R = FMath::Clamp(DispRpm, 0.0f, 1.0f);
		const bool bRed = R > Red0;
		P.Arc(C, TachR, 9.0f, TachA0, TachA0 + TachSpan * FMath::Min(R, Red0), Mix(Accent, Paper(), 0.10f), Mix(Accent, Paper(), 0.55f));
		if (bRed)
		{
			P.Arc(C, TachR, 11.0f, TachA0 + TachSpan * Red0, TachA0 + TachSpan * R, Red());
		}
		// 指针点
		{
			const float A = TachA0 + TachSpan * R;
			const V2 D(FMath::Cos(A), FMath::Sin(A));
			P.LineSeg(C + D * (TachR - 20.0f), C + D * (TachR + 12.0f), 4.0f, bRed ? Red() : Paper());
		}
		// 刻度
		const int32 NK = FMath::Clamp(FMath::RoundToInt(Tel.MaxRPM / 1000.0f), 4, 12);
		for (int32 i = 0; i <= NK; ++i)
		{
			const float T = static_cast<float>(i) / NK;
			const float A = TachA0 + TachSpan * T;
			const V2 D(FMath::Cos(A), FMath::Sin(A));
			P.LineSeg(C + D * (TachR - 10.0f), C + D * (TachR - 19.0f), 2.0f, T >= Red0 ? Fade(Red(), 0.9f) : Paper(0.55f));
		}
		// 增压：外侧短弧
		{
			constexpr float B0 = 2.4435f, BSpan = 1.15f;
			P.Arc(C, TachR + 18.0f, 4.0f, B0, B0 + BSpan, Paper(0.16f));
			const float B = FMath::Clamp(DispBoost / 1.6f, 0.0f, 1.0f);
			if (B > 0.01f) P.Arc(C, TachR + 18.0f, 4.0f, B0, B0 + BSpan * B, Hex(0x8FE6FF));
		}
		// 踏板：右侧两条外弧，自下而上增长（内油门、外刹车）
		{
			constexpr float P1 = TachA0 + TachSpan, PSpan = 1.15f;
			P.Arc(C, TachR + 18.0f, 5.0f, P1, P1 - PSpan, Paper(0.16f));
			P.Arc(C, TachR + 28.0f, 5.0f, P1, P1 - PSpan, Paper(0.16f));
			if (DispThr > 0.01f) P.Arc(C, TachR + 18.0f, 5.0f, P1, P1 - PSpan * DispThr, Green());
			if (DispBrk > 0.01f) P.Arc(C, TachR + 28.0f, 5.0f, P1, P1 - PSpan * DispBrk, Red());
		}
		// 挡位
		{
			const FApexRect G(C.X - 34.0f, C.Y + 88.0f, 68.0f, 62.0f);
			P.Skew(G, bRed ? Red() : Paper(0.96f));
		}
		if (Tel.bHandbrakeActive)
		{
			P.Skew(FApexRect(C.X - 32.0f, C.Y + 156.0f, 64.0f, 27.0f), Red());
		}
		if (ShiftHint() != 0)
		{
			const float Pulse = 0.65f + 0.35f * FMath::Sin(static_cast<float>(FPlatformTime::Seconds()) * 16.0f);
			P.Skew(FApexRect(C.X - 74.0f, C.Y - 96.0f, 148.0f, 36.0f), Fade(ShiftHint() > 0 ? Red() : Accent, Pulse));
		}
	}

	void PaintSpeedText(FApexPainter& P, const V2& S) const
	{
		const V2 C = TachC(S);
		const int32 Kmh = FMath::RoundToInt(FMath::Abs(Tel.SpeedKmh));
		TSpeed.Set(Kmh, [Kmh] { return FString::FromInt(Kmh); });
		TSpeed.Draw(P, C.X + 2.0f, C.Y - 14.0f, Paper(), EApexH::Center);
		TKmh.Draw(P, C.X, C.Y + 50.0f, TextMid(), EApexH::Center);
		const bool bRed = DispRpm > 0.86f;
		TGear.Set(Tel.Gear, [this] { return Tel.Gear > 0 ? FString::FromInt(Tel.Gear) : FString(Tel.Gear < 0 ? TEXT("R") : TEXT("N")); });
		TGear.Draw(P, C.X, C.Y + 119.0f, bRed ? Paper() : Ink(), EApexH::Center, EApexV::Middle, 0.0f);
		TGearMode.Set(Tel.bManualGearbox ? 1 : 0, [this] { return FString(Tel.bManualGearbox ? TEXT("手动") : TEXT("自动")); });
		TGearMode.Draw(P, C.X + 50.0f, C.Y + 122.0f, TextMid());
		if (Tel.bHandbrakeActive)
		{
			THand.Draw(P, C.X, C.Y + 169.5f, Paper(), EApexH::Center, EApexV::Middle, 0.0f);
		}
		if (const int32 Hint = ShiftHint())
		{
			P.Text(Hint > 0 ? TEXT("升挡  E") : TEXT("降挡  Q"), C.X, C.Y - 78.0f, EApexFont::BodyBold, 22.0f, Paper(), EApexH::Center);
		}
		const int32 NK = FMath::Clamp(FMath::RoundToInt(Tel.MaxRPM / 1000.0f), 4, 12);
		for (int32 i = 0; i <= NK; ++i)
		{
			const float T = static_cast<float>(i) / NK;
			const float A = TachA0 + TachSpan * T;
			const V2 Pos = C + V2(FMath::Cos(A), FMath::Sin(A)) * (TachR - 34.0f);
			TTick[i].Draw(P, Pos.X, Pos.Y, T >= 0.86f ? Fade(Red(), 0.95f) : Paper(0.62f), EApexH::Center, EApexV::Middle, 0.0f);
		}
		const int32 B10 = FMath::RoundToInt(DispBoost * 10.0f);
		TBoost.Set(B10, [B10] { return FString::Printf(TEXT("增压 %d.%d"), B10 / 10, B10 % 10); });
		TBoost.Draw(P, C.X - 50.0f, C.Y + 122.0f, TextMid(), EApexH::Right, EApexV::Middle, 0.5f);
	}

	// =====================================================================
	// 左上：模式信息
	// =====================================================================
	void PaintInfoShapes(FApexPainter& P, const V2& S) const
	{
		P.CircleGrad(V2(120.0f, 96.0f), 470.0f, InkDeep(0.60f), InkDeep(0.0f));
		P.Skew(FApexRect(52.0f, 44.0f, 7.0f, 34.0f), Accent, 0.30f);
		if (Mode == EApexGameModeType::DriftAttack3Min)
		{
			// 剩余时间细条
			const float K = FMath::Clamp(Tel.SessionTimeRemaining / 180.0f, 0.0f, 1.0f);
			P.Skew(FApexRect(54.0f, 168.0f, 300.0f, 5.0f), Paper(0.20f), 0.5f);
			P.Skew(FApexRect(54.0f, 168.0f, 300.0f * K, 5.0f), Tel.SessionTimeRemaining < 15.0f ? Red() : Accent, 0.5f);
		}
		else if (Mode == EApexGameModeType::Circuit3Laps)
		{
			const int32 N = FMath::Max(1, Tel.TotalLaps);
			for (int32 i = 0; i < N; ++i)
			{
				P.Skew(FApexRect(54.0f + i * 102.0f, 168.0f, 94.0f, 5.0f), i + 1 < Tel.CurrentLap ? Accent : (i + 1 == Tel.CurrentLap ? Paper(0.85f) : Paper(0.22f)), 0.5f);
			}
		}
	}

	void PaintInfoText(FApexPainter& P, const V2& S) const
	{
		const float X = 78.0f;
		TMode.Set(static_cast<int32>(Mode), [this] { return ModeName(Mode); });
		TMode.Draw(P, X, 61.0f, TextHi());

		const int32 Total = FMath::RoundToInt(DispTotal);
		if (Mode == EApexGameModeType::DriftAttack3Min)
		{
			const int32 Sec = FMath::Max(0, FMath::CeilToInt(Tel.SessionTimeRemaining));
			TBig.Set((1ll << 40) + Sec, [Sec] { return FString::Printf(TEXT("%d:%02d"), Sec / 60, Sec % 60); });
			const bool bHurry = Sec <= 15;
			const float Pulse = bHurry ? 0.65f + 0.35f * FMath::Abs(FMath::Sin(TimeNow * 5.0f)) : 1.0f;
			TBig.Draw(P, 50.0f, 124.0f, bHurry ? WithA(Red(), Pulse) : Paper());
			TLabA.Set(2, [] { return FString(TEXT("总分")); });
			TValA.Set(Total, [Total] { return FormatScore(Total); });
			TLabB.Set(3, [] { return FString(TEXT("最佳单漂")); });
			TValB.Set(Tel.BestSingleDrift, [this] { return FormatScore(Tel.BestSingleDrift); });
		}
		else if (Mode == EApexGameModeType::Circuit3Laps)
		{
			TBig.Set((2ll << 40) + Tel.CurrentLap * 100 + Tel.TotalLaps, [this] { return FString::Printf(TEXT("%d/%d"), FMath::Min(Tel.CurrentLap, Tel.TotalLaps), Tel.TotalLaps); });
			TBig.Draw(P, 50.0f, 124.0f, Paper());
			const int64 Cs = FMath::FloorToInt(Tel.CurrentLapTime * 20.0f);
			TLapNow.Set(Cs, [this] { return FormatTime(Tel.CurrentLapTime); });
			TLapNow.Draw(P, 62.0f + TBig.Size.X + 22.0f, 134.0f, Accent);
			TLapCap.Set(1, [] { return FString(TEXT("本圈用时")); });
			TLapCap.Draw(P, 62.0f + TBig.Size.X + 22.0f, 102.0f, TextMid());
			TLabA.Set(4, [] { return FString(TEXT("总分")); });
			TValA.Set(Total, [Total] { return FormatScore(Total); });
			TLabB.Set(5, [] { return FString(TEXT("最快圈")); });
			TValB.Set((4ll << 40) + FMath::RoundToInt(Tel.BestLapTime * 100.0f), [this] { return FormatTimeOrDash(Tel.BestLapTime); });
		}
		else
		{
			TBig.Set(Total, [Total] { return FormatScore(Total); });
			TBig.Draw(P, 50.0f, 124.0f, Paper());
			TLabA.Set(6, [this] { return FString(TEXT("当前圈")); });
			const int64 Cs = FMath::FloorToInt(Tel.CurrentLapTime * 20.0f);
			TValA.Set((3ll << 40) + Cs, [this] { return FormatTime(Tel.CurrentLapTime); });
			TLabB.Set(7, [] { return FString(TEXT("最佳单漂")); });
			TValB.Set(Tel.BestSingleDrift, [this] { return FormatScore(Tel.BestSingleDrift); });
		}
		const float Y = 202.0f;
		TLabA.Draw(P, 52.0f, Y, TextMid());
		TValA.Draw(P, 52.0f + TLabA.Size.X + 12.0f, Y, Paper());
		const float X2 = FMath::Max(238.0f, 52.0f + TLabA.Size.X + 12.0f + TValA.Size.X + 32.0f);
		TLabB.Draw(P, X2, Y, TextMid());
		TValB.Draw(P, X2 + TLabB.Size.X + 12.0f, Y, Paper());
	}

	// =====================================================================
	// 左下：圆形小地图
	// =====================================================================
	void CacheMinimap(const TArray<FVector2D>& In)
	{
		MapPts.Reset(In.Num());
		ZoneMask.Reset();
		for (const FVector2D& Pt : In)
		{
			MapPts.Add(V2(Pt));
		}
		ZoneMask.Init(false, MapPts.Num());
		if (PC.IsValid() && PC->GetTrackActor() && MapPts.Num() > 0)
		{
			const AApexTrackSplineActor* Track = PC->GetTrackActor();
			const float Len = FMath::Max(1.0f, Track->GetTrackLength());
			for (const FApexDriftZoneInfo& Z : Track->DriftZones)
			{
				const int32 A = FMath::FloorToInt(Z.StartDistance / Len * MapPts.Num());
				const int32 B = FMath::CeilToInt(Z.EndDistance / Len * MapPts.Num());
				for (int32 i = A; i <= B; ++i)
				{
					if (MapPts.Num() > 0) ZoneMask[((i % MapPts.Num()) + MapPts.Num()) % MapPts.Num()] = true;
				}
			}
		}
		Work.Reserve(MapPts.Num() + 2);
		Piece.Reserve(MapPts.Num() + 2);
	}

	void PaintMinimap(FApexPainter& P, const V2& S) const
	{
		const float R = 112.0f;
		const V2 C(56.0f + R, S.Y - 52.0f - R);
		if (MapPts.Num() < 8)
		{
			return;
		}
		P.CircleGrad(C, R + 34.0f, InkDeep(0.55f), InkDeep(0.0f));
		P.Circle(C, R, Ink(0.62f));
		P.Ring(C, R, 2.0f, Paper(0.34f));

		const float View = 0.17f;                 // 可视半径（归一化赛道坐标）
		const float K = (R - 8.0f) / View;
		const float Ang = -(Tel.PlayerHeadingRad) - HALF_PI;
		const float Cs = FMath::Cos(Ang), Sn = FMath::Sin(Ang);
		const V2 Pl = V2(Tel.PlayerMinimapPos);
		const int32 N = MapPts.Num();
		Work.Reset();
		for (int32 i = 0; i <= N; ++i)
		{
			const V2 D = (MapPts[i % N] - Pl) * K;
			Work.Add(V2(D.X * Cs - D.Y * Sn, D.X * Sn + D.Y * Cs));
		}
		const float RR = R - 5.0f;
		auto EmitPass = [&](float Th, TFunctionRef<bool(int32)> Use, const FLinearColor& Col)
		{
			Piece.Reset();
			auto FlushPiece = [&]()
			{
				if (Piece.Num() >= 2) P.Stroke(Piece, Th, Col);
				Piece.Reset();
			};
			for (int32 i = 0; i < N; ++i)
			{
				if (!Use(i))
				{
					FlushPiece();
					continue;
				}
				V2 A = Work[i], B = Work[i + 1];
				// 与圆求交裁剪
				const V2 D = B - A;
				const float a = D.X * D.X + D.Y * D.Y;
				const float b = 2.0f * (A.X * D.X + A.Y * D.Y);
				const float c = A.X * A.X + A.Y * A.Y - RR * RR;
				float T0 = 0.0f, T1 = 1.0f;
				if (a < 1e-6f)
				{
					if (c > 0.0f) { FlushPiece(); continue; }
				}
				else
				{
					const float Disc = b * b - 4.0f * a * c;
					if (Disc <= 0.0f) { FlushPiece(); continue; }
					const float Sq = FMath::Sqrt(Disc);
					T0 = FMath::Max(0.0f, (-b - Sq) / (2.0f * a));
					T1 = FMath::Min(1.0f, (-b + Sq) / (2.0f * a));
					if (T0 >= T1) { FlushPiece(); continue; }
				}
				const V2 PA = C + A + D * T0, PB = C + A + D * T1;
				if (Piece.Num() == 0) Piece.Add(PA);
				else if (T0 > 0.0f) { FlushPiece(); Piece.Add(PA); }
				Piece.Add(PB);
				if (T1 < 1.0f) FlushPiece();
			}
			FlushPiece();
		};
		EmitPass(15.0f, [](int32) { return true; }, InkDeep(0.9f));
		EmitPass(8.0f, [](int32) { return true; }, Paper(0.88f));
		EmitPass(8.0f, [this](int32 i) { return ZoneMask.IsValidIndex(i) && ZoneMask[i]; }, Accent);

		// 起终点
		{
			V2 St = Work[0];
			const float Dist = FMath::Sqrt(St.X * St.X + St.Y * St.Y);
			if (Dist < RR - 6.0f)
			{
				V2 T = Work[1] - Work[0];
				const float TL = FMath::Max(1e-4f, FMath::Sqrt(T.X * T.X + T.Y * T.Y));
				T = T * (1.0f / TL);
				const V2 Nn(-T.Y, T.X);
				for (int32 i = -2; i < 2; ++i)
				{
					P.LineSeg(C + St + Nn * (i * 5.0f), C + St + Nn * (i * 5.0f + 5.0f), 5.0f, (i & 1) ? InkDeep() : Hex(0xFFFFFF));
				}
			}
			else
			{
				const V2 D = St * (1.0f / FMath::Max(1.0f, Dist));
				P.Circle(C + D * (R - 1.0f), 6.0f, InkDeep());
				P.Circle(C + D * (R - 1.0f), 4.0f, Paper());
			}
		}
		// 车标
		const V2 Arrow[4] = { C + V2(0, -13), C + V2(10, 11), C + V2(0, 5), C + V2(-10, 11) };
		P.Circle(C, 17.0f, InkDeep(0.35f));
		{
			const V2 LH[3] = { Arrow[0], Arrow[2], Arrow[3] };
			const V2 RH[3] = { Arrow[0], Arrow[1], Arrow[2] };
			P.Poly(MakeArrayView(LH, 3), Mix(Accent, Paper(), 0.25f));
			P.Poly(MakeArrayView(RH, 3), Accent);
			P.Stroke(MakeArrayView(Arrow, 4), 2.0f, InkDeep(), true);
		}
	}

	// =====================================================================
	// 顶部中央：漂移得分 / 倍率 / 角度
	// =====================================================================
	static constexpr float GaugeMax = 70.0f;
	static constexpr float GaugeHalf = 1.0821f; // 62 度
	V2 GaugeC(const V2& S) const { return V2(S.X * 0.5f, 262.0f); }
	static float GaugeAng(float Deg) { return -HALF_PI + FMath::Clamp(Deg, -GaugeMax, GaugeMax) / GaugeMax * GaugeHalf; }

	void PaintDriftShapes(FApexPainter& P, const V2& S) const
	{
		const float Old = P.GetAlpha();
		P.SetAlpha(Old * DriftA);
		const V2 C = GaugeC(S);
		constexpr float R = 208.0f;
		P.CircleGrad(V2(C.X, 150.0f), 330.0f, InkDeep(0.50f), InkDeep(0.0f));

		// 左右两侧刻度带：绿 / 橙 / 红
		auto Band = [&](float D0, float D1, const FLinearColor& Col)
		{
			P.Arc(C, R, 5.0f, GaugeAng(D0) + 0.006f, GaugeAng(D1) - 0.006f, Col);
			P.Arc(C, R, 5.0f, GaugeAng(-D1) + 0.006f, GaugeAng(-D0) - 0.006f, Col);
		};
		P.Arc(C, R, 5.0f, GaugeAng(-12.5f) + 0.006f, GaugeAng(12.5f) - 0.006f, Paper(0.26f));
		Band(12.5f, 35.0f, Fade(Green(), 0.45f));
		Band(35.0f, 55.0f, Fade(Orange(), 0.50f));
		Band(55.0f, 70.0f, Fade(Red(), 0.60f));
		// 当前角度
		const float Slip = FMath::Clamp(DispSlip, -GaugeMax, GaugeMax);
		const FLinearColor Col = AngleColor(FMath::Abs(Slip));
		P.Arc(C, R, 12.0f, GaugeAng(0.0f), GaugeAng(Slip), Fade(Col, 0.55f), Col);
		{
			const float A = GaugeAng(Slip);
			const V2 D(FMath::Cos(A), FMath::Sin(A)), T(-D.Y, D.X);
			const V2 Tip[3] = { C + D * (R + 9.0f), C + D * (R + 28.0f) + T * 10.0f, C + D * (R + 28.0f) - T * 10.0f };
			P.Poly(MakeArrayView(Tip, 3), Col);
			P.LineSeg(C + D * (R - 12.0f), C + D * (R + 12.0f), 3.5f, Paper());
		}
		P.LineSeg(C + V2(0, -R - 6.0f), C + V2(0, -R + 8.0f), 2.0f, Paper(0.7f));

		// 倍率底
		{
			const float Grow = 8.0f * MultPop;
			const FApexRect M(C.X - 52.0f - Grow, 216.0f - Grow * 0.5f, 104.0f + Grow * 2.0f, 42.0f + Grow);
			if (MultPop > 0.01f) P.GlowSkew(M, 26.0f, WithA(Accent, 0.6f * MultPop));
			P.Skew(M, Mix(Accent, Paper(), 0.6f * MultPop));
		}
		// 衔接条
		{
			const float G = FMath::Clamp(Tel.TransitionGraceNormalized, 0.0f, 1.0f);
			const bool bCount = G < 0.995f && G > 0.0f;
			const FApexRect B(C.X - 130.0f, 276.0f, 260.0f, 6.0f);
			P.Skew(B, Paper(0.18f), 0.5f);
			const FLinearColor BC = bCount ? (G > 0.4f ? Orange() : Red()) : Paper(0.75f);
			const float W = B.W * G;
			P.Skew(FApexRect(B.CX() - W * 0.5f, B.Y, W, B.H), BC, 0.5f);
		}
		if (Tel.bInClippingZone)
		{
			const float Pulse = 0.75f + 0.25f * FMath::Sin(TimeNow * 9.0f);
			P.Skew(FApexRect(C.X + 70.0f, 221.0f, 118.0f, 32.0f), WithA(Gold(), Pulse));
		}
		P.SetAlpha(Old);
	}

	void PaintDriftText(FApexPainter& P, const V2& S) const
	{
		const float Old = P.GetAlpha();
		P.SetAlpha(Old * DriftA);
		const V2 C = GaugeC(S);
		const int32 Sc = FMath::RoundToInt(DispScore);
		TDrift.Set(Sc, [Sc] { return FormatScore(Sc); });
		TDrift.Draw(P, C.X, 158.0f, Paper(), EApexH::Center, EApexV::Middle, 0.5f);
		const int32 Deg = FMath::RoundToInt(FMath::Abs(DispSlip));
		TDeg.Set(Deg, [Deg] { return FString::Printf(TEXT("%d°"), Deg); });
		TDeg.Draw(P, C.X + 4.0f, 92.0f, AngleColor(FMath::Abs(DispSlip)), EApexH::Center);
		const int32 M10 = FMath::RoundToInt(Tel.DriftMultiplier * 10.0f);
		TMult.Set(M10, [M10] { return FString::Printf(TEXT("×%d.%d"), M10 / 10, M10 % 10); });
		TMult.DrawScaled(P, C.X, 237.0f, 1.0f + 0.25f * MultPop, Ink(), 0.0f);
		if (Tel.bInClippingZone)
		{
			TClip.Draw(P, C.X + 129.0f, 237.0f, Ink(), EApexH::Center, EApexV::Middle, 0.0f);
		}
		const float G = Tel.TransitionGraceNormalized;
		if (G > 0.0f && G < 0.995f)
		{
			const int32 D10 = FMath::CeilToInt(G * 24.0f);
			TGrace.Set(D10, [D10] { return FString::Printf(TEXT("衔接 %d.%d 秒"), D10 / 10, D10 % 10); });
			TGrace.Draw(P, C.X, 302.0f, G > 0.4f ? Orange() : Red(), EApexH::Center);
		}
		P.SetAlpha(Old);
	}

	// =====================================================================
	// 区域名滑入 / 事件提示 / 逆行 / 按键提示 / 帧率 / 倒计时
	// =====================================================================
	float ZoneAlpha() const
	{
		if (ZoneT > 3.2f) return 0.0f;
		return FMath::Min(EaseOut(ZoneT / 0.3f), FMath::Clamp((3.2f - ZoneT) / 0.4f, 0.0f, 1.0f));
	}
	FApexRect ZoneRect() const
	{
		const float W = FMath::Max(250.0f, FMath::Max(TZone.Size.X, TZoneSub.Size.X) + 92.0f);
		return FApexRect(58.0f - 60.0f * (1.0f - EaseOut(ZoneT / 0.3f)), 262.0f, W, 74.0f);
	}
	void PaintZoneShapes(FApexPainter& P, const V2& S) const
	{
		const float A = ZoneAlpha();
		if (A < 0.01f) return;
		const FApexRect R = ZoneRect();
		P.SkewH(R, Ink(0.80f * A), Ink(0.40f * A));
		P.Skew(FApexRect(R.X, R.Y, 7.0f, R.H), WithA(Accent, A));
	}
	void PaintZoneText(FApexPainter& P, const V2& S) const
	{
		const float A = ZoneAlpha();
		if (A < 0.01f) return;
		const FApexRect R = ZoneRect();
		TZone.Draw(P, R.X + 34.0f, R.Y + 29.0f, Paper(A));
		TZoneSub.Draw(P, R.X + 30.0f, R.Y + 57.0f, WithA(Accent, A), EApexH::Left, EApexV::Middle, 0.0f);
	}

	void PaintCallout(FApexPainter& P, const V2& S) const
	{
		const float A = FMath::Clamp(Tel.CalloutAlpha * 1.6f, 0.0f, 1.0f);
		if (A < 0.02f || TCall.S.IsEmpty())
		{
			return;
		}
		const float In = EaseOut(CalloutT / 0.22f);
		const float Y = (DriftA > 0.3f) ? 352.0f : 300.0f;
		const float X = S.X * 0.5f + 46.0f * (1.0f - In);
		const float W = FMath::Max(TCall.Size.X, TCallSub.Size.X) + 120.0f;
		const FLinearColor C = bCalloutBad ? Red() : Gold();
		const float Old = P.GetAlpha();
		P.SetAlpha(Old * A * In);
		P.RectH(FApexRect(X - W * 0.5f, Y - 34.0f, W * 0.5f, 80.0f), InkDeep(0.0f), InkDeep(0.66f));
		P.RectH(FApexRect(X, Y - 34.0f, W * 0.5f, 80.0f), InkDeep(0.66f), InkDeep(0.0f));
		TCall.Draw(P, X, Y, C, EApexH::Center, EApexV::Middle, 0.5f);
		TCallSub.Draw(P, X, Y + 31.0f, Paper(0.82f), EApexH::Center, EApexV::Middle, 0.3f);
		P.SetAlpha(Old);
	}

	FApexRect WrongRect(const V2& S) const { return FApexRect(S.X * 0.5f - 210.0f, 428.0f, 420.0f, 76.0f); }
	void PaintWrongShapes(FApexPainter& P, const V2& S) const
	{
		if (WrongA < 0.02f) return;
		const FApexRect R = WrongRect(S);
		const float F = 0.93f + 0.07f * FMath::Sin(TimeNow * 10.0f);
		P.GlowSkew(R, 28.0f, WithA(Red(), 0.30f * WrongA));
		P.Skew(R, WithA(Hex(0xD81E35), 0.94f * WrongA * F));
		for (int32 i = 0; i < 2; ++i)
		{
			const float Dir = i == 0 ? 1.0f : -1.0f;
			const float BX = i == 0 ? R.X + 40.0f : R.R() - 40.0f;
			for (int32 k = 0; k < 2; ++k)
			{
				const float X = BX + Dir * k * 18.0f;
				const V2 Pts[3] = { { X + Dir * 9.0f, R.CY() - 16.0f }, { X - Dir * 7.0f, R.CY() }, { X + Dir * 9.0f, R.CY() + 16.0f } };
				P.Stroke(MakeArrayView(Pts, 3), 5.0f, Paper(WrongA * (0.55f + 0.45f * FMath::Sin(TimeNow * 8.0f - k * 1.2f))));
			}
		}
	}
	void PaintWrongText(FApexPainter& P, const V2& S) const
	{
		if (WrongA < 0.02f) return;
		const FApexRect R = WrongRect(S);
		TWrong.Draw(P, R.CX(), R.CY() - 10.0f, Paper(WrongA), EApexH::Center, EApexV::Middle, 0.0f);
		TWrongSub.Draw(P, R.CX(), R.CY() + 23.0f, Paper(0.85f * WrongA), EApexH::Center, EApexV::Middle, 0.0f);
	}

	float HintAlpha() const
	{
		if (State != EApexGameState::Playing && State != EApexGameState::Countdown) return 0.0f;
		if (Tel.bAutoRun || (PC.IsValid() && PC->IsAutoPending())) return 0.0f;
		const float T = State == EApexGameState::Countdown ? 0.5f : HintT;
		return FMath::Clamp((8.0f - T) / 1.2f, 0.0f, 1.0f);
	}
	static constexpr int32 NumHints = 8;
	float HintWidth() const
	{
		float W = 0.0f;
		for (int32 i = 0; i < NumHints; ++i)
		{
			W += FMath::Max(30.0f, THintKey[i].Size.X + 18.0f) + 8.0f + THintLab[i].Size.X + (i + 1 < NumHints ? 26.0f : 0.0f);
		}
		return W;
	}
	void PaintHintShapes(FApexPainter& P, const V2& S) const
	{
		const float A = HintAlpha();
		if (A < 0.01f) return;
		const float W = HintWidth();
		const float X = S.X * 0.5f - W * 0.5f, CY = S.Y - 62.0f;
		P.Skew(FApexRect(X - 30.0f, CY - 27.0f, W + 60.0f, 54.0f), Ink(0.66f * A));
		float Cur = X;
		for (int32 i = 0; i < NumHints; ++i)
		{
			const float KW = FMath::Max(30.0f, THintKey[i].Size.X + 18.0f);
			P.RoundRect(FApexRect(Cur, CY - 15.0f, KW, 30.0f), 6.0f, Paper(0.94f * A));
			Cur += KW + 8.0f + THintLab[i].Size.X + 26.0f;
		}
	}
	void PaintHintText(FApexPainter& P, const V2& S) const
	{
		const float A = HintAlpha();
		if (A < 0.01f) return;
		const float W = HintWidth();
		float Cur = S.X * 0.5f - W * 0.5f;
		const float CY = S.Y - 62.0f;
		for (int32 i = 0; i < NumHints; ++i)
		{
			const float KW = FMath::Max(30.0f, THintKey[i].Size.X + 18.0f);
			THintKey[i].Draw(P, Cur + KW * 0.5f, CY, Ink(A), EApexH::Center, EApexV::Middle, 0.0f);
			THintLab[i].Draw(P, Cur + KW + 8.0f, CY, Paper(0.92f * A), EApexH::Left, EApexV::Middle, 0.0f);
			Cur += KW + 8.0f + THintLab[i].Size.X + 26.0f;
		}
	}

	// 自动漂移标识 + 进度
	void PaintAuto(FApexPainter& P, const V2& S) const
	{
		const bool bPending = PC.IsValid() && PC->IsAutoPending();
		if (!Tel.bAutoRun && !bPending) return;
		const float W = 520.0f, CY = S.Y - 70.0f;
		const FApexRect R(S.X * 0.5f - W * 0.5f, CY - 32.0f, W, 64.0f);
		P.Skew(R, Ink(0.70f));
		P.Skew(FApexRect(R.X, R.Y, 8.0f, R.H), Accent, Slant);
		const float Pulse = 0.55f + 0.45f * FMath::Sin(TimeNow * 5.0f);
		P.Circle(V2(R.X + 38.0f, CY - 8.0f), 6.0f, Fade(Red(), bPending ? 0.4f : Pulse));
		P.Text(TEXT("自动漂移"), R.X + 56.0f, CY - 8.0f, EApexFont::Display, 25.0f, Paper());
		P.Text(Tel.bAutoDirector ? TEXT("任意驾驶键接管 · V 关运镜 · 跑完复位") : TEXT("任意驾驶键接管 · V 开运镜 · 跑完复位"), R.R() - 22.0f, CY - 8.0f, EApexFont::Body, 16.0f, Paper(0.82f), EApexH::Right);
		const FApexRect Bar(R.X + 30.0f, CY + 15.0f, W - 64.0f, 5.0f);
		P.Skew(Bar, Paper(0.18f), 0.34f);
		P.Skew(FApexRect(Bar.X, Bar.Y, FMath::Max(4.0f, Bar.W * Tel.AutoProgress), Bar.H), Accent, 0.34f);
	}

	void PaintTrainHint(FApexPainter& P, const V2& S) const
	{
		if (!PC.IsValid() || !PC->IsFreeRoamMap() || State != EApexGameState::Playing || Tel.bAutoRun) return;
		const float T = PC->GetPlayingTime();
		const float A = FMath::Clamp(T / 0.4f, 0.0f, 1.0f) * FMath::Clamp((9.0f - T) / 1.0f, 0.0f, 1.0f);
		if (A <= 0.01f) return;
		const float W = 620.0f, CY = S.Y - 128.0f;
		const FApexRect R(S.X * 0.5f - W * 0.5f, CY - 26.0f, W, 52.0f);
		P.Skew(R, Ink(0.66f * A));
		P.Skew(FApexRect(R.X, R.Y, 8.0f, R.H), Fade(Accent, A), Slant);
		P.Text(TEXT("训练场"), R.X + 32.0f, CY, EApexFont::Display, 24.0f, Paper(A));
		P.Text(TEXT("随便开 · 按一次复位原地扶正 · 连按两次回起点并复原道具"), R.R() - 22.0f, CY, EApexFont::Body, 17.0f, Paper(0.86f * A), EApexH::Right);
	}

	void PaintFps(FApexPainter& P, const V2& S) const
	{
		if (!PC.IsValid() || !PC->GetSaveData() || !PC->GetSaveData()->bShowFps)
		{
			return;
		}
		const int32 Fps = FMath::RoundToInt(Tel.SmoothedFps);
		TFps.Set(Fps, [Fps] { return FString::FromInt(Fps); });
		const FLinearColor C = Fps >= 115 ? Green() : (Fps >= 60 ? Gold() : Red());
		TFpsCap.Draw(P, S.X - 44.0f, 54.0f, TextMid(), EApexH::Right, EApexV::Baseline);
		TFps.Draw(P, S.X - 44.0f - TFpsCap.Size.X - 8.0f, 54.0f, C, EApexH::Right, EApexV::Baseline);
	}

	void PaintCountdown(FApexPainter& P, const V2& S) const
	{
		const V2 C(S.X * 0.5f, S.Y * 0.40f);
		if (State == EApexGameState::Countdown)
		{
			const int32 Step = FMath::CeilToInt(Countdown);
			if (Step > 3)
			{
				TReady.Draw(P, C.X, C.Y, Paper(), EApexH::Center, EApexV::Middle, 0.5f);
				return;
			}
			const float F = 1.0f - (Countdown - FMath::FloorToFloat(Countdown)); // 本数字内 0..1
			const float T = (Step <= 0) ? 1.0f : F;
			const float K = 1.0f + 0.7f * (1.0f - EaseOut(T / 0.28f));
			const float A = FMath::Min(EaseOut(T / 0.10f), FMath::Clamp((1.02f - T) / 0.22f, 0.0f, 1.0f));
			const FLinearColor Col = Step == 1 ? Orange() : Paper();
			P.CircleGrad(C, 250.0f, InkDeep(0.42f), InkDeep(0.0f));
			P.Ring(C, 148.0f, 5.0f, Paper(0.18f));
			P.Arc(C, 148.0f, 7.0f, -HALF_PI, -HALF_PI + 2.0f * PI * (1.0f - T), Accent);
			const float RingT = EaseOut(T / 0.55f);
			P.Ring(C, 148.0f + 120.0f * RingT, 4.0f, WithA(Accent, 0.55f * (1.0f - RingT)));
			TCount.Set(Step, [Step] { return FString::FromInt(FMath::Max(1, Step)); });
			TCount.DrawScaled(P, C.X, C.Y, K, WithA(Col, A), 0.4f);
		}
		else if (State == EApexGameState::Playing && GoT >= 0.0f && GoT < 1.1f)
		{
			const float K = 0.7f + 0.3f * EaseOutBack(GoT / 0.25f) + 0.25f * FMath::Max(0.0f, GoT - 0.6f);
			const float A = FMath::Clamp((1.1f - GoT) / 0.45f, 0.0f, 1.0f);
			const float RingT = EaseOut(GoT / 0.7f);
			P.Ring(C, 120.0f + 380.0f * RingT, 8.0f, WithA(Accent, 0.6f * (1.0f - RingT)));
			TGo.DrawScaled(P, C.X, C.Y, K, WithA(Mix(Accent, Paper(), 0.35f), A), 0.45f);
		}
	}

	// =====================================================================
	void InitTexts()
	{
		TSpeed.Init(EApexFont::NumHeavy, 112.0f);
		TKmh.Init(EApexFont::Num, 23.0f); TKmh.SetStr(TEXT("km/h"));
		TGear.Init(EApexFont::NumHeavy, 56.0f);
		TGearMode.Init(EApexFont::Body, 17.0f);
		THand.Init(EApexFont::BodyBold, 17.0f); THand.SetStr(TEXT("手刹"));
		TBoost.Init(EApexFont::Body, 16.0f);
		for (int32 i = 0; i < 13; ++i)
		{
			TTick[i].Init(EApexFont::Num, 18.0f);
			TTick[i].SetStr(FString::FromInt(i));
		}
		TMode.Init(EApexFont::Display, 27.0f);
		TBig.Init(EApexFont::NumHeavy, 76.0f);
		TLapNow.Init(EApexFont::Num, 42.0f);
		TLapCap.Init(EApexFont::Body, 17.0f);
		TLabA.Init(EApexFont::Body, 17.0f);
		TLabB.Init(EApexFont::Body, 17.0f);
		TValA.Init(EApexFont::Num, 27.0f);
		TValB.Init(EApexFont::Num, 27.0f);
		TDrift.Init(EApexFont::NumHeavy, 96.0f);
		TDeg.Init(EApexFont::Num, 32.0f);
		TMult.Init(EApexFont::NumHeavy, 34.0f);
		TClip.Init(EApexFont::BodyBold, 17.0f); TClip.SetStr(TEXT("贴弯 x1.65"));
		TGrace.Init(EApexFont::BodyBold, 17.0f);
		TZone.Init(EApexFont::Display, 33.0f);
		TZoneSub.Init(EApexFont::Num, 17.0f);
		TCall.Init(EApexFont::Display, 42.0f);
		TCallSub.Init(EApexFont::Num, 20.0f);
		TWrong.Init(EApexFont::Display, 36.0f); TWrong.SetStr(TEXT("方向错误"));
		TWrongSub.Init(EApexFont::Body, 18.0f);
		TFps.Init(EApexFont::Num, 30.0f);
		TFpsCap.Init(EApexFont::Num, 18.0f); TFpsCap.SetStr(TEXT("FPS"));
		TReady.Init(EApexFont::Display, 92.0f); TReady.SetStr(TEXT("准备"));
		TCount.Init(EApexFont::NumHeavy, 220.0f);
		TGo.Init(EApexFont::NumHeavy, 220.0f); TGo.SetStr(TEXT("GO!"));
		static const TCHAR* Labs[NumHints] = { TEXT("油门"), TEXT("转向"), TEXT("手刹起漂"), TEXT("刹车"), TEXT("视角"), TEXT("复位"), TEXT("自动漂移"), TEXT("暂停") };
		for (int32 i = 0; i < NumHints; ++i)
		{
			THintKey[i].Init(EApexFont::BodyBold, 17.0f);
			THintLab[i].Init(EApexFont::Body, 18.0f); THintLab[i].SetStr(Labs[i]);
		}
	}

	/** 提示里的按键名跟着改键走 */
	void RefreshKeyTexts()
	{
		KeysVer = ApexKeys::Version();
		using ApexKeys::Name;
		const FString Keys[NumHints] = {
			Name(EApexBind::Throttle), Name(EApexBind::SteerLeft) + TEXT(" ") + Name(EApexBind::SteerRight), Name(EApexBind::Handbrake),
			Name(EApexBind::Brake), Name(EApexBind::Camera), Name(EApexBind::Reset), Name(EApexBind::AutoRun), TEXT("Esc") };
		for (int32 i = 0; i < NumHints; ++i)
		{
			THintKey[i].SetStr(Keys[i]);
		}
		TWrongSub.SetStr(FString::Printf(TEXT("请掉头，或按 %s 复位到赛道"), *Name(EApexBind::Reset)));
	}
	uint32 KeysVer = 0;

	TWeakObjectPtr<AApexDriftPlayerController> PC;
	FApexDriftTelemetry Tel;
	EApexGameModeType Mode = EApexGameModeType::FreeDrift;
	EApexGameState State = EApexGameState::MainMenu;
	float Countdown = 3.0f;
	TArray<V2> MapPts;
	TArray<bool> ZoneMask;
	mutable TArray<V2> Work;
	mutable TArray<V2> Piece;
	mutable FLinearColor Accent = FLinearColor::White;

	float TimeNow = 0.0f, HintT = 0.0f, GoT = -1.0f, ZoneT = 100.0f, CalloutT = 100.0f;
	float DriftA = 0.0f, DispScore = 0.0f, DispTotal = 0.0f, DispRpm = 0.0f, DispSlip = 0.0f;
	float DispThr = 0.0f, DispBrk = 0.0f, DispBoost = 0.0f, WrongA = 0.0f, MultPop = 0.0f, LastMult = 1.0f;
	FString ZoneKey, CalloutKey;
	bool bCalloutBad = false;

	mutable FHudText TSpeed, TKmh, TGear, TGearMode, THand, TBoost, TTick[13];
	mutable FHudText TMode, TBig, TLapNow, TLapCap, TLabA, TLabB, TValA, TValB;
	mutable FHudText TDrift, TDeg, TMult, TClip, TGrace, TZone, TZoneSub, TCall, TCallSub;
	mutable FHudText TWrong, TWrongSub, TFps, TFpsCap, TReady, TCount, TGo;
	mutable FHudText THintKey[NumHints], THintLab[NumHints];
#if !UE_BUILD_SHIPPING
	mutable uint64 ProfCycles = 0;
	mutable int32 ProfFrames = 0;
#endif
};

TSharedRef<SWidget> UApexHudWidget::RebuildWidget()
{
	HudCanvas = SNew(SApexHudCanvas);
	return HudCanvas.ToSharedRef();
}

void UApexHudWidget::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	HudCanvas.Reset();
}

void UApexHudWidget::UpdateTelemetry(const FApexDriftTelemetry& InTelemetry, EApexGameModeType InMode, EApexGameState InState, float InCountdownSec, const TArray<FVector2D>& InTrackMinimapPts)
{
	if (HudCanvas.IsValid())
	{
		HudCanvas->SetData(GetOwningPlayer<AApexDriftPlayerController>(), InTelemetry, InMode, InState, InCountdownSec, InTrackMinimapPts);
	}
}

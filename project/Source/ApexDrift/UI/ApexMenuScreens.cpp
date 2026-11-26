#include "UI/ApexMenuScreens.h"
#include "Core/ApexDriftPlayerController.h"
#include "Core/ApexDriftSaveGame.h"
#include "Core/ApexMapRegistry.h"
#include "Core/ApexPaintRegistry.h"
#include "Vehicle/ApexCarRegistry.h"
#include "Vehicle/ApexDriftVehiclePawn.h"
#include "Kismet/KismetSystemLibrary.h"

using namespace ApexUI;

namespace
{
	const FApexMapInfo& MapInfo(int32 i)
	{
		return FApexMapRegistry::GetMaps()[FApexMapRegistry::ClampIndex(i)];
	}

	FApexMapRecord RecordOf(const UApexDriftSaveGame* Save, int32 Map)
	{
		if (Save)
		{
			if (const FApexMapRecord* R = Save->MapRecords.Find(MapInfo(Map).Id))
			{
				return *R;
			}
		}
		return FApexMapRecord();
	}

	/** 中文大数字：整数部分 */
	FString KmText(float Km) { return FString::Printf(TEXT("%.1f km"), Km); }

	float SkewAt(const FApexRect& R, float Y) { return Slant * (R.CY() - Y); }
}

namespace ApexUI
{
	FLinearColor MapAccent(int32 Map) { return MapInfo(Map).Accent; }

	void Backdrop(FApexPainter& P, const FVector2f& S, float BlurStrength, float InkA)
	{
		P.Blur(FApexRect(0, 0, S.X, S.Y), BlurStrength);
		P.RectV(FApexRect(-2, -2, S.X + 4, S.Y + 4), Ink(InkA * 0.92f), InkDeep(InkA));
	}

	void ScreenTitle(FApexPainter& P, float X, float CY, const FString& Zh, const FString& En, const FLinearColor& Accent, float TitlePx)
	{
		P.Skew(FApexRect(X + 2.0f, CY - TitlePx * 0.42f, 9.0f, TitlePx * 0.84f), Accent, Slant * 1.6f);
		const float W = P.Text(Zh, X + 30.0f, CY, EApexFont::Display, TitlePx, TextHi());
		P.Text(En, X + 30.0f + W + 20.0f, CY + TitlePx * 0.36f, EApexFont::Num, TitlePx * 0.36f, Fade(Accent, 0.95f), EApexH::Left, EApexV::Baseline, 140);
	}

	void PrimaryButton(FApexPainter& P, const FApexRect& R, const FString& Zh, const FString& Sub, const FLinearColor& Accent, float Hover, float Press, float Time, float FontPx)
	{
		const float Grow = 4.0f * Hover - 3.0f * Press;
		const FApexRect B(R.X - Grow, R.Y - Grow * 0.5f, R.W + Grow * 2, R.H + Grow);
		P.GlowSkew(B, 26.0f + 10.0f * Hover, WithA(Accent, 0.26f + 0.22f * Hover));
		P.SkewH(B, Mix(Accent, Paper(), 0.10f + 0.16f * Hover), Mix(Accent, Paper(), 0.34f + 0.20f * Hover));
		// 流光
		{
			const float T = FMath::Frac(Time * 0.38f);
			const float SX = B.X + (B.W + 120.0f) * T - 90.0f;
			const float X0 = FMath::Max(B.X, SX), X1 = FMath::Min(B.R(), SX + 60.0f);
			if (X1 - X0 > 2.0f)
			{
				P.SetShear(Slant, B.CY());
				const float Mid = (X0 + X1) * 0.5f;
				P.RectH(FApexRect(X0, B.Y, Mid - X0, B.H), Paper(0.0f), Paper(0.34f));
				P.RectH(FApexRect(Mid, B.Y, X1 - Mid, B.H), Paper(0.34f), Paper(0.0f));
				P.ClearShear();
			}
		}
		const bool bSub = !Sub.IsEmpty();
		const float TX = B.X + 34.0f;
		P.Text(Zh, TX, B.CY() - (bSub ? B.H * 0.13f : 0.0f), EApexFont::Display, FontPx, Ink());
		if (bSub)
		{
			P.TextFit(Sub, TX - 4.0f, B.CY() + B.H * 0.27f, EApexFont::BodyBold, 15.0f, B.W - 120.0f, Ink(0.72f));
		}
		// 箭头
		for (int32 i = 0; i < 3; ++i)
		{
			const float Ph = FMath::Frac(Time * 1.2f - i * 0.22f);
			const float A = 0.30f + 0.70f * FMath::Clamp(1.0f - Ph * 1.6f, 0.0f, 1.0f);
			const float AX = B.R() - 76.0f + i * 19.0f + 4.0f * Hover;
			const FVector2f Pts[3] = { { AX, B.CY() - 13.0f }, { AX + 13.0f, B.CY() }, { AX, B.CY() + 13.0f } };
			P.Stroke(MakeArrayView(Pts, 3), 4.5f, Ink(A));
		}
	}

	void GhostButton(FApexPainter& P, const FApexRect& R, const FString& Zh, EApexIcon I, float Hover, float Press, float FontPx)
	{
		P.Skew(R, Mix(Paper(0.10f), Paper(0.94f), Hover));
		P.StrokeSkew(R, 1.5f, Paper(0.22f + 0.5f * Hover));
		const FLinearColor C = Mix(TextHi(), Ink(), Hover);
		float TX = R.CX();
		if (I != EApexIcon::None)
		{
			const float W = Measure(Zh, Font(EApexFont::Display, FontPx)).X;
			const float Total = FontPx + 12.0f + W;
			Icon(P, I, FVector2f(R.CX() - Total * 0.5f + FontPx * 0.5f, R.CY()), FontPx * 0.92f, C, 2.2f);
			P.Text(Zh, R.CX() - Total * 0.5f + FontPx + 12.0f, R.CY(), EApexFont::Display, FontPx, C);
		}
		else
		{
			P.Text(Zh, TX, R.CY(), EApexFont::Display, FontPx, C, EApexH::Center);
		}
	}

	void ControlsPage(FApexPainter& P, const FVector2f& S, const FLinearColor& Accent, float Time)
	{
		Backdrop(P, S, 14.0f, 0.86f);
		const float Scale = FMath::Min3(1.0f, (S.X - 160.0f) / GuideW, (S.Y - 250.0f) / GuideH);
		const float X = (S.X - GuideW * Scale) * 0.5f;
		ScreenTitle(P, X, 92.0f, TEXT("操作说明"), TEXT("CONTROLS"), Accent, 56.0f);
		P.Flush();
		const int32 L = P.GetLayer() + 1;
		const FGeometry Child = P.GetGeometry().MakeChild(FVector2f(GuideW, GuideH), FSlateLayoutTransform(Scale, FVector2f(X, 160.0f)));
		int32 OutL = L;
		{
			FApexPainter C(Child, P.GetOut(), L, P.GetAlpha());
			ControlsGuide(C, 0.0f, 0.0f, Accent, Time);
			OutL = C.Finish();
		}
		P.SetLayer(OutL + 1);
	}
}

// =========================================================================
// 主菜单
// =========================================================================

void SApexMainMenu::ResetSelection()
{
	SelMap = CurrentMap();
	HeroMap = SelMap;
	HeroT = 1.0f;
	SelMode = Save.IsValid() ? Save->LastMode : EApexGameModeType::FreeDrift;
	bAccentInit = false;
}

void SApexMainMenu::OnShown()
{
	Modal = EModal::None;
	// 回到菜单时，场景就是当前地图
	SelMap = CurrentMap();
	HeroMap = SelMap;
}

int32 SApexMainMenu::DefaultFocus() const
{
	if (Modal == EModal::Controls) return IdCloseControls;
	if (Modal == EModal::Quit) return IdQuitNo;
	if (GFocusMapsOnNextMenu)
	{
		GFocusMapsOnNextMenu = false;
		return IdMap0 + (SelMap + 1) % FApexMapRegistry::NumMaps();
	}
	return IdStart;
}

int32 SApexMainMenu::CurrentMap() const
{
	return PC.IsValid() ? FApexMapRegistry::ClampIndex(PC->GetCurrentMapIndex()) : 0;
}

int32 SApexMainMenu::WeatherOf(int32 Map) const
{
	if (PC.IsValid() && Map == CurrentMap())
	{
		return FMath::Clamp(PC->GetCurrentWeatherIndex(), 0, 2);
	}
	return Save.IsValid() ? Save->GetWeatherForMap(MapInfo(Map).Id) : 0;
}

void SApexMainMenu::SelectMap(int32 Map)
{
	if (Map != SelMap)
	{
		SelMap = Map;
		HeroT = 0.0f;
		HeroMap = Map;
		Click();
	}
	else if (bNavMode)
	{
		// 已选中的卡片再次确认：直接把焦点送到“开始比赛”
		SetFocusId(IdStart, true);
	}
}

void SApexMainMenu::SelectWeather(int32 Idx)
{
	if (PC.IsValid() && SelMap == CurrentMap())
	{
		PC->SetWeatherIndex(Idx);
	}
	else if (Save.IsValid())
	{
		// 不是当前加载的地图：只记录，进图时生效
		Save->MapWeather.Add(MapInfo(SelMap).Id, Idx);
		Save->SaveToDisk();
		Click();
	}
}

SApexMainMenu::FLayout SApexMainMenu::Layout(const FVector2f& S) const
{
	FLayout L;
	L.X0 = FMath::Max(64.0f, (S.X - 2200.0f) * 0.5f);
	L.X1 = S.X - L.X0;
	L.DockH = 396.0f;
	L.DockY = S.Y - L.DockH;

	const int32 NMap = FApexMapRegistry::NumMaps();
	const float CardsW = FMath::Min(980.0f, (S.X - 2.0f * L.X0) * 0.53f), CardH = 262.0f, Gap = NMap > 3 ? 16.0f : 24.0f;
	const float CardW = (CardsW - Gap * (NMap - 1)) / NMap;
	for (int32 i = 0; i < NMap; ++i)
	{
		L.Card.Add(FApexRect(L.X0 + 14.0f + i * (CardW + Gap), L.DockY + 58.0f, CardW, CardH));
	}
	L.RX = L.Card.Last().R() + 62.0f;
	L.RW = L.X1 - L.RX;
	const float TileGap = 18.0f;
	const float TileW = (L.RW - TileGap * 2.0f - 12.0f) / 3.0f;
	for (int32 i = 0; i < 3; ++i)
	{
		L.Mode[i] = FApexRect(L.RX + 6.0f + i * (TileW + TileGap), L.DockY + 58.0f, TileW, 82.0f);
		L.Weather[i] = FApexRect(L.RX + 6.0f + i * (TileW + TileGap), L.DockY + 186.0f, TileW, 50.0f);
	}
	L.Start = FApexRect(L.X1 - 338.0f - 8.0f, L.DockY + 258.0f, 338.0f, 84.0f);
	const int32 NPaint = FApexPaintRegistry::Num();
	const float PaintW = L.Start.X - 34.0f - L.RX;
	const float Step = FMath::Min(48.0f, PaintW / FMath::Max(1, NPaint));
	for (int32 i = 0; i < NPaint; ++i)
	{
		L.Paint.Add(FVector2f(L.RX + 18.0f + i * Step, L.DockY + 320.0f));
	}
	L.PaintR = FMath::Min(16.0f, Step * 0.36f);

	float NX = L.X1;
	const float NavW[3] = { 124.0f, 162.0f, 124.0f };
	for (int32 i = 2; i >= 0; --i)
	{
		NX -= NavW[i];
		L.Nav[i] = FApexRect(NX, 28.0f, NavW[i] - 10.0f, 46.0f);
	}
	L.Auto = FApexRect(NX - 190.0f, 28.0f, 176.0f, 46.0f);
	L.Preview = FApexRect(L.X0 + 10.0f, FMath::Min(L.DockY - 110.0f, 486.0f), 214.0f, 52.0f);
	L.CarPanel = FApexRect(L.X1 - 336.0f, 138.0f, 336.0f, 440.0f);
	L.CarPrev = FApexRect(L.CarPanel.X + 6.0f, L.CarPanel.B() - 56.0f, 156.0f, 54.0f);
	L.CarNext = FApexRect(L.CarPanel.X + 174.0f, L.CarPanel.B() - 56.0f, 156.0f, 54.0f);
	return L;
}

void SApexMainMenu::BuildItems(const FVector2f& S)
{
	if (Modal == EModal::Controls)
	{
		Put(IdCloseControls, FApexRect(S.X * 0.5f - 130.0f, S.Y - 92.0f, 260.0f, 56.0f));
		return;
	}
	if (Modal == EModal::Quit)
	{
		Put(IdQuitNo, FApexRect(S.X * 0.5f - 236.0f, S.Y * 0.5f + 46.0f, 220.0f, 58.0f));
		Put(IdQuitYes, FApexRect(S.X * 0.5f + 16.0f, S.Y * 0.5f + 46.0f, 220.0f, 58.0f));
		return;
	}
	const FLayout L = Layout(S);
	for (int32 i = 0; i < L.Card.Num(); ++i)
	{
		Put(IdMap0 + i, L.Card[i], SelMap == i);
	}
	for (int32 i = 0; i < 3; ++i)
	{
		Put(IdMode0 + i, L.Mode[i], static_cast<int32>(SelMode) == i);
		Put(IdWeather0 + i, L.Weather[i], WeatherOf(SelMap) == i);
		Put(IdNavSettings + i, L.Nav[i]);
	}
	const int32 PaintSel = Save.IsValid() ? FApexPaintRegistry::Wrap(Save->PaintPresetIndex) : 0;
	const float PR = L.PaintR + 7.0f;
	for (int32 i = 0; i < L.Paint.Num(); ++i)
	{
		Put(IdPaint0 + i, FApexRect(L.Paint[i].X - PR, L.Paint[i].Y - PR, PR * 2, PR * 2), PaintSel == i);
	}
	if (SelMap != CurrentMap())
	{
		Put(IdPreview, L.Preview);
	}
	Put(IdAuto, L.Auto);
	Put(IdCarPrev, L.CarPrev);
	Put(IdCarNext, L.CarNext);
	Put(IdStart, L.Start);
}

void SApexMainMenu::TickScreen(float Dt)
{
	const FLinearColor Target = MapAccent(SelMap);
	if (!bAccentInit)
	{
		Accent = Target;
		bAccentInit = true;
	}
	Accent = Mix(Accent, Target, 1.0f - FMath::Exp(-Dt * 9.0f));
	if (FLinearColor::Dist(Accent, Target) < 0.02f) Accent = Target;
	HeroT = FMath::Min(1.0f, HeroT + Dt * 3.2f);
	CarT = FMath::Min(1.0f, CarT + Dt * 3.6f);
	const FApexCarInfo& CarInfo = FApexCarRegistry::Get(CurrentCar());
	for (int32 i = 0; i < 4; ++i) CarStat[i] = Approach(CarStat[i], CarInfo.Stats[i], Dt, 8.0f);
	ModalT = Approach(ModalT, Modal == EModal::None ? 0.0f : 1.0f, Dt, 14.0f);
}

void SApexMainMenu::OnActivate(int32 Id, const FVector2f& LocalPos, bool bMouse)
{
	if (Id >= IdMap0 && Id < IdMap0 + 3) { SelectMap(Id - IdMap0); return; }
	if (Id >= IdMode0 && Id < IdMode0 + 3)
	{
		SelMode = static_cast<EApexGameModeType>(Id - IdMode0);
		if (Save.IsValid()) Save->LastMode = SelMode;
		Click();
		return;
	}
	if (Id >= IdWeather0 && Id < IdWeather0 + 3) { SelectWeather(Id - IdWeather0); return; }
	if (Id >= IdPaint0 && Id < IdPaint0 + 16)
	{
		if (PC.IsValid()) PC->SetVehiclePaint(Id - IdPaint0);
		return;
	}
	switch (Id)
	{
	case IdStart:
		if (PC.IsValid()) PC->StartRaceOnMap(SelMap, SelMode);
		break;
	case IdAuto:
		if (PC.IsValid()) PC->StartAutoRunOnMap(SelMap);
		break;
	case IdCarPrev:
		StepCar(-1);
		break;
	case IdCarNext:
		StepCar(1);
		break;
	case IdPreview:
		if (PC.IsValid()) PC->PreviewMap(SelMap);
		break;
	case IdNavSettings:
		if (PC.IsValid()) PC->OpenSettingsMenu();
		break;
	case IdNavControls:
		Click();
		Modal = EModal::Controls;
		FocusId = IdCloseControls;
		break;
	case IdNavQuit:
		Click();
		Modal = EModal::Quit;
		FocusId = IdQuitNo;
		break;
	case IdCloseControls:
		Click();
		Modal = EModal::None;
		FocusId = IdNavControls;
		break;
	case IdQuitNo:
		Click();
		Modal = EModal::None;
		FocusId = IdNavQuit;
		break;
	case IdQuitYes:
		Click();
		if (PC.IsValid())
		{
			UKismetSystemLibrary::QuitGame(PC.Get(), PC.Get(), EQuitPreference::Quit, false);
		}
		break;
	default:
		break;
	}
}

bool SApexMainMenu::OnBack()
{
	Click();
	if (Modal == EModal::None)
	{
		Modal = EModal::Quit;
		FocusId = IdQuitNo;
	}
	else
	{
		FocusId = (Modal == EModal::Controls) ? IdNavControls : IdStart;
		Modal = EModal::None;
	}
	return true;
}

bool SApexMainMenu::OnMenuKey()
{
	if (Modal == EModal::None && bGamepad && PC.IsValid())
	{
		PC->StartRaceOnMap(SelMap, SelMode);
	}
	return true;
}

bool SApexMainMenu::OnShoulder(int32 Dir)
{
	if (Modal == EModal::None)
	{
		const int32 N = FApexMapRegistry::NumMaps();
		SelectMap((SelMap + Dir + N) % N);
	}
	return true;
}

bool SApexMainMenu::OnTrigger(int32 Dir)
{
	if (Modal == EModal::None)
	{
		StepCar(Dir);
	}
	return true;
}

int32 SApexMainMenu::CurrentCar() const
{
	return Save.IsValid() ? FApexCarRegistry::Wrap(Save->VehicleIndex) : 0;
}

void SApexMainMenu::StepCar(int32 Dir)
{
	if (PC.IsValid())
	{
		PC->SetVehicleCar(CurrentCar() + Dir);
		CarT = 0.0f;
	}
}

bool SApexMainMenu::BlocksMouse(const FVector2f& P) const
{
	if (Modal == EModal::None)
	{
		const FLayout L = Layout(DesignSize);
		if (P.X > L.CarPanel.X - 10.0f && P.Y > L.CarPanel.B() - 70.0f && P.Y < L.CarPanel.B() + 10.0f) return true;
	}
	// 中部留给 3D 视口拖动环视
	return Modal != EModal::None || P.Y > DesignSize.Y - 396.0f;
}

void SApexMainMenu::PaintScreen(FApexPainter& P, const FVector2f& S) const
{
	const FLayout L = Layout(S);
	const float Base = P.GetAlpha();
	P.SetAlpha(Base * EaseOut(OpenT / 0.35f));
	PaintHero(P, S, L);
	PaintCar(P, S, L);
	PaintTop(P, S, L);
	PaintDock(P, S, L);
	P.SetAlpha(Base);
	if (Modal != EModal::None)
	{
		PaintModal(P, S);
	}
}

void SApexMainMenu::PaintTop(FApexPainter& P, const FVector2f& S, const FLayout& L) const
{
	const TPair<float, FLinearColor> Stops[4] = { { 0.0f, Ink(0.84f) }, { 0.46f, Ink(0.60f) }, { 0.74f, Ink(0.20f) }, { 1.0f, Ink(0.0f) } };
	P.RectStops(FApexRect(-2, -2, S.X + 4, 170.0f), MakeArrayView(Stops));
	const float CY = 51.0f;
	// 字标
	float X = L.X0;
	for (int32 i = 0; i < 3; ++i)
	{
		P.Skew(FApexRect(X + 4.0f + i * 11.0f, CY - 17.0f, 6.0f, 34.0f), Fade(Accent, 1.0f - i * 0.30f), 0.34f);
	}
	X += 48.0f;
	X += P.Text(TEXT("APEX DRIFT"), X, CY, EApexFont::NumHeavy, 46.0f, TextHi(), EApexH::Left, EApexV::Middle, 10) + 18.0f;
	P.Skew(FApexRect(X, CY - 14.0f, 2.0f, 28.0f), Paper(0.35f), 0.34f);
	P.Text(TEXT("日本漂移之旅"), X + 18.0f, CY + 1.0f, EApexFont::Display, 22.0f, TextMid(), EApexH::Left, EApexV::Middle, 40);

	{
		// 看自动漂移
		const FApexRect& R = L.Auto;
		const float H = HoverOf(IdAuto);
		P.Skew(R, Mix(WithA(Accent, 0.92f), Paper(0.96f), H));
		const float W = Measure(TEXT("自动漂移"), Font(EApexFont::Display, 22.0f)).X;
		Icon(P, EApexIcon::Play, FVector2f(R.CX() - W * 0.5f - 12.0f, R.CY()), 18.0f, Ink(), 2.4f);
		P.Text(TEXT("自动漂移"), R.CX() - W * 0.5f + 10.0f, R.CY(), EApexFont::Display, 22.0f, Ink());
		if (IsFocused(IdAuto)) FocusRing(P, R, Slant, 5.0f);
	}
	static const TCHAR* NavZh[3] = { TEXT("设置"), TEXT("操作说明"), TEXT("退出") };
	static const EApexIcon NavIc[3] = { EApexIcon::Gear, EApexIcon::Pad, EApexIcon::Exit };
	for (int32 i = 0; i < 3; ++i)
	{
		const FApexRect& R = L.Nav[i];
		const float H = HoverOf(IdNavSettings + i);
		if (H > 0.01f)
		{
			P.Skew(R, Paper(0.94f * H));
		}
		const FLinearColor C = Mix(TextHi(), Ink(), H);
		const float W = Measure(NavZh[i], Font(EApexFont::Display, 22.0f)).X;
		const float Total = 22.0f + 10.0f + W;
		Icon(P, NavIc[i], FVector2f(R.CX() - Total * 0.5f + 11.0f, R.CY()), 22.0f, C, 2.0f);
		P.Text(NavZh[i], R.CX() - Total * 0.5f + 32.0f, R.CY(), EApexFont::Display, 22.0f, C, EApexH::Left, EApexV::Middle, 0, 0.4f * (1.0f - H));
		if (IsFocused(IdNavSettings + i))
		{
			FocusRing(P, R, Slant, 5.0f);
		}
	}
}

void SApexMainMenu::PaintHero(FApexPainter& P, const FVector2f& S, const FLayout& L) const
{
	const FApexMapInfo& Info = MapInfo(HeroMap);
	// 左侧压暗，保证文字在任何天气下可读
	P.RectH(FApexRect(-2, -2, L.X0 + 760.0f, L.DockY + 4.0f), Ink(0.70f), Ink(0.0f));

	const float T = EaseOut(HeroT) * EaseOut((OpenT - 0.05f) / 0.45f);
	const float X = L.X0 - 26.0f * (1.0f - T);
	const float Old = P.GetAlpha();
	P.SetAlpha(Old * T);

	const float Y0 = 138.0f;
	Seal(P, FApexRect(X, Y0, 48.0f, 48.0f), MapSeal(HeroMap), Accent, Ink());
	P.Text(Info.Region, X + 66.0f, Y0 + 24.0f, EApexFont::BodyBold, 21.0f, TextHi(), EApexH::Left, EApexV::Middle, 60, 0.4f);

	P.Text(Info.NameZh, X - 4.0f, Y0 + 162.0f, EApexFont::Display, 112.0f, TextHi(), EApexH::Left, EApexV::Baseline, 10, 0.45f);
	P.Text(Info.NameEn, X + 2.0f, Y0 + 200.0f, EApexFont::Num, 27.0f, Accent, EApexH::Left, EApexV::Middle, 180, 0.35f);
	P.Text(Info.Tagline, X + 2.0f, Y0 + 244.0f, EApexFont::Body, 21.0f, Paper(0.92f), EApexH::Left, EApexV::Middle, 0, 0.5f);

	// 数据条
	const FApexMapRecord Rec = RecordOf(Save.Get(), HeroMap);
	struct FStat { const TCHAR* Label; FString Value; };
	const FStat Stats[3] = {
		{ TEXT("赛道全长"), KmText(Info.LengthKm) },
		{ TEXT("挑战最高分"), Rec.BestAttack > 0 ? FormatScore(Rec.BestAttack) : FString(TEXT("--")) },
		{ TEXT("最快单圈"), FormatTimeOrDash(Rec.BestLapSec) },
	};
	float SX = X + 4.0f;
	const float SY = Y0 + 298.0f;
	for (int32 i = 0; i < 3; ++i)
	{
		P.Skew(FApexRect(SX, SY - 22.0f, 3.0f, 46.0f), WithA(Accent, 0.9f), Slant);
		const float W1 = P.Text(Stats[i].Label, SX + 16.0f, SY - 14.0f, EApexFont::Body, 15.0f, TextMid(), EApexH::Left, EApexV::Middle, 0, 0.4f);
		const float W2 = P.Text(Stats[i].Value, SX + 15.0f, SY + 13.0f, EApexFont::Num, 28.0f, TextHi(), EApexH::Left, EApexV::Middle, 0, 0.4f);
		SX += FMath::Max(FMath::Max(W1, W2) + 48.0f, 138.0f);
	}
	P.SetAlpha(Old);

	if (SelMap != CurrentMap())
	{
		const FApexRect& R = L.Preview;
		const float H = HoverOf(IdPreview);
		P.Skew(R, Mix(Ink(0.72f), Paper(0.95f), H));
		P.StrokeSkew(R, 1.5f, Mix(Paper(0.45f), Paper(1.0f), H));
		const FLinearColor C = Mix(TextHi(), Ink(), H);
		Icon(P, EApexIcon::Eye, FVector2f(R.X + 42.0f, R.CY()), 26.0f, C, 2.0f);
		P.Text(TEXT("前往预览"), R.X + 70.0f, R.CY(), EApexFont::Display, 23.0f, C);
		Icon(P, EApexIcon::ArrowRight, FVector2f(R.R() - 30.0f + 3.0f * H, R.CY()), 15.0f, C, 2.4f);
		P.Text(FString::Printf(TEXT("背景仍是 %s，点这里切换实景"), *MapInfo(CurrentMap()).NameZh), R.R() + 24.0f, R.CY(), EApexFont::Body, 16.0f, TextMid(), EApexH::Left, EApexV::Middle, 0, 0.5f);
		if (IsFocused(IdPreview))
		{
			FocusRing(P, R);
		}
	}
}

void SApexMainMenu::PaintCar(FApexPainter& P, const FVector2f& S, const FLayout& L) const
{
	const int32 Cur = CurrentCar();
	const FApexCarInfo& C = FApexCarRegistry::Get(Cur);
	const FApexRect& R = L.CarPanel;
	// 右侧压暗，与左侧标题对称
	P.RectH(FApexRect(R.X - 250.0f, -2.0f, S.X - R.X + 254.0f, L.DockY + 4.0f), Ink(0.0f), Ink(0.66f));

	const float Old = P.GetAlpha();
	P.SetAlpha(Old * EaseOut((OpenT - 0.05f) / 0.45f));
	const float X = R.X;
	float Y = R.Y + 12.0f;
	SectionLabel(P, X, Y, TEXT(""), TEXT("选择车辆"), TEXT("GARAGE"), Accent);
	P.Text(FString::Printf(TEXT("%d / %d"), Cur + 1, FApexCarRegistry::Num()), R.R(), Y, EApexFont::Num, 22.0f, TextMid(), EApexH::Right);

	const float T = EaseOut(CarT);
	const float DX = 18.0f * (1.0f - T);
	const float A1 = P.GetAlpha();
	P.SetAlpha(A1 * T);
	P.TextFit(C.Name, X - 2.0f + DX, Y + 60.0f, EApexFont::NumHeavy, 66.0f, R.W, TextHi(), EApexH::Left, EApexV::Middle, 10);
	{
		const FApexRect TagRect(X + 6.0f + DX, Y + 100.0f, 22.0f + Measure(C.ClassZh, Font(EApexFont::Display, 19.0f)).X, 30.0f);
		P.Skew(TagRect, Accent);
		P.Text(C.ClassZh, TagRect.CX(), TagRect.CY(), EApexFont::Display, 19.0f, Ink(), EApexH::Center);
	}
	P.TextFit(C.Tagline, X + DX, Y + 156.0f, EApexFont::Body, 17.0f, R.W, Paper(0.92f));
	P.SetAlpha(A1);

	static const TCHAR* Labels[4] = { TEXT("极速"), TEXT("加速"), TEXT("抓地"), TEXT("漂移") };
	Y += 196.0f;
	for (int32 i = 0; i < 4; ++i)
	{
		const float CY = Y + i * 38.0f;
		P.Text(Labels[i], X, CY, EApexFont::Body, 17.0f, TextMid(), EApexH::Left, EApexV::Middle, 0, 0.4f);
		const FApexRect Bar(X + 56.0f, CY - 5.0f, R.W - 56.0f - (i == 0 ? 98.0f : 4.0f), 10.0f);
		P.Skew(Bar, Paper(0.16f), 0.34f);
		const float W = FMath::Max(6.0f, Bar.W * FMath::Clamp(CarStat[i], 0.0f, 1.0f));
		P.SkewH(FApexRect(Bar.X, Bar.Y, W, Bar.H), WithA(Accent, 0.75f), Accent, 0.34f);
		if (i == 0)
		{
			P.Text(FString::Printf(TEXT("%.0f km/h"), C.TopKmh), R.R(), CY, EApexFont::Num, 21.0f, TextHi(), EApexH::Right, EApexV::Middle, 0, 0.4f);
		}
	}

	// 车辆序号点
	const int32 N = FApexCarRegistry::Num();
	for (int32 i = 0; i < N; ++i)
	{
		const float W = (R.W - (N - 1) * 6.0f) / N;
		P.Skew(FApexRect(X + i * (W + 6.0f), L.CarPrev.Y - 20.0f, W, 5.0f), i == Cur ? Accent : Paper(0.22f), 0.34f);
	}
	for (int32 k = 0; k < 2; ++k)
	{
		const int32 Id = k == 0 ? IdCarPrev : IdCarNext;
		const FApexRect& B = k == 0 ? L.CarPrev : L.CarNext;
		const float H = HoverOf(Id);
		P.Skew(B, Mix(Ink(0.62f), Paper(0.95f), H));
		P.StrokeSkew(B, 1.4f, Mix(Paper(0.36f), Paper(1.0f), H));
		const FLinearColor TC = Mix(TextHi(), Ink(), H);
		Icon(P, k == 0 ? EApexIcon::ArrowLeft : EApexIcon::ArrowRight, FVector2f(k == 0 ? B.X + 32.0f - 3.0f * H : B.R() - 32.0f + 3.0f * H, B.CY()), 16.0f, TC, 2.6f);
		P.Text(k == 0 ? TEXT("上一辆") : TEXT("下一辆"), B.CX() + (k == 0 ? 12.0f : -12.0f), B.CY(), EApexFont::Display, 22.0f, TC, EApexH::Center);
		if (IsFocused(Id)) FocusRing(P, B);
	}
	P.SetAlpha(Old);
}

void SApexMainMenu::PaintCard(FApexPainter& P, const FLayout& L, int32 i) const
{
	const FApexMapInfo& Info = MapInfo(i);
	const float Sel = SelectOf(IdMap0 + i);
	const float Hov = HoverOf(IdMap0 + i);
	const float Press = PressOf(IdMap0 + i);
	const FApexRect R0 = L.Card[i];
	const float K = 1.0f + 0.035f * Sel + 0.012f * Hov - 0.012f * Press;
	const FApexRect R(R0.CX() - R0.W * K * 0.5f, R0.CY() - R0.H * K * 0.5f - 8.0f * Sel - 3.0f * Hov, R0.W * K, R0.H * K);
	const FLinearColor A = Info.Accent;

	P.GlowSkew(R, 22.0f, InkDeep(0.40f));
	if (Sel > 0.01f)
	{
		P.GlowSkew(R, 34.0f, WithA(A, 0.42f * Sel));
	}

	const float ImgH = FMath::RoundToFloat(R.H * 0.665f);
	const FApexRect Img(R.X, R.Y, R.W, ImgH);
	const FApexRect Strip(R.X, R.Y + ImgH, R.W, R.H - ImgH);
	P.SetShear(Slant, R.CY());
	MapArt(P, Img, i, FMath::Lerp(0.70f, 1.0f, FMath::Max(Sel, Hov)));
	P.RectV(FApexRect(Img.X, Img.Y + ImgH * 0.42f, Img.W, ImgH * 0.58f), Ink(0.0f), Ink(0.82f));
	P.Rect(Strip, Mix(Ink(0.94f), A, Sel));
	P.ClearShear();
	P.StrokeSkew(R, FMath::Lerp(1.5f, 3.5f, Sel), Mix(Paper(0.24f + 0.4f * Hov), A, Sel));

	// 名称
	const float NameY = Img.B() - 40.0f;
	const float NX = R.X + 20.0f + SkewAt(R, NameY);
	P.TextFit(Info.NameZh, NX, NameY, EApexFont::Display, R.W > 270.0f ? 37.0f : 32.0f, R.W - 34.0f, TextHi(), EApexH::Left, EApexV::Middle, 10);
	P.TextFit(Info.NameEn, R.X + 20.0f + SkewAt(R, Img.B() - 14.0f), Img.B() - 14.0f, EApexFont::Num, 15.0f, R.W - 40.0f, Paper(0.82f), EApexH::Left, EApexV::Middle, 130);

	// 角标
	if (i == CurrentMap())
	{
		const float TY = Img.Y + 24.0f;
		const FApexRect TagR(R.X + 20.0f + SkewAt(R, TY), TY - 13.0f, 86.0f, 26.0f);
		P.Skew(TagR, Ink(0.78f));
		P.Circle(FVector2f(TagR.X + 15.0f, TY), 4.0f, Green());
		P.Text(TEXT("当前场景"), TagR.X + 24.0f, TY, EApexFont::BodyBold, 13.0f, TextHi());
	}
	if (Sel > 0.02f)
	{
		const FVector2f C(R.R() - 28.0f + SkewAt(R, Img.Y + 26.0f), Img.Y + 26.0f);
		P.Circle(C, 15.0f * Sel, A);
		Icon(P, EApexIcon::Check, C, 16.0f * Sel, Ink(), 3.0f);
	}

	// 数据行
	const FLinearColor Hi = Mix(TextHi(), Ink(), Sel);
	const FLinearColor Lo = Mix(TextLow(), Ink(0.66f), Sel);
	const float Y1 = Strip.Y + Strip.H * 0.30f;
	const float Y2 = Strip.Y + Strip.H * 0.68f;
	const float X1 = R.X + 18.0f;
	P.Text(TEXT("全长"), X1 + SkewAt(R, Y1), Y1, EApexFont::Body, 14.0f, Lo);
	P.Text(KmText(Info.LengthKm), X1 + SkewAt(R, Y2), Y2, EApexFont::Num, 23.0f, Hi);
	const float X2 = R.X + R.W * 0.34f;
	P.Text(TEXT("难度"), X2 + SkewAt(R, Y1), Y1, EApexFont::Body, 14.0f, Lo);
	Difficulty(P, X2 + SkewAt(R, Y2) + 2.0f, Y2, Info.Difficulty, Mix(A, Ink(), Sel), Mix(Paper(0.20f), Ink(0.24f), Sel), 8.0f, 15.0f, 4.5f);

	const FApexMapRecord Rec = RecordOf(Save.Get(), i);
	FString BestLabel, BestValue;
	if (SelMode == EApexGameModeType::Circuit3Laps)
	{
		BestLabel = TEXT("最快圈");
		BestValue = Rec.BestLapSec > 0.1f ? FormatTime(Rec.BestLapSec) : FString();
	}
	else
	{
		const int32 V = SelMode == EApexGameModeType::DriftAttack3Min ? Rec.BestAttack : Rec.BestFreeDrift;
		BestLabel = TEXT("最佳成绩");
		BestValue = V > 0 ? FormatScore(V) : FString();
	}
	const float XR = R.R() - 20.0f;
	P.Text(BestLabel, XR + SkewAt(R, Y1), Y1, EApexFont::Body, 14.0f, Lo, EApexH::Right);
	if (BestValue.IsEmpty())
	{
		P.Text(TEXT("暂无"), XR + SkewAt(R, Y2), Y2, EApexFont::Body, 17.0f, Lo, EApexH::Right);
	}
	else
	{
		P.Text(BestValue, XR + SkewAt(R, Y2), Y2, EApexFont::Num, 23.0f, Hi, EApexH::Right);
	}
	if (IsFocused(IdMap0 + i))
	{
		FocusRing(P, R, Slant, 8.0f);
	}
}

void SApexMainMenu::PaintDock(FApexPainter& P, const FVector2f& S, const FLayout& L) const
{
	const FApexRect Dock(-2.0f, L.DockY, S.X + 4.0f, L.DockH + 4.0f);
	P.Blur(FApexRect(0.0f, L.DockY, S.X, L.DockH), 12.0f);
	P.RectV(Dock, Ink(0.62f), InkDeep(0.88f));
	P.RectH(FApexRect(Dock.X, L.DockY, Dock.W, 1.5f), Paper(0.34f), Paper(0.10f));
	P.SkewH(FApexRect(L.X0, L.DockY - 2.0f, 220.0f, 5.0f), Accent, WithA(Accent, 0.0f), 0.6f);

	SectionLabel(P, L.X0, L.DockY + 32.0f, TEXT("01"), TEXT("选择赛道"), TEXT("TRACK"), Accent);
	SectionLabel(P, L.RX, L.DockY + 32.0f, TEXT("02"), TEXT("比赛模式"), TEXT("MODE"), Accent);
	SectionLabel(P, L.RX, L.DockY + 164.0f, TEXT("03"), TEXT("时段天气"), TEXT("TIME & WEATHER"), Accent);
	const float LabelW = SectionLabel(P, L.RX, L.DockY + 270.0f, TEXT("04"), TEXT("车身漆色"), TEXT(""), Accent);
	const int32 PaintSel = Save.IsValid() ? FApexPaintRegistry::Wrap(Save->PaintPresetIndex) : 0;
	P.Text(FApexPaintRegistry::GetPaints()[PaintSel].NameZh, L.RX + LabelW + 4.0f, L.DockY + 270.0f, EApexFont::BodyBold, 18.0f, TextMid());

	// 天气说明
	{
		const FApexMapInfo& Info = MapInfo(SelMap);
		int32 Show = WeatherOf(SelMap);
		for (int32 i = 0; i < 3; ++i)
		{
			if (IsHot(IdWeather0 + i)) Show = i;
		}
		if (Info.Weathers.IsValidIndex(Show))
		{
			P.Text(Info.Weathers[Show].Detail, L.X1 - 2.0f, L.DockY + 164.0f, EApexFont::Body, 16.0f, TextMid(), EApexH::Right);
		}
	}

	// 赛道卡片：先画未选中的，选中的压在上面
	for (int32 i = 0; i < L.Card.Num(); ++i) { if (i != SelMap) PaintCard(P, L, i); }
	PaintCard(P, L, SelMap);

	// 模式
	for (int32 i = 0; i < 3; ++i)
	{
		const FApexRect& R = L.Mode[i];
		const EApexGameModeType M = static_cast<EApexGameModeType>(i);
		const float Sel = SelectOf(IdMode0 + i), Hov = HoverOf(IdMode0 + i);
		P.Skew(R, Mix(Paper(0.075f + 0.09f * Hov), Paper(0.96f), Sel));
		if (Sel < 0.98f) P.StrokeSkew(R, 1.2f, Paper((0.16f + 0.35f * Hov) * (1.0f - Sel)));
		if (Sel > 0.01f) P.Skew(FApexRect(R.X, R.Y, 8.0f, R.H), WithA(Accent, Sel), Slant);
		const FLinearColor C = Mix(TextHi(), Ink(), Sel);
		const FLinearColor C2 = Mix(TextLow(), Ink(0.70f), Sel);
		Icon(P, ModeIcon(M), FVector2f(R.X + 44.0f, R.CY()), 34.0f, C, 2.6f);
		P.Text(ModeName(M), R.X + 76.0f, R.Y + 29.0f, EApexFont::Display, 25.0f, C);
		P.TextFit(ModeDesc(M), R.X + 73.0f, R.Y + 57.0f, EApexFont::Body, 15.0f, R.W - 92.0f, C2);
		if (IsFocused(IdMode0 + i)) FocusRing(P, R, Slant, 5.0f);
	}

	// 天气
	{
		const FApexMapInfo& Info = MapInfo(SelMap);
		for (int32 i = 0; i < 3 && i < Info.Weathers.Num(); ++i)
		{
			const FApexRect& R = L.Weather[i];
			const float Sel = SelectOf(IdWeather0 + i), Hov = HoverOf(IdWeather0 + i);
			P.Skew(R, Mix(Paper(0.075f + 0.09f * Hov), Paper(0.96f), Sel));
			if (Sel < 0.98f) P.StrokeSkew(R, 1.2f, Paper((0.16f + 0.35f * Hov) * (1.0f - Sel)));
			if (Sel > 0.01f) P.Skew(FApexRect(R.X, R.Y, 8.0f, R.H), WithA(Accent, Sel), Slant);
			const FLinearColor C = Mix(TextHi(), Ink(), Sel);
			Icon(P, WeatherIcon(Info.Weathers[i].IconKey), FVector2f(R.X + 44.0f, R.CY()), 27.0f, C, 2.2f);
			P.Text(Info.Weathers[i].Name, R.X + 74.0f, R.CY(), EApexFont::Display, 22.0f, C);
			if (IsFocused(IdWeather0 + i)) FocusRing(P, R, Slant, 5.0f);
		}
	}

	// 漆色
	for (int32 i = 0; i < L.Paint.Num(); ++i)
	{
		const FApexPaintInfo& Info = FApexPaintRegistry::GetPaints()[i];
		const float Sel = SelectOf(IdPaint0 + i), Hov = HoverOf(IdPaint0 + i);
		const FVector2f C = L.Paint[i];
		const float R = L.PaintR + 1.5f * Hov + 1.0f * Sel;
		const FLinearColor Base = FLinearColor(Info.Swatch);
		P.Circle(C + FVector2f(0.0f, 2.0f), R + 1.0f, InkDeep(0.5f));
		P.Circle(C, R, Base);
		P.CircleGrad(C + FVector2f(-R * 0.26f, -R * 0.32f), R * 0.62f, Paper(0.50f), Paper(0.0f));
		P.Ring(C, R, 1.2f, Paper(0.30f));
		if (Sel > 0.01f) P.Ring(C, R + 5.0f, 2.6f, Paper(Sel));
		if (IsFocused(IdPaint0 + i))
		{
			P.Ring(C, R + 9.0f, 2.4f, WithA(Accent, 0.95f));
		}
		else if (Hov > 0.02f)
		{
			P.Ring(C, R + 5.0f, 2.0f, Paper(0.6f * Hov));
		}
	}

	// 开始
	{
		const FString Sub = (SelMap == CurrentMap())
			? FString::Printf(TEXT("%s · %s"), *MapInfo(SelMap).NameZh, *ModeName(SelMode))
			: FString::Printf(TEXT("加载 %s · %s"), *MapInfo(SelMap).NameZh, *ModeName(SelMode));
		PrimaryButton(P, L.Start, TEXT("开始比赛"), Sub, Accent, HoverOf(IdStart), PressOf(IdStart), TimeNow, 40.0f);
		if (IsFocused(IdStart)) FocusRing(P, L.Start, Slant, 9.0f);
	}

	// 按键提示
	const float HY = S.Y - 28.0f;
	const TTuple<FString, FString, FString> Hints[] = {
		MakeTuple(FString(TEXT("↑|↓|←|→")), FString(TEXT("左摇杆")), FString(TEXT("移动")) ),
		MakeTuple(FString(TEXT("Enter")), FString(TEXT("A")), FString(TEXT("确认")) ),
		MakeTuple(FString(TEXT("Q|E")), FString(TEXT("LB|RB")), FString(TEXT("切换赛道")) ),
		MakeTuple(FString(TEXT("Z|C")), FString(TEXT("LT|RT")), FString(TEXT("切换车辆")) ),
		MakeTuple(FString(TEXT("Esc")), FString(TEXT("B")), FString(TEXT("退出")) ),
	};
	HintBar(P, L.X0, HY, MakeArrayView(Hints));
	P.Text(TEXT("在画面空白处按住鼠标左键拖动，可环视赛车"), L.X1, HY, EApexFont::Body, 16.0f, TextLow(), EApexH::Right);
}

void SApexMainMenu::PaintModal(FApexPainter& P, const FVector2f& S) const
{
	const float Old = P.GetAlpha();
	P.SetAlpha(Old * FMath::Clamp(ModalT * 1.15f, 0.0f, 1.0f));
	if (Modal == EModal::Controls)
	{
		ControlsPage(P, S, Accent, TimeNow);
		const FApexRect R = RectOf(IdCloseControls);
		GhostButton(P, R, TEXT("知道了"), EApexIcon::Check, HoverOf(IdCloseControls), PressOf(IdCloseControls), 24.0f);
		if (IsFocused(IdCloseControls)) FocusRing(P, R);
	}
	else
	{
		Backdrop(P, S, 12.0f, 0.66f);
		const FApexRect B(S.X * 0.5f - 300.0f, S.Y * 0.5f - 130.0f, 600.0f, 270.0f);
		P.Skew(B, Ink(0.92f), Slant * 0.5f);
		P.StrokeSkew(B, 1.2f, Paper(0.20f), Slant * 0.5f);
		P.SkewH(FApexRect(B.X + 12.0f, B.Y, 200.0f, 4.0f), Accent, WithA(Accent, 0.0f), 0.4f);
		P.Text(TEXT("退出游戏？"), B.CX(), B.Y + 70.0f, EApexFont::Display, 44.0f, TextHi(), EApexH::Center);
		P.Text(TEXT("设置和成绩已自动保存"), B.CX(), B.Y + 124.0f, EApexFont::Body, 18.0f, TextMid(), EApexH::Center);
		GhostButton(P, RectOf(IdQuitNo), TEXT("继续游戏"), EApexIcon::None, HoverOf(IdQuitNo), PressOf(IdQuitNo), 25.0f);
		GhostButton(P, RectOf(IdQuitYes), TEXT("退出到桌面"), EApexIcon::None, HoverOf(IdQuitYes), PressOf(IdQuitYes), 25.0f);
		if (IsFocused(IdQuitNo)) FocusRing(P, RectOf(IdQuitNo));
		if (IsFocused(IdQuitYes)) FocusRing(P, RectOf(IdQuitYes));
	}
	P.SetAlpha(Old);
}

// =========================================================================
// 暂停
// =========================================================================

FApexRect SApexPauseMenu::ItemRect(const FVector2f& S, int32 i) const
{
	const float X0 = FMath::Max(120.0f, (S.X - 1760.0f) * 0.5f + 60.0f);
	return FApexRect(X0, 366.0f + i * 80.0f, 470.0f, 66.0f);
}

void SApexPauseMenu::BuildItems(const FVector2f& S)
{
	if (bControls)
	{
		Put(IdCloseControls, FApexRect(S.X * 0.5f - 130.0f, S.Y - 92.0f, 260.0f, 56.0f));
		return;
	}
	for (int32 i = 0; i < 6; ++i)
	{
		Put(IdResume + i, ItemRect(S, i));
	}
}

void SApexPauseMenu::OnActivate(int32 Id, const FVector2f& LocalPos, bool bMouse)
{
	if (!PC.IsValid())
	{
		return;
	}
	switch (Id)
	{
	case IdResume: PC->ResumeGame(); break;
	case IdReset: PC->ResetVehicleToTrack(); PC->ResumeGame(); break;
	case IdRestart: PC->RestartCurrentRace(); break;
	case IdSettings: PC->OpenSettingsMenu(); break;
	case IdControls: Click(); bControls = true; FocusId = IdCloseControls; break;
	case IdCloseControls: Click(); bControls = false; FocusId = IdControls; break;
	case IdMenu: PC->ReturnToMainMenu(); break;
	default: break;
	}
}

bool SApexPauseMenu::OnBack()
{
	if (bControls)
	{
		Click();
		bControls = false;
		FocusId = IdControls;
		return true;
	}
	if (PC.IsValid()) PC->ResumeGame();
	return true;
}

bool SApexPauseMenu::OnMenuKey() { return OnBack(); }

void SApexPauseMenu::PaintScreen(FApexPainter& P, const FVector2f& S) const
{
	const int32 Map = PC.IsValid() ? PC->GetCurrentMapIndex() : 0;
	const FLinearColor Accent = MapAccent(Map);
	const FApexMapInfo& Info = MapInfo(Map);
	const float T = EaseOut(OpenT / 0.30f);
	const float Base = P.GetAlpha();
	P.SetAlpha(Base * T);

	if (bControls)
	{
		ControlsPage(P, S, Accent, TimeNow);
		const FApexRect R = RectOf(IdCloseControls);
		GhostButton(P, R, TEXT("知道了"), EApexIcon::Check, HoverOf(IdCloseControls), PressOf(IdCloseControls), 24.0f);
		if (IsFocused(IdCloseControls)) FocusRing(P, R);
		P.SetAlpha(Base);
		return;
	}

	Backdrop(P, S, 15.0f, 0.70f);
	P.RectH(FApexRect(-2, -2, S.X * 0.62f, S.Y + 4), InkDeep(0.60f), InkDeep(0.0f));

	const FApexRect R0 = ItemRect(S, 0);
	const float X0 = R0.X;
	P.Text(TEXT("PAUSED"), X0 + 2.0f, 158.0f, EApexFont::Num, 30.0f, Accent, EApexH::Left, EApexV::Middle, 260);
	P.Text(TEXT("暂停"), X0 - 6.0f, 310.0f, EApexFont::Display, 132.0f, TextHi(), EApexH::Left, EApexV::Baseline, 20);

	struct FEntry { const TCHAR* Zh; EApexIcon I; };
	static const FEntry E[6] = {
		{ TEXT("继续驾驶"), EApexIcon::Play }, { TEXT("复位到赛道"), EApexIcon::Reset }, { TEXT("重新开始"), EApexIcon::Restart },
		{ TEXT("设置"), EApexIcon::Gear }, { TEXT("操作说明"), EApexIcon::Pad }, { TEXT("返回主菜单"), EApexIcon::Home },
	};
	for (int32 i = 0; i < 6; ++i)
	{
		const FApexRect R = ItemRect(S, i);
		const float H = HoverOf(IdResume + i);
		const float Dx = 10.0f * H - 24.0f * (1.0f - EaseOut((OpenT - 0.03f * i) / 0.32f));
		const FApexRect B = R.Offset(Dx, 0.0f);
		if (i == 0)
		{
			P.SkewH(B, Mix(Accent, Paper(), 0.08f + 0.2f * H), Mix(Accent, Paper(), 0.32f + 0.2f * H));
		}
		else
		{
			P.SkewH(B, Mix(Paper(0.08f), Paper(0.96f), H), Mix(Paper(0.02f), Paper(0.96f), H));
		}
		const FLinearColor C = i == 0 ? Ink() : Mix(TextHi(), Ink(), H);
		Icon(P, E[i].I, FVector2f(B.X + 46.0f, B.CY()), 27.0f, C, 2.4f);
		P.Text(E[i].Zh, B.X + 84.0f, B.CY(), EApexFont::Display, 33.0f, C);
		if (IsFocused(IdResume + i)) FocusRing(P, B);
	}

	// 右侧：本局信息
	const float CW = 560.0f;
	const float CX = S.X - X0 - CW;
	const float CY = 250.0f;
	const FApexRect Art(CX, CY, CW, 250.0f);
	const float CardH = 250.0f + 258.0f;
	P.GlowSkew(FApexRect(CX, CY, CW, CardH), 30.0f, InkDeep(0.35f), 0.0f);
	P.Rect(FApexRect(CX, CY + 250.0f, CW, CardH - 250.0f), Ink(0.82f));
	MapArt(P, Art, Map, 1.0f);
	P.RectV(FApexRect(CX, CY + 90.0f, CW, 160.0f), Ink(0.0f), Ink(0.82f));
	Seal(P, FApexRect(CX + 28.0f, CY + 24.0f, 46.0f, 46.0f), MapSeal(Map), Accent, Ink());
	P.Text(Info.NameZh, CX + 28.0f, CY + 226.0f, EApexFont::Display, 54.0f, TextHi(), EApexH::Left, EApexV::Baseline);
	P.Text(ModeName(PC.IsValid() ? PC->GetCurrentMode() : EApexGameModeType::FreeDrift), CX + CW - 28.0f, CY + 210.0f, EApexFont::Display, 26.0f, Accent, EApexH::Right);

	FApexDriftTelemetry Tel;
	if (PC.IsValid() && PC->GetVehiclePawn())
	{
		Tel = PC->GetVehiclePawn()->GetTelemetry();
	}
	const float BY = CY + 250.0f + 60.0f;
	P.Text(TEXT("本局得分"), CX + 28.0f, BY - 18.0f, EApexFont::Body, 17.0f, TextLow());
	P.Text(FormatScore(Tel.TotalScore), CX + 26.0f, BY + 24.0f, EApexFont::NumHeavy, 60.0f, TextHi());
	if (PC.IsValid() && PC->GetCurrentMode() == EApexGameModeType::DriftAttack3Min)
	{
		P.Text(TEXT("剩余时间"), CX + CW - 28.0f, BY - 18.0f, EApexFont::Body, 17.0f, TextLow(), EApexH::Right);
		P.Text(FormatTime(Tel.SessionTimeRemaining), CX + CW - 28.0f, BY + 24.0f, EApexFont::Num, 46.0f, Accent, EApexH::Right);
	}
	else
	{
		P.Text(TEXT("当前圈"), CX + CW - 28.0f, BY - 18.0f, EApexFont::Body, 17.0f, TextLow(), EApexH::Right);
		P.Text(FormatTime(Tel.CurrentLapTime), CX + CW - 28.0f, BY + 24.0f, EApexFont::Num, 46.0f, Accent, EApexH::Right);
	}
	P.Rect(FApexRect(CX + 28.0f, BY + 76.0f, CW - 56.0f, 1.0f), Line());
	struct FStat { const TCHAR* L; FString V; };
	const FStat St[4] = {
		{ TEXT("最佳单漂"), FormatScore(Tel.BestSingleDrift) },
		{ TEXT("最大角度"), FString::Printf(TEXT("%.0f°"), Tel.PeakAngleDeg) },
		{ TEXT("最高倍率"), FString::Printf(TEXT("×%.1f"), Tel.MaxDriftCombo) },
		{ TEXT("最快圈"), FormatTimeOrDash(Tel.BestLapTime) },
	};
	for (int32 i = 0; i < 4; ++i)
	{
		const float QX = CX + 28.0f + (CW - 56.0f) * 0.25f * i;
		P.Text(St[i].L, QX, BY + 112.0f, EApexFont::Body, 16.0f, TextLow());
		P.TextFit(St[i].V, QX, BY + 146.0f, EApexFont::Num, 30.0f, (CW - 56.0f) * 0.25f - 10.0f, TextHi());
	}

	const TTuple<FString, FString, FString> Hints[] = {
		MakeTuple(FString(TEXT("↑|↓")), FString(TEXT("左摇杆")), FString(TEXT("选择")) ),
		MakeTuple(FString(TEXT("Enter")), FString(TEXT("A")), FString(TEXT("确认")) ),
		MakeTuple(FString(TEXT("Esc")), FString(TEXT("B")), FString(TEXT("继续驾驶")) ),
	};
	HintBar(P, X0, S.Y - 56.0f, MakeArrayView(Hints));
	P.SetAlpha(Base);
}

// =========================================================================
// 结算
// =========================================================================

void SApexFinish::SetResult(const FApexDriftTelemetry& T, EApexGameModeType M, bool bRecord)
{
	Tel = T;
	Mode = M;
	bNewRecord = bRecord;
}

void SApexFinish::OnShown()
{
	ShownScore = 0.0f;
	bGradeSound = false;
}

TCHAR SApexFinish::Grade() const
{
	// 按“每公里得分”折算，圈赛按跑过的里程，限时赛按 180 秒约 4.5 km
	const float Km = (Mode == EApexGameModeType::Circuit3Laps)
		? FMath::Max(1.0f, MapInfo(PC.IsValid() ? PC->GetCurrentMapIndex() : 0).LengthKm * Tel.TotalLaps)
		: 4.5f;
	const float PerKm = Tel.TotalScore / Km;
	const float K = (Mode == EApexGameModeType::Circuit3Laps) ? 0.6f : 1.0f;
	if (PerKm >= 24000.0f * K) return TEXT('S');
	if (PerKm >= 12000.0f * K) return TEXT('A');
	if (PerKm >= 4800.0f * K) return TEXT('B');
	return TEXT('C');
}

void SApexFinish::BuildItems(const FVector2f& S)
{
	const float X0 = FMath::Max(120.0f, (S.X - 1680.0f) * 0.5f);
	const float Y = S.Y - 200.0f;
	Put(IdRetry, FApexRect(X0 + 10.0f, Y, 338.0f, 84.0f));
	Put(IdChangeMap, FApexRect(X0 + 388.0f, Y + 10.0f, 230.0f, 64.0f));
	Put(IdMenu, FApexRect(X0 + 646.0f, Y + 10.0f, 250.0f, 64.0f));
}

void SApexFinish::TickScreen(float Dt)
{
	const float Delay = OpenT - 0.45f;
	if (Delay > 0.0f)
	{
		ShownScore = Tel.TotalScore * EaseOut(Delay / 1.5f);
	}
	if (!bGradeSound && OpenT > 1.75f)
	{
		bGradeSound = true;
		PlayUISound(PC.Get(), FName(TEXT("Bank")));
	}
}

void SApexFinish::OnActivate(int32 Id, const FVector2f& LocalPos, bool bMouse)
{
	if (!PC.IsValid())
	{
		return;
	}
	switch (Id)
	{
	case IdRetry: PC->RestartCurrentRace(); break;
	case IdChangeMap: GFocusMapsOnNextMenu = true; PC->ReturnToMainMenu(); break;
	case IdMenu: PC->ReturnToMainMenu(); break;
	default: break;
	}
}

bool SApexFinish::OnBack()
{
	if (PC.IsValid()) PC->ReturnToMainMenu();
	return true;
}

void SApexFinish::PaintScreen(FApexPainter& P, const FVector2f& S) const
{
	const int32 Map = PC.IsValid() ? PC->GetCurrentMapIndex() : 0;
	const FLinearColor Accent = MapAccent(Map);
	const FApexMapInfo& Info = MapInfo(Map);
	const float Base = P.GetAlpha();
	P.SetAlpha(Base * EaseOut(OpenT / 0.35f));
	Backdrop(P, S, 16.0f, 0.74f);
	const float X0 = FMath::Max(120.0f, (S.X - 1680.0f) * 0.5f);

	// 右侧评级大字
	const float GX = S.X - X0 - 250.0f;
	const TCHAR G = Grade();
	const FLinearColor GC = G == TEXT('S') ? Gold() : (G == TEXT('A') ? Accent : (G == TEXT('B') ? Hex(0x9FE3C2) : Paper(0.9f)));
	{
		// 斜纹底
		for (int32 i = 0; i < 7; ++i)
		{
			const float W = 26.0f;
			P.Skew(FApexRect(GX - 250.0f + i * 78.0f, 150.0f, W, 560.0f), WithA(GC, 0.05f + 0.012f * i), 0.34f);
		}
		const float GT = (OpenT - 1.55f) / 0.45f;
		if (GT > 0.0f)
		{
			const float K = FMath::Clamp(GT, 0.0f, 1.0f);
			P.CircleGrad(FVector2f(GX, 420.0f), 330.0f, WithA(GC, 0.26f * K), WithA(GC, 0.0f));
			const float Old = P.GetAlpha();
			P.SetAlpha(Old * K);
			const float Dy = 40.0f * (1.0f - EaseOutBack(GT));
			P.Text(FString::Chr(G), GX, 560.0f - Dy, EApexFont::NumHeavy, 440.0f, GC, EApexH::Center, EApexV::Baseline, 0, 0.35f);
			P.Text(TEXT("评级"), GX - 168.0f, 218.0f, EApexFont::Display, 30.0f, TextHi());
			P.Text(TEXT("RANK"), GX - 96.0f, 221.0f, EApexFont::Num, 22.0f, GC, EApexH::Left, EApexV::Middle, 200);
			static const TCHAR* Say[4] = { TEXT("完美掌控，这条路是你的了"), TEXT("漂亮的走线，再稳一点就是 S"), TEXT("节奏不错，试着把角度留得更久"), TEXT("先求连贯，再求角度") };
			const int32 SayI = G == TEXT('S') ? 0 : (G == TEXT('A') ? 1 : (G == TEXT('B') ? 2 : 3));
			P.Text(Say[SayI], GX, 640.0f, EApexFont::Body, 21.0f, TextMid(), EApexH::Center);
			P.SetAlpha(Old);
		}
	}

	// 左侧
	Seal(P, FApexRect(X0, 128.0f, 46.0f, 46.0f), MapSeal(Map), Accent, Ink());
	P.Text(FString::Printf(TEXT("%s · %s"), *Info.NameZh, *ModeName(Mode)), X0 + 64.0f, 151.0f, EApexFont::Display, 28.0f, TextHi());
	const float TitleW = P.Text(TEXT("本局结束"), X0 - 4.0f, 286.0f, EApexFont::Display, 96.0f, TextHi(), EApexH::Left, EApexV::Baseline, 10);
	P.Text(TEXT("FINISH"), X0 + TitleW + 22.0f, 284.0f, EApexFont::Num, 36.0f, Accent, EApexH::Left, EApexV::Baseline, 200);

	P.Text(TEXT("总得分"), X0 + 2.0f, 352.0f, EApexFont::Body, 22.0f, TextMid());
	const float SW = P.Text(FormatScore(FMath::RoundToInt(ShownScore)), X0 - 4.0f, 506.0f, EApexFont::NumHeavy, 168.0f, TextHi(), EApexH::Left, EApexV::Baseline, 0, 0.3f);

	if (bNewRecord)
	{
		const float BT = (OpenT - 2.0f) / 0.4f;
		if (BT > 0.0f)
		{
			const float K = EaseOutBack(BT);
			const FApexRect B(X0 + SW + 34.0f - 10.0f * (1.0f - K), 410.0f, 196.0f, 58.0f);
			const float Old = P.GetAlpha();
			P.SetAlpha(Old * FMath::Clamp(BT, 0.0f, 1.0f));
			P.GlowSkew(B, 22.0f, WithA(Gold(), 0.35f));
			P.SkewH(B, Gold(), Hex(0xFFE39A));
			Icon(P, EApexIcon::Crown, FVector2f(B.X + 40.0f, B.CY() - 1.0f), 32.0f, Ink(), 2.0f);
			P.Text(TEXT("新纪录"), B.X + 72.0f, B.CY(), EApexFont::Display, 33.0f, Ink());
			P.SetAlpha(Old);
		}
	}

	// 统计
	struct FStat { const TCHAR* L; FString V; };
	const FStat St[4] = {
		{ TEXT("最佳单漂"), FormatScore(Tel.BestSingleDrift) },
		{ TEXT("最大角度"), FString::Printf(TEXT("%.0f°"), Tel.PeakAngleDeg) },
		{ TEXT("最高倍率"), FString::Printf(TEXT("×%.1f"), Tel.MaxDriftCombo) },
		{ TEXT("最快圈"), FormatTimeOrDash(Tel.BestLapTime) },
	};
	const float SY = 588.0f;
	const float CellW = 214.0f;
	for (int32 i = 0; i < 4; ++i)
	{
		const float T = EaseOut((OpenT - 0.5f - i * 0.08f) / 0.4f);
		const float Old = P.GetAlpha();
		P.SetAlpha(Old * T);
		const FApexRect C(X0 + 8.0f + i * (CellW + 14.0f), SY + 16.0f * (1.0f - T), CellW, 112.0f);
		P.Skew(C, Paper(0.08f));
		P.Skew(FApexRect(C.X, C.Y, 5.0f, C.H), Accent);
		P.Text(St[i].L, C.X + 32.0f, C.Y + 34.0f, EApexFont::Body, 18.0f, TextMid());
		P.TextFit(St[i].V, C.X + 26.0f, C.Y + 75.0f, EApexFont::Num, 42.0f, CellW - 44.0f, TextHi());
		P.SetAlpha(Old);
	}

	PrimaryButton(P, RectOf(IdRetry), TEXT("再来一次"), TEXT(""), Accent, HoverOf(IdRetry), PressOf(IdRetry), TimeNow, 40.0f);
	GhostButton(P, RectOf(IdChangeMap), TEXT("换赛道"), EApexIcon::Map, HoverOf(IdChangeMap), PressOf(IdChangeMap), 26.0f);
	GhostButton(P, RectOf(IdMenu), TEXT("返回主菜单"), EApexIcon::Home, HoverOf(IdMenu), PressOf(IdMenu), 26.0f);
	if (IsFocused(IdRetry)) FocusRing(P, RectOf(IdRetry), Slant, 9.0f);
	if (IsFocused(IdChangeMap)) FocusRing(P, RectOf(IdChangeMap));
	if (IsFocused(IdMenu)) FocusRing(P, RectOf(IdMenu));

	const TTuple<FString, FString, FString> Hints[] = {
		MakeTuple(FString(TEXT("←|→")), FString(TEXT("左摇杆")), FString(TEXT("选择")) ),
		MakeTuple(FString(TEXT("Enter")), FString(TEXT("A")), FString(TEXT("确认")) ),
		MakeTuple(FString(TEXT("Esc")), FString(TEXT("B")), FString(TEXT("返回主菜单")) ),
	};
	HintBar(P, X0, S.Y - 56.0f, MakeArrayView(Hints));
	P.SetAlpha(Base);
}

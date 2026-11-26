#include "UI/ApexScreen.h"
#include "Core/ApexDriftPlayerController.h"
#include "Core/ApexDriftSaveGame.h"
#include "Engine/Console.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Framework/Application/SlateApplication.h"
#include "InputCoreTypes.h"

void SApexScreen::Construct(const FArguments& InArgs)
{
	ApexUI::EnsureLoaded();
	SetCanTick(true);
	ForceVolatile(true); // 自绘动画，开启全局失效缓存时也要每帧重画
}

void SApexScreen::NotifyShown()
{
	if (bShown)
	{
		return;
	}
	bShown = true;
	OpenT = 0.0f;
	PressId = 0;
	bDragging = false;
	HoverId = 0;
	StickDirX = StickDirY = 0;
	OnShown();
	FocusId = DefaultFocus();
}

void SApexScreen::NotifyHidden()
{
	if (bShown)
	{
		OnHidden();
	}
	bShown = false;
	bDragging = false;
	PressId = 0;
}

void SApexScreen::UpdateLayout(const FGeometry& Geo)
{
	// 视口坐标已经过 UMG 的 DPI 缩放（短边 1080）；这里只处理非 16:9 和没有 DPI 缩放的情况
	const FVector2f L = FVector2f(Geo.GetLocalSize());
	DesignScale = FMath::Max(0.05f, FMath::Min(L.Y / 1080.0f, L.X / 1760.0f));
	DesignSize = FVector2f(L.X / DesignScale, L.Y / DesignScale);
}

void SApexScreen::Rebuild()
{
	for (FApexItem& I : Items)
	{
		I.bAlive = false;
	}
	BuildItems(DesignSize);
}

FApexItem& SApexScreen::Put(int32 Id, const FApexRect& R, bool bSelected, bool bEnabled)
{
	FApexItem* Found = Items.FindByPredicate([Id](const FApexItem& I) { return I.Id == Id; });
	if (!Found)
	{
		Found = &Items.AddDefaulted_GetRef();
		Found->Id = Id;
		Found->Select = bSelected ? 1.0f : 0.0f;
	}
	Found->Rect = R;
	Found->bSelected = bSelected;
	Found->bEnabled = bEnabled;
	Found->bAlive = true;
	return *Found;
}

const FApexItem* SApexScreen::Find(int32 Id) const
{
	return Items.FindByPredicate([Id](const FApexItem& I) { return I.Id == Id && I.bAlive; });
}

void SApexScreen::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	const float Dt = FMath::Clamp(InDeltaTime, 0.0f, 0.1f);
	TimeNow += Dt;
	OpenT += Dt;
	UpdateLayout(AllottedGeometry);
	TickScreen(Dt);
	Rebuild();

	if (FocusId == 0 || !Find(FocusId))
	{
		FocusId = DefaultFocus();
	}

	for (FApexItem& I : Items)
	{
		const bool bHot = I.bAlive && I.bEnabled && IsHot(I.Id);
		I.Hover = ApexUI::Approach(I.Hover, bHot ? 1.0f : 0.0f, Dt, 16.0f);
		I.Select = ApexUI::Approach(I.Select, I.bSelected ? 1.0f : 0.0f, Dt, 13.0f);
		I.Press = ApexUI::Approach(I.Press, (PressId == I.Id && I.bAlive) ? 1.0f : 0.0f, Dt, 24.0f);
	}

	// 摇杆连发
	if (StickDirX != 0 || StickDirY != 0)
	{
		StickRepeat -= Dt;
		if (StickRepeat <= 0.0f)
		{
			StickRepeat = 0.13f;
			Navigate(StickDirX, StickDirY);
		}
	}

	// 维持键盘焦点：拖动环视会让视口抢走焦点，松开后收回来
	FSlateApplication& App = FSlateApplication::Get();
	bool bConsoleOpen = false;
	if (GEngine && GEngine->GameViewport && GEngine->GameViewport->ViewportConsole)
	{
		bConsoleOpen = GEngine->GameViewport->ViewportConsole->ConsoleActive();
	}
	if (bShown && !bConsoleOpen && App.GetPressedMouseButtons().Num() == 0 && !HasAnyUserFocus().IsSet())
	{
		App.SetUserFocus(0, SharedThis(this), EFocusCause::SetDirectly);
	}
}

int32 SApexScreen::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const FGeometry DesignGeo = AllottedGeometry.MakeChild(FVector2f(DesignSize), FSlateLayoutTransform(DesignScale, FVector2f(0.0f, 0.0f)));
	FApexPainter P(DesignGeo, OutDrawElements, LayerId, InWidgetStyle.GetColorAndOpacityTint().A);
	PaintScreen(P, DesignSize);
	return P.Finish();
}

FVector2f SApexScreen::ToDesign(const FGeometry& Geo, const FVector2D& ScreenPos) const
{
	return FVector2f(Geo.AbsoluteToLocal(ScreenPos)) / DesignScale;
}

int32 SApexScreen::HitTest(const FVector2f& P) const
{
	for (int32 i = Items.Num() - 1; i >= 0; --i)
	{
		const FApexItem& I = Items[i];
		if (I.bAlive && I.bEnabled && I.Rect.Contains(P))
		{
			return I.Id;
		}
	}
	return 0;
}

void SApexScreen::SetFocusId(int32 Id, bool bSound)
{
	if (FocusId != Id)
	{
		FocusId = Id;
		if (bSound)
		{
			ApexUI::PlayUISound(PC.Get(), FName(TEXT("Hover")));
		}
	}
}

void SApexScreen::Click() const
{
	if (PC.IsValid())
	{
		PC->PlayMenuClickSound();
	}
}

void SApexScreen::ActivateItem(int32 Id, const FVector2f& LocalPos, bool bMouse)
{
	const FApexItem* I = Find(Id);
	if (I && I->bEnabled)
	{
		OnActivate(Id, LocalPos, bMouse);
	}
}

bool SApexScreen::Navigate(int32 DX, int32 DY)
{
	bNavMode = true;
	Rebuild();
	const FApexItem* Cur = Find(FocusId);
	if (!Cur)
	{
		SetFocusId(DefaultFocus(), true);
		return true;
	}
	if (DX != 0 && OnAdjust(FocusId, DX))
	{
		return true;
	}
	const FApexRect C = Cur->Rect;
	float BestScore = TNumericLimits<float>::Max();
	int32 BestId = 0;
	for (const FApexItem& I : Items)
	{
		if (!I.bAlive || !I.bEnabled || I.Id == FocusId)
		{
			continue;
		}
		const FApexRect& R = I.Rect;
		float Primary, Perp;
		if (DX != 0)
		{
			const float Delta = (R.CX() - C.CX()) * DX;
			if (Delta < 4.0f) continue;
			// 左右方向上大部分重叠的是上下关系，不参与横向移动
			if (FMath::Min(C.R(), R.R()) - FMath::Max(C.X, R.X) > 0.5f * FMath::Min(C.W, R.W)) continue;
			Primary = FMath::Max(0.0f, DX > 0 ? R.X - C.R() : C.X - R.R()) + Delta * 0.05f;
			Perp = FMath::Max(0.0f, FMath::Max(R.Y - C.B(), C.Y - R.B()));
			const float CenterOff = FMath::Abs(R.CY() - C.CY());
			// 横向移动时要求大致同一行
			if (Perp > 60.0f) Perp *= 4.0f;
			Perp = Perp * 3.0f + CenterOff * 0.25f;
		}
		else
		{
			const float Delta = (R.CY() - C.CY()) * DY;
			if (Delta < 4.0f) continue;
			// 同理，同一行的元素不参与纵向移动
			if (FMath::Min(C.B(), R.B()) - FMath::Max(C.Y, R.Y) > 0.3f * FMath::Min(C.H, R.H)) continue;
			Primary = FMath::Max(0.0f, DY > 0 ? R.Y - C.B() : C.Y - R.B()) + Delta * 0.05f;
			Perp = FMath::Max(0.0f, FMath::Max(R.X - C.R(), C.X - R.R()));
			Perp = Perp * 1.6f + FMath::Abs(R.CX() - C.CX()) * 0.18f;
		}
		const float Score = Primary + Perp;
		if (Score < BestScore)
		{
			BestScore = Score;
			BestId = I.Id;
		}
	}
	if (BestId != 0)
	{
		SetFocusId(BestId, true);
		return true;
	}
	return false;
}

FReply SApexScreen::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	const bool bPad = Key.IsGamepadKey();
	int32 DX = 0, DY = 0;
	if (Key == EKeys::Left || Key == EKeys::A || Key == EKeys::Gamepad_DPad_Left) DX = -1;
	else if (Key == EKeys::Right || Key == EKeys::D || Key == EKeys::Gamepad_DPad_Right) DX = 1;
	else if (Key == EKeys::Up || Key == EKeys::W || Key == EKeys::Gamepad_DPad_Up) DY = -1;
	else if (Key == EKeys::Down || Key == EKeys::S || Key == EKeys::Gamepad_DPad_Down) DY = 1;
	if (DX != 0 || DY != 0)
	{
		bGamepad = bPad;
		Navigate(DX, DY);
		return FReply::Handled();
	}
	// 摇杆由 OnAnalogValueChanged 统一处理
	if (Key == EKeys::Gamepad_LeftStick_Up || Key == EKeys::Gamepad_LeftStick_Down || Key == EKeys::Gamepad_LeftStick_Left || Key == EKeys::Gamepad_LeftStick_Right
		|| Key == EKeys::Tab)
	{
		return FReply::Handled();
	}
	if (Key == EKeys::Enter || Key == EKeys::SpaceBar || Key == EKeys::Gamepad_FaceButton_Bottom)
	{
		if (!InKeyEvent.IsRepeat())
		{
			bGamepad = bPad;
			bNavMode = true;
			Rebuild();
			const FApexItem* I = Find(FocusId);
			if (I)
			{
				ActivateItem(FocusId, I->Rect.Center(), false);
			}
		}
		return FReply::Handled();
	}
	if (Key == EKeys::Escape || Key == EKeys::BackSpace || Key == EKeys::Gamepad_FaceButton_Right)
	{
		if (!InKeyEvent.IsRepeat())
		{
			bGamepad = bPad;
			OnBack();
		}
		return FReply::Handled();
	}
	if (Key == EKeys::P || Key == EKeys::Gamepad_Special_Right)
	{
		if (!InKeyEvent.IsRepeat())
		{
			bGamepad = bPad;
			OnMenuKey();
		}
		return FReply::Handled();
	}
	if (Key == EKeys::Q || Key == EKeys::Gamepad_LeftShoulder || Key == EKeys::E || Key == EKeys::Gamepad_RightShoulder)
	{
		if (!InKeyEvent.IsRepeat())
		{
			bGamepad = bPad;
			const bool bNext = (Key == EKeys::E || Key == EKeys::Gamepad_RightShoulder);
			OnShoulder(bNext ? 1 : -1);
		}
		return FReply::Handled();
	}
	if (Key == EKeys::Z || Key == EKeys::Gamepad_LeftTrigger || Key == EKeys::C || Key == EKeys::Gamepad_RightTrigger)
	{
		if (!InKeyEvent.IsRepeat())
		{
			bGamepad = bPad;
			OnTrigger((Key == EKeys::C || Key == EKeys::Gamepad_RightTrigger) ? 1 : -1);
		}
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

FReply SApexScreen::OnAnalogValueChanged(const FGeometry& MyGeometry, const FAnalogInputEvent& InEvent)
{
	const FKey Key = InEvent.GetKey();
	if (Key != EKeys::Gamepad_LeftX && Key != EKeys::Gamepad_LeftY)
	{
		return FReply::Unhandled();
	}
	if (Key == EKeys::Gamepad_LeftX) StickX = InEvent.GetAnalogValue();
	else StickY = InEvent.GetAnalogValue();

	int32 NX = 0, NY = 0;
	constexpr float On = 0.55f, Off = 0.35f;
	const float Th = (StickDirX != 0 || StickDirY != 0) ? Off : On;
	if (FMath::Abs(StickX) > FMath::Abs(StickY))
	{
		if (FMath::Abs(StickX) > Th) NX = StickX > 0 ? 1 : -1;
	}
	else if (FMath::Abs(StickY) > Th)
	{
		NY = StickY > 0 ? -1 : 1;
	}
	if (NX != StickDirX || NY != StickDirY)
	{
		StickDirX = NX;
		StickDirY = NY;
		StickRepeat = 0.38f;
		if (NX != 0 || NY != 0)
		{
			bGamepad = true;
			Navigate(NX, NY);
		}
	}
	return FReply::Handled();
}

FReply SApexScreen::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	const FVector2f Pos = ToDesign(MyGeometry, MouseEvent.GetScreenSpacePosition());
	const bool bMoved = FVector2f::DistSquared(Pos, MousePos) > 0.5f;
	MousePos = Pos;
	if (bDragging && PressId != 0)
	{
		OnDrag(PressId, Pos, false);
		return FReply::Handled();
	}
	if (bMoved)
	{
		const int32 Hit = HitTest(Pos);
		if (bNavMode)
		{
			bNavMode = false;
			bGamepad = false;
		}
		if (Hit != HoverId)
		{
			HoverId = Hit;
			if (Hit != 0)
			{
				FocusId = Hit;
				ApexUI::PlayUISound(PC.Get(), FName(TEXT("Hover")));
			}
		}
	}
	return BlocksMouse(Pos) ? FReply::Handled() : FReply::Unhandled();
}

void SApexScreen::OnMouseLeave(const FPointerEvent& MouseEvent)
{
	if (!bDragging)
	{
		HoverId = 0;
	}
}

void SApexScreen::OnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent)
{
	if (bDragging && PressId != 0)
	{
		OnDrag(PressId, MousePos, true);
	}
	bDragging = false;
	PressId = 0;
}

FReply SApexScreen::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	const FVector2f Pos = ToDesign(MyGeometry, MouseEvent.GetScreenSpacePosition());
	MousePos = Pos;
	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		const int32 Hit = HitTest(Pos);
		if (Hit != 0)
		{
			bNavMode = false;
			HoverId = Hit;
			FocusId = Hit;
			PressId = Hit;
			bDragging = OnDrag(Hit, Pos, false);
			return FReply::Handled().CaptureMouse(SharedThis(this));
		}
	}
	return BlocksMouse(Pos) ? FReply::Handled() : FReply::Unhandled();
}

FReply SApexScreen::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	const FVector2f Pos = ToDesign(MyGeometry, MouseEvent.GetScreenSpacePosition());
	MousePos = Pos;
	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && PressId != 0)
	{
		const int32 Id = PressId;
		const bool bWasDrag = bDragging;
		PressId = 0;
		bDragging = false;
		if (bWasDrag)
		{
			OnDrag(Id, Pos, true);
		}
		else if (HitTest(Pos) == Id)
		{
			ActivateItem(Id, Pos, true);
		}
		return FReply::Handled().ReleaseMouseCapture();
	}
	return BlocksMouse(Pos) ? FReply::Handled() : FReply::Unhandled();
}

FReply SApexScreen::OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	return BlocksMouse(ToDesign(MyGeometry, MouseEvent.GetScreenSpacePosition())) ? FReply::Handled() : FReply::Unhandled();
}

FCursorReply SApexScreen::OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const
{
	const FVector2f Pos = ToDesign(MyGeometry, CursorEvent.GetScreenSpacePosition());
	if (HitTest(Pos) != 0)
	{
		return FCursorReply::Cursor(EMouseCursor::Hand);
	}
	return BlocksMouse(Pos) ? FCursorReply::Cursor(EMouseCursor::Default) : FCursorReply::Unhandled();
}

void SApexScreen::FocusRing(FApexPainter& P, const FApexRect& R, float K, float Pad) const
{
	const float Pulse = 0.5f + 0.5f * FMath::Sin(TimeNow * 5.0f);
	const FApexRect O(R.X - Pad, R.Y - Pad, R.W + Pad * 2, R.H + Pad * 2);
	P.StrokeSkew(O, 3.0f, ApexUI::Paper(0.82f + 0.18f * Pulse), K);
}

void SApexScreen::HintBar(FApexPainter& P, float X, float CY, TConstArrayView<TTuple<FString, FString, FString>> Hints) const
{
	float Cur = X;
	for (const auto& H : Hints)
	{
		Cur += ApexUI::KeyHint(P, Cur, CY, bGamepad ? H.Get<1>() : H.Get<0>(), H.Get<2>()) + 28.0f;
	}
}

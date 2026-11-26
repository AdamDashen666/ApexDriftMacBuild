#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Core/ApexDriftTypes.h"
#include "UI/ApexUIStyle.h"
#include "UI/ApexUIArt.h"

class AApexDriftPlayerController;
class UApexDriftSaveGame;

/** 屏幕上一个可交互元素（每帧由 BuildItems 重新登记，动画按 Id 保留） */
struct FApexItem
{
	int32 Id = 0;
	FApexRect Rect;
	bool bEnabled = true;
	bool bAlive = false;
	float Hover = 0.0f;   // 悬停/焦点 0..1
	float Select = 0.0f;  // 选中 0..1（由屏幕设置 bSelected 驱动）
	float Press = 0.0f;
	bool bSelected = false;
};

/**
 * 所有菜单页的基类：
 * - 统一的设计坐标（高 1080，宽随比例，过窄时整体缩小）
 * - 鼠标 / 键盘 / 手柄共用一套焦点，方向键按几何位置找最近元素
 * - 自己取得并维持 Slate 键盘焦点，Esc / B 由屏幕处理并返回 Handled
 */
class SApexScreen : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SApexScreen) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	void SetPlayer(AApexDriftPlayerController* InPC, UApexDriftSaveGame* InSave) { PC = InPC; Save = InSave; }
	/** 由 UserWidget::SetVisibility 调用：从隐藏变为可见 */
	void NotifyShown();
	void NotifyHidden();

	// SWidget
	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(1920.0f, 1080.0f); }
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply OnAnalogValueChanged(const FGeometry& MyGeometry, const FAnalogInputEvent& InEvent) override;
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonDoubleClick(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override { return OnMouseButtonDown(MyGeometry, MouseEvent); }
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual void OnMouseLeave(const FPointerEvent& MouseEvent) override;
	virtual void OnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent) override;
	virtual FReply OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FCursorReply OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const override;
	virtual FNavigationReply OnNavigation(const FGeometry& MyGeometry, const FNavigationEvent& InNavigationEvent) override { return FNavigationReply::Stop(); }

	// 供自动截图流程调用
	void DebugFocus(int32 Id) { FocusId = Id; bNavMode = true; }
	void DebugActivate(int32 Id) { ActivateItem(Id, FVector2f::ZeroVector, false); }
	int32 GetFocusId() const { return FocusId; }
	FApexRect DebugRect(int32 Id) const { return RectOf(Id); }

protected:
	// ---- 子类实现 ----
	virtual void BuildItems(const FVector2f& S) = 0;
	virtual void PaintScreen(FApexPainter& P, const FVector2f& S) const = 0;
	virtual void TickScreen(float Dt) {}
	virtual void OnShown() {}
	virtual void OnHidden() {}
	virtual void OnActivate(int32 Id, const FVector2f& LocalPos, bool bMouse) {}
	/** 左右调节，返回 true 表示已处理（不再移动焦点） */
	virtual bool OnAdjust(int32 Id, int32 Dir) { return false; }
	/** 拖拽（滑条）。返回 true 表示该元素支持拖拽 */
	virtual bool OnDrag(int32 Id, const FVector2f& Pos, bool bEnd) { return false; }
	virtual bool OnBack() { return false; }
	/** Esc/P/Start 以外的菜单键；Start 默认等同 OnMenuKey */
	virtual bool OnMenuKey() { return false; }
	virtual bool OnShoulder(int32 Dir) { return false; }
	/** Z/C 或 LT/RT */
	virtual bool OnTrigger(int32 Dir) { return false; }
	virtual int32 DefaultFocus() const { return 0; }
	/** 该点是否挡住鼠标（不透给后面的 3D 视口） */
	virtual bool BlocksMouse(const FVector2f& P) const { return true; }

	// ---- 子类使用 ----
	FApexItem& Put(int32 Id, const FApexRect& R, bool bSelected = false, bool bEnabled = true);
	const FApexItem* Find(int32 Id) const;
	float HoverOf(int32 Id) const { const FApexItem* I = Find(Id); return I ? I->Hover : 0.0f; }
	float SelectOf(int32 Id) const { const FApexItem* I = Find(Id); return I ? I->Select : 0.0f; }
	float PressOf(int32 Id) const { const FApexItem* I = Find(Id); return I ? I->Press : 0.0f; }
	FApexRect RectOf(int32 Id) const { const FApexItem* I = Find(Id); return I ? I->Rect : FApexRect(); }
	bool IsFocused(int32 Id) const { return bNavMode && FocusId == Id; }
	bool IsHot(int32 Id) const { return bNavMode ? FocusId == Id : HoverId == Id; }
	void SetFocusId(int32 Id, bool bSound);
	void Click() const;
	FVector2f ToDesign(const FGeometry& Geo, const FVector2D& ScreenPos) const;
	/** 平行四边形焦点框 */
	void FocusRing(FApexPainter& P, const FApexRect& R, float K = ApexUI::Slant, float Pad = 6.0f) const;
	/** 底部按键提示（随输入设备切换键盘/手柄文案） */
	void HintBar(FApexPainter& P, float X, float CY, TConstArrayView<TTuple<FString, FString, FString>> Hints) const;

	TWeakObjectPtr<AApexDriftPlayerController> PC;
	TWeakObjectPtr<UApexDriftSaveGame> Save;

	TArray<FApexItem> Items;
	FVector2f DesignSize = FVector2f(1920.0f, 1080.0f);
	float DesignScale = 1.0f;
	float OpenT = 0.0f;       // 打开后经过的秒数
	float TimeNow = 0.0f;
	int32 FocusId = 0;
	int32 HoverId = 0;
	int32 PressId = 0;
	bool bDragging = false;
	bool bNavMode = false;    // 键盘/手柄导航中（显示焦点框）
	bool bGamepad = false;    // 最近输入来自手柄
	FVector2f MousePos = FVector2f(-1, -1);

private:
	void UpdateLayout(const FGeometry& Geo);
	void Rebuild();
	void ActivateItem(int32 Id, const FVector2f& LocalPos, bool bMouse);
	bool Navigate(int32 DX, int32 DY);
	int32 HitTest(const FVector2f& P) const;

	float StickX = 0.0f, StickY = 0.0f;
	float StickRepeat = 0.0f;
	int32 StickDirX = 0, StickDirY = 0;
	bool bShown = false;
};

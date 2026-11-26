#pragma once

#include "CoreMinimal.h"
#include "UI/ApexScreen.h"

/** 主菜单 */
class SApexMainMenu : public SApexScreen
{
public:
	enum EId : int32
	{
		IdMap0 = 10, IdMode0 = 20, IdWeather0 = 30, IdPaint0 = 40,
		IdStart = 60, IdPreview = 61, IdCarPrev = 62, IdCarNext = 63,
		IdNavSettings = 70, IdNavControls = 71, IdNavQuit = 72, IdAuto = 73,
		IdCloseControls = 80, IdQuitYes = 90, IdQuitNo = 91,
	};

	void ResetSelection();
	// 截图流程用
	void DebugSelectMap(int32 Map) { SelMap = Map; HeroMap = Map; HeroT = 1.0f; }

protected:
	virtual void BuildItems(const FVector2f& S) override;
	virtual void PaintScreen(FApexPainter& P, const FVector2f& S) const override;
	virtual void TickScreen(float Dt) override;
	virtual void OnShown() override;
	virtual void OnActivate(int32 Id, const FVector2f& LocalPos, bool bMouse) override;
	virtual bool OnBack() override;
	virtual bool OnMenuKey() override;
	virtual bool OnShoulder(int32 Dir) override;
	virtual bool OnTrigger(int32 Dir) override;
	virtual int32 DefaultFocus() const override;
	virtual bool BlocksMouse(const FVector2f& P) const override;

private:
	enum class EModal : uint8 { None, Controls, Quit };

	struct FLayout
	{
		float X0 = 0, X1 = 0, DockY = 0, DockH = 396;
		TArray<FApexRect, TInlineAllocator<6>> Card;
		float RX = 0, RW = 0;
		FApexRect Mode[3];
		FApexRect Weather[3];
		TArray<FVector2f, TInlineAllocator<12>> Paint;
		float PaintR = 17.0f;
		FApexRect Start;
		FApexRect Nav[3];
		FApexRect Preview;
		FApexRect CarPanel, CarPrev, CarNext;
		FApexRect Auto;
	};
	FLayout Layout(const FVector2f& S) const;
	int32 CurrentMap() const;
	int32 WeatherOf(int32 Map) const;
	void SelectMap(int32 Map);
	void SelectWeather(int32 Idx);
	int32 CurrentCar() const;
	void StepCar(int32 Dir);

	void PaintTop(FApexPainter& P, const FVector2f& S, const FLayout& L) const;
	void PaintHero(FApexPainter& P, const FVector2f& S, const FLayout& L) const;
	void PaintDock(FApexPainter& P, const FVector2f& S, const FLayout& L) const;
	void PaintCard(FApexPainter& P, const FLayout& L, int32 i) const;
	void PaintCar(FApexPainter& P, const FVector2f& S, const FLayout& L) const;
	void PaintModal(FApexPainter& P, const FVector2f& S) const;

	int32 SelMap = 0;
	int32 HeroMap = 0;
	float HeroT = 1.0f;
	float CarT = 1.0f;
	float CarStat[4] = { 0, 0, 0, 0 };
	EApexGameModeType SelMode = EApexGameModeType::FreeDrift;
	EModal Modal = EModal::None;
	float ModalT = 0.0f;
	FLinearColor Accent = FLinearColor::White;
	bool bAccentInit = false;
};

/** 暂停 */
class SApexPauseMenu : public SApexScreen
{
public:
	enum EId : int32 { IdResume = 10, IdReset, IdRestart, IdSettings, IdControls, IdMenu, IdCloseControls = 80 };

protected:
	virtual void BuildItems(const FVector2f& S) override;
	virtual void PaintScreen(FApexPainter& P, const FVector2f& S) const override;
	virtual void OnShown() override { bControls = false; }
	virtual void OnActivate(int32 Id, const FVector2f& LocalPos, bool bMouse) override;
	virtual bool OnBack() override;
	virtual bool OnMenuKey() override;
	virtual int32 DefaultFocus() const override { return bControls ? IdCloseControls : IdResume; }

private:
	FApexRect ItemRect(const FVector2f& S, int32 i) const;
	bool bControls = false;
};

/** 结算 */
class SApexFinish : public SApexScreen
{
public:
	enum EId : int32 { IdRetry = 10, IdChangeMap, IdMenu };
	void SetResult(const FApexDriftTelemetry& T, EApexGameModeType M, bool bRecord);

protected:
	virtual void BuildItems(const FVector2f& S) override;
	virtual void PaintScreen(FApexPainter& P, const FVector2f& S) const override;
	virtual void TickScreen(float Dt) override;
	virtual void OnShown() override;
	virtual void OnActivate(int32 Id, const FVector2f& LocalPos, bool bMouse) override;
	virtual bool OnBack() override;
	virtual int32 DefaultFocus() const override { return IdRetry; }

private:
	TCHAR Grade() const;
	FApexDriftTelemetry Tel;
	EApexGameModeType Mode = EApexGameModeType::FreeDrift;
	bool bNewRecord = false;
	float ShownScore = 0.0f;
	bool bGradeSound = false;
};

namespace ApexUI
{
	/** 主按钮（强调色实心平行四边形 + 流光） */
	void PrimaryButton(FApexPainter& P, const FApexRect& R, const FString& Zh, const FString& Sub, const FLinearColor& Accent, float Hover, float Press, float Time, float FontPx = 36.0f);
	/** 次按钮 */
	void GhostButton(FApexPainter& P, const FApexRect& R, const FString& Zh, EApexIcon I, float Hover, float Press, float FontPx = 22.0f);
	/** 全屏模糊 + 墨色遮罩 */
	void Backdrop(FApexPainter& P, const FVector2f& S, float BlurStrength, float InkA);
	/** 大标题 + 英文点缀 */
	void ScreenTitle(FApexPainter& P, float X, float CY, const FString& Zh, const FString& En, const FLinearColor& Accent, float Px = 60.0f);
	/** 全屏操作说明页 */
	void ControlsPage(FApexPainter& P, const FVector2f& S, const FLinearColor& Accent, float Time);
	FLinearColor MapAccent(int32 Map);
}

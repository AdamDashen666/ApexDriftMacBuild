#pragma once

#include "CoreMinimal.h"
#include "UI/ApexScreen.h"

/** 设置页：左侧分类 + 中间设置行 + 右侧当前项的详细说明 */
class SApexSettings : public SApexScreen
{
public:
	enum EId : int32 { IdTab0 = 10, IdRow0 = 100, IdDefaults = 60, IdBack = 61 };

	void Construct(const FArguments& InArgs);
	void DebugSetTab(int32 T) { Tab = T; TabT = 1.0f; FocusId = 0; }

protected:
	virtual void BuildItems(const FVector2f& S) override;
	virtual void PaintScreen(FApexPainter& P, const FVector2f& S) const override;
	virtual void TickScreen(float Dt) override;
	virtual void OnShown() override;
	virtual void OnHidden() override;
	virtual void OnActivate(int32 Id, const FVector2f& LocalPos, bool bMouse) override;
	virtual bool OnAdjust(int32 Id, int32 Dir) override;
	virtual bool OnDrag(int32 Id, const FVector2f& Pos, bool bEnd) override;
	virtual bool OnBack() override;
	virtual bool OnMenuKey() override { return OnBack(); }
	virtual bool OnShoulder(int32 Dir) override;
	virtual bool OnCaptureKey(const FKey& Key, bool bRepeat) override;
	virtual int32 DefaultFocus() const override;

private:
	enum class EType : uint8 { Choice, Slider, Key };
	struct FRow
	{
		EType Type = EType::Choice;
		FString Name;
		FString Desc;                       // 行内一句话
		TFunction<FString()> DynDesc;       // 有则代替 Desc（文字里带按键名时用）
		int32 Bind = -1;                    // EType::Key：EApexBind
		FString Detail;                     // 右侧长说明
		TArray<FString> Options;
		TArray<FString> OptionDescs;
		TFunction<int32(const UApexDriftSaveGame&)> Get;
		TFunction<void(UApexDriftSaveGame&, int32)> Set;
		TFunction<float(const UApexDriftSaveGame&)> GetF;
		TFunction<void(UApexDriftSaveGame&, float)> SetF;
		TFunction<FString(float)> Fmt;
		TFunction<FString(const UApexDriftSaveGame&)> DisabledWhy; // 返回非空 = 不可用
		float Min = 0.0f, Max = 1.0f, Step = 0.05f;
		bool bVideo = false;
	};
	struct FPage
	{
		FString Name;
		FString En;
		EApexIcon Icon = EApexIcon::None;
		TArray<FRow> Rows;
	};
	struct FLayout
	{
		float X0 = 0, ListX = 0, ListW = 900, DetailX = 0, DetailW = 468, Y0 = 176, RowH = 84;
		int32 PerCol = 0;                   // >0 时设置行分两列（按键页）
	};

	FLayout Layout(const FVector2f& S) const;
	FApexRect RowRect(const FLayout& L, int32 i) const;
	FApexRect SliderRect(const FApexRect& Row) const;
	FApexRect ArrowRect(const FApexRect& Row, bool bRight) const;
	FApexRect KeyCapRect(const FApexRect& Row) const;
	float ControlWidth(const FRow& R) const;
	static FString RowDesc(const FRow& R) { return R.DynDesc ? R.DynDesc() : R.Desc; }
	void StartCapture(int32 Row);
	void StopCapture(const FString& Msg);
	FString WhyDisabled(const FRow& R) const;
	const FRow* RowOf(int32 Id) const;
	void StepChoice(const FRow& R, int32 Dir);
	void SetSlider(const FRow& R, float V);
	void LivePreview() const;
	void Commit(bool bVideo);
	void SetTab(int32 T);
	void RestoreDefaults();
	void PaintRow(FApexPainter& P, const FLayout& L, int32 i) const;
	void PaintDetail(FApexPainter& P, const FLayout& L, const FVector2f& S) const;
	FLinearColor AccentColor() const;

	TArray<FPage> Pages;
	int32 Tab = 0;
	float TabT = 1.0f;
	int32 LastRow = 0;
	float PendingCommit = -1.0f;
	int32 CaptureRow = -1;              // 正在等新按键的行
	FKey SwallowKey;                    // 刚绑定的键，吞掉它的按住重复，免得拿来导航
	FString Notice;                     // 改键结果提示
	float NoticeT = 0.0f;
};

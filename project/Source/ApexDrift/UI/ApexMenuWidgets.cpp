#include "UI/ApexMenuWidgets.h"
#include "UI/ApexMenuScreens.h"
#include "UI/ApexSettingsScreen.h"
#include "Core/ApexDriftPlayerController.h"
#include "Core/ApexDriftSaveGame.h"

namespace
{
	/** 显示 / 隐藏通知 Slate 屏幕：重置入场动画并接管焦点 */
	void NotifyScreen(SApexScreen* Screen, ESlateVisibility Vis)
	{
		if (!Screen)
		{
			return;
		}
		if (Vis == ESlateVisibility::Collapsed || Vis == ESlateVisibility::Hidden)
		{
			Screen->NotifyHidden();
		}
		else
		{
			Screen->NotifyShown();
		}
	}
}

// =========================================================================
// 主菜单
// =========================================================================

void UApexMainMenuWidget::InitController(AApexDriftPlayerController* InPC, UApexDriftSaveGame* InSave)
{
	OwnerPC = InPC;
	SaveData = InSave;
	if (Screen.IsValid())
	{
		Screen->SetPlayer(InPC, InSave);
		Screen->ResetSelection();
	}
}

void UApexMainMenuWidget::RefreshLabels()
{
	// 所有数值绘制时实时读取，这里只需保证屏幕处于显示状态
	if (Screen.IsValid())
	{
		Screen->SetPlayer(OwnerPC, SaveData);
	}
}

void UApexMainMenuWidget::SetVisibility(ESlateVisibility InVisibility)
{
	Super::SetVisibility(InVisibility);
	NotifyScreen(Screen.Get(), InVisibility);
}

SApexScreen* UApexMainMenuWidget::GetScreen() const { return Screen.Get(); }

TSharedRef<SWidget> UApexMainMenuWidget::RebuildWidget()
{
	Screen = SNew(SApexMainMenu);
	Screen->SetPlayer(OwnerPC, SaveData);
	Screen->ResetSelection();
	NotifyScreen(Screen.Get(), GetVisibility());
	return Screen.ToSharedRef();
}

void UApexMainMenuWidget::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	Screen.Reset();
}

// =========================================================================
// 暂停
// =========================================================================

void UApexPauseMenuWidget::InitController(AApexDriftPlayerController* InPC)
{
	OwnerPC = InPC;
	if (Screen.IsValid())
	{
		Screen->SetPlayer(InPC, InPC ? InPC->GetSaveData() : nullptr);
	}
}

void UApexPauseMenuWidget::SetVisibility(ESlateVisibility InVisibility)
{
	Super::SetVisibility(InVisibility);
	NotifyScreen(Screen.Get(), InVisibility);
}

SApexScreen* UApexPauseMenuWidget::GetScreen() const { return Screen.Get(); }

TSharedRef<SWidget> UApexPauseMenuWidget::RebuildWidget()
{
	Screen = SNew(SApexPauseMenu);
	Screen->SetPlayer(OwnerPC, OwnerPC ? OwnerPC->GetSaveData() : nullptr);
	NotifyScreen(Screen.Get(), GetVisibility());
	return Screen.ToSharedRef();
}

void UApexPauseMenuWidget::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	Screen.Reset();
}

// =========================================================================
// 设置
// =========================================================================

void UApexSettingsWidget::InitController(AApexDriftPlayerController* InPC, UApexDriftSaveGame* InSave)
{
	OwnerPC = InPC;
	SaveData = InSave;
	if (Screen.IsValid())
	{
		Screen->SetPlayer(InPC, InSave);
	}
}

void UApexSettingsWidget::RefreshValues()
{
	if (Screen.IsValid())
	{
		Screen->SetPlayer(OwnerPC, SaveData);
	}
}

void UApexSettingsWidget::SetVisibility(ESlateVisibility InVisibility)
{
	Super::SetVisibility(InVisibility);
	NotifyScreen(Screen.Get(), InVisibility);
}

SApexScreen* UApexSettingsWidget::GetScreen() const { return Screen.Get(); }

TSharedRef<SWidget> UApexSettingsWidget::RebuildWidget()
{
	Screen = SNew(SApexSettings);
	Screen->SetPlayer(OwnerPC, SaveData);
	NotifyScreen(Screen.Get(), GetVisibility());
	return Screen.ToSharedRef();
}

void UApexSettingsWidget::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	Screen.Reset();
}

// =========================================================================
// 结算
// =========================================================================

void UApexFinishWidget::InitAndShow(AApexDriftPlayerController* InPC, const FApexDriftTelemetry& FinalTelemetry, EApexGameModeType ModeType, int32 InRecordFlags)
{
	OwnerPC = InPC;
	Telemetry = FinalTelemetry;
	Mode = ModeType;
	RecordFlags = InRecordFlags;
	if (Screen.IsValid())
	{
		Screen->SetPlayer(InPC, InPC ? InPC->GetSaveData() : nullptr);
		Screen->SetResult(Telemetry, Mode, RecordFlags);
	}
}

void UApexFinishWidget::SetVisibility(ESlateVisibility InVisibility)
{
	Super::SetVisibility(InVisibility);
	NotifyScreen(Screen.Get(), InVisibility);
}

SApexScreen* UApexFinishWidget::GetScreen() const { return Screen.Get(); }

TSharedRef<SWidget> UApexFinishWidget::RebuildWidget()
{
	Screen = SNew(SApexFinish);
	Screen->SetPlayer(OwnerPC, OwnerPC ? OwnerPC->GetSaveData() : nullptr);
	Screen->SetResult(Telemetry, Mode, RecordFlags);
	NotifyScreen(Screen.Get(), GetVisibility());
	return Screen.ToSharedRef();
}

void UApexFinishWidget::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	Screen.Reset();
}

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Core/ApexDriftTypes.h"
#include "ApexMenuWidgets.generated.h"

class AApexDriftPlayerController;
class UApexDriftSaveGame;
class SApexScreen;
class SApexMainMenu;
class SApexPauseMenu;
class SApexSettings;
class SApexFinish;

/** 主菜单：一屏完成 选赛道 → 选模式 → 选天气 → 选漆色 → 开始 */
UCLASS()
class APEXDRIFT_API UApexMainMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitController(AApexDriftPlayerController* InPC, UApexDriftSaveGame* InSave);
	void RefreshLabels();
	virtual void SetVisibility(ESlateVisibility InVisibility) override;
	SApexScreen* GetScreen() const;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

private:
	UPROPERTY()
	TObjectPtr<AApexDriftPlayerController> OwnerPC;
	UPROPERTY()
	TObjectPtr<UApexDriftSaveGame> SaveData;

	TSharedPtr<SApexMainMenu> Screen;
};

UCLASS()
class APEXDRIFT_API UApexPauseMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitController(AApexDriftPlayerController* InPC);
	virtual void SetVisibility(ESlateVisibility InVisibility) override;
	SApexScreen* GetScreen() const;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

private:
	UPROPERTY()
	TObjectPtr<AApexDriftPlayerController> OwnerPC;

	TSharedPtr<SApexPauseMenu> Screen;
};

UCLASS()
class APEXDRIFT_API UApexSettingsWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitController(AApexDriftPlayerController* InPC, UApexDriftSaveGame* InSave);
	void RefreshValues();
	virtual void SetVisibility(ESlateVisibility InVisibility) override;
	SApexScreen* GetScreen() const;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

private:
	UPROPERTY()
	TObjectPtr<AApexDriftPlayerController> OwnerPC;
	UPROPERTY()
	TObjectPtr<UApexDriftSaveGame> SaveData;

	TSharedPtr<SApexSettings> Screen;
};

UCLASS()
class APEXDRIFT_API UApexFinishWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitAndShow(AApexDriftPlayerController* InPC, const FApexDriftTelemetry& FinalTelemetry, EApexGameModeType ModeType, int32 RecordFlags);
	virtual void SetVisibility(ESlateVisibility InVisibility) override;
	SApexScreen* GetScreen() const;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

private:
	UPROPERTY()
	TObjectPtr<AApexDriftPlayerController> OwnerPC;

	FApexDriftTelemetry Telemetry;
	EApexGameModeType Mode = EApexGameModeType::FreeDrift;
	int32 RecordFlags = 0;

	TSharedPtr<SApexFinish> Screen;
};

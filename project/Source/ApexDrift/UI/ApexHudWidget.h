#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Core/ApexDriftTypes.h"
#include "ApexHudWidget.generated.h"

class SApexHudCanvas;

/** 驾驶 HUD：单个自绘叶子控件，不含模糊，不吃鼠标 */
UCLASS()
class APEXDRIFT_API UApexHudWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void UpdateTelemetry(const FApexDriftTelemetry& InTelemetry, EApexGameModeType InMode, EApexGameState InState, float InCountdownSec, const TArray<FVector2D>& InTrackMinimapPts);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

private:
	TSharedPtr<SApexHudCanvas> HudCanvas;
};

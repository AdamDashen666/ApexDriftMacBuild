#pragma once

#include "CoreMinimal.h"

class SWidget;

/** 关卡加载画面（在 MoviePlayer 线程绘制） */
TSharedRef<SWidget> MakeApexLoadingScreen(int32 MapIndex);

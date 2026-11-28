#pragma once

#include "CoreMinimal.h"

#if PLATFORM_WINDOWS
/**
 * DLSS 5 神经渲染（第三方 OptiScaler-NR）的游戏内设置。
 * OptiScaler 只在启动时读一次 OptiScaler.ini；运行中只认 [DlssNr] ToggleKey 这个快捷键开关神经渲染
 * （它的 menu_common.cpp：UpdateManualInput / HandleMenuShortcuts）。所以：
 * - 开 / 关：游戏把 ToggleKey 设成 F24（键盘上一般没有），切换时给自己的窗口发一次 F24 按下 / 松开，立即生效；
 *   同时写进 INI，下次启动保持。ToggleKey 是第一次启动时才写上的，那一次要重启后才能即时切换。
 * - 次数：没有运行中的入口，只能写 INI，重启游戏后生效。
 * 游戏看不到 OptiScaler 内部状态：玩家在 OptiScaler 自己的菜单（Insert）里改了又没保存，界面上的“当前”就不准。
 */
namespace ApexNeuralRendering
{
	enum class EStatus : uint8 { Missing, ReadFailed, Ready, WriteFailed };

	struct FSettings
	{
		EStatus Status = EStatus::Missing;
		bool bEnabled = false;          // INI 里的值（玩家选的，下次启动用）
		int32 Passes = 1;               // INI 里的值（下次启动用）
		bool bEnabledAtLaunch = false;  // 本次启动时 OptiScaler 读到的
		int32 PassesAtLaunch = 1;       // 本次启动时 OptiScaler 读到的 = 现在实际的次数
		bool bRunningEnabled = false;   // 现在实际开着没有（启动值 + 本局用快捷键切过的）
		bool bLiveToggle = false;       // 本次启动时 ToggleKey 已经是 F24：开关能立即生效
		bool bTogglePending = false;    // 还有切换没发完（OptiScaler 两次快捷键至少隔 1 秒）
	};

	/** 游戏启动时调用一次：认出 OptiScaler、记下它本次读到的配置，没有开关快捷键就写上 F24（下次启动起生效） */
	void Initialize();
	const FSettings& GetSettings();
	void SetEnabled(bool bEnabled);
	void SetPasses(int32 Passes);
	/** 有改动要重启才生效（次数，或这次还不能即时切换的开关） */
	bool NeedsRestart();
	/** 用同样的命令行重新启动游戏 */
	bool RestartGame();

	/** -ApexNrSelfTest=<OptiScaler.ini 的副本>：只在内存里测 INI 读写，结果写到 Saved/Bench/nr_selftest.txt */
	bool RunSelfTest(const FString& SampleIniPath, TArray<FString>& OutReport);
}
#endif

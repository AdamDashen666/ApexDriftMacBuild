#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"

class UApexDriftSaveGame;

/** 可以改键的驾驶动作（键盘）。方向键、手柄、Esc / P 暂停固定不变 */
enum class EApexBind : uint8
{
	Throttle,
	Brake,
	SteerLeft,
	SteerRight,
	Handbrake,
	ShiftUp,
	ShiftDown,
	Camera,
	LookBack,
	Reset,
	Headlights,
	AutoRun,
	Director,
	Count
};

/**
 * 键盘改键：当前键位存在全局表里（单人游戏只有一份），由玩家控制器读档时载入；
 * 界面各处的按键提示都从这里取名字，改键后 Version() 加一，界面据此刷新。
 */
namespace ApexKeys
{
	constexpr int32 Num = static_cast<int32>(EApexBind::Count);

	struct FInfo
	{
		const TCHAR* Id;      // 存档里的名字，不能改
		const TCHAR* Label;   // 设置页的名字
		const TCHAR* Short;   // 键盘示意图上的两字标签
		const TCHAR* Desc;
		const TCHAR* Fixed;   // 不能改的其他键
		FKey Default;
	};

	const FInfo& Info(EApexBind B);

	/** 从存档载入（没改过的用默认） */
	void Load(const UApexDriftSaveGame* Save);
	FKey Get(EApexBind B);
	FKey GetDefault(EApexBind B);
	/** 改键并写进存档（不落盘）。键已被别的动作占用时两者互换，返回被换走的动作，没有则 Count */
	EApexBind Set(UApexDriftSaveGame* Save, EApexBind B, const FKey& K);
	void ResetAll(UApexDriftSaveGame* Save);
	bool IsAllDefault();

	/** 只能绑键盘键；Esc、P、回车、Tab、方向键另有固定用途。不能绑时 OutWhy 给出原因 */
	bool CanBind(const FKey& K, FString* OutWhy = nullptr);
	/** 绑在这个键上的动作，没有返回 Count */
	EApexBind Find(const FKey& K);

	/** 界面上的按键名：字母原样，空格、左 Shift 之类写中文 */
	FString KeyName(const FKey& K);
	FString Name(EApexBind B);

	uint32 Version();
}

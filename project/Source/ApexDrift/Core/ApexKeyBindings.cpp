#include "Core/ApexKeyBindings.h"
#include "Core/ApexDriftSaveGame.h"

namespace
{
	const ApexKeys::FInfo& InfoAt(int32 i)
	{
		static const ApexKeys::FInfo Table[ApexKeys::Num] = {
			{ TEXT("Throttle"),   TEXT("油门"),     TEXT("油门"), TEXT("加速"),           TEXT("方向键 ↑ · 手柄 RT"),     EKeys::W },
			{ TEXT("Brake"),      TEXT("刹车 / 倒车"), TEXT("刹车"), TEXT("减速，停住后按住倒车"),   TEXT("方向键 ↓ · 手柄 LT"),     EKeys::S },
			{ TEXT("SteerLeft"),  TEXT("左转"),     TEXT("左转"), TEXT("方向往左打"),                     TEXT("方向键 ← · 手柄左摇杆"),  EKeys::A },
			{ TEXT("SteerRight"), TEXT("右转"),     TEXT("右转"), TEXT("方向往右打"),                     TEXT("方向键 → · 手柄左摇杆"),  EKeys::D },
			{ TEXT("Handbrake"),  TEXT("手刹"),     TEXT("手刹"), TEXT("点一下起漂，别一直按住"),         TEXT("手柄 A / X"),             EKeys::SpaceBar },
			{ TEXT("ShiftUp"),    TEXT("升挡"),     TEXT("升挡"), TEXT("手动挡时升一挡"),                 TEXT("手柄 RB"),                EKeys::E },
			{ TEXT("ShiftDown"),  TEXT("降挡"),     TEXT("降挡"), TEXT("手动挡时降一挡"),                 TEXT("手柄 LB"),                EKeys::Q },
			{ TEXT("Camera"),     TEXT("切换视角"), TEXT("视角"), TEXT("几个机位轮流切换"), TEXT("手柄 Y"),                 EKeys::C },
			{ TEXT("LookBack"),   TEXT("向后看"),   TEXT("后视"), TEXT("按住看车后"),                     TEXT("手柄右摇杆按下"),         EKeys::B },
			{ TEXT("Reset"),      TEXT("复位"),     TEXT("复位"), TEXT("回到赛道；训练场连按两次回起点"), TEXT("手柄 View"), EKeys::R },
			{ TEXT("Headlights"), TEXT("车灯"),     TEXT("车灯"), TEXT("开关前大灯"),                     TEXT("手柄十字键 ↑"),           EKeys::H },
			{ TEXT("AutoRun"),    TEXT("自动漂移"), TEXT("自动"), TEXT("自动漂一圈，按驾驶键接管"), TEXT("手柄十字键 ↓"),      EKeys::F },
			{ TEXT("Director"),   TEXT("自动运镜"), TEXT("运镜"), TEXT("自动漂移时开关电影运镜"),         TEXT("手柄十字键 →"),           EKeys::V },
		};
		return Table[FMath::Clamp(i, 0, ApexKeys::Num - 1)];
	}

	FKey GKeys[ApexKeys::Num];
	bool GLoaded = false;
	uint32 GVersion = 1;

	void EnsureLoaded()
	{
		if (!GLoaded)
		{
			for (int32 i = 0; i < ApexKeys::Num; ++i) GKeys[i] = InfoAt(i).Default;
			GLoaded = true;
		}
	}

	void WriteSave(UApexDriftSaveGame* Save)
	{
		if (!Save) return;
		Save->KeyBinds.Reset();
		for (int32 i = 0; i < ApexKeys::Num; ++i)
		{
			// 只存改过的，以后调整默认键位时没改过的玩家自动跟上
			if (GKeys[i] != InfoAt(i).Default)
			{
				Save->KeyBinds.Add(FName(InfoAt(i).Id), GKeys[i]);
			}
		}
	}
}

namespace ApexKeys
{
	const FInfo& Info(EApexBind B) { return InfoAt(static_cast<int32>(B)); }

	void Load(const UApexDriftSaveGame* Save)
	{
		GLoaded = false;
		EnsureLoaded();
		if (Save)
		{
			for (int32 i = 0; i < Num; ++i)
			{
				const FKey* K = Save->KeyBinds.Find(FName(InfoAt(i).Id));
				if (K && CanBind(*K))
				{
					GKeys[i] = *K;
				}
			}
			// 存档被手改出重复键时整套退回默认，免得两个动作抢一个键
			bool bDup = false;
			for (int32 i = 0; i < Num && !bDup; ++i)
			{
				for (int32 j = 0; j < i; ++j)
				{
					if (GKeys[j] == GKeys[i]) { bDup = true; break; }
				}
			}
			if (bDup)
			{
				for (int32 i = 0; i < Num; ++i) GKeys[i] = InfoAt(i).Default;
			}
		}
		++GVersion;
	}

	FKey Get(EApexBind B)
	{
		EnsureLoaded();
		return GKeys[FMath::Clamp(static_cast<int32>(B), 0, Num - 1)];
	}

	FKey GetDefault(EApexBind B) { return Info(B).Default; }

	EApexBind Find(const FKey& K)
	{
		EnsureLoaded();
		for (int32 i = 0; i < Num; ++i)
		{
			if (GKeys[i] == K) return static_cast<EApexBind>(i);
		}
		return EApexBind::Count;
	}

	EApexBind Set(UApexDriftSaveGame* Save, EApexBind B, const FKey& K)
	{
		EnsureLoaded();
		const int32 i = static_cast<int32>(B);
		if (i < 0 || i >= Num || !CanBind(K) || GKeys[i] == K)
		{
			return EApexBind::Count;
		}
		const EApexBind Other = Find(K);
		if (Other != EApexBind::Count)
		{
			GKeys[static_cast<int32>(Other)] = GKeys[i];
		}
		GKeys[i] = K;
		WriteSave(Save);
		++GVersion;
		return Other;
	}

	void ResetAll(UApexDriftSaveGame* Save)
	{
		for (int32 i = 0; i < Num; ++i) GKeys[i] = InfoAt(i).Default;
		GLoaded = true;
		WriteSave(Save);
		++GVersion;
	}

	bool IsAllDefault()
	{
		EnsureLoaded();
		for (int32 i = 0; i < Num; ++i)
		{
			if (GKeys[i] != InfoAt(i).Default) return false;
		}
		return true;
	}

	bool CanBind(const FKey& K, FString* OutWhy)
	{
		auto No = [OutWhy](const TCHAR* Why) { if (OutWhy) *OutWhy = Why; return false; };
		if (!K.IsValid() || K == EKeys::AnyKey) return No(TEXT("无效的按键"));
		if (K.IsGamepadKey()) return No(TEXT("手柄按键是固定的，这里只能改键盘"));
		if (K.IsMouseButton() || K.IsAnalog() || K.IsTouch()) return No(TEXT("只能用键盘按键"));
		if (K == EKeys::Escape || K == EKeys::P) return No(TEXT("Esc 和 P 固定用来暂停"));
		if (K == EKeys::Enter || K == EKeys::Tab) return No(TEXT("回车和 Tab 留给菜单使用"));
		if (K == EKeys::Up || K == EKeys::Down || K == EKeys::Left || K == EKeys::Right) return No(TEXT("方向键固定用于驾驶"));
		return true;
	}

	FString KeyName(const FKey& K)
	{
		if (!K.IsValid()) return TEXT("—");
		if (K == EKeys::SpaceBar) return TEXT("空格");
		if (K == EKeys::LeftShift) return TEXT("左 Shift");
		if (K == EKeys::RightShift) return TEXT("右 Shift");
		if (K == EKeys::BackSpace) return TEXT("退格");
		if (K == EKeys::CapsLock) return TEXT("Caps");
		if (K == EKeys::Escape) return TEXT("Esc");
#if PLATFORM_MAC
		if (K == EKeys::LeftControl) return TEXT("左 Control");
		if (K == EKeys::RightControl) return TEXT("右 Control");
		if (K == EKeys::LeftAlt) return TEXT("左 Option");
		if (K == EKeys::RightAlt) return TEXT("右 Option");
		if (K == EKeys::LeftCommand) return TEXT("左 Command");
		if (K == EKeys::RightCommand) return TEXT("右 Command");
#else
		if (K == EKeys::LeftControl) return TEXT("左 Ctrl");
		if (K == EKeys::RightControl) return TEXT("右 Ctrl");
		if (K == EKeys::LeftAlt) return TEXT("左 Alt");
		if (K == EKeys::RightAlt) return TEXT("右 Alt");
#endif
		return K.GetDisplayName(false).ToString();
	}

	FString Name(EApexBind B) { return KeyName(Get(B)); }

	uint32 Version() { return GVersion; }
}

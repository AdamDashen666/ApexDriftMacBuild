#include "Core/ApexNeuralRenderingSettings.h"

#if PLATFORM_WINDOWS
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "GenericPlatform/GenericWindow.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "String/Find.h"
#include "Widgets/SWindow.h"
#include "Windows/AllowWindowsPlatformTypes.h"
#include <psapi.h>
#include "Windows/HideWindowsPlatformTypes.h"

DEFINE_LOG_CATEGORY_STATIC(LogApexNR, Log, All);

namespace ApexNeuralRendering
{
	namespace
	{
		constexpr int32 GameToggleVk = 0x87;      // VK_F24：游戏专用的神经渲染开关键
		constexpr double ToggleSpacing = 1.25;   // OptiScaler 两次快捷键至少隔 1 秒（menu_common.cpp debounceThreshold）
		constexpr double KeyHold = 0.08;         // 按下到松开，像一次真实点按（OptiScaler 看松开那一下）

		struct FIniValues
		{
			bool bEnabled = false;   // OptiScaler 只认 true / false，其他（auto、1、缺省）都是默认值 false
			int32 Passes = 1;        // auto / 缺省 / 无效 = 1，限制 1..3
			int32 ToggleKey = 0;     // auto / 缺省 = 0（没有快捷键）
		};

		struct FState
		{
			FSettings S;
			bool bInitialized = false;
			FString IniPath;
			FDateTime FileStamp;
			double LastPoll = -1.0;
			bool bWantEnabled = false;       // 玩家最后选的开关（即时切换的目标）
			double LastToggle = -100.0;
			double KeyUpAt = -1.0;           // >0：到这个时间发松开
			FTSTicker::FDelegateHandle Ticker;
		};

		FState& St()
		{
			static FState S;
			return S;
		}

		// ---------------- INI 文本（纯函数，自测也用） ----------------

		// [DlssNr] 段里 Key 的位置：值的起止；段头之后（插入新键用）。同名键以最后一个为准（和 OptiScaler 用的 SimpleIni 一样）
		struct FKeyPos
		{
			int32 ValueStart = INDEX_NONE;
			int32 ValueLen = 0;
			int32 AfterHeader = INDEX_NONE;
		};

		FKeyPos FindKey(const FString& C, const TCHAR* Key)
		{
			FKeyPos P;
			bool bIn = false;
			int32 Pos = 0;
			const int32 N = C.Len();
			while (Pos < N)
			{
				const int32 Eol = C.Find(TEXT("\n"), ESearchCase::CaseSensitive, ESearchDir::FromStart, Pos);
				const int32 LineEnd = Eol == INDEX_NONE ? N : Eol;
				const int32 Next = Eol == INDEX_NONE ? N : Eol + 1;
				const FStringView Line(*C + Pos, LineEnd - Pos);
				const FStringView Trim = Line.TrimStartAndEnd();
				if (Trim.StartsWith(TEXT('[')))
				{
					bIn = Trim.Equals(TEXT("[DlssNr]"), ESearchCase::IgnoreCase);
					if (bIn && P.AfterHeader == INDEX_NONE) P.AfterHeader = Next;
				}
				else if (bIn && !Trim.StartsWith(TEXT(';')) && !Trim.StartsWith(TEXT('#')))
				{
					int32 Eq;
					if (Line.FindChar(TEXT('='), Eq) && Line.Left(Eq).TrimStartAndEnd().Equals(Key, ESearchCase::IgnoreCase))
					{
						int32 VS = Pos + Eq + 1, VE = LineEnd;
						for (int32 i = VS; i < VE; ++i)
						{
							if (C[i] == TEXT(';')) { VE = i; break; }   // 行内注释
						}
						while (VS < VE && FChar::IsWhitespace(C[VS])) ++VS;
						while (VE > VS && FChar::IsWhitespace(C[VE - 1])) --VE;   // 含 \r
						P.ValueStart = VS;
						P.ValueLen = VE - VS;
					}
				}
				Pos = Next;
			}
			return P;
		}

		FString GetValue(const FString& C, const TCHAR* Key)
		{
			const FKeyPos P = FindKey(C, Key);
			return P.ValueStart == INDEX_NONE ? FString() : C.Mid(P.ValueStart, P.ValueLen);
		}

		int32 ParseInt(const FString& V, int32 Fallback)
		{
			if (V.IsEmpty()) return Fallback;
			const bool bHex = V.StartsWith(TEXT("0x"), ESearchCase::IgnoreCase) && V.Len() > 2;
			const FString Digits = bHex ? V.Mid(2) : V;
			for (TCHAR Ch : Digits)
			{
				if (!(bHex ? FChar::IsHexDigit(Ch) : FChar::IsDigit(Ch))) return Fallback;
			}
			return static_cast<int32>(FCString::Strtoi(*Digits, nullptr, bHex ? 16 : 10));
		}

		FIniValues Parse(const FString& C)
		{
			FIniValues V;
			V.bEnabled = GetValue(C, TEXT("Enabled")).Equals(TEXT("true"), ESearchCase::IgnoreCase);
			const int32 Passes = ParseInt(GetValue(C, TEXT("Passes")), 1);
			V.Passes = FMath::Clamp(Passes <= 0 ? 1 : Passes, 1, 3);
			V.ToggleKey = ParseInt(GetValue(C, TEXT("ToggleKey")), 0);
			return V;
		}

		// 改 [DlssNr] 里的键（保留行内注释和换行风格）；没有这个键就加在段头后面，没有这一段就补在文件末尾
		void SetValue(FString& C, const TCHAR* Key, const FString& Value)
		{
			const FKeyPos P = FindKey(C, Key);
			if (P.ValueStart != INDEX_NONE)
			{
				C.RemoveAt(P.ValueStart, P.ValueLen, EAllowShrinking::No);
				C.InsertAt(P.ValueStart, Value);
				return;
			}
			const FString Eol = C.Contains(TEXT("\r\n")) ? FString(TEXT("\r\n")) : FString(TEXT("\n"));
			const FString Line = FString::Printf(TEXT("%s=%s"), Key, *Value) + Eol;
			if (P.AfterHeader != INDEX_NONE)
			{
				int32 At = P.AfterHeader;
				if (At >= C.Len() && !C.EndsWith(TEXT("\n")))
				{
					C += Eol;   // 段头是最后一行且没有换行
					At = C.Len();
				}
				C.InsertAt(At, Line);
				return;
			}
			if (!C.IsEmpty() && !C.EndsWith(TEXT("\n"))) C += Eol;
			C += Eol + TEXT("[DlssNr]") + Eol + Line;
		}

		// ---------------- 文件 ----------------

		bool LoadIni(const FString& Path, FString& Out, bool* bOutBom = nullptr)
		{
			TArray<uint8> Bytes;
			if (!FFileHelper::LoadFileToArray(Bytes, *Path, FILEREAD_Silent)) return false;
			if (bOutBom) *bOutBom = Bytes.Num() >= 3 && Bytes[0] == 0xEF && Bytes[1] == 0xBB && Bytes[2] == 0xBF;
			FFileHelper::BufferToString(Out, Bytes.GetData(), Bytes.Num());
			return true;
		}

		// 先写临时文件再替换，OptiScaler 或玩家同时读也不会读到半截
		bool EditIni(TFunctionRef<void(FString&)> Edit)
		{
			FState& S = St();
			FString C;
			bool bBom = false;
			if (!LoadIni(S.IniPath, C, &bBom)) return false;
			Edit(C);
			const FString Temp = FPaths::CreateTempFilename(*FPaths::GetPath(S.IniPath), TEXT("ApexDlssNr-"), TEXT(".tmp"));
			const bool bOk = FFileHelper::SaveStringToFile(C, *Temp, bBom ? FFileHelper::EEncodingOptions::ForceUTF8 : FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM) &&
				::MoveFileExW(*Temp, *S.IniPath, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
			if (!bOk)
			{
				IFileManager::Get().Delete(*Temp, false, false, true);
				UE_LOG(LogApexNR, Warning, TEXT("写 %s 失败"), *S.IniPath);
				return false;
			}
			S.FileStamp = IFileManager::Get().GetTimeStamp(*S.IniPath);
			return true;
		}

		// ---------------- OptiScaler 在不在 ----------------

		// 看本进程加载的模块里有没有版本信息写着 OptiScaler 的（它可以用 dxgi / winmm / version / dbghelp / wininet 等任何代理名装）
		bool FindOptiScaler(FString& OutModulePath)
		{
			HMODULE Mods[1024];
			DWORD Needed = 0;
			if (!::K32EnumProcessModules(::GetCurrentProcess(), Mods, sizeof(Mods), &Needed)) return false;
			const int32 Count = FMath::Min<int32>(static_cast<int32>(Needed / sizeof(HMODULE)), UE_ARRAY_COUNT(Mods));
			for (int32 i = 0; i < Count; ++i)
			{
				HRSRC Res = ::FindResourceW(Mods[i], MAKEINTRESOURCEW(VS_VERSION_INFO), RT_VERSION);
				if (!Res) continue;
				HGLOBAL Mem = ::LoadResource(Mods[i], Res);
				const void* Data = Mem ? ::LockResource(Mem) : nullptr;
				const DWORD Size = ::SizeofResource(Mods[i], Res);
				if (!Data || Size < 64) continue;
				const FStringView Text(static_cast<const TCHAR*>(Data), static_cast<int32>(Size / sizeof(TCHAR)));
				if (UE::String::FindFirst(Text, TEXT("OptiScaler"), ESearchCase::CaseSensitive) == INDEX_NONE) continue;
				TCHAR Path[MAX_PATH] = {};
				if (::GetModuleFileNameW(Mods[i], Path, MAX_PATH) == 0) continue;
				OutModulePath = Path;
				return true;
			}
			return false;
		}

		// ---------------- 即时切换 ----------------

		HWND GameWindow()
		{
			if (GEngine && GEngine->GameViewport)
			{
				if (TSharedPtr<SWindow> Window = GEngine->GameViewport->GetWindow())
				{
					if (TSharedPtr<FGenericWindow> Native = Window->GetNativeWindow())
					{
						return static_cast<HWND>(Native->GetOSWindowHandle());
					}
				}
			}
			return nullptr;
		}

		// 给游戏自己的窗口发 F24：OptiScaler 子类化了这个窗口，按 WM_KEYDOWN / WM_KEYUP 记按键（input_system_messages.cpp）
		bool PostToggleKey(bool bDown)
		{
			HWND Wnd = GameWindow();
			if (!Wnd) return false;
			const UINT Scan = ::MapVirtualKeyW(GameToggleVk, MAPVK_VK_TO_VSC);
			const LPARAM L = static_cast<LPARAM>(1u | (Scan << 16) | (bDown ? 0u : 0xC0000000u));
			return ::PostMessageW(Wnd, bDown ? WM_KEYDOWN : WM_KEYUP, static_cast<WPARAM>(GameToggleVk), L) != 0;
		}

		bool Tick(float)
		{
			FState& S = St();
			const double Now = FPlatformTime::Seconds();
			if (S.KeyUpAt > 0.0 && Now >= S.KeyUpAt)
			{
				PostToggleKey(false);
				S.KeyUpAt = -1.0;
			}
			if (S.KeyUpAt < 0.0 && S.S.bLiveToggle && S.bWantEnabled != S.S.bRunningEnabled && Now - S.LastToggle >= ToggleSpacing)
			{
				if (PostToggleKey(true))
				{
					S.KeyUpAt = Now + KeyHold;
					S.LastToggle = Now;
					S.S.bRunningEnabled = !S.S.bRunningEnabled;
					UE_LOG(LogApexNR, Log, TEXT("发送 DLSS 5 开关键（F24），神经渲染 -> %s"), S.S.bRunningEnabled ? TEXT("开") : TEXT("关"));
				}
			}
			S.S.bTogglePending = S.S.bLiveToggle && (S.bWantEnabled != S.S.bRunningEnabled || S.KeyUpAt > 0.0);
			return true;
		}

		// INI 被别人改了（多半是 OptiScaler 菜单里点了保存，它保存的就是它当时在跑的值）：跟着更新
		void Refresh()
		{
			FState& S = St();
			if (S.S.Status != EStatus::Ready && S.S.Status != EStatus::WriteFailed) return;
			const double Now = FPlatformTime::Seconds();
			if (Now - S.LastPoll < 0.5) return;
			S.LastPoll = Now;
			const FDateTime Stamp = IFileManager::Get().GetTimeStamp(*S.IniPath);
			if (Stamp == S.FileStamp) return;
			S.FileStamp = Stamp;
			FString C;
			if (!LoadIni(S.IniPath, C)) return;
			const FIniValues V = Parse(C);
			S.S.bEnabled = V.bEnabled;
			S.S.Passes = V.Passes;
			if (!S.S.bTogglePending)
			{
				S.bWantEnabled = V.bEnabled;
				S.S.bRunningEnabled = V.bEnabled;
			}
		}
	}

	void Initialize()
	{
		FState& S = St();
		if (S.bInitialized) return;
		S.bInitialized = true;
		if (IsRunningCommandlet()) return;

		FString ModulePath;
		FString FakeIni;
		if (FParse::Value(FCommandLine::Get(), TEXT("ApexNrFakeIni="), FakeIni))
		{
			// 测试：没有 OptiScaler 时假装装了，只读写这份 INI（看设置页各种状态、F24 会不会干扰游戏）
			S.IniPath = FakeIni;
			ModulePath = TEXT("（测试）") + FakeIni;
		}
		else
		{
			if (GIsEditor || !FindOptiScaler(ModulePath))
			{
				UE_LOG(LogApexNR, Log, TEXT("没有加载 OptiScaler，DLSS 5 设置不可用"));
				return;
			}
			// OptiScaler 读的是它自己 DLL 旁边的 OptiScaler.ini（Config.cpp：Util::DllPath().parent_path()）
			S.IniPath = FPaths::Combine(FPaths::GetPath(ModulePath), TEXT("OptiScaler.ini"));
		}
		FString C;
		if (!LoadIni(S.IniPath, C))
		{
			S.S.Status = EStatus::ReadFailed;
			UE_LOG(LogApexNR, Warning, TEXT("读不到 %s"), *S.IniPath);
			return;
		}
		const FIniValues V = Parse(C);
		S.S.Status = EStatus::Ready;
		S.S.bEnabled = S.S.bEnabledAtLaunch = S.S.bRunningEnabled = S.bWantEnabled = V.bEnabled;
		S.S.Passes = S.S.PassesAtLaunch = V.Passes;
		S.S.bLiveToggle = V.ToggleKey == GameToggleVk;
		S.FileStamp = IFileManager::Get().GetTimeStamp(*S.IniPath);
		if (V.ToggleKey <= 0)
		{
			// 还没有开关快捷键：写上 F24，下次启动起开关就能即时生效（玩家自己设了别的键就不动它）
			if (!EditIni([](FString& X) { SetValue(X, TEXT("ToggleKey"), FString::FromInt(GameToggleVk)); }))
			{
				S.S.Status = EStatus::WriteFailed;
			}
		}
		S.Ticker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&Tick), 0.0f);
		UE_LOG(LogApexNR, Log, TEXT("OptiScaler：%s，神经渲染 %s，%d 次，开关键 %d（%s）"), *ModulePath, V.bEnabled ? TEXT("开") : TEXT("关"), V.Passes, V.ToggleKey,
			S.S.bLiveToggle ? TEXT("可即时切换") : TEXT("本次启动不能即时切换"));
		if (!FakeIni.IsEmpty() && FParse::Param(FCommandLine::Get(), TEXT("ApexNrFakeChange")))
		{
			SetEnabled(!V.bEnabled);           // 能即时切换时会发一次 F24
			SetPasses(V.Passes % 3 + 1);       // 次数要重启：设置页应出现“立即重启生效”
		}
	}

	const FSettings& GetSettings()
	{
		Initialize();
		Refresh();
		return St().S;
	}

	void SetEnabled(bool bEnabled)
	{
		FState& S = St();
		if (S.S.Status != EStatus::Ready && S.S.Status != EStatus::WriteFailed) return;
		S.bWantEnabled = bEnabled;   // 能即时切换时 Tick 里发快捷键
		if (EditIni([bEnabled](FString& X) { SetValue(X, TEXT("Enabled"), bEnabled ? TEXT("true") : TEXT("false")); }))
		{
			S.S.Status = EStatus::Ready;
			S.S.bEnabled = bEnabled;
		}
		else
		{
			S.S.Status = EStatus::WriteFailed;
		}
		S.S.bTogglePending = S.S.bLiveToggle && S.bWantEnabled != S.S.bRunningEnabled;
	}

	void SetPasses(int32 Passes)
	{
		FState& S = St();
		if (S.S.Status != EStatus::Ready && S.S.Status != EStatus::WriteFailed) return;
		Passes = FMath::Clamp(Passes, 1, 3);
		if (EditIni([Passes](FString& X) { SetValue(X, TEXT("Passes"), FString::FromInt(Passes)); }))
		{
			S.S.Status = EStatus::Ready;
			S.S.Passes = Passes;
		}
		else
		{
			S.S.Status = EStatus::WriteFailed;
		}
	}

	bool NeedsRestart()
	{
		const FSettings& S = GetSettings();
		if (S.Status != EStatus::Ready && S.Status != EStatus::WriteFailed) return false;
		return S.Passes != S.PassesAtLaunch || (!S.bLiveToggle && S.bEnabled != S.bEnabledAtLaunch);
	}

	bool RestartGame()
	{
		const FString Exe = FPlatformProcess::ExecutablePath();
		FProcHandle Proc = FPlatformProcess::CreateProc(*Exe, FCommandLine::GetOriginal(), true, false, false, nullptr, 0, nullptr, nullptr);
		if (!Proc.IsValid())
		{
			UE_LOG(LogApexNR, Warning, TEXT("重启失败：启动不了 %s"), *Exe);
			return false;
		}
		FPlatformProcess::CloseProc(Proc);
		UE_LOG(LogApexNR, Log, TEXT("为应用 DLSS 5 设置重启游戏"));
		FPlatformMisc::RequestExit(false, TEXT("ApexNeuralRendering::RestartGame"));
		return true;
	}

	bool RunSelfTest(const FString& SampleIniPath, TArray<FString>& Out)
	{
		int32 Fail = 0;
		auto Check = [&](bool bOk, const FString& What)
		{
			Out.Add(FString::Printf(TEXT("%s %s"), bOk ? TEXT("通过") : TEXT("失败"), *What));
			Fail += bOk ? 0 : 1;
		};

		// 1. 真实的 OptiScaler.ini：改三个键，其余一个字节都不变
		FString Real;
		if (LoadIni(SampleIniPath, Real))
		{
			const FIniValues V0 = Parse(Real);
			Out.Add(FString::Printf(TEXT("样本：Enabled=%d Passes=%d ToggleKey=%d 长度=%d"), V0.bEnabled ? 1 : 0, V0.Passes, V0.ToggleKey, Real.Len()));
			FString C = Real;
			SetValue(C, TEXT("Enabled"), V0.bEnabled ? TEXT("false") : TEXT("true"));
			SetValue(C, TEXT("Passes"), TEXT("2"));
			SetValue(C, TEXT("ToggleKey"), FString::FromInt(GameToggleVk));
			const FIniValues V1 = Parse(C);
			Check(V1.bEnabled == !V0.bEnabled && V1.Passes == 2 && V1.ToggleKey == GameToggleVk, TEXT("真实 INI 改开关 / 次数 / 开关键后读回一致"));
			// 改回去应该和原文完全一样（ToggleKey 原来是 auto 时写回 auto）
			SetValue(C, TEXT("Enabled"), GetValue(Real, TEXT("Enabled")));
			SetValue(C, TEXT("Passes"), GetValue(Real, TEXT("Passes")));
			SetValue(C, TEXT("ToggleKey"), GetValue(Real, TEXT("ToggleKey")));
			Check(C == Real, TEXT("真实 INI 改回原值后逐字节相同（其他段、注释、CRLF 都没动）"));
			Check(GetValue(Real, TEXT("Enabled")) == GetValue(Real.Replace(TEXT("[FrameGen]"), TEXT("[FrameGenX]")), TEXT("Enabled")), TEXT("只读 [DlssNr] 段，不会读到 [FrameGen] / [DLSS] 的 Enabled"));
		}
		else
		{
			Check(false, FString::Printf(TEXT("读不到样本 %s"), *SampleIniPath));
		}

		// 2. auto、1、缺省：和 OptiScaler 一样当默认值
		{
			const FIniValues V = Parse(TEXT("[DlssNr]\r\nEnabled=auto\r\nPasses=auto\r\nToggleKey=auto\r\n"));
			Check(!V.bEnabled && V.Passes == 1 && V.ToggleKey == 0, TEXT("auto = 关 / 1 次 / 无开关键"));
			Check(!Parse(TEXT("[DlssNr]\nEnabled=1\n")).bEnabled, TEXT("Enabled=1 按 OptiScaler 的规则是默认值（关）"));
			Check(Parse(TEXT("[DlssNr]\nEnabled=TRUE\nPasses=9\n")).Passes == 3 && Parse(TEXT("[DlssNr]\nEnabled=TRUE\n")).bEnabled, TEXT("TRUE 不分大小写，次数限制到 3"));
			Check(Parse(TEXT("[DlssNr]\nToggleKey=0x87\n")).ToggleKey == 0x87, TEXT("十六进制开关键"));
		}
		// 3. 缺键、缺段、段头在最后一行
		{
			FString C = TEXT("[Other]\r\nPasses=7\r\n[DlssNr]\r\n; comment\r\nEnabled=true\r\n[Next]\r\nX=1\r\n");
			SetValue(C, TEXT("Passes"), TEXT("3"));
			Check(Parse(C).Passes == 3 && C.Contains(TEXT("[Other]\r\nPasses=7")) && C.Contains(TEXT("[DlssNr]\r\nPasses=3\r\n")), TEXT("段里没有的键插在 [DlssNr] 下面，不碰别的段"));
			FString D = TEXT("[A]\nB=1");
			SetValue(D, TEXT("Enabled"), TEXT("true"));
			Check(Parse(D).bEnabled && D.StartsWith(TEXT("[A]\nB=1\n")), TEXT("没有 [DlssNr] 段时补在末尾"));
			FString E = TEXT("[DlssNr]");
			SetValue(E, TEXT("Enabled"), TEXT("true"));
			Check(Parse(E).bEnabled && E == TEXT("[DlssNr]\nEnabled=true\n"), TEXT("段头是最后一行且没换行"));
		}
		// 4. 行内注释、重复键
		{
			FString C = TEXT("[DlssNr]\nPasses=2 ; note\n");
			Check(Parse(C).Passes == 2, TEXT("行内注释读得出"));
			SetValue(C, TEXT("Passes"), TEXT("3"));
			Check(C == TEXT("[DlssNr]\nPasses=3 ; note\n"), TEXT("改值保留行内注释"));
			FString Dup = TEXT("[DlssNr]\nEnabled=true\nEnabled=false\n");
			Check(!Parse(Dup).bEnabled, TEXT("重复键以最后一个为准"));
			SetValue(Dup, TEXT("Enabled"), TEXT("true"));
			Check(Parse(Dup).bEnabled, TEXT("重复键改的是生效的那个"));
		}
		Out.Add(FString::Printf(TEXT("结果：%s（失败 %d 项）"), Fail == 0 ? TEXT("全部通过") : TEXT("有失败"), Fail));
		return Fail == 0;
	}
}
#endif

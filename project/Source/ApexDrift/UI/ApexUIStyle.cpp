#include "UI/ApexUIStyle.h"
#include "Core/ApexDriftPlayerController.h"
#include "Core/ApexMapRegistry.h"
#include "Vehicle/ApexDriftVehiclePawn.h"
#include "Audio/ApexVehicleAudioComponent.h"
#include "Engine/FontFace.h"
#include "Engine/Texture2D.h"
#include "Fonts/CompositeFont.h"
#include "Fonts/FontCache.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/IConsoleManager.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"

// =========================================================================
// 字体 / 缩略图
// =========================================================================

namespace ApexUI
{
	bool GFocusMapsOnNextMenu = false;

	namespace
	{
		struct FAssets
		{
			TSharedPtr<FStandaloneCompositeFont> Display;
			TSharedPtr<FStandaloneCompositeFont> Body;
			TSharedPtr<FStandaloneCompositeFont> Num;
			TArray<FSlateBrush*> Thumbs;
			bool bFontsOk = false;
			bool bLoaded = false;
		};

		// 进程生命周期内常驻，避免静态析构顺序问题
		FAssets& Assets()
		{
			static FAssets* A = new FAssets();
			return *A;
		}

		UFontFace* LoadFace(const TCHAR* Name)
		{
			const FString Pkg = FString::Printf(TEXT("/Game/UI/Fonts/%s"), Name);
			UFontFace* Face = LoadObject<UFontFace>(nullptr, *FString::Printf(TEXT("%s.%s"), *Pkg, Name), nullptr, LOAD_NoWarn | LOAD_Quiet);
			if (Face)
			{
				Face->AddToRoot();
			}
			return Face;
		}

		void AddEntry(FTypeface& T, const TCHAR* Name, UFontFace* Face)
		{
			FTypefaceEntry& E = T.Fonts.AddDefaulted_GetRef();
			E.Name = FName(Name);
			E.Font = FFontData(Face);
		}
	}

	FLinearColor Mix(const FLinearColor& A, const FLinearColor& B, float T)
	{
		T = FMath::Clamp(T, 0.0f, 1.0f);
		const FColor CA = A.ToFColor(true);
		const FColor CB = B.ToFColor(true);
		auto L = [T](uint8 X, uint8 Y) { return static_cast<uint8>(FMath::RoundToInt(FMath::Lerp(static_cast<float>(X), static_cast<float>(Y), T))); };
		FLinearColor Out = FLinearColor(FColor(L(CA.R, CB.R), L(CA.G, CB.G), L(CA.B, CB.B)));
		Out.A = FMath::Lerp(A.A, B.A, T);
		return Out;
	}

	void EnsureLoaded()
	{
		FAssets& A = Assets();
		if (A.bLoaded || !IsInGameThread())
		{
			return;
		}
		A.bLoaded = true;

		// 中文字形多、字号多，允许多几页字形图集，避免刷新抖动
		if (IConsoleVariable* Var = IConsoleManager::Get().FindConsoleVariable(TEXT("Slate.MaxFontAtlasPagesBeforeFlush")))
		{
			if (Var->GetInt() < 8) Var->Set(8, ECVF_SetByCode);
		}

		UFontFace* FDisplay = LoadFace(TEXT("FF_Display"));
		UFontFace* FBodyR = LoadFace(TEXT("FF_Body_Regular"));
		UFontFace* FBodyB = LoadFace(TEXT("FF_Body_Bold"));
		UFontFace* FLatM = LoadFace(TEXT("FF_Latin_Medium"));
		UFontFace* FLatS = LoadFace(TEXT("FF_Latin_SemiBold"));
		UFontFace* FNumM = LoadFace(TEXT("FF_Num_Medium"));
		UFontFace* FNumS = LoadFace(TEXT("FF_Num_SemiBold"));
		UFontFace* FNumH = LoadFace(TEXT("FF_Num_Heavy"));

		if (FDisplay && FBodyR && FBodyB && FLatM && FLatS && FNumM && FNumS && FNumH)
		{
			// 标题：得意黑，缺字回落思源粗体
			A.Display = MakeShared<FStandaloneCompositeFont>();
			AddEntry(A.Display->DefaultTypeface, TEXT("Regular"), FDisplay);
			AddEntry(A.Display->FallbackTypeface.Typeface, TEXT("Regular"), FBodyB);

			// 正文：思源黑体；拉丁字母与数字用窄体
			A.Body = MakeShared<FStandaloneCompositeFont>();
			AddEntry(A.Body->DefaultTypeface, TEXT("Regular"), FBodyR);
			AddEntry(A.Body->DefaultTypeface, TEXT("Bold"), FBodyB);
			FCompositeSubFont& Sub = A.Body->SubTypefaces.AddDefaulted_GetRef();
			AddEntry(Sub.Typeface, TEXT("Regular"), FLatM);
			AddEntry(Sub.Typeface, TEXT("Bold"), FLatS);
			Sub.ScalingFactor = 1.10f;
			Sub.CharacterRanges.Add(FInt32Range(FInt32Range::BoundsType::Inclusive(0x21), FInt32Range::BoundsType::Inclusive(0x7E)));

			// 数字与英文：窄体斜体，缺字回落思源粗体
			A.Num = MakeShared<FStandaloneCompositeFont>();
			AddEntry(A.Num->DefaultTypeface, TEXT("SemiBold"), FNumS);
			AddEntry(A.Num->DefaultTypeface, TEXT("Heavy"), FNumH);
			AddEntry(A.Num->DefaultTypeface, TEXT("Medium"), FNumM);
			AddEntry(A.Num->FallbackTypeface.Typeface, TEXT("SemiBold"), FBodyB);
			AddEntry(A.Num->FallbackTypeface.Typeface, TEXT("Heavy"), FBodyB);
			AddEntry(A.Num->FallbackTypeface.Typeface, TEXT("Medium"), FBodyR);
			A.bFontsOk = true;
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[ApexUI] /Game/UI/Fonts 缺少字体，回退引擎默认字体（请运行 Tools/ue_import_ui.py）"));
		}

		for (const FApexMapInfo& Info : FApexMapRegistry::GetMaps())
		{
			FSlateBrush* Brush = nullptr;
			if (!Info.ThumbnailPath.IsEmpty())
			{
				if (UTexture2D* Tex = LoadObject<UTexture2D>(nullptr, *Info.ThumbnailPath, nullptr, LOAD_NoWarn | LOAD_Quiet))
				{
					Tex->AddToRoot();
					Tex->bForceMiplevelsToBeResident = true;
					Brush = new FSlateBrush();
					Brush->SetResourceObject(Tex);
					Brush->ImageSize = FVector2f(1024.0f, 576.0f);
					Brush->DrawAs = ESlateBrushDrawType::Image;
				}
			}
			A.Thumbs.Add(Brush);
		}
	}

	FSlateFontInfo Font(EApexFont F, float SizePx, int32 LetterSpacing)
	{
		const FAssets& A = Assets();
		const float Pt = FMath::Max(1.0f, SizePx * 0.75f);
		FSlateFontInfo Info;
		if (A.bFontsOk)
		{
			static const FName NRegular(TEXT("Regular")), NBold(TEXT("Bold")), NSemi(TEXT("SemiBold")), NHeavy(TEXT("Heavy")), NMedium(TEXT("Medium"));
			switch (F)
			{
			case EApexFont::Display:  Info = FSlateFontInfo(TSharedPtr<const FCompositeFont>(A.Display), Pt, NRegular); break;
			case EApexFont::Body:     Info = FSlateFontInfo(TSharedPtr<const FCompositeFont>(A.Body), Pt, NRegular); break;
			case EApexFont::BodyBold: Info = FSlateFontInfo(TSharedPtr<const FCompositeFont>(A.Body), Pt, NBold); break;
			case EApexFont::Num:      Info = FSlateFontInfo(TSharedPtr<const FCompositeFont>(A.Num), Pt, NSemi); break;
			case EApexFont::NumHeavy: Info = FSlateFontInfo(TSharedPtr<const FCompositeFont>(A.Num), Pt, NHeavy); break;
			case EApexFont::NumLight: Info = FSlateFontInfo(TSharedPtr<const FCompositeFont>(A.Num), Pt, NMedium); break;
			}
		}
		else
		{
			const bool bRegular = (F == EApexFont::Body || F == EApexFont::NumLight);
			Info = FCoreStyle::GetDefaultFontStyle(bRegular ? "Regular" : "Bold", Pt);
		}
		Info.LetterSpacing = LetterSpacing;
		return Info;
	}

	FVector2f Measure(const FString& S, const FSlateFontInfo& InFont)
	{
		const TSharedRef<FSlateFontMeasure> FM = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
		return FVector2f(FM->Measure(S, InFont, 1.0f));
	}

	const FSlateBrush* ThumbBrush(int32 MapIndex)
	{
		const FAssets& A = Assets();
		return A.Thumbs.IsValidIndex(MapIndex) ? A.Thumbs[MapIndex] : nullptr;
	}

	FString MapSeal(int32 MapIndex)
	{
		const FApexMapInfo& Info = FApexMapRegistry::GetMaps()[FApexMapRegistry::ClampIndex(MapIndex)];
		if (Info.Id == FName(TEXT("FujiSakura"))) return TEXT("樱");
		if (Info.Id == FName(TEXT("MomijiTouge"))) return TEXT("枫");
		if (Info.Id == FName(TEXT("KuroshioCircuit"))) return TEXT("潮");
		if (Info.Id == FName(TEXT("TrainingPark"))) return TEXT("练");
		return Info.NameZh.Left(1);
	}

	FString ModeName(EApexGameModeType Mode)
	{
		switch (Mode)
		{
		case EApexGameModeType::DriftAttack3Min: return TEXT("三分钟挑战");
		case EApexGameModeType::Circuit3Laps:    return TEXT("三圈竞速");
		default:                                 return TEXT("自由漂移");
		}
	}

	FString ModeDesc(EApexGameModeType Mode)
	{
		switch (Mode)
		{
		case EApexGameModeType::DriftAttack3Min: return TEXT("180 秒内冲击最高分");
		case EApexGameModeType::Circuit3Laps:    return TEXT("跑满三圈，比圈速和得分");
		default:                                 return TEXT("不限时，随心练习");
		}
	}

	FString FormatScore(int32 Value)
	{
		const bool bNeg = Value < 0;
		FString Digits = FString::FromInt(FMath::Abs(Value));
		for (int32 i = Digits.Len() - 3; i > 0; i -= 3)
		{
			Digits.InsertAt(i, TEXT(','));
		}
		return bNeg ? TEXT("-") + Digits : Digits;
	}

	FString FormatTime(float Sec)
	{
		Sec = FMath::Max(0.0f, Sec);
		const int32 Cs = FMath::FloorToInt(Sec * 100.0f);
		return FString::Printf(TEXT("%d:%02d.%02d"), Cs / 6000, (Cs / 100) % 60, Cs % 100);
	}

	FString FormatTimeOrDash(float Sec)
	{
		return Sec > 0.1f ? FormatTime(Sec) : FString(TEXT("--:--.--"));
	}

	void PlayUISound(AApexDriftPlayerController* PC, FName Cue)
	{
		if (PC && PC->GetVehiclePawn() && PC->GetVehiclePawn()->VehicleAudioComp)
		{
			PC->GetVehiclePawn()->VehicleAudioComp->PlayUISound(Cue);
		}
	}

	void SplitCallout(const FString& In, FString& OutMain, FString& OutSub)
	{
		OutMain.Reset();
		OutSub.Reset();
		FString S = In;
		S.TrimStartAndEndInline();
		if (S.IsEmpty())
		{
			return;
		}

		// 只有英文的提示：给出中文主行
		if (S.StartsWith(TEXT("CLIPPING ZONE x")))
		{
			OutMain = TEXT("贴弯加成 ×") + S.RightChop(15);
			OutSub = TEXT("CLIPPING ZONE");
			return;
		}
		if (S.StartsWith(TEXT("DRIFT BANKED")))
		{
			const int32 Banked = FCString::Atoi(*S.RightChop(12).Replace(TEXT("+"), TEXT("")).TrimStartAndEnd());
			OutMain = FString::Printf(TEXT("得分入账 +%s"), *FormatScore(Banked));
			OutSub = TEXT("DRIFT BANKED");
			return;
		}
		static const TCHAR* ZoneTable[][2] = {
			{ TEXT("SUNSET HARBOR SWEEPER"),    TEXT("落日港湾大弯") },
			{ TEXT("COASTAL VIADUCT ESSES"),    TEXT("海岸高架连弯") },
			{ TEXT("TOUGE SWITCHBACK HAIRPIN"), TEXT("山道回头弯") },
			{ TEXT("DOCKYARD DRIFT PLAZA"),     TEXT("码头漂移广场") },
		};
		for (const auto& Row : ZoneTable)
		{
			if (S.Equals(Row[0], ESearchCase::IgnoreCase))
			{
				OutMain = Row[1];
				OutSub = Row[0];
				return;
			}
		}

		// 中英混排：第一个中日韩字符起为主行
		int32 FirstCjk = INDEX_NONE;
		for (int32 i = 0; i < S.Len(); ++i)
		{
			if (S[i] >= 0x2E80)
			{
				FirstCjk = i;
				break;
			}
		}
		if (FirstCjk == INDEX_NONE)
		{
			OutMain = S;
			return;
		}
		FString Head = S.Left(FirstCjk).TrimStartAndEnd();
		OutMain = S.Mid(FirstCjk).TrimStartAndEnd();
		bool bHeadHasLetter = false;
		for (const TCHAR Ch : Head)
		{
			bHeadHasLetter |= FChar::IsAlpha(Ch);
		}
		if (!bHeadHasLetter)
		{
			OutMain = S; // 例如“+500 擦墙”
			return;
		}
		// 英文里带的加分数字并入主行，例如 WALL TAP +500!
		int32 PlusAt = INDEX_NONE;
		if (Head.FindChar(TEXT('+'), PlusAt))
		{
			FString Num = Head.Mid(PlusAt);
			Num.RemoveFromEnd(TEXT("!"));
			OutMain += TEXT(" ") + Num;
			Head = Head.Left(PlusAt).TrimEnd();
		}
		while (Head.Len() > 0 && (Head.EndsWith(TEXT("!")) || Head.EndsWith(TEXT("-")) || Head.EndsWith(TEXT("/")) || Head.EndsWith(TEXT("|"))))
		{
			Head.LeftChopInline(1);
			Head.TrimEndInline();
		}
		OutSub = Head;
	}
}

// =========================================================================
// FApexPainter
// =========================================================================

namespace
{
	constexpr int32 MaxPainterDepth = 6;
	struct FPainterScratch
	{
		TArray<FSlateVertex> Verts[MaxPainterDepth];
		TArray<SlateIndex> Idx[MaxPainterDepth];
		int32 Depth = 0;
	};
	FPainterScratch& Scratch()
	{
		thread_local FPainterScratch S;
		return S;
	}

	inline float Cross2(const FVector2f& A, const FVector2f& B) { return A.X * B.Y - A.Y * B.X; }
	inline FVector2f SafeDir(const FVector2f& D)
	{
		const float L = FMath::Sqrt(D.X * D.X + D.Y * D.Y);
		return L > 1e-5f ? D * (1.0f / L) : FVector2f(0, 0);
	}
}

FApexPainter::FApexPainter(const FGeometry& InGeo, FSlateWindowElementList& InOut, int32 InLayer, float InAlpha)
	: Geo(InGeo)
	, Out(InOut)
	, Layer(InLayer)
	, Alpha(InAlpha)
	, Verts(Scratch().Verts[FMath::Min(Scratch().Depth, MaxPainterDepth - 1)])
	, Idx(Scratch().Idx[FMath::Min(Scratch().Depth, MaxPainterDepth - 1)])
{
	++Scratch().Depth;
	const FSlateRenderTransform& RT = Geo.GetAccumulatedRenderTransform();
	const FVector2f P0 = FVector2f(TransformPoint(RT, FVector2f(0.0f, 0.0f)));
	const FVector2f P1 = FVector2f(TransformPoint(RT, FVector2f(0.0f, 1.0f)));
	Trans = P0;
	Scale = FMath::Max(0.01f, P1.Y - P0.Y);
	Px = 1.0f / Scale;
	Verts.Reset();
	Idx.Reset();
}

FApexPainter::~FApexPainter()
{
	Flush();
	--Scratch().Depth;
}

void FApexPainter::BeginShape()
{
	if (Last == ELast::Text || Last == ELast::Post)
	{
		++Layer;
	}
	Last = ELast::Shape;
}

uint32 FApexPainter::AddV(const FVector2f& P, const FColor& C)
{
	FSlateVertex& V = Verts[Verts.AddUninitialized()];
	V.TexCoords[0] = 0.0f;
	V.TexCoords[1] = 0.0f;
	V.TexCoords[2] = 1.0f;
	V.TexCoords[3] = 1.0f;
	V.MaterialTexCoords = FVector2f(0.0f, 0.0f);
	V.Position = FVector2f((P.X + Shear * (ShearY0 - P.Y)) * Scale + Trans.X, P.Y * Scale + Trans.Y);
	V.Color = C;
	V.SecondaryColor = FColor(0, 0, 0, 0);
	V.PixelSize[0] = 1;
	V.PixelSize[1] = 1;
	return static_cast<uint32>(Verts.Num() - 1);
}

void FApexPainter::Flush()
{
	if (Idx.Num() > 0)
	{
		FSlateDrawElement::MakeCustomVerts(Out, Layer, FSlateResourceHandle(), Verts, Idx, nullptr, 0, 0);
	}
	Verts.Reset();
	Idx.Reset();
}

void FApexPainter::FillConvex(const FVector2f* Pts, const FColor* Cols, int32 N)
{
	if (N < 3)
	{
		return;
	}
	BeginShape();
	float Area = 0.0f;
	for (int32 i = 0; i < N; ++i)
	{
		Area += Cross2(Pts[i], Pts[(i + 1) % N]);
	}
	const float Sign = Area >= 0.0f ? 1.0f : -1.0f;
	const uint32 Base = static_cast<uint32>(Verts.Num());
	const float Half = 0.5f * Px;
	for (int32 i = 0; i < N; ++i)
	{
		const FVector2f& Prev = Pts[(i + N - 1) % N];
		const FVector2f& Cur = Pts[i];
		const FVector2f& Next = Pts[(i + 1) % N];
		const FVector2f D0 = SafeDir(Cur - Prev);
		const FVector2f D1 = SafeDir(Next - Cur);
		const FVector2f N0(D0.Y * Sign, -D0.X * Sign);
		const FVector2f N1(D1.Y * Sign, -D1.X * Sign);
		FVector2f M = (N0 + N1) * 0.5f;
		const float L2 = M.X * M.X + M.Y * M.Y;
		if (L2 > 1e-4f)
		{
			M *= FMath::Min(1.0f / L2, 4.0f);
		}
		FColor C = Cols[i];
		AddV(Cur - M * Half, C);
		C.A = 0;
		AddV(Cur + M * Half, C);
	}
	for (int32 i = 2; i < N; ++i)
	{
		Tri(Base, Base + (i - 1) * 2, Base + i * 2);
	}
	for (int32 i = 0; i < N; ++i)
	{
		const uint32 A = Base + i * 2;
		const uint32 B = Base + ((i + 1) % N) * 2;
		Quad(A, A + 1, B + 1, B);
	}
}

void FApexPainter::Rect(const FApexRect& R, const FLinearColor& C) { RectV(R, C, C); }

void FApexPainter::RectV(const FApexRect& R, const FLinearColor& Top, const FLinearColor& Bottom)
{
	if (R.W <= 0.0f || R.H <= 0.0f) return;
	const FVector2f P[4] = { { R.X, R.Y }, { R.R(), R.Y }, { R.R(), R.B() }, { R.X, R.B() } };
	const FColor T = Pack(Top), B = Pack(Bottom);
	const FColor Cols[4] = { T, T, B, B };
	FillConvex(P, Cols, 4);
}

void FApexPainter::RectH(const FApexRect& R, const FLinearColor& Left, const FLinearColor& Right)
{
	if (R.W <= 0.0f || R.H <= 0.0f) return;
	const FVector2f P[4] = { { R.X, R.Y }, { R.R(), R.Y }, { R.R(), R.B() }, { R.X, R.B() } };
	const FColor L = Pack(Left), Rr = Pack(Right);
	const FColor Cols[4] = { L, Rr, Rr, L };
	FillConvex(P, Cols, 4);
}

void FApexPainter::Skew(const FApexRect& R, const FLinearColor& C, float K) { SkewH(R, C, C, K); }

void FApexPainter::SkewH(const FApexRect& R, const FLinearColor& Left, const FLinearColor& Right, float K)
{
	if (R.W <= 0.0f || R.H <= 0.0f) return;
	const float D = K * R.H * 0.5f;
	const FVector2f P[4] = { { R.X + D, R.Y }, { R.R() + D, R.Y }, { R.R() - D, R.B() }, { R.X - D, R.B() } };
	const FColor L = Pack(Left), Rr = Pack(Right);
	const FColor Cols[4] = { L, Rr, Rr, L };
	FillConvex(P, Cols, 4);
}

void FApexPainter::SkewV(const FApexRect& R, const FLinearColor& Top, const FLinearColor& Bottom, float K)
{
	if (R.W <= 0.0f || R.H <= 0.0f) return;
	const float D = K * R.H * 0.5f;
	const FVector2f P[4] = { { R.X + D, R.Y }, { R.R() + D, R.Y }, { R.R() - D, R.B() }, { R.X - D, R.B() } };
	const FColor T = Pack(Top), B = Pack(Bottom);
	const FColor Cols[4] = { T, T, B, B };
	FillConvex(P, Cols, 4);
}

static int32 BuildRoundRect(const FApexRect& R, float Radius, float ScalePx, FVector2f* OutPts, int32 MaxPts)
{
	Radius = FMath::Clamp(Radius, 0.0f, FMath::Min(R.W, R.H) * 0.5f);
	const int32 Seg = FMath::Clamp(FMath::CeilToInt(FMath::Sqrt(Radius * ScalePx) * 1.6f), 2, 12);
	int32 N = 0;
	const FVector2f C[4] = { { R.R() - Radius, R.Y + Radius }, { R.R() - Radius, R.B() - Radius }, { R.X + Radius, R.B() - Radius }, { R.X + Radius, R.Y + Radius } };
	for (int32 k = 0; k < 4; ++k)
	{
		const float A0 = -HALF_PI + k * HALF_PI;
		for (int32 i = 0; i <= Seg && N < MaxPts; ++i)
		{
			const float A = A0 + HALF_PI * i / Seg;
			const FVector2f Pt = C[k] + FVector2f(FMath::Cos(A), FMath::Sin(A)) * Radius;
			// 胶囊形时相邻圆角端点重合，重复点会让羽化边自交
			if (N > 0 && FVector2f::DistSquared(Pt, OutPts[N - 1]) < 1e-4f) continue;
			OutPts[N++] = Pt;
		}
	}
	if (N > 1 && FVector2f::DistSquared(OutPts[0], OutPts[N - 1]) < 1e-4f) --N;
	return N;
}

void FApexPainter::RoundRect(const FApexRect& R, float Radius, const FLinearColor& C)
{
	FVector2f P[56];
	const int32 N = BuildRoundRect(R, Radius, Scale, P, 56);
	FColor Cols[56];
	const FColor K = Pack(C);
	for (int32 i = 0; i < N; ++i) Cols[i] = K;
	FillConvex(P, Cols, N);
}

void FApexPainter::Circle(const FVector2f& Center, float Radius, const FLinearColor& C)
{
	const int32 N = FMath::Clamp(FMath::CeilToInt(FMath::Sqrt(Radius * Scale) * 5.0f), 12, 96);
	FVector2f P[96];
	FColor Cols[96];
	const FColor K = Pack(C);
	for (int32 i = 0; i < N; ++i)
	{
		const float A = 2.0f * PI * i / N;
		P[i] = Center + FVector2f(FMath::Cos(A), FMath::Sin(A)) * Radius;
		Cols[i] = K;
	}
	FillConvex(P, Cols, N);
}

void FApexPainter::CircleGrad(const FVector2f& Center, float Radius, const FLinearColor& Inner, const FLinearColor& Outer)
{
	BeginShape();
	const int32 N = FMath::Clamp(FMath::CeilToInt(FMath::Sqrt(Radius * Scale) * 4.0f), 16, 96);
	const FColor CI = Pack(Inner), CO = Pack(Outer);
	const uint32 C0 = AddV(Center, CI);
	for (int32 i = 0; i < N; ++i)
	{
		const float A = 2.0f * PI * i / N;
		AddV(Center + FVector2f(FMath::Cos(A), FMath::Sin(A)) * Radius, CO);
	}
	for (int32 i = 0; i < N; ++i)
	{
		Tri(C0, C0 + 1 + i, C0 + 1 + (i + 1) % N);
	}
}

void FApexPainter::Poly(TConstArrayView<FVector2f> Pts, const FLinearColor& C)
{
	TArray<FColor, TInlineAllocator<64>> Cols;
	Cols.Init(Pack(C), Pts.Num());
	FillConvex(Pts.GetData(), Cols.GetData(), Pts.Num());
}

void FApexPainter::PolyGrad(TConstArrayView<FVector2f> Pts, TConstArrayView<FLinearColor> InCols)
{
	TArray<FColor, TInlineAllocator<64>> Cols;
	for (const FLinearColor& C : InCols) Cols.Add(Pack(C));
	FillConvex(Pts.GetData(), Cols.GetData(), FMath::Min(Pts.Num(), Cols.Num()));
}

void FApexPainter::Terrain(TConstArrayView<FVector2f> Top, float BaseY, const FLinearColor& CTop, const FLinearColor& CBottom)
{
	if (Top.Num() < 2) return;
	BeginShape();
	const FColor T = Pack(CTop), B = Pack(CBottom);
	FColor T0 = T;
	T0.A = 0;
	const uint32 Base = static_cast<uint32>(Verts.Num());
	for (const FVector2f& P : Top)
	{
		AddV(FVector2f(P.X, P.Y - 0.5f * Px), T0);
		AddV(FVector2f(P.X, P.Y + 0.5f * Px), T);
		AddV(FVector2f(P.X, BaseY), B);
	}
	for (int32 i = 0; i + 1 < Top.Num(); ++i)
	{
		const uint32 A = Base + i * 3;
		const uint32 N = A + 3;
		Quad(A, N, N + 1, A + 1);
		Quad(A + 1, N + 1, N + 2, A + 2);
	}
}

void FApexPainter::Strip(TConstArrayView<FVector2f> Top, TConstArrayView<FVector2f> Bottom, const FLinearColor& CTop, const FLinearColor& CBottom)
{
	const int32 N = FMath::Min(Top.Num(), Bottom.Num());
	if (N < 2) return;
	BeginShape();
	const FColor T = Pack(CTop), B = Pack(CBottom);
	FColor T0 = T, B0 = B;
	T0.A = 0;
	B0.A = 0;
	const uint32 Base = static_cast<uint32>(Verts.Num());
	const float H = 0.6f * Px;
	for (int32 i = 0; i < N; ++i)
	{
		AddV(FVector2f(Top[i].X, Top[i].Y - H), T0);
		AddV(FVector2f(Top[i].X, Top[i].Y + H), T);
		AddV(FVector2f(Bottom[i].X, Bottom[i].Y - H), B);
		AddV(FVector2f(Bottom[i].X, Bottom[i].Y + H), B0);
	}
	for (int32 i = 0; i + 1 < N; ++i)
	{
		const uint32 A = Base + i * 4;
		const uint32 C = A + 4;
		Quad(A, C, C + 1, A + 1);
		Quad(A + 1, C + 1, C + 2, A + 2);
		Quad(A + 2, C + 2, C + 3, A + 3);
	}
}

void FApexPainter::FillStar(const FVector2f& Center, TConstArrayView<FVector2f> Pts, const FLinearColor& C)
{
	const int32 N = Pts.Num();
	if (N < 3) return;
	BeginShape();
	const FColor K = Pack(C);
	const uint32 C0 = AddV(Center, K);
	for (const FVector2f& Pt : Pts)
	{
		AddV(Pt, K);
	}
	for (int32 i = 0; i < N; ++i)
	{
		Tri(C0, C0 + 1 + i, C0 + 1 + (i + 1) % N);
	}
}

void FApexPainter::RectStops(const FApexRect& R, TConstArrayView<TPair<float, FLinearColor>> Stops)
{
	if (Stops.Num() < 2) return;
	BeginShape();
	const uint32 Base = static_cast<uint32>(Verts.Num());
	for (const TPair<float, FLinearColor>& St : Stops)
	{
		const FColor C = Pack(St.Value);
		AddV(FVector2f(R.X, R.Y + R.H * St.Key), C);
		AddV(FVector2f(R.R(), R.Y + R.H * St.Key), C);
	}
	for (int32 i = 0; i + 1 < Stops.Num(); ++i)
	{
		const uint32 A = Base + i * 2;
		Quad(A, A + 1, A + 3, A + 2);
	}
}

void FApexPainter::RectCorners(const FApexRect& R, const FLinearColor& TL, const FLinearColor& TR, const FLinearColor& BR, const FLinearColor& BL)
{
	if (R.W <= 0.0f || R.H <= 0.0f) return;
	const FVector2f Pts[4] = { { R.X, R.Y }, { R.R(), R.Y }, { R.R(), R.B() }, { R.X, R.B() } };
	const FColor Cols[4] = { Pack(TL), Pack(TR), Pack(BR), Pack(BL) };
	FillConvex(Pts, Cols, 4);
}

void FApexPainter::StrokeImpl(const FVector2f* Pts, int32 N, float Thickness, const FColor& InC, bool bClosed)
{
	if (N < 2 || InC.A == 0)
	{
		return;
	}
	BeginShape();
	FColor C = InC;
	if (Thickness < Px)
	{
		C.A = static_cast<uint8>(FMath::Clamp(C.A * Thickness / Px, 0.0f, 255.0f));
		Thickness = Px;
	}
	const float In = FMath::Max(0.0f, Thickness * 0.5f - 0.5f * Px);
	const float OutW = Thickness * 0.5f + 0.5f * Px;
	FColor C0 = C;
	C0.A = 0;
	const uint32 Base = static_cast<uint32>(Verts.Num());
	for (int32 i = 0; i < N; ++i)
	{
		FVector2f D0, D1;
		if (bClosed)
		{
			D0 = SafeDir(Pts[i] - Pts[(i + N - 1) % N]);
			D1 = SafeDir(Pts[(i + 1) % N] - Pts[i]);
		}
		else
		{
			D0 = i > 0 ? SafeDir(Pts[i] - Pts[i - 1]) : SafeDir(Pts[1] - Pts[0]);
			D1 = i + 1 < N ? SafeDir(Pts[i + 1] - Pts[i]) : D0;
		}
		const FVector2f N0(D0.Y, -D0.X);
		const FVector2f N1(D1.Y, -D1.X);
		FVector2f M = (N0 + N1) * 0.5f;
		const float L2 = M.X * M.X + M.Y * M.Y;
		if (L2 > 1e-4f)
		{
			M *= FMath::Min(1.0f / L2, 3.0f);
		}
		else
		{
			M = N1;
		}
		AddV(Pts[i] + M * OutW, C0);
		AddV(Pts[i] + M * In, C);
		AddV(Pts[i] - M * In, C);
		AddV(Pts[i] - M * OutW, C0);
	}
	const int32 SegN = bClosed ? N : N - 1;
	for (int32 i = 0; i < SegN; ++i)
	{
		const uint32 A = Base + i * 4;
		const uint32 B = Base + ((i + 1) % N) * 4;
		Quad(A, B, B + 1, A + 1);
		Quad(A + 1, B + 1, B + 2, A + 2);
		Quad(A + 2, B + 2, B + 3, A + 3);
	}
}

void FApexPainter::Stroke(TConstArrayView<FVector2f> Pts, float Thickness, const FLinearColor& C, bool bClosed)
{
	StrokeImpl(Pts.GetData(), Pts.Num(), Thickness, Pack(C), bClosed);
}

void FApexPainter::LineSeg(const FVector2f& A, const FVector2f& B, float Thickness, const FLinearColor& C)
{
	const FVector2f P[2] = { A, B };
	StrokeImpl(P, 2, Thickness, Pack(C), false);
}

void FApexPainter::StrokeRect(const FApexRect& R, float Thickness, const FLinearColor& C)
{
	const FVector2f P[4] = { { R.X, R.Y }, { R.R(), R.Y }, { R.R(), R.B() }, { R.X, R.B() } };
	StrokeImpl(P, 4, Thickness, Pack(C), true);
}

void FApexPainter::StrokeSkew(const FApexRect& R, float Thickness, const FLinearColor& C, float K)
{
	const float D = K * R.H * 0.5f;
	const FVector2f P[4] = { { R.X + D, R.Y }, { R.R() + D, R.Y }, { R.R() - D, R.B() }, { R.X - D, R.B() } };
	StrokeImpl(P, 4, Thickness, Pack(C), true);
}

void FApexPainter::StrokeRoundRect(const FApexRect& R, float Radius, float Thickness, const FLinearColor& C)
{
	FVector2f P[56];
	const int32 N = BuildRoundRect(R, Radius, Scale, P, 56);
	StrokeImpl(P, N, Thickness, Pack(C), true);
}

void FApexPainter::Ring(const FVector2f& Center, float Radius, float Thickness, const FLinearColor& C)
{
	const int32 N = FMath::Clamp(FMath::CeilToInt(FMath::Sqrt(Radius * Scale) * 5.0f), 16, 96);
	FVector2f P[96];
	for (int32 i = 0; i < N; ++i)
	{
		const float A = 2.0f * PI * i / N;
		P[i] = Center + FVector2f(FMath::Cos(A), FMath::Sin(A)) * Radius;
	}
	StrokeImpl(P, N, Thickness, Pack(C), true);
}

void FApexPainter::Arc(const FVector2f& Center, float Radius, float Thickness, float A0, float A1, const FLinearColor& InC0, const FLinearColor& InC1)
{
	const float Span = FMath::Abs(A1 - A0);
	if (Span < 1e-4f || Thickness <= 0.0f)
	{
		return;
	}
	BeginShape();
	const int32 N = FMath::Clamp(FMath::CeilToInt(Span * FMath::Sqrt(Radius * Scale) * 0.8f) + 1, 2, 128);
	const float RIn0 = Radius - Thickness * 0.5f - 0.5f * Px;
	const float RIn1 = Radius - Thickness * 0.5f + 0.5f * Px;
	const float ROut1 = Radius + Thickness * 0.5f - 0.5f * Px;
	const float ROut0 = Radius + Thickness * 0.5f + 0.5f * Px;
	const FColor CA = Pack(InC0), CB = Pack(InC1);
	const bool bSame = (CA == CB);
	const uint32 Base = static_cast<uint32>(Verts.Num());
	for (int32 i = 0; i <= N; ++i)
	{
		const float T = static_cast<float>(i) / N;
		const float A = FMath::Lerp(A0, A1, T);
		const FVector2f D(FMath::Cos(A), FMath::Sin(A));
		FColor C = CA;
		if (!bSame)
		{
			C.R = static_cast<uint8>(FMath::Lerp(static_cast<float>(CA.R), static_cast<float>(CB.R), T));
			C.G = static_cast<uint8>(FMath::Lerp(static_cast<float>(CA.G), static_cast<float>(CB.G), T));
			C.B = static_cast<uint8>(FMath::Lerp(static_cast<float>(CA.B), static_cast<float>(CB.B), T));
			C.A = static_cast<uint8>(FMath::Lerp(static_cast<float>(CA.A), static_cast<float>(CB.A), T));
		}
		FColor C0 = C;
		C0.A = 0;
		AddV(Center + D * RIn0, C0);
		AddV(Center + D * RIn1, C);
		AddV(Center + D * ROut1, C);
		AddV(Center + D * ROut0, C0);
	}
	for (int32 i = 0; i < N; ++i)
	{
		const uint32 A = Base + i * 4;
		const uint32 B = A + 4;
		Quad(A, B, B + 1, A + 1);
		Quad(A + 1, B + 1, B + 2, A + 2);
		Quad(A + 2, B + 2, B + 3, A + 3);
	}
}

void FApexPainter::GlowSkew(const FApexRect& R, float Spread, const FLinearColor& C, float K)
{
	BeginShape();
	const FColor C1 = Pack(C);
	FColor C0 = C1;
	C0.A = 0;
	// 圆角外扩环：四角各 6 段
	const float D = K * R.H * 0.5f;
	const FVector2f Corner[4] = { { R.R() + D, R.Y }, { R.R() - D, R.B() }, { R.X - D, R.B() }, { R.X + D, R.Y } };
	const uint32 Base = static_cast<uint32>(Verts.Num());
	constexpr int32 Seg = 6;
	int32 N = 0;
	for (int32 k = 0; k < 4; ++k)
	{
		const float A0 = -HALF_PI + k * HALF_PI;
		for (int32 i = 0; i <= Seg; ++i)
		{
			const float A = A0 + HALF_PI * i / Seg;
			const FVector2f Dir(FMath::Cos(A), FMath::Sin(A));
			AddV(Corner[k], C1);
			FColor Mid = C1;
			Mid.A = static_cast<uint8>(C1.A * 0.36f);
			AddV(Corner[k] + Dir * Spread * 0.38f, Mid);
			AddV(Corner[k] + Dir * Spread, C0);
			++N;
		}
	}
	for (int32 i = 0; i < N; ++i)
	{
		const uint32 A = Base + i * 3;
		const uint32 B = Base + ((i + 1) % N) * 3;
		Quad(A, B, B + 1, A + 1);
		Quad(A + 1, B + 1, B + 2, A + 2);
	}
}

void FApexPainter::Image(const FApexRect& R, const FSlateBrush* Brush, const FLinearColor& Tint, const FVector2f& UV0, const FVector2f& UV1)
{
	if (!Brush || R.W <= 0.0f || R.H <= 0.0f)
	{
		return;
	}
	Flush();
	if (Last != ELast::None)
	{
		++Layer;
	}
	Last = ELast::Shape;
	const FColor C = Pack(Tint);
	FColor C0 = C;
	C0.A = 0;
	const float H = 0.5f * Px;
	const FApexRect In = R.Inset(H);
	const FApexRect OutR = R.Inset(-H);
	const uint32 Base = 0;
	const FVector2f PI4[4] = { { In.X, In.Y }, { In.R(), In.Y }, { In.R(), In.B() }, { In.X, In.B() } };
	const FVector2f PO4[4] = { { OutR.X, OutR.Y }, { OutR.R(), OutR.Y }, { OutR.R(), OutR.B() }, { OutR.X, OutR.B() } };
	const FVector2f UV[4] = { { UV0.X, UV0.Y }, { UV1.X, UV0.Y }, { UV1.X, UV1.Y }, { UV0.X, UV1.Y } };
	for (int32 i = 0; i < 4; ++i)
	{
		const uint32 A = AddV(PI4[i], C);
		Verts[A].TexCoords[0] = UV[i].X;
		Verts[A].TexCoords[1] = UV[i].Y;
		Verts[A].MaterialTexCoords = UV[i];
		const uint32 B = AddV(PO4[i], C0);
		Verts[B].TexCoords[0] = UV[i].X;
		Verts[B].TexCoords[1] = UV[i].Y;
		Verts[B].MaterialTexCoords = UV[i];
	}
	Quad(Base, Base + 2, Base + 4, Base + 6);
	for (int32 i = 0; i < 4; ++i)
	{
		const uint32 A = Base + i * 2;
		const uint32 B = Base + ((i + 1) % 4) * 2;
		Quad(A, A + 1, B + 1, B);
	}
	const FSlateResourceHandle Handle = Brush->GetRenderingResource();
	if (Handle.IsValid())
	{
		FSlateDrawElement::MakeCustomVerts(Out, Layer, Handle, Verts, Idx, nullptr, 0, 0);
	}
	Verts.Reset();
	Idx.Reset();
	++Layer;
	Last = ELast::None;
}

void FApexPainter::Blur(const FApexRect& R, float Strength)
{
	Strength *= Scale * Alpha;
	if (Strength < 0.6f || R.W <= 1.0f || R.H <= 1.0f)
	{
		return;
	}
	Flush();
	if (Last != ELast::None)
	{
		++Layer;
	}
	const FVector2f TL(R.X * Scale + Trans.X, R.Y * Scale + Trans.Y);
	const FVector2f Sz(R.W * Scale, R.H * Scale);
	int32 Kernel = FMath::RoundToInt(Strength * 3.0f);
	int32 Down = 0;
	if (Kernel > 9)
	{
		Down = Kernel >= 64 ? (Kernel >= 96 ? 4 : 3) : 2;
		Kernel /= Down;
	}
	if (Kernel % 2 == 0) ++Kernel;
	Kernel = FMath::Clamp(Kernel, 3, 255);
	int32 RW = FMath::RoundToInt(Sz.X), RH = FMath::RoundToInt(Sz.Y);
	float S = FMath::Max(0.5f, Strength);
	if (Down > 0)
	{
		RW = FMath::DivideAndRoundUp(RW, Down);
		RH = FMath::DivideAndRoundUp(RH, Down);
		S /= Down;
	}
	const FPaintGeometry PG(TL, Sz, Scale);
	Out.PushClip(FSlateClippingZone(FSlateRect(TL.X, TL.Y, TL.X + Sz.X, TL.Y + Sz.Y)));
	FSlateDrawElement::MakePostProcessBlur(Out, Layer, PG, FVector4f(static_cast<float>(Kernel), S, static_cast<float>(RW), static_cast<float>(RH)), Down, FVector4f(0.0f, 0.0f, 0.0f, 0.0f));
	Out.PopClip();
	Last = ELast::Post;
}

float FApexPainter::AscentEm(EApexFont F)
{
	switch (F)
	{
	case EApexFont::Display: return 0.97f;
	case EApexFont::Body:
	case EApexFont::BodyBold: return 1.16f;
	default: return 1.0f;
	}
}

float FApexPainter::MiddleEm(EApexFont F)
{
	switch (F)
	{
	case EApexFont::Display: return 0.578f;
	case EApexFont::Body:
	case EApexFont::BodyBold: return 0.79f;
	default: return 0.65f;
	}
}

float FApexPainter::Text(const FString& S, float X, float Y, EApexFont F, float SizePx, const FLinearColor& C, EApexH H, EApexV V, int32 Spacing, float ShadowA)
{
	if (S.IsEmpty())
	{
		return 0.0f;
	}
	const FSlateFontInfo Info = ApexUI::Font(F, SizePx, Spacing);
	float TopY = Y;
	if (V == EApexV::Middle) TopY = Y - MiddleEm(F) * SizePx;
	else if (V == EApexV::Baseline) TopY = Y - AscentEm(F) * SizePx;
	float LeftX = X;

	if (Spacing != 0)
	{
		// 字距只在整形文字路径里生效
		const TSharedRef<FSlateFontCache> Cache = FSlateApplication::Get().GetRenderer()->GetFontCache();
		const FShapedGlyphSequenceRef Seq = Cache->ShapeUnidirectionalText(S, Info, Scale, TextBiDi::ETextDirection::LeftToRight, ETextShapingMethod::KerningOnly);
		const float W = Seq->GetMeasuredWidth() * Px;
		if (H == EApexH::Center) LeftX = X - W * 0.5f;
		else if (H == EApexH::Right) LeftX = X - W;
		if (Last == ELast::Shape || Last == ELast::Post)
		{
			Flush();
			++Layer;
		}
		Last = ELast::Text;
		FLinearColor Col = C;
		Col.A *= Alpha;
		const FVector2f Box(FMath::Max(8.0f, W + SizePx), SizePx * 1.8f);
		if (ShadowA > 0.0f)
		{
			const float D = FMath::Clamp(Info.Size * 0.045f, 1.0f, 4.0f);
			FSlateDrawElement::MakeShapedText(Out, Layer, Geo.ToPaintGeometry(Box, FSlateLayoutTransform(FVector2f(LeftX + D * 0.6f, TopY + D))), Seq, ESlateDrawEffect::None, FLinearColor(0.0f, 0.0f, 0.0f, ShadowA * Col.A), FLinearColor::Transparent);
			++Layer;
		}
		FSlateDrawElement::MakeShapedText(Out, Layer, Geo.ToPaintGeometry(Box, FSlateLayoutTransform(FVector2f(LeftX, TopY))), Seq, ESlateDrawEffect::None, Col, FLinearColor::Transparent);
		return W;
	}

	const TSharedRef<FSlateFontMeasure> FM = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
	const FVector2f Sz = FVector2f(FM->Measure(S, Info, Scale)) * Px;
	if (H == EApexH::Center) LeftX = X - Sz.X * 0.5f;
	else if (H == EApexH::Right) LeftX = X - Sz.X;
	TextCached(S, Info, FVector2f(LeftX, TopY), C, ShadowA);
	return Sz.X;
}

float FApexPainter::TextFit(const FString& S, float X, float Y, EApexFont F, float SizePx, float MaxW, const FLinearColor& C, EApexH H, EApexV V, int32 Spacing)
{
	const FSlateFontInfo Info = ApexUI::Font(F, SizePx, Spacing);
	const TSharedRef<FSlateFontMeasure> FM = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
	const float W = FVector2f(FM->Measure(S, Info, 1.0f)).X + (Spacing > 0 ? S.Len() * SizePx * 0.75f * Spacing / 1000.0f : 0.0f);
	if (W > MaxW && W > 1.0f)
	{
		SizePx = FMath::Max(9.0f, FMath::FloorToFloat(SizePx * MaxW / W));
	}
	return Text(S, X, Y, F, SizePx, C, H, V, Spacing);
}

void FApexPainter::TextCached(const FString& S, const FSlateFontInfo& InFont, const FVector2f& TopLeft, const FLinearColor& C, float ShadowA)
{
	if (Last == ELast::Shape || Last == ELast::Post)
	{
		Flush();
		++Layer;
	}
	Last = ELast::Text;
	const FVector2f Box(FMath::Max(8.0f, S.Len() * InFont.Size * 1.6f), InFont.Size * 2.4f);
	FLinearColor Col = C;
	Col.A *= Alpha;
	if (ShadowA > 0.0f)
	{
		const float D = FMath::Clamp(InFont.Size * 0.045f, 1.0f, 4.0f);
		FSlateDrawElement::MakeText(Out, Layer, Geo.ToPaintGeometry(Box, FSlateLayoutTransform(TopLeft + FVector2f(D * 0.6f, D))), S, InFont, ESlateDrawEffect::None, FLinearColor(0.0f, 0.0f, 0.0f, ShadowA * Col.A));
		++Layer;
	}
	FSlateDrawElement::MakeText(Out, Layer, Geo.ToPaintGeometry(Box, FSlateLayoutTransform(TopLeft)), S, InFont, ESlateDrawEffect::None, Col);
}

void FApexPainter::TextScaled(const FString& S, const FSlateFontInfo& InFont, const FVector2f& Size, const FVector2f& TopLeft, float InScale, const FLinearColor& C, float ShadowA)
{
	if (Last == ELast::Shape || Last == ELast::Post)
	{
		Flush();
		++Layer;
	}
	Last = ELast::Text;
	FLinearColor Col = C;
	Col.A *= Alpha;
	const FSlateRenderTransform RT(InScale);
	if (ShadowA > 0.0f)
	{
		const float D = FMath::Clamp(InFont.Size * 0.045f, 1.0f, 6.0f);
		FSlateDrawElement::MakeText(Out, Layer, Geo.ToPaintGeometry(Size, FSlateLayoutTransform(TopLeft + FVector2f(D * 0.6f, D)), RT), S, InFont, ESlateDrawEffect::None, FLinearColor(0.0f, 0.0f, 0.0f, ShadowA * Col.A));
		++Layer;
	}
	FSlateDrawElement::MakeText(Out, Layer, Geo.ToPaintGeometry(Size, FSlateLayoutTransform(TopLeft), RT), S, InFont, ESlateDrawEffect::None, Col);
}

int32 FApexPainter::TextWrap(const FString& S, float X, float Y, float MaxW, EApexFont F, float SizePx, float LineH, const FLinearColor& C, int32 MaxLines)
{
	const FSlateFontInfo Info = ApexUI::Font(F, SizePx);
	const TSharedRef<FSlateFontMeasure> FM = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
	static const FString NoStart = TEXT("，。、；：！？）》”’…%,.!?)");
	auto IsLatin = [](TCHAR Ch) { return Ch > 0x20 && Ch < 0x2000; };

	int32 Line = 0;
	int32 Start = 0;
	const int32 Len = S.Len();
	while (Start < Len && Line < MaxLines)
	{
		float W = 0.0f;
		int32 i = Start;
		int32 LastBreak = INDEX_NONE;
		for (; i < Len; ++i)
		{
			const TCHAR Ch = S[i];
			if (Ch == TEXT('\n'))
			{
				break;
			}
			if (i > Start && !(IsLatin(Ch) && IsLatin(S[i - 1])) && !NoStart.Contains(FString::Chr(Ch)))
			{
				LastBreak = i;
			}
			const float Adv = FVector2f(FM->Measure(FStringView(&Ch, 1), Info, 1.0f)).X;
			if (W + Adv > MaxW && i > Start)
			{
				if (LastBreak != INDEX_NONE && LastBreak > Start)
				{
					i = LastBreak;
				}
				break;
			}
			W += Adv;
		}
		Text(S.Mid(Start, i - Start).TrimEnd(), X, Y + Line * LineH, F, SizePx, C, EApexH::Left, EApexV::Middle);
		++Line;
		Start = i;
		if (Start < Len && (S[Start] == TEXT('\n') || S[Start] == TEXT(' ')))
		{
			++Start;
		}
	}
	return Line;
}

void FApexPainter::PushClip(const FApexRect& R)
{
	Flush();
	if (Last != ELast::None)
	{
		++Layer;
		Last = ELast::None;
	}
	const FVector2f TL(R.X * Scale + Trans.X, R.Y * Scale + Trans.Y);
	Out.PushClip(FSlateClippingZone(FSlateRect(TL.X, TL.Y, TL.X + R.W * Scale, TL.Y + R.H * Scale)));
}

void FApexPainter::PopClip()
{
	Flush();
	if (Last != ELast::None)
	{
		++Layer;
		Last = ELast::None;
	}
	Out.PopClip();
}

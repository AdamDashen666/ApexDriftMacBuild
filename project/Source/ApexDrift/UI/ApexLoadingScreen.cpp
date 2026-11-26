#include "UI/ApexLoadingScreen.h"
#include "UI/ApexUIStyle.h"
#include "UI/ApexUIArt.h"
#include "Core/ApexMapRegistry.h"
#include "HAL/PlatformTime.h"
#include "Widgets/SLeafWidget.h"

using namespace ApexUI;

/**
 * 关卡加载画面。构造在游戏线程（字体、缩略图在这里常驻），绘制可能在加载线程：
 * 只读构造时拷贝的文字，不访问世界或控制器。
 */
class SApexLoading : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SApexLoading) : _MapIndex(0) {}
		SLATE_ARGUMENT(int32, MapIndex)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		EnsureLoaded();
		MapIndex = FApexMapRegistry::ClampIndex(InArgs._MapIndex);
		Info = FApexMapRegistry::GetMaps()[MapIndex];
		SealText = MapSeal(MapIndex);
		static const TCHAR* Tips[] = {
			TEXT("入弯前先收油再打方向，车头先指进去，后轮才好甩。"),
			TEXT("手刹点一下就够，按太久车会停下来，而不是漂起来。"),
			TEXT("漂起来以后眼睛看出弯点，方向盘会跟着眼睛走。"),
			TEXT("角度在绿区最稳，橙区得分更高，红区容易打转。"),
			TEXT("两段漂移间隔在 2.4 秒以内，倍率就不会断。"),
			TEXT("贴着弯心的得分区漂过去，得分会乘以 1.65。"),
			TEXT("湿地只是好看，抓地和干地一样，放心踩油门。"),
			TEXT("轮胎尖叫声变细，说明角度在收，再补一点油门。"),
			TEXT("反打方向要快，松方向要慢，车尾才不会甩回来。"),
			TEXT("按 C 换到引擎盖视角，更容易看清车头和弯心的距离。"),
			TEXT("轻轻擦一下外墙也能加分，撞上去就全没了。"),
		};
		const int32 N = UE_ARRAY_COUNT(Tips);
		Tip = Tips[FMath::RandRange(0, N - 1)];
		T0 = FPlatformTime::Seconds();
		SetCanTick(false);
		ForceVolatile(true);
	}

	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(1920.0f, 1080.0f); }

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override
	{
		const FVector2f L = FVector2f(AllottedGeometry.GetLocalSize());
		const float Scale = FMath::Max(0.05f, FMath::Min(L.Y / 1080.0f, L.X / 1440.0f));
		const FVector2f S(L.X / Scale, L.Y / Scale);
		const FGeometry Geo = AllottedGeometry.MakeChild(S, FSlateLayoutTransform(Scale, FVector2f(0.0f, 0.0f)));
		const float T = static_cast<float>(FPlatformTime::Seconds() - T0);

		FApexPainter P(Geo, OutDrawElements, LayerId, 1.0f);
		P.Rect(FApexRect(-4, -4, S.X + 8, S.Y + 8), InkDeep());

		// 主视觉缓慢推近，始终铺满
		const float Zoom = 1.03f + 0.05f * FMath::Min(1.0f, T / 12.0f);
		const float W = FMath::Max(S.X, S.Y * 16.0f / 9.0f) * Zoom;
		const float H = W * 9.0f / 16.0f;
		P.PushClip(FApexRect(0, 0, S.X, S.Y));
		MapArt(P, FApexRect((S.X - W) * 0.5f, (S.Y - H) * 0.5f, W, H), MapIndex, 1.0f);
		P.PopClip();
		P.RectV(FApexRect(-4, S.Y * 0.40f, S.X + 8, S.Y * 0.60f + 4), InkDeep(0.0f), InkDeep(0.94f));
		P.RectH(FApexRect(-4, -4, S.X * 0.56f, S.Y + 8), InkDeep(0.60f), InkDeep(0.0f));

		const float X = 96.0f;
		const float Y = S.Y - 96.0f;
		const float In = EaseOut(T / 0.5f);
		const float Dx = -30.0f * (1.0f - In);
		P.SetAlpha(In);
		Seal(P, FApexRect(X + Dx, Y - 384.0f, 60.0f, 60.0f), SealText, Info.Accent, Ink());
		P.Text(Info.Region, X + 80.0f + Dx, Y - 354.0f, EApexFont::BodyBold, 24.0f, TextHi(), EApexH::Left, EApexV::Middle, 60, 0.4f);
		P.Text(Info.NameZh, X - 8.0f + Dx, Y - 172.0f, EApexFont::Display, 148.0f, TextHi(), EApexH::Left, EApexV::Baseline, 20, 0.4f);
		P.Text(Info.NameEn, X + 2.0f + Dx, Y - 128.0f, EApexFont::Num, 30.0f, Info.Accent, EApexH::Left, EApexV::Middle, 220, 0.3f);
		P.Text(Info.Tagline, X + 2.0f + Dx, Y - 84.0f, EApexFont::Body, 22.0f, Paper(0.88f), EApexH::Left, EApexV::Middle, 0, 0.4f);

		// 小贴士
		const float TipY = Y - 18.0f;
		P.Skew(FApexRect(X + 4.0f, TipY - 15.0f, 92.0f, 30.0f), Paper(0.94f));
		P.Text(TEXT("漂移贴士"), X + 50.0f, TipY, EApexFont::BodyBold, 17.0f, Ink(), EApexH::Center);
		P.Text(Tip, X + 114.0f, TipY, EApexFont::Body, 20.0f, Paper(0.92f));
		P.SetAlpha(1.0f);

		// 细进度动画
		const float BY = S.Y - 46.0f;
		const float BW = S.X - X * 2.0f;
		P.Rect(FApexRect(X, BY, BW, 2.0f), Paper(0.20f));
		const float Ph = FMath::Frac(T * 0.55f);
		const float SegW = BW * 0.26f;
		const float SX = X - SegW + (BW + SegW) * Ph;
		const float A0 = FMath::Max(X, SX), A1 = FMath::Min(X + BW, SX + SegW);
		if (A1 > A0 + 1.0f)
		{
			P.RectH(FApexRect(A0, BY - 1.5f, A1 - A0, 5.0f), WithA(Info.Accent, 0.0f), Info.Accent);
		}
		// 右下：加载中 + 转动的轮圈
		const FVector2f C(S.X - X - 22.0f, Y - 22.0f);
		P.Ring(C, 22.0f, 3.5f, Paper(0.9f));
		for (int32 i = 0; i < 5; ++i)
		{
			const float A = T * 4.2f + i * 2.0f * PI / 5.0f;
			const FVector2f D(FMath::Cos(A), FMath::Sin(A));
			P.LineSeg(C + D * 5.0f, C + D * 20.0f, 3.5f, Paper(0.85f));
		}
		P.Circle(C, 5.0f, Info.Accent);
		P.Text(TEXT("正在加载"), C.X - 42.0f, C.Y, EApexFont::Display, 28.0f, TextHi(), EApexH::Right, EApexV::Middle, 40, 0.4f);
		return P.Finish();
	}

private:
	int32 MapIndex = 0;
	FApexMapInfo Info;
	FString SealText;
	FString Tip;
	double T0 = 0.0;
};

TSharedRef<SWidget> MakeApexLoadingScreen(int32 MapIndex)
{
	return SNew(SApexLoading).MapIndex(MapIndex);
}

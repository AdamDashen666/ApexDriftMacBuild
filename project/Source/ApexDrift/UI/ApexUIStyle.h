#pragma once

#include "CoreMinimal.h"
#include "Fonts/SlateFontInfo.h"
#include "Layout/Geometry.h"
#include "Rendering/RenderingCommon.h"
#include "Styling/SlateBrush.h"
#include "Core/ApexDriftTypes.h"

class FSlateWindowElementList;
class AApexDriftPlayerController;

/**
 * 整套界面共用的设计规范与矢量绘制器。
 * 设计坐标系统一为“高 1080”，宽随屏幕比例变化；尺寸按 8px 栅格取值。
 * 形状母题：与得意黑字形相同的 8 度前倾平行四边形。
 */
enum class EApexFont : uint8
{
	Display,   // 得意黑：标题
	Body,      // 思源黑体 Regular，拉丁/数字走窄体
	BodyBold,  // 思源黑体 Bold
	Num,       // Barlow Condensed SemiBold Italic：数字、英文点缀
	NumHeavy,  // Barlow Condensed ExtraBold Italic：大号数字
	NumLight,  // Barlow Condensed Medium Italic
};

enum class EApexH : uint8 { Left, Center, Right };
enum class EApexV : uint8 { Middle, Baseline, Top };

namespace ApexUI
{
	/** 8 度前倾 */
	constexpr float Slant = 0.1405f;

	inline FLinearColor Hex(uint32 Rgb, float A = 1.0f)
	{
		FLinearColor C = FLinearColor(FColor((Rgb >> 16) & 0xFF, (Rgb >> 8) & 0xFF, Rgb & 0xFF));
		C.A = A;
		return C;
	}
	inline FLinearColor Fade(FLinearColor C, float A) { C.A *= A; return C; }
	inline FLinearColor WithA(FLinearColor C, float A) { C.A = A; return C; }
	/** 在 sRGB 空间混色（和 Slate 的混合空间一致） */
	FLinearColor Mix(const FLinearColor& A, const FLinearColor& B, float T);

	// 色板
	inline FLinearColor Ink(float A = 1.0f)     { return Hex(0x0D0D11, A); }
	inline FLinearColor InkDeep(float A = 1.0f) { return Hex(0x07070A, A); }
	inline FLinearColor Paper(float A = 1.0f)   { return Hex(0xF7F2E8, A); }
	inline FLinearColor TextHi()  { return Paper(1.0f); }
	inline FLinearColor TextMid() { return Paper(0.76f); }
	inline FLinearColor TextLow() { return Paper(0.54f); }
	inline FLinearColor Line(float A = 0.14f) { return Paper(A); }
	inline FLinearColor Gold()   { return Hex(0xFFC94D); }
	inline FLinearColor Red()    { return Hex(0xFF3D4D); }
	inline FLinearColor Green()  { return Hex(0x4FE0A0); }
	inline FLinearColor Orange() { return Hex(0xFF9A3D); }

	/** 加载字体与缩略图并常驻（游戏线程，可重复调用） */
	void EnsureLoaded();
	/** 像素字号（设计坐标），LetterSpacing 单位 1/1000 em */
	FSlateFontInfo Font(EApexFont F, float Px, int32 LetterSpacing = 0);
	FVector2f Measure(const FString& S, const FSlateFontInfo& Font);

	/** 地图缩略图；没有对应资源时返回 nullptr，调用方画占位插画 */
	const FSlateBrush* ThumbBrush(int32 MapIndex);
	/** 印章字：樱 / 枫 / 潮 */
	FString MapSeal(int32 MapIndex);

	FString ModeName(EApexGameModeType Mode);
	FString ModeDesc(EApexGameModeType Mode);
	FString FormatScore(int32 V);            // 1,234,567
	FString FormatTime(float Sec);           // 1:23.45
	FString FormatTimeOrDash(float Sec);

	void PlayUISound(AApexDriftPlayerController* PC, FName Cue);

	/** 把“BEST LAP! 刷新最快圈速”这类中英混排拆成中文主行 + 英文副行 */
	void SplitCallout(const FString& In, FString& OutMain, FString& OutSub);

	inline float EaseOut(float T) { T = FMath::Clamp(T, 0.0f, 1.0f); const float K = 1.0f - T; return 1.0f - K * K * K; }
	inline float EaseOutBack(float T)
	{
		T = FMath::Clamp(T, 0.0f, 1.0f);
		const float C1 = 1.70158f, C3 = C1 + 1.0f, K = T - 1.0f;
		return 1.0f + C3 * K * K * K + C1 * K * K;
	}
	inline float Approach(float V, float Target, float Dt, float Speed) { return V + (Target - V) * (1.0f - FMath::Exp(-Dt * Speed)); }

	/** 主菜单下次显示时把初始焦点放在赛道卡片上（“换赛道”用） */
	extern bool GFocusMapsOnNextMenu;
}

struct FApexRect
{
	float X = 0, Y = 0, W = 0, H = 0;
	FApexRect() = default;
	FApexRect(float InX, float InY, float InW, float InH) : X(InX), Y(InY), W(InW), H(InH) {}
	float R() const { return X + W; }
	float B() const { return Y + H; }
	float CX() const { return X + W * 0.5f; }
	float CY() const { return Y + H * 0.5f; }
	FVector2f Center() const { return FVector2f(CX(), CY()); }
	bool Contains(const FVector2f& P) const { return P.X >= X && P.X < R() && P.Y >= Y && P.Y < B(); }
	FApexRect Inset(float D) const { return FApexRect(X + D, Y + D, W - D * 2, H - D * 2); }
	FApexRect Offset(float DX, float DY) const { return FApexRect(X + DX, Y + DY, W, H); }
};

/**
 * 抗锯齿矢量批绘制：所有形状累积成一个顶点缓冲，遇到文字/贴图/模糊才提交。
 * 只支持无旋转的布局变换（平移 + 缩放）。
 */
class FApexPainter
{
public:
	FApexPainter(const FGeometry& InGeo, FSlateWindowElementList& InOut, int32 InLayer, float InAlpha = 1.0f);
	~FApexPainter();
	FApexPainter(const FApexPainter&) = delete;
	FApexPainter& operator=(const FApexPainter&) = delete;

	void SetAlpha(float A) { Alpha = A; }
	float GetAlpha() const { return Alpha; }
	/** 形状剪切：X += K * (Y0 - Y)，只影响形状和贴图，不影响文字 */
	void SetShear(float K, float Y0) { Shear = K; ShearY0 = Y0; }
	void ClearShear() { Shear = 0.0f; }

	// ---- 填充 ----
	void Rect(const FApexRect& R, const FLinearColor& C);
	void RectV(const FApexRect& R, const FLinearColor& Top, const FLinearColor& Bottom);
	void RectH(const FApexRect& R, const FLinearColor& Left, const FLinearColor& Right);
	/** 前倾平行四边形，倾斜围绕中心，左右各超出外框 K*H/2 */
	void Skew(const FApexRect& R, const FLinearColor& C, float K = ApexUI::Slant);
	void SkewH(const FApexRect& R, const FLinearColor& Left, const FLinearColor& Right, float K = ApexUI::Slant);
	void SkewV(const FApexRect& R, const FLinearColor& Top, const FLinearColor& Bottom, float K = ApexUI::Slant);
	void RoundRect(const FApexRect& R, float Radius, const FLinearColor& C);
	void Circle(const FVector2f& Center, float Radius, const FLinearColor& C);
	void CircleGrad(const FVector2f& Center, float Radius, const FLinearColor& Inner, const FLinearColor& Outer);
	/** 凸多边形 */
	void Poly(TConstArrayView<FVector2f> Pts, const FLinearColor& C);
	void PolyGrad(TConstArrayView<FVector2f> Pts, TConstArrayView<FLinearColor> Cols);
	/** 以 BaseY 为底、上沿为折线的填充（山、海面） */
	void Terrain(TConstArrayView<FVector2f> Top, float BaseY, const FLinearColor& CTop, const FLinearColor& CBottom);
	/** 两条等长折线之间的带状填充，上下边缘抗锯齿 */
	void Strip(TConstArrayView<FVector2f> Top, TConstArrayView<FVector2f> Bottom, const FLinearColor& CTop, const FLinearColor& CBottom);
	/** 以 Center 为心的星形填充（轮廓由调用方再描边） */
	void FillStar(const FVector2f& Center, TConstArrayView<FVector2f> Pts, const FLinearColor& C);
	/** 竖向多段渐变（段间共用顶点，没有接缝；Stops = 相对高度 0..1 + 颜色） */
	void RectStops(const FApexRect& R, TConstArrayView<TPair<float, FLinearColor>> Stops);
	/** 四角不同色的矩形 */
	void RectCorners(const FApexRect& R, const FLinearColor& TL, const FLinearColor& TR, const FLinearColor& BR, const FLinearColor& BL);

	// ---- 描边 ----
	void Stroke(TConstArrayView<FVector2f> Pts, float Thickness, const FLinearColor& C, bool bClosed = false);
	void LineSeg(const FVector2f& A, const FVector2f& B, float Thickness, const FLinearColor& C);
	void StrokeRect(const FApexRect& R, float Thickness, const FLinearColor& C);
	void StrokeSkew(const FApexRect& R, float Thickness, const FLinearColor& C, float K = ApexUI::Slant);
	void StrokeRoundRect(const FApexRect& R, float Radius, float Thickness, const FLinearColor& C);
	void Ring(const FVector2f& Center, float Radius, float Thickness, const FLinearColor& C);
	/** 角度单位弧度，0 = 右，屏幕坐标顺时针为正 */
	void Arc(const FVector2f& Center, float Radius, float Thickness, float A0, float A1, const FLinearColor& C0, const FLinearColor& C1);
	void Arc(const FVector2f& Center, float Radius, float Thickness, float A0, float A1, const FLinearColor& C) { Arc(Center, Radius, Thickness, A0, A1, C, C); }
	/** 柔和外发光（平行四边形） */
	void GlowSkew(const FApexRect& R, float Spread, const FLinearColor& C, float K = ApexUI::Slant);

	// ---- 贴图 / 模糊 ----
	/** 贴图填充（受剪切影响），UV 为 0..1 */
	void Image(const FApexRect& R, const FSlateBrush* Brush, const FLinearColor& Tint, const FVector2f& UV0 = FVector2f(0, 0), const FVector2f& UV1 = FVector2f(1, 1));
	void Blur(const FApexRect& R, float Strength);

	// ---- 文字 ----
	/** 返回文字宽度 */
	float Text(const FString& S, float X, float Y, EApexFont F, float SizePx, const FLinearColor& C, EApexH H = EApexH::Left, EApexV V = EApexV::Middle, int32 Spacing = 0, float ShadowA = 0.0f);
	/** 超宽时自动缩小字号 */
	float TextFit(const FString& S, float X, float Y, EApexFont F, float SizePx, float MaxW, const FLinearColor& C, EApexH H = EApexH::Left, EApexV V = EApexV::Middle, int32 Spacing = 0);
	/** 自动换行（按字断行，避头点），返回行数 */
	int32 TextWrap(const FString& S, float X, float Y, float MaxW, EApexFont F, float SizePx, float LineH, const FLinearColor& C, int32 MaxLines = 8);
	/** 供 HUD 用：字体与尺寸由调用方缓存 */
	void TextCached(const FString& S, const FSlateFontInfo& InFont, const FVector2f& TopLeft, const FLinearColor& C, float ShadowA = 0.0f);

	/** 以中心点缩放的文字（渲染变换缩放，不产生新字号的字形） */
	void TextScaled(const FString& S, const FSlateFontInfo& InFont, const FVector2f& Size, const FVector2f& TopLeft, float InScale, const FLinearColor& C, float ShadowA = 0.0f);

	void PushClip(const FApexRect& R);
	void PopClip();

	void Flush();
	int32 GetLayer() const { return Layer; }
	/** 子绘制器（不同缩放）画完后同步层号 */
	void SetLayer(int32 InLayer) { Flush(); Layer = InLayer; Last = ELast::None; }
	int32 Finish() { Flush(); return Layer + 1; }
	float GetScale() const { return Scale; }
	float PixelSize() const { return Px; }
	const FGeometry& GetGeometry() const { return Geo; }
	FSlateWindowElementList& GetOut() const { return Out; }

	/** 各字体的度量（单位 em）：基线、视觉中线距行框顶部 */
	static float AscentEm(EApexFont F);
	static float MiddleEm(EApexFont F);

private:
	FGeometry Geo;
	FSlateWindowElementList& Out;
	int32 Layer;
	float Alpha = 1.0f;
	float Scale = 1.0f;
	FVector2f Trans = FVector2f::ZeroVector;
	float Shear = 0.0f;
	float ShearY0 = 0.0f;
	float Px = 1.0f; // 一个屏幕像素对应的设计单位

	enum class ELast : uint8 { None, Shape, Text, Post };
	ELast Last = ELast::None;

	TArray<FSlateVertex>& Verts;
	TArray<SlateIndex>& Idx;

	FColor Pack(FLinearColor C) const { C.A *= Alpha; return C.ToFColor(true); }
	void BeginShape();
	uint32 AddV(const FVector2f& P, const FColor& C);
	void Tri(uint32 A, uint32 B, uint32 C) { Idx.Add(A); Idx.Add(B); Idx.Add(C); }
	void Quad(uint32 A, uint32 B, uint32 C, uint32 D) { Tri(A, B, C); Tri(A, C, D); }
	void FillConvex(const FVector2f* Pts, const FColor* Cols, int32 N);
	void StrokeImpl(const FVector2f* Pts, int32 N, float Thickness, const FColor& C, bool bClosed);
};

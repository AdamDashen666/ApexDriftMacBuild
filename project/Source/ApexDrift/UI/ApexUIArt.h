#pragma once

#include "CoreMinimal.h"
#include "UI/ApexUIStyle.h"

/** 全部图标与插画都是矢量自绘，不依赖贴图 */
enum class EApexIcon : uint8
{
	None,
	Sun, Sunset, Rain, Moon, Cloud, Mist,
	ModeFree, ModeAttack, ModeCircuit,
	Gear, Pad, Exit,
	ArrowRight, ArrowLeft, Check, Star, Crown,
	Play, Reset, Restart, Home,
	Display, Wheel, Speaker, Keyboard, Eye, Map,
};

namespace ApexUI
{
	void Icon(FApexPainter& P, EApexIcon I, const FVector2f& C, float Size, const FLinearColor& Col, float Thickness = 2.0f);
	EApexIcon WeatherIcon(FName Key);
	EApexIcon ModeIcon(EApexGameModeType Mode);

	/** 地图主视觉：有缩略图用缩略图，没有则画矢量占位插画。形状受 P 当前剪切影响 */
	void MapArt(FApexPainter& P, const FApexRect& R, int32 MapIndex, float Bright = 1.0f);

	/** 键帽 */
	float KeyCap(FApexPainter& P, float X, float CY, const FString& Label, float H = 26.0f, bool bDark = false);
	/** “键帽 + 说明”，返回占用宽度 */
	float KeyHint(FApexPainter& P, float X, float CY, const FString& Key, const FString& Label);
	/** 带节号的小标题：01 选择赛道 TRACK */
	float SectionLabel(FApexPainter& P, float X, float CY, const FString& No, const FString& Zh, const FString& En, const FLinearColor& Accent);
	/** 难度：5 个前倾小条 */
	void Difficulty(FApexPainter& P, float X, float CY, int32 Level, const FLinearColor& On, const FLinearColor& Off, float W = 9.0f, float H = 14.0f, float Gap = 5.0f);
	/** 印章 */
	void Seal(FApexPainter& P, const FApexRect& R, const FString& Ch, const FLinearColor& Bg, const FLinearColor& Fg);

	/** 操作说明（键盘图 + 手柄图 + 三步漂移教学），设计尺寸 1680 x 800 */
	void ControlsGuide(FApexPainter& P, float X, float Y, const FLinearColor& Accent, float Time);
	constexpr float GuideW = 1680.0f;
	constexpr float GuideH = 800.0f;
}

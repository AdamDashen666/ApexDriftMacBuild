#include "UI/ApexUIArt.h"
#include "Core/ApexMapRegistry.h"

namespace
{
	using V2 = FVector2f;

	V2 Dir(float A) { return V2(FMath::Cos(A), FMath::Sin(A)); }

	void ArcLine(FApexPainter& P, const V2& C, float R, float Deg0, float Deg1, float Th, const FLinearColor& Col)
	{
		P.Arc(C, R, Th, FMath::DegreesToRadians(Deg0), FMath::DegreesToRadians(Deg1), Col);
	}

	/** 云朵轮廓 */
	void CloudShape(FApexPainter& P, const V2& C, float S, const FLinearColor& Col, float Th)
	{
		TArray<V2, TInlineAllocator<40>> Pts;
		auto AddArc = [&](V2 Ctr, float R, float D0, float D1, int32 N)
		{
			for (int32 i = 0; i <= N; ++i)
			{
				Pts.Add(Ctr + Dir(FMath::DegreesToRadians(FMath::Lerp(D0, D1, static_cast<float>(i) / N))) * R);
			}
		};
		AddArc(C + V2(-0.46f, 0.16f) * S, 0.34f * S, 90.0f, 250.0f, 7);
		AddArc(C + V2(-0.06f, -0.16f) * S, 0.44f * S, 200.0f, 335.0f, 8);
		AddArc(C + V2(0.50f, 0.14f) * S, 0.36f * S, -80.0f, 90.0f, 7);
		P.Stroke(Pts, Th, Col, true);
	}

	V2 SafeNormal2(const V2& V)
	{
		const float L = FMath::Sqrt(V.X * V.X + V.Y * V.Y);
		return L > 1e-5f ? V2(V.X / L, V.Y / L) : V2(0, 0);
	}

	float Hash01(int32 I)
	{
		const float V = FMath::Sin(I * 12.9898f + 78.233f) * 43758.5453f;
		return V - FMath::FloorToFloat(V);
	}
}

namespace ApexUI
{
	EApexIcon WeatherIcon(FName Key)
	{
		static const FName NSun(TEXT("Sun")), NSunset(TEXT("Sunset")), NRain(TEXT("Rain")), NMoon(TEXT("Moon")), NCloud(TEXT("Cloud")), NMist(TEXT("Mist"));
		if (Key == NSunset) return EApexIcon::Sunset;
		if (Key == NRain) return EApexIcon::Rain;
		if (Key == NMoon) return EApexIcon::Moon;
		if (Key == NCloud) return EApexIcon::Cloud;
		if (Key == NMist) return EApexIcon::Mist;
		return EApexIcon::Sun;
	}

	EApexIcon ModeIcon(EApexGameModeType Mode)
	{
		switch (Mode)
		{
		case EApexGameModeType::DriftAttack3Min: return EApexIcon::ModeAttack;
		case EApexGameModeType::Circuit3Laps:    return EApexIcon::ModeCircuit;
		default:                                 return EApexIcon::ModeFree;
		}
	}

	void Icon(FApexPainter& P, EApexIcon I, const FVector2f& C, float Size, const FLinearColor& Col, float Th)
	{
		const float S = Size * 0.5f;
		auto L = [&](float X0, float Y0, float X1, float Y1) { P.LineSeg(C + V2(X0, Y0) * S, C + V2(X1, Y1) * S, Th, Col); };
		auto PolyL = [&](std::initializer_list<V2> Pts, bool bClosed = false)
		{
			TArray<V2, TInlineAllocator<16>> A;
			for (const V2& Pt : Pts) A.Add(C + Pt * S);
			P.Stroke(A, Th, Col, bClosed);
		};
		auto Fill = [&](std::initializer_list<V2> Pts)
		{
			TArray<V2, TInlineAllocator<16>> A;
			for (const V2& Pt : Pts) A.Add(C + Pt * S);
			P.Poly(A, Col);
		};

		switch (I)
		{
		case EApexIcon::Sun:
			P.Ring(C, 0.44f * S, Th, Col);
			for (int32 i = 0; i < 8; ++i)
			{
				const V2 D = Dir(i * PI / 4.0f);
				P.LineSeg(C + D * 0.70f * S, C + D * 0.98f * S, Th, Col);
			}
			break;
		case EApexIcon::Sunset:
			ArcLine(P, C + V2(0, 0.30f) * S, 0.46f * S, 180.0f, 360.0f, Th, Col);
			for (int32 i = 0; i < 5; ++i)
			{
				const V2 D = Dir(PI + i * PI / 4.0f);
				P.LineSeg(C + V2(0, 0.30f) * S + D * 0.70f * S, C + V2(0, 0.30f) * S + D * 0.98f * S, Th, Col);
			}
			L(-1.0f, 0.30f, 1.0f, 0.30f);
			L(-0.62f, 0.62f, 0.62f, 0.62f);
			L(-0.26f, 0.92f, 0.26f, 0.92f);
			break;
		case EApexIcon::Rain:
			CloudShape(P, C + V2(0, -0.28f) * S, S * 0.86f, Col, Th);
			L(-0.42f, 0.46f, -0.56f, 0.90f);
			L(0.04f, 0.46f, -0.10f, 0.90f);
			L(0.50f, 0.46f, 0.36f, 0.90f);
			break;
		case EApexIcon::Moon:
		{
			TArray<V2, TInlineAllocator<40>> A;
			for (int32 i = 0; i <= 18; ++i)
			{
				A.Add(C + V2(0.04f, 0.0f) * S + Dir(FMath::DegreesToRadians(FMath::Lerp(40.0f, 290.0f, i / 18.0f))) * 0.82f * S);
			}
			const V2 C2 = C + V2(0.44f, -0.22f) * S;
			for (int32 i = 0; i <= 14; ++i)
			{
				A.Add(C2 + Dir(FMath::DegreesToRadians(FMath::Lerp(258.0f, 72.0f, i / 14.0f))) * 0.60f * S);
			}
			P.Stroke(A, Th, Col, true);
			break;
		}
		case EApexIcon::Cloud:
			CloudShape(P, C, S, Col, Th);
			break;
		case EApexIcon::Mist:
			for (int32 k = 0; k < 4; ++k)
			{
				const float Y = -0.66f + k * 0.44f;
				const float X0 = (k % 2) ? -0.62f : -0.95f;
				TArray<V2, TInlineAllocator<16>> A;
				for (int32 i = 0; i <= 8; ++i)
				{
					const float T = i / 8.0f;
					A.Add(C + V2(X0 + T * 1.55f, Y + FMath::Sin(T * 2.0f * PI) * 0.12f) * S);
				}
				P.Stroke(A, Th, Col, false);
			}
			break;
		case EApexIcon::ModeFree:
		{
			// S 形轨迹 + 箭头
			TArray<V2, TInlineAllocator<32>> A;
			for (int32 i = 0; i <= 18; ++i)
			{
				const float T = i / 18.0f;
				A.Add(C + V2(-0.92f + T * 1.66f, FMath::Sin(T * 2.0f * PI) * -0.52f + 0.04f) * S);
			}
			P.Stroke(A, Th, Col, false);
			PolyL({ V2(0.46f, -0.24f), V2(0.76f, 0.06f), V2(0.90f, -0.36f) });
			break;
		}
		case EApexIcon::ModeAttack:
			P.Ring(C + V2(0, 0.12f) * S, 0.74f * S, Th, Col);
			L(-0.24f, -0.96f, 0.24f, -0.96f);
			L(0.0f, -0.96f, 0.0f, -0.62f);
			L(0.56f, -0.56f, 0.74f, -0.74f);
			L(0.0f, 0.12f, 0.34f, -0.22f);
			P.Circle(C + V2(0, 0.12f) * S, 0.10f * S, Col);
			break;
		case EApexIcon::ModeCircuit:
		{
			// 黑白方格旗
			L(-0.74f, -0.94f, -0.74f, 0.96f);
			const float X0 = -0.74f, Y0 = -0.86f, W = 0.42f, H = 0.34f;
			for (int32 r = 0; r < 3; ++r)
			{
				for (int32 c = 0; c < 4; ++c)
				{
					if ((r + c) % 2 == 0)
					{
						const float Wave = FMath::Sin(c * 1.2f) * 0.07f;
						const float Wave2 = FMath::Sin((c + 1) * 1.2f) * 0.07f;
						Fill({ V2(X0 + c * W, Y0 + r * H + Wave), V2(X0 + (c + 1) * W, Y0 + r * H + Wave2), V2(X0 + (c + 1) * W, Y0 + (r + 1) * H + Wave2), V2(X0 + c * W, Y0 + (r + 1) * H + Wave) });
					}
				}
			}
			break;
		}
		case EApexIcon::Gear:
		{
			TArray<V2, TInlineAllocator<40>> A;
			for (int32 k = 0; k < 8; ++k)
			{
				const float A0 = k * PI / 4.0f;
				A.Add(C + Dir(A0 - 0.26f) * 0.70f * S);
				A.Add(C + Dir(A0 - 0.17f) * 0.96f * S);
				A.Add(C + Dir(A0 + 0.17f) * 0.96f * S);
				A.Add(C + Dir(A0 + 0.26f) * 0.70f * S);
			}
			P.Stroke(A, Th, Col, true);
			P.Ring(C, 0.32f * S, Th, Col);
			break;
		}
		case EApexIcon::Pad:
		{
			TArray<V2, TInlineAllocator<24>> A;
			const V2 Pts[] = { { -0.52f, -0.50f }, { 0.52f, -0.50f }, { 0.82f, -0.34f }, { 1.0f, 0.42f }, { 0.84f, 0.66f }, { 0.60f, 0.58f }, { 0.34f, 0.22f }, { -0.34f, 0.22f }, { -0.60f, 0.58f }, { -0.84f, 0.66f }, { -1.0f, 0.42f }, { -0.82f, -0.34f } };
			for (const V2& Pt : Pts) A.Add(C + Pt * S);
			P.Stroke(A, Th, Col, true);
			L(-0.62f, -0.14f, -0.26f, -0.14f);
			L(-0.44f, -0.32f, -0.44f, 0.04f);
			P.Circle(C + V2(0.34f, -0.06f) * S, 0.10f * S, Col);
			P.Circle(C + V2(0.58f, -0.24f) * S, 0.10f * S, Col);
			break;
		}
		case EApexIcon::Exit:
			PolyL({ V2(0.10f, -0.86f), V2(-0.80f, -0.86f), V2(-0.80f, 0.86f), V2(0.10f, 0.86f) });
			L(-0.28f, 0.0f, 0.90f, 0.0f);
			PolyL({ V2(0.50f, -0.40f), V2(0.90f, 0.0f), V2(0.50f, 0.40f) });
			break;
		case EApexIcon::ArrowRight:
			PolyL({ V2(-0.32f, -0.72f), V2(0.40f, 0.0f), V2(-0.32f, 0.72f) });
			break;
		case EApexIcon::ArrowLeft:
			PolyL({ V2(0.32f, -0.72f), V2(-0.40f, 0.0f), V2(0.32f, 0.72f) });
			break;
		case EApexIcon::Check:
			PolyL({ V2(-0.72f, 0.04f), V2(-0.20f, 0.56f), V2(0.76f, -0.54f) });
			break;
		case EApexIcon::Star:
		{
			for (int32 k = 0; k < 5; ++k)
			{
				const float A0 = -HALF_PI + k * 2.0f * PI / 5.0f;
				const V2 Tip = C + Dir(A0) * S;
				const V2 L1 = C + Dir(A0 - PI / 5.0f) * S * 0.44f;
				const V2 R1 = C + Dir(A0 + PI / 5.0f) * S * 0.44f;
				const V2 Pts[4] = { C, L1, Tip, R1 };
				P.Poly(MakeArrayView(Pts, 4), Col);
			}
			break;
		}
		case EApexIcon::Crown:
			Fill({ V2(-0.90f, -0.40f), V2(-0.42f, 0.06f), V2(-0.60f, 0.56f), V2(-0.72f, 0.56f) });
			Fill({ V2(0.0f, -0.74f), V2(0.40f, 0.10f), V2(0.40f, 0.56f), V2(-0.40f, 0.56f), V2(-0.40f, 0.10f) });
			Fill({ V2(0.90f, -0.40f), V2(0.72f, 0.56f), V2(0.40f, 0.56f), V2(0.42f, 0.06f) });
			Fill({ V2(-0.72f, 0.66f), V2(0.72f, 0.66f), V2(0.70f, 0.84f), V2(-0.70f, 0.84f) });
			break;
		case EApexIcon::Play:
			Fill({ V2(-0.46f, -0.78f), V2(0.80f, 0.0f), V2(-0.46f, 0.78f) });
			break;
		case EApexIcon::Reset:
			// 车辆回到赛道：向下箭头落在线上
			L(0.0f, -0.92f, 0.0f, 0.34f);
			PolyL({ V2(-0.42f, -0.06f), V2(0.0f, 0.36f), V2(0.42f, -0.06f) });
			L(-0.86f, 0.80f, 0.86f, 0.80f);
			break;
		case EApexIcon::Restart:
			ArcLine(P, C, 0.76f * S, -50.0f, 250.0f, Th, Col);
			PolyL({ V2(0.10f, -0.90f), V2(0.50f, -0.58f), V2(0.16f, -0.20f) });
			break;
		case EApexIcon::Home:
			PolyL({ V2(-0.94f, 0.02f), V2(0.0f, -0.82f), V2(0.94f, 0.02f) });
			PolyL({ V2(-0.64f, -0.18f), V2(-0.64f, 0.82f), V2(0.64f, 0.82f), V2(0.64f, -0.18f) });
			break;
		case EApexIcon::Display:
			P.StrokeRect(FApexRect(C.X - 0.94f * S, C.Y - 0.72f * S, 1.88f * S, 1.16f * S), Th, Col);
			L(0.0f, 0.46f, 0.0f, 0.80f);
			L(-0.46f, 0.80f, 0.46f, 0.80f);
			break;
		case EApexIcon::Wheel:
			P.Ring(C, 0.86f * S, Th, Col);
			P.Ring(C, 0.24f * S, Th, Col);
			L(-0.84f, -0.04f, -0.24f, 0.0f);
			L(0.84f, -0.04f, 0.24f, 0.0f);
			L(0.0f, 0.24f, 0.0f, 0.86f);
			break;
		case EApexIcon::Speaker:
			PolyL({ V2(-0.90f, -0.30f), V2(-0.52f, -0.30f), V2(-0.06f, -0.72f), V2(-0.06f, 0.72f), V2(-0.52f, 0.30f), V2(-0.90f, 0.30f) }, true);
			ArcLine(P, C + V2(0.04f, 0.0f) * S, 0.40f * S, -45.0f, 45.0f, Th, Col);
			ArcLine(P, C + V2(0.04f, 0.0f) * S, 0.78f * S, -45.0f, 45.0f, Th, Col);
			break;
		case EApexIcon::Keyboard:
			P.StrokeRoundRect(FApexRect(C.X - 0.98f * S, C.Y - 0.62f * S, 1.96f * S, 1.24f * S), 0.16f * S, Th, Col);
			for (int32 k = 0; k < 4; ++k)
			{
				P.Circle(C + V2(-0.60f + k * 0.40f, -0.22f) * S, 0.085f * S, Col);
			}
			L(-0.46f, 0.26f, 0.46f, 0.26f);
			break;
		case EApexIcon::Eye:
		{
			TArray<V2, TInlineAllocator<40>> A;
			for (int32 i = 0; i <= 12; ++i) { const float T = i / 12.0f; A.Add(C + V2(-0.96f + 1.92f * T, -FMath::Sin(T * PI) * 0.60f) * S); }
			for (int32 i = 1; i < 12; ++i) { const float T = i / 12.0f; A.Add(C + V2(0.96f - 1.92f * T, FMath::Sin(T * PI) * 0.60f) * S); }
			P.Stroke(A, Th, Col, true);
			P.Circle(C, 0.26f * S, Col);
			break;
		}
		case EApexIcon::Map:
			PolyL({ V2(-0.92f, -0.56f), V2(-0.30f, -0.80f), V2(0.30f, -0.56f), V2(0.92f, -0.80f), V2(0.92f, 0.56f), V2(0.30f, 0.80f), V2(-0.30f, 0.56f), V2(-0.92f, 0.80f) }, true);
			L(-0.30f, -0.80f, -0.30f, 0.56f);
			L(0.30f, -0.56f, 0.30f, 0.80f);
			break;
		default:
			break;
		}
	}

	// =====================================================================
	// 地图占位插画
	// =====================================================================

	/** 五瓣花 */
	static void Blossom(FApexPainter& P, const V2& C, float R, float Rot, const FLinearColor& Petal, const FLinearColor& Core)
	{
		for (int32 k = 0; k < 5; ++k)
		{
			const V2 D = Dir(Rot + k * 2.0f * PI / 5.0f);
			P.Circle(C + D * R * 0.52f, R * 0.46f, Petal);
		}
		P.Circle(C, R * 0.40f, Petal);
		for (int32 k = 0; k < 5; ++k)
		{
			const V2 D = Dir(Rot + (k + 0.5f) * 2.0f * PI / 5.0f);
			P.Circle(C + D * R * 0.17f, R * 0.075f, Core);
		}
	}

	/** 枫叶：五裂片 + 叶柄 */
	static void MapleLeaf(FApexPainter& P, const V2& C, float R, float Rot, const FLinearColor& Col)
	{
		static const float Ang[5] = { 0.0f, 0.95f, -0.95f, 1.85f, -1.85f };
		static const float Len[5] = { 1.0f, 0.92f, 0.92f, 0.62f, 0.62f };
		for (int32 k = 0; k < 5; ++k)
		{
			const V2 D = Dir(Rot + Ang[k]);
			const V2 N(-D.Y, D.X);
			const V2 Q[4] = { C - D * R * 0.10f, C + D * R * Len[k] * 0.46f + N * R * 0.24f, C + D * R * Len[k], C + D * R * Len[k] * 0.46f - N * R * 0.24f };
			P.Poly(MakeArrayView(Q, 4), Col);
		}
		const V2 D = Dir(Rot);
		P.LineSeg(C, C - D * R * 0.75f, FMath::Max(1.0f, R * 0.07f), Col);
	}

	static void ArtFuji(FApexPainter& P, const FApexRect& R)
	{
		auto Pt = [&](float X, float Y) { return V2(R.X + R.W * X, R.Y + R.H * Y); };
		// 相邻色带上下各叠 2px，避免接缝
		P.RectV(FApexRect(R.X, R.Y, R.W, R.H * 0.42f + 2.0f), Hex(0x6EA3DC), Hex(0xC8DBF2));
		P.RectV(FApexRect(R.X, R.Y + R.H * 0.42f, R.W, R.H * 0.31f), Hex(0xC8DBF2), Hex(0xFFDCE2));
		P.Circle(Pt(0.76f, 0.34f), R.H * 0.125f, Hex(0xFF8C9E));
		// 薄云
		for (int32 i = 0; i < 5; ++i)
		{
			const float X = 0.05f + 0.20f * i + 0.06f * Hash01(i + 41);
			const float Y = 0.40f + 0.10f * Hash01(i * 3 + 5);
			const float W = 0.14f + 0.10f * Hash01(i * 7 + 3);
			const FApexRect Cl(R.X + R.W * X, R.Y + R.H * Y, FMath::Min(R.W * W, R.R() - (R.X + R.W * X)), R.H * 0.020f);
			P.RoundRect(Cl, Cl.H * 0.5f, Hex(0xFFFFFF, 0.46f));
		}
		// 远山
		{
			TArray<V2, TInlineAllocator<64>> A;
			for (int32 i = 0; i <= 48; ++i)
			{
				const float T = i / 48.0f;
				A.Add(Pt(T, 0.63f - 0.035f * FMath::Sin(T * 9.0f) - 0.02f * FMath::Sin(T * 23.0f + 1.0f)));
			}
			P.Terrain(A, R.Y + R.H * 0.74f, Hex(0xB4BFE2), Hex(0xC9CDE8));
		}
		// 富士山
		auto Sil = [](float X)
		{
			if (X < 0.405f) return 0.70f - 0.50f * FMath::Pow(FMath::Clamp((X - 0.02f) / 0.385f, 0.0f, 1.0f), 1.7f);
			if (X > 0.465f) return 0.70f - 0.50f * FMath::Pow(FMath::Clamp((0.90f - X) / 0.435f, 0.0f, 1.0f), 1.7f);
			const float K = (X - 0.435f) / 0.030f;
			return 0.200f + 0.016f * (1.0f - K * K);
		};
		{
			TArray<V2, TInlineAllocator<128>> A, Refl0, Refl1;
			for (int32 i = 0; i <= 88; ++i)
			{
				const float X = 0.02f + 0.88f * i / 88.0f;
				A.Add(Pt(X, Sil(X)));
				Refl0.Add(Pt(X, 0.712f));
				Refl1.Add(Pt(X, 0.712f + (0.70f - Sil(X)) * 0.50f));
			}
			P.Terrain(A, R.Y + R.H * 0.72f, Hex(0x5672B2), Hex(0x9AACD8));
			// 雪线：上沿取山形，下沿为长短不一的雪沟
			TArray<V2, TInlineAllocator<96>> S0, S1;
			for (int32 i = 0; i <= 72; ++i)
			{
				const float X = 0.326f + 0.236f * i / 72.0f;
				const float U = (X - 0.326f) / 0.236f * 7.0f;
				const float F = FMath::Frac(U);
				const float Tri = F < 0.5f ? F * 2.0f : (1.0f - F) * 2.0f;
				const int32 Cell = FMath::FloorToInt(U);
				const float Amp = 0.035f + 0.085f * Hash01(Cell * 5 + 2) * FMath::Clamp(1.2f - FMath::Abs(X - 0.440f) / 0.14f, 0.25f, 1.0f);
				const float Y0 = Sil(X);
				S0.Add(Pt(X, Y0));
				S1.Add(Pt(X, FMath::Max(Y0, 0.335f + Amp * Tri - 0.02f * (1.0f - FMath::Abs(X - 0.44f) / 0.12f))));
			}
			P.Strip(S0, S1, Hex(0xFFFFFF), Hex(0xEEF2FB));
			// 湖面
			P.RectV(FApexRect(R.X, R.Y + R.H * 0.70f, R.W, R.H * 0.30f), Hex(0xD3DDF1), Hex(0x6F97CC));
			P.Strip(Refl0, Refl1, Hex(0x7C95C8, 0.66f), Hex(0x7C95C8, 0.20f));
		}
		P.Rect(FApexRect(R.X, R.Y + R.H * 0.699f, R.W, R.H * 0.006f), Hex(0xFFFFFF, 0.55f));
		for (int32 i = 0; i < 14; ++i)
		{
			const float Y = 0.73f + i * 0.019f;
			const float W = 0.03f + 0.10f * Hash01(i + 3);
			const float X = 0.04f + (0.90f - W) * Hash01(i * 7 + 1);
			const FApexRect L(R.X + R.W * X, R.Y + R.H * Y, R.W * W, R.H * 0.0045f);
			P.RoundRect(L, L.H * 0.5f, Hex(0xFFFFFF, 0.38f));
		}
		// 樱枝
		const FLinearColor Bark = Hex(0x5A3440);
		auto Branch = [&](std::initializer_list<V2> Pts, float Th)
		{
			TArray<V2, TInlineAllocator<8>> A;
			for (const V2& Q : Pts) A.Add(Pt(Q.X, Q.Y));
			P.Stroke(A, R.H * Th, Bark, false);
		};
		Branch({ V2(0.004f, 0.03f), V2(0.09f, 0.085f), V2(0.19f, 0.10f), V2(0.30f, 0.075f) }, 0.016f);
		Branch({ V2(0.09f, 0.085f), V2(0.15f, 0.17f), V2(0.20f, 0.20f) }, 0.010f);
		Branch({ V2(0.996f, 0.02f), V2(0.93f, 0.065f), V2(0.84f, 0.07f) }, 0.013f);
		for (int32 i = 0; i < 58; ++i)
		{
			const bool bLeft = i % 4 != 0;
			const float A = Hash01(i * 3 + 11), B = Hash01(i * 5 + 7);
			const float Rr = R.H * (0.016f + 0.018f * Hash01(i * 9 + 2));
			const float X = bLeft ? (0.31f * A) : (1.0f - 0.18f * A);
			const float Along = bLeft ? (0.05f + 0.05f * A + (i % 3 == 0 ? 0.10f * A : 0.0f)) : 0.055f;
			const float Y = Along + (B - 0.5f) * 0.13f * (1.0f - 0.5f * A);
			V2 C = Pt(X, Y);
			C.X = FMath::Clamp(C.X, R.X + Rr, R.R() - Rr);
			C.Y = FMath::Clamp(C.Y, R.Y + Rr, R.B() - Rr);
			const uint32 Cols[4] = { 0xFFA9C4, 0xFFC3D6, 0xFFDCE7, 0xFF95B6 };
			Blossom(P, C, Rr, A * 6.0f, Hex(Cols[i % 4], 0.97f), Hex(0xE4547F, 0.75f));
		}
		// 落瓣
		for (int32 i = 0; i < 18; ++i)
		{
			const V2 C = Pt(0.08f + 0.86f * Hash01(i * 3 + 1), 0.18f + 0.60f * Hash01(i * 7 + 5));
			const float Rr = R.H * (0.006f + 0.005f * Hash01(i));
			const V2 D = Dir(Hash01(i * 11) * 2.0f * PI) * Rr, N = V2(-D.Y, D.X) * 0.55f;
			const V2 Q[4] = { C + D, C + N, C - D, C - N };
			P.Poly(MakeArrayView(Q, 4), Hex(0xFFC9DA, 0.85f));
		}
	}

	static void ArtMomiji(FApexPainter& P, const FApexRect& R)
	{
		auto Pt = [&](float X, float Y) { return V2(R.X + R.W * X, R.Y + R.H * Y); };
		P.RectV(FApexRect(R.X, R.Y, R.W, R.H * 0.62f + 2.0f), Hex(0xFFE9C4), Hex(0xFFB071));
		P.Rect(FApexRect(R.X, R.Y + R.H * 0.62f, R.W, R.H * 0.38f), Hex(0xFFB071));
		P.Circle(Pt(0.60f, 0.34f), R.H * 0.19f, Hex(0xFFF4DA));
		const uint32 Top[4] = { 0xF39A62, 0xDE6A3C, 0xB9402A, 0x74211F };
		const uint32 Bot[4] = { 0xFFC08A, 0xEE8A52, 0xD2573A, 0x4A1418 };
		for (int32 k = 0; k < 4; ++k)
		{
			TArray<V2, TInlineAllocator<128>> A;
			for (int32 i = 0; i <= 96; ++i)
			{
				const float T = i / 96.0f;
				const float Base = 0.46f + k * 0.125f;
				const float Y = Base - (0.10f - k * 0.012f) * FMath::Sin(T * (4.2f + k * 1.3f) + k * 1.7f)
					- 0.014f * FMath::Abs(FMath::Sin(T * (34.0f - k * 5.0f) + k)) - 0.008f * FMath::Abs(FMath::Sin(T * 71.0f + k * 2.0f));
				A.Add(Pt(T, FMath::Clamp(Y, 0.02f, 0.98f)));
			}
			P.Terrain(A, R.B(), Hex(Top[k]), Hex(Bot[k]));
			if (k < 3)
			{
				// 山间薄雾
				P.RectV(FApexRect(R.X, R.Y + R.H * (0.50f + k * 0.13f), R.W, R.H * 0.10f), Hex(0xFFE6C4, 0.0f), Hex(0xFFE6C4, 0.18f));
				P.RectV(FApexRect(R.X, R.Y + R.H * (0.60f + k * 0.13f) - 0.5f, R.W, R.H * 0.08f), Hex(0xFFE6C4, 0.18f), Hex(0xFFE6C4, 0.0f));
			}
		}
		// 盘山路：由近到远变细，收在第三道山脊
		{
			TArray<V2, TInlineAllocator<160>> A;
			for (int32 i = 0; i <= 120; ++i)
			{
				const float T = i / 120.0f;
				const float Amp = 0.30f * FMath::Pow(1.0f - T, 1.5f) + 0.02f;
				A.Add(Pt(0.46f + Amp * FMath::Sin(T * 14.5f + 0.4f) + T * 0.10f, 0.985f - 0.275f * FMath::Pow(T, 0.82f)));
			}
			for (int32 Pass = 0; Pass < 2; ++Pass)
			{
				for (int32 i = 0; i + 3 < A.Num(); i += 3)
				{
					const float T = static_cast<float>(i) / A.Num();
					const float W = FMath::Lerp(0.030f, 0.005f, FMath::Sqrt(T)) * R.H;
					P.Stroke(MakeArrayView(&A[i], 4), Pass == 0 ? W * 1.5f : W, Pass == 0 ? Hex(0x2A1014) : Hex(0xFFE7C8), false);
				}
			}
		}
		for (int32 i = 0; i < 40; ++i)
		{
			const float A = Hash01(i * 3 + 5), B = Hash01(i * 5 + 17);
			const float Rr = R.H * (0.018f + 0.024f * Hash01(i * 9 + 4));
			V2 C = Pt(1.0f - 0.36f * A * A, 0.28f * B * (1.0f - 0.6f * A));
			C.X = FMath::Clamp(C.X, R.X + Rr, R.R() - Rr);
			C.Y = FMath::Clamp(C.Y, R.Y + Rr, R.B() - Rr);
			const uint32 Cols[4] = { 0xD93A22, 0xF06A2A, 0xB5261E, 0xFF9340 };
			MapleLeaf(P, C, Rr, 0.6f + Hash01(i) * 2.4f, Hex(Cols[i % 4], 0.97f));
		}
		for (int32 i = 0; i < 9; ++i)
		{
			const float Rr = R.H * (0.012f + 0.010f * Hash01(i * 4 + 1));
			MapleLeaf(P, Pt(0.10f + 0.80f * Hash01(i * 3 + 2), 0.30f + 0.46f * Hash01(i * 5 + 3)), Rr, Hash01(i * 7) * 6.0f, Hex(i % 2 ? 0xD93A22 : 0xF06A2A, 0.85f));
		}
	}

	static void ArtKuroshio(FApexPainter& P, const FApexRect& R)
	{
		auto Pt = [&](float X, float Y) { return V2(R.X + R.W * X, R.Y + R.H * Y); };
		P.RectV(FApexRect(R.X, R.Y, R.W, R.H * 0.36f + 2.0f), Hex(0x143A78), Hex(0x2F86C9));
		P.RectV(FApexRect(R.X, R.Y + R.H * 0.36f, R.W, R.H * 0.26f), Hex(0x2F86C9), Hex(0xFFD2A0));
		// 云带
		for (int32 i = 0; i < 6; ++i)
		{
			const float W = 0.16f + 0.16f * Hash01(i * 5 + 2);
			const float X = (0.98f - W) * Hash01(i * 3 + 9);
			const FApexRect Cl(R.X + R.W * X, R.Y + R.H * (0.36f + 0.035f * i), R.W * W, R.H * 0.012f);
			P.RoundRect(Cl, Cl.H * 0.5f, Hex(0xFFE3C8, 0.30f));
		}
		// 落日
		{
			TArray<V2, TInlineAllocator<80>> A;
			for (int32 i = 0; i <= 64; ++i)
			{
				A.Add(Pt(0.30f, 0.612f) + Dir(PI + PI * i / 64.0f) * R.H * 0.20f);
			}
			P.FillStar(Pt(0.30f, 0.612f), A, Hex(0xFFF0C8));
		}
		// 远岛
		{
			TArray<V2, TInlineAllocator<48>> A;
			for (int32 i = 0; i <= 40; ++i)
			{
				const float T = i / 40.0f;
				A.Add(Pt(0.40f + 0.30f * T, 0.612f - 0.045f * FMath::Sin(T * PI) - 0.008f * FMath::Sin(T * 17.0f)));
			}
			P.Terrain(A, R.Y + R.H * 0.615f, Hex(0x6E7FA8), Hex(0x8A8FAE));
		}
		P.RectV(FApexRect(R.X, R.Y + R.H * 0.61f, R.W, R.H * 0.39f), Hex(0x3C8FC6), Hex(0x0B2A55));
		for (int32 i = 0; i < 14; ++i)
		{
			const float T = i / 13.0f;
			const float W = (0.20f - 0.13f * T) * (0.6f + 0.7f * Hash01(i + 2));
			const FApexRect L(R.X + R.W * (0.30f - W * 0.5f + 0.02f * (Hash01(i * 3) - 0.5f)), R.Y + R.H * (0.625f + T * 0.30f), R.W * W, R.H * 0.009f);
			P.RoundRect(L, L.H * 0.5f, Hex(0xFFE2B0, 0.78f * (1.0f - 0.6f * T)));
		}
		// 码头与集装箱
		const FLinearColor Dark = Hex(0x0A1C38);
		P.Rect(FApexRect(R.X + R.W * 0.50f, R.Y + R.H * 0.70f, R.W * 0.50f, R.H * 0.07f), Dark);
		const uint32 Box[5] = { 0xD9563C, 0x2E9BD6, 0xF2B84B, 0x3F6BB0, 0xC8CED8 };
		for (int32 c = 0; c < 8; ++c)
		{
			const int32 N = 1 + static_cast<int32>(Hash01(c * 13 + 3) * 2.8f);
			for (int32 k = 0; k < N; ++k)
			{
				const FApexRect Bx(R.X + R.W * (0.520f + c * 0.0585f), R.Y + R.H * (0.70f - (k + 1) * 0.052f), R.W * 0.054f, R.H * 0.047f);
				P.Rect(Bx, Hex(Box[(c * 3 + k * 2) % 5]));
				for (int32 q = 1; q < 6; ++q)
				{
					P.Rect(FApexRect(Bx.X + Bx.W * q / 6.0f, Bx.Y + Bx.H * 0.12f, FMath::Max(0.6f, Bx.W * 0.012f), Bx.H * 0.76f), Ink(0.18f));
				}
			}
		}
		// 岸桥
		for (int32 k = 0; k < 2; ++k)
		{
			const float X = 0.62f + k * 0.22f;
			const float Th = R.H * 0.014f;
			P.LineSeg(Pt(X, 0.70f), Pt(X + 0.016f, 0.26f), Th, Dark);
			P.LineSeg(Pt(X + 0.09f, 0.70f), Pt(X + 0.074f, 0.26f), Th, Dark);
			P.LineSeg(Pt(X + 0.006f, 0.46f), Pt(X + 0.084f, 0.46f), Th, Dark);
			P.LineSeg(Pt(X - 0.16f, 0.30f), Pt(X + 0.13f, 0.30f), Th * 1.2f, Dark);
			P.LineSeg(Pt(X + 0.045f, 0.15f), Pt(X - 0.15f, 0.30f), Th * 0.5f, Dark);
			P.LineSeg(Pt(X + 0.045f, 0.15f), Pt(X + 0.12f, 0.30f), Th * 0.5f, Dark);
			P.LineSeg(Pt(X + 0.016f, 0.265f), Pt(X + 0.045f, 0.148f), Th * 0.8f, Dark);
			P.LineSeg(Pt(X + 0.074f, 0.265f), Pt(X + 0.045f, 0.148f), Th * 0.8f, Dark);
			P.Circle(Pt(X + 0.045f, 0.148f), R.H * 0.009f, Hex(0xFF5A4A));
		}
		// 海岸公路
		{
			TArray<V2, TInlineAllocator<80>> A, B;
			for (int32 i = 0; i <= 60; ++i)
			{
				const float T = i / 60.0f;
				const float Y = 0.965f - 0.11f * T - 0.05f * FMath::Sin(T * 5.0f);
				A.Add(Pt(T, Y));
				B.Add(Pt(T, Y - 0.022f));
			}
			P.Terrain(A, R.B(), Hex(0x0A1830), Hex(0x060E1E));
			P.Stroke(A, R.H * 0.010f, Hex(0x7FD4FF, 0.85f), false);
			for (int32 i = 0; i + 1 < B.Num(); i += 4)
			{
				P.LineSeg(B[i], A[i], R.H * 0.005f, Hex(0x7FD4FF, 0.55f));
			}
			P.Stroke(B, R.H * 0.005f, Hex(0x7FD4FF, 0.55f), false);
		}
	}

	void MapArt(FApexPainter& P, const FApexRect& R, int32 MapIndex, float Bright)
	{
		if (const FSlateBrush* Brush = ThumbBrush(MapIndex))
		{
			// 等比裁切填满
			const float Src = 16.0f / 9.0f;
			const float Dst = R.W / FMath::Max(1.0f, R.H);
			FVector2f UV0(0, 0), UV1(1, 1);
			if (Dst < Src)
			{
				const float K = Dst / Src;
				UV0.X = 0.5f - K * 0.5f;
				UV1.X = 0.5f + K * 0.5f;
			}
			else
			{
				const float K = Src / Dst;
				UV0.Y = 0.5f - K * 0.5f;
				UV1.Y = 0.5f + K * 0.5f;
			}
			P.Image(R, Brush, FLinearColor::White, UV0, UV1);
		}
		else
		{
			const FName Id = FApexMapRegistry::GetMaps()[FApexMapRegistry::ClampIndex(MapIndex)].Id;
			if (Id == FName(TEXT("MomijiTouge"))) ArtMomiji(P, R);
			else if (Id == FName(TEXT("KuroshioCircuit"))) ArtKuroshio(P, R);
			else ArtFuji(P, R);
		}
		if (Bright < 0.995f)
		{
			P.Rect(R, Ink(1.0f - Bright));
		}
	}

	// =====================================================================
	// 小部件
	// =====================================================================

	float KeyCap(FApexPainter& P, float X, float CY, const FString& Label, float H, bool bDark)
	{
		const float Sz = H * 0.60f;
		const FSlateFontInfo F = Font(EApexFont::BodyBold, Sz);
		const float W = FMath::Max(H, Measure(Label, F).X + H * 0.60f);
		const FApexRect R(X, CY - H * 0.5f, W, H);
		P.RoundRect(FApexRect(R.X, R.Y + 2.0f, R.W, R.H), 5.0f, bDark ? Ink(0.35f) : InkDeep(0.55f));
		P.RoundRect(R, 5.0f, bDark ? Ink(0.92f) : Paper(0.92f));
		P.Text(Label, R.CX(), R.CY(), EApexFont::BodyBold, Sz, bDark ? Paper() : Ink(), EApexH::Center, EApexV::Middle);
		return W;
	}

	float KeyHint(FApexPainter& P, float X, float CY, const FString& Key, const FString& Label)
	{
		float Cur = X;
		TArray<FString> Keys;
		Key.ParseIntoArray(Keys, TEXT("|"));
		for (const FString& K : Keys)
		{
			Cur += KeyCap(P, Cur, CY, K, 24.0f) + 5.0f;
		}
		Cur += 3.0f;
		Cur += P.Text(Label, Cur, CY, EApexFont::Body, 16.0f, TextMid());
		return Cur - X;
	}

	float SectionLabel(FApexPainter& P, float X, float CY, const FString& No, const FString& Zh, const FString& En, const FLinearColor& Accent)
	{
		float Cur = X;
		if (!No.IsEmpty())
		{
			Cur += P.Text(No, Cur, CY, EApexFont::NumHeavy, 24.0f, Accent) + 10.0f;
		}
		Cur += P.Text(Zh, Cur, CY, EApexFont::Display, 21.0f, TextHi()) + 10.0f;
		if (!En.IsEmpty())
		{
			Cur += P.Text(En, Cur, CY + 1.0f, EApexFont::NumLight, 14.0f, TextLow(), EApexH::Left, EApexV::Middle, 120);
		}
		return Cur - X;
	}

	void Difficulty(FApexPainter& P, float X, float CY, int32 Level, const FLinearColor& On, const FLinearColor& Off, float W, float H, float Gap)
	{
		for (int32 i = 0; i < 5; ++i)
		{
			P.Skew(FApexRect(X + i * (W + Gap), CY - H * 0.5f, W, H), i < Level ? On : Off, 0.30f);
		}
	}

	void Seal(FApexPainter& P, const FApexRect& R, const FString& Ch, const FLinearColor& Bg, const FLinearColor& Fg)
	{
		P.RoundRect(R, R.W * 0.12f, Bg);
		P.StrokeRoundRect(R.Inset(R.W * 0.09f), R.W * 0.07f, FMath::Max(1.0f, R.W * 0.035f), Fade(Fg, 0.55f));
		P.Text(Ch, R.CX() - R.W * 0.01f, R.CY(), EApexFont::Display, R.H * 0.62f, Fg, EApexH::Center, EApexV::Middle);
	}

	// =====================================================================
	// 操作说明
	// =====================================================================

	namespace
	{
		struct FKeyDef
		{
			const TCHAR* Key;
			const TCHAR* Label; // nullptr = 未使用
		};

		void DrawKey(FApexPainter& P, const FApexRect& R, const FString& Key, const FString& Label, const FLinearColor& Accent, bool bStrong)
		{
			const bool bUsed = !Label.IsEmpty();
			if (bUsed)
			{
				P.RoundRect(FApexRect(R.X, R.Y + 4.0f, R.W, R.H), 8.0f, InkDeep(0.65f));
				P.RoundRect(R, 8.0f, bStrong ? Accent : Paper(0.95f));
				P.Text(Key, R.CX(), R.Y + R.H * 0.36f, EApexFont::BodyBold, Key.Len() > 2 ? 17.0f : 22.0f, Ink(), EApexH::Center);
				P.TextFit(Label, R.CX(), R.Y + R.H * 0.74f, EApexFont::BodyBold, 14.0f, R.W - 8.0f, Ink(0.80f), EApexH::Center);
			}
			else
			{
				P.RoundRect(R, 8.0f, Paper(0.07f));
				P.Text(Key, R.CX(), R.CY(), EApexFont::Body, 18.0f, Paper(0.26f), EApexH::Center);
			}
		}

		/** 俯视小车：车身朝向 Yaw（弧度，0=上），前轮转角 Steer */
		void MiniCar(FApexPainter& P, const V2& C, float Yaw, float Steer, float S, const FLinearColor& Body)
		{
			const V2 F(FMath::Sin(Yaw), -FMath::Cos(Yaw));
			const V2 Rt(-F.Y, F.X);
			auto At = [&](float X, float Y) { return C + Rt * (X * S) + F * (Y * S); };
			auto Wheel = [&](float X, float Y, float A)
			{
				const V2 Wf(FMath::Sin(Yaw + A), -FMath::Cos(Yaw + A));
				const V2 Wr(-Wf.Y, Wf.X);
				const V2 O = At(X, Y);
				const V2 Q[4] = { O + Wf * 7.0f * S + Wr * 2.8f * S, O + Wf * 7.0f * S - Wr * 2.8f * S, O - Wf * 7.0f * S - Wr * 2.8f * S, O - Wf * 7.0f * S + Wr * 2.8f * S };
				P.Poly(MakeArrayView(Q, 4), InkDeep());
			};
			Wheel(-11.5f, 15.0f, Steer);
			Wheel(11.5f, 15.0f, Steer);
			Wheel(-11.5f, -15.0f, 0.0f);
			Wheel(11.5f, -15.0f, 0.0f);
			const V2 B[8] = { At(-8, 27), At(8, 27), At(11.5f, 21), At(11.5f, -22), At(9, -27), At(-9, -27), At(-11.5f, -22), At(-11.5f, 21) };
			P.Poly(MakeArrayView(B, 8), Body);
			const V2 G[4] = { At(-8, 10), At(8, 10), At(7, 2), At(-7, 2) };
			P.Poly(MakeArrayView(G, 4), Ink(0.82f));
			const V2 G2[4] = { At(-7, -11), At(7, -11), At(8, -17), At(-8, -17) };
			P.Poly(MakeArrayView(G2, 4), Ink(0.82f));
		}

		void ArrowHead(FApexPainter& P, const V2& Tip, const V2& D, float S, const FLinearColor& C)
		{
			const V2 N(-D.Y, D.X);
			const V2 T[3] = { Tip, Tip - D * S + N * S * 0.55f, Tip - D * S - N * S * 0.55f };
			P.Poly(MakeArrayView(T, 3), C);
		}

		void DrawGamepad(FApexPainter& P, float X, float Y, const FLinearColor& Accent)
		{
			// 手柄主体，设计区域 620 x 372
			const V2 C(X + 310.0f, Y + 196.0f);
			auto At = [&](float PX, float PY) { return C + V2(PX, PY); };
			const FLinearColor Body = Hex(0x2B2B31);
			// 轮廓：对控制点做闭合 Catmull-Rom 平滑，星形填充后描边
			{
				const V2 Ctl[] = {
					V2(-70, -80), V2(0, -78), V2(70, -80), V2(128, -80), V2(166, -54), V2(188, 10), V2(202, 84), V2(194, 140), V2(166, 158), V2(136, 136),
					V2(104, 98), V2(66, 76), V2(0, 74), V2(-66, 76), V2(-104, 98), V2(-136, 136), V2(-166, 158), V2(-194, 140), V2(-202, 84), V2(-188, 10), V2(-166, -54), V2(-128, -80) };
				const int32 NC = UE_ARRAY_COUNT(Ctl);
				TArray<V2, TInlineAllocator<160>> Curve;
				for (int32 i = 0; i < NC; ++i)
				{
					const V2 P0 = Ctl[(i + NC - 1) % NC], P1 = Ctl[i], P2 = Ctl[(i + 1) % NC], P3 = Ctl[(i + 2) % NC];
					for (int32 k = 0; k < 6; ++k)
					{
						const float t = k / 6.0f, t2 = t * t, t3 = t2 * t;
						Curve.Add(C + (P1 * 2.0f + (P2 - P0) * t + (P0 * 2.0f - P1 * 5.0f + P2 * 4.0f - P3) * t2 + (P1 * 3.0f - P0 - P2 * 3.0f + P3) * t3) * 0.5f);
					}
				}
				P.FillStar(At(0, -60), Curve, Body);
				P.Stroke(Curve, 3.0f, Paper(0.62f), true);
			}
			// 扳机与肩键
			P.RoundRect(FApexRect(C.X - 158, C.Y - 132, 70, 30), 9.0f, Accent);
			P.RoundRect(FApexRect(C.X + 88, C.Y - 132, 70, 30), 9.0f, Accent);
			P.RoundRect(FApexRect(C.X - 162, C.Y - 98, 80, 16), 7.0f, Paper(0.92f));
			P.RoundRect(FApexRect(C.X + 82, C.Y - 98, 80, 16), 7.0f, Paper(0.92f));
			P.Text(TEXT("LT"), C.X - 123, C.Y - 117, EApexFont::BodyBold, 15, Ink(), EApexH::Center);
			P.Text(TEXT("RT"), C.X + 123, C.Y - 117, EApexFont::BodyBold, 15, Ink(), EApexH::Center);
			// 左摇杆
			P.Circle(At(-112, -18), 34, InkDeep());
			P.Ring(At(-112, -18), 30, 3.0f, Accent);
			P.Circle(At(-112, -18), 18, Hex(0x4A4A52));
			ArrowHead(P, At(-112 + 27, -18), V2(1, 0), 8, Paper());
			ArrowHead(P, At(-112 - 27, -18), V2(-1, 0), 8, Paper());
			// 十字键
			P.RoundRect(FApexRect(C.X - 66 - 10, C.Y + 34 - 28, 20, 56), 4, InkDeep());
			P.RoundRect(FApexRect(C.X - 66 - 28, C.Y + 34 - 10, 56, 20), 4, InkDeep());
			P.RoundRect(FApexRect(C.X - 66 - 9, C.Y + 34 - 27, 18, 20), 4, Paper(0.92f));
			// 右摇杆
			P.Circle(At(62, 34), 31, InkDeep());
			P.Ring(At(62, 34), 27, 2.5f, Paper(0.6f));
			P.Circle(At(62, 34), 16, Hex(0x4A4A52));
			// ABXY
			auto Btn = [&](float PX, float PY, const TCHAR* T, bool bOn)
			{
				P.Circle(At(PX, PY), 15, bOn ? Accent : InkDeep());
				P.Text(T, C.X + PX, C.Y + PY, EApexFont::BodyBold, 16, bOn ? Ink() : Paper(0.6f), EApexH::Center);
			};
			Btn(118, 12, TEXT("A"), true);
			Btn(148, -18, TEXT("B"), false);
			Btn(88, -18, TEXT("X"), true);
			Btn(118, -48, TEXT("Y"), true);
			// 菜单键
			P.RoundRect(FApexRect(C.X - 38, C.Y - 34, 24, 13), 6, Paper(0.92f));
			P.RoundRect(FApexRect(C.X + 14, C.Y - 34, 24, 13), 6, Paper(0.92f));

			// 引线标注
			auto Lead = [&](std::initializer_list<V2> Pts, const FString& Zh, bool bRight)
			{
				TArray<V2, TInlineAllocator<6>> A;
				for (const V2& Pt : Pts) A.Add(C + Pt);
				P.Stroke(A, 1.5f, Paper(0.42f), false);
				P.Circle(A[0], 3.5f, Accent);
				const V2 E = A.Last();
				P.Text(Zh, E.X + (bRight ? 8 : -8), E.Y, EApexFont::BodyBold, 16, TextHi(), bRight ? EApexH::Left : EApexH::Right);
			};
			Lead({ V2(-123, -132), V2(-150, -160), V2(-206, -160) }, TEXT("刹车 / 倒车"), false);
			Lead({ V2(-162, -90), V2(-206, -104) }, TEXT("LB 降挡"), false);
			Lead({ V2(-146, -18), V2(-206, -40) }, TEXT("转向"), false);
			Lead({ V2(-66, 10), V2(-66, 10), V2(-214, 22) }, TEXT("↑ 车灯"), false);
			Lead({ V2(-26, -34), V2(-40, -160), V2(-74, -180) }, TEXT("复位到赛道"), false);
			Lead({ V2(123, -132), V2(150, -160), V2(206, -160) }, TEXT("油门"), true);
			Lead({ V2(162, -90), V2(206, -116) }, TEXT("RB 升挡"), true);
			Lead({ V2(133, -48), V2(206, -72) }, TEXT("Y 切换视角"), true);
			Lead({ V2(133, 12), V2(206, -14) }, TEXT("A / X 手刹"), true);
			Lead({ V2(62, 64), V2(84, 104), V2(84, 176) }, TEXT("右摇杆按下 后视"), true);
			Lead({ V2(26, -34), V2(40, -160), V2(58, -180) }, TEXT("暂停"), true);
		}

		void DriftStep(FApexPainter& P, const FApexRect& R, int32 Step, const FLinearColor& Accent, float Time)
		{
			P.RoundRect(R, 10.0f, Paper(0.06f));
			const FApexRect Pic(R.X + 16, R.Y + 16, 228, R.H - 32);
			P.RoundRect(Pic, 8.0f, Hex(0x1B1C21));

			// 弯道：从左下进入，向右出弯
			const V2 Pivot(Pic.X + 186.0f, Pic.Y + Pic.H - 16.0f);
			const float Rm = 112.0f;
			P.PushClip(Pic);
			P.Arc(Pivot, Rm, 74.0f, PI * 0.98f, PI * 1.56f, Hex(0x3A3B43));
			P.Arc(Pivot, Rm - 37.0f, 4.0f, PI * 0.98f, PI * 1.56f, Accent);
			P.Arc(Pivot, Rm + 37.0f, 3.0f, PI * 0.98f, PI * 1.56f, Paper(0.85f));
			for (int32 i = 0; i < 7; ++i)
			{
				const float A0 = PI * (1.0f + i * 0.082f);
				P.Arc(Pivot, Rm, 2.5f, A0, A0 + 0.10f, Paper(0.35f));
			}
			auto OnArc = [&](float A, float Rr) { return Pivot + Dir(A) * Rr; };
			const float Pulse = 0.5f + 0.5f * FMath::Sin(Time * 4.0f);
			if (Step == 0)
			{
				const float A = PI * 1.10f;
				MiniCar(P, OnArc(A, Rm + 6), A - PI + 0.06f, 0.55f, 1.0f, Paper());
				// 转向箭头
				const V2 C0 = OnArc(A, Rm + 6);
				P.Arc(C0 + V2(34, -10), 26, 4.0f, -PI * 0.95f, -PI * 0.25f, WithA(Accent, 0.6f + 0.4f * Pulse));
				ArrowHead(P, C0 + V2(34, -10) + Dir(-PI * 0.25f) * 26.0f + V2(4, 6), SafeNormal2(V2(0.62f, 0.78f)), 13.0f, Accent);
			}
			else if (Step == 1)
			{
				const float A = PI * 1.22f;
				const V2 C0 = OnArc(A, Rm + 4);
				P.Arc(Pivot, Rm + 16, 4.0f, PI * 1.02f, A - 0.14f, InkDeep(0.85f));
				P.Arc(Pivot, Rm - 4, 4.0f, PI * 1.02f, A - 0.12f, InkDeep(0.85f));
				MiniCar(P, C0, A - PI + 0.50f, 0.20f, 1.0f, Paper());
				P.RoundRect(FApexRect(Pic.X + 16, Pic.Y + 14, 92, 30), 6.0f, WithA(Accent, 0.55f + 0.45f * Pulse));
				P.Text(TEXT("空格"), Pic.X + 62, Pic.Y + 29, EApexFont::BodyBold, 17, Ink(), EApexH::Center);
			}
			else
			{
				const float A = PI * 1.34f;
				const V2 C0 = OnArc(A, Rm + 2);
				P.Arc(Pivot, Rm + 18, 5.0f, PI * 1.04f, A - 0.16f, InkDeep(0.9f));
				P.Arc(Pivot, Rm - 6, 5.0f, PI * 1.04f, A - 0.12f, InkDeep(0.9f));
				for (int32 i = 0; i < 5; ++i)
				{
					const float T = FMath::Frac(Time * 0.7f + i * 0.2f);
					P.Circle(OnArc(A - 0.24f - T * 0.34f, Rm + 16 + T * 14.0f), 5.0f + T * 12.0f, Paper(0.22f * (1.0f - T)));
				}
				MiniCar(P, C0, A - PI + 0.72f, -0.60f, 1.0f, Paper());
				// 继续打向弯心的箭头
				P.Arc(C0 + V2(8, -34), 24, 4.0f, -PI * 0.80f, -PI * 0.10f, WithA(Accent, 0.6f + 0.4f * Pulse));
				ArrowHead(P, C0 + V2(8, -34) + Dir(-PI * 0.10f) * 24.0f + V2(2, 8), SafeNormal2(V2(0.30f, 0.95f)), 13.0f, Accent);
				P.RoundRect(FApexRect(Pic.X + 16, Pic.Y + 14, 92, 30), 6.0f, Paper(0.92f));
				P.Text(TEXT("油门 W"), Pic.X + 62, Pic.Y + 29, EApexFont::BodyBold, 17, Ink(), EApexH::Center);
			}
			P.PopClip();

			static const TCHAR* Titles[3] = { TEXT("入弯前先打方向"), TEXT("点一下手刹"), TEXT("油门稳住，方向控角") };
			static const TCHAR* Bodies[3] = {
				TEXT("车速 70 以上，提前收一点油门，把方向打向弯心，让车头先指进去。"),
				TEXT("短按空格（手柄 A）约半秒，后轮锁死，车尾就会甩出去。别一直按住。"),
				TEXT("踩住油门、方向继续打向弯心就能一直漂；松方向或反打马上回正。连漂：反方向打，再点一下手刹。") };
			const float TX = Pic.R() + 24.0f;
			P.Text(FString::FromInt(Step + 1), TX, R.Y + 46.0f, EApexFont::NumHeavy, 52.0f, Accent);
			P.Text(Titles[Step], TX + 38.0f, R.Y + 46.0f, EApexFont::Display, 28.0f, TextHi());
			P.TextWrap(Bodies[Step], TX, R.Y + 96.0f, R.R() - TX - 20.0f, EApexFont::Body, 18.0f, 30.0f, TextMid(), 4);
		}
	}

	void ControlsGuide(FApexPainter& P, float X, float Y, const FLinearColor& Accent, float Time)
	{
		// ---- 键盘 ----
		SectionLabel(P, X, Y + 12, TEXT(""), TEXT("键盘"), TEXT("KEYBOARD"), Accent);
		constexpr float U = 66.0f, G = 8.0f;
		const float KX = X, KY = Y + 44;
		struct FRowDef { float Indent; TArray<FKeyDef> Keys; };
		static const TArray<FRowDef> Rows = {
			{ 0.0f, { { TEXT("Esc"), TEXT("暂停") } } },
			{ 0.0f, { { TEXT("Q"), TEXT("降挡") }, { TEXT("W"), TEXT("油门") }, { TEXT("E"), TEXT("升挡") }, { TEXT("R"), TEXT("复位") }, { TEXT("T"), nullptr }, { TEXT("Y"), nullptr }, { TEXT("U"), nullptr }, { TEXT("I"), nullptr }, { TEXT("O"), nullptr }, { TEXT("P"), TEXT("暂停") } } },
			{ 22.0f, { { TEXT("A"), TEXT("左转") }, { TEXT("S"), TEXT("刹车") }, { TEXT("D"), TEXT("右转") }, { TEXT("F"), TEXT("自动") }, { TEXT("G"), nullptr }, { TEXT("H"), TEXT("车灯") }, { TEXT("J"), nullptr }, { TEXT("K"), nullptr }, { TEXT("L"), nullptr } } },
			{ 52.0f, { { TEXT("Z"), nullptr }, { TEXT("X"), nullptr }, { TEXT("C"), TEXT("视角") }, { TEXT("V"), TEXT("运镜") }, { TEXT("B"), TEXT("后视") }, { TEXT("N"), nullptr }, { TEXT("M"), nullptr } } },
		};
		for (int32 r = 0; r < Rows.Num(); ++r)
		{
			float CX = KX + Rows[r].Indent;
			for (const FKeyDef& K : Rows[r].Keys)
			{
				const FString Key = K.Key;
				const bool bStrong = (Key == TEXT("W") || Key == TEXT("A") || Key == TEXT("S") || Key == TEXT("D"));
				DrawKey(P, FApexRect(CX, KY + r * (U + G), U, U), Key, K.Label ? FString(K.Label) : FString(), Accent, bStrong);
				CX += U + G;
			}
		}
		// 空格
		DrawKey(P, FApexRect(KX + 2 * (U + G) + 8, KY + 4 * (U + G), 4 * U + 3 * G, U), TEXT("空格 Space"), TEXT("手刹（起漂）"), Accent, true);
		// 方向键
		{
			const float AX = KX + 10 * (U + G) + 34, AY = KY + 2 * (U + G);
			auto ArrowKey = [&](float PX, float PY, const V2& D, const TCHAR* Label)
			{
				const FApexRect R(PX, PY, U, U);
				P.RoundRect(FApexRect(R.X, R.Y + 4, R.W, R.H), 8.0f, InkDeep(0.65f));
				P.RoundRect(R, 8.0f, Accent);
				ArrowHead(P, V2(R.CX(), R.Y + R.H * 0.34f) + D * 9.0f, D, 18.0f, Ink());
				P.Text(Label, R.CX(), R.Y + R.H * 0.76f, EApexFont::BodyBold, 14.0f, Ink(0.80f), EApexH::Center);
			};
			ArrowKey(AX + U + G, AY, V2(0, -1), TEXT("油门"));
			ArrowKey(AX, AY + U + G, V2(-1, 0), TEXT("左转"));
			ArrowKey(AX + U + G, AY + U + G, V2(0, 1), TEXT("刹车"));
			ArrowKey(AX + 2 * (U + G), AY + U + G, V2(1, 0), TEXT("右转"));
			P.Text(TEXT("方向键与 WASD 等效"), AX + 1.5f * U + G, AY + 2 * (U + G) + 22, EApexFont::Body, 15.0f, TextLow(), EApexH::Center);
		}

		// ---- 手柄 ----
		const float PX = X + 1066.0f;
		SectionLabel(P, PX, Y + 12, TEXT(""), TEXT("手柄"), TEXT("GAMEPAD"), Accent);
		P.Rect(FApexRect(PX - 32.0f, Y + 4, 1.0f, 420.0f), Line(0.12f));
		DrawGamepad(P, PX - 6.0f, Y + 40.0f, Accent);

		// ---- 三步漂移 ----
		const float SY = Y + 476.0f;
		SectionLabel(P, X, SY + 12, TEXT(""), TEXT("三步学会漂移"), TEXT("HOW TO DRIFT"), Accent);
		constexpr float CardW = (GuideW - 48.0f) / 3.0f;
		for (int32 i = 0; i < 3; ++i)
		{
			DriftStep(P, FApexRect(X + i * (CardW + 24.0f), SY + 44.0f, CardW, 248.0f), i, Accent, Time);
		}
	}
}

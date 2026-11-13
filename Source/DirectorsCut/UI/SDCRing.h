// Director's Cut — circular progress ring (success odds, mission timers) (Agent C).
#pragma once

#include "CoreMinimal.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Rendering/DrawElements.h"
#include "Brushes/SlateRoundedBoxBrush.h"

class SDCRing : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SDCRing)
		: _Size(48.f), _Thickness(4.f), _Fill(0.f), _Color(FLinearColor::White), _Track(FLinearColor(1, 1, 1, 0.08f)), _bShowTrack(true)
		{}
		SLATE_ARGUMENT(float, Size)
		SLATE_ARGUMENT(float, Thickness)
		SLATE_ATTRIBUTE(float, Fill)          // 0..1, clockwise from 12 o'clock
		SLATE_ATTRIBUTE(FLinearColor, Color)
		SLATE_ARGUMENT(FLinearColor, Track)
		SLATE_ARGUMENT(bool, bShowTrack)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		Size = InArgs._Size; Thickness = InArgs._Thickness; Fill = InArgs._Fill; Color = InArgs._Color; Track = InArgs._Track; bShowTrack = InArgs._bShowTrack;
	}

	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(Size, Size); }

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geo, const FSlateRect& Clip, FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const override
	{
		const FVector2f L = Geo.GetLocalSize();
		const FVector2f C = L * 0.5f;
		const float R = FMath::Min(L.X, L.Y) * 0.5f - Thickness * 0.5f - 0.5f;
		const float Op = Style.GetColorAndOpacityTint().A;
		auto Arc = [&](float From, float To, const FLinearColor& Col, int32 InLayer)
		{
			if (To - From <= 0.0005f) { return; }
			const int32 N = FMath::Max(4, FMath::CeilToInt((To - From) * 96.f));
			TArray<FVector2f> P; P.Reserve(N + 1);
			for (int32 i = 0; i <= N; ++i)
			{
				const float A = (From + (To - From) * (float)i / (float)N) * 2.f * PI - PI * 0.5f;
				P.Add(C + FVector2f(FMath::Cos(A), FMath::Sin(A)) * R);
			}
			FLinearColor Tint = Col; Tint.A *= Op;
			FSlateDrawElement::MakeLines(Out, InLayer, Geo.ToPaintGeometry(), MoveTemp(P), ESlateDrawEffect::None, Tint, true, Thickness);
		};
		if (bShowTrack) { Arc(0.f, 1.f, Track, Layer); }
		Arc(0.f, FMath::Clamp(Fill.Get(), 0.f, 1.f), Color.Get(), Layer + 1);
		return Layer + 1;
	}

private:
	float Size = 48.f;
	float Thickness = 4.f;
	TAttribute<float> Fill;
	TAttribute<FLinearColor> Color;
	FLinearColor Track;
	bool bShowTrack = true;
};

/** Report roll bar: track, success zone (0..Chance), and a marker that slides to the roll. */
class SDCRollBar : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SDCRollBar) : _Height(10.f), _Chance(0.5f), _Roll(0.f), _ZoneColor(FLinearColor::Green), _MarkerColor(FLinearColor::White) {}
		SLATE_ARGUMENT(float, Height)
		SLATE_ARGUMENT(float, Chance)
		SLATE_ATTRIBUTE(float, Roll)            // animated 0..1 (marker position)
		SLATE_ARGUMENT(FLinearColor, ZoneColor)
		SLATE_ATTRIBUTE(FLinearColor, MarkerColor)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		Height = InArgs._Height; Chance = InArgs._Chance; Roll = InArgs._Roll; ZoneColor = InArgs._ZoneColor; MarkerColor = InArgs._MarkerColor;
		Track = FSlateRoundedBoxBrush(FLinearColor::White, Height * 0.5f);
	}
	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(100.f, Height + 10.f); }
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geo, const FSlateRect& Clip, FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const override
	{
		const FVector2f L = Geo.GetLocalSize();
		const float Op = Style.GetColorAndOpacityTint().A;
		const float Y = (L.Y - Height) * 0.5f;
		auto Rect = [&](float X0, float W, float H, float Y0, const FSlateBrush* B, FLinearColor C, int32 InLayer)
		{
			if (W <= 0.f) { return; }
			C.A *= Op;
			FSlateDrawElement::MakeBox(Out, InLayer, Geo.ToPaintGeometry(FVector2f(W, H), FSlateLayoutTransform(FVector2f(X0, Y0))), B, ESlateDrawEffect::None, C);
		};
		Rect(0.f, L.X, Height, Y, &Track, FLinearColor(0.96f, 0.25f, 0.37f, 0.16f), Layer);
		Rect(0.f, L.X * FMath::Clamp(Chance, 0.f, 1.f), Height, Y, &Track, ZoneColor, Layer + 1);
		const float MX = FMath::Clamp(Roll.Get(), 0.f, 1.f) * L.X;
		Rect(MX - 1.5f, 3.f, L.Y, 0.f, &Marker, MarkerColor.Get(), Layer + 2);
		return Layer + 2;
	}
private:
	float Height = 10.f;
	float Chance = 0.5f;
	TAttribute<float> Roll;
	FLinearColor ZoneColor;
	TAttribute<FLinearColor> MarkerColor;
	FSlateRoundedBoxBrush Track = FSlateRoundedBoxBrush(FLinearColor::White, 4.f);
	FSlateRoundedBoxBrush Marker = FSlateRoundedBoxBrush(FLinearColor::White, 1.5f);
};

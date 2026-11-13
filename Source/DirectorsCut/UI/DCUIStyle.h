// Director's Cut — art-deco noir UI style set + small Slate widget helpers (Agent C).
#pragma once

#include "CoreMinimal.h"
#include "Fonts/SlateFontInfo.h"
#include "Styling/SlateBrush.h"
#include "Styling/SlateTypes.h"
#include "Widgets/SWidget.h"
#include "Core/DCTypes.h"

class SWidget;

namespace DCUI
{
	// ── Palette (linear) ────────────────────────────────────────────────────
	FLinearColor Rgb(uint8 R, uint8 G, uint8 B, uint8 A = 255);
	extern const FLinearColor Ink;        // near-black ink on paper
	extern const FLinearColor InkSoft;    // secondary ink
	extern const FLinearColor Paper;      // cream card
	extern const FLinearColor PaperDark;  // shaded paper
	extern const FLinearColor Night;      // panel background
	extern const FLinearColor NightLight; // raised dark surface
	extern const FLinearColor Gold;
	extern const FLinearColor GoldDim;
	extern const FLinearColor Cream;      // text on dark
	extern const FLinearColor Muted;      // secondary text on dark
	extern const FLinearColor Red;        // stamps / warnings
	extern const FLinearColor RedBright;
	extern const FLinearColor Green;
	extern const FLinearColor GreenBright;

	inline FLinearColor WithAlpha(FLinearColor Col, float A) { Col.A = A; return Col; }
	FLinearColor TypeCol(EDCCriminalType T); // DC::TypeColor, slightly toned for paper

	// ── Fonts ────────────────────────────────────────────────────────────────
	FSlateFontInfo Display(float Size); // Limelight (deco titles)
	FSlateFontInfo Type(float Size);    // Special Elite (typewriter)
	FSlateFontInfo Sans(float Size);    // Oswald (labels, numbers)
	FSlateFontInfo Big(float Size);     // Bebas Neue (big numbers, stamps)

	// ── Brushes / styles ────────────────────────────────────────────────────
	enum class EBrush : uint8
	{
		White,          // plain white (tint with ColorAndOpacity)
		PanelDark,      // night fill + gold hairline
		PanelInner,     // night-light fill + dim gold hairline
		PaperCard,      // cream + dim gold hairline
		PaperCardHover, // cream + bright gold outline
		PaperShade,     // darker paper inset
		Chip,           // rounded pill (tint)
		Outline,        // transparent + gold hairline
		StampOutline,   // transparent + thick red outline
		Dim,            // full screen dim
		Round,          // rounded fill for bars (tint)
		MAX
	};
	const FSlateBrush* Brush(EBrush B);

	enum class EBtn : uint8 { Gold, Red, Ghost, Paper, Tab, TabActive, MAX };
	const FButtonStyle& ButtonStyle(EBtn B);
	const FSliderStyle& SliderStyle();
	const FCheckBoxStyle& CheckStyle();
	const FScrollBarStyle& ScrollBarStyle();

	// ── Audio / context ─────────────────────────────────────────────────────
	void SetContext(UObject* WorldContext);
	UObject* GetContext();
	void Sfx(FName Name, float Volume = 1.f);

	// ── Formatting ──────────────────────────────────────────────────────────
	FString Money(int64 V);          // $25,000 / -$1,200
	FString PctStr(float V01);          // 42%
	FString SignedPctStr(float V01);    // +4% / -3%
	FString Roman(int32 V);          // tier numerals
	FString GearSummary(const FDCGearItem& G); // "+10 Finesse, -5 Stealth, +2 Luck"

	// ── Widget helpers ──────────────────────────────────────────────────────
	TSharedRef<SWidget> Txt(const FString& S, const FSlateFontInfo& Font, const FLinearColor& Col, bool bWrap = false, ETextJustify::Type Justify = ETextJustify::Left);
	TSharedRef<SWidget> TxtA(TAttribute<FText> S, const FSlateFontInfo& Font, TAttribute<FSlateColor> Col, ETextJustify::Type Justify = ETextJustify::Left);
	/** Button with text label. OnClick plays the Click sfx. */
	TSharedRef<SWidget> Button(const FString& Label, TFunction<void()> OnClick, EBtn Kind = EBtn::Gold, TAttribute<bool> Enabled = true, float FontSize = 18.f, const FString& Tooltip = FString());
	TSharedRef<SWidget> ButtonContent(TSharedRef<SWidget> Content, TFunction<void()> OnClick, EBtn Kind = EBtn::Paper, TAttribute<bool> Enabled = true, const FString& Tooltip = FString());
	TSharedRef<SWidget> Chip(const FString& Label, const FLinearColor& Col, float FontSize = 14.f);
	TSharedRef<SWidget> TypeChip(EDCCriminalType T, float FontSize = 14.f);
	TSharedRef<SWidget> HLine(const FLinearColor& Col, float Thickness = 1.f);
	TSharedRef<SWidget> VLine(const FLinearColor& Col, float Thickness = 1.f);
	TSharedRef<SWidget> Bar(TAttribute<float> Fill01, const FLinearColor& Fg, const FLinearColor& Bg, float Height = 8.f);
	/** Stat line: label, value number, bar 0..100, optional delta (gear). */
	TSharedRef<SWidget> StatRow(const FString& Label, int32 Value, int32 Delta, const FLinearColor& Col, bool bOnPaper, float LabelWidth = 70.f);
	/** Section header: gold deco caps with hairlines. */
	TSharedRef<SWidget> Header(const FString& S, bool bOnPaper = false, float Size = 20.f);
	/** Paper card. */
	TSharedRef<SWidget> PaperCard(TSharedRef<SWidget> Content, FMargin Pad = FMargin(14.f));
	/** Dark inset. */
	TSharedRef<SWidget> DarkCard(TSharedRef<SWidget> Content, FMargin Pad = FMargin(12.f));
	/** Deco corner ornament frame around a big panel (double gold hairline). */
	TSharedRef<SWidget> DecoFrame(TSharedRef<SWidget> Content, FMargin Pad = FMargin(22.f));
	/** Rotated stamp text with red outline. */
	TSharedRef<SWidget> Stamp(const FString& S, const FLinearColor& Col, float Size = 40.f, float AngleDeg = -8.f);
	/** "Label ........ Value" row. */
	TSharedRef<SWidget> KeyValue(const FString& K, const FString& V, const FLinearColor& VCol, bool bOnPaper = false, float Size = 17.f);
	/** Deco diamond pips (levels, luck). */
	TSharedRef<SWidget> Pips(int32 Filled, int32 Max, const FLinearColor& On, const FLinearColor& Off, float Size = 10.f);
	/** Row of film sprocket holes. */
	TSharedRef<SWidget> Sprockets(int32 Count = 40, float Size = 10.f);
}

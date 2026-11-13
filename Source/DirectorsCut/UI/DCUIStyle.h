// Director's Cut — v2 UI style: dark glass, one amber accent, Inter + JetBrains Mono + Lucide icons (Agent C).
//
// Everything is pure Slate and built at runtime. Fonts load from Content/Fonts (see LICENSES.md); icons are Lucide
// glyphs looked up by name in lucide_codepoints.json.
#pragma once

#include "CoreMinimal.h"
#include "Fonts/SlateFontInfo.h"
#include "Styling/SlateBrush.h"
#include "Styling/SlateTypes.h"
#include "Styling/SlateColor.h"
#include "Widgets/SWidget.h"
#include "Core/DCTypes.h"

class SToolTip;
class IToolTip;

namespace DCUI
{
	// ── Palette (linear) ────────────────────────────────────────────────────
	FLinearColor Hex(uint32 RGB, float Alpha = 1.f);
	inline FLinearColor A(FLinearColor C, float Alpha) { C.A = Alpha; return C; }

	extern const FLinearColor Glass;      // panel fill  #0B0D10 @ 92%
	extern const FLinearColor GlassSoft;  // HUD pill fill (a touch lighter, more transparent)
	extern const FLinearColor Card;       // raised surface on glass
	extern const FLinearColor CardHi;     // hovered surface
	extern const FLinearColor Inset;      // sunken wells, bar tracks
	extern const FLinearColor Line;       // 1 px hairline (white ~9%)
	extern const FLinearColor LineHi;     // hovered hairline
	extern const FLinearColor Text;       // primary text
	extern const FLinearColor TextDim;    // secondary text
	extern const FLinearColor TextFaint;  // tertiary / disabled
	extern const FLinearColor Accent;     // sodium amber #F5A524
	extern const FLinearColor AccentInk;  // text on accent
	extern const FLinearColor Good;       // success green
	extern const FLinearColor Bad;        // danger red
	extern const FLinearColor Warn;       // warning yellow
	extern const FLinearColor Info;       // calm blue

	FLinearColor ChanceColor(float Chance);       // red -> amber -> green by success chance
	FLinearColor OutcomeColor(EDCMissionOutcome O);
	FLinearColor TierColor(int32 Tier);

	// ── Fonts ────────────────────────────────────────────────────────────────
	enum class EW : uint8 { Regular, Medium, SemiBold, Bold };
	FSlateFontInfo Font(float Size, EW Weight = EW::Regular, int32 LetterSpacing = 0);
	FSlateFontInfo Mono(float Size, int32 LetterSpacing = 0);
	/** Small caps label font (Inter SemiBold, wide tracking). Use with ToUpper(). */
	FSlateFontInfo Caps(float Size = 8.5f);

	// ── Icons ────────────────────────────────────────────────────────────────
	/** The glyph string for a Lucide icon name ("circle-help" if unknown). */
	FString IconGlyph(FName LucideName);
	bool HasIcon(FName LucideName);
	FSlateFontInfo IconFont(float Size);
	TSharedRef<SWidget> Icon(FName LucideName, float Size, TAttribute<FSlateColor> Color = FSlateColor(FLinearColor::White));
	TSharedRef<SWidget> IconA(TAttribute<FName> LucideName, float Size, TAttribute<FSlateColor> Color);

	// ── Brushes ──────────────────────────────────────────────────────────────
	/** Cached rounded box. Radius < 0 = pill / circle (half height). */
	const FSlateBrush* Box(const FLinearColor& Fill, float Radius = 10.f, const FLinearColor& Outline = FLinearColor::Transparent, float OutlineWidth = 0.f);
	const FSlateBrush* WhiteBrush();
	const FSlateBrush* NoBrush();
	/** Vertical gradient-ish shade used behind the title/modal screens (left-to-right fade). */
	const FSlateBrush* ShadeLeft();

	// ── Widget styles ─────────────────────────────────────────────────────────
	enum class EBtn : uint8 { Primary, Secondary, Ghost, Danger, Card, CardSelected, Dock, DockActive, MAX };
	const FButtonStyle& ButtonStyle(EBtn Kind);
	const FSliderStyle& SliderStyle();
	const FScrollBarStyle& ScrollBarStyle();
	const FProgressBarStyle& ProgressStyle();

	// ── Context / audio ──────────────────────────────────────────────────────
	void SetContext(UObject* WorldContext);
	UObject* GetContext();
	void Sfx(FName Name, float Volume = 1.f);

	// ── Time / easing ────────────────────────────────────────────────────────
	double Now();
	float EaseOut(float X);
	float EaseInOut(float X);

	// ── Formatting ──────────────────────────────────────────────────────────
	FString Money(int64 V);           // $25,000 / -$1,200
	FString MoneyShort(int64 V);      // $25k / $1.2M
	FString Pct(float V01);           // 42%
	FString SignedPct(float V01);     // +4% / -3%
	FString HourMin(float HourOfDay); // 14:37

	// ── Widget helpers ──────────────────────────────────────────────────────
	TSharedRef<SWidget> Txt(const FString& S, const FSlateFontInfo& F, const FLinearColor& Col = Text, bool bWrap = false);
	TSharedRef<SWidget> TxtA(TAttribute<FText> S, const FSlateFontInfo& F, TAttribute<FSlateColor> Col);
	/** Small caps label ("YOUR CUT"). */
	TSharedRef<SWidget> Label(const FString& S, const FLinearColor& Col = TextFaint, float Size = 8.5f);
	/** Icon followed by text, vertically centred. */
	TSharedRef<SWidget> IconText(FName Icon, const FString& S, const FSlateFontInfo& F, const FLinearColor& IconCol, const FLinearColor& TextCol = Text, float IconSize = 0.f, float Gap = 6.f);
	TSharedRef<SWidget> IconTextA(FName Icon, TAttribute<FText> S, const FSlateFontInfo& F, TAttribute<FSlateColor> IconCol, TAttribute<FSlateColor> TextCol, float IconSize = 0.f, float Gap = 6.f);

	/** Styled tooltip: optional icon, title, body lines (muted), optional accent value. */
	TSharedRef<SToolTip> Tip(const FString& Title, const FString& Body = FString(), FName Icon = NAME_None, const FLinearColor& IconCol = Accent);
	TSharedRef<SToolTip> TipWidget(TSharedRef<SWidget> Content);

	/** Button. Plays Click; Hover plays a soft tick. Never takes keyboard focus (so Esc/Space keep reaching the game). */
	TSharedRef<SWidget> Button(TSharedRef<SWidget> Content, TFunction<void()> OnClick, EBtn Kind = EBtn::Secondary, TAttribute<bool> Enabled = true, TSharedPtr<IToolTip> ToolTip = nullptr, FMargin Pad = FMargin(14.f, 8.f));
	/** Icon + label button. */
	TSharedRef<SWidget> TextButton(FName Icon, const FString& LabelText, TFunction<void()> OnClick, EBtn Kind = EBtn::Secondary, TAttribute<bool> Enabled = true, TSharedPtr<IToolTip> ToolTip = nullptr, float Size = 11.f);
	/** Square icon-only button. */
	TSharedRef<SWidget> IconButton(FName Icon, TFunction<void()> OnClick, TSharedPtr<IToolTip> ToolTip = nullptr, EBtn Kind = EBtn::Ghost, float Size = 14.f, TAttribute<bool> Enabled = true);

	/** Glass panel (main panels). */
	TSharedRef<SWidget> GlassPanel(TSharedRef<SWidget> Content, FMargin Pad = FMargin(24.f), float Radius = 14.f);
	/** Raised card on glass. */
	TSharedRef<SWidget> CardBox(TSharedRef<SWidget> Content, FMargin Pad = FMargin(14.f), const FLinearColor& Outline = Line, float Radius = 10.f);
	/** Pill with tint fill. */
	TSharedRef<SWidget> Pill(TSharedRef<SWidget> Content, const FLinearColor& Fill, FMargin Pad = FMargin(8.f, 3.f), const FLinearColor& Outline = FLinearColor::Transparent);
	/** Tinted icon tile (square, rounded). */
	TSharedRef<SWidget> IconTile(FName Icon, float Size, const FLinearColor& Col, float IconScale = 0.5f, float FillAlpha = 0.14f);
	/** Count badge (red dot with a number), hidden at 0. */
	TSharedRef<SWidget> Badge(TAttribute<int32> Count, const FLinearColor& Col = Bad);

	TSharedRef<SWidget> HLine(const FLinearColor& Col = Line, float Thickness = 1.f);
	TSharedRef<SWidget> VLine(const FLinearColor& Col = Line, float Thickness = 1.f);
	/** Rounded progress bar. */
	TSharedRef<SWidget> Bar(TAttribute<float> Fill01, TAttribute<FSlateColor> Col, float Height = 4.f, const FLinearColor& Track = FLinearColor(1, 1, 1, 0.07f));
	/** Pips (tier, room level). */
	TSharedRef<SWidget> Pips(int32 Filled, int32 Max, const FLinearColor& On, float Size = 6.f, const FLinearColor& Off = FLinearColor(1, 1, 1, 0.12f));

	/** Crew avatar: type icon on a dark disc with a quality-coloured ring. Optional status overlay icon. */
	TSharedRef<SWidget> Avatar(const FDCCriminal& C, float Size, bool bDim = false);
	TSharedRef<SWidget> AvatarType(EDCCriminalType T, EDCQuality Q, float Size, bool bDim = false);
	/** Mood icon by willingness (smile / meh / frown / angry). */
	FName MoodIcon(float Willingness, bool bRefuses);
	FLinearColor MoodColor(float Willingness, bool bRefuses);
	/** Icon + tooltip for a plan block reason. */
	FName BlockIcon(EDCPlanBlock B);
	FString BlockText(EDCPlanBlock B);
}

#include "UI/DCUIStyle.h"
#include "Core/DCAudioSubsystem.h"
#include "Brushes/SlateColorBrush.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Fonts/CompositeFont.h"
#include "Styling/CoreStyle.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SSpacer.h"
#include "DirectorsCut.h"

namespace DCUI
{
	FLinearColor Rgb(uint8 R, uint8 G, uint8 B, uint8 A) { return FLinearColor::FromSRGBColor(FColor(R, G, B, A)); }

	const FLinearColor Ink        = Rgb(0x1d, 0x18, 0x14);
	const FLinearColor InkSoft    = Rgb(0x5a, 0x4e, 0x40);
	const FLinearColor Paper      = Rgb(0xee, 0xe4, 0xcc);
	const FLinearColor PaperDark  = Rgb(0xd8, 0xc9, 0xa6);
	const FLinearColor Night      = Rgb(0x15, 0x12, 0x10, 0xf2);
	const FLinearColor NightLight = Rgb(0x24, 0x1f, 0x1a, 0xff);
	const FLinearColor Gold       = Rgb(0xd4, 0xaf, 0x62);
	const FLinearColor GoldDim    = Rgb(0x8c, 0x72, 0x40);
	const FLinearColor Cream      = Rgb(0xf3, 0xe9, 0xd2);
	const FLinearColor Muted      = Rgb(0xa8, 0x9c, 0x88);
	const FLinearColor Red        = Rgb(0xb0, 0x1e, 0x1c);
	const FLinearColor RedBright  = Rgb(0xe2, 0x3b, 0x2e);
	const FLinearColor Green      = Rgb(0x2f, 0x6e, 0x34);
	const FLinearColor GreenBright= Rgb(0x6c, 0xc0, 0x5a);

	FLinearColor TypeCol(EDCCriminalType T)
	{
		// Contract colours are very saturated linear values; pull them slightly toward ink so they read on paper.
		const FLinearColor Base = DC::TypeColor(T);
		return FLinearColor::LerpUsingHSV(Base, Ink, 0.12f);
	}

	// ── Fonts ────────────────────────────────────────────────────────────────
	struct FFontSet
	{
		TSharedPtr<const FCompositeFont> Display, Type, Sans, Big;
		bool bInit = false;
	};
	static FFontSet& Fonts()
	{
		static FFontSet S;
		if (!S.bInit)
		{
			S.bInit = true;
			const FString Dir = FPaths::ProjectContentDir() / TEXT("Fonts");
			const FString Fallback = FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Regular.ttf");
			auto Make = [&](const TCHAR* File, const TCHAR* Name) -> TSharedPtr<const FCompositeFont>
			{
				const FString Path = Dir / File;
				if (!IFileManager::Get().FileExists(*Path))
				{
					UE_LOG(LogDC, Warning, TEXT("[UI] Font missing: %s (falling back to Roboto)"), *Path);
					return nullptr;
				}
				TSharedRef<FStandaloneCompositeFont> F = MakeShared<FStandaloneCompositeFont>(FName(Name), Path, EFontHinting::Default, EFontLoadingPolicy::LazyLoad);
				if (IFileManager::Get().FileExists(*Fallback))
				{
					F->FallbackTypeface.Typeface.AppendFont(TEXT("Fallback"), Fallback, EFontHinting::Default, EFontLoadingPolicy::LazyLoad);
				}
				return F;
			};
			S.Display = Make(TEXT("Limelight-Regular.ttf"), TEXT("DCDisplay"));
			S.Type    = Make(TEXT("SpecialElite-Regular.ttf"), TEXT("DCType"));
			S.Sans    = Make(TEXT("Oswald-Variable.ttf"), TEXT("DCSans"));
			S.Big     = Make(TEXT("BebasNeue-Regular.ttf"), TEXT("DCBig"));
		}
		return S;
	}
	static FSlateFontInfo MakeFontInfo(const TSharedPtr<const FCompositeFont>& F, float Size, const TCHAR* FallbackStyle)
	{
		if (F.IsValid()) { return FSlateFontInfo(F, Size); }
		return FCoreStyle::GetDefaultFontStyle(FallbackStyle, FMath::RoundToInt(Size));
	}
	FSlateFontInfo Display(float Size) { return MakeFontInfo(Fonts().Display, Size, TEXT("Bold")); }
	FSlateFontInfo Type(float Size)    { return MakeFontInfo(Fonts().Type, Size, TEXT("Regular")); }
	FSlateFontInfo Sans(float Size)    { return MakeFontInfo(Fonts().Sans, Size, TEXT("Regular")); }
	FSlateFontInfo Big(float Size)     { return MakeFontInfo(Fonts().Big, Size, TEXT("Bold")); }

	// ── Brushes ──────────────────────────────────────────────────────────────
	static FSlateBrush MakeBox(const FLinearColor& Fill, const FLinearColor& Outline, float Width, float Radius)
	{
		FSlateRoundedBoxBrush B(Fill, Radius);
		B.OutlineSettings.Color = FSlateColor(Outline);
		B.OutlineSettings.Width = Width;
		B.OutlineSettings.CornerRadii = FVector4(Radius, Radius, Radius, Radius);
		B.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		return B;
	}

	struct FStyleSet
	{
		FSlateBrush Brushes[(int32)EBrush::MAX];
		FButtonStyle Buttons[(int32)EBtn::MAX];
		FSliderStyle Slider;
		FCheckBoxStyle Check;
		FScrollBarStyle Scroll;
		FStyleSet()
		{
			Brushes[(int32)EBrush::White]          = FSlateColorBrush(FLinearColor::White);
			Brushes[(int32)EBrush::PanelDark]      = MakeBox(Night, Gold, 1.f, 2.f);
			Brushes[(int32)EBrush::PanelInner]     = MakeBox(NightLight, GoldDim, 1.f, 2.f);
			Brushes[(int32)EBrush::PaperCard]      = MakeBox(Paper, GoldDim, 1.f, 3.f);
			Brushes[(int32)EBrush::PaperCardHover] = MakeBox(Paper, Gold, 2.f, 3.f);
			Brushes[(int32)EBrush::PaperShade]     = MakeBox(PaperDark, WithAlpha(GoldDim, 0.6f), 1.f, 2.f);
			Brushes[(int32)EBrush::Chip]           = FSlateRoundedBoxBrush(FLinearColor::White, 9.f);
			Brushes[(int32)EBrush::Outline]        = MakeBox(FLinearColor::Transparent, Gold, 1.f, 0.f);
			Brushes[(int32)EBrush::StampOutline]   = MakeBox(FLinearColor::Transparent, FLinearColor::White, 4.f, 4.f);
			Brushes[(int32)EBrush::Dim]            = FSlateColorBrush(Rgb(0x05, 0x04, 0x03, 0xd8));
			Brushes[(int32)EBrush::Round]          = FSlateRoundedBoxBrush(FLinearColor::White, 3.f);

			auto MakeBtn = [](const FLinearColor& Fill, const FLinearColor& Hover, const FLinearColor& Outline, const FLinearColor& Fg, const FLinearColor& HoverFg)
			{
				FButtonStyle S;
				S.SetNormal(MakeBox(Fill, Outline, 1.f, 2.f));
				S.SetHovered(MakeBox(Hover, Gold, 1.5f, 2.f));
				S.SetPressed(MakeBox(Hover * 0.8f, Gold, 2.f, 2.f));
				S.SetDisabled(MakeBox(WithAlpha(Fill, Fill.A * 0.35f), WithAlpha(Outline, 0.3f), 1.f, 2.f));
				S.SetNormalForeground(Fg);
				S.SetHoveredForeground(HoverFg);
				S.SetPressedForeground(HoverFg);
				S.SetDisabledForeground(WithAlpha(Fg, 0.4f));
				S.SetNormalPadding(FMargin(14.f, 6.f, 14.f, 7.f));
				S.SetPressedPadding(FMargin(14.f, 7.f, 14.f, 6.f));
				return S;
			};
			Buttons[(int32)EBtn::Gold]  = MakeBtn(Rgb(0x2a, 0x22, 0x18), Rgb(0x4a, 0x3b, 0x22), Gold, Gold, Cream);
			Buttons[(int32)EBtn::Red]   = MakeBtn(Rgb(0x8e, 0x18, 0x16), Rgb(0xb8, 0x24, 0x1f), Rgb(0xe8, 0xb0, 0x60), Cream, FLinearColor::White);
			Buttons[(int32)EBtn::Ghost] = MakeBtn(WithAlpha(Night, 0.f), WithAlpha(Gold, 0.15f), WithAlpha(GoldDim, 0.7f), Muted, Cream);
			Buttons[(int32)EBtn::Paper] = MakeBtn(Paper, Rgb(0xf8, 0xf0, 0xdc), GoldDim, Ink, Ink);
			Buttons[(int32)EBtn::Paper].SetNormalPadding(FMargin(0.f)).SetPressedPadding(FMargin(0.f, 1.f, 0.f, -1.f));
			Buttons[(int32)EBtn::Paper].SetHovered(MakeBox(Rgb(0xfa, 0xf3, 0xe0), Gold, 2.f, 3.f));
			Buttons[(int32)EBtn::Paper].SetDisabled(MakeBox(PaperDark, WithAlpha(GoldDim, 0.4f), 1.f, 3.f));
			Buttons[(int32)EBtn::Paper].SetDisabledForeground(InkSoft);
			Buttons[(int32)EBtn::Tab]       = MakeBtn(WithAlpha(Night, 0.f), WithAlpha(Gold, 0.12f), WithAlpha(Gold, 0.f), Muted, Gold);
			Buttons[(int32)EBtn::TabActive] = MakeBtn(WithAlpha(Gold, 0.18f), WithAlpha(Gold, 0.25f), Gold, Gold, Cream);
			Buttons[(int32)EBtn::Tab].SetNormalPadding(FMargin(10.f, 4.f)).SetPressedPadding(FMargin(10.f, 5.f, 10.f, 3.f));
			Buttons[(int32)EBtn::TabActive].SetNormalPadding(FMargin(10.f, 4.f)).SetPressedPadding(FMargin(10.f, 5.f, 10.f, 3.f));

			Slider.SetNormalBarImage(FSlateRoundedBoxBrush(GoldDim, 2.f));
			Slider.SetHoveredBarImage(FSlateRoundedBoxBrush(Gold, 2.f));
			Slider.SetDisabledBarImage(FSlateRoundedBoxBrush(WithAlpha(GoldDim, 0.3f), 2.f));
			Slider.SetNormalThumbImage(MakeBox(Gold, Ink, 2.f, 3.f));
			Slider.NormalThumbImage.ImageSize = FVector2D(16.f, 28.f);
			Slider.SetHoveredThumbImage(MakeBox(Cream, Gold, 2.f, 3.f));
			Slider.HoveredThumbImage.ImageSize = FVector2D(16.f, 28.f);
			Slider.SetDisabledThumbImage(MakeBox(GoldDim, Ink, 2.f, 3.f));
			Slider.DisabledThumbImage.ImageSize = FVector2D(16.f, 28.f);
			Slider.SetBarThickness(4.f);

			Scroll = FCoreStyle::Get().GetWidgetStyle<FScrollBarStyle>("ScrollBar");
			Scroll.SetNormalThumbImage(FSlateRoundedBoxBrush(WithAlpha(Gold, 0.45f), 3.f));
			Scroll.SetHoveredThumbImage(FSlateRoundedBoxBrush(WithAlpha(Gold, 0.7f), 3.f));
			Scroll.SetDraggedThumbImage(FSlateRoundedBoxBrush(Gold, 3.f));
			Scroll.SetVerticalBackgroundImage(FSlateColorBrush(WithAlpha(Ink, 0.15f)));
			Scroll.SetHorizontalBackgroundImage(FSlateColorBrush(WithAlpha(Ink, 0.15f)));
			Scroll.SetThickness(6.f);
		}
	};
	static FStyleSet& Style() { static FStyleSet S; return S; }

	const FSlateBrush* Brush(EBrush B) { return &Style().Brushes[(int32)B]; }
	const FButtonStyle& ButtonStyle(EBtn B) { return Style().Buttons[(int32)B]; }
	const FSliderStyle& SliderStyle() { return Style().Slider; }
	const FCheckBoxStyle& CheckStyle() { return Style().Check; }
	const FScrollBarStyle& ScrollBarStyle() { return Style().Scroll; }

	// ── Audio ────────────────────────────────────────────────────────────────
	static TWeakObjectPtr<UObject> GContext;
	void SetContext(UObject* WorldContext) { GContext = WorldContext; }
	UObject* GetContext() { return GContext.Get(); }
	void Sfx(FName Name, float Volume)
	{
		if (UObject* Ctx = GContext.Get())
		{
			if (UDCAudioSubsystem* A = UDCAudioSubsystem::Get(Ctx)) { A->PlaySFX(Name, Volume); }
		}
	}

	// ── Formatting ───────────────────────────────────────────────────────────
	FString Money(int64 V)
	{
		const FString N = FText::AsNumber(FMath::Abs(V)).ToString();
		return V < 0 ? FString(TEXT("-$")) + N : FString(TEXT("$")) + N;
	}
	FString Pct(float V01) { return FString::Printf(TEXT("%d%%"), FMath::RoundToInt(V01 * 100.f)); }
	FString SignedPct(float V01)
	{
		const int32 P = FMath::RoundToInt(V01 * 100.f);
		return FString::Printf(TEXT("%s%d%%"), P >= 0 ? TEXT("+") : TEXT(""), P);
	}
	FString Roman(int32 V)
	{
		static const TCHAR* R[] = { TEXT("0"), TEXT("I"), TEXT("II"), TEXT("III"), TEXT("IV"), TEXT("V") };
		return (V >= 0 && V <= 5) ? FString(R[V]) : FString::FromInt(V);
	}
	FString GearSummary(const FDCGearItem& G)
	{
		TArray<FString> Parts;
		for (int32 i = 0; i < (int32)EDCStat::MAX; ++i)
		{
			const int32 V = G.StatBonus.Get((EDCStat)i);
			if (V != 0) { Parts.Add(FString::Printf(TEXT("%+d %s"), V, DC::StatName((EDCStat)i))); }
		}
		if (G.LuckBonus != 0) { Parts.Add(FString::Printf(TEXT("%+d Luck"), G.LuckBonus)); }
		if (!FMath::IsNearlyEqual(G.ArrestRiskMultiplier, 1.f))
		{
			Parts.Add(FString::Printf(TEXT("%+d%% arrest risk"), FMath::RoundToInt((G.ArrestRiskMultiplier - 1.f) * 100.f)));
		}
		return Parts.Num() ? FString::Join(Parts, TEXT(", ")) : FString(TEXT("No stat change"));
	}

	// ── Widgets ──────────────────────────────────────────────────────────────
	TSharedRef<SWidget> Txt(const FString& S, const FSlateFontInfo& Font, const FLinearColor& Col, bool bWrap, ETextJustify::Type Justify)
	{
		return SNew(STextBlock)
			.Text(FText::FromString(S))
			.Font(Font)
			.ColorAndOpacity(Col)
			.AutoWrapText(bWrap)
			.Justification(Justify);
	}

	TSharedRef<SWidget> TxtA(TAttribute<FText> S, const FSlateFontInfo& Font, TAttribute<FSlateColor> Col, ETextJustify::Type Justify)
	{
		return SNew(STextBlock).Text(S).Font(Font).ColorAndOpacity(Col).Justification(Justify);
	}

	TSharedRef<SWidget> ButtonContent(TSharedRef<SWidget> Content, TFunction<void()> OnClick, EBtn Kind, TAttribute<bool> Enabled, const FString& Tooltip)
	{
		TSharedRef<SButton> B = SNew(SButton)
			.ButtonStyle(&ButtonStyle(Kind))
			.IsFocusable(false)
			.IsEnabled(Enabled)
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Center)
			.OnHovered_Lambda([]() { Sfx(DCSfx::Hover, 0.35f); })
			.OnClicked_Lambda([OnClick]() { Sfx(DCSfx::Click); if (OnClick) { OnClick(); } return FReply::Handled(); })
			[
				Content
			];
		if (!Tooltip.IsEmpty()) { B->SetToolTipText(FText::FromString(Tooltip)); }
		return B;
	}

	TSharedRef<SWidget> Button(const FString& Label, TFunction<void()> OnClick, EBtn Kind, TAttribute<bool> Enabled, float FontSize, const FString& Tooltip)
	{
		TSharedRef<SWidget> Content = SNew(STextBlock)
			.Text(FText::FromString(Label.ToUpper()))
			.Font(Sans(FontSize))
			.Justification(ETextJustify::Center);
		TSharedRef<SWidget> B = ButtonContent(SNew(SBox).HAlign(HAlign_Center)[Content], OnClick, Kind, Enabled, Tooltip);
		return B;
	}

	TSharedRef<SWidget> Chip(const FString& Label, const FLinearColor& Col, float FontSize)
	{
		const float Lum = 0.2126f * Col.R + 0.7152f * Col.G + 0.0722f * Col.B;
		return SNew(SBorder)
			.BorderImage(Brush(EBrush::Chip))
			.BorderBackgroundColor(Col)
			.Padding(FMargin(8.f, 1.f, 8.f, 2.f))
			[
				SNew(STextBlock)
				.Text(FText::FromString(Label.ToUpper()))
				.Font(Sans(FontSize))
				.ColorAndOpacity(Lum > 0.35f ? Ink : Cream)
			];
	}

	TSharedRef<SWidget> TypeChip(EDCCriminalType T, float FontSize) { return Chip(DC::TypeName(T), TypeCol(T), FontSize); }

	TSharedRef<SWidget> HLine(const FLinearColor& Col, float Thickness)
	{
		return SNew(SBox).HeightOverride(Thickness)[SNew(SImage).Image(Brush(EBrush::White)).ColorAndOpacity(Col)];
	}
	TSharedRef<SWidget> VLine(const FLinearColor& Col, float Thickness)
	{
		return SNew(SBox).WidthOverride(Thickness)[SNew(SImage).Image(Brush(EBrush::White)).ColorAndOpacity(Col)];
	}

	TSharedRef<SWidget> Bar(TAttribute<float> Fill01, const FLinearColor& Fg, const FLinearColor& Bg, float Height)
	{
		return SNew(SBox).HeightOverride(Height)
			[
				SNew(SOverlay)
				+ SOverlay::Slot()[SNew(SImage).Image(Brush(EBrush::Round)).ColorAndOpacity(Bg)]
				+ SOverlay::Slot()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(TAttribute<float>::CreateLambda([Fill01]() { return FMath::Clamp(Fill01.Get(), 0.f, 1.f); }))
					[SNew(SImage).Image(Brush(EBrush::Round)).ColorAndOpacity(Fg)]
					+ SHorizontalBox::Slot().FillWidth(TAttribute<float>::CreateLambda([Fill01]() { return 1.f - FMath::Clamp(Fill01.Get(), 0.f, 1.f); }))
					[SNew(SSpacer)]
				]
			];
	}

	TSharedRef<SWidget> StatRow(const FString& Label, int32 Value, int32 Delta, const FLinearColor& Col, bool bOnPaper, float LabelWidth)
	{
		const FLinearColor TextCol = bOnPaper ? Ink : Cream;
		const FLinearColor Bg = bOnPaper ? WithAlpha(Ink, 0.12f) : WithAlpha(Cream, 0.1f);
		TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[SNew(SBox).WidthOverride(LabelWidth)[Txt(Label, Sans(14), bOnPaper ? InkSoft : Muted)]]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[SNew(SBox).WidthOverride(30.f)[Txt(FString::FromInt(Value), Sans(15), TextCol, false, ETextJustify::Right)]]
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center).Padding(8.f, 0.f, 0.f, 0.f)
			[Bar(Value / 100.f, Col, Bg, 7.f)];
		if (Delta != 0)
		{
			Row->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(6.f, 0.f, 0.f, 0.f)
				[SNew(SBox).WidthOverride(34.f)[Txt(FString::Printf(TEXT("%+d"), Delta), Sans(14), Delta > 0 ? (bOnPaper ? Green : GreenBright) : RedBright)]];
		}
		return Row;
	}

	TSharedRef<SWidget> Header(const FString& S, bool bOnPaper, float Size)
	{
		const FLinearColor LineCol = bOnPaper ? WithAlpha(GoldDim, 0.8f) : WithAlpha(Gold, 0.6f);
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[SNew(SBox).WidthOverride(18.f)[HLine(LineCol)]]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8.f, 0.f)
			[Txt(S.ToUpper(), Display(Size), bOnPaper ? Ink : Gold)]
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[HLine(LineCol)];
	}

	TSharedRef<SWidget> PaperCard(TSharedRef<SWidget> Content, FMargin Pad)
	{
		return SNew(SBorder).BorderImage(Brush(EBrush::PaperCard)).Padding(Pad).ForegroundColor(Ink)[Content];
	}
	TSharedRef<SWidget> DarkCard(TSharedRef<SWidget> Content, FMargin Pad)
	{
		return SNew(SBorder).BorderImage(Brush(EBrush::PanelInner)).Padding(Pad)[Content];
	}

	TSharedRef<SWidget> DecoFrame(TSharedRef<SWidget> Content, FMargin Pad)
	{
		// Outer gold hairline, 4px gap, inner dim hairline, plus little corner diamonds.
		auto Diamond = []()
		{
			return SNew(SBox).WidthOverride(9.f).HeightOverride(9.f)
				.RenderTransform(FSlateRenderTransform(FQuat2D(FMath::DegreesToRadians(45.f))))
				.RenderTransformPivot(FVector2D(0.5f, 0.5f))
				[SNew(SImage).Image(Brush(EBrush::White)).ColorAndOpacity(Gold)];
		};
		return SNew(SOverlay)
			+ SOverlay::Slot()
			[
				SNew(SBorder).BorderImage(Brush(EBrush::PanelDark)).Padding(4.f)
				[
					SNew(SBorder).BorderImage(Brush(EBrush::Outline)).BorderBackgroundColor(WithAlpha(Gold, 0.35f)).Padding(Pad)
					[Content]
				]
			]
			+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(-4.f, -4.f, 0.f, 0.f)[Diamond()]
			+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(0.f, -4.f, -4.f, 0.f)[Diamond()]
			+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Bottom).Padding(-4.f, 0.f, 0.f, -4.f)[Diamond()]
			+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Bottom).Padding(0.f, 0.f, -4.f, -4.f)[Diamond()];
	}

	TSharedRef<SWidget> Stamp(const FString& S, const FLinearColor& Col, float Size, float AngleDeg)
	{
		const float T = FMath::Max(3.f, Size / 18.f);
		return SNew(SBox)
			.RenderTransform(FSlateRenderTransform(FQuat2D(FMath::DegreesToRadians(AngleDeg))))
			.RenderTransformPivot(FVector2D(0.5f, 0.5f))
			[
				SNew(SOverlay)
				+ SOverlay::Slot().Padding(FMargin(16.f + T, 4.f + T, 16.f + T, 2.f + T))
				[
					SNew(STextBlock).Text(FText::FromString(S)).Font(Big(Size)).ColorAndOpacity(Col)
				]
				+ SOverlay::Slot().VAlign(VAlign_Top)[HLine(Col, T)]
				+ SOverlay::Slot().VAlign(VAlign_Bottom)[HLine(Col, T)]
				+ SOverlay::Slot().HAlign(HAlign_Left)[VLine(Col, T)]
				+ SOverlay::Slot().HAlign(HAlign_Right)[VLine(Col, T)]
			];
	}

	TSharedRef<SWidget> Sprockets(int32 Count, float Size)
	{
		TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox);
		for (int32 i = 0; i < Count; ++i)
		{
			Row->AddSlot().FillWidth(1.f).HAlign(HAlign_Center)
				[SNew(SBox).WidthOverride(Size * 1.4f).HeightOverride(Size)[SNew(SImage).Image(Brush(EBrush::Round)).ColorAndOpacity(WithAlpha(Cream, 0.18f))]];
		}
		return Row;
	}

	TSharedRef<SWidget> KeyValue(const FString& K, const FString& V, const FLinearColor& VCol, bool bOnPaper, float Size)
	{
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[Txt(K, Sans(Size - 1.f), bOnPaper ? InkSoft : Muted)]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Txt(V, Sans(Size), VCol)];
	}

	TSharedRef<SWidget> Pips(int32 Filled, int32 Max, const FLinearColor& On, const FLinearColor& Off, float Size)
	{
		TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox);
		for (int32 i = 0; i < Max; ++i)
		{
			Row->AddSlot().AutoWidth().Padding(0.f, 0.f, Size * 0.45f, 0.f)
				[SNew(SBox).WidthOverride(Size).HeightOverride(Size)
					.RenderTransform(FSlateRenderTransform(FQuat2D(FMath::DegreesToRadians(45.f)))).RenderTransformPivot(FVector2D(0.5f, 0.5f))
					[SNew(SImage).Image(Brush(EBrush::White)).ColorAndOpacity(i < Filled ? On : Off)]];
		}
		return Row;
	}
}

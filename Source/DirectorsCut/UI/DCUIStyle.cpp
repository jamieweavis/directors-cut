#include "UI/DCUIStyle.h"
#include "Core/DCAudioSubsystem.h"
#include "Core/DCSettings.h"
#include "Brushes/SlateColorBrush.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Brushes/SlateNoResource.h"
#include "Fonts/CompositeFont.h"
#include "Styling/CoreStyle.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SToolTip.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "World/DCPortraits.h"
#include "DirectorsCut.h"

namespace DCUI
{
	FLinearColor Hex(uint32 RGB, float Alpha)
	{
		FLinearColor C = FLinearColor::FromSRGBColor(FColor((RGB >> 16) & 0xff, (RGB >> 8) & 0xff, RGB & 0xff, 255));
		C.A = Alpha;
		return C;
	}

	// Accent: sodium-vapour amber (#F5A524). Streetlights, cash and warning tape: it reads as "night-time city" without
	// any neon, stays legible on near-black glass at small sizes, and is far enough from the semantic green/red that the
	// odds ring (green) and its bonus overflow (amber) never get confused.
	FLinearColor Glass = Hex(0x0B0D10, 0.92f);
	FLinearColor GlassSoft = Hex(0x0E1115, 0.86f);
	FLinearColor Card = Hex(0x161A20, 0.92f);
	FLinearColor CardHi = Hex(0x1E232B, 0.96f);
	FLinearColor Inset = Hex(0x07080A, 0.70f);
	FLinearColor Line = FLinearColor(1.f, 1.f, 1.f, 0.085f);
	FLinearColor LineHi = FLinearColor(1.f, 1.f, 1.f, 0.20f);
	FLinearColor Text = Hex(0xE8EAED);
	FLinearColor TextDim = Hex(0x9AA3AE);
	FLinearColor TextFaint = Hex(0x5C6570);
	FLinearColor Accent = Hex(0xF5A524);
	FLinearColor AccentInk = Hex(0x14100A);
	FLinearColor Good = Hex(0x34D399);
	FLinearColor Bad = Hex(0xF43F5E);
	FLinearColor Warn = Hex(0xFACC15);
	FLinearColor Info = Hex(0x60A5FA);

	void ApplyHighContrast(bool bOn)
	{
		Glass     = bOn ? Hex(0x050608, 0.985f) : Hex(0x0B0D10, 0.92f);
		GlassSoft = bOn ? Hex(0x07090B, 0.97f) : Hex(0x0E1115, 0.86f);
		Card      = bOn ? Hex(0x14181E, 1.f) : Hex(0x161A20, 0.92f);
		CardHi    = bOn ? Hex(0x222832, 1.f) : Hex(0x1E232B, 0.96f);
		Line      = bOn ? FLinearColor(1.f, 1.f, 1.f, 0.24f) : FLinearColor(1.f, 1.f, 1.f, 0.085f);
		LineHi    = bOn ? FLinearColor(1.f, 1.f, 1.f, 0.55f) : FLinearColor(1.f, 1.f, 1.f, 0.20f);
		Text      = bOn ? Hex(0xFFFFFF) : Hex(0xE8EAED);
		TextDim   = bOn ? Hex(0xCDD3DA) : Hex(0x9AA3AE);
		TextFaint = bOn ? Hex(0x9AA3AE) : Hex(0x5C6570);
	}

	static FPrefs GPrefs;
	const FPrefs& Prefs() { return GPrefs; }

	EUrgency Urgency(double S)
	{
		if (S < 0.0) { return EUrgency::Calm; }
		if (S < UrgentCritical) { return EUrgency::Critical; }
		if (S < UrgentSoon) { return EUrgency::Soon; }
		return EUrgency::Calm;
	}
	FLinearColor UrgencyColor(double S, const FLinearColor& Calm)
	{
		if (!GPrefs.bExpiryWarnings) { return Calm; }
		switch (Urgency(S))
		{
		case EUrgency::Critical: return Bad;
		case EUrgency::Soon:     return Accent;
		default:                 return Calm;
		}
	}
	float Pulse(float Hz)
	{
		if (GPrefs.bReduceFlashing) { return 1.f; }
		return 0.5f + 0.5f * FMath::Cos((float)FMath::Fmod(FPlatformTime::Seconds() * Hz, 1.0) * 2.f * PI);
	}

	FLinearColor ChanceColor(float C)
	{
		if (C >= 0.75f) { return Good; }
		if (C >= 0.5f) { return FLinearColor::LerpUsingHSV(Warn, Good, (C - 0.5f) / 0.25f); }
		if (C >= 0.25f) { return FLinearColor::LerpUsingHSV(Bad, Warn, (C - 0.25f) / 0.25f); }
		return Bad;
	}
	FLinearColor OutcomeColor(EDCMissionOutcome O)
	{
		switch (O)
		{
		case EDCMissionOutcome::Success: return Good;
		case EDCMissionOutcome::Failed:  return Warn;
		case EDCMissionOutcome::Busted:  return Bad;
		default: return TextDim;
		}
	}
	FLinearColor TierColor(int32 Tier)
	{
		switch (FMath::Clamp(Tier, 1, 4))
		{
		case 1: return Hex(0x9AA3AE);
		case 2: return Hex(0x60A5FA);
		case 3: return Hex(0xA78BFA);
		default: return Accent;
		}
	}

	// ── Fonts ────────────────────────────────────────────────────────────────
	struct FFonts
	{
		TSharedPtr<const FCompositeFont> Inter[4], Mono, Icons;
		TMap<FName, int32> Codepoints;
		bool bInit = false;
	};
	static FFonts& Fonts()
	{
		static FFonts S;
		if (S.bInit) { return S; }
		S.bInit = true;
		const FString Dir = FPaths::ProjectContentDir() / TEXT("Fonts");
		const FString Fallback = FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Regular.ttf");
		auto Make = [&](const TCHAR* File, const TCHAR* Name, bool bFallback) -> TSharedPtr<const FCompositeFont>
		{
			const FString Path = Dir / File;
			if (!IFileManager::Get().FileExists(*Path) || IFileManager::Get().FileSize(*Path) < 4096)
			{
				UE_LOG(LogDC, Warning, TEXT("[UI] Font missing or an LFS pointer: %s (falling back to Roboto)"), *Path);
				return nullptr;
			}
			TSharedRef<FStandaloneCompositeFont> F = MakeShared<FStandaloneCompositeFont>(FName(Name), Path, EFontHinting::Default, EFontLoadingPolicy::LazyLoad);
			if (bFallback && IFileManager::Get().FileExists(*Fallback))
			{
				F->FallbackTypeface.Typeface.AppendFont(TEXT("Fallback"), Fallback, EFontHinting::Default, EFontLoadingPolicy::LazyLoad);
			}
			return F;
		};
		S.Inter[0] = Make(TEXT("Inter-Regular.ttf"), TEXT("DCInterRegular"), true);
		S.Inter[1] = Make(TEXT("Inter-Medium.ttf"), TEXT("DCInterMedium"), true);
		S.Inter[2] = Make(TEXT("Inter-SemiBold.ttf"), TEXT("DCInterSemiBold"), true);
		S.Inter[3] = Make(TEXT("Inter-Bold.ttf"), TEXT("DCInterBold"), true);
		S.Mono = Make(TEXT("JetBrainsMono-Variable.ttf"), TEXT("DCMono"), true);
		S.Icons = Make(TEXT("Lucide.ttf"), TEXT("DCLucide"), false);

		FString Json;
		if (FFileHelper::LoadFileToString(Json, *(Dir / TEXT("lucide_codepoints.json"))))
		{
			TSharedPtr<FJsonObject> Obj;
			const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
			if (FJsonSerializer::Deserialize(Reader, Obj) && Obj.IsValid())
			{
				for (const TPair<FString, TSharedPtr<FJsonValue>>& KV : Obj->Values)
				{
					S.Codepoints.Add(FName(*KV.Key), (int32)KV.Value->AsNumber());
				}
			}
		}
		UE_LOG(LogDC, Log, TEXT("[UI] Lucide icons loaded: %d"), S.Codepoints.Num());
		return S;
	}

	// Small sizes get a nudge: the UI is laid out at 1080p and scales down to 720p, where 7-8 pt gets too thin.
	static float Legible(float Size) { Size *= GPrefs.TextScale; return Size < 10.f ? Size + 0.5f : Size; }

	FSlateFontInfo Font(float Size, EW Weight, int32 LetterSpacing)
	{
		Size = Legible(Size);
		const TSharedPtr<const FCompositeFont>& F = Fonts().Inter[(int32)Weight];
		FSlateFontInfo I = F.IsValid() ? FSlateFontInfo(F, Size) : FCoreStyle::GetDefaultFontStyle(Weight >= EW::SemiBold ? TEXT("Bold") : TEXT("Regular"), FMath::RoundToInt(Size));
		I.LetterSpacing = LetterSpacing;
		return I;
	}
	FSlateFontInfo Mono(float Size, int32 LetterSpacing)
	{
		Size = Legible(Size);
		const TSharedPtr<const FCompositeFont>& F = Fonts().Mono;
		FSlateFontInfo I = F.IsValid() ? FSlateFontInfo(F, Size) : FCoreStyle::GetDefaultFontStyle(TEXT("Mono"), FMath::RoundToInt(Size));
		I.LetterSpacing = LetterSpacing;
		return I;
	}
	FSlateFontInfo Caps(float Size) { return Font(Size, EW::SemiBold, 140); }

	// ── Icons ────────────────────────────────────────────────────────────────
	bool HasIcon(FName N) { return Fonts().Codepoints.Contains(N); }
	FString IconGlyph(FName N)
	{
		FFonts& F = Fonts();
		const int32* CP = F.Codepoints.Find(N);
		if (!CP)
		{
			static TSet<FName> Warned;
			if (!N.IsNone() && !Warned.Contains(N)) { Warned.Add(N); UE_LOG(LogDC, Warning, TEXT("[UI] Unknown Lucide icon '%s'"), *N.ToString()); }
			CP = F.Codepoints.Find(TEXT("circle-help"));
		}
		if (!CP) { return TEXT("?"); }
		FString S; S.AppendChar((TCHAR)*CP);
		return S;
	}
	FSlateFontInfo IconFont(float Size)
	{
		const TSharedPtr<const FCompositeFont>& F = Fonts().Icons;
		return F.IsValid() ? FSlateFontInfo(F, Size) : FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), FMath::RoundToInt(Size));
	}
	TSharedRef<SWidget> Icon(FName N, float Size, TAttribute<FSlateColor> Color)
	{
		return SNew(STextBlock).Text(FText::FromString(IconGlyph(N))).Font(IconFont(Size)).ColorAndOpacity(Color).Visibility(EVisibility::HitTestInvisible);
	}
	TSharedRef<SWidget> IconA(TAttribute<FName> N, float Size, TAttribute<FSlateColor> Color)
	{
		return SNew(STextBlock).Text_Lambda([N]() { return FText::FromString(IconGlyph(N.Get())); }).Font(IconFont(Size)).ColorAndOpacity(Color).Visibility(EVisibility::HitTestInvisible);
	}

	// ── Brushes ──────────────────────────────────────────────────────────────
	const FSlateBrush* Box(const FLinearColor& Fill, float Radius, const FLinearColor& Outline, float OutlineWidth)
	{
		static TMap<uint32, TUniquePtr<FSlateBrush>> Cache;
		uint32 H = GetTypeHash(Fill.ToFColor(false));
		H = HashCombine(H, GetTypeHash(FMath::RoundToInt(Fill.A * 1000.f)));
		H = HashCombine(H, GetTypeHash(FMath::RoundToInt(Radius * 10.f)));
		H = HashCombine(H, GetTypeHash(Outline.ToFColor(false)));
		H = HashCombine(H, GetTypeHash(FMath::RoundToInt(Outline.A * 1000.f)));
		H = HashCombine(H, GetTypeHash(FMath::RoundToInt(OutlineWidth * 10.f)));
		if (TUniquePtr<FSlateBrush>* Found = Cache.Find(H)) { return Found->Get(); }
		FSlateBrush* B = nullptr;
		if (Radius < 0.f)
		{
			B = new FSlateRoundedBoxBrush(Fill, Outline, OutlineWidth);
			B->OutlineSettings.RoundingType = ESlateBrushRoundingType::HalfHeightRadius;
		}
		else
		{
			B = new FSlateRoundedBoxBrush(Fill, Radius, Outline, OutlineWidth);
		}
		Cache.Add(H, TUniquePtr<FSlateBrush>(B));
		return B;
	}
	const FSlateBrush* WhiteBrush() { static FSlateColorBrush B(FLinearColor::White); return &B; }
	const FSlateBrush* NoBrush() { static FSlateNoResource B; return &B; }
	const FSlateBrush* ShadeLeft() { static FSlateColorBrush B(FLinearColor(0.f, 0.f, 0.f, 1.f)); return &B; }

	// ── Styles ───────────────────────────────────────────────────────────────
	struct FStyles
	{
		FButtonStyle Buttons[(int32)EBtn::MAX];
		FSliderStyle Slider;
		FScrollBarStyle Scroll;
		FProgressBarStyle Progress;
		FStyles()
		{
			auto Make = [](const FLinearColor& Fill, const FLinearColor& Hover, const FLinearColor& Outline, const FLinearColor& HoverOutline, const FLinearColor& Fg, float Radius)
			{
				FButtonStyle S;
				S.SetNormal(*Box(Fill, Radius, Outline, Outline.A > 0.f ? 1.f : 0.f));
				S.SetHovered(*Box(Hover, Radius, HoverOutline, HoverOutline.A > 0.f ? 1.f : 0.f));
				S.SetPressed(*Box(Hover * FLinearColor(0.85f, 0.85f, 0.85f, 1.f), Radius, HoverOutline, HoverOutline.A > 0.f ? 1.f : 0.f));
				FLinearColor DF = Fill; DF.A *= 0.45f;
				S.SetDisabled(*Box(DF, Radius, A(Outline, Outline.A * 0.5f), Outline.A > 0.f ? 1.f : 0.f));
				S.SetNormalForeground(Fg); S.SetHoveredForeground(Fg); S.SetPressedForeground(Fg); S.SetDisabledForeground(A(Fg, 0.4f));
				S.SetNormalPadding(FMargin(0.f)); S.SetPressedPadding(FMargin(0.f, 1.f, 0.f, -1.f));
				return S;
			};
			Buttons[(int32)EBtn::Primary]      = Make(Accent, Hex(0xFFB940), FLinearColor::Transparent, A(FLinearColor::White, 0.35f), AccentInk, 8.f);
			Buttons[(int32)EBtn::Secondary]    = Make(Hex(0x1A1E25, 0.95f), Hex(0x252B34), Line, LineHi, Text, 8.f);
			Buttons[(int32)EBtn::Ghost]        = Make(FLinearColor::Transparent, A(FLinearColor::White, 0.07f), FLinearColor::Transparent, FLinearColor::Transparent, TextDim, 8.f);
			Buttons[(int32)EBtn::Danger]       = Make(A(Bad, 0.16f), A(Bad, 0.28f), A(Bad, 0.45f), A(Bad, 0.8f), Bad, 8.f);
			Buttons[(int32)EBtn::Card]         = Make(Card, CardHi, Line, LineHi, Text, 10.f);
			Buttons[(int32)EBtn::CardSelected] = Make(Hex(0x221D14, 0.95f), Hex(0x2A2318, 0.98f), A(Accent, 0.75f), Accent, Text, 10.f);
			Buttons[(int32)EBtn::Dock]         = Make(FLinearColor::Transparent, A(FLinearColor::White, 0.08f), FLinearColor::Transparent, A(FLinearColor::White, 0.10f), TextDim, 10.f);
			Buttons[(int32)EBtn::DockActive]   = Make(A(Accent, 0.16f), A(Accent, 0.24f), A(Accent, 0.55f), Accent, Accent, 10.f);

			FSlateBrush Bar = *Box(FLinearColor::Transparent, 2.f);
			Slider.SetNormalBarImage(Bar).SetHoveredBarImage(Bar).SetDisabledBarImage(Bar);
			const FSlateBrush* Thumb = Box(Text, -1.f, Accent, 3.f);
			FSlateBrush ThumbB = *Thumb; ThumbB.ImageSize = FVector2D(18.f, 18.f);
			FSlateBrush ThumbH = *Box(FLinearColor::White, -1.f, Hex(0xFFB940), 3.f); ThumbH.ImageSize = FVector2D(20.f, 20.f);
			Slider.SetNormalThumbImage(ThumbB).SetHoveredThumbImage(ThumbH).SetDisabledThumbImage(ThumbB);
			Slider.SetBarThickness(4.f);

			Scroll = FCoreStyle::Get().GetWidgetStyle<FScrollBarStyle>("ScrollBar");
			const FSlateBrush ThumbS = *Box(A(FLinearColor::White, 0.14f), 3.f);
			const FSlateBrush ThumbSH = *Box(A(FLinearColor::White, 0.28f), 3.f);
			Scroll.SetNormalThumbImage(ThumbS).SetHoveredThumbImage(ThumbSH).SetDraggedThumbImage(ThumbSH);
			const FSlateBrush NoB = *NoBrush();
			Scroll.SetVerticalBackgroundImage(NoB).SetHorizontalBackgroundImage(NoB);
			Scroll.SetVerticalTopSlotImage(NoB).SetVerticalBottomSlotImage(NoB).SetHorizontalTopSlotImage(NoB).SetHorizontalBottomSlotImage(NoB);

			Progress = FCoreStyle::Get().GetWidgetStyle<FProgressBarStyle>("ProgressBar");
			Progress.SetBackgroundImage(*Box(FLinearColor::White, -1.f));
			Progress.SetFillImage(*Box(FLinearColor::White, -1.f));
			Progress.SetMarqueeImage(*Box(FLinearColor::White, -1.f));
			Progress.SetEnableFillAnimation(false);
		}
	};
	static FStyles& Styles() { static FStyles S; return S; }

	void RefreshPrefs()
	{
		const bool bHC = DCSettings::HighContrastUI();
		GPrefs.bReduceMotion = DCSettings::ReduceMotion();
		GPrefs.bReduceFlashing = DCSettings::ReduceFlashing();
		GPrefs.bExpiryWarnings = DCSettings::ExpiryWarnings();
		GPrefs.TextScale = FMath::Clamp(DCSettings::TextScale(), 0.8f, 1.4f);
		GPrefs.UIScale = FMath::Clamp(DCSettings::UIScale(), 0.8f, 1.5f);
		GPrefs.bShowFPS = DCSettings::ShowFPS();
		if (bHC != GPrefs.bHighContrast)
		{
			GPrefs.bHighContrast = bHC;
			ApplyHighContrast(bHC);
			Styles() = FStyles(); // same addresses: live widgets keep valid style pointers
		}
	}
	const FButtonStyle& ButtonStyle(EBtn Kind) { return Styles().Buttons[(int32)Kind]; }
	const FSliderStyle& SliderStyle() { return Styles().Slider; }
	const FScrollBarStyle& ScrollBarStyle() { return Styles().Scroll; }
	const FProgressBarStyle& ProgressStyle() { return Styles().Progress; }

	// ── Context ──────────────────────────────────────────────────────────────
	static TWeakObjectPtr<UObject> GContext;
	void SetContext(UObject* W) { GContext = W; }
	UObject* GetContext() { return GContext.Get(); }
	void Sfx(FName Name, float Volume)
	{
		if (UDCAudioSubsystem* Au = UDCAudioSubsystem::Get(GContext.Get())) { Au->PlaySFX(Name, Volume); }
	}

	double Now() { return FPlatformTime::Seconds(); }
	float EaseOut(float X) { X = FMath::Clamp(X, 0.f, 1.f); return 1.f - FMath::Pow(1.f - X, 3.f); }
	float EaseInOut(float X) { X = FMath::Clamp(X, 0.f, 1.f); return X < 0.5f ? 4.f * X * X * X : 1.f - FMath::Pow(-2.f * X + 2.f, 3.f) / 2.f; }

	// ── Formatting ──────────────────────────────────────────────────────────
	FString Money(int64 V)
	{
		const bool bNeg = V < 0;
		uint64 U = (uint64)(bNeg ? -V : V);
		FString Digits = FString::Printf(TEXT("%llu"), U);
		FString Out;
		for (int32 i = 0; i < Digits.Len(); ++i)
		{
			if (i > 0 && (Digits.Len() - i) % 3 == 0) { Out.AppendChar(TEXT(',')); }
			Out.AppendChar(Digits[i]);
		}
		return (bNeg ? TEXT("-$") : TEXT("$")) + Out;
	}
	FString MoneyShort(int64 V)
	{
		const double D = (double)FMath::Abs(V);
		const TCHAR* Sign = V < 0 ? TEXT("-") : TEXT("");
		if (D >= 1e6) { return FString::Printf(TEXT("%s$%.1fM"), Sign, D / 1e6); }
		if (D >= 1e4) { return FString::Printf(TEXT("%s$%.0fk"), Sign, D / 1e3); }
		if (D >= 1e3) { return FString::Printf(TEXT("%s$%.1fk"), Sign, D / 1e3); }
		return Money(V);
	}
	FString Pct(float V) { return FString::Printf(TEXT("%d%%"), FMath::RoundToInt(V * 100.f)); }
	FString SignedPct(float V) { const int32 P = FMath::RoundToInt(V * 100.f); return FString::Printf(TEXT("%s%d%%"), P >= 0 ? TEXT("+") : TEXT(""), P); }
	FString HourMin(float H)
	{
		H = FMath::Fmod(FMath::Max(0.f, H), 24.f);
		const int32 Hr = (int32)H;
		const int32 Mn = FMath::Clamp((int32)((H - Hr) * 60.f), 0, 59);
		return FString::Printf(TEXT("%02d:%02d"), Hr, Mn);
	}

	// ── Widgets ──────────────────────────────────────────────────────────────
	TSharedRef<SWidget> Txt(const FString& S, const FSlateFontInfo& F, const FLinearColor& Col, bool bWrap)
	{
		return SNew(STextBlock).Text(FText::FromString(S)).Font(F).ColorAndOpacity(Col).AutoWrapText(bWrap);
	}
	TSharedRef<SWidget> TxtA(TAttribute<FText> S, const FSlateFontInfo& F, TAttribute<FSlateColor> Col)
	{
		return SNew(STextBlock).Text(S).Font(F).ColorAndOpacity(Col);
	}
	TSharedRef<SWidget> Label(const FString& S, const FLinearColor& Col, float Size)
	{
		return SNew(STextBlock).Text(FText::FromString(S.ToUpper())).Font(Caps(Size)).ColorAndOpacity(Col);
	}
	TSharedRef<SWidget> IconText(FName IconName, const FString& S, const FSlateFontInfo& F, const FLinearColor& IconCol, const FLinearColor& TextCol, float IconSize, float Gap)
	{
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, Gap, 0.f)[Icon(IconName, IconSize > 0.f ? IconSize : F.Size * 1.05f, IconCol)]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Txt(S, F, TextCol)];
	}
	TSharedRef<SWidget> IconTextA(FName IconName, TAttribute<FText> S, const FSlateFontInfo& F, TAttribute<FSlateColor> IconCol, TAttribute<FSlateColor> TextCol, float IconSize, float Gap)
	{
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, Gap, 0.f)[Icon(IconName, IconSize > 0.f ? IconSize : F.Size * 1.05f, IconCol)]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[TxtA(S, F, TextCol)];
	}

	TSharedRef<SToolTip> TipWidget(TSharedRef<SWidget> Content)
	{
		return SNew(SToolTip)
			.BorderImage(Box(Hex(0x0E1014, 0.97f), 8.f, LineHi, 1.f))
			.TextMargin(FMargin(12.f, 9.f))
			[
				SNew(SBox).MaxDesiredWidth(320.f)[Content]
			];
	}
	TSharedRef<SToolTip> Tip(const FString& Title, const FString& Body, FName IconName, const FLinearColor& IconCol)
	{
		TSharedRef<SVerticalBox> V = SNew(SVerticalBox);
		TSharedRef<SHorizontalBox> Head = SNew(SHorizontalBox);
		if (!IconName.IsNone())
		{
			Head->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 8.f, 0.f)[Icon(IconName, 13.f, IconCol)];
		}
		Head->AddSlot().FillWidth(1.f).VAlign(VAlign_Center)[Txt(Title, Font(11.f, EW::SemiBold), Text, true)];
		V->AddSlot().AutoHeight()[Head];
		if (!Body.IsEmpty())
		{
			V->AddSlot().AutoHeight().Padding(0.f, 4.f, 0.f, 0.f)[Txt(Body, Font(10.f), TextDim, true)];
		}
		return TipWidget(V);
	}

	TSharedRef<SWidget> Button(TSharedRef<SWidget> Content, TFunction<void()> OnClick, EBtn Kind, TAttribute<bool> Enabled, TSharedPtr<IToolTip> ToolTip, FMargin Pad)
	{
		TSharedRef<SButton> B = SNew(SButton)
			.ButtonStyle(&ButtonStyle(Kind))
			.ContentPadding(Pad)
			.IsFocusable(false)
			.IsEnabled(Enabled)
			.HAlign((Kind == EBtn::Card || Kind == EBtn::CardSelected) ? HAlign_Fill : HAlign_Center)
			.VAlign((Kind == EBtn::Card || Kind == EBtn::CardSelected) ? VAlign_Fill : VAlign_Center)
			.OnHovered_Lambda([]() { Sfx(DCSfx::Hover, 0.35f); })
			.OnClicked_Lambda([OnClick]() { if (OnClick) { OnClick(); } return FReply::Handled(); })
			[Content];
		if (ToolTip.IsValid()) { B->SetToolTip(ToolTip); }
		return B;
	}
	TSharedRef<SWidget> TextButton(FName IconName, const FString& LabelText, TFunction<void()> OnClick, EBtn Kind, TAttribute<bool> Enabled, TSharedPtr<IToolTip> ToolTip, float Size)
	{
		FLinearColor Fg = Kind == EBtn::Primary ? AccentInk : (Kind == EBtn::Danger ? Bad : (Kind == EBtn::Ghost ? TextDim : Text));
		TSharedRef<SHorizontalBox> H = SNew(SHorizontalBox);
		if (!IconName.IsNone())
		{
			H->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, LabelText.IsEmpty() ? 0.f : 7.f, 0.f)[Icon(IconName, Size * 1.15f, Fg)];
		}
		if (!LabelText.IsEmpty())
		{
			H->AddSlot().AutoWidth().VAlign(VAlign_Center)[Txt(LabelText, Font(Size, EW::SemiBold), Fg)];
		}
		return Button(H, OnClick, Kind, Enabled, ToolTip, FMargin(Size * 1.2f, Size * 0.72f));
	}
	TSharedRef<SWidget> IconButton(FName IconName, TFunction<void()> OnClick, TSharedPtr<IToolTip> ToolTip, EBtn Kind, float Size, TAttribute<bool> Enabled)
	{
		FLinearColor Fg = Kind == EBtn::Primary ? AccentInk : (Kind == EBtn::Danger ? Bad : (Kind == EBtn::Ghost ? TextDim : Text));
		return Button(SNew(SBox).WidthOverride(Size * 1.5f).HeightOverride(Size * 1.5f).HAlign(HAlign_Center).VAlign(VAlign_Center)[Icon(IconName, Size, Fg)],
			OnClick, Kind, Enabled, ToolTip, FMargin(Size * 0.35f));
	}

	TSharedRef<SWidget> GlassPanel(TSharedRef<SWidget> Content, FMargin Pad, float Radius)
	{
		return SNew(SOverlay)
			// soft drop shadow
			+ SOverlay::Slot().Padding(FMargin(-6.f, -2.f, -6.f, -12.f))[SNew(SBorder).BorderImage(Box(FLinearColor(0, 0, 0, 0.35f), Radius + 8.f)).Visibility(EVisibility::HitTestInvisible)]
			+ SOverlay::Slot().Padding(FMargin(-2.f, 0.f, -2.f, -5.f))[SNew(SBorder).BorderImage(Box(FLinearColor(0, 0, 0, 0.35f), Radius + 3.f)).Visibility(EVisibility::HitTestInvisible)]
			+ SOverlay::Slot()
			[
				SNew(SBorder).BorderImage(Box(Glass, Radius, Line, 1.f)).Padding(Pad)[Content]
			];
	}
	TSharedRef<SWidget> CardBox(TSharedRef<SWidget> Content, FMargin Pad, const FLinearColor& Outline, float Radius)
	{
		return SNew(SBorder).BorderImage(Box(Card, Radius, Outline, Outline.A > 0.f ? 1.f : 0.f)).Padding(Pad)[Content];
	}
	TSharedRef<SWidget> Pill(TSharedRef<SWidget> Content, const FLinearColor& Fill, FMargin Pad, const FLinearColor& Outline)
	{
		return SNew(SBorder).BorderImage(Box(Fill, -1.f, Outline, Outline.A > 0.f ? 1.f : 0.f)).Padding(Pad).VAlign(VAlign_Center)[Content];
	}
	TSharedRef<SWidget> IconTile(FName IconName, float Size, const FLinearColor& Col, float IconScale, float FillAlpha)
	{
		return SNew(SBox).WidthOverride(Size).HeightOverride(Size)
			[
				SNew(SBorder).BorderImage(Box(A(Col, FillAlpha), Size * 0.24f, A(Col, FillAlpha * 1.8f), 1.f)).HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(0.f)
				[Icon(IconName, Size * IconScale, Col)]
			];
	}
	TSharedRef<SWidget> Badge(TAttribute<int32> Count, const FLinearColor& Col)
	{
		return SNew(SBorder)
			.BorderImage(Box(Col, -1.f, Hex(0x0B0D10), 2.f))
			.Padding(FMargin(5.f, 1.f, 5.f, 2.f))
			.Visibility_Lambda([Count]() { return Count.Get() > 0 ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
			.HAlign(HAlign_Center)
			[
				SNew(SBox).MinDesiredWidth(9.f).HAlign(HAlign_Center)
				[
					SNew(STextBlock).Font(Font(9.f, EW::Bold)).ColorAndOpacity(Col == Accent ? AccentInk : FLinearColor::White)
					.Text_Lambda([Count]() { const int32 N = Count.Get(); return FText::FromString(N > 9 ? TEXT("9+") : FString::FromInt(N)); })
				]
			];
	}
	TSharedRef<SWidget> HLine(const FLinearColor& Col, float Thickness)
	{
		return SNew(SBox).HeightOverride(Thickness)[SNew(SImage).Image(WhiteBrush()).ColorAndOpacity(Col)];
	}
	TSharedRef<SWidget> VLine(const FLinearColor& Col, float Thickness)
	{
		return SNew(SBox).WidthOverride(Thickness)[SNew(SImage).Image(WhiteBrush()).ColorAndOpacity(Col)];
	}
	TSharedRef<SWidget> Bar(TAttribute<float> Fill01, TAttribute<FSlateColor> Col, float Height, const FLinearColor& Track)
	{
		return SNew(SBox).HeightOverride(Height)
			[
				SNew(SOverlay)
				+ SOverlay::Slot()[SNew(SBorder).BorderImage(Box(Track, -1.f)).Padding(0.f)]
				+ SOverlay::Slot()
				[
					SNew(SProgressBar)
					.Style(&ProgressStyle())
					.BackgroundImage(NoBrush())
					.BorderPadding(FVector2D(0.f, 0.f))
					.Percent_Lambda([Fill01]() { return TOptional<float>(FMath::Clamp(Fill01.Get(), 0.f, 1.f)); })
					.FillColorAndOpacity(Col)
				]
			];
	}
	TSharedRef<SWidget> Pips(int32 Filled, int32 Max, const FLinearColor& On, float Size, const FLinearColor& Off)
	{
		TSharedRef<SHorizontalBox> H = SNew(SHorizontalBox);
		for (int32 i = 0; i < Max; ++i)
		{
			H->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, i + 1 < Max ? Size * 0.5f : 0.f, 0.f)
			[
				SNew(SBox).WidthOverride(Size * 1.6f).HeightOverride(Size)
				[SNew(SBorder).BorderImage(Box(i < Filled ? On : Off, Size * 0.3f)).Padding(0.f)]
			];
		}
		return H;
	}

	TSharedRef<SWidget> AvatarType(EDCCriminalType T, EDCQuality Q, float Size, bool bDim)
	{
		FLinearColor QC = DC::QualityColor(Q);
		FLinearColor TC = DC::TypeColor(T);
		const float Ring = FMath::Max(1.5f, Size * 0.055f);
		return SNew(SBox).WidthOverride(Size).HeightOverride(Size)
			[
				SNew(SOverlay)
				+ SOverlay::Slot()
				[
					SNew(SBorder)
					.BorderImage(Box(Hex(0x1A1F27, 1.f), -1.f, A(QC, bDim ? 0.35f : 1.f), Ring))
					.ColorAndOpacity(FLinearColor(1, 1, 1, bDim ? 0.55f : 1.f))
					.HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(0.f)
					[Icon(DC::TypeIcon(T), Size * 0.42f, bDim ? A(TC, 0.55f) : TC)]
				]
			];
	}
	TSharedRef<SWidget> Avatar(const FDCCriminal& C, float Size, bool bDim)
	{
		// A head-and-shoulders portrait of the crew member's own body, baked once per look by UDCPortraits. Until it's
		// ready (a few frames the first time a look is seen) the type icon stands in.
		const uint32 Key = UDCPortraits::KeyFor(C);
		const FLinearColor QC = DC::QualityColor(C.Quality);
		const FLinearColor TC = DC::TypeColor(C.Type);
		const float Ring = FMath::Max(1.5f, Size * 0.05f);
		return SNew(SBox).WidthOverride(Size).HeightOverride(Size)
			[
				SNew(SOverlay)
				+ SOverlay::Slot()
				[
					SNew(SBorder)
					.BorderImage(Box(Hex(0x1A1F27, 1.f), -1.f, FLinearColor::Transparent, 0.f))
					.HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(0.f)
					[Icon(DC::TypeIcon(C.Type), Size * 0.42f, bDim ? A(TC, 0.55f) : TC)]
				]
				+ SOverlay::Slot()
				[
					SNew(SImage)
					.Image_Lambda([Key]() { return UDCPortraits::Find(Key); })
					.ColorAndOpacity(bDim ? FLinearColor(0.42f, 0.42f, 0.44f, 1.f) : FLinearColor::White)
					.Visibility(EVisibility::HitTestInvisible)
				]
				+ SOverlay::Slot()
				[
					SNew(SBorder).BorderImage(Box(FLinearColor::Transparent, -1.f, A(QC, bDim ? 0.35f : 1.f), Ring)).Padding(0.f)
					.Visibility(EVisibility::HitTestInvisible)
				]
			];
	}

	FName MoodIcon(float W, bool bRefuses)
	{
		if (bRefuses) { return TEXT("angry"); }
		if (W >= 0.95f) { return TEXT("smile"); }
		if (W >= 0.7f) { return TEXT("meh"); }
		return TEXT("frown");
	}
	FLinearColor MoodColor(float W, bool bRefuses)
	{
		if (bRefuses) { return Bad; }
		if (W >= 0.95f) { return Good; }
		if (W >= 0.7f) { return Warn; }
		return Hex(0xFB923C);
	}
	FName BlockIcon(EDCPlanBlock B)
	{
		switch (B)
		{
		case EDCPlanBlock::SlotsEmpty:      return TEXT("user-plus");
		case EDCPlanBlock::CrewUnavailable: return TEXT("user-x");
		case EDCPlanBlock::Refusal:         return TEXT("angry");
		case EDCPlanBlock::NoCash:          return TEXT("wallet");
		case EDCPlanBlock::NoCapacity:      return TEXT("car");
		case EDCPlanBlock::Expired:         return TEXT("timer-off");
		case EDCPlanBlock::Locked:          return TEXT("lock");
		default:                            return TEXT("play");
		}
	}
	FString BlockText(EDCPlanBlock B)
	{
		switch (B)
		{
		case EDCPlanBlock::SlotsEmpty:      return TEXT("Fill every crew slot");
		case EDCPlanBlock::CrewUnavailable: return TEXT("Someone in the crew is busy");
		case EDCPlanBlock::Refusal:         return TEXT("Someone refuses at this cut");
		case EDCPlanBlock::NoCash:          return TEXT("Not enough cash");
		case EDCPlanBlock::NoCapacity:      return TEXT("Too many missions running");
		case EDCPlanBlock::Expired:         return TEXT("No longer available");
		case EDCPlanBlock::Locked:          return TEXT("Locked");
		default:                            return TEXT("Ready");
		}
	}
}

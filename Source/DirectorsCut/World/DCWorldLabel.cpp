#include "World/DCWorldLabel.h"
#include "Components/WidgetComponent.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/CoreStyle.h"
#include "Fonts/CompositeFont.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace
{
	const FLinearColor GlassFill(0.012f, 0.014f, 0.018f, 0.80f);
	const FLinearColor Hairline(1.f, 1.f, 1.f, 0.10f);

	FString FontPath(const TCHAR* File) { return FPaths::ProjectContentDir() / TEXT("Fonts") / File; }

	FSlateFontInfo FileFont(const TCHAR* File, float Size, const char* FallbackStyle)
	{
		static TMap<FString, TSharedPtr<const FCompositeFont>> Cache;
		const FString P = FontPath(File);
		if (const TSharedPtr<const FCompositeFont>* F = Cache.Find(P)) { return FSlateFontInfo(*F, Size); }
		if (FPaths::FileExists(P))
		{
			TSharedPtr<const FCompositeFont> F = MakeShared<FStandaloneCompositeFont>(NAME_None, P, EFontHinting::Default, EFontLoadingPolicy::LazyLoad);
			Cache.Add(P, F);
			return FSlateFontInfo(F, Size);
		}
		return FCoreStyle::GetDefaultFontStyle(FallbackStyle, Size);
	}

	FSlateBrush MakePill(const FLinearColor& Fill, const FLinearColor& Outline, float Radius)
	{
		FSlateBrush B;
		B.DrawAs = ESlateBrushDrawType::RoundedBox;
		B.TintColor = FSlateColor(Fill);
		B.OutlineSettings = FSlateBrushOutlineSettings(FVector4(Radius, Radius, Radius, Radius), FSlateColor(Outline), 1.f);
		return B;
	}
}

FString UDCWorldLabel::LucideGlyph(FName Icon)
{
	static TMap<FName, int32> Codes;
	static bool bLoaded = false;
	if (!bLoaded)
	{
		bLoaded = true;
		FString Json;
		if (FFileHelper::LoadFileToString(Json, *FontPath(TEXT("lucide_codepoints.json"))))
		{
			TSharedPtr<FJsonObject> Obj;
			const TSharedRef<TJsonReader<>> R = TJsonReaderFactory<>::Create(Json);
			if (FJsonSerializer::Deserialize(R, Obj) && Obj.IsValid())
			{
				for (const auto& KV : Obj->Values)
				{
					double V = 0.0;
					if (KV.Value.IsValid() && KV.Value->TryGetNumber(V)) { Codes.Add(FName(*KV.Key), (int32)V); }
				}
			}
		}
	}
	if (Icon.IsNone()) { return FString(); }
	const int32* C = Codes.Find(Icon);
	if (!C) { return FString(); }
	return FString::Chr((TCHAR)*C);
}

FSlateFontInfo UDCWorldLabel::TextFont(float Size) { return FileFont(TEXT("Inter-SemiBold.ttf"), Size, "Bold"); }
FSlateFontInfo UDCWorldLabel::MonoFont(float Size) { return FileFont(TEXT("JetBrainsMono-Variable.ttf"), Size, "Regular"); }
FSlateFontInfo UDCWorldLabel::IconFont(float Size) { return FileFont(TEXT("Lucide.ttf"), Size, "Regular"); }

TSharedRef<SWidget> UDCWorldLabel::RebuildWidget()
{
	TSharedRef<FDCLabelSpec> S = Spec;
	const float Radius = 7.f;
	static const FSlateBrush Pill = MakePill(GlassFill, Hairline, Radius);
	static const FSlateBrush BarBack = MakePill(FLinearColor(1.f, 1.f, 1.f, 0.10f), FLinearColor::Transparent, 1.5f);
	static const FSlateBrush BarFill = MakePill(FLinearColor::White, FLinearColor::Transparent, 1.5f);
	static const FSlateBrush AccentPill = MakePill(GlassFill, FLinearColor::White, Radius);

	auto Hidden = [](bool bShow) { return bShow ? EVisibility::HitTestInvisible : EVisibility::Collapsed; };

	TSharedRef<SWidget> W =
		SNew(SBorder)
		.BorderImage_Lambda([S]() { return S->bAccentOutline ? &AccentPill : &Pill; })
		.BorderBackgroundColor_Lambda([S]() { return FSlateColor(S->bAccentOutline ? FLinearColor(S->Accent.R, S->Accent.G, S->Accent.B, 1.f) : FLinearColor::White); })
		.ColorAndOpacity_Lambda([S]() { return FLinearColor(1.f, 1.f, 1.f, S->Opacity); })
		.Padding(FMargin(8.f, 4.f, 9.f, 4.f))
		.Visibility(EVisibility::HitTestInvisible)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0.f, 0.f, 5.f, 0.f))
				[
					SAssignNew(IconText, STextBlock)
					.Visibility_Lambda([S, Hidden]() { return Hidden(!S->Icon.IsNone()); })
					.Text_Lambda([S]() { return FText::FromString(LucideGlyph(S->Icon)); })
					.ColorAndOpacity_Lambda([S]() { return FSlateColor(S->Accent); })
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SAssignNew(MainText, STextBlock)
					.Visibility_Lambda([S, Hidden]() { return Hidden(!S->Text.IsEmpty()); })
					.Text_Lambda([S]() { return FText::FromString(S->Text); })
					.ColorAndOpacity_Lambda([S]() { return FSlateColor(S->TextColor); })
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(6.f, 1.f, 0.f, 0.f))
				[
					SAssignNew(SubText, STextBlock)
					.Visibility_Lambda([S, Hidden]() { return Hidden(!S->Sub.IsEmpty()); })
					.Text_Lambda([S]() { return FText::FromString(S->Sub); })
					.ColorAndOpacity(FSlateColor(FLinearColor(0.62f, 0.65f, 0.70f)))
				]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 4.f, 0.f, 1.f))
			[
				SNew(SBox)
				.HeightOverride(3.f)
				.MinDesiredWidth(56.f)
				.Visibility_Lambda([S, Hidden]() { return Hidden(S->Progress >= 0.f); })
				[
					SNew(SBorder)
					.BorderImage(&BarBack)
					.Padding(0.f)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().FillWidth(TAttribute<float>::CreateLambda([S]() { return FMath::Clamp(S->Progress, 0.001f, 1.f); }))
						[
							SNew(SBorder).BorderImage(&BarFill).Padding(0.f)
							.BorderBackgroundColor_Lambda([S]() { return FSlateColor(S->Accent); })
						]
						+ SHorizontalBox::Slot().FillWidth(TAttribute<float>::CreateLambda([S]() { return FMath::Clamp(1.f - S->Progress, 0.001f, 1.f); }))
						[
							SNew(SSpacer)
						]
					]
				]
			]
		];
	ApplyFonts();
	return W;
}

void UDCWorldLabel::ApplyFonts()
{
	const float Sz = Spec->Size;
	if (IconText) { IconText->SetFont(IconFont(Sz + 2.f)); }
	if (MainText) { MainText->SetFont(TextFont(Sz)); }
	if (SubText) { SubText->SetFont(MonoFont(Sz - 1.f)); }
}

void UDCWorldLabel::SetSpec(const FDCLabelSpec& InSpec)
{
	const bool bFont = InSpec.Size != Spec->Size;
	*Spec = InSpec;
	if (bFont) { ApplyFonts(); }
}

void UDCWorldLabel::SetLabel(const FString& InText, const FLinearColor& InColor, int32 InFontSize)
{
	FDCLabelSpec S = *Spec;
	S.Text = InText;
	S.Accent = InColor;
	S.Size = (float)InFontSize;
	SetSpec(S);
}

UWidgetComponent* UDCWorldLabel::CreateLabelComponent(AActor* Owner, USceneComponent* Parent, const FVector& RelativeLocation)
{
	if (!Owner || !Parent) { return nullptr; }
	UWidgetComponent* W = NewObject<UWidgetComponent>(Owner);
	W->SetWidgetSpace(EWidgetSpace::Screen);
	W->SetWidgetClass(UDCWorldLabel::StaticClass());
	W->SetDrawAtDesiredSize(true);
	W->SetPivot(FVector2D(0.5f, 1.f));
	W->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	W->SetGenerateOverlapEvents(false);
	W->SetupAttachment(Parent);
	W->SetRelativeLocation(RelativeLocation);
	W->SetUsingAbsoluteRotation(true);
	W->SetUsingAbsoluteScale(true);
	W->RegisterComponent();
	Owner->AddInstanceComponent(W);
	return W;
}

static UDCWorldLabel* GetLabel(UWidgetComponent* Comp)
{
	if (!Comp) { return nullptr; }
	if (!Comp->GetUserWidgetObject()) { Comp->InitWidget(); }
	return Cast<UDCWorldLabel>(Comp->GetUserWidgetObject());
}

void UDCWorldLabel::SetComponentSpec(UWidgetComponent* Comp, const FDCLabelSpec& InSpec)
{
	if (UDCWorldLabel* L = GetLabel(Comp)) { L->SetSpec(InSpec); }
}

void UDCWorldLabel::SetComponentLabel(UWidgetComponent* Comp, const FString& InText, const FLinearColor& InColor, int32 InFontSize)
{
	if (UDCWorldLabel* L = GetLabel(Comp)) { L->SetLabel(InText, InColor, InFontSize); }
}

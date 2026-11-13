#include "World/DCWorldLabel.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Components/WidgetComponent.h"
#include "Styling/CoreStyle.h"

TSharedRef<SWidget> UDCWorldLabel::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		Border = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("LabelBorder"));
		Border->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.62f));
		Border->SetPadding(FMargin(8.f, 2.f, 8.f, 3.f));
		Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("LabelText"));
		Text->SetJustification(ETextJustify::Center);
		Border->SetContent(Text);
		WidgetTree->RootWidget = Border;
		ApplyPending();
	}
	return Super::RebuildWidget();
}

void UDCWorldLabel::ApplyPending()
{
	if (!Text) { return; }
	Text->SetText(FText::FromString(PendingText));
	Text->SetColorAndOpacity(FSlateColor(PendingColor));
	FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("Bold", PendingSize);
	Font.LetterSpacing = 60;
	Text->SetFont(Font);
}

void UDCWorldLabel::SetLabel(const FString& InText, const FLinearColor& InColor, int32 InFontSize)
{
	if (PendingText == InText && PendingColor == InColor && PendingSize == InFontSize && Text) { return; }
	PendingText = InText;
	PendingColor = InColor;
	PendingSize = InFontSize;
	ApplyPending();
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

void UDCWorldLabel::SetComponentLabel(UWidgetComponent* Comp, const FString& InText, const FLinearColor& InColor, int32 InFontSize)
{
	if (!Comp) { return; }
	if (!Comp->GetUserWidgetObject()) { Comp->InitWidget(); }
	if (UDCWorldLabel* L = Cast<UDCWorldLabel>(Comp->GetUserWidgetObject()))
	{
		L->SetLabel(InText, InColor, InFontSize);
	}
}

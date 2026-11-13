// Director's Cut — small screen-space labels floating over the diorama: station names (icon + name), crew name plates,
// badges and build timers. Modern dark-glass pill: Lucide icon, Inter text, JetBrains Mono numbers, optional progress bar.
// Agent E owns.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Fonts/SlateFontInfo.h"
#include "DCWorldLabel.generated.h"

class UWidgetComponent;
class USceneComponent;

struct FDCLabelSpec
{
	FName Icon;                                  // Lucide icon name (NAME_None = no icon)
	FString Text;                                // main text (Inter SemiBold)
	FString Sub;                                 // secondary text (JetBrains Mono, dimmer), e.g. "Lv 3", "1:20"
	FLinearColor Accent = FLinearColor::White;   // icon colour, progress fill, accent dot
	FLinearColor TextColor = FLinearColor(0.93f, 0.94f, 0.96f);
	float Progress = -1.f;                       // 0..1 shows a thin bar under the text; <0 hides it
	float Size = 12.f;                           // text size (icon is a little bigger)
	float Opacity = 1.f;                         // whole label
	bool bAccentOutline = false;                 // accent-tinted hairline (hover / highlight)
};

UCLASS()
class DIRECTORSCUT_API UDCWorldLabel : public UUserWidget
{
	GENERATED_BODY()
public:
	void SetSpec(const FDCLabelSpec& InSpec);
	const FDCLabelSpec& GetSpec() const { return *Spec; }
	/** Legacy convenience: text + colour only. */
	void SetLabel(const FString& InText, const FLinearColor& InColor, int32 InFontSize = 12);

	/** Creates + registers a screen-space widget component showing a UDCWorldLabel, attached to Parent. */
	static UWidgetComponent* CreateLabelComponent(AActor* Owner, USceneComponent* Parent, const FVector& RelativeLocation);
	static void SetComponentSpec(UWidgetComponent* Comp, const FDCLabelSpec& InSpec);
	static void SetComponentLabel(UWidgetComponent* Comp, const FString& InText, const FLinearColor& InColor, int32 InFontSize = 12);

	/** Lucide glyph for an icon name (empty if unknown). Reads Content/Fonts/lucide_codepoints.json once. */
	static FString LucideGlyph(FName Icon);
	/** Fonts from Content/Fonts (raw TTF). */
	static FSlateFontInfo TextFont(float Size);
	static FSlateFontInfo MonoFont(float Size);
	static FSlateFontInfo IconFont(float Size);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	TSharedRef<FDCLabelSpec> Spec = MakeShared<FDCLabelSpec>();
	TSharedPtr<class STextBlock> IconText, MainText, SubText;
	void ApplyFonts();
};

// Director's Cut — tiny screen-space label used for station names and criminal name plates. Agent E owns.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DCWorldLabel.generated.h"

class UBorder;
class UTextBlock;
class UWidgetComponent;
class USceneComponent;

UCLASS()
class DIRECTORSCUT_API UDCWorldLabel : public UUserWidget
{
	GENERATED_BODY()
public:
	void SetLabel(const FString& InText, const FLinearColor& InColor, int32 InFontSize = 12);

	/** Creates + registers a screen-space widget component showing a UDCWorldLabel, attached to Parent. */
	static UWidgetComponent* CreateLabelComponent(AActor* Owner, USceneComponent* Parent, const FVector& RelativeLocation);
	/** Sets the text on a component created by CreateLabelComponent (initializes the widget if needed). */
	static void SetComponentLabel(UWidgetComponent* Comp, const FString& InText, const FLinearColor& InColor, int32 InFontSize = 12);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

	UPROPERTY() TObjectPtr<UBorder> Border;
	UPROPERTY() TObjectPtr<UTextBlock> Text;
	FString PendingText;
	FLinearColor PendingColor = FLinearColor::White;
	int32 PendingSize = 12;
	void ApplyPending();
};

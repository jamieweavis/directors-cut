// Director's Cut — game speed controls (HUD, left end of the bottom dock) and their hotkeys.
// Pause / 1x / 2x / 4x. Keys: Space = pause / resume, 1 / 2 / 3 = 1x / 2x / 4x.
#include "UI/SDCRootInternal.h"
#include "Framework/Application/SlateApplication.h"
#include "InputCoreTypes.h"

namespace
{
	const TCHAR* const SpeedLabel[UDCRunSubsystem::NumGameSpeeds] = { TEXT("1×"), TEXT("2×"), TEXT("4×") };
	const TCHAR* const SpeedName[UDCRunSubsystem::NumGameSpeeds] = { TEXT("Normal speed"), TEXT("Fast"), TEXT("Fastest") };
	const TCHAR* const SpeedKey[UDCRunSubsystem::NumGameSpeeds] = { TEXT("1"), TEXT("2"), TEXT("3") };

	TSharedRef<SToolTip> KeyTip(const FString& Name, const FString& Key, const FString& Body = FString())
	{
		TSharedRef<SVerticalBox> V = SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Txt(Name, Font(11.f, EW::SemiBold), Text)]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(10.f, 0.f, 0.f, 0.f)
				[Pill(Txt(Key, Mono(9.f), TextDim), A(FLinearColor::White, 0.06f), FMargin(6.f, 1.f), Line)]
			];
		if (!Body.IsEmpty()) { V->AddSlot().AutoHeight().Padding(0.f, 4.f, 0.f, 0.f)[Txt(Body, Font(10.f), TextDim, true)]; }
		return TipWidget(V);
	}
}

bool SDCRoot::IsTextInputFocused()
{
	if (!FSlateApplication::IsInitialized()) { return false; }
	const TSharedPtr<SWidget> W = FSlateApplication::Get().GetKeyboardFocusedWidget();
	if (!W.IsValid()) { return false; }
	const FString T = W->GetTypeAsString();
	return T.Contains(TEXT("EditableText")) || T.Contains(TEXT("MultiLineEditable")) || T.Contains(TEXT("SpinBox"));
}

bool SDCRoot::IsSpeedPausedNow() const
{
	const UDCRunSubsystem* R = Run();
	return R && R->HasActiveRun() && R->IsSpeedPaused();
}

void SDCRoot::SetSpeed(int32 SpeedIndex)
{
	UDCRunSubsystem* R = Run();
	if (!R || !IsPlaying()) { return; }
	if (R->GetGameSpeed() == SpeedIndex && !R->IsSpeedPaused()) { return; }
	R->SetGameSpeed(SpeedIndex);
	Sfx(DCSfx::Click);
}

void SDCRoot::ToggleSpeedPause()
{
	UDCRunSubsystem* R = Run();
	if (!R || !IsPlaying()) { return; }
	R->ToggleSpeedPaused();
	Sfx(DCSfx::Click);
}

bool SDCRoot::HandleSpeedKey(const FKey& Key)
{
	if (Key == EKeys::SpaceBar) { ToggleSpeedPause(); return true; }
	int32 Speed = INDEX_NONE;
	if (Key == EKeys::One || Key == EKeys::NumPadOne) { Speed = 0; }
	else if (Key == EKeys::Two || Key == EKeys::NumPadTwo) { Speed = 1; }
	else if (Key == EKeys::Three || Key == EKeys::NumPadThree) { Speed = 2; }
	if (Speed == INDEX_NONE) { return false; }
	SetSpeed(Speed);
	return true;
}

TSharedRef<SWidget> SDCRoot::BuildHudSpeed()
{
	auto R = [this]() { return Run(); };
	auto Paused = [this]() { return IsSpeedPausedNow(); };

	// One segment: a highlight layer under a ghost button. bOn = lit (accent), bHeld = the speed a pause will resume.
	auto Segment = [](TSharedRef<SWidget> Content, TFunction<bool()> On, TFunction<bool()> Held, TFunction<void()> Click, TSharedRef<SToolTip> TipW)
	{
		return SNew(SOverlay)
			+ SOverlay::Slot()
			[
				SNew(SBorder).Visibility(EVisibility::HitTestInvisible).Padding(0.f)
				.BorderImage_Lambda([On, Held]()
				{
					if (On()) { return Box(A(Accent, 0.18f), 6.f, A(Accent, 0.75f), 1.f); }
					if (Held()) { return Box(FLinearColor::Transparent, 6.f, A(Accent, 0.35f), 1.f); }
					return NoBrush();
				})
			]
			+ SOverlay::Slot()
			[
				Button(SNew(SBox).WidthOverride(26.f).HeightOverride(22.f).HAlign(HAlign_Center).VAlign(VAlign_Center)[Content],
					Click, EBtn::Ghost, true, TipW, FMargin(1.f))
			];
	};

	TSharedRef<SHorizontalBox> H = SNew(SHorizontalBox);
	H->AddSlot().AutoWidth().VAlign(VAlign_Center)
	[
		Segment(IconA(TAttribute<FName>::CreateLambda([Paused]() { return FName(Paused() ? TEXT("play") : TEXT("pause")); }), 12.f,
				TAttribute<FSlateColor>::CreateLambda([Paused]() { return FSlateColor(Paused() ? Accent : TextDim); })),
			Paused, []() { return false; }, [this]() { ToggleSpeedPause(); },
			KeyTip(TEXT("Pause / resume"), TEXT("SPACE"), TEXT("Stops the clock. Plan, build and hire while time stands still.")))
	];
	for (int32 i = 0; i < UDCRunSubsystem::NumGameSpeeds; ++i)
	{
		auto Sel = [R, i]() { const UDCRunSubsystem* X = R(); return X && X->GetGameSpeed() == i; };
		auto On = [Sel, Paused]() { return Sel() && !Paused(); };
		auto Held = [Sel, Paused]() { return Sel() && Paused(); };
		H->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(2.f, 0.f, 0.f, 0.f)
		[
			Segment(TxtA(FText::FromString(SpeedLabel[i]), Mono(10.5f), TAttribute<FSlateColor>::CreateLambda([On, Held]() { return FSlateColor(On() ? Accent : (Held() ? Text : TextDim)); })),
				On, Held, [this, i]() { SetSpeed(i); }, KeyTip(SpeedName[i], SpeedKey[i]))
		];
	}

	return SNew(SBorder)
		.BorderImage_Lambda([Paused]() { return Box(Hex(0x0A0C0F, 0.85f), 8.f, Paused() ? A(Accent, 0.55f) : Line, 1.f); })
		.Padding(2.f)
		[H];
}

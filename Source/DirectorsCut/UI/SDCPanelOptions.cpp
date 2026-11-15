// Director's Cut — v3 screens: options (audio / graphics / accessibility / gameplay), save slots, leaderboard,
// and the shared confirm dialog. All of them are sub-views of the Title, Pause and end screens (Agent C).
#include "UI/SDCRootInternal.h"
#include "GameFramework/GameUserSettings.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/SlateRenderer.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Engine/Engine.h"
#include "DirectorsCut.h"

// ─────────────────────────────────────────────────────────────────────────────
// Graphics state (pending edits until Apply)
// ─────────────────────────────────────────────────────────────────────────────

namespace
{
	constexpr int32 NumQ = 10;
	const TCHAR* QName[NumQ] = { TEXT("View distance"), TEXT("Shadows"), TEXT("Global illumination"), TEXT("Reflections"), TEXT("Post-processing"),
		TEXT("Textures"), TEXT("Effects"), TEXT("Foliage"), TEXT("Shading"), TEXT("Anti-aliasing") };
	const TCHAR* QIcon[NumQ] = { TEXT("eye"), TEXT("cloud-sun"), TEXT("lightbulb"), TEXT("sparkle"), TEXT("aperture"),
		TEXT("image"), TEXT("sparkles"), TEXT("trees"), TEXT("palette"), TEXT("scan-line") };

	int32 GetQ(const UGameUserSettings* S, int32 i)
	{
		switch (i)
		{
		case 0: return S->GetViewDistanceQuality();
		case 1: return S->GetShadowQuality();
		case 2: return S->GetGlobalIlluminationQuality();
		case 3: return S->GetReflectionQuality();
		case 4: return S->GetPostProcessingQuality();
		case 5: return S->GetTextureQuality();
		case 6: return S->GetVisualEffectQuality();
		case 7: return S->GetFoliageQuality();
		case 8: return S->GetShadingQuality();
		default: return S->GetAntiAliasingQuality();
		}
	}
	void SetQ(UGameUserSettings* S, int32 i, int32 V)
	{
		switch (i)
		{
		case 0: S->SetViewDistanceQuality(V); break;
		case 1: S->SetShadowQuality(V); break;
		case 2: S->SetGlobalIlluminationQuality(V); break;
		case 3: S->SetReflectionQuality(V); break;
		case 4: S->SetPostProcessingQuality(V); break;
		case 5: S->SetTextureQuality(V); break;
		case 6: S->SetVisualEffectQuality(V); break;
		case 7: S->SetFoliageQuality(V); break;
		case 8: S->SetShadingQuality(V); break;
		default: S->SetAntiAliasingQuality(V); break;
		}
	}
	UGameUserSettings* GUS() { return GEngine ? GEngine->GetGameUserSettings() : nullptr; }

	const float ResScales[] = { 0.f, 50.f, 67.f, 75.f, 85.f, 100.f };
	const float FpsLimits[] = { 30.f, 60.f, 120.f, 144.f, 0.f };

	FString Hms(double Seconds)
	{
		const int64 S = FMath::Max<int64>(0, (int64)FMath::RoundToDouble(Seconds));
		return FString::Printf(TEXT("%02lld:%02lld:%02lld"), S / 3600, (S / 60) % 60, S % 60);
	}
	FString LocalDate(const FDateTime& Utc, bool bTime)
	{
		if (Utc.GetTicks() <= 0) { return TEXT("—"); }
		static const TCHAR* Mon[12] = { TEXT("Jan"), TEXT("Feb"), TEXT("Mar"), TEXT("Apr"), TEXT("May"), TEXT("Jun"), TEXT("Jul"), TEXT("Aug"), TEXT("Sep"), TEXT("Oct"), TEXT("Nov"), TEXT("Dec") };
		const FDateTime L = Utc + (FDateTime::Now() - FDateTime::UtcNow());
		FString Out = FString::Printf(TEXT("%s %d"), Mon[FMath::Clamp(L.GetMonth() - 1, 0, 11)], L.GetDay());
		if (L.GetYear() != FDateTime::Now().GetYear()) { Out += FString::Printf(TEXT(", %d"), L.GetYear()); }
		if (bTime) { Out += FString::Printf(TEXT(" · %02d:%02d"), L.GetHour(), L.GetMinute()); }
		return Out;
	}

	// ── Controls ─────────────────────────────────────────────────────────────

	TSharedRef<SWidget> Switch(TFunction<bool()> Get, TFunction<void(bool)> Set, TAttribute<bool> Enabled = true)
	{
		auto Knob = [](const FLinearColor& C) { return SNew(SBox).WidthOverride(16.f).HeightOverride(16.f)[SNew(SBorder).BorderImage(Box(C, -1.f)).Padding(0.f)]; };
		return Button(
			SNew(SBox).WidthOverride(40.f).HeightOverride(22.f)
			[
				SNew(SBorder)
				.BorderImage_Lambda([Get]() { return Get() ? Box(Accent, -1.f) : Box(Hex(0x0A0C0F, 0.9f), -1.f, LineHi, 1.f); })
				.Padding(FMargin(3.f))
				[
					SNew(SOverlay)
					+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Center)[SNew(SBox).Visibility_Lambda([Get]() { return Get() ? EVisibility::Hidden : EVisibility::HitTestInvisible; })[Knob(TextDim)]]
					+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Center)[SNew(SBox).Visibility_Lambda([Get]() { return Get() ? EVisibility::HitTestInvisible : EVisibility::Hidden; })[Knob(AccentInk)]]
				]
			], [Get, Set]() { Set(!Get()); }, EBtn::Ghost, Enabled, nullptr, FMargin(2.f));
	}

	TSharedRef<SWidget> Segmented(const TArray<FString>& Labels, TFunction<int32()> Get, TFunction<void(int32)> Set, TAttribute<bool> Enabled = true)
	{
		TSharedRef<SHorizontalBox> H = SNew(SHorizontalBox);
		for (int32 i = 0; i < Labels.Num(); ++i)
		{
			H->AddSlot().FillWidth(1.f).Padding(i ? 2.f : 0.f, 0.f, 0.f, 0.f)
			[
				SNew(SOverlay)
				+ SOverlay::Slot()
				[SNew(SBorder).Visibility(EVisibility::HitTestInvisible).BorderImage_Lambda([Get, i]() { return Get() == i ? Box(A(Accent, 0.16f), 6.f, A(Accent, 0.7f), 1.f) : NoBrush(); })]
				+ SOverlay::Slot()
				[
					Button(SNew(SBox).HAlign(HAlign_Center)
						[TxtA(FText::FromString(Labels[i]), Font(10.f, EW::Medium), TAttribute<FSlateColor>::CreateLambda([Get, i]() { return FSlateColor(Get() == i ? Accent : TextDim); }))],
						[Set, i]() { Set(i); }, EBtn::Ghost, Enabled, nullptr, FMargin(6.f, 5.f))
				]
			];
		}
		return SNew(SBorder).BorderImage(Box(Hex(0x0A0C0F, 0.85f), 8.f, Line, 1.f)).Padding(3.f)[H];
	}

	TSharedRef<SWidget> Stepper(TFunction<FString()> Value, TFunction<void(int32)> Step, TAttribute<bool> Enabled = true)
	{
		return SNew(SBorder).BorderImage(Box(Hex(0x0A0C0F, 0.85f), 8.f, Line, 1.f)).Padding(3.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth()[IconButton(TEXT("chevron-left"), [Step]() { Step(-1); }, nullptr, EBtn::Ghost, 12.f, Enabled)]
			+ SHorizontalBox::Slot().FillWidth(1.f).HAlign(HAlign_Center).VAlign(VAlign_Center)
			[TxtA(TAttribute<FText>::CreateLambda([Value]() { return FText::FromString(Value()); }), Mono(11.f), TAttribute<FSlateColor>::CreateLambda([Enabled]() { return FSlateColor(Enabled.Get() ? Text : TextFaint); }))]
			+ SHorizontalBox::Slot().AutoWidth()[IconButton(TEXT("chevron-right"), [Step]() { Step(1); }, nullptr, EBtn::Ghost, 12.f, Enabled)]
		];
	}

	TSharedRef<SWidget> SliderCtl(TFunction<float()> Get01, TFunction<void(float)> Set01, TFunction<FString()> ValueText, TFunction<void()> OnEnd = nullptr, TAttribute<bool> Enabled = true)
	{
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
			[
				SNew(SBox).HeightOverride(24.f)
				[
					SNew(SOverlay)
					+ SOverlay::Slot().VAlign(VAlign_Center).Padding(FMargin(9.f, 0.f))[Bar(TAttribute<float>::CreateLambda(Get01), FSlateColor(Accent), 4.f)]
					+ SOverlay::Slot()
					[
						SNew(SSlider).Style(&SliderStyle()).IsFocusable(false).MinValue(0.f).MaxValue(1.f).IsEnabled(Enabled)
						.Value_Lambda([Get01]() { return Get01(); })
						.OnValueChanged_Lambda([Set01](float V) { Set01(V); })
						.OnMouseCaptureEnd_Lambda([OnEnd]() { Sfx(DCSfx::Click); if (OnEnd) { OnEnd(); } })
					]
				]
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(10.f, 0.f, 0.f, 0.f)
			[SNew(SBox).WidthOverride(46.f).HAlign(HAlign_Right)[TxtA(TAttribute<FText>::CreateLambda([ValueText]() { return FText::FromString(ValueText()); }), Mono(10.f), FSlateColor(Text))]];
	}

	TSharedRef<SWidget> Row(FName IconName, const FString& Name, const FString& Desc, TSharedRef<SWidget> Control, float ControlW = 330.f)
	{
		TSharedRef<SVerticalBox> L = SNew(SVerticalBox) + SVerticalBox::Slot().AutoHeight()[Txt(Name, Font(11.f, EW::Medium), Text)];
		if (!Desc.IsEmpty()) { L->AddSlot().AutoHeight().Padding(0.f, 2.f, 0.f, 0.f)[Txt(Desc, Font(9.f), TextFaint, true)]; }
		return SNew(SBox).Padding(FMargin(0.f, 7.f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 12.f, 0.f)[SNew(SBox).WidthOverride(18.f).HAlign(HAlign_Center)[Icon(IconName, 13.f, TextDim)]]
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center).Padding(0.f, 0.f, 16.f, 0.f)[L]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[SNew(SBox).WidthOverride(ControlW)[Control]]
		];
	}
	TSharedRef<SWidget> Section(const FString& Name)
	{
		return SNew(SBox).Padding(FMargin(0.f, 14.f, 0.f, 4.f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 10.f, 0.f)[Label(Name, Accent, 8.f)]
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[HLine()]
		];
	}
	TSharedRef<SWidget> SwitchRight(TFunction<bool()> Get, TFunction<void(bool)> Set)
	{
		return SNew(SBox).HAlign(HAlign_Right)[Switch(Get, Set)];
	}
	/** Read-only key caps, right-aligned (control listings). */
	TSharedRef<SWidget> Keys(const TArray<FString>& KeyCaps)
	{
		TSharedRef<SHorizontalBox> H = SNew(SHorizontalBox);
		for (int32 i = 0; i < KeyCaps.Num(); ++i)
		{
			H->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(i ? 4.f : 0.f, 0.f, 0.f, 0.f)
			[Pill(Txt(KeyCaps[i], Mono(9.5f), Text), A(FLinearColor::White, 0.06f), FMargin(8.f, 2.f), Line)];
		}
		return SNew(SBox).HAlign(HAlign_Right)[H];
	}
	auto BoolKey(const TCHAR* Key, bool bDefault) { return [Key, bDefault]() { return DCSettings::GetBool(Key, bDefault); }; }
}

struct SDCRoot::FGfx
{
	int32 Mode = (int32)EWindowMode::Windowed;
	FIntPoint Res = FIntPoint(1600, 900);
	bool bVSync = false;
	float FpsLimit = 0.f;
	int32 Q[NumQ] = { 3, 3, 3, 3, 3, 3, 3, 3, 3, 3 };
	float ResScale = 0.f;
	bool operator==(const FGfx& O) const
	{
		if (Mode != O.Mode || Res != O.Res || bVSync != O.bVSync || !FMath::IsNearlyEqual(FpsLimit, O.FpsLimit) || !FMath::IsNearlyEqual(ResScale, O.ResScale, 0.5f)) { return false; }
		for (int32 i = 0; i < NumQ; ++i) { if (Q[i] != O.Q[i]) { return false; } }
		return true;
	}
	int32 Overall() const
	{
		for (int32 i = 1; i < NumQ; ++i) { if (Q[i] != Q[0]) { return -1; } }
		return Q[0];
	}
	static FGfx FromEngine()
	{
		FGfx G;
		if (const UGameUserSettings* S = GUS())
		{
			G.Mode = (int32)S->GetFullscreenMode();
			G.Res = S->GetScreenResolution();
			G.bVSync = S->IsVSyncEnabled();
			G.FpsLimit = S->GetFrameRateLimit();
			for (int32 i = 0; i < NumQ; ++i) { G.Q[i] = FMath::Clamp(GetQ(S, i), 0, 4); }
			float Norm = 0.f, Val = 0.f, Min = 0.f, Max = 100.f;
			S->GetResolutionScaleInformationEx(Norm, Val, Min, Max);
			G.ResScale = Val;
		}
		return G;
	}
};

void SDCRoot::LoadGfxPending()
{
	GfxPending = MakeShared<FGfx>(FGfx::FromEngine());
}

bool SDCRoot::IsGfxDirty() const
{
	return GfxPending.IsValid() && !(*GfxPending == FGfx::FromEngine());
}

void SDCRoot::ApplyGfxPending()
{
	UGameUserSettings* S = GUS();
	if (!S || !GfxPending.IsValid()) { return; }
	const FGfx Old = FGfx::FromEngine();
	const FGfx& G = *GfxPending;
	const bool bVideoMode = Old.Mode != G.Mode || Old.Res != G.Res;
	S->SetFullscreenMode((EWindowMode::Type)G.Mode);
	S->SetScreenResolution(G.Res);
	S->SetVSyncEnabled(G.bVSync);
	S->SetFrameRateLimit(G.FpsLimit);
	for (int32 i = 0; i < NumQ; ++i) { SetQ(S, i, G.Q[i]); }
	S->SetResolutionScaleValueEx(G.ResScale);
	S->ApplySettings(false);
	Sfx(DCSfx::Confirm);
	if (!bVideoMode)
	{
		S->ConfirmVideoMode();
		S->SaveSettings();
		Toast(TEXT("Graphics applied"), TEXT("monitor-check"), false);
		Rebuild();
		return;
	}
	// Display changes can leave the player with a black or unusable screen: keep only if they confirm in time.
	Ask(TEXT("Keep these display settings?"), FString::Printf(TEXT("%s · %d × %d"), G.Mode == 0 ? TEXT("Fullscreen") : (G.Mode == 1 ? TEXT("Borderless") : TEXT("Windowed")), G.Res.X, G.Res.Y),
		TEXT("monitor"), TEXT("Keep"),
		[this]()
		{
			if (UGameUserSettings* X = GUS()) { X->ConfirmVideoMode(); X->SaveSettings(); }
			LoadGfxPending();
			Rebuild();
		}, false, 10.0,
		[this]()
		{
			if (UGameUserSettings* X = GUS()) { X->RevertVideoMode(); X->ApplyResolutionSettings(false); X->SaveSettings(); }
			LoadGfxPending();
			Rebuild();
			Toast(TEXT("Display settings reverted"), TEXT("undo-2"), false);
		});
}

// ─────────────────────────────────────────────────────────────────────────────
// Player settings the UI applies itself
// ─────────────────────────────────────────────────────────────────────────────

void SDCRoot::ApplyPlayerSettings(bool bRebuild)
{
	DCUI::RefreshPrefs();
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication& App = FSlateApplication::Get();
		if (FSlateRenderer* Rend = App.GetRenderer())
		{
			const int32 Mode = DCSettings::ColorVisionMode();
			const EColorVisionDeficiency T = Mode == 1 ? EColorVisionDeficiency::Protanope : (Mode == 2 ? EColorVisionDeficiency::Deuteranope : (Mode == 3 ? EColorVisionDeficiency::Tritanope : EColorVisionDeficiency::NormalVision));
			Rend->SetColorVisionDeficiencyType(T, FMath::Clamp(DCSettings::ColorVisionSeverity(), 1, 10), true, false);
		}
	}
	FApp::SetVolumeMultiplier(DCSettings::MuteAll() ? 0.f : 1.f);
	FApp::SetUnfocusedVolumeMultiplier(DCSettings::MuteWhenUnfocused() ? 0.f : 1.f);
	if (bRebuild) { RebuildAll(); }
}

void SDCRoot::SetDevView(const FString& V)
{
	if (V.IsEmpty() || V == TEXT("-")) { return; }
	if (V.StartsWith(TEXT("options")))
	{
		OptTab = V.EndsWith(TEXT("graphics")) ? 1 : (V.EndsWith(TEXT("access")) ? 2 : (V.EndsWith(TEXT("gameplay")) ? 3 : 0));
		SetView(EView::Options);
	}
	else if (V == TEXT("slots.new")) { SetView(EView::SlotsNew); }
	else if (V == TEXT("slots.load")) { SetView(EView::SlotsLoad); }
	else if (V == TEXT("slots.save")) { SetView(EView::SlotsSave); }
	else if (V == TEXT("confirm")) { Ask(TEXT("Overwrite slot 2?"), TEXT("Day 4 · 14:20 · $18,400 will be replaced. This can't be undone."), TEXT("save"), TEXT("Overwrite"), []() {}, true); }
	else if (V == TEXT("keep")) { Ask(TEXT("Keep these display settings?"), TEXT("Windowed · 1600 × 900"), TEXT("monitor"), TEXT("Keep"), []() {}, false, 10.0, []() {}); }
	else if (V == TEXT("hc")) { DCSettings::SetBool(TEXT("HighContrast"), true); ApplyPlayerSettings(true); }
}

bool SDCRoot::DevAction(const FString& Verb, const FString& Arg)
{
	UDCRunSubsystem* R = Run();
	const int32 Index = FCString::Atoi(*Arg);
	if (Verb == TEXT("view")) { if (Arg == TEXT("none")) { SetView(EView::None); } else { SetDevView(Arg); } return true; }
	if (Verb == TEXT("autofill")) { if (PlanMissionId == INDEX_NONE) { return false; } AutoFill(); return true; }
	if (Verb == TEXT("start")) { if (PlanMissionId == INDEX_NONE) { return false; } StartPlan(); return true; }
	if (Verb == TEXT("cut")) { PlanCut = FMath::Clamp(FCString::Atof(*Arg), 0.f, 1.f); Sfx(DCSfx::Click); RecomputePlan(); Rebuild(); return true; }
	if (Verb == TEXT("crew") || Verb == TEXT("gear"))
	{
		if (!R || !R->GetRoster().IsValidIndex(Index)) { return false; }
		(Verb == TEXT("crew") ? CrewSel : GearSel) = R->GetRoster()[Index].Id;
		DismissArmed = INDEX_NONE;
		Sfx(DCSfx::Click); Rebuild();
		return true;
	}
	if (Verb == TEXT("perk"))
	{
		const TArray<FDCPerk> O = R ? R->GetPerkOffer() : TArray<FDCPerk>();
		if (!O.IsValidIndex(Index)) { return false; }
		PerkSel = O[Index].Id; Sfx(DCSfx::Click); Rebuild();
		return true;
	}
	if (Verb == TEXT("pickperk"))
	{
		if (!R || PerkSel == NAME_None || !R->PickPerk(PerkSel)) { return false; }
		PerkSel = NAME_None; Sfx(DCSfx::Perk); Close(true);
		return true;
	}
	if (Verb == TEXT("close")) { if (Confirm.IsValid()) { EndConfirm(false); } SetView(EView::None); Close(true); return true; }
	if (Verb == TEXT("hud")) { SetVisibility(Arg == TEXT("off") ? EVisibility::Collapsed : EVisibility::SelfHitTestInvisible); return true; }
	if (Verb == TEXT("speed")) { SetSpeed(FMath::Clamp(Index, 0, UDCRunSubsystem::NumGameSpeeds - 1)); return true; }
	if (Verb == TEXT("titleintro")) { ReplayTitleIntro(); return true; }
	return false;
}

// ─────────────────────────────────────────────────────────────────────────────
// Confirm dialog
// ─────────────────────────────────────────────────────────────────────────────

void SDCRoot::Ask(const FString& Title, const FString& Body, FName IconName, const FString& Yes, TFunction<void()> OnYes, bool bDanger, double Timeout, TFunction<void()> OnNo)
{
	Confirm = MakeShared<FConfirm>();
	Confirm->Title = Title; Confirm->Body = Body; Confirm->Icon = IconName; Confirm->Yes = Yes;
	Confirm->No = Timeout > 0.0 ? TEXT("Revert") : TEXT("Cancel");
	Confirm->bDanger = bDanger; Confirm->Timeout = Timeout; Confirm->T0 = Now();
	Confirm->OnYes = MoveTemp(OnYes); Confirm->OnNo = MoveTemp(OnNo);
	ConfirmSlot->SetContent(BuildConfirm());
	if (!bConfirmShown) { ConfirmFadeT0 = Now(); }
	bConfirmShown = true;
	Sfx(DCSfx::PanelOpen, 0.6f);
}

void SDCRoot::EndConfirm(bool bYes)
{
	if (!Confirm.IsValid()) { return; }
	TSharedPtr<FConfirm> C = Confirm;
	Confirm.Reset();
	bConfirmShown = false;
	ConfirmFadeT0 = Now();
	if (bYes) { if (C->OnYes) { C->OnYes(); } }
	else { Sfx(DCSfx::PanelClose, 0.6f); if (C->OnNo) { C->OnNo(); } }
}

TSharedRef<SWidget> SDCRoot::BuildConfirm()
{
	if (!Confirm.IsValid()) { return SNullWidget::NullWidget; }
	const TSharedPtr<FConfirm> C = Confirm;
	const FLinearColor Col = C->bDanger ? Bad : Accent;
	TSharedRef<SVerticalBox> V = SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top).Padding(0.f, 0.f, 14.f, 0.f)[IconTile(C->Icon.IsNone() ? FName(TEXT("circle-help")) : C->Icon, 40.f, Col)]
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()[Txt(C->Title, Font(15.f, EW::SemiBold), Text, true)]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f, 0.f, 0.f)[Txt(C->Body, Font(10.5f), TextDim, true)]
			]
		];
	if (C->Timeout > 0.0)
	{
		const double T0 = C->T0, Len = C->Timeout;
		V->AddSlot().AutoHeight().Padding(0.f, 16.f, 0.f, 0.f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[TxtA(TAttribute<FText>::CreateLambda([T0, Len]() { return FText::FromString(FString::Printf(TEXT("Reverting in %d s"), FMath::Max(0, (int32)FMath::CeilToDouble(Len - (Now() - T0))))); }), Font(10.f, EW::Medium), FSlateColor(Accent))]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f, 0.f, 0.f)
			[Bar(TAttribute<float>::CreateLambda([T0, Len]() { return FMath::Clamp(1.f - (float)((Now() - T0) / Len), 0.f, 1.f); }), FSlateColor(Accent), 3.f)]
		];
	}
	V->AddSlot().AutoHeight().Padding(0.f, 20.f, 0.f, 0.f).HAlign(HAlign_Right)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 8.f, 0.f)[TextButton(C->Timeout > 0.0 ? TEXT("undo-2") : TEXT("x"), C->No, [this]() { EndConfirm(false); }, EBtn::Secondary, true, Tip(C->No, TEXT("Esc")), 11.f)]
		+ SHorizontalBox::Slot().AutoWidth()[TextButton(C->bDanger ? TEXT("triangle-alert") : TEXT("check"), C->Yes, [this]() { EndConfirm(true); }, C->bDanger ? EBtn::Danger : EBtn::Primary, true, nullptr, 11.f)]
	];
	return GlassPanel(SNew(SBox).WidthOverride(440.f)[V], FMargin(24.f, 22.f));
}

// ─────────────────────────────────────────────────────────────────────────────
// Sub-views
// ─────────────────────────────────────────────────────────────────────────────

void SDCRoot::SetView(EView V)
{
	if (V == EView::Options && View != EView::Options) { LoadGfxPending(); }
	if (View == EView::Options && V != EView::Options && IsGfxDirty()) { LoadGfxPending(); } // unapplied graphics edits are dropped
	const bool bChanged = V != View;
	View = V;
	bClearBoardArmed = false;
	if (bChanged) { Sfx(V == EView::None ? DCSfx::PanelClose : DCSfx::PanelOpen, 0.7f); }
	SavedScroll.Reset(); Scrolls.Reset();
	if (Current != EDCPanel::None) { ModalSlot->SetContent(BuildPanel(Current)); LastSig = ComputeSignature(); }
}

TSharedRef<SWidget> SDCRoot::BuildView()
{
	switch (View)
	{
	case EView::Options:   return BuildOptions();
	case EView::SlotsNew:
	case EView::SlotsLoad:
	case EView::SlotsSave: return BuildSlots(View);
	default:               return SNullWidget::NullWidget;
	}
}

// ── Options ──────────────────────────────────────────────────────────────────

TSharedRef<SWidget> SDCRoot::BuildOptions()
{
	if (!GfxPending.IsValid()) { LoadGfxPending(); }
	static const FName TabIcon[4] = { TEXT("volume-2"), TEXT("monitor"), TEXT("accessibility"), TEXT("mouse") };
	static const TCHAR* TabName[4] = { TEXT("Audio"), TEXT("Graphics"), TEXT("Accessibility"), TEXT("Gameplay") };

	TSharedRef<SHorizontalBox> Tabs = SNew(SHorizontalBox);
	for (int32 i = 0; i < 4; ++i)
	{
		const bool bOn = OptTab == i;
		Tabs->AddSlot().AutoWidth().Padding(0.f, 0.f, 4.f, 0.f)
		[
			SNew(SOverlay)
			+ SOverlay::Slot()[SNew(SBorder).Visibility(EVisibility::HitTestInvisible).BorderImage(bOn ? Box(A(Accent, 0.14f), 8.f, A(Accent, 0.6f), 1.f) : NoBrush())]
			+ SOverlay::Slot()
			[
				Button(IconText(TabIcon[i], TabName[i], Font(11.f, EW::SemiBold), bOn ? Accent : TextDim, bOn ? Text : TextDim, 13.f, 8.f),
					[this, i]() { if (OptTab != i) { OptTab = i; SavedScroll.Reset(); Scrolls.Reset(); ModalSlot->SetContent(BuildPanel(Current)); } }, EBtn::Ghost, true, nullptr, FMargin(14.f, 8.f))
			]
		];
	}

	TSharedRef<SWidget> Body = OptTab == 0 ? OptAudio() : (OptTab == 1 ? OptGraphics() : (OptTab == 2 ? OptAccess() : OptGameplay()));

	// Footer: graphics has Apply / Revert / Auto-detect; everything else saves as you go.
	TSharedRef<SWidget> Footer = SNullWidget::NullWidget;
	if (OptTab == 1)
	{
		auto Dirty = [this]() { return IsGfxDirty(); };
		Footer = SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[TextButton(TEXT("gauge"), TEXT("Auto-detect"), [this]()
				{
					if (UGameUserSettings* S = GUS())
					{
						S->RunHardwareBenchmark();
						S->ApplyHardwareBenchmarkResults();
						LoadGfxPending();
						const int32 O = GfxPending->Overall();
						static const TCHAR* N[5] = { TEXT("Low"), TEXT("Medium"), TEXT("High"), TEXT("Epic"), TEXT("Max") };
						Toast(FString::Printf(TEXT("Detected · %s"), O >= 0 ? N[O] : TEXT("Custom")), TEXT("gauge"), false);
						Rebuild();
					}
				}, EBtn::Secondary, true, Tip(TEXT("Auto-detect"), TEXT("Benchmarks this machine and picks matching quality levels. Applies straight away."), TEXT("gauge")), 10.5f)]
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center).Padding(14.f, 0.f)
			[SNew(SBox).Visibility_Lambda([Dirty]() { return Dirty() ? EVisibility::Visible : EVisibility::Hidden; })[IconText(TEXT("circle-dot"), TEXT("Unapplied changes"), Font(10.f, EW::Medium), Accent, TextDim, 10.f, 6.f)]]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 8.f, 0.f)
			[TextButton(TEXT("undo-2"), TEXT("Revert"), [this]() { LoadGfxPending(); Rebuild(); }, EBtn::Secondary, TAttribute<bool>::CreateLambda(Dirty), nullptr, 10.5f)]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[TextButton(TEXT("check"), TEXT("Apply"), [this]() { ApplyGfxPending(); }, EBtn::Primary, TAttribute<bool>::CreateLambda(Dirty), nullptr, 10.5f)];
	}
	else
	{
		Footer = SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[IconText(TEXT("save"), TEXT("Changes save automatically"), Font(10.f), TextFaint, TextFaint, 10.f, 6.f)]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[TextButton(TEXT("rotate-ccw"), TEXT("Defaults"), [this]()
				{
					if (OptTab == 0)
					{
						if (UDCAudioSubsystem* Au = UDCAudioSubsystem::Get(GetContext())) { Au->SetVolume(EDCVolumeChannel::Master, 1.f); Au->SetVolume(EDCVolumeChannel::SFX, 1.f); Au->SetVolume(EDCVolumeChannel::Ambience, 0.8f); }
						DCSettings::SetBool(TEXT("MuteAll"), false); DCSettings::SetBool(TEXT("MuteWhenUnfocused"), true);
					}
					else if (OptTab == 2)
					{
						DCSettings::SetFloat(TEXT("UIScale"), 1.f); DCSettings::SetFloat(TEXT("TextScale"), 1.f); DCSettings::SetInt(TEXT("ColorVision"), 0);
						DCSettings::SetInt(TEXT("ColorVisionSeverity"), 8); DCSettings::SetBool(TEXT("HighContrast"), false); DCSettings::SetBool(TEXT("ReduceMotion"), false);
						DCSettings::SetBool(TEXT("ReduceFlashing"), false); DCSettings::SetBool(TEXT("ShowCrewNames"), true); DCSettings::SetBool(TEXT("ExpiryWarnings"), true);
						DCSettings::SetBool(TEXT("PauseOnFocusLoss"), true);
					}
					else
					{
						DCSettings::SetBool(TEXT("EdgeScroll"), false); DCSettings::SetFloat(TEXT("CameraPanSpeed"), 1.f); DCSettings::SetFloat(TEXT("CameraZoomSpeed"), 1.f);
						DCSettings::SetBool(TEXT("DebugView"), false);
					}
					ApplyPlayerSettings(true);
				}, EBtn::Ghost, true, Tip(TEXT("Restore defaults"), TEXT("Resets this tab only")), 10.5f)];
	}

	TSharedRef<SWidget> CardW = GlassPanel(
		SNew(SBox).WidthOverride(800.f).HeightOverride(640.f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 12.f, 0.f)[IconTile(TEXT("settings"), 38.f, Accent)]
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()[Txt(TEXT("Options"), Font(17.f, EW::SemiBold), Text)]
					+ SVerticalBox::Slot().AutoHeight()[Txt(TEXT("Sound, display and comfort"), Font(10.f), TextDim)]
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[IconButton(TEXT("x"), [this]() { SetView(EView::None); }, Tip(TEXT("Back"), TEXT("Esc")), EBtn::Ghost, 15.f)]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 16.f, 0.f, 0.f)[Tabs]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 10.f, 0.f, 4.f)[HLine()]
			+ SVerticalBox::Slot().FillHeight(1.f)[Scroll(SNew(SBox).Padding(FMargin(2.f, 0.f, 6.f, 8.f))[Body])]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f, 0.f, 14.f)[HLine()]
			+ SVerticalBox::Slot().AutoHeight()[Footer]
		], FMargin(26.f, 22.f));
	return SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center)[CardW];
}

TSharedRef<SWidget> SDCRoot::OptAudio()
{
	TWeakObjectPtr<UDCAudioSubsystem> WAu = UDCAudioSubsystem::Get(GetContext());
	auto Vol = [WAu](FName IconName, const FString& Name, const FString& Desc, EDCVolumeChannel Ch)
	{
		auto Get = [WAu, Ch]() { return WAu.IsValid() ? WAu->GetVolume(Ch) : 1.f; };
		return Row(IconName, Name, Desc, SliderCtl(Get, [WAu, Ch](float V) { if (WAu.IsValid()) { WAu->SetVolume(Ch, V); } },
			[Get]() { return FString::Printf(TEXT("%d%%"), FMath::RoundToInt(Get() * 100.f)); }, [WAu]() { if (WAu.IsValid()) { WAu->PlaySFX(DCSfx::Click); } },
			TAttribute<bool>::CreateLambda([]() { return !DCSettings::MuteAll(); })));
	};
	auto Toggle = [this](const TCHAR* Key, bool bDef)
	{
		return SwitchRight(BoolKey(Key, bDef), [this, Key](bool b) { DCSettings::SetBool(Key, b); ApplyPlayerSettings(false); });
	};
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()[Section(TEXT("Volume"))]
		+ SVerticalBox::Slot().AutoHeight()[Vol(TEXT("volume-2"), TEXT("Master"), TEXT(""), EDCVolumeChannel::Master)]
		+ SVerticalBox::Slot().AutoHeight()[Vol(TEXT("mouse-pointer-click"), TEXT("Effects"), TEXT("Interface, jobs, cash, builds"), EDCVolumeChannel::SFX)]
		+ SVerticalBox::Slot().AutoHeight()[Vol(TEXT("building-2"), TEXT("Ambience"), TEXT("The house, the fields, the weather"), EDCVolumeChannel::Ambience)]
		+ SVerticalBox::Slot().AutoHeight()[Section(TEXT("Muting"))]
		+ SVerticalBox::Slot().AutoHeight()[Row(TEXT("volume-x"), TEXT("Mute all"), TEXT("Silence everything without losing your levels"), Toggle(TEXT("MuteAll"), false))]
		+ SVerticalBox::Slot().AutoHeight()[Row(TEXT("app-window"), TEXT("Mute when unfocused"), TEXT("Go quiet while another window is in front"), Toggle(TEXT("MuteWhenUnfocused"), true))];
}

TSharedRef<SWidget> SDCRoot::OptGraphics()
{
	TSharedPtr<FGfx> G = GfxPending;
	static const TArray<FString> QLabels = { TEXT("Low"), TEXT("Medium"), TEXT("High"), TEXT("Epic"), TEXT("Max") };

	auto ResList = [G]()
	{
		TArray<FIntPoint> L;
		if (G->Mode == (int32)EWindowMode::Fullscreen) { UKismetSystemLibrary::GetSupportedFullscreenResolutions(L); }
		else { UKismetSystemLibrary::GetConvenientWindowedResolutions(L); }
		L.AddUnique(G->Res);
		L.Sort([](const FIntPoint& A_, const FIntPoint& B_) { return A_.X * A_.Y == B_.X * B_.Y ? A_.X < B_.X : A_.X * A_.Y < B_.X * B_.Y; });
		return L;
	};
	auto IsBorderless = [G]() { return G->Mode == (int32)EWindowMode::WindowedFullscreen; };

	TSharedRef<SVerticalBox> V = SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()[Section(TEXT("Display"))]
		+ SVerticalBox::Slot().AutoHeight()[Row(TEXT("app-window"), TEXT("Window mode"), TEXT(""),
			Segmented({ TEXT("Windowed"), TEXT("Borderless"), TEXT("Fullscreen") },
				[G]() { return G->Mode == (int32)EWindowMode::Windowed ? 0 : (G->Mode == (int32)EWindowMode::WindowedFullscreen ? 1 : 2); },
				[G](int32 i)
				{
					G->Mode = i == 0 ? (int32)EWindowMode::Windowed : (i == 1 ? (int32)EWindowMode::WindowedFullscreen : (int32)EWindowMode::Fullscreen);
					if (i == 1) { if (const UGameUserSettings* S = GUS()) { G->Res = S->GetDesktopResolution(); } }
				}))]
		+ SVerticalBox::Slot().AutoHeight()[Row(TEXT("monitor"), TEXT("Resolution"), TEXT(""),
			Stepper([G, IsBorderless]() { return IsBorderless() ? FString::Printf(TEXT("%d × %d  (desktop)"), G->Res.X, G->Res.Y) : FString::Printf(TEXT("%d × %d"), G->Res.X, G->Res.Y); },
				[G, ResList](int32 Dir)
				{
					const TArray<FIntPoint> L = ResList();
					const int32 I = L.IndexOfByKey(G->Res);
					G->Res = L[FMath::Clamp(I + Dir, 0, L.Num() - 1)];
				}, TAttribute<bool>::CreateLambda([IsBorderless]() { return !IsBorderless(); })))]
		+ SVerticalBox::Slot().AutoHeight()[Row(TEXT("refresh-cw"), TEXT("VSync"), TEXT("Removes tearing; can add a little input delay"),
			SwitchRight([G]() { return G->bVSync; }, [G](bool b) { G->bVSync = b; }))]
		+ SVerticalBox::Slot().AutoHeight()[Row(TEXT("gauge"), TEXT("Frame rate limit"), TEXT(""),
			Segmented({ TEXT("30"), TEXT("60"), TEXT("120"), TEXT("144"), TEXT("Off") },
				[G]() { for (int32 i = 0; i < 5; ++i) { if (FMath::IsNearlyEqual(G->FpsLimit, FpsLimits[i])) { return i; } } return -1; },
				[G](int32 i) { G->FpsLimit = FpsLimits[i]; }))]
		+ SVerticalBox::Slot().AutoHeight()[Row(TEXT("scaling"), TEXT("Resolution scale"), TEXT("Render below native for speed; the UI stays sharp"),
			Segmented({ TEXT("Auto"), TEXT("50%"), TEXT("67%"), TEXT("75%"), TEXT("85%"), TEXT("100%") },
				[G]() { for (int32 i = 0; i < 6; ++i) { if (FMath::IsNearlyEqual(G->ResScale, ResScales[i], 0.6f)) { return i; } } return -1; },
				[G](int32 i) { G->ResScale = ResScales[i]; }), 360.f)]
		+ SVerticalBox::Slot().AutoHeight()[Row(TEXT("activity"), TEXT("Show FPS"), TEXT("Small counter in the corner (applies now)"),
			SwitchRight([]() { return DCSettings::ShowFPS(); }, [](bool b) { DCSettings::SetBool(TEXT("ShowFPS"), b); DCUI::RefreshPrefs(); }))]
		+ SVerticalBox::Slot().AutoHeight()[Row(TEXT("palette"), TEXT("Cel shading"), TEXT("Ink outlines and stepped lighting (applies now)"),
			SwitchRight([]() { return DCSettings::CelShading(); }, [](bool b) { DCSettings::SetBool(TEXT("ToonOutlines"), b); }))]
		+ SVerticalBox::Slot().AutoHeight()[Section(TEXT("Quality"))]
		+ SVerticalBox::Slot().AutoHeight()[Row(TEXT("layers"), TEXT("Overall quality"), TEXT("Sets everything below; tweak any line to go custom"),
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()[Segmented(QLabels, [G]() { return G->Overall(); }, [G](int32 i) { for (int32 k = 0; k < NumQ; ++k) { G->Q[k] = i; } })]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right).Padding(0.f, 4.f, 2.f, 0.f)
			[SNew(SBox).Visibility_Lambda([G]() { return G->Overall() < 0 ? EVisibility::Visible : EVisibility::Collapsed; })[Txt(TEXT("CUSTOM"), Caps(7.5f), Accent)]], 400.f)];
	for (int32 i = 0; i < NumQ; ++i)
	{
		V->AddSlot().AutoHeight()[Row(QIcon[i], QName[i], TEXT(""), Segmented(QLabels, [G, i]() { return G->Q[i]; }, [G, i](int32 L) { G->Q[i] = L; }), 400.f)];
	}
	return V;
}

TSharedRef<SWidget> SDCRoot::OptAccess()
{
	auto Toggle = [this](const TCHAR* Key, bool bDef, bool bRebuild)
	{
		return SwitchRight(BoolKey(Key, bDef), [this, Key, bRebuild](bool b) { DCSettings::SetBool(Key, b); ApplyPlayerSettings(bRebuild); });
	};
	auto UIScaleNow = [this]() { return PendingUIScale > 0.f ? PendingUIScale : DCSettings::UIScale(); };
	static const float TextSteps[4] = { 0.9f, 1.f, 1.15f, 1.3f };

	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()[Section(TEXT("Readability"))]
		+ SVerticalBox::Slot().AutoHeight()[Row(TEXT("scaling"), TEXT("UI scale"), TEXT("Size of every panel and the HUD (applies on release)"),
			SliderCtl([UIScaleNow]() { return (UIScaleNow() - 0.8f) / 0.7f; },
				[this](float V) { PendingUIScale = FMath::RoundToFloat((0.8f + V * 0.7f) * 20.f) / 20.f; },
				[UIScaleNow]() { return FString::Printf(TEXT("%.2f×"), UIScaleNow()); },
				[this]() { if (PendingUIScale > 0.f) { DCSettings::SetFloat(TEXT("UIScale"), PendingUIScale); PendingUIScale = -1.f; ApplyPlayerSettings(false); } }))]
		+ SVerticalBox::Slot().AutoHeight()[Row(TEXT("type"), TEXT("Text size"), TEXT("Text only, on top of UI scale"),
			Segmented({ TEXT("Small"), TEXT("Default"), TEXT("Large"), TEXT("Larger") },
				[]() { const float T = DCSettings::TextScale(); int32 Best = 1; for (int32 i = 0; i < 4; ++i) { if (FMath::Abs(TextSteps[i] - T) < FMath::Abs(TextSteps[Best] - T)) { Best = i; } } return Best; },
				[this](int32 i) { DCSettings::SetFloat(TEXT("TextScale"), TextSteps[i]); ApplyPlayerSettings(true); }))]
		+ SVerticalBox::Slot().AutoHeight()[Row(TEXT("contrast"), TEXT("High-contrast UI"), TEXT("Brighter text, solid panels, stronger outlines"), Toggle(TEXT("HighContrast"), false, true))]
		+ SVerticalBox::Slot().AutoHeight()[Section(TEXT("Colour"))]
		+ SVerticalBox::Slot().AutoHeight()[Row(TEXT("eye"), TEXT("Colour vision"), TEXT("Shifts the whole picture to separate the colours you confuse"),
			Segmented({ TEXT("Off"), TEXT("Protan"), TEXT("Deutan"), TEXT("Tritan") },
				[]() { return FMath::Clamp(DCSettings::ColorVisionMode(), 0, 3); },
				[this](int32 i) { DCSettings::SetInt(TEXT("ColorVision"), i); ApplyPlayerSettings(false); }))]
		+ SVerticalBox::Slot().AutoHeight()[Row(TEXT("sliders-horizontal"), TEXT("Correction strength"), TEXT(""),
			SliderCtl([]() { return (DCSettings::ColorVisionSeverity() - 1) / 9.f; },
				[this](float V) { DCSettings::SetInt(TEXT("ColorVisionSeverity"), 1 + FMath::RoundToInt(V * 9.f)); ApplyPlayerSettings(false); },
				[]() { return FString::FromInt(DCSettings::ColorVisionSeverity()); }, nullptr,
				TAttribute<bool>::CreateLambda([]() { return DCSettings::ColorVisionMode() != 0; })))]
		+ SVerticalBox::Slot().AutoHeight()[Section(TEXT("Comfort"))]
		+ SVerticalBox::Slot().AutoHeight()[Row(TEXT("move"), TEXT("Reduce motion"), TEXT("No camera sweeps, shakes or sliding panels"), Toggle(TEXT("ReduceMotion"), false, false))]
		+ SVerticalBox::Slot().AutoHeight()[Row(TEXT("zap-off"), TEXT("Reduce flashing"), TEXT("No flicker, strobes or pulsing warnings"), Toggle(TEXT("ReduceFlashing"), false, false))]
		+ SVerticalBox::Slot().AutoHeight()[Row(TEXT("pause"), TEXT("Pause when unfocused"), TEXT("Opens the pause menu when you switch away"), Toggle(TEXT("PauseOnFocusLoss"), true, false))]
		+ SVerticalBox::Slot().AutoHeight()[Section(TEXT("Information"))]
		+ SVerticalBox::Slot().AutoHeight()[Row(TEXT("tag"), TEXT("Crew name plates"), TEXT("Names above your crew in the house"), Toggle(TEXT("ShowCrewNames"), true, false))]
		+ SVerticalBox::Slot().AutoHeight()[Row(TEXT("hourglass"), TEXT("Expiry warnings"), TEXT("Amber and red timers, a toast and a chime before leads and recruits run out"), Toggle(TEXT("ExpiryWarnings"), true, false))];
}

TSharedRef<SWidget> SDCRoot::OptGameplay()
{
	auto Speed = [](const TCHAR* Key)
	{
		return SliderCtl([Key]() { return (DCSettings::GetFloat(Key, 1.f) - 0.5f) / 1.5f; },
			[Key](float V) { DCSettings::SetFloat(Key, FMath::RoundToFloat((0.5f + V * 1.5f) * 10.f) / 10.f); },
			[Key]() { return FString::Printf(TEXT("%.1f×"), DCSettings::GetFloat(Key, 1.f)); });
	};
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()[Section(TEXT("Camera"))]
		+ SVerticalBox::Slot().AutoHeight()[Row(TEXT("move"), TEXT("Edge scrolling"), TEXT("Pan when the mouse touches the edge of the screen"),
			SwitchRight([]() { return DCSettings::EdgeScroll(); }, [](bool b) { DCSettings::SetBool(TEXT("EdgeScroll"), b); }))]
		+ SVerticalBox::Slot().AutoHeight()[Row(TEXT("move-horizontal"), TEXT("Pan speed"), TEXT("Keyboard and edge panning"), Speed(TEXT("CameraPanSpeed")))]
		+ SVerticalBox::Slot().AutoHeight()[Row(TEXT("zoom-in"), TEXT("Zoom speed"), TEXT("Mouse wheel"), Speed(TEXT("CameraZoomSpeed")))]
		+ SVerticalBox::Slot().AutoHeight()[Section(TEXT("Time"))]
		+ SVerticalBox::Slot().AutoHeight()[Row(TEXT("pause"), TEXT("Pause / resume"), TEXT("Stops the clock; resumes at the speed you had"), Keys({ TEXT("SPACE") }))]
		+ SVerticalBox::Slot().AutoHeight()[Row(TEXT("fast-forward"), TEXT("Game speed"), TEXT("1×, 2× or 4×. Also on the clock bar. Every run starts at 1×"), Keys({ TEXT("1"), TEXT("2"), TEXT("3") }))]
		+ SVerticalBox::Slot().AutoHeight()[Section(TEXT("Developer"))]
		+ SVerticalBox::Slot().AutoHeight()[Row(TEXT("bug"), TEXT("Debug view"), TEXT("Draws what the game generates: build grid, room layout, doorways, furniture, the walkable tile map and crew routes"),
			SwitchRight([]() { return DCSettings::DebugView(); }, [](bool b) { DCSettings::SetBool(TEXT("DebugView"), b); }))];
}

// ── Save slots ───────────────────────────────────────────────────────────────

TArray<FDCSaveSlotInfo> SDCRoot::SlotInfos() const
{
	TArray<FDCSaveSlotInfo> Out;
	if (bFakeData)
	{
		// Dev screenshots only (-DCUIFakeData).
		for (int32 i = 0; i < UDCRunSubsystem::NumSaveSlots; ++i) { FDCSaveSlotInfo I; I.Slot = i; Out.Add(I); }
		Out[0].bUsed = true; Out[0].Day = 4; Out[0].Hour = 14.33f; Out[0].Cash = 18400; Out[0].MissionsSucceeded = 11; Out[0].RosterCount = 5; Out[0].RoomsBuilt = 3; Out[0].PlayTime = 5420.0; Out[0].SavedAt = FDateTime::UtcNow() - FTimespan::FromMinutes(12);
		Out[1].bUsed = true; Out[1].bFinished = true; Out[1].bWon = true; Out[1].Day = 9; Out[1].Hour = 22.1f; Out[1].Cash = 412000; Out[1].MissionsSucceeded = 38; Out[1].RosterCount = 8; Out[1].RoomsBuilt = 7; Out[1].PlayTime = 11873.0; Out[1].SavedAt = FDateTime::UtcNow() - FTimespan::FromDays(2);
		return Out;
	}
	if (const UDCRunSubsystem* R = Run()) { Out = R->GetSaveSlots(); }
	while (Out.Num() < UDCRunSubsystem::NumSaveSlots) { FDCSaveSlotInfo I; I.Slot = Out.Num(); Out.Add(I); }
	return Out;
}

TArray<FDCRunRecord> SDCRoot::LeaderboardRows() const
{
	if (bFakeData)
	{
		TArray<FDCRunRecord> L;
		auto Add = [&L](bool bWon, double T, int32 Days, int32 Jobs, int32 Take, int32 Ago)
		{
			FDCRunRecord R; R.bWon = bWon; R.PlayTime = T; R.DaysSurvived = Days; R.MissionsSucceeded = Jobs; R.TotalTake = Take; R.EndedAt = FDateTime::UtcNow() - FTimespan::FromDays(Ago); L.Add(R);
		};
		Add(true, 9874.0, 8, 34, 512000, 1); Add(true, 10642.0, 8, 36, 468000, 4); Add(true, 11873.0, 9, 38, 412000, 2); Add(true, 13390.0, 10, 41, 455500, 9);
		Add(false, 8815.0, 7, 24, 187000, 5); Add(false, 6120.0, 5, 17, 96000, 3); Add(false, 3904.0, 3, 9, 38500, 8); Add(false, 2210.0, 2, 6, 21500, 6);
		return L;
	}
	const UDCRunSubsystem* R = Run();
	return R ? R->GetLeaderboard() : TArray<FDCRunRecord>();
}

void SDCRoot::ActOnSlot(int32 Slot, EView Mode)
{
	UDCRunSubsystem* R = Run();
	if (!R) { return; }
	if (Mode == EView::SlotsNew)
	{
		bHasEndSummary = false;
		R->SetPaused(false);
		R->StartNewRunInSlot(Slot, 0);
		Sfx(DCSfx::Confirm);
		View = EView::None;
		Close(true);
	}
	else if (Mode == EView::SlotsLoad)
	{
		if (R->HasActiveRun() && R->GetPhase() == EDCGamePhase::Playing) { R->SaveRun(); }
		bHasEndSummary = false;
		R->SetPaused(false);
		if (!R->LoadSlot(Slot)) { Sfx(DCSfx::Error); Toast(TEXT("Couldn't load that slot"), TEXT("triangle-alert"), true); return; }
		Sfx(DCSfx::Confirm);
		View = EView::None;
		const EDCGamePhase P = R->GetPhase();
		if (P == EDCGamePhase::GameOver || P == EDCGamePhase::Won) { EndSummary = R->GetRunSummary(); bHasEndSummary = true; Open(P == EDCGamePhase::Won ? EDCPanel::Victory : EDCPanel::GameOver); }
		else { Close(true); }
		Toast(FString::Printf(TEXT("Loaded slot %d"), Slot + 1), TEXT("folder-open"), false);
	}
	else if (Mode == EView::SlotsSave)
	{
		if (R->SaveToSlot(Slot)) { Sfx(DCSfx::Confirm); Toast(FString::Printf(TEXT("Saved to slot %d"), Slot + 1), TEXT("save"), false); SetView(EView::None); }
		else { Sfx(DCSfx::Error); Toast(TEXT("Couldn't save to that slot"), TEXT("triangle-alert"), true); }
	}
}

TSharedRef<SWidget> SDCRoot::SlotCard(const FDCSaveSlotInfo& I, EView Mode)
{
	const UDCRunSubsystem* R = Run();
	const int32 Slot = I.Slot;
	const bool bCurrent = !bFakeData && R && R->HasActiveRun() && R->GetCurrentSlot() == Slot;
	const FLinearColor StateCol = !I.bUsed ? TextFaint : (I.bFinished ? (I.bWon ? Accent : Bad) : Good);
	const FString StateText = !I.bUsed ? TEXT("EMPTY") : (I.bFinished ? (I.bWon ? TEXT("WON") : TEXT("LOST")) : TEXT("IN PROGRESS"));
	const FName StateIcon = !I.bUsed ? FName(TEXT("circle-dashed")) : (I.bFinished ? (I.bWon ? FName(TEXT("crown")) : FName(TEXT("skull"))) : FName(TEXT("play")));

	TSharedRef<SHorizontalBox> Head = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[Label(FString::Printf(TEXT("Slot %d"), Slot + 1), TextDim, 8.5f)]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Pill(IconText(StateIcon, StateText, Caps(7.5f), StateCol, StateCol, 9.f, 4.f), A(StateCol, 0.12f), FMargin(7.f, 2.f))];
	if (bCurrent) { Head->InsertSlot(1).AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 6.f, 0.f)[Pill(Txt(TEXT("CURRENT"), Caps(7.5f), Info), A(Info, 0.12f), FMargin(7.f, 2.f))]; }

	TSharedRef<SWidget> Mid = SNullWidget::NullWidget;
	if (I.bUsed)
	{
		auto Stat = [](FName IconName, const FString& V, const FString& Tip_)
		{
			TSharedRef<SWidget> W = IconText(IconName, V, Mono(10.5f), TextDim, Text, 11.f, 7.f);
			W->SetToolTip(Tip(Tip_));
			return W;
		};
		Mid = SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom)[Txt(FString::Printf(TEXT("Day %d"), I.Day), Font(20.f, EW::SemiBold), Text)]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom).Padding(10.f, 0.f, 0.f, 3.f)[Txt(HourMin(I.Hour), Mono(12.f), TextDim)]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 12.f, 0.f, 0.f)
			[
				SNew(SUniformGridPanel).SlotPadding(FMargin(0.f, 3.f))
				+ SUniformGridPanel::Slot(0, 0)[Stat(TEXT("banknote"), MoneyShort(I.Cash), TEXT("Cash"))]
				+ SUniformGridPanel::Slot(1, 0)[Stat(TEXT("trophy"), FString::FromInt(I.MissionsSucceeded), TEXT("Jobs done"))]
				+ SUniformGridPanel::Slot(0, 1)[Stat(TEXT("users"), FString::FromInt(I.RosterCount), TEXT("Crew"))]
				+ SUniformGridPanel::Slot(1, 1)[Stat(TEXT("hammer"), FString::FromInt(I.RoomsBuilt), TEXT("Buildings"))]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 10.f, 0.f, 0.f)[HLine()]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 8.f, 0.f, 0.f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[IconText(TEXT("clock"), Hms(I.PlayTime), Mono(9.5f), TextFaint, TextDim, 10.f, 5.f)]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Txt(LocalDate(I.SavedAt, true), Font(9.f), TextFaint)]
			];
	}
	else
	{
		Mid = SNew(SBox).HeightOverride(118.f).HAlign(HAlign_Center).VAlign(VAlign_Center)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[Icon(TEXT("file-plus"), 26.f, TextFaint)]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 8.f, 0.f, 0.f)[Txt(TEXT("Empty slot"), Font(11.f), TextDim)]
			];
	}

	// Actions
	TSharedRef<SHorizontalBox> Act = SNew(SHorizontalBox);
	const FString Summary = I.bUsed ? FString::Printf(TEXT("Day %d · %s · %s"), I.Day, *HourMin(I.Hour), *Money(I.Cash)) : FString();
	if (Mode == EView::SlotsNew)
	{
		Act->AddSlot().FillWidth(1.f)[TextButton(TEXT("plus"), I.bUsed ? TEXT("Start over here") : TEXT("Start here"), [this, Slot, I, Summary]()
		{
			if (!I.bUsed) { ActOnSlot(Slot, EView::SlotsNew); return; }
			Ask(FString::Printf(TEXT("Overwrite slot %d?"), Slot + 1), FString::Printf(TEXT("%s will be replaced by a fresh run. This can't be undone."), *Summary), TEXT("triangle-alert"), TEXT("Overwrite"),
				[this, Slot]() { ActOnSlot(Slot, EView::SlotsNew); }, true);
		}, I.bUsed ? EBtn::Secondary : EBtn::Primary, true, nullptr, 10.5f)];
	}
	else if (Mode == EView::SlotsLoad)
	{
		Act->AddSlot().FillWidth(1.f)[TextButton(I.bFinished ? TEXT("eye") : TEXT("folder-open"), I.bFinished ? TEXT("View") : TEXT("Load"), [this, Slot]() { ActOnSlot(Slot, EView::SlotsLoad); },
			EBtn::Primary, I.bUsed && !bCurrent, I.bFinished ? TSharedPtr<IToolTip>(Tip(TEXT("Finished run"), TEXT("Opens its final results. Save it elsewhere or start fresh from there."), TEXT("flag"))) : TSharedPtr<IToolTip>(), 10.5f)];
		Act->AddSlot().AutoWidth().Padding(6.f, 0.f, 0.f, 0.f)[IconButton(TEXT("trash-2"), [this, Slot, Summary]()
		{
			Ask(FString::Printf(TEXT("Delete slot %d?"), Slot + 1), FString::Printf(TEXT("%s will be gone for good."), *Summary), TEXT("trash-2"), TEXT("Delete"),
				[this, Slot]()
				{
					UDCRunSubsystem* X = Run();
					if (X && X->DeleteSlot(Slot)) { Sfx(DCSfx::Click); Toast(FString::Printf(TEXT("Slot %d deleted"), Slot + 1), TEXT("trash-2"), false); }
					else { Sfx(DCSfx::Error); }
					SetView(View);
				}, true);
		}, Tip(TEXT("Delete"), bCurrent ? TEXT("Can't delete the run you're playing") : TEXT("Removes this save")), EBtn::Danger, 13.f, I.bUsed && !bCurrent)];
	}
	else
	{
		Act->AddSlot().FillWidth(1.f)[TextButton(TEXT("save"), bCurrent ? TEXT("Save") : TEXT("Save here"), [this, Slot, I, bCurrent, Summary]()
		{
			if (!I.bUsed || bCurrent) { ActOnSlot(Slot, EView::SlotsSave); return; }
			Ask(FString::Printf(TEXT("Overwrite slot %d?"), Slot + 1), FString::Printf(TEXT("%s will be replaced by this run."), *Summary), TEXT("save"), TEXT("Overwrite"),
				[this, Slot]() { ActOnSlot(Slot, EView::SlotsSave); }, true);
		}, I.bUsed && !bCurrent ? EBtn::Secondary : EBtn::Primary, true, nullptr, 10.5f)];
	}

	return CardBox(
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()[Head]
		+ SVerticalBox::Slot().FillHeight(1.f).Padding(0.f, 14.f, 0.f, 0.f)[SNew(SBox).MinDesiredHeight(118.f).VAlign(VAlign_Center)[Mid]]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 14.f, 0.f, 0.f)[Act],
		FMargin(16.f, 14.f), bCurrent ? A(Info, 0.5f) : Line);
}

TSharedRef<SWidget> SDCRoot::BuildSlots(EView Mode)
{
	const TArray<FDCSaveSlotInfo> Infos = SlotInfos();
	TSharedRef<SUniformGridPanel> Grid = SNew(SUniformGridPanel).SlotPadding(FMargin(6.f));
	for (int32 i = 0; i < Infos.Num(); ++i) { Grid->AddSlot(i, 0)[SlotCard(Infos[i], Mode)]; }

	const FString Title = Mode == EView::SlotsNew ? TEXT("New run") : (Mode == EView::SlotsLoad ? TEXT("Load a run") : TEXT("Save this run"));
	const FString Sub = Mode == EView::SlotsNew ? TEXT("Pick a slot. Your run autosaves there as you play.")
		: (Mode == EView::SlotsLoad ? TEXT("Pick up any run where you left it. Finished runs open their results.")
		: TEXT("Copy this run into a slot. That slot becomes the one it autosaves to."));
	const FName IconName = Mode == EView::SlotsNew ? FName(TEXT("plus")) : (Mode == EView::SlotsLoad ? FName(TEXT("folder-open")) : FName(TEXT("save")));

	TSharedRef<SWidget> CardW = GlassPanel(
		SNew(SBox).WidthOverride(860.f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 12.f, 0.f)[IconTile(IconName, 38.f, Accent)]
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()[Txt(Title, Font(17.f, EW::SemiBold), Text)]
					+ SVerticalBox::Slot().AutoHeight()[Txt(Sub, Font(10.f), TextDim)]
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[IconButton(TEXT("x"), [this]() { SetView(EView::None); }, Tip(TEXT("Back"), TEXT("Esc")), EBtn::Ghost, 15.f)]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 16.f, 0.f, 12.f)[HLine()]
			+ SVerticalBox::Slot().AutoHeight()[SNew(SBox).Padding(FMargin(-6.f))[Grid]]
		], FMargin(26.f, 22.f, 26.f, 26.f));
	return SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center)[CardW];
}

// ── Leaderboard (title screen) ───────────────────────────────────────────────

TSharedRef<SWidget> SDCRoot::BuildLeaderboard()
{
	const TArray<FDCRunRecord> Rows = LeaderboardRows();
	const float W[7] = { 34.f, 40.f, 96.f, 56.f, 54.f, 86.f, 0.f }; // last = fill
	auto Cols = [&W](TArray<TSharedRef<SWidget>> Cells)
	{
		TSharedRef<SHorizontalBox> H = SNew(SHorizontalBox);
		for (int32 i = 0; i < Cells.Num(); ++i)
		{
			if (W[i] > 0.f) { H->AddSlot().AutoWidth().VAlign(VAlign_Center)[SNew(SBox).WidthOverride(W[i])[Cells[i]]]; }
			else { H->AddSlot().FillWidth(1.f).VAlign(VAlign_Center).HAlign(HAlign_Right)[Cells[i]]; }
		}
		return H;
	};

	TSharedRef<SVerticalBox> Table = SNew(SVerticalBox);
	Table->AddSlot().AutoHeight().Padding(10.f, 0.f, 10.f, 8.f)
	[Cols({ Label(TEXT("#")), Label(TEXT("")), Label(TEXT("Run time")), Label(TEXT("Days")), Label(TEXT("Jobs")), Label(TEXT("Take")), Label(TEXT("Date")) })];
	Table->AddSlot().AutoHeight()[HLine()];

	if (Rows.Num() == 0)
	{
		Table->AddSlot().AutoHeight().Padding(0.f, 34.f, 0.f, 30.f).HAlign(HAlign_Center)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[Icon(TEXT("trophy"), 28.f, TextFaint)]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 10.f, 0.f, 0.f)[Txt(TEXT("No finished runs yet"), Font(11.5f, EW::Medium), TextDim)]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 3.f, 0.f, 0.f)[Txt(TEXT("Win or go broke: every finished run lands here."), Font(9.5f), TextFaint)]
		];
	}
	for (int32 i = 0; i < Rows.Num() && i < 8; ++i)
	{
		const FDCRunRecord& Rec = Rows[i];
		const FLinearColor RC = Rec.bWon ? Accent : Bad;
		TSharedRef<SWidget> Res = Icon(Rec.bWon ? TEXT("crown") : TEXT("skull"), 13.f, RC);
		TSharedRef<SWidget> ResBox = SNew(SBox)[Res];
		ResBox->SetToolTip(Tip(Rec.bWon ? TEXT("Pulled off the Big Score") : TEXT("Went broke"), Rec.EndReason, Rec.bWon ? TEXT("crown") : TEXT("skull"), RC));
		const FLinearColor RankCol = i == 0 ? Accent : (i < 3 ? Text : TextDim);
		Table->AddSlot().AutoHeight().Padding(0.f, 2.f, 0.f, 0.f)
		[
			SNew(SBorder).BorderImage(i % 2 == 0 ? Box(A(FLinearColor::White, 0.025f), 7.f) : NoBrush()).Padding(FMargin(10.f, 7.f))
			[
				Cols({
					Txt(FString::FromInt(i + 1), Mono(11.f), RankCol),
					ResBox,
					Txt(Hms(Rec.PlayTime), Mono(11.f), Rec.bWon ? Text : TextDim),
					Txt(FString::FromInt(Rec.DaysSurvived), Mono(10.5f), TextDim),
					Txt(FString::FromInt(Rec.MissionsSucceeded), Mono(10.5f), TextDim),
					Txt(MoneyShort(Rec.TotalTake), Mono(10.5f), TextDim),
					Txt(LocalDate(Rec.EndedAt, false), Font(9.5f), TextFaint) })
			]
		];
	}

	TSharedRef<SWidget> Clear = Rows.Num() == 0 ? SNullWidget::NullWidget : IconButton(TEXT("trash-2"), [this]()
	{
		Ask(TEXT("Clear the leaderboard?"), TEXT("Every finished run on the board is removed. Save slots aren't touched."), TEXT("trash-2"), TEXT("Clear"),
			[this]() { if (UDCRunSubsystem* X = Run()) { X->ClearLeaderboard(); } Sfx(DCSfx::Click); Rebuild(); }, true);
	}, Tip(TEXT("Clear leaderboard")), EBtn::Ghost, 13.f);

	return GlassPanel(
		SNew(SBox).WidthOverride(520.f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 10.f, 0.f)[IconTile(TEXT("trophy"), 32.f, Accent)]
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()[Txt(TEXT("Leaderboard"), Font(14.f, EW::SemiBold), Text)]
					+ SVerticalBox::Slot().AutoHeight()[Txt(TEXT("Fastest wins first, then the longest survivors"), Font(9.5f), TextDim)]
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Clear]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 14.f, 0.f, 10.f)[HLine()]
			+ SVerticalBox::Slot().AutoHeight()[Table]
		], FMargin(20.f, 18.f));
}

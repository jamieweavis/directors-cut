// Director's Cut — SDCRoot: plumbing, HUD, dock, toasts, run events (Agent C).
#include "UI/SDCRoot.h"
#include "UI/SDCRootInternal.h"
#include "UI/SDCHudNotify.h"
#include "Framework/Application/SlateApplication.h"
#include "InputCoreTypes.h"
#include "Misc/CommandLine.h"
#include "Widgets/Layout/SDPIScaler.h"
#include "DirectorsCut.h"

// ─────────────────────────────────────────────────────────────────────────────
// Construction
// ─────────────────────────────────────────────────────────────────────────────

void SDCRoot::Construct(const FArguments& InArgs)
{
	Owner = InArgs._Owner;
	PlanCache = MakeShared<FPlanCache>();
	// The root covers the whole viewport: only its visible children may eat clicks, never the empty space.
	SetVisibility(EVisibility::SelfHitTestInvisible);

	bFakeData = !UE_BUILD_SHIPPING && FParse::Param(FCommandLine::Get(), TEXT("DCUIFakeData"));
	DCUI::RefreshPrefs();
	ChildSlot[SNew(SDPIScaler).DPIScale_Lambda([]() { return Prefs().UIScale; })[BuildRootContent()]];

	BindRun();
	if (const UDCRunSubsystem* R = Run()) { DisplayCash = R->GetCash(); bCashInit = true; }
}

SDCRoot::~SDCRoot() { UnbindRun(); }

TSharedRef<SWidget> SDCRoot::BuildRootContent()
{
	auto PanelAlpha = [this]() -> float
	{
		const float T = (float)(Now() - PanelT0);
		return bPanelShown ? EaseOut(T / 0.18f) : 1.f - EaseOut(T / 0.12f);
	};
	auto ModalAlpha = [this]() -> float
	{
		const float T = (float)(Now() - ModalT0);
		return bModalShown ? EaseOut(T / 0.2f) : 1.f - EaseOut(T / 0.14f);
	};
	auto ConfirmAlpha = [this]() -> float
	{
		const float T = (float)(Now() - ConfirmFadeT0);
		return bConfirmShown ? EaseOut(T / 0.16f) : 1.f - EaseOut(T / 0.12f);
	};
	auto HudVis = [this]() { return (HasRun() && IsPlaying() && Current != EDCPanel::Title) ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed; };

	return SNew(SOverlay)
		.Visibility(EVisibility::SelfHitTestInvisible)

		// ── HUD: active mission + build chips (top-left) ──
		+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(FMargin(20.f, 20.f, 0.f, 0.f))
		[
			SNew(SBox).Visibility_Lambda(HudVis)[SAssignNew(ChipBox, SVerticalBox)]
		]

		// ── HUD: clock + stats bar (top-right) ──
		+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(FMargin(0.f, 20.f, 20.f, 0.f))
		[
			SNew(SBox).Visibility_Lambda(HudVis)[BuildHudStats()]
		]

		// ── Dock (bottom-centre) ──
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(FMargin(0.f, 0.f, 0.f, 18.f))
		[
			SNew(SBox).Visibility_Lambda(HudVis)[BuildDock()]
		]

		// ── HUD: crew list (bottom-left, clear of the dock) ──
		+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Bottom).Padding(FMargin(20.f, 0.f, 0.f, 18.f))
		[
			SNew(SBox).Visibility_Lambda(HudVis)[BuildHudCrew()]
		]

		// ── Mission-result cards (bottom-right, above the dock line; hold while a panel or modal covers them) ──
		+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Bottom).Padding(FMargin(0.f, 0.f, 20.f, 104.f))
		[
			SNew(SBox).Visibility_Lambda(HudVis)
			[
				SAssignNew(HudNotify, SDCHudNotify)
				.Suspended_Lambda([this]() { return bPanelShown || bModalShown || bConfirmShown; })
				.OnOpenReport([this](int32) { Open(EDCPanel::Reports); })
			]
		]

		// ── Panel (right-docked, slides in) ──
		+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Fill).Padding(FMargin(20.f, 82.f, 20.f, 104.f))
		[
			SNew(SBorder)
			.BorderImage(NoBrush())
			.Padding(0.f)
			.ColorAndOpacity_Lambda([PanelAlpha]() { return FLinearColor(1, 1, 1, PanelAlpha()); })
			.RenderTransform_Lambda([PanelAlpha]() { return TOptional<FSlateRenderTransform>(FSlateRenderTransform(FVector2f((1.f - PanelAlpha()) * (Prefs().bReduceMotion ? 0.f : 28.f), 0.f))); })
			.Visibility_Lambda([this, PanelAlpha]() { return bPanelShown ? EVisibility::SelfHitTestInvisible : (PanelAlpha() > 0.01f ? EVisibility::HitTestInvisible : EVisibility::Collapsed); })
			[
				SAssignNew(PanelSlot, SBox).WidthOverride_Lambda([this]() { return FOptionalSize(PanelWidth(Shown)); })
			]
		]

		// ── Modal layer ──
		+ SOverlay::Slot()
		[
			SNew(SBorder)
			.BorderImage(WhiteBrush())
			.BorderBackgroundColor_Lambda([this, ModalAlpha]() { return FLinearColor(0.01f, 0.012f, 0.016f, (ShownModal == EDCPanel::Title ? 0.f : 0.72f) * ModalAlpha()); })
			.Padding(0.f)
			.ColorAndOpacity_Lambda([ModalAlpha]() { return FLinearColor(1, 1, 1, ModalAlpha()); })
			.Visibility_Lambda([this, ModalAlpha]() { return bModalShown ? EVisibility::Visible : (ModalAlpha() > 0.01f ? EVisibility::HitTestInvisible : EVisibility::Collapsed); })
			[
				SAssignNew(ModalSlot, SBox)
			]
		]

		// ── Toasts (top-centre) ──
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Top).Padding(FMargin(0.f, 22.f, 0.f, 0.f))
		[
			SAssignNew(ToastBox, SVerticalBox).Visibility(EVisibility::HitTestInvisible)
		]

		// ── Confirm dialog (above everything but toasts) ──
		+ SOverlay::Slot()
		[
			SNew(SBorder)
			.BorderImage(WhiteBrush())
			.BorderBackgroundColor_Lambda([ConfirmAlpha]() { return FLinearColor(0.01f, 0.012f, 0.016f, 0.6f * ConfirmAlpha()); })
			.Padding(0.f)
			.ColorAndOpacity_Lambda([ConfirmAlpha]() { return FLinearColor(1, 1, 1, ConfirmAlpha()); })
			.Visibility_Lambda([this, ConfirmAlpha]() { return bConfirmShown ? EVisibility::Visible : (ConfirmAlpha() > 0.01f ? EVisibility::HitTestInvisible : EVisibility::Collapsed); })
			[
				SAssignNew(ConfirmSlot, SBox).HAlign(HAlign_Center).VAlign(VAlign_Center)
			]
		]

		// ── FPS readout (Options > Graphics > Show FPS) ──
		+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Bottom).Padding(FMargin(14.f, 0.f, 0.f, 12.f))
		[
			SNew(SBox)
			.Visibility_Lambda([]() { return Prefs().bShowFPS ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
			[
				Pill(TxtA(TAttribute<FText>::CreateLambda([this]() { return FText::FromString(FString::Printf(TEXT("%d FPS  %.1f ms"), FMath::RoundToInt(FpsSmoothed), 1000.f / FMath::Max(1.f, FpsSmoothed))); }),
					Mono(9.f), TAttribute<FSlateColor>::CreateLambda([this]() { return FSlateColor(FpsSmoothed >= 50.f ? Good : (FpsSmoothed >= 28.f ? Accent : Bad)); })),
					A(GlassSoft, 0.8f), FMargin(8.f, 3.f), Line)
			]
		];

}

void SDCRoot::RebuildAll()
{
	// Palette / text size changed: every widget captured old colours and fonts at build time.
	Toasts.Reset();
	LastChipSig = 0;
	ChildSlot[SNew(SDPIScaler).DPIScale_Lambda([]() { return Prefs().UIScale; })[BuildRootContent()]];
	if (IsModal(Current) && Current != EDCPanel::None) { ShownModal = Current; ModalSlot->SetContent(BuildPanel(Current)); }
	else if (Current != EDCPanel::None) { Shown = Current; PanelSlot->SetContent(BuildPanel(Current)); }
	if (Confirm.IsValid()) { ConfirmSlot->SetContent(BuildConfirm()); }
	LastSig = ComputeSignature();
}


UDCRunSubsystem* SDCRoot::Run() const
{
	UObject* Ctx = GetContext();
	return Ctx ? UDCRunSubsystem::Get(Ctx) : nullptr;
}
bool SDCRoot::HasRun() const { const UDCRunSubsystem* R = Run(); return R && R->HasActiveRun(); }
bool SDCRoot::IsPlaying() const { const UDCRunSubsystem* R = Run(); return R && R->HasActiveRun() && R->GetPhase() == EDCGamePhase::Playing; }

// ─────────────────────────────────────────────────────────────────────────────
// Run events
// ─────────────────────────────────────────────────────────────────────────────

void SDCRoot::BindRun()
{
	UDCRunSubsystem* R = Run();
	if (!R) { return; }
	HCompleted = R->OnMissionCompleted.AddSP(this, &SDCRoot::OnMissionCompleted);
	HPerk = R->OnPerkOffered.AddSP(this, &SDCRoot::OnPerkOffered);
	HOver = R->OnGameOver.AddLambda([this](const FDCRunSummary& S) { OnRunEnded(S, false); });
	HWon = R->OnRunWon.AddLambda([this](const FDCRunSummary& S) { OnRunEnded(S, true); });
	HPhase = R->OnPhaseChanged.AddSP(this, &SDCRoot::OnPhaseChanged);
	HAdded = R->OnMissionAdded.AddLambda([this](const FDCMission& M)
	{
		if (M.bFinal) { Toast(TEXT("The Big Score is on the board"), TEXT("crown"), false); }
		else if (M.bRare) { Toast(FString::Printf(TEXT("Rare job · %s"), *M.DisplayName), TEXT("sparkles"), false); }
	});
	HLevel = R->OnCriminalLevelUp.AddLambda([this](int32 Id, int32 Lvl)
	{
		const UDCRunSubsystem* RR = Run();
		const FDCCriminal* C = RR ? RR->FindCriminal(Id) : nullptr;
		Toast(FString::Printf(TEXT("%s · level %d"), C ? *C->GetShortName() : TEXT("Crew"), Lvl), TEXT("chevrons-up"), false);
	});
	HRoom = R->OnRoomChanged.AddLambda([this](const FDCRoom& Room)
	{
		const FString Name = Room.DisplayName.IsEmpty() ? FString(DC::RoomName(Room.Type)) : Room.DisplayName;
		if (Room.State == EDCRoomState::Built)
		{
			Toast(Room.Level <= 1 ? FString::Printf(TEXT("%s wing added"), *Name) : FString::Printf(TEXT("%s · level %d"), *Name, Room.Level), Room.Icon.IsNone() ? DC::RoomIcon(Room.Type) : Room.Icon, false);
		}
	});
	HPayday = R->OnPayday.AddLambda([this](int32 Day, int32 Paid)
	{
		Toast(FString::Printf(TEXT("Payday · %s"), *Money(-Paid)), TEXT("wallet"), false);
	});
	HHired = R->OnCriminalHired.AddLambda([this](const FDCCriminal& C) { Toast(FString::Printf(TEXT("%s joined"), *C.GetShortName()), TEXT("user-plus"), false); });
	HHealed = R->OnCriminalHealed.AddLambda([this](int32 Id)
	{
		const UDCRunSubsystem* RR = Run();
		const FDCCriminal* C = RR ? RR->FindCriminal(Id) : nullptr;
		if (C) { Toast(FString::Printf(TEXT("%s is back on their feet"), *C->GetShortName()), TEXT("heart-pulse"), false); }
	});
}

void SDCRoot::UnbindRun()
{
	UDCRunSubsystem* R = Run();
	if (!R) { return; }
	R->OnMissionCompleted.Remove(HCompleted);
	R->OnPerkOffered.Remove(HPerk);
	R->OnGameOver.Remove(HOver);
	R->OnRunWon.Remove(HWon);
	R->OnPhaseChanged.Remove(HPhase);
	R->OnMissionAdded.Remove(HAdded);
	R->OnCriminalLevelUp.Remove(HLevel);
	R->OnRoomChanged.Remove(HRoom);
	R->OnPayday.Remove(HPayday);
	R->OnCriminalHired.Remove(HHired);
	R->OnCriminalHealed.Remove(HHealed);
}

void SDCRoot::OnMissionCompleted(const FDCMissionReport& R)
{
	if (HudNotify.IsValid()) { HudNotify->Push(R); }
	const UDCRunSubsystem* Rs = Run();
	const bool bLawyer = Rs && Rs->IsRoomBuilt(EDCRoomType::LawyerOffice);
	for (const FDCCrewMemberResult& MR : R.Members)
	{
		if (MR.bArrested)
		{
			Toast(FString::Printf(TEXT("%s was arrested%s"), *MR.Name, bLawyer ? TEXT(" · bail them out at the Lawyer") : TEXT(" · no Lawyer, they're gone")), TEXT("siren"), true);
		}
		else if (MR.bQuit)
		{
			Toast(FString::Printf(TEXT("%s walked out · loyalty too low"), *MR.Name), TEXT("door-open"), true);
		}
	}
}

void SDCRoot::OnPerkOffered(const TArray<FDCPerk>&)
{
	Toast(TEXT("New perk to pick"), TEXT("sparkles"), false);
	bPerkQueued = true;
}

void SDCRoot::OnRunEnded(const FDCRunSummary& S, bool bWon)
{
	EndSummary = S;
	bHasEndSummary = true;
	Open(bWon ? EDCPanel::Victory : EDCPanel::GameOver);
}

void SDCRoot::OnPhaseChanged(EDCGamePhase P)
{
	if (P == EDCGamePhase::Title) { bHasEndSummary = false; if (Current != EDCPanel::Title) { Open(EDCPanel::Title); } }
	else if (P == EDCGamePhase::Playing)
	{
		bHasEndSummary = false;
		if (Current == EDCPanel::Title || Current == EDCPanel::GameOver || Current == EDCPanel::Victory) { Close(true); }
		bCashInit = false;
		SeenMissionIds.Reset(); FirstSeenAt.Reset(); ReportAnimT0.Reset();
		PlanMissionId = INDEX_NONE; PlanCrew.Reset(); CrewSel = INDEX_NONE; GearSel = INDEX_NONE;
	}
	else if (P == EDCGamePhase::GameOver || P == EDCGamePhase::Won)
	{
		const EDCPanel Want = P == EDCGamePhase::Won ? EDCPanel::Victory : EDCPanel::GameOver;
		if (Current != Want)
		{
			if (!bHasEndSummary) { if (const UDCRunSubsystem* R = Run()) { EndSummary = R->GetRunSummary(); bHasEndSummary = true; } }
			Open(Want);
		}
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Panel management
// ─────────────────────────────────────────────────────────────────────────────

bool SDCRoot::IsModal(EDCPanel P)
{
	switch (P)
	{
	case EDCPanel::Title:
	case EDCPanel::Pause:
	case EDCPanel::PerkDraft:
	case EDCPanel::GameOver:
	case EDCPanel::Victory:
		return true;
	default:
		return false;
	}
}

float SDCRoot::PanelWidth(EDCPanel P) const
{
	switch (P)
	{
	case EDCPanel::MissionPlanner: return 1260.f;
	case EDCPanel::Missions:       return 1080.f;
	case EDCPanel::Crew:           return 1180.f;
	case EDCPanel::Workbench:      return 1180.f;
	case EDCPanel::Recruits:       return 1040.f;
	case EDCPanel::Build:          return 1120.f;
	case EDCPanel::Reports:        return 860.f;
	default:                       return 760.f;
	}
}

void SDCRoot::SetCurrent(EDCPanel P)
{
	const EDCPanel Prev = Current;
	Current = P;
	if (UDCRunSubsystem* R = Run())
	{
		// The Esc menu is the only thing that pauses the clock.
		if (P == EDCPanel::Pause && !R->IsPaused() && R->HasActiveRun()) { R->SetPaused(true); }
		if (Prev == EDCPanel::Pause && P != EDCPanel::Pause && R->IsPaused()) { R->SetPaused(false); }
	}
	if (UDCUISubsystem* O = Owner.Get())
	{
		O->OpenPanelType = P;
		O->OnPanelChanged.Broadcast(P);
	}
}

void SDCRoot::Open(EDCPanel P)
{
	if (P == EDCPanel::None) { Close(true); return; }
	if (!IsModal(P) && !IsPlaying()) { return; }
	if (P == EDCPanel::PerkDraft) { const UDCRunSubsystem* R = Run(); if (!R || !R->HasPendingPerkOffer()) { return; } }

	const EDCPanel Prev = Current;
	if (P == EDCPanel::MissionPlanner && Prev != EDCPanel::MissionPlanner)
	{
		// No explicit mission: keep the current one if it still exists, else the first board posting.
		if (const UDCRunSubsystem* R = Run())
		{
			if (!R->FindMission(PlanMissionId))
			{
				PlanMissionId = INDEX_NONE; PlanCrew.Reset();
				for (const FDCMission& M : R->GetMissions()) { if (!M.bInProgress) { PlanMissionId = M.Id; break; } }
				if (PlanMissionId == INDEX_NONE && R->GetMissions().Num()) { PlanMissionId = R->GetMissions()[0].Id; }
			}
		}
	}
	if (P == EDCPanel::Pause) { bAbandonArmed = false; }
	if (P != Prev) { View = EView::None; bClearBoardArmed = false; }
	if (P == EDCPanel::Title && Prev != EDCPanel::Title) { TitleT0 = Now(); }
	if (P == EDCPanel::PerkDraft && Prev != EDCPanel::PerkDraft) { PerkDraftT0 = Now(); PerkSel = NAME_None; }
	if (P == EDCPanel::Reports || P == EDCPanel::Missions) { RecomputePlan(); }

	SavedScroll.Reset();
	Scrolls.Reset();
	SetCurrent(P);
	if (P == EDCPanel::MissionPlanner)
	{
		// Screenshot runs (-DCShots) can't click: show the planner with the auto-filled crew.
		if (PlanCrew.Num() == 0 && FCString::Strifind(FCommandLine::Get(), TEXT("DCShots=")))
		{
			if (const UDCRunSubsystem* R = Run()) { PlanCrew = R->SuggestPlan(PlanMissionId, PlanCut).CriminalIds; }
		}
		RecomputePlan();
	}
	const double T = Now();
	if (IsModal(P))
	{
		ShownModal = P;
		ModalSlot->SetContent(BuildPanel(P));
		if (!bModalShown) { ModalT0 = T; }
		bModalShown = true;
		if (bPanelShown) { bPanelShown = false; PanelT0 = T; }
	}
	else
	{
		Shown = P;
		PanelSlot->SetContent(BuildPanel(P));
		if (Prev != P && (!bPanelShown || IsModal(Prev))) { PanelT0 = T; }
		bPanelShown = true;
		if (bModalShown) { bModalShown = false; ModalT0 = T; }
	}
	LastSig = ComputeSignature();
	if (Prev != P) { Sfx(DCSfx::PanelOpen, 0.8f); }
}

void SDCRoot::OpenPlanner(int32 MissionId)
{
	if (MissionId != PlanMissionId) { PlanCrew.Reset(); }
	PlanMissionId = MissionId;
	if (Current == EDCPanel::MissionPlanner) { RecomputePlan(); Rebuild(); Sfx(DCSfx::Click); }
	else { Open(EDCPanel::MissionPlanner); }
}

void SDCRoot::Close(bool bForce)
{
	if (Current == EDCPanel::None) { return; }
	const UDCRunSubsystem* R = Run();
	if (!bForce)
	{
		// These screens only leave through their own buttons.
		if (Current == EDCPanel::Title) { return; }
		if ((Current == EDCPanel::GameOver || Current == EDCPanel::Victory) && R && R->GetPhase() != EDCGamePhase::Playing) { return; }
		if (Current == EDCPanel::PerkDraft && R && R->HasPendingPerkOffer()) { return; }
	}
	const double T = Now();
	if (IsModal(Current)) { bModalShown = false; ModalT0 = T; }
	else { bPanelShown = false; PanelT0 = T; }
	Sfx(DCSfx::PanelClose, 0.8f);
	SetCurrent(EDCPanel::None);
}

void SDCRoot::Back()
{
	if (Confirm.IsValid()) { EndConfirm(false); return; }
	if (View != EView::None) { SetView(EView::None); return; }
	if (Current == EDCPanel::None) { if (IsPlaying()) { Open(EDCPanel::Pause); } return; }
	if (Current == EDCPanel::MissionPlanner) { Open(EDCPanel::Missions); return; }
	if (Current == EDCPanel::Pause || !IsModal(Current)) { Close(); return; }
}

bool SDCRoot::IsWorldBlocked() const
{
	if (bModalShown || bConfirmShown) { return true; }
	return IsHovered();
}

bool SDCRoot::HandleHotkey(const FKey& Key)
{
	if (!IsPlaying() || (bModalShown && Current != EDCPanel::None) || Confirm.IsValid() || IsTextInputFocused()) { return false; }
	if (HandleSpeedKey(Key)) { return true; }
	EDCPanel P = EDCPanel::None;
	if (Key == EKeys::C) { P = EDCPanel::Crew; }
	else if (Key == EKeys::G) { P = EDCPanel::Workbench; }
	else if (Key == EKeys::R) { P = EDCPanel::Recruits; }
	else if (Key == EKeys::B) { P = EDCPanel::Build; }
	else if (Key == EKeys::N) { P = EDCPanel::Reports; }
	else if (Key == EKeys::M) { P = EDCPanel::Missions; }
	if (P == EDCPanel::None) { return false; }
	if (Current == P) { Close(); } else { Open(P); }
	return true;
}

TSharedRef<SWidget> SDCRoot::Scroll(TSharedRef<SWidget> Content)
{
	TSharedRef<SScrollBox> S = SNew(SScrollBox)
		.ScrollBarStyle(&ScrollBarStyle())
		.ScrollBarThickness(FVector2D(5.f, 5.f))
		.ScrollBarPadding(FMargin(4.f, 0.f, 0.f, 0.f))
		+ SScrollBox::Slot().Padding(FMargin(0.f, 0.f, 8.f, 0.f))[Content];
	const int32 Idx = Scrolls.Add(S);
	if (SavedScroll.IsValidIndex(Idx)) { S->SetScrollOffset(SavedScroll[Idx]); }
	return S;
}

void SDCRoot::Rebuild()
{
	if (Current == EDCPanel::None) { return; }
	SavedScroll.Reset();
	for (const TSharedPtr<SScrollBox>& S : Scrolls) { SavedScroll.Add(S.IsValid() ? S->GetScrollOffset() : 0.f); }
	Scrolls.Reset();
	if (IsModal(Current)) { ModalSlot->SetContent(BuildPanel(Current)); }
	else { Shown = Current; PanelSlot->SetContent(BuildPanel(Current)); }
	LastSig = ComputeSignature();
}

TSharedRef<SWidget> SDCRoot::BuildPanel(EDCPanel P)
{
	switch (P)
	{
	case EDCPanel::Title:          return BuildTitle();
	case EDCPanel::Missions:       return BuildMissions();
	case EDCPanel::MissionPlanner: return BuildPlanner();
	case EDCPanel::Crew:           return BuildCrew();
	case EDCPanel::Workbench:      return BuildWorkbench();
	case EDCPanel::Recruits:       return BuildRecruits();
	case EDCPanel::Build:          return BuildBuild();
	case EDCPanel::PerkDraft:      return BuildPerkDraft();
	case EDCPanel::Reports:        return BuildReports();
	case EDCPanel::GameOver:       return BuildEnd(false);
	case EDCPanel::Victory:        return BuildEnd(true);
	case EDCPanel::Pause:          return BuildPause();
	case EDCPanel::OpsRoom:
	case EDCPanel::Clinic:
	case EDCPanel::Lawyer:
	case EDCPanel::ServerRoom:     return BuildRoomPanel(P);
	default:                       return SNullWidget::NullWidget;
	}
}

TSharedRef<SWidget> SDCRoot::Shell(FName IconName, const FString& Title, TSharedRef<SWidget> Body, TSharedPtr<SWidget> HeaderRight, const FString& Sub)
{
	TSharedRef<SVerticalBox> TitleCol = SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()[Txt(Title, Font(17.f, EW::SemiBold), Text)];
	if (!Sub.IsEmpty()) { TitleCol->AddSlot().AutoHeight().Padding(0.f, 1.f, 0.f, 0.f)[Txt(Sub, Font(10.f), TextDim)]; }

	return GlassPanel(
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 12.f, 0.f)[IconTile(IconName, 38.f, Accent, 0.5f, 0.12f)]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[TitleCol]
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center).HAlign(HAlign_Right).Padding(16.f, 0.f)
			[HeaderRight.IsValid() ? HeaderRight.ToSharedRef() : SNullWidget::NullWidget]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[IconButton(TEXT("x"), [this]() { Close(); }, Tip(TEXT("Close"), TEXT("Esc")), EBtn::Ghost, 15.f)]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 16.f, 0.f, 18.f)[HLine()]
		+ SVerticalBox::Slot().FillHeight(1.f)[Body],
		FMargin(24.f, 20.f, 24.f, 22.f));
}

// ─────────────────────────────────────────────────────────────────────────────
// Signatures: rebuild panels only when what they show actually changed
// ─────────────────────────────────────────────────────────────────────────────

uint32 SDCRoot::ComputeSignature() const
{
	const UDCRunSubsystem* R = Run();
	if (!R) { return 0; }
	uint32 H = 17;
	auto Mix = [&H](uint32 V) { H = HashCombine(H, V); };
	Mix((uint32)R->GetPhase()); Mix(R->HasActiveRun() ? 1 : 0);
	if (!R->HasActiveRun()) { return H; }
	Mix((uint32)R->GetCash());
	for (const FDCCriminal& C : R->GetRoster())
	{
		Mix(C.Id); Mix((uint32)C.Status); Mix(C.Level); Mix(C.XP); Mix(C.Loyalty); Mix(C.ActiveMissionId);
		for (const TPair<EDCGearSlot, FName>& E : C.Equipped) { Mix((uint32)E.Key); Mix(GetTypeHash(E.Value)); }
		Mix(C.Traits.Num()); Mix(C.Abilities.Num());
	}
	Mix(R->GetFallen().Num());
	for (const FDCMission& M : R->GetMissions()) { Mix(M.Id); Mix(M.bInProgress ? 1 : 0); }
	for (const FDCCriminal& C : R->GetRecruits()) { Mix(C.Id); }
	for (const FName& N : R->GetStash()) { Mix(GetTypeHash(N)); }
	for (const FDCRoom& Room : R->GetRooms()) { Mix((uint32)Room.State); Mix(Room.Level); Mix(Room.bCanAct ? 1 : 0); }
	Mix(R->GetReports().Num()); Mix(R->GetUnseenReportCount());
	Mix(R->GetPerksTaken().Num()); Mix(R->HasPendingPerkOffer() ? 1 : 0);
	Mix(R->GetGearSlotCount()); Mix(R->GetUnlockedTier()); Mix(R->GetMaxActiveMissions()); Mix(R->GetRosterCap());
	Mix(R->IsNight() ? 1 : 0);
	if (Current == EDCPanel::Workbench || Current == EDCPanel::Crew)
	{
		for (const FDCGearItem& G : R->GetGearCatalogue()) { Mix(GetTypeHash(G.Id)); Mix(G.bAvailable ? 1 : 0); Mix(G.Price); }
	}
	return H;
}

uint32 SDCRoot::ComputeChipSignature() const
{
	const UDCRunSubsystem* R = Run();
	if (!R || !R->HasActiveRun()) { return 0; }
	uint32 H = 31;
	for (const FDCMission& M : R->GetMissions()) { if (M.bInProgress) { H = HashCombine(H, M.Id); } }
	for (const FDCRoom& Room : R->GetRooms()) { if (Room.State == EDCRoomState::Building) { H = HashCombine(H, 1000 + (uint32)Room.Type); } }
	return H;
}

int32 SDCRoot::NewPostingCount() const
{
	const UDCRunSubsystem* R = Run();
	if (!R || !R->HasActiveRun()) { return 0; }
	int32 N = 0;
	for (const FDCMission& M : R->GetMissions()) { if (!M.bInProgress && !SeenMissionIds.Contains(M.Id)) { ++N; } }
	return N;
}

bool SDCRoot::CanBuildSomething() const
{
	const UDCRunSubsystem* R = Run();
	if (!R || !R->HasActiveRun() || R->IsBuildCapacityFull()) { return false; }
	for (const FDCRoom& Room : R->GetRooms()) { if (Room.bCanAct && !Room.bMaxLevel) { return true; } }
	return false;
}

// ─────────────────────────────────────────────────────────────────────────────
// Tick
// ─────────────────────────────────────────────────────────────────────────────

void SDCRoot::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
	const double T = Now();
	UDCRunSubsystem* R = Run();

	// Keep the visible screen consistent with the run phase.
	if (R)
	{
		if (!R->HasActiveRun() && R->GetPhase() == EDCGamePhase::Title && Current != EDCPanel::Title) { Open(EDCPanel::Title); }
		if (IsPlaying() && Current == EDCPanel::Title) { Close(true); }
		if (R->GetPhase() == EDCGamePhase::GameOver && Current != EDCPanel::GameOver && Current != EDCPanel::Title) { Open(EDCPanel::GameOver); }
		if (R->GetPhase() == EDCGamePhase::Won && Current != EDCPanel::Victory && Current != EDCPanel::Title) { Open(EDCPanel::Victory); }
		if (IsPlaying() && R->HasPendingPerkOffer() && Current == EDCPanel::None && !bPanelShown && Now() - PanelT0 > 0.4)
		{
			bPerkQueued = false;
			Open(EDCPanel::PerkDraft);
		}
	}

	// Missions panel open = postings seen.
	if (R && R->HasActiveRun())
	{
		const double RT = R->GetRunTime();
		for (const FDCMission& M : R->GetMissions())
		{
			if (!FirstSeenAt.Contains(M.Id)) { FirstSeenAt.Add(M.Id, RT); }
			if (Current == EDCPanel::Missions || (Current == EDCPanel::MissionPlanner && M.Id == PlanMissionId)) { SeenMissionIds.Add(M.Id); }
		}
	}

	if (Current == EDCPanel::MissionPlanner) { RecomputePlan(); }

	// FPS readout (smoothed so it's readable)
	if (InDeltaTime > 0.f) { FpsSmoothed = FMath::Lerp(FpsSmoothed, 1.f / InDeltaTime, FMath::Min(1.f, InDeltaTime * 2.f)); }

	// Pause when the window loses focus (Options > Accessibility).
	if (FSlateApplication::IsInitialized())
	{
		const bool bActive = FSlateApplication::Get().IsActive();
		if (!bActive && bAppWasActive && DCSettings::PauseOnFocusLoss() && IsPlaying() && R && !R->IsPaused() && !bModalShown)
		{
			Open(EDCPanel::Pause);
		}
		bAppWasActive = bActive;
	}

	// Timed confirm (graphics "keep these settings?") runs out = No.
	if (Confirm.IsValid() && Confirm->Timeout > 0.0 && T - Confirm->T0 >= Confirm->Timeout) { EndConfirm(false); }

	TickExpiry();

	if (T >= NextSigCheck)
	{
		NextSigCheck = T + 0.2;
		const uint32 Chip = ComputeChipSignature();
		if (Chip != LastChipSig) { LastChipSig = Chip; RebuildChips(); }
		TickHudCrew();
		const bool bMouseDown = FSlateApplication::IsInitialized() && FSlateApplication::Get().GetPressedMouseButtons().Num() > 0;
		if (Current != EDCPanel::None && !bMouseDown && Current != EDCPanel::Title)
		{
			const uint32 Sig = ComputeSignature();
			if (Sig != LastSig) { Rebuild(); }
		}
	}

	// Cash ticker eases toward the real value.
	if (R && R->HasActiveRun())
	{
		const double Target = R->GetCash();
		if (!bCashInit) { DisplayCash = Target; bCashInit = true; }
		const double Diff = Target - DisplayCash;
		if (FMath::Abs(Diff) >= 1.0 && T - CashFlashT0 > 0.6) { CashFlashT0 = T; bCashFlashUp = Diff > 0.0; }
		DisplayCash = FMath::Abs(Diff) < 1.0 ? Target : DisplayCash + Diff * FMath::Min(1.0, InDeltaTime * 5.0) + FMath::Sign(Diff) * FMath::Min(FMath::Abs(Diff), 1.0);
	}

	// Toasts: one on screen at a time; the next queued one shows when it ends.
	for (int32 i = Toasts.Num() - 1; i >= 0; --i)
	{
		if (T >= *Toasts[i].End)
		{
			ToastBox->RemoveSlot(Toasts[i].Widget.ToSharedRef());
			Toasts.RemoveAt(i);
		}
	}
	if (Toasts.Num() == 0 && PendingToasts.Num() > 0)
	{
		const FPendingToast P = PendingToasts[0];
		PendingToasts.RemoveAt(0);
		ShowToastNow(P.Msg, P.Icon, P.bWarn);
	}
}

void SDCRoot::Toast(const FString& Msg, FName IconName, bool bWarn)
{
	UE_LOG(LogDC, Log, TEXT("[Toast] %s"), *Msg);
	if (!ToastBox.IsValid()) { return; }
	if (Toasts.Num() == 0 && PendingToasts.Num() == 0) { ShowToastNow(Msg, IconName, bWarn); return; }
	// Busy: queue it (skip exact repeats) and cut the current toast's hold short so the line keeps moving.
	if (Toasts.Num() > 0 && Toasts.Last().Msg == Msg) { return; }
	for (const FPendingToast& P : PendingToasts) { if (P.Msg == Msg) { return; } }
	PendingToasts.Add({ Msg, IconName, bWarn });
	if (PendingToasts.Num() > 6) { PendingToasts.RemoveAt(0); }
	const double T = Now();
	for (FToast& Cur : Toasts) { *Cur.End = FMath::Min(*Cur.End, FMath::Max(Cur.Time + ToastMinHold, T + ToastFadeOut)); }
}

void SDCRoot::ShowToastNow(const FString& Msg, FName IconName, bool bWarn)
{
	const double T0 = Now();
	TSharedRef<double> End = MakeShared<double>(T0 + ToastHold);
	const FLinearColor IC = bWarn ? Bad : Accent;
	TSharedRef<SWidget> W = SNew(SBorder)
		.BorderImage(NoBrush())
		.Padding(0.f)
		.ColorAndOpacity_Lambda([T0, End]()
		{
			const double N = Now();
			const float In = EaseOut((float)(N - T0) / 0.16f);
			const float Out = 1.f - FMath::Clamp((float)((N - (*End - ToastFadeOut)) / ToastFadeOut), 0.f, 1.f);
			return FLinearColor(1, 1, 1, FMath::Min(In, Out));
		})
		.RenderTransform_Lambda([T0]() { return TOptional<FSlateRenderTransform>(FSlateRenderTransform(FVector2f(0.f, (1.f - EaseOut((float)(Now() - T0) / 0.22f)) * -14.f))); })
		[
			SNew(SBorder)
			.BorderImage(Box(Hex(0x0E1115, 0.94f), -1.f, bWarn ? A(Bad, 0.45f) : LineHi, 1.f))
			.Padding(FMargin(8.f, 7.f, 18.f, 7.f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 10.f, 0.f)
				[
					SNew(SBox).WidthOverride(26.f).HeightOverride(26.f)
					[
						SNew(SBorder).BorderImage(Box(A(IC, 0.16f), -1.f)).HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(0.f)
						[Icon(IconName.IsNone() ? FName(bWarn ? TEXT("triangle-alert") : TEXT("info")) : IconName, 12.f, IC)]
					]
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Txt(Msg, Font(11.f, EW::Medium), Text)]
			]
		];
	ToastBox->AddSlot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 0.f, 0.f, 8.f)[W];
	Toasts.Add({ W, T0, End, Msg });
}

// ─────────────────────────────────────────────────────────────────────────────
// HUD
// ─────────────────────────────────────────────────────────────────────────────

TSharedRef<SWidget> SDCRoot::BuildHudClock()
{
	// Clock + day; BuildHudStats() frames it inside the top-right bar. Upkeep lives in the crew list (bottom-left).
	auto R = [this]() { return Run(); };

	TSharedRef<SWidget> Clock = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 10.f, 0.f)
		[
			SNew(SBox).WidthOverride(30.f).HeightOverride(30.f)
			[
				SNew(SBorder)
				.BorderImage_Lambda([R]() { const UDCRunSubsystem* X = R(); return Box(A((X && X->IsNight()) ? Info : Accent, 0.14f), -1.f); })
				.HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(0.f)
				[
					IconA(TAttribute<FName>::CreateLambda([R]()
					{
						const UDCRunSubsystem* X = R();
						if (!X) { return FName(TEXT("sun")); }
						const float H = X->GetHourOfDay();
						if (H >= 5.f && H < 7.f) { return FName(TEXT("sunrise")); }
						if (H >= 19.f && H < 20.5f) { return FName(TEXT("sunset")); }
						return FName(X->IsNight() ? TEXT("moon") : TEXT("sun"));
					}), 14.f, TAttribute<FSlateColor>::CreateLambda([R]() { const UDCRunSubsystem* X = R(); return FSlateColor((X && X->IsNight()) ? Info : Accent); }))
				]
			]
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom)
				[TxtA(TAttribute<FText>::CreateLambda([R]() { const UDCRunSubsystem* X = R(); return FText::FromString(X ? HourMin(X->GetHourOfDay()) : TEXT("--:--")); }), Mono(17.f),
					TAttribute<FSlateColor>::CreateLambda([this]() { return FSlateColor(IsSpeedPausedNow() ? TextDim : Text); }))]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom).Padding(8.f, 0.f, 0.f, 2.f)
				[
					// Player pause: the day label reads PAUSED (gently pulsing) until time runs again.
					SNew(SBorder).BorderImage(NoBrush()).Padding(0.f)
					.ColorAndOpacity_Lambda([this]() { return FLinearColor(1, 1, 1, IsSpeedPausedNow() ? 0.6f + 0.4f * Pulse(0.8f) : 1.f); })
					[
						TxtA(TAttribute<FText>::CreateLambda([this, R]()
						{
							if (IsSpeedPausedNow()) { return FText::FromString(TEXT("PAUSED")); }
							const UDCRunSubsystem* X = R();
							return FText::FromString(FString::Printf(TEXT("DAY %d"), X ? X->GetDay() : 1));
						}), Caps(9.f), TAttribute<FSlateColor>::CreateLambda([this]() { return FSlateColor(IsSpeedPausedNow() ? Accent : TextDim); }))
					]
				]
			]
		];

	TSharedRef<SWidget> W = Clock;
	W->SetToolTip(TipWidget(
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()[Txt(TEXT("Payday at dawn"), Font(11.f, EW::SemiBold), Text)]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f, 0.f, 0.f)[Txt(TEXT("Crew wages and building upkeep are paid at 06:00. Can't pay = game over."), Font(10.f), TextDim, true)]));
	return W;
}

TSharedRef<SWidget> SDCRoot::BuildHudStats()
{
	auto R = [this]() { return Run(); };
	auto Sep = []() { return SNew(SBox).Padding(FMargin(12.f, 4.f)).HeightOverride(26.f)[VLine(Line)]; };

	TSharedRef<SWidget> Cash = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 8.f, 0.f)[Icon(TEXT("banknote"), 15.f, Accent)]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[
			SNew(SBox).MinDesiredWidth(96.f)
			[
				TxtA(TAttribute<FText>::CreateLambda([this]() { return FText::FromString(Money((int64)FMath::RoundToDouble(DisplayCash))); }), Mono(16.f),
					TAttribute<FSlateColor>::CreateLambda([this]()
					{
						const float F = 1.f - FMath::Clamp((float)(Now() - CashFlashT0) / 0.9f, 0.f, 1.f);
						return FSlateColor(FLinearColor::LerpUsingHSV(Text, bCashFlashUp ? Good : Bad, F * 0.85f));
					}))
			]
		];
	Cash->SetToolTip(Tip(TEXT("Cash"), TEXT("Your take from every job. Pays for gear, new wings, hires and wages."), TEXT("banknote")));

	TSharedRef<SWidget> Crew = IconTextA(TEXT("users"), TAttribute<FText>::CreateLambda([R]()
		{
			const UDCRunSubsystem* X = R();
			if (!X) { return FText::GetEmpty(); }
			int32 Avail = 0; for (const FDCCriminal& C : X->GetRoster()) { if (C.IsAvailable()) { ++Avail; } }
			return FText::FromString(FString::Printf(TEXT("%d/%d"), X->GetRoster().Num(), X->GetRosterCap()));
		}), Mono(13.f), FSlateColor(TextDim), FSlateColor(Text), 14.f, 7.f);
	Crew->SetToolTip(Tip(TEXT("Crew"), TEXT("Roster / bunk capacity"), TEXT("users")));

	TSharedRef<SWidget> Active = IconTextA(TEXT("car-front"), TAttribute<FText>::CreateLambda([R]()
		{
			const UDCRunSubsystem* X = R();
			return X ? FText::FromString(FString::Printf(TEXT("%d/%d"), X->GetActiveMissionCount(), X->GetMaxActiveMissions())) : FText::GetEmpty();
		}), Mono(13.f), FSlateColor(TextDim), FSlateColor(Text), 14.f, 7.f);
	Active->SetToolTip(Tip(TEXT("Missions running"), TEXT("Active / max at once (Kit Room adds more)"), TEXT("car-front")));

	TSharedRef<SWidget> Wins = IconTextA(TEXT("trophy"), TAttribute<FText>::CreateLambda([R]()
		{
			const UDCRunSubsystem* X = R();
			return X ? FText::FromString(FString::FromInt(X->GetMissionsSucceeded())) : FText::GetEmpty();
		}), Mono(13.f), FSlateColor(TextDim), FSlateColor(Text), 13.f, 7.f);
	Wins->SetToolTip(Tip(TEXT("Successful jobs"), TEXT("Unlock tiers, wings and perks"), TEXT("trophy")));

	TSharedRef<SWidget> Bell = SNew(SOverlay)
		+ SOverlay::Slot()
		[IconButton(TEXT("bell"), [this]() { if (Current == EDCPanel::Reports) { Close(); } else { Open(EDCPanel::Reports); } }, Tip(TEXT("Reports"), TEXT("N")), EBtn::Ghost, 15.f)]
		+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(FMargin(0.f, -3.f, -4.f, 0.f))
		[Badge(TAttribute<int32>::CreateLambda([R]() { const UDCRunSubsystem* X = R(); return X ? X->GetUnseenReportCount() : 0; }))];

	return SNew(SBorder)
		.BorderImage(Box(GlassSoft, 12.f, Line, 1.f))
		.Padding(FMargin(8.f, 6.f, 6.f, 6.f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[BuildHudClock()]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Sep()]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Cash]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Sep()]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Crew]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(16.f, 0.f, 0.f, 0.f)[Active]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(16.f, 0.f, 0.f, 0.f)[Wins]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Sep()]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Bell]
		];
}

TSharedRef<SWidget> SDCRoot::UrgencyDot(TFunction<double()> SecondsLeft)
{
	// Countdown pill shown above a dock button while something behind it is about to expire (amber < 60 s, red < 20 s).
	auto Col = [SecondsLeft]() { return UrgencyColor(SecondsLeft(), TextDim); };
	auto Crit = [SecondsLeft]() { return Urgency(SecondsLeft()) == EUrgency::Critical; };
	return SNew(SBorder)
		.BorderImage(NoBrush())
		.Padding(0.f)
		.Visibility_Lambda([SecondsLeft]() { return (Prefs().bExpiryWarnings && Urgency(SecondsLeft()) != EUrgency::Calm) ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
		.ColorAndOpacity_Lambda([Crit]() { return FLinearColor(1, 1, 1, Crit() ? 0.55f + 0.45f * Pulse(1.4f) : 1.f); })
		[
			SNew(SBorder)
			.BorderImage_Lambda([Col]() { return Box(Hex(0x0E1115, 0.96f), -1.f, A(Col(), 0.8f), 1.f); })
			.Padding(FMargin(5.f, 1.f, 6.f, 1.f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 3.f, 0.f)
				[Icon(TEXT("hourglass"), 8.f, TAttribute<FSlateColor>::CreateLambda([Col]() { return FSlateColor(Col()); }))]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[TxtA(TAttribute<FText>::CreateLambda([SecondsLeft]() { return FText::FromString(DC::Clock(SecondsLeft())); }), Mono(8.5f), TAttribute<FSlateColor>::CreateLambda([Col]() { return FSlateColor(Col()); }))]
			]
		];
}

TSharedRef<SWidget> SDCRoot::DockButton(FName IconName, const FString& Name, const FString& Key, EDCPanel Panel, TAttribute<int32> BadgeCount, TAttribute<bool> Enabled, TFunction<double()> ExpiresIn)
{
	auto IsActive = [this, Panel]() { return Current == Panel || (Panel == EDCPanel::Missions && Current == EDCPanel::MissionPlanner); };
	TSharedRef<SButton> B = SNew(SButton)
		.ButtonStyle(&ButtonStyle(EBtn::Dock))
		.ContentPadding(FMargin(0.f))
		.IsFocusable(false)
		.IsEnabled(Enabled)
		.OnHovered_Lambda([]() { Sfx(DCSfx::Hover, 0.35f); })
		.OnClicked_Lambda([this, Panel, IsActive]()
		{
			Sfx(DCSfx::Click);
			if (IsActive()) { Close(); } else { Open(Panel); }
			return FReply::Handled();
		})
		[
			SNew(SBox).WidthOverride(52.f).HeightOverride(52.f).HAlign(HAlign_Center).VAlign(VAlign_Center)
			[
				Icon(IconName, 18.f, TAttribute<FSlateColor>::CreateLambda([IsActive]() { return FSlateColor(IsActive() ? Accent : Text); }))
			]
		];
	B->SetToolTip(TipWidget(
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Txt(Name, Font(11.f, EW::SemiBold), Text)]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(10.f, 0.f, 0.f, 0.f)
		[Key.IsEmpty() ? SNullWidget::NullWidget : Pill(Txt(Key, Mono(9.f), TextDim), A(FLinearColor::White, 0.06f), FMargin(6.f, 1.f), Line)]));

	return SNew(SOverlay)
		+ SOverlay::Slot()
		[
			SNew(SBorder)
			.BorderImage_Lambda([IsActive]() { return IsActive() ? Box(A(Accent, 0.14f), 10.f, A(Accent, 0.5f), 1.f) : NoBrush(); })
			.Padding(0.f)[B]
		]
		+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(FMargin(0.f, -2.f, -2.f, 0.f))[Badge(BadgeCount, Accent)]
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Top).Padding(FMargin(-12.f, -22.f, -12.f, 0.f))[ExpiresIn ? UrgencyDot(ExpiresIn) : SNullWidget::NullWidget];
}

TSharedRef<SWidget> SDCRoot::BuildDock()
{
	auto R = [this]() { return Run(); };
	auto Built = [R](EDCRoomType T) { return TAttribute<EVisibility>::CreateLambda([R, T]() { const UDCRunSubsystem* X = R(); return (X && X->IsRoomBuilt(T)) ? EVisibility::Visible : EVisibility::Collapsed; }); };
	auto AnyRoomPanel = [R]()
	{
		const UDCRunSubsystem* X = R();
		return (X && (X->IsRoomBuilt(EDCRoomType::OpsRoom) || X->IsRoomBuilt(EDCRoomType::Clinic) || X->IsRoomBuilt(EDCRoomType::LawyerOffice) || X->IsRoomBuilt(EDCRoomType::ServerRoom))) ? EVisibility::Visible : EVisibility::Collapsed;
	};
	auto Zero = TAttribute<int32>(0);

	return SNew(SBorder)
		.BorderImage(Box(GlassSoft, 16.f, Line, 1.f))
		.Padding(FMargin(6.f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4.f, 0.f, 2.f, 0.f)[BuildHudSpeed()]
			+ SHorizontalBox::Slot().AutoWidth().Padding(8.f, 10.f)[VLine(Line)]
			+ SHorizontalBox::Slot().AutoWidth().Padding(2.f, 0.f)[DockButton(TEXT("map"), TEXT("Missions"), TEXT("M"), EDCPanel::Missions, TAttribute<int32>::CreateLambda([this]() { return NewPostingCount(); }), true, [this]() { return SoonestLeadExpiry(); })]
			+ SHorizontalBox::Slot().AutoWidth().Padding(2.f, 0.f)[DockButton(TEXT("users"), TEXT("Crew"), TEXT("C"), EDCPanel::Crew, Zero)]
			+ SHorizontalBox::Slot().AutoWidth().Padding(2.f, 0.f)[DockButton(TEXT("wrench"), TEXT("Gear"), TEXT("G"), EDCPanel::Workbench, Zero)]
			+ SHorizontalBox::Slot().AutoWidth().Padding(2.f, 0.f)[DockButton(TEXT("user-plus"), TEXT("Recruits"), TEXT("R"), EDCPanel::Recruits, Zero, true, [this]() { return RecruitExpiry(); })]
			+ SHorizontalBox::Slot().AutoWidth().Padding(2.f, 0.f)[DockButton(TEXT("hammer"), TEXT("Expand"), TEXT("B"), EDCPanel::Build, TAttribute<int32>::CreateLambda([this, R]()
				{
					const UDCRunSubsystem* X = R();
					return (X && X->HasPendingPerkOffer()) || CanBuildSomething() ? 1 : 0;
				}))]
			+ SHorizontalBox::Slot().AutoWidth().Padding(8.f, 10.f)[SNew(SBox).Visibility_Lambda(AnyRoomPanel)[VLine(Line)]]
			+ SHorizontalBox::Slot().AutoWidth().Padding(2.f, 0.f)[SNew(SBox).Visibility(Built(EDCRoomType::OpsRoom))[DockButton(DC::RoomIcon(EDCRoomType::OpsRoom), TEXT("Ops Room"), TEXT(""), EDCPanel::OpsRoom, Zero)]]
			+ SHorizontalBox::Slot().AutoWidth().Padding(2.f, 0.f)[SNew(SBox).Visibility(Built(EDCRoomType::Clinic))[DockButton(DC::RoomIcon(EDCRoomType::Clinic), TEXT("Clinic"), TEXT(""), EDCPanel::Clinic,
				TAttribute<int32>::CreateLambda([R]() { const UDCRunSubsystem* X = R(); int32 N = 0; if (X) { for (const FDCCriminal& C : X->GetRoster()) { if (C.Status == EDCCriminalStatus::Injured) { ++N; } } } return N; }))]]
			+ SHorizontalBox::Slot().AutoWidth().Padding(2.f, 0.f)[SNew(SBox).Visibility(Built(EDCRoomType::LawyerOffice))[DockButton(DC::RoomIcon(EDCRoomType::LawyerOffice), TEXT("Lawyer"), TEXT(""), EDCPanel::Lawyer,
				TAttribute<int32>::CreateLambda([R]() { const UDCRunSubsystem* X = R(); return X ? X->GetFallen().Num() : 0; }))]]
			+ SHorizontalBox::Slot().AutoWidth().Padding(2.f, 0.f)[SNew(SBox).Visibility(Built(EDCRoomType::ServerRoom))[DockButton(DC::RoomIcon(EDCRoomType::ServerRoom), TEXT("Server Room"), TEXT(""), EDCPanel::ServerRoom, Zero)]]
		];
}

// ── Active-mission chips (left column) ──────────────────────────────────────

void SDCRoot::RebuildChips()
{
	if (!ChipBox.IsValid()) { return; }
	ChipBox->ClearChildren();
	const UDCRunSubsystem* R = Run();
	if (!R || !R->HasActiveRun()) { return; }
	for (const FDCMission& M : R->GetMissions())
	{
		if (M.bInProgress) { ChipBox->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 8.f)[MissionChip(M)]; }
	}
	for (const FDCRoom& Room : R->GetRooms())
	{
		if (Room.State == EDCRoomState::Building) { ChipBox->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 8.f)[BuildChip(Room)]; }
	}
}

TSharedRef<SWidget> SDCRoot::MissionChip(const FDCMission& M)
{
	const int32 Id = M.Id;
	auto R = [this]() { return Run(); };
	const FLinearColor TC = M.bFinal ? Accent : TierColor(M.Tier);
	TSharedRef<SHorizontalBox> Crew = SNew(SHorizontalBox);
	const UDCRunSubsystem* Run0 = Run();
	for (int32 CId : M.CrewIds)
	{
		const FDCCriminal* C = Run0 ? Run0->FindCriminal(CId) : nullptr;
		if (C) { Crew->AddSlot().AutoWidth().Padding(0.f, 0.f, 3.f, 0.f)[Avatar(*C, 18.f)]; }
	}
	const float Chance = M.LockedChance;
	TSharedRef<SWidget> Content = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 10.f, 0.f)
		[
			SNew(SOverlay)
			+ SOverlay::Slot()[SNew(SDCRing).Size(38.f).Thickness(3.f).Color(TC).Fill_Lambda([R, Id]() { const UDCRunSubsystem* X = R(); return X ? X->GetMissionProgress(Id) : 0.f; })]
			+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)[Icon(M.Icon, 14.f, Text)]
		]
		+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock).Text(FText::FromString(M.DisplayName)).Font(Font(10.5f, EW::SemiBold)).ColorAndOpacity(Text).OverflowPolicy(ETextOverflowPolicy::Ellipsis)
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f, 0.f, 0.f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[Crew]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(6.f, 0.f, 0.f, 0.f)
				[Txt(FString::Printf(TEXT("+%s"), *Money(FMath::RoundToInt(M.Payout * M.Cut))), Mono(10.f), Accent)]
			]
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(10.f, 0.f, 0.f, 0.f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right)
			[TxtA(TAttribute<FText>::CreateLambda([R, Id]() { const UDCRunSubsystem* X = R(); return FText::FromString(X ? DC::Clock(X->GetMissionTimeRemaining(Id)) : TEXT("")); }), Mono(12.f), FSlateColor(Text))]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right).Padding(0.f, 2.f, 0.f, 0.f)
			[Txt(Pct(Chance), Mono(9.f), ChanceColor(Chance))]
		];

	TSharedRef<SWidget> B = Button(SNew(SBox).WidthOverride(272.f)[Content], [this, Id]() { OpenPlanner(Id); }, EBtn::Card, true, nullptr, FMargin(8.f, 7.f, 12.f, 7.f));
	B->SetToolTip(Tip(M.DisplayName, FString::Printf(TEXT("%s odds · your take %s if it lands (payout %s) · click for details"), *Pct(Chance), *Money(FMath::RoundToInt(M.Payout * M.Cut)), *Money(M.Payout)), M.Icon, TC));
	return SNew(SBorder).BorderImage(Box(FLinearColor(0, 0, 0, 0.25f), 11.f)).Padding(0.f)[B];
}

TSharedRef<SWidget> SDCRoot::BuildChip(const FDCRoom& Room)
{
	const EDCRoomType Type = Room.Type;
	auto R = [this]() { return Run(); };
	auto Progress = [R, Type]()
	{
		const UDCRunSubsystem* X = R();
		if (!X) { return 0.f; }
		const FDCRoom Q = X->GetRoom(Type);
		const double Len = Q.BuildEndsAt - Q.BuildStartedAt;
		return Len > 0.0 ? (float)FMath::Clamp((X->GetRunTime() - Q.BuildStartedAt) / Len, 0.0, 1.0) : 0.f;
	};
	const FString Name = Room.DisplayName.IsEmpty() ? FString(DC::RoomName(Type)) : Room.DisplayName;
	TSharedRef<SWidget> Content = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 10.f, 0.f)
		[
			SNew(SOverlay)
			+ SOverlay::Slot()[SNew(SDCRing).Size(38.f).Thickness(3.f).Color(Warn).Fill_Lambda(Progress)]
			+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)[Icon(TEXT("hammer"), 14.f, Warn)]
		]
		+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()[Txt(Name, Font(10.5f, EW::SemiBold), Text)]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 3.f, 0.f, 0.f)[Txt(FString::Printf(TEXT("LEVEL %d"), Room.Level + 1), Caps(8.f), TextDim)]
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(10.f, 0.f, 0.f, 0.f)
		[TxtA(TAttribute<FText>::CreateLambda([R, Type]() { const UDCRunSubsystem* X = R(); return FText::FromString(X ? DC::Clock(X->GetRoom(Type).BuildEndsAt - X->GetRunTime()) : TEXT("")); }), Mono(12.f), FSlateColor(Text))];
	return SNew(SBorder).BorderImage(Box(FLinearColor(0, 0, 0, 0.25f), 11.f)).Padding(0.f)
		[Button(SNew(SBox).WidthOverride(272.f)[Content], [this]() { Open(EDCPanel::Build); }, EBtn::Card, true, Tip(TEXT("Under construction"), Name, TEXT("hammer"), Warn), FMargin(8.f, 7.f, 12.f, 7.f))];
}

// ─────────────────────────────────────────────────────────────────────────────
// Expiry warnings (v3): leads and the recruit pool escalate calm -> amber (< 60 s) -> red + pulse (< 20 s)
// ─────────────────────────────────────────────────────────────────────────────

double SDCRoot::SoonestLeadExpiry() const
{
	const UDCRunSubsystem* R = Run();
	if (!R || !R->HasActiveRun()) { return -1.0; }
	const double RT = R->GetRunTime();
	double Best = -1.0;
	for (const FDCMission& M : R->GetMissions())
	{
		if (M.bInProgress || M.ExpiresAt < 0.0) { continue; }
		const double Left = FMath::Max(0.0, M.ExpiresAt - RT);
		if (Best < 0.0 || Left < Best) { Best = Left; }
	}
	return Best;
}

double SDCRoot::RecruitExpiry() const
{
	const UDCRunSubsystem* R = Run();
	if (!R || !R->HasActiveRun() || R->GetRecruits().Num() == 0) { return -1.0; }
	return FMath::Max(0.0, R->GetRecruitRefreshIn());
}

void SDCRoot::TickExpiry()
{
	const UDCRunSubsystem* R = Run();
	if (!R || !R->HasActiveRun() || R->GetPhase() != EDCGamePhase::Playing)
	{
		LastBoard.Reset(); Ghosts.Reset(); WarnedMissions.Reset(); bWarnedRecruits = false;
		return;
	}
	const double RT = R->GetRunTime();
	const bool bWarn = Prefs().bExpiryWarnings;
	auto Chime = []()
	{
		if (UDCAudioSubsystem* Au = UDCAudioSubsystem::Get(GetContext())) { Au->PlaySFX(DCSfx::Error, 0.35f, 1.25f); }
	};

	// One toast + soft chime when a lead crosses the critical line.
	TArray<FDCMission> Board;
	for (const FDCMission& M : R->GetMissions())
	{
		if (M.bInProgress || M.bFinal) { continue; }
		Board.Add(M);
		if (M.ExpiresAt < 0.0) { continue; }
		const double Left = M.ExpiresAt - RT;
		if (Left > 0.0 && Left <= UrgentCritical && !WarnedMissions.Contains(M.Id))
		{
			WarnedMissions.Add(M.Id);
			if (bWarn) { Toast(FString::Printf(TEXT("Lead expiring · %s"), *M.DisplayName), TEXT("hourglass"), true); Chime(); }
		}
	}

	// Recruit pool rolls over.
	const double RI = RecruitExpiry();
	if (RI >= 0.0 && RI <= UrgentCritical)
	{
		if (!bWarnedRecruits)
		{
			bWarnedRecruits = true;
			if (bWarn) { Toast(TEXT("Recruits moving on soon · hire now or lose them"), TEXT("user-round-x"), true); Chime(); }
		}
	}
	else if (RI > UrgentCritical) { bWarnedRecruits = false; }

	// Postings that left the board unstarted fade out in place instead of vanishing.
	const double T = Now();
	for (int32 i = 0; i < LastBoard.Num(); ++i)
	{
		const FDCMission& Old = LastBoard[i];
		if (!R->FindMission(Old.Id) && Old.ExpiresAt >= 0.0 && RT >= Old.ExpiresAt - 1.0)
		{
			Ghosts.Add({ Old, i, T });
			WarnedMissions.Remove(Old.Id);
		}
	}
	LastBoard = MoveTemp(Board);
	const int32 Before = Ghosts.Num();
	Ghosts.RemoveAll([T](const FGhost& G) { return T - G.T0 > 1.0; });
	if (Ghosts.Num() != Before && Current == EDCPanel::Missions) { Rebuild(); }
}

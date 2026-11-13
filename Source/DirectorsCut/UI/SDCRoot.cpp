#include "UI/SDCRoot.h"
#include "UI/SDCRootInternal.h"
#include "Misc/ConfigCacheIni.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "DirectorsCut.h"

static const TCHAR* GTutSection = TEXT("/Script/DirectorsCut.DCUI");

// ─────────────────────────────────────────────────────────────────────────────
// Construction & plumbing
// ─────────────────────────────────────────────────────────────────────────────

void SDCRoot::Construct(const FArguments& InArgs)
{
	Owner = InArgs._Owner;
	PlanCache = MakeShared<FPlanCache>();
	Dailies = MakeShared<FDailiesState>();

	auto PanelAlpha = [this]() -> float
	{
		const float T = (float)(Now() - PanelT0);
		return bPanelShown ? EaseOut(T / 0.22f) : 1.f - FMath::Clamp(T / 0.14f, 0.f, 1.f);
	};
	auto ModalAlpha = [this]() -> float
	{
		const float T = (float)(Now() - ModalT0);
		return bModalShown ? EaseOut(T / 0.25f) : 1.f - FMath::Clamp(T / 0.16f, 0.f, 1.f);
	};

	ChildSlot
	[
		SNew(SOverlay)
		.Visibility(EVisibility::SelfHitTestInvisible)

		// ── Top bar ──
		+ SOverlay::Slot().VAlign(VAlign_Top)
		[
			SAssignNew(TopBar, SBorder)
			.BorderImage(FStyleDefaults::GetNoBrush())
			.Padding(0.f)
			.Visibility_Lambda([this]() { return HasRun() && !(bModalShown && Current != EDCPanel::Settings && Current != EDCPanel::PerkDraft) ? EVisibility::Visible : EVisibility::Collapsed; })
			[
				BuildTopBar()
			]
		]

		// ── Non-modal panel (docked right, slides in) ──
		+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Fill).Padding(FMargin(16.f, 84.f, 18.f, 18.f))
		[
			SAssignNew(PanelFrame, SBorder)
			.BorderImage(FStyleDefaults::GetNoBrush())
			.Padding(0.f)
			.ColorAndOpacity_Lambda([PanelAlpha]() { return FLinearColor(1, 1, 1, PanelAlpha()); })
			.RenderTransform_Lambda([this, PanelAlpha]() { return TOptional<FSlateRenderTransform>(FSlateRenderTransform(FVector2f((1.f - PanelAlpha()) * 60.f, 0.f))); })
			.Visibility_Lambda([this, PanelAlpha]() { return (bPanelShown || PanelAlpha() > 0.01f) ? EVisibility::Visible : EVisibility::Collapsed; })
			[
				SAssignNew(PanelSlot, SBox).WidthOverride(1200.f)
			]
		]

		// ── Tutorial strip ──
		+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Bottom).Padding(FMargin(18.f, 0.f, 0.f, 18.f))
		[
			SAssignNew(TutorialStrip, SBorder)
			.BorderImage(FStyleDefaults::GetNoBrush())
			.Padding(0.f)
			.Visibility_Lambda([this]() { return (TutorialStep >= 0 && HasRun() && !bModalShown) ? EVisibility::Visible : EVisibility::Collapsed; })
			[
				BuildTutorial()
			]
		]

		// ── Modal layer ──
		+ SOverlay::Slot()
		[
			SAssignNew(ModalFrame, SBorder)
			.BorderImage(Brush(EBrush::Dim))
			.BorderBackgroundColor_Lambda([this]() { return FLinearColor(1, 1, 1, Current == EDCPanel::Title ? 0.0f : 1.f); })
			.Padding(0.f)
			.ColorAndOpacity_Lambda([ModalAlpha]() { return FLinearColor(1, 1, 1, ModalAlpha()); })
			.Visibility_Lambda([this, ModalAlpha]() { return (bModalShown || ModalAlpha() > 0.01f) ? EVisibility::Visible : EVisibility::Collapsed; })
			[
				SAssignNew(ModalSlot, SBox)
			]
		]

		// ── Toasts ──
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Top).Padding(FMargin(0.f, 92.f, 0.f, 0.f))
		[
			SAssignNew(ToastBox, SVerticalBox).Visibility(EVisibility::HitTestInvisible)
		]
	];

	BindRun();
	bool bDone = false;
	GConfig->GetBool(GTutSection, TEXT("TutorialDone"), bDone, GGameUserSettingsIni);
	TutorialStep = bDone ? -1 : 0;
}

SDCRoot::~SDCRoot() { UnbindRun(); }

UDCRunSubsystem* SDCRoot::Run() const
{
	UObject* Ctx = GetContext();
	return Ctx ? UDCRunSubsystem::Get(Ctx) : nullptr;
}

bool SDCRoot::HasRun() const
{
	const UDCRunSubsystem* R = Run();
	return R && R->HasActiveRun();
}

void SDCRoot::BindRun()
{
	UDCRunSubsystem* R = Run();
	if (!R) { return; }
	HState  = R->OnRunStateChanged.AddSP(this, &SDCRoot::OnRunState);
	HNight  = R->OnNightResolved.AddSP(this, &SDCRoot::OnNightResolved);
	HPerk   = R->OnPerkOffered.AddSP(this, &SDCRoot::OnPerkOffered);
	HOver   = R->OnGameOver.AddSP(this, &SDCRoot::OnGameOver);
	HWon    = R->OnRunWon.AddSP(this, &SDCRoot::OnRunWon);
	HCommit = R->OnPlanCommitted.AddLambda([this](const FDCCrewPlan&) { AdvanceTutorial(1); });
	HRoom   = R->OnRoomChanged.AddLambda([this](const FDCRoom& Room)
	{
		Sfx(DCSfx::StingRoom);
		Toast(FString::Printf(TEXT("%s %s"), *(Room.DisplayName.IsEmpty() ? FString(DC::RoomName(Room.Type)) : Room.DisplayName),
			Room.Level <= 1 ? TEXT("is open for business") : *FString::Printf(TEXT("upgraded to level %d"), Room.Level)), false);
	});
	HHire   = R->OnCriminalHired.AddLambda([this](const FDCCriminal& C) { Toast(FString::Printf(TEXT("%s joins the cast"), *C.GetDisplayName()), false); });
	HLevel  = R->OnCriminalLevelUp.AddLambda([this](int32, int32) { /* shown in the Dailies */ });
}

void SDCRoot::UnbindRun()
{
	UDCRunSubsystem* R = Run();
	if (!R) { return; }
	R->OnRunStateChanged.Remove(HState);
	R->OnNightResolved.Remove(HNight);
	R->OnPerkOffered.Remove(HPerk);
	R->OnGameOver.Remove(HOver);
	R->OnRunWon.Remove(HWon);
	R->OnPlanCommitted.Remove(HCommit);
	R->OnRoomChanged.Remove(HRoom);
	R->OnCriminalHired.Remove(HHire);
	R->OnCriminalLevelUp.Remove(HLevel);
}

bool SDCRoot::IsModal(EDCPanel P)
{
	switch (P)
	{
	case EDCPanel::Title:
	case EDCPanel::Dailies:
	case EDCPanel::PerkDraft:
	case EDCPanel::GameOver:
	case EDCPanel::Credits:
	case EDCPanel::Settings:
		return true;
	default:
		return false;
	}
}

void SDCRoot::SetCurrent(EDCPanel P)
{
	Current = P;
	if (UDCUISubsystem* O = Owner.Get())
	{
		O->OpenPanelType = P;
		O->OnPanelChanged.Broadcast(P);
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Panel management
// ─────────────────────────────────────────────────────────────────────────────

void SDCRoot::Open(EDCPanel P)
{
	if (P == EDCPanel::None) { Close(true); return; }
	if (!HasRun() && !IsModal(P)) { return; }

	const EDCPanel Prev = Current;
	if (P == EDCPanel::Settings) { ReturnPanel = (Prev == EDCPanel::Title || Prev == EDCPanel::GameOver || Prev == EDCPanel::Credits) ? Prev : EDCPanel::None; }
	if (P == EDCPanel::CrewPlanner && Prev != EDCPanel::CrewPlanner)
	{
		// No explicit job: first uncommitted job on the board.
		if (UDCRunSubsystem* R = Run())
		{
			const FDCJob* J = R->FindJob(PlanJobId);
			if (!J || J->bCommitted)
			{
				PlanJobId = INDEX_NONE;
				for (const FDCJob& Job : R->GetJobs()) { if (!Job.bCommitted) { PlanJobId = Job.Id; break; } }
				if (PlanJobId == INDEX_NONE && R->GetJobs().Num()) { PlanJobId = R->GetJobs()[0].Id; }
				PlanCrew.Reset();
			}
		}
	}
	if (P == EDCPanel::CrewPlanner) { AdvanceTutorial(0); }

	SavedScroll.Reset();
	Scrolls.Reset();
	SetCurrent(P);
	const double T = Now();
	if (IsModal(P))
	{
		ModalSlot->SetContent(BuildPanel(P));
		if (!bModalShown) { ModalT0 = T; }
		bModalShown = true;
		if (bPanelShown) { bPanelShown = false; PanelT0 = T; }
	}
	else
	{
		PanelSlot->SetContent(BuildPanel(P));
		if (Prev != P) { PanelT0 = T; }
		bPanelShown = true;
		if (bModalShown) { bModalShown = false; ModalT0 = T; }
	}
	if (Prev != P) { Sfx(DCSfx::PanelOpen); }
}

void SDCRoot::OpenPlanner(int32 JobId)
{
	if (JobId != PlanJobId) { PlanCrew.Reset(); }
	PlanJobId = JobId;
	if (UDCRunSubsystem* R = Run())
	{
		// If the job is already committed, show the committed cast.
		for (const FDCCrewPlan& P : R->GetCommittedPlans())
		{
			if (P.JobId == JobId) { PlanCrew = P.CriminalIds; PlanCut = P.Cut; }
		}
	}
	if (Current == EDCPanel::CrewPlanner) { Rebuild(); AdvanceTutorial(0); }
	else { Open(EDCPanel::CrewPlanner); }
}

void SDCRoot::Close(bool bForce)
{
	if (Current == EDCPanel::None) { return; }
	const UDCRunSubsystem* R = Run();
	if (!bForce)
	{
		// Never strand the player: these screens only leave through their own buttons.
		if (Current == EDCPanel::Title && !HasRun()) { return; }
		if (Current == EDCPanel::Dailies && R && R->GetPhase() == EDCGamePhase::Dailies) { return; }
		if (Current == EDCPanel::GameOver && R && R->GetPhase() == EDCGamePhase::GameOver) { return; }
		if (Current == EDCPanel::Credits && R && R->GetPhase() == EDCGamePhase::Won) { return; }
		if (Current == EDCPanel::PerkDraft && R && R->HasPendingPerkOffer()) { return; }
	}
	if (Current == EDCPanel::Settings && ReturnPanel != EDCPanel::None)
	{
		const EDCPanel Ret = ReturnPanel;
		ReturnPanel = EDCPanel::None;
		Open(Ret);
		return;
	}
	const double T = Now();
	if (IsModal(Current)) { bModalShown = false; ModalT0 = T; }
	else { bPanelShown = false; PanelT0 = T; }
	Sfx(DCSfx::PanelClose);
	SetCurrent(EDCPanel::None);
}

void SDCRoot::Back()
{
	if (Current == EDCPanel::None) { if (HasRun()) { Open(EDCPanel::Settings); } return; }
	if (Current == EDCPanel::Settings || !IsModal(Current)) { Close(); return; }
	if (Current == EDCPanel::Dailies) { DailiesNext(); }
}

bool SDCRoot::IsWorldBlocked() const
{
	if (bModalShown) { return true; }
	if (TopBar.IsValid() && TopBar->IsHovered()) { return true; }
	if (bPanelShown && PanelFrame.IsValid() && PanelFrame->IsHovered()) { return true; }
	if (TutorialStrip.IsValid() && TutorialStep >= 0 && TutorialStrip->IsHovered()) { return true; }
	return false;
}

TSharedRef<SWidget> SDCRoot::Scroll(TSharedRef<SWidget> Content, bool bOnPaper)
{
	TSharedRef<SScrollBox> S = SNew(SScrollBox)
		.ScrollBarStyle(&ScrollBarStyle())
		.ScrollBarThickness(FVector2D(6.f, 6.f))
		+ SScrollBox::Slot().Padding(FMargin(0.f, 0.f, 10.f, 0.f))[Content];
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
	else { PanelSlot->SetContent(BuildPanel(Current)); }
}

TSharedRef<SWidget> SDCRoot::BuildPanel(EDCPanel P)
{
	switch (P)
	{
	case EDCPanel::Title:         return BuildTitle();
	case EDCPanel::JobBoard:      return BuildJobBoard();
	case EDCPanel::CrewPlanner:   return BuildPlanner();
	case EDCPanel::Workbench:     return BuildWorkbench();
	case EDCPanel::Roster:        return BuildRoster();
	case EDCPanel::Desk:          return BuildDesk();
	case EDCPanel::Blueprint:     return BuildBlueprint();
	case EDCPanel::PerkDraft:     return BuildPerkDraft();
	case EDCPanel::Dailies:       return BuildDailies();
	case EDCPanel::GameOver:      return BuildGameOver();
	case EDCPanel::Credits:       return BuildCredits();
	case EDCPanel::Settings:      return BuildSettings();
	case EDCPanel::ScreeningRoom: return BuildScreening();
	case EDCPanel::Clinic:        return BuildClinic();
	case EDCPanel::Lawyer:        return BuildLawyer();
	case EDCPanel::ServerCloset:  return BuildServer();
	default:                      return SNullWidget::NullWidget;
	}
}

TSharedRef<SWidget> SDCRoot::PanelShell(const FString& Title, const FString& Subtitle, TSharedRef<SWidget> Body)
{
	return DecoFrame(
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Bottom)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()[Txt(Title.ToUpper(), Display(34), Gold)]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f, 0.f, 0.f)[Txt(Subtitle, Type(15), Muted, true)]
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top)
			[
				SNew(SBox).MinDesiredWidth(46.f)
				[Button(TEXT("X"), [this]() { Close(); }, EBtn::Ghost, true, 18.f, TEXT("Close (Esc)"))]
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 10.f, 0.f, 14.f)[HLine(WithAlpha(Gold, 0.7f))]
		+ SVerticalBox::Slot().FillHeight(1.f)[Body]);
}

// ─────────────────────────────────────────────────────────────────────────────
// Tick, toasts, run events
// ─────────────────────────────────────────────────────────────────────────────

void SDCRoot::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
	const double T = Now();

	if (bDirty && !bSliderDragging)
	{
		bDirty = false;
		if (Current != EDCPanel::None && Current != EDCPanel::Dailies && Current != EDCPanel::Title && Current != EDCPanel::Credits && Current != EDCPanel::Settings)
		{
			if (Current == EDCPanel::CrewPlanner) { RecomputePlan(); }
			Rebuild();
		}
	}

	// Cash counter eases toward the real value.
	if (const UDCRunSubsystem* R = Run())
	{
		const double Target = R->GetCash();
		if (!bCashInit) { DisplayCash = Target; bCashInit = true; }
		const double Diff = Target - DisplayCash;
		DisplayCash = FMath::Abs(Diff) < 1.0 ? Target : DisplayCash + Diff * FMath::Min(1.0, InDeltaTime * 6.0);
	}

	// Toasts expire.
	for (int32 i = Toasts.Num() - 1; i >= 0; --i)
	{
		if (T - Toasts[i].Time > 4.0)
		{
			ToastBox->RemoveSlot(Toasts[i].Widget.ToSharedRef());
			Toasts.RemoveAt(i);
		}
	}

	if (Current == EDCPanel::Dailies) { DailiesTick(); }
}

void SDCRoot::Toast(const FString& Msg, bool bWarn)
{
	if (!ToastBox.IsValid()) { return; }
	const double T0 = Now();
	TSharedRef<SWidget> W = SNew(SBorder)
		.BorderImage(Brush(EBrush::PanelDark))
		.BorderBackgroundColor(bWarn ? RedBright : FLinearColor::White)
		.Padding(FMargin(18.f, 8.f))
		.ColorAndOpacity_Lambda([T0]()
		{
			const float A = (float)(Now() - T0);
			return FLinearColor(1, 1, 1, FMath::Min(EaseOut(A / 0.2f), 1.f - FMath::Clamp((A - 3.4f) / 0.6f, 0.f, 1.f)));
		})
		.RenderTransform_Lambda([T0]() { return TOptional<FSlateRenderTransform>(FSlateRenderTransform(FVector2f(0.f, (1.f - EaseOut((float)(Now() - T0) / 0.25f)) * -16.f))); })
		[
			SNew(STextBlock).Text(FText::FromString(Msg)).Font(Sans(18)).ColorAndOpacity(bWarn ? RedBright : Cream)
		];
	ToastBox->AddSlot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 0.f, 0.f, 6.f)[W];
	Toasts.Add({ W, T0 });
	if (Toasts.Num() > 4) { ToastBox->RemoveSlot(Toasts[0].Widget.ToSharedRef()); Toasts.RemoveAt(0); }
	if (bWarn) { Sfx(DCSfx::Error, 0.8f); }
	UE_LOG(LogDC, Log, TEXT("[Toast] %s"), *Msg);
}

void SDCRoot::OnRunState() { bDirty = true; }

void SDCRoot::OnNightResolved(const FDCNightReport& R)
{
	Dailies = MakeShared<FDailiesState>();
	Dailies->Report = R;
	Dailies->T0 = Now();
	Dailies->bLive = true;
	Dailies->bSummary = R.Results.Num() == 0;
	AdvanceTutorial(2);
	Open(EDCPanel::Dailies);
}

void SDCRoot::OnPerkOffered(const TArray<FDCPerk>& Perks)
{
	if (Current == EDCPanel::Dailies || Current == EDCPanel::GameOver || Current == EDCPanel::Credits) { bPerkPending = true; return; }
	Open(EDCPanel::PerkDraft);
}

void SDCRoot::OnGameOver(const FDCRunSummary& S)
{
	EndSummary = S; bHasEndSummary = true;
	if (Current == EDCPanel::Dailies) { return; } // shown after the reveal
	Open(EDCPanel::GameOver);
}

void SDCRoot::OnRunWon(const FDCRunSummary& S)
{
	EndSummary = S; bHasEndSummary = true;
	if (Current == EDCPanel::Dailies) { return; }
	Open(EDCPanel::Credits);
}

void SDCRoot::RollCamera(bool bFromButton)
{
	UDCRunSubsystem* R = Run();
	if (!R || !R->HasActiveRun()) { return; }
	if (R->GetPhase() != EDCGamePhase::Planning || (bModalShown && Current != EDCPanel::None)) { return; }
	const int32 N = R->GetCommittedPlans().Num();
	if (N == 0)
	{
		if (!bFromButton) { Toast(TEXT("Nobody's cast yet. Pick a job on the Job Board first."), true); return; }
		const double T = Now();
		if (T - LastRollClick > 3.0)
		{
			LastRollClick = T;
			Toast(TEXT("No crew cast tonight. Click ROLL CAMERA again to sit the night out."), true);
			return;
		}
	}
	LastRollClick = -10.0;
	if (bPanelShown) { Close(true); }
	Sfx(DCSfx::Clapper);
	R->EndNight();
	if (R->GetPhase() == EDCGamePhase::Dailies && Current != EDCPanel::Dailies)
	{
		OnNightResolved(R->GetLastNightReport());
	}
}

void SDCRoot::ShowStartScreen()
{
	Open(EDCPanel::Title);
	if (UDCAudioSubsystem* A = UDCAudioSubsystem::Get(GetContext())) { A->SetMusicState(EDCMusicState::Title); }
}

// ─────────────────────────────────────────────────────────────────────────────
// Tutorial
// ─────────────────────────────────────────────────────────────────────────────

void SDCRoot::AdvanceTutorial(int32 FromStep)
{
	if (TutorialStep < 0 || TutorialStep != FromStep) { return; }
	TutorialStep++;
	if (TutorialStep >= 3) { DismissTutorial(); }
	else { Sfx(DCSfx::Typewriter, 0.6f); }
}

void SDCRoot::DismissTutorial()
{
	TutorialStep = -1;
	GConfig->SetBool(GTutSection, TEXT("TutorialDone"), true, GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);
}

TSharedRef<SWidget> SDCRoot::BuildTutorial()
{
	static const TCHAR* Titles[] = { TEXT("PICK A JOB"), TEXT("CAST & CUT"), TEXT("ROLL CAMERA") };
	static const TCHAR* Hints[] = {
		TEXT("Open the JOB BOARD (the cork board, or JOBS up top) and pick tonight's production."),
		TEXT("Cast the required roles from your crew, set your cut with the slider, watch the odds, then COMMIT."),
		TEXT("Hit ROLL CAMERA (or Space) to send the crew out. The Dailies show how it went."),
	};
	TSharedRef<SHorizontalBox> Steps = SNew(SHorizontalBox);
	for (int32 i = 0; i < 3; ++i)
	{
		Steps->AddSlot().AutoWidth().Padding(0.f, 0.f, 14.f, 0.f).VAlign(VAlign_Center)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(SBox).WidthOverride(26.f).HeightOverride(26.f)
				[
					SNew(SBorder).BorderImage(Brush(EBrush::Chip)).HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(0.f)
					.BorderBackgroundColor_Lambda([this, i]() { return TutorialStep == i ? Gold : (TutorialStep > i ? Green : WithAlpha(Muted, 0.35f)); })
					[
						SNew(STextBlock).Text(FText::FromString(FString::FromInt(i + 1))).Font(Sans(14)).ColorAndOpacity(Ink)
					]
				]
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(6.f, 0.f, 0.f, 0.f)
			[
				SNew(STextBlock).Text(FText::FromString(Titles[i])).Font(Sans(16))
				.ColorAndOpacity_Lambda([this, i]() { return FSlateColor(TutorialStep == i ? Gold : Muted); })
			]
		];
	}
	return SNew(SBox).WidthOverride(600.f)
	[
		SNew(SBorder).BorderImage(Brush(EBrush::PanelDark)).Padding(FMargin(16.f, 10.f, 16.f, 12.f))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[Txt(TEXT("FIRST DAY ON SET"), Display(17), Gold)]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[Button(TEXT("Skip tips"), [this]() { DismissTutorial(); }, EBtn::Ghost, true, 13.f, TEXT("Hide these hints for good"))]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f, 0.f, 0.f)[Steps]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 8.f, 0.f, 0.f)
			[
				SNew(STextBlock).Font(Type(16)).ColorAndOpacity(Cream).AutoWrapText(true)
				.Text_Lambda([this]() { return FText::FromString(TutorialStep >= 0 && TutorialStep < 3 ? Hints[TutorialStep] : TEXT("")); })
			]
		]
	];
}

// ─────────────────────────────────────────────────────────────────────────────
// Top bar
// ─────────────────────────────────────────────────────────────────────────────

TSharedRef<SWidget> SDCRoot::BuildTopBar()
{
	auto StatBlock = [this](const FString& Label, TFunction<FString()> Value, TFunction<FLinearColor()> Col, const FString& Tip)
	{
		TSharedRef<SWidget> W = SNew(SBox).MinDesiredWidth(84.f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()[Txt(Label, Sans(12), Muted)]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, -4.f, 0.f, 0.f)
			[
				SNew(STextBlock).Font(Big(32))
				.Text_Lambda([Value]() { return FText::FromString(Value()); })
				.ColorAndOpacity_Lambda([Col]() { return FSlateColor(Col()); })
			]
		];
		W->SetToolTipText(FText::FromString(Tip));
		return W;
	};
	auto Sep = []() { return SNew(SBox).Padding(FMargin(14.f, 8.f)).HeightOverride(52.f)[VLine(WithAlpha(Gold, 0.35f))]; };

	auto Tab = [this](const FString& Label, EDCPanel P, const FString& Tip)
	{
		return SNew(SOverlay)
			+ SOverlay::Slot()[Button(Label, [this, P]() { if (Current == P) { Close(); } else { Open(P); } }, EBtn::Tab, true, 16.f, Tip)]
			+ SOverlay::Slot().VAlign(VAlign_Bottom).Padding(8.f, 0.f)
			[
				SNew(SBox).HeightOverride(2.f).Visibility_Lambda([this, P]() { return (Current == P || (P == EDCPanel::JobBoard && Current == EDCPanel::CrewPlanner)) ? EVisibility::HitTestInvisible : EVisibility::Hidden; })
				[SNew(SImage).Image(Brush(EBrush::White)).ColorAndOpacity(Gold)]
			];
	};

	TSharedRef<SHorizontalBox> Tabs = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth()[Tab(TEXT("Jobs"), EDCPanel::JobBoard, TEXT("Job Board: tonight's productions"))]
		+ SHorizontalBox::Slot().AutoWidth()[Tab(TEXT("Crew"), EDCPanel::Roster, TEXT("Your cast (the Lounge)"))]
		+ SHorizontalBox::Slot().AutoWidth()[Tab(TEXT("Gear"), EDCPanel::Workbench, TEXT("Workbench: buy and equip gear"))]
		+ SHorizontalBox::Slot().AutoWidth()[Tab(TEXT("Desk"), EDCPanel::Desk, TEXT("Director's Desk: finances and recruits"))]
		+ SHorizontalBox::Slot().AutoWidth()[Tab(TEXT("Build"), EDCPanel::Blueprint, TEXT("Blueprint Table: build and upgrade rooms"))]
		+ SHorizontalBox::Slot().AutoWidth()[Tab(TEXT("Menu"), EDCPanel::Settings, TEXT("Projector: settings, seed, quit (Esc)"))];

	return SNew(SBorder)
		.BorderImage(Brush(EBrush::PanelDark))
		.Padding(FMargin(18.f, 6.f, 12.f, 6.f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 8.f, 0.f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()[Txt(TEXT("DIRECTOR'S CUT"), Display(24), Gold)]
				+ SVerticalBox::Slot().AutoHeight()[Txt(TEXT("a crime picture in nightly instalments"), Type(11), Muted)]
			]
			+ SHorizontalBox::Slot().AutoWidth()[Sep()]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[StatBlock(TEXT("CASH"), [this]() { return Money((int64)FMath::RoundToDouble(DisplayCash)); }, [this]() { const UDCRunSubsystem* R = Run(); return (R && R->GetCash() < R->GetUpkeepDue()) ? RedBright : Gold; }, TEXT("Cash on hand. Run out and the picture is cancelled."))]
			+ SHorizontalBox::Slot().AutoWidth()[Sep()]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[StatBlock(TEXT("NIGHT"), [this]() { const UDCRunSubsystem* R = Run(); return R ? FString::FromInt(R->GetNight()) : FString(); }, []() { return Cream; }, TEXT("Current night"))]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(14.f, 0.f, 0.f, 0.f)
			[StatBlock(TEXT("HEISTS"), [this]() { const UDCRunSubsystem* R = Run(); return R ? FString::FromInt(R->GetHeistsCompleted()) : FString(); }, []() { return Cream; }, TEXT("Successful heists. More heists unlock bigger jobs, but everything gets harder."))]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(14.f, 0.f, 0.f, 0.f)
			[StatBlock(TEXT("UPKEEP DUE"), [this]() { const UDCRunSubsystem* R = Run(); return R ? Money(-R->GetUpkeepDue()) : FString(); }, []() { return Rgb(0xe0, 0x8a, 0x70); }, TEXT("Charged when you Roll Camera: crew wages + rooms."))]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(14.f, 0.f, 0.f, 0.f)
			[StatBlock(TEXT("CREWS OUT"), [this]() { const UDCRunSubsystem* R = Run(); return R ? FString::Printf(TEXT("%d/%d"), R->GetCommittedPlans().Num(), R->GetMaxCrewsPerNight()) : FString(); },
				[this]() { const UDCRunSubsystem* R = Run(); return (R && R->GetCommittedPlans().Num() > 0) ? GreenBright : Cream; }, TEXT("Crews committed tonight / crews you can run per night (build the Garage for more)"))]
			+ SHorizontalBox::Slot().AutoWidth()[Sep()]
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[Tabs]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(10.f, 0.f, 0.f, 0.f)
			[
				SNew(SBox).HeightOverride(56.f).MinDesiredWidth(190.f)
				[
					ButtonContent(
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[Txt(TEXT("ROLL CAMERA"), Big(30), Cream)]
						+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, -6.f, 0.f, 0.f)
						[
							SNew(STextBlock).Font(Sans(11)).ColorAndOpacity(WithAlpha(Cream, 0.75f))
							.Text_Lambda([this]()
							{
								const UDCRunSubsystem* R = Run();
								const int32 N = R ? R->GetCommittedPlans().Num() : 0;
								return FText::FromString(N > 0 ? FString::Printf(TEXT("END NIGHT  -  %d CREW%s OUT  [SPACE]"), N, N == 1 ? TEXT("") : TEXT("S")) : FString(TEXT("END NIGHT  [SPACE]")));
							})
						],
						[this]() { RollCamera(true); }, EBtn::Red,
						TAttribute<bool>::CreateLambda([this]() { const UDCRunSubsystem* R = Run(); return R && R->GetPhase() == EDCGamePhase::Planning; }),
						TEXT("End the night: committed crews pull their jobs, upkeep is paid."))
				]
			]
		];
}

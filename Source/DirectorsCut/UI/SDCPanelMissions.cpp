// Director's Cut — mission board, mission planner and reports (Agent C).
#include "UI/SDCRootInternal.h"
#include "DirectorsCut.h"

// ─────────────────────────────────────────────────────────────────────────────
// Shared bits
// ─────────────────────────────────────────────────────────────────────────────

TSharedRef<SWidget> SDCRoot::ThreatRow(const TArray<EDCThreat>& Threats, float Size)
{
	TSharedRef<SHorizontalBox> H = SNew(SHorizontalBox);
	for (EDCThreat T : Threats)
	{
		TSharedRef<SWidget> Tile = SNew(SBox).WidthOverride(Size).HeightOverride(Size)
			[
				SNew(SBorder).BorderImage(Box(A(Bad, 0.10f), Size * 0.28f, A(Bad, 0.28f), 1.f)).HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(0.f)
				[Icon(DC::ThreatIcon(T), Size * 0.5f, A(Hex(0xFDA4AF), 0.95f))]
			];
		Tile->SetToolTip(Tip(DC::ThreatName(T), TEXT("Threat · counter it with an ability or gear"), DC::ThreatIcon(T), Bad));
		H->AddSlot().AutoWidth().Padding(0.f, 0.f, 4.f, 0.f)[Tile];
	}
	return H;
}

static TSharedRef<SWidget> Stat(FName IconName, TAttribute<FText> Value, const FLinearColor& IconCol, const FSlateFontInfo& F, TAttribute<FSlateColor> ValCol = FSlateColor(Text))
{
	return IconTextA(IconName, Value, F, FSlateColor(IconCol), ValCol, F.Size * 1.05f, 5.f);
}

static TSharedRef<SWidget> CrewSlotsIcons(int32 Size, float IconSize, const FLinearColor& Col)
{
	TSharedRef<SHorizontalBox> H = SNew(SHorizontalBox);
	for (int32 i = 0; i < Size; ++i) { H->AddSlot().AutoWidth().Padding(0.f, 0.f, 1.f, 0.f)[Icon(TEXT("user-round"), IconSize, Col)]; }
	return H;
}

// ─────────────────────────────────────────────────────────────────────────────
// Missions board
// ─────────────────────────────────────────────────────────────────────────────

TSharedRef<SWidget> SDCRoot::BuildMissions()
{
	UDCRunSubsystem* R = Run();
	if (!R) { return SNullWidget::NullWidget; }
	auto RR = [this]() { return Run(); };

	TSharedRef<SVerticalBox> Body = SNew(SVerticalBox);

	// In progress
	TArray<const FDCMission*> Active, Board;
	const FDCMission* Final = nullptr;
	for (const FDCMission& M : R->GetMissions())
	{
		if (M.bInProgress) { Active.Add(&M); }
		else if (M.bFinal) { Final = &M; }
		else { Board.Add(&M); }
	}
	if (Active.Num())
	{
		Body->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 8.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Label(TEXT("In progress"))]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8.f, 0.f)[Txt(FString::Printf(TEXT("%d/%d"), R->GetActiveMissionCount(), R->GetMaxActiveMissions()), Mono(9.f), TextFaint)]
		];
		for (const FDCMission* M : Active) { Body->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 6.f)[ActiveMissionRow(*M)]; }
		Body->AddSlot().AutoHeight().Padding(0.f, 12.f, 0.f, 0.f)[SNullWidget::NullWidget];
	}

	Body->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 8.f)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Label(TEXT("Board"))]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8.f, 0.f)[Txt(FString::FromInt(Board.Num() + (Final ? 1 : 0)), Mono(9.f), TextFaint)]
	];
	if (Final) { Body->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 12.f)[MissionCard(*Final)]; }

	if (Board.Num() == 0 && !Final)
	{
		Body->AddSlot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 40.f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[Icon(TEXT("radar"), 30.f, TextFaint)]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 10.f, 0.f, 0.f)[Txt(TEXT("No postings right now"), Font(11.f), TextDim)]
		];
	}
	else
	{
		TSharedRef<SUniformGridPanel> Grid = SNew(SUniformGridPanel).SlotPadding(FMargin(6.f));
		for (int32 i = 0; i < Board.Num(); ++i) { Grid->AddSlot(i % 3, i / 3)[MissionCard(*Board[i])]; }
		Body->AddSlot().AutoHeight().Padding(-6.f, -6.f, -6.f, 0.f)[Grid];
	}

	TSharedRef<SWidget> Right = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 6.f, 0.f)[Icon(TEXT("radio"), 12.f, TextDim)]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[TxtA(TAttribute<FText>::CreateLambda([RR]() { const UDCRunSubsystem* X = RR(); return FText::FromString(X ? DC::Clock(X->GetNextMissionIn()) : TEXT("")); }), Mono(11.f), FSlateColor(TextDim))];
	Right->SetToolTip(Tip(TEXT("Next posting"), TEXT("New jobs post while the board has room"), TEXT("radio")));

	return Shell(TEXT("map"), TEXT("Missions"), Scroll(Body), Right);
}

TSharedRef<SWidget> SDCRoot::ActiveMissionRow(const FDCMission& M)
{
	auto RR = [this]() { return Run(); };
	const int32 Id = M.Id;
	const FLinearColor TC = M.bFinal ? Accent : TierColor(M.Tier);
	const UDCRunSubsystem* R = Run();
	TSharedRef<SHorizontalBox> Crew = SNew(SHorizontalBox);
	for (int32 CId : M.CrewIds) { if (const FDCCriminal* C = R ? R->FindCriminal(CId) : nullptr) { Crew->AddSlot().AutoWidth().Padding(0.f, 0.f, 4.f, 0.f)[Avatar(*C, 24.f)]; } }

	TSharedRef<SWidget> Content = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 12.f, 0.f)[IconTile(M.Icon, 36.f, TC)]
		+ SHorizontalBox::Slot().FillWidth(0.35f).VAlign(VAlign_Center)[Txt(M.DisplayName, Font(12.f, EW::SemiBold), Text)]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(12.f, 0.f)[Crew]
		+ SHorizontalBox::Slot().FillWidth(0.65f).VAlign(VAlign_Center).Padding(12.f, 0.f)
		[Bar(TAttribute<float>::CreateLambda([RR, Id]() { const UDCRunSubsystem* X = RR(); return X ? X->GetMissionProgress(Id) : 0.f; }), FSlateColor(TC), 4.f)]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8.f, 0.f)
		[SNew(SBox).MinDesiredWidth(46.f).HAlign(HAlign_Right)[TxtA(TAttribute<FText>::CreateLambda([RR, Id]() { const UDCRunSubsystem* X = RR(); return FText::FromString(X ? DC::Clock(X->GetMissionTimeRemaining(Id)) : TEXT("")); }), Mono(12.f), FSlateColor(Text))]]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8.f, 0.f, 0.f, 0.f)
		[Pill(Txt(Pct(M.LockedChance), Mono(10.f), ChanceColor(M.LockedChance)), A(ChanceColor(M.LockedChance), 0.12f), FMargin(8.f, 2.f))];
	return Button(Content, [this, Id]() { OpenPlanner(Id); }, EBtn::Card, true, nullptr, FMargin(10.f, 8.f, 14.f, 8.f));
}

TSharedRef<SWidget> SDCRoot::MissionCard(const FDCMission& M)
{
	UDCRunSubsystem* R = Run();
	const int32 Id = M.Id;
	auto RR = [this]() { return Run(); };
	const FLinearColor TC = M.bFinal ? Accent : TierColor(M.Tier);

	// Best-crew estimate (what the auto-fill would get right now).
	float BestChance = 0.f; bool bStaffable = false;
	if (R)
	{
		const FDCCrewPlan P = R->SuggestPlan(M.Id, 0.3f);
		const FDCSuccessBreakdown B = R->PreviewPlan(P);
		BestChance = B.SuccessChance;
		bStaffable = P.CriminalIds.Num() >= M.CrewSize;
	}
	const bool bNew = !SeenMissionIds.Contains(M.Id);

	TSharedRef<SHorizontalBox> Flags = SNew(SHorizontalBox);
	if (bNew) { Flags->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(4.f, 0.f, 0.f, 0.f)[Pill(Txt(TEXT("NEW"), Caps(7.5f), AccentInk), Accent, FMargin(6.f, 1.f))]; }
	if (M.bNight)
	{
		TSharedRef<SWidget> W = Icon(TEXT("moon"), 12.f, Info);
		TSharedRef<SBox> B = SNew(SBox)[W]; B->SetToolTip(Tip(TEXT("Night job"), TEXT("Bonus odds if started after dark"), TEXT("moon"), Info));
		B->SetVisibility(EVisibility::Visible);
		Flags->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(6.f, 0.f, 0.f, 0.f)[B];
	}
	if (M.bRare)
	{
		TSharedRef<SBox> B = SNew(SBox)[Icon(TEXT("sparkles"), 12.f, Accent)]; B->SetToolTip(Tip(TEXT("Rare"), TEXT("Better pay and loot"), TEXT("sparkles")));
		Flags->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(6.f, 0.f, 0.f, 0.f)[B];
	}
	if (!M.LootGearId.IsNone())
	{
		TSharedRef<SBox> B = SNew(SBox)[Icon(TEXT("package"), 12.f, TextDim)];
		const FDCGearItem* G = R ? R->FindGear(M.LootGearId) : nullptr;
		B->SetToolTip(Tip(TEXT("Loot"), G ? G->DisplayName : M.LootGearId.ToString(), G ? G->Icon : FName(TEXT("package"))));
		Flags->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(6.f, 0.f, 0.f, 0.f)[B];
	}

	const double First = FirstSeenAt.Contains(M.Id) ? FirstSeenAt[M.Id] : (R ? R->GetRunTime() : 0.0);
	auto ExpiryFrac = [RR, Id, First]()
	{
		const UDCRunSubsystem* X = RR();
		const FDCMission* Q = X ? X->FindMission(Id) : nullptr;
		if (!Q || Q->ExpiresAt < 0.0) { return 1.f; }
		const double Len = FMath::Max(1.0, Q->ExpiresAt - First);
		return (float)FMath::Clamp((Q->ExpiresAt - X->GetRunTime()) / Len, 0.0, 1.0);
	};

	TSharedRef<SWidget> Top = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top).Padding(0.f, 0.f, 12.f, 0.f)[IconTile(M.bFinal ? FName(TEXT("crown")) : M.Icon, M.bFinal ? 52.f : 44.f, TC)]
		+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
				[SNew(STextBlock).Text(FText::FromString(M.DisplayName)).Font(Font(M.bFinal ? 15.f : 12.5f, EW::SemiBold)).ColorAndOpacity(Text).OverflowPolicy(ETextOverflowPolicy::Ellipsis)]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Flags]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 3.f, 0.f, 0.f)
			[SNew(STextBlock).Text(FText::FromString(M.Flavour)).Font(Font(9.5f)).ColorAndOpacity(TextDim).OverflowPolicy(ETextOverflowPolicy::Ellipsis)]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f, 0.f, 0.f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Pips(M.Tier, 4, TC, 4.f)]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8.f, 0.f, 0.f, 0.f)[Pill(Txt(FString::Printf(TEXT("LV %d"), M.Level), Font(8.f, EW::Bold, 60), TextDim), A(FLinearColor::White, 0.06f), FMargin(6.f, 1.f))]
			]
		];

	TSharedRef<SWidget> Meta = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[CrewSlotsIcons(M.CrewSize, 11.f, TextDim)]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(12.f, 0.f, 0.f, 0.f)[Stat(TEXT("clock"), FText::FromString(DC::Clock(M.Duration)), TextDim, Mono(10.f))]
		+ SHorizontalBox::Slot().FillWidth(1.f)[SNullWidget::NullWidget]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[ThreatRow(M.Threats, 22.f)];

	TSharedRef<SWidget> Money_ = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()[Label(TEXT("Payout"), TextFaint, 7.5f)]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 1.f, 0.f, 0.f)[Txt(Money(M.Payout), Mono(M.bFinal ? 17.f : 14.f), Text)]
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom).Padding(10.f, 0.f, 0.f, 2.f)
		[M.Cost > 0 ? Txt(FString::Printf(TEXT("cost %s"), *Money(M.Cost)), Mono(9.f), TextFaint) : SNullWidget::NullWidget]
		+ SHorizontalBox::Slot().FillWidth(1.f)[SNullWidget::NullWidget]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 6.f, 0.f)[Icon(bStaffable ? TEXT("wand-sparkles") : TEXT("user-x"), 11.f, bStaffable ? TextFaint : Bad)]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(SOverlay)
				+ SOverlay::Slot()[SNew(SDCRing).Size(34.f).Thickness(3.f).Fill(FMath::Min(BestChance, 1.f)).Color(ChanceColor(BestChance))]
				+ SOverlay::Slot()[SNew(SDCRing).Size(34.f).Thickness(3.f).Fill(FMath::Clamp(BestChance - 1.f, 0.f, 1.f)).Color(Accent).bShowTrack(false)]
				+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)[Txt(FString::FromInt(FMath::RoundToInt(BestChance * 100.f)), Mono(8.5f), Text)]
			]
		];
	Money_->SetToolTip(Tip(TEXT("Best crew available"), bStaffable ? FString::Printf(TEXT("Auto-fill odds right now: %s"), *Pct(BestChance)) : TEXT("Not enough free crew"), TEXT("wand-sparkles")));

	TSharedRef<SWidget> Content = SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()[Top]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 12.f, 0.f, 0.f)[Meta]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 12.f, 0.f, 10.f)[HLine()]
		+ SVerticalBox::Slot().AutoHeight()[Money_]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 10.f, 0.f, 0.f)
		[
			SNew(SBox).Visibility(M.ExpiresAt >= 0.0 ? EVisibility::Visible : EVisibility::Collapsed)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
				[Bar(TAttribute<float>::CreateLambda(ExpiryFrac), TAttribute<FSlateColor>::CreateLambda([ExpiryFrac]() { return FSlateColor(ExpiryFrac() < 0.25f ? Bad : A(TextDim, 0.6f)); }), 2.f)]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8.f, 0.f, 0.f, 0.f)
				[TxtA(TAttribute<FText>::CreateLambda([RR, Id]()
					{
						const UDCRunSubsystem* X = RR(); const FDCMission* Q = X ? X->FindMission(Id) : nullptr;
						return FText::FromString(Q ? DC::Clock(Q->ExpiresAt - X->GetRunTime()) : TEXT(""));
					}), Mono(8.5f), FSlateColor(TextFaint))]
			]
		];

	TSharedRef<SWidget> Btn = Button(Content, [this, Id]() { OpenPlanner(Id); }, EBtn::Card, true, nullptr, FMargin(14.f, 14.f, 14.f, 12.f));
	if (M.bRare || M.bFinal)
	{
		return SNew(SOverlay)
			+ SOverlay::Slot().Padding(-3.f)[SNew(SBorder).BorderImage(Box(A(Accent, 0.07f), 13.f, A(Accent, 0.22f), 1.f)).Visibility(EVisibility::HitTestInvisible)]
			+ SOverlay::Slot()[Btn]
			+ SOverlay::Slot()[SNew(SBorder).BorderImage(Box(FLinearColor::Transparent, 10.f, A(Accent, 0.55f), 1.f)).Visibility(EVisibility::HitTestInvisible)];
	}
	return Btn;
}

// ─────────────────────────────────────────────────────────────────────────────
// Planner
// ─────────────────────────────────────────────────────────────────────────────

void SDCRoot::RecomputePlan()
{
	const UDCRunSubsystem* R = Run();
	const FDCMission* M = R ? R->FindMission(PlanMissionId) : nullptr;
	if (!M) { PlanCache->bHas = false; PlanCache->B = FDCSuccessBreakdown(); return; }
	FDCCrewPlan P;
	P.MissionId = M->Id;
	P.CriminalIds = M->bInProgress ? M->CrewIds : PlanCrew;
	P.Cut = M->bInProgress ? M->Cut : PlanCut;
	PlanCache->B = R->PreviewPlan(P);
	PlanCache->bHas = true;
}

void SDCRoot::ToggleCrew(int32 CriminalId)
{
	const UDCRunSubsystem* R = Run();
	const FDCMission* M = R ? R->FindMission(PlanMissionId) : nullptr;
	const FDCCriminal* C = R ? R->FindCriminal(CriminalId) : nullptr;
	if (!M || M->bInProgress || !C) { return; }
	if (PlanCrew.Contains(CriminalId)) { PlanCrew.Remove(CriminalId); Sfx(DCSfx::Click); }
	else
	{
		if (!C->IsAvailable()) { Sfx(DCSfx::Error); return; }
		if (PlanCrew.Num() >= M->CrewSize) { PlanCrew.Pop(); }
		PlanCrew.Add(CriminalId);
		Sfx(DCSfx::SlotFill);
	}
	RecomputePlan();
	Rebuild();
}

void SDCRoot::AutoFill()
{
	const UDCRunSubsystem* R = Run();
	if (!R || !R->FindMission(PlanMissionId)) { return; }
	const FDCCrewPlan P = R->SuggestPlan(PlanMissionId, PlanCut);
	PlanCrew = P.CriminalIds;
	Sfx(PlanCrew.Num() ? DCSfx::SlotFill : DCSfx::Error);
	RecomputePlan();
	Rebuild();
}

void SDCRoot::StartPlan()
{
	UDCRunSubsystem* R = Run();
	const FDCMission* M = R ? R->FindMission(PlanMissionId) : nullptr;
	if (!M) { return; }
	const FString Name = M->DisplayName;
	const FName MIcon = M->Icon;
	FDCCrewPlan P; P.MissionId = PlanMissionId; P.CriminalIds = PlanCrew; P.Cut = PlanCut;
	EDCPlanBlock Block = EDCPlanBlock::None;
	if (R->StartMission(P, &Block))
	{
		Sfx(DCSfx::Confirm);
		Toast(FString::Printf(TEXT("%s · crew is rolling"), *Name), MIcon.IsNone() ? FName(TEXT("car-front")) : MIcon, false);
		PlanCrew.Reset();
		PlanMissionId = INDEX_NONE;
		Close(true);
	}
	else
	{
		Sfx(DCSfx::Error);
		Toast(BlockText(Block), BlockIcon(Block), true);
	}
}

TSharedRef<SWidget> SDCRoot::BuildPlanner()
{
	UDCRunSubsystem* R = Run();
	const FDCMission* MP = R ? R->FindMission(PlanMissionId) : nullptr;
	if (!R || !MP)
	{
		return Shell(TEXT("map"), TEXT("Mission"),
			SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[Icon(TEXT("timer-off"), 32.f, TextFaint)]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 12.f)[Txt(TEXT("This job is gone"), Font(12.f), TextDim)]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[TextButton(TEXT("map"), TEXT("Board"), [this]() { Open(EDCPanel::Missions); })]
			]);
	}
	const FDCMission M = *MP;
	if (M.bInProgress) { return PlannerRunning(M); }
	if (!PlanCache->bHas) { RecomputePlan(); }
	const FDCSuccessBreakdown& B0 = PlanCache->B;
	TSharedPtr<FPlanCache> Cache = PlanCache;
	auto RR = [this]() { return Run(); };
	const FLinearColor TC = M.bFinal ? Accent : TierColor(M.Tier);

	// Uncovered threats right now (used to highlight roster counters).
	TArray<EDCThreat> Uncovered;
	for (const FDCThreatSlot& S : B0.Threats) { if (!S.IsCountered()) { Uncovered.Add(S.Threat); } }
	if (B0.Threats.Num() == 0) { Uncovered = M.Threats; }

	// ── Threats ────────────────────────────────────────────────────────────
	TSharedRef<SHorizontalBox> Threats = SNew(SHorizontalBox);
	const TArray<FDCThreatSlot>& Slots = B0.Threats;
	const int32 NumThreats = FMath::Max(Slots.Num(), M.Threats.Num());
	for (int32 i = 0; i < NumThreats; ++i)
	{
		const EDCThreat T = Slots.IsValidIndex(i) ? Slots[i].Threat : M.Threats[i];
		const bool bCovered = Slots.IsValidIndex(i) && Slots[i].IsCountered();
		const FDCCriminal* By = bCovered ? R->FindCriminal(Slots[i].CounteredBy) : nullptr;
		const FLinearColor Col = bCovered ? Good : Bad;
		FString SourceName;
		if (bCovered)
		{
			if (const FDCAbility* Ab = R->FindAbility(Slots[i].Source)) { SourceName = Ab->DisplayName; }
			else if (const FDCGearItem* G = R->FindGear(Slots[i].Source)) { SourceName = G->DisplayName; }
			else { SourceName = Slots[i].Source.ToString(); }
		}
		// who in the roster could counter this?
		FString Who;
		if (!bCovered)
		{
			for (const FDCCriminal& C : R->GetRoster()) { if (R->GetCounters(C.Id).Contains(T)) { Who += (Who.IsEmpty() ? TEXT("") : TEXT(", ")) + C.GetShortName(); } }
		}
		TSharedRef<SWidget> Tile = SNew(SBox).WidthOverride(66.f).HeightOverride(66.f)
			[
				SNew(SOverlay)
				+ SOverlay::Slot()
				[
					SNew(SBorder).BorderImage(Box(A(Col, bCovered ? 0.14f : 0.08f), 14.f, A(Col, bCovered ? 0.6f : 0.3f), 1.f)).HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(0.f)
					[Icon(DC::ThreatIcon(T), 26.f, bCovered ? Good : Hex(0xFDA4AF))]
				]
				+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(FMargin(0.f, -5.f, -5.f, 0.f))
				[
					SNew(SBox).WidthOverride(18.f).HeightOverride(18.f).Visibility(bCovered ? EVisibility::HitTestInvisible : EVisibility::Collapsed)
					[SNew(SBorder).BorderImage(Box(Good, -1.f, Hex(0x0B0D10), 2.f)).HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(0.f)[Icon(TEXT("check"), 9.f, Hex(0x06281A))]]
				]
				+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Bottom).Padding(FMargin(0.f, 0.f, -6.f, -6.f))
				[By ? Avatar(*By, 24.f) : SNullWidget::NullWidget]
			];
		Tile->SetToolTip(bCovered
			? Tip(FString::Printf(TEXT("%s · covered"), DC::ThreatName(T)), FString::Printf(TEXT("%s · %s"), By ? *By->GetShortName() : TEXT("?"), *SourceName), DC::ThreatIcon(T), Good)
			: Tip(FString::Printf(TEXT("%s · uncovered"), DC::ThreatName(T)), Who.IsEmpty() ? TEXT("Nobody in your crew counters this. Try gear.") : FString::Printf(TEXT("Can counter: %s"), *Who), DC::ThreatIcon(T), Bad));
		Threats->AddSlot().AutoWidth().Padding(0.f, 0.f, 12.f, 0.f)[Tile];
	}

	// ── Crew slots ─────────────────────────────────────────────────────────
	TSharedRef<SHorizontalBox> CrewSlots = SNew(SHorizontalBox);
	for (int32 i = 0; i < M.CrewSize; ++i)
	{
		const FDCCriminal* C = PlanCrew.IsValidIndex(i) ? R->FindCriminal(PlanCrew[i]) : nullptr;
		TSharedRef<SWidget> Inner = SNullWidget::NullWidget;
		if (C)
		{
			const int32 CId = C->Id;
			const float Eff = R->GetEffectiveLevel(CId);
			const FLinearColor LC = LevelColor(Eff, M.Level);
			const bool bRefuse = B0.RefusingIds.Contains(CId);
			TSharedRef<SHorizontalBox> Ctr = SNew(SHorizontalBox);
			for (EDCThreat T : R->GetCounters(CId))
			{
				const bool bUseful = M.Threats.Contains(T);
				Ctr->AddSlot().AutoWidth().Padding(1.f, 0.f)[Icon(DC::ThreatIcon(T), 10.f, bUseful ? Good : TextFaint)];
			}
			Inner = SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
				[
					SNew(SOverlay)
					+ SOverlay::Slot()[Avatar(*C, 52.f)]
					+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Bottom).Padding(FMargin(0.f, 0.f, -8.f, -4.f))
					[
						SNew(SBorder).BorderImage(Box(Hex(0x0B0D10), -1.f, A(LC, 0.8f), 1.f)).Padding(FMargin(5.f, 0.f))
						[Txt(FString::Printf(TEXT("%d"), FMath::FloorToInt(Eff)), Mono(9.f), LC)]
					]
					+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(FMargin(-8.f, -4.f, 0.f, 0.f))
					[
						SNew(SBox).WidthOverride(20.f).HeightOverride(20.f)
						[
							SNew(SBorder).BorderImage(Box(Hex(0x0B0D10), -1.f)).HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(0.f)
							[
								IconA(TAttribute<FName>::CreateLambda([Cache, CId]() { const float* W = Cache->B.Willingness.Find(CId); return MoodIcon(W ? *W : 1.f, Cache->B.RefusingIds.Contains(CId)); }), 12.f,
									TAttribute<FSlateColor>::CreateLambda([Cache, CId]() { const float* W = Cache->B.Willingness.Find(CId); return FSlateColor(MoodColor(W ? *W : 1.f, Cache->B.RefusingIds.Contains(CId))); }))
							]
						]
					]
				]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 8.f, 0.f, 0.f)
				[SNew(STextBlock).Text(FText::FromString(C->GetShortName())).Font(Font(10.5f, EW::SemiBold)).ColorAndOpacity(bRefuse ? Bad : Text).OverflowPolicy(ETextOverflowPolicy::Ellipsis)]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 4.f, 0.f, 0.f)[Ctr];
		}
		else
		{
			Inner = SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
				[
					SNew(SBox).WidthOverride(52.f).HeightOverride(52.f)
					[SNew(SBorder).BorderImage(Box(A(FLinearColor::White, 0.03f), -1.f, A(FLinearColor::White, 0.16f), 1.f)).HAlign(HAlign_Center).VAlign(VAlign_Center)[Icon(TEXT("plus"), 18.f, TextFaint)]]
				]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 8.f, 0.f, 0.f)[Label(TEXT("Empty"), TextFaint, 8.f)];
		}
		const int32 SlotCId = C ? C->Id : INDEX_NONE;
		TSharedRef<SWidget> SlotW = SNew(SBox).WidthOverride(124.f).HeightOverride(124.f)
			[
				Button(SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center)[Inner], [this, SlotCId]() { if (SlotCId != INDEX_NONE) { ToggleCrew(SlotCId); } },
					C ? EBtn::Card : EBtn::Ghost, true, C ? Tip(C->GetDisplayName(), TEXT("Click to remove"), DC::TypeIcon(C->Type), DC::TypeColor(C->Type)) : Tip(TEXT("Empty slot"), TEXT("Pick crew from the list on the right")), FMargin(6.f))
			];
		CrewSlots->AddSlot().AutoWidth().Padding(0.f, 0.f, 10.f, 0.f)
		[
			C ? SlotW : (TSharedRef<SWidget>)(SNew(SBorder).BorderImage(Box(FLinearColor::Transparent, 10.f, A(FLinearColor::White, 0.10f), 1.f)).Padding(0.f)[SlotW])
		];
	}

	// ── Odds ring ──────────────────────────────────────────────────────────
	auto Chance = [Cache]() { return Cache->B.SuccessChance; };
	TSharedRef<SWidget> Ring = SNew(SBox).WidthOverride(172.f).HeightOverride(172.f)
		[
			SNew(SOverlay)
			+ SOverlay::Slot()[SNew(SDCRing).Size(172.f).Thickness(4.f).Track(A(Accent, 0.08f)).Color(Accent).Fill_Lambda([Chance]() { return FMath::Clamp(Chance() - 1.f, 0.f, 1.f); })]
			+ SOverlay::Slot().Padding(11.f)[SNew(SDCRing).Size(150.f).Thickness(10.f).Fill_Lambda([Chance]() { return FMath::Min(Chance(), 1.f); }).Color_Lambda([Chance]() { return ChanceColor(Chance()); })]
			+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[Label(TEXT("Odds"), TextFaint, 8.f)]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom)
					[TxtA(TAttribute<FText>::CreateLambda([Chance]() { return FText::FromString(FString::FromInt(FMath::RoundToInt(Chance() * 100.f))); }), Mono(32.f), TAttribute<FSlateColor>::CreateLambda([Chance]() { return FSlateColor(Chance() > 1.f ? Accent : Text); }))]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom).Padding(2.f, 0.f, 0.f, 7.f)[Txt(TEXT("%"), Mono(13.f), TextDim)]
				]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
				[
					SNew(SBox).Visibility_Lambda([Cache]() { return Cache->B.BonusChance > 0.f ? EVisibility::Visible : EVisibility::Hidden; })
					[
						IconTextA(TEXT("sparkles"), TAttribute<FText>::CreateLambda([Cache]() { return FText::FromString(FString::Printf(TEXT("BONUS %s"), *Pct(Cache->B.BonusChance))); }), Caps(8.f), FSlateColor(Accent), FSlateColor(Accent), 9.f, 4.f)
					]
				]
			]
		];
	Ring->SetToolTip(Tip(TEXT("Success chance"), TEXT("Crew level vs mission level, countered threats, traits/perks, and how willing the crew is at your cut. Above 100% rolls for a bonus payout."), TEXT("target")));

	// ── Breakdown bars ─────────────────────────────────────────────────────
	auto BarRow = [](FName IconName, const FString& Name, TFunction<float()> Value, TFunction<FString()> Txt_, const FLinearColor& Col, const FString& TipText)
	{
		TSharedRef<SWidget> W = SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 8.f, 0.f)[SNew(SBox).WidthOverride(16.f)[Icon(IconName, 12.f, TextDim)]]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[SNew(SBox).WidthOverride(88.f)[Label(Name, TextDim, 8.f)]]
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[Bar(TAttribute<float>::CreateLambda([Value]() { return Value(); }), FSlateColor(Col), 4.f)]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(10.f, 0.f, 0.f, 0.f)
			[SNew(SBox).WidthOverride(48.f).HAlign(HAlign_Right)[TxtA(TAttribute<FText>::CreateLambda([Txt_]() { return FText::FromString(Txt_()); }), Mono(10.f), FSlateColor(Text))]];
		W->SetToolTip(Tip(Name, TipText, IconName));
		return W;
	};
	TSharedRef<SWidget> Breakdown = SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 9.f)
		[BarRow(TEXT("users"), TEXT("Crew"), [Cache]() { return Cache->B.CrewChance; }, [Cache]() { return Pct(Cache->B.CrewChance); }, Info, TEXT("Each filled slot adds odds, scaled by level vs the mission level"))]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 9.f)
		[BarRow(TEXT("shield-check"), TEXT("Counters"), [Cache]() { return Cache->B.ThreatChance; }, [Cache]() { return Pct(Cache->B.ThreatChance); }, Good, TEXT("Every countered threat adds odds"))]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 9.f)
		[BarRow(TEXT("sparkles"), TEXT("Modifiers"), [Cache]() { return FMath::Abs(Cache->B.ModifierChance); }, [Cache]() { return SignedPct(Cache->B.ModifierChance); }, Accent, TEXT("Traits, perks, rooms and the night bonus"))]
		+ SVerticalBox::Slot().AutoHeight()
		[BarRow(TEXT("handshake"), TEXT("Willing"), [Cache]() { return Cache->B.WillFactor; }, [Cache]() { return FString::Printf(TEXT("x%.2f"), Cache->B.WillFactor); }, Warn, TEXT("A greedy cut makes the crew less willing, which multiplies the odds down"))];

	// Modifier chips
	TSharedRef<SWrapBox> Mods = SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(6.f, 6.f));
	for (const FDCModifierHit& H : B0.Modifiers)
	{
		const FLinearColor MC = H.bNegative ? Bad : Good;
		FString Parts;
		if (!FMath::IsNearlyZero(H.ChanceDelta)) { Parts += FString::Printf(TEXT("%s odds  "), *SignedPct(H.ChanceDelta)); }
		if (!FMath::IsNearlyEqual(H.DurationMult, 1.f)) { Parts += FString::Printf(TEXT("x%.2f time  "), H.DurationMult); }
		if (!FMath::IsNearlyEqual(H.PayoutMult, 1.f)) { Parts += FString::Printf(TEXT("x%.2f pay"), H.PayoutMult); }
		FString Who;
		if (const FDCCriminal* C = R->FindCriminal(H.CriminalId)) { Who = C->GetShortName() + TEXT(" · "); }
		TSharedRef<SWidget> Chip = Pill(
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 5.f, 0.f)[Icon(H.Icon.IsNone() ? FName(TEXT("sparkle")) : H.Icon, 10.f, MC)]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[Txt(!FMath::IsNearlyZero(H.ChanceDelta) ? SignedPct(H.ChanceDelta) : (!FMath::IsNearlyEqual(H.DurationMult, 1.f) ? FString::Printf(TEXT("x%.2f"), H.DurationMult) : FString::Printf(TEXT("x%.2f"), H.PayoutMult)), Mono(9.f), MC)],
			A(MC, 0.10f), FMargin(8.f, 3.f), A(MC, 0.25f));
		Chip->SetToolTip(Tip(Who + H.Label, Parts.TrimEnd(), H.Icon, MC));
		Mods->AddSlot()[Chip];
	}

	// ── Numbers ────────────────────────────────────────────────────────────
	auto Num = [](FName IconName, const FString& Name, TAttribute<FText> V, TAttribute<FSlateColor> Col, float Size, const FString& TipText)
	{
		TSharedRef<SWidget> W = SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()[IconText(IconName, Name.ToUpper(), Caps(7.5f), TextFaint, TextFaint, 10.f, 5.f)]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f, 0.f, 0.f)[TxtA(V, Mono(Size), Col)];
		W->SetToolTip(Tip(Name, TipText, IconName));
		return W;
	};
	TSharedRef<SWidget> Numbers = SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f)[Num(TEXT("clock"), TEXT("Time"), TAttribute<FText>::CreateLambda([Cache]() { return FText::FromString(DC::Clock(Cache->B.Duration)); }), FSlateColor(Text), 13.f, TEXT("How long the crew is gone (real seconds)"))]
			+ SHorizontalBox::Slot().FillWidth(1.f)[Num(TEXT("siren"), TEXT("Arrest"), TAttribute<FText>::CreateLambda([Cache]() { return FText::FromString(Pct(Cache->B.ArrestChance)); }),
				TAttribute<FSlateColor>::CreateLambda([Cache]() { return FSlateColor(Cache->B.ArrestChance >= 0.2f ? Bad : (Cache->B.ArrestChance > 0.f ? Warn : TextDim)); }), 13.f, TEXT("Chance each member is arrested if the job goes bust"))]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 14.f, 0.f, 0.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f)[Num(TEXT("receipt"), TEXT("Cost"), TAttribute<FText>::CreateLambda([Cache]() { return FText::FromString(Money(-Cache->B.Cost)); }),
				TAttribute<FSlateColor>::CreateLambda([this, Cache]() { const UDCRunSubsystem* X = Run(); return FSlateColor((X && X->GetCash() < Cache->B.Cost) ? Bad : TextDim); }), 13.f, TEXT("Paid upfront when the crew leaves"))]
			+ SHorizontalBox::Slot().FillWidth(1.f)[Num(TEXT("banknote"), TEXT("Payout"), TAttribute<FText>::CreateLambda([Cache]() { return FText::FromString(Money(Cache->B.Payout)); }), FSlateColor(Text), 13.f, TEXT("Total take on success, split by your cut"))]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 14.f, 0.f, 0.f)
		[Num(TEXT("crown"), TEXT("Your take"), TAttribute<FText>::CreateLambda([Cache]() { return FText::FromString(Money(Cache->B.DirectorTake)); }), FSlateColor(Accent), 22.f, TEXT("Payout x your cut. The crew splits the rest."))];

	// ── Cut slider ─────────────────────────────────────────────────────────
	TSharedRef<SHorizontalBox> Moods = SNew(SHorizontalBox);
	for (int32 CId : PlanCrew)
	{
		const FDCCriminal* C = R->FindCriminal(CId);
		if (!C) { continue; }
		TSharedRef<SWidget> MoodW = SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Avatar(*C, 22.f)]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4.f, 0.f, 0.f, 0.f)
			[
				IconA(TAttribute<FName>::CreateLambda([Cache, CId]() { const float* W = Cache->B.Willingness.Find(CId); return MoodIcon(W ? *W : 1.f, Cache->B.RefusingIds.Contains(CId)); }), 14.f,
					TAttribute<FSlateColor>::CreateLambda([Cache, CId]() { const float* W = Cache->B.Willingness.Find(CId); return FSlateColor(MoodColor(W ? *W : 1.f, Cache->B.RefusingIds.Contains(CId))); }))
			];
		const FString Name = C->GetShortName();
		MoodW->SetToolTip(TipWidget(TxtA(TAttribute<FText>::CreateLambda([Cache, CId, Name]()
		{
			const float* W = Cache->B.Willingness.Find(CId);
			const bool bRef = Cache->B.RefusingIds.Contains(CId);
			return FText::FromString(FString::Printf(TEXT("%s · %s%s"), *Name, bRef ? TEXT("refuses at this cut") : TEXT("willing "), bRef ? TEXT("") : *Pct(W ? *W : 1.f)));
		}), Font(10.5f, EW::Medium), FSlateColor(Text))));
		Moods->AddSlot().AutoWidth().Padding(0.f, 0.f, 12.f, 0.f)[MoodW];
	}
	auto CutToSlider = [](float Cut) { return (Cut - 0.1f) / 0.8f; };
	TSharedRef<SWidget> CutRow = SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[IconText(TEXT("percent"), TEXT("YOUR CUT"), Caps(8.f), TextDim, TextDim, 10.f, 6.f)]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(10.f, 0.f, 0.f, 0.f)
			[TxtA(TAttribute<FText>::CreateLambda([this]() { return FText::FromString(Pct(PlanCut)); }), Mono(15.f), FSlateColor(Accent))]
			+ SHorizontalBox::Slot().FillWidth(1.f)[SNullWidget::NullWidget]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Moods]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 8.f, 0.f, 0.f)
		[
			SNew(SBox).HeightOverride(24.f)
			[
				SNew(SOverlay)
				+ SOverlay::Slot().VAlign(VAlign_Center).Padding(FMargin(9.f, 0.f))
				[Bar(TAttribute<float>::CreateLambda([this, CutToSlider]() { return CutToSlider(PlanCut); }), FSlateColor(Accent), 4.f)]
				+ SOverlay::Slot()
				[
					SNew(SSlider)
					.Style(&SliderStyle())
					.IsFocusable(false)
					.MinValue(0.f).MaxValue(1.f)
					.StepSize(1.f / 16.f)
					.MouseUsesStep(true)
					.Value_Lambda([this, CutToSlider]() { return CutToSlider(PlanCut); })
					.OnValueChanged_Lambda([this](float V)
					{
						const float NewCut = FMath::Clamp(FMath::RoundToFloat((0.1f + V * 0.8f) / 0.05f) * 0.05f, 0.1f, 0.9f);
						if (!FMath::IsNearlyEqual(NewCut, PlanCut)) { PlanCut = NewCut; RecomputePlan(); Sfx(DCSfx::Hover, 0.5f); }
					})
				]
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f, 0.f, 0.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f)[Txt(TEXT("10%"), Mono(8.f), TextFaint)]
			+ SHorizontalBox::Slot().AutoWidth()[Txt(TEXT("90%"), Mono(8.f), TextFaint)]
		];
	CutRow->SetToolTip(Tip(TEXT("Your cut"), TEXT("The Director's share of the payout. Above a crew member's fair cut they get less willing (and loyal); far above it they refuse."), TEXT("percent")));

	// ── Actions ────────────────────────────────────────────────────────────
	TSharedRef<SWidget> StartContent = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 8.f, 0.f)
		[IconA(TAttribute<FName>::CreateLambda([Cache]() { return Cache->B.bValid ? FName(TEXT("play")) : BlockIcon(Cache->B.Block); }), 15.f, FSlateColor(AccentInk))]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[TxtA(TAttribute<FText>::CreateLambda([Cache]() { return FText::FromString(Cache->B.bValid ? TEXT("START") : TEXT("START")); }), Font(12.f, EW::Bold, 120), FSlateColor(AccentInk))];
	TSharedRef<SWidget> StartBtn = Button(SNew(SBox).MinDesiredWidth(150.f).HAlign(HAlign_Center)[StartContent], [this]() { StartPlan(); }, EBtn::Primary,
		TAttribute<bool>::CreateLambda([Cache]() { return Cache->B.bValid; }),
		TipWidget(
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 8.f, 0.f)
			[IconA(TAttribute<FName>::CreateLambda([Cache]() { return Cache->B.bValid ? FName(TEXT("circle-check")) : BlockIcon(Cache->B.Block); }), 12.f,
				TAttribute<FSlateColor>::CreateLambda([Cache]() { return FSlateColor(Cache->B.bValid ? Good : Bad); }))]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[TxtA(TAttribute<FText>::CreateLambda([Cache]() { return FText::FromString(Cache->B.bValid ? TEXT("Send the crew") : BlockText(Cache->B.Block)); }), Font(10.5f, EW::Medium), FSlateColor(Text))]),
		FMargin(18.f, 11.f));
	TSharedRef<SWidget> BlockHint = SNew(SBox).Visibility_Lambda([Cache]() { return Cache->B.bValid ? EVisibility::Hidden : EVisibility::Visible; })
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 6.f, 0.f)
			[IconA(TAttribute<FName>::CreateLambda([Cache]() { return BlockIcon(Cache->B.Block); }), 12.f, FSlateColor(Bad))]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[TxtA(TAttribute<FText>::CreateLambda([Cache]() { return FText::FromString(BlockText(Cache->B.Block)); }), Font(10.f), FSlateColor(A(Bad, 0.9f)))]
		];

	TSharedRef<SWidget> Actions = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[TextButton(TEXT("wand-sparkles"), TEXT("Auto-fill"), [this]() { AutoFill(); }, EBtn::Secondary, true, Tip(TEXT("Auto-fill"), TEXT("Pick the best free crew: counters first, then level")))]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8.f, 0.f, 0.f, 0.f)
		[IconButton(TEXT("eraser"), [this]() { PlanCrew.Reset(); RecomputePlan(); Rebuild(); Sfx(DCSfx::Click); }, Tip(TEXT("Clear crew")), EBtn::Secondary, 13.f, TAttribute<bool>::CreateLambda([this]() { return PlanCrew.Num() > 0; }))]
		+ SHorizontalBox::Slot().FillWidth(1.f).HAlign(HAlign_Right).VAlign(VAlign_Center).Padding(16.f, 0.f)[BlockHint]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[StartBtn];

	// ── Roster ─────────────────────────────────────────────────────────────
	TArray<const FDCCriminal*> Sorted;
	for (const FDCCriminal& C : R->GetRoster()) { Sorted.Add(&C); }
	Sorted.StableSort([](const FDCCriminal& X, const FDCCriminal& Y) { return X.IsAvailable() && !Y.IsAvailable(); });
	TSharedRef<SVerticalBox> Roster = SNew(SVerticalBox);
	const double Now_ = R->GetRunTime();
	for (const FDCCriminal* CP : Sorted)
	{
		const FDCCriminal& C = *CP;
		const int32 CId = C.Id;
		const bool bIn = PlanCrew.Contains(CId);
		const bool bAvail = C.IsAvailable();
		const float Eff = R->GetEffectiveLevel(CId);
		const FLinearColor LC = LevelColor(Eff, M.Level);
		bool bHelps = false;
		TSharedRef<SHorizontalBox> Ctr = SNew(SHorizontalBox);
		for (EDCThreat T : R->GetCounters(CId))
		{
			const bool bNeeded = Uncovered.Contains(T);
			bHelps |= bNeeded;
			const bool bOnMission = M.Threats.Contains(T);
			Ctr->AddSlot().AutoWidth().Padding(0.f, 0.f, 3.f, 0.f)
			[
				SNew(SBox).WidthOverride(20.f).HeightOverride(20.f)
				[
					SNew(SBorder).BorderImage(bNeeded ? Box(A(Good, 0.18f), 5.f, A(Good, 0.55f), 1.f) : NoBrush()).HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(0.f)
					[Icon(DC::ThreatIcon(T), 11.f, bNeeded ? Good : (bOnMission ? TextDim : TextFaint))]
				]
			];
		}
		TSharedRef<SWidget> Right_ = bAvail
			? (TSharedRef<SWidget>)(SNew(SBorder).BorderImage(Box(A(LC, 0.12f), -1.f, A(LC, 0.4f), 1.f)).Padding(FMargin(7.f, 1.f))[Txt(FString::Printf(TEXT("LV %d"), FMath::FloorToInt(Eff)), Font(8.5f, EW::Bold, 40), LC)])
			: (TSharedRef<SWidget>)(SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 4.f, 0.f)[Icon(StatusIcon(C), 11.f, StatusColor(C))]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[TxtA(TAttribute<FText>::CreateLambda([RR, CId]()
					{
						const UDCRunSubsystem* X = RR(); const FDCCriminal* Q = X ? X->FindCriminal(CId) : nullptr;
						if (!Q) { return FText::GetEmpty(); }
						if (Q->Status == EDCCriminalStatus::Injured) { return FText::FromString(DC::Clock(Q->InjuredUntil - X->GetRunTime())); }
						if (Q->Status == EDCCriminalStatus::OnMission) { return FText::FromString(DC::Clock(X->GetMissionTimeRemaining(Q->ActiveMissionId))); }
						return FText::GetEmpty();
					}), Mono(9.5f), FSlateColor(TextDim))]);

		TSharedRef<SWidget> Row = SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 10.f, 0.f)
			[
				SNew(SOverlay)
				+ SOverlay::Slot()[Avatar(C, 36.f, !bAvail)]
				+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Bottom).Padding(FMargin(0.f, 0.f, -3.f, -3.f))
				[
					SNew(SBox).WidthOverride(16.f).HeightOverride(16.f).Visibility(bIn ? EVisibility::HitTestInvisible : EVisibility::Collapsed)
					[SNew(SBorder).BorderImage(Box(Accent, -1.f, Hex(0x0B0D10), 2.f)).HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(0.f)[Icon(TEXT("check"), 8.f, AccentInk)]]
				]
			]
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[SNew(STextBlock).Text(FText::FromString(C.GetShortName())).Font(Font(10.5f, EW::SemiBold)).ColorAndOpacity(bAvail ? Text : TextFaint).OverflowPolicy(ETextOverflowPolicy::Ellipsis)]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 3.f, 0.f, 0.f)[Ctr]
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(6.f, 0.f, 0.f, 0.f)[Right_];

		TSharedRef<SWidget> Btn = Button(Row, [this, CId]() { ToggleCrew(CId); }, bIn ? EBtn::CardSelected : EBtn::Card, true, nullptr, FMargin(10.f, 7.f));
		Btn->SetToolTip(TipWidget(CrewTip(C)));
		Roster->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 6.f)
		[
			SNew(SOverlay)
			+ SOverlay::Slot()[Btn]
			+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Fill).Padding(FMargin(0.f, 10.f))
			[SNew(SBox).WidthOverride(2.f).Visibility(bHelps && bAvail && !bIn ? EVisibility::HitTestInvisible : EVisibility::Collapsed)[SNew(SBorder).BorderImage(Box(Good, 1.f))]]
		];
	}
	(void)Now_;

	// ── Header right ───────────────────────────────────────────────────────
	const int32 MId = M.Id;
	TSharedRef<SHorizontalBox> HR = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Pips(M.Tier, 4, TC, 5.f)]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(10.f, 0.f, 0.f, 0.f)[Pill(Txt(FString::Printf(TEXT("LV %d"), M.Level), Font(9.f, EW::Bold, 60), Text), A(FLinearColor::White, 0.07f), FMargin(8.f, 2.f), Line)]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(10.f, 0.f, 0.f, 0.f)[CrewSlotsIcons(M.CrewSize, 12.f, TextDim)];
	if (M.bNight)
	{
		TSharedRef<SWidget> Moon = Pill(IconText(TEXT("moon"), TEXT("NIGHT"), Caps(8.f), Info, Info, 10.f, 5.f), A(Info, 0.10f), FMargin(8.f, 2.f));
		Moon->SetToolTip(Tip(TEXT("Night job"), TEXT("Bonus odds if started after dark (20:00-06:00)"), TEXT("moon"), Info));
		HR->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(10.f, 0.f, 0.f, 0.f)[Moon];
	}
	if (M.bRare) { HR->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(10.f, 0.f, 0.f, 0.f)[Pill(IconText(TEXT("sparkles"), TEXT("RARE"), Caps(8.f), Accent, Accent, 10.f, 5.f), A(Accent, 0.10f), FMargin(8.f, 2.f))]; }
	if (M.ExpiresAt >= 0.0)
	{
		TSharedRef<SWidget> Exp = Stat(TEXT("hourglass"), TAttribute<FText>::CreateLambda([RR, MId]()
			{
				const UDCRunSubsystem* X = RR(); const FDCMission* Q = X ? X->FindMission(MId) : nullptr;
				return FText::FromString(Q ? DC::Clock(Q->ExpiresAt - X->GetRunTime()) : TEXT(""));
			}), TextDim, Mono(10.f), FSlateColor(TextDim));
		Exp->SetToolTip(Tip(TEXT("Posting expires"), TEXT("Start it before the timer runs out"), TEXT("hourglass")));
		HR->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(14.f, 0.f, 0.f, 0.f)[Exp];
	}
	HR->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(14.f, 0.f, 0.f, 0.f)[IconButton(TEXT("arrow-left"), [this]() { Open(EDCPanel::Missions); }, Tip(TEXT("Back to the board")), EBtn::Ghost, 14.f)];

	// ── Layout ─────────────────────────────────────────────────────────────
	TSharedRef<SWidget> Left = SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()[Label(TEXT("Threats"))]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 10.f, 0.f, 0.f)[Threats]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 22.f, 0.f, 0.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Label(TEXT("Crew"))]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8.f, 0.f)[Txt(FString::Printf(TEXT("%d/%d"), PlanCrew.Num(), M.CrewSize), Mono(9.f), TextFaint)]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 10.f, 0.f, 0.f)[CrewSlots]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 22.f, 0.f, 0.f)
		[
			CardBox(
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Ring]
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center).Padding(26.f, 0.f, 0.f, 0.f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()[Breakdown]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, B0.Modifiers.Num() ? 12.f : 0.f, 0.f, 0.f)[Mods]
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(26.f, 0.f, 0.f, 0.f)[SNew(SBox).HeightOverride(150.f)[VLine()]]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(22.f, 0.f, 4.f, 0.f)[SNew(SBox).WidthOverride(200.f)[Numbers]],
				FMargin(18.f))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 12.f, 0.f, 0.f)[CardBox(CutRow, FMargin(18.f, 14.f))]
		+ SVerticalBox::Slot().FillHeight(1.f)[SNullWidget::NullWidget]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 18.f, 0.f, 0.f)[Actions];

	TSharedRef<SWidget> RightCol = SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 10.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Label(TEXT("Roster"))]
			+ SHorizontalBox::Slot().FillWidth(1.f).HAlign(HAlign_Right).VAlign(VAlign_Center)
			[IconText(TEXT("shield-check"), TEXT("counters a gap"), Font(8.5f), Good, TextFaint, 9.f, 4.f)]
		]
		+ SVerticalBox::Slot().FillHeight(1.f)[Scroll(Roster)];

	TSharedRef<SWidget> Body = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(1.f)[Left]
		+ SHorizontalBox::Slot().AutoWidth().Padding(24.f, 0.f)[VLine()]
		+ SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(330.f)[RightCol]];

	return Shell(M.Icon, M.DisplayName, Body, HR, M.Flavour);
}

TSharedRef<SWidget> SDCRoot::PlannerRunning(const FDCMission& M)
{
	UDCRunSubsystem* R = Run();
	auto RR = [this]() { return Run(); };
	const int32 Id = M.Id;
	const FLinearColor TC = M.bFinal ? Accent : TierColor(M.Tier);
	const FDCSuccessBreakdown& B = PlanCache->B;

	TSharedRef<SHorizontalBox> Threats = SNew(SHorizontalBox);
	for (int32 i = 0; i < M.Threats.Num(); ++i)
	{
		const bool bCovered = B.Threats.IsValidIndex(i) && B.Threats[i].IsCountered();
		const FLinearColor Col = bCovered ? Good : Bad;
		Threats->AddSlot().AutoWidth().Padding(0.f, 0.f, 8.f, 0.f)
		[
			SNew(SBox).WidthOverride(44.f).HeightOverride(44.f)
			[SNew(SBorder).BorderImage(Box(A(Col, 0.12f), 10.f, A(Col, 0.45f), 1.f)).HAlign(HAlign_Center).VAlign(VAlign_Center)[Icon(DC::ThreatIcon(M.Threats[i]), 18.f, Col)]]
		];
	}
	TSharedRef<SHorizontalBox> Crew = SNew(SHorizontalBox);
	for (int32 CId : M.CrewIds)
	{
		if (const FDCCriminal* C = R ? R->FindCriminal(CId) : nullptr)
		{
			Crew->AddSlot().AutoWidth().Padding(0.f, 0.f, 16.f, 0.f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[Avatar(*C, 48.f)]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 6.f, 0.f, 0.f)[Txt(C->GetShortName(), Font(10.f, EW::SemiBold), Text)]
			];
		}
	}
	const float Ch = M.LockedChance;
	TSharedRef<SWidget> Body = SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(SOverlay)
				+ SOverlay::Slot()[SNew(SDCRing).Size(150.f).Thickness(8.f).Color(TC).Fill_Lambda([RR, Id]() { const UDCRunSubsystem* X = RR(); return X ? X->GetMissionProgress(Id) : 0.f; })]
				+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[Label(TEXT("Back in"), TextFaint, 8.f)]
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
					[TxtA(TAttribute<FText>::CreateLambda([RR, Id]() { const UDCRunSubsystem* X = RR(); return FText::FromString(X ? DC::Clock(X->GetMissionTimeRemaining(Id)) : TEXT("")); }), Mono(26.f), FSlateColor(Text))]
				]
			]
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center).Padding(32.f, 0.f, 0.f, 0.f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()[Label(TEXT("Crew"))]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 10.f, 0.f, 0.f)[Crew]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 22.f, 0.f, 0.f)[Label(TEXT("Threats"))]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 10.f, 0.f, 0.f)[Threats]
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 28.f, 0.f, 0.f)
		[
			CardBox(
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.f)[Stat(TEXT("target"), FText::FromString(Pct(Ch)), ChanceColor(Ch), Mono(15.f), FSlateColor(ChanceColor(Ch)))]
				+ SHorizontalBox::Slot().FillWidth(1.f)[Stat(TEXT("banknote"), FText::FromString(Money(M.Payout)), TextDim, Mono(15.f))]
				+ SHorizontalBox::Slot().FillWidth(1.f)[Stat(TEXT("percent"), FText::FromString(Pct(M.Cut)), TextDim, Mono(15.f))]
				+ SHorizontalBox::Slot().FillWidth(1.f)[Stat(TEXT("crown"), FText::FromString(Money(FMath::RoundToInt(M.Payout * M.Cut))), Accent, Mono(15.f), FSlateColor(Accent))],
				FMargin(18.f, 14.f))
		];
	TSharedRef<SWidget> HR = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Pill(IconText(TEXT("car-front"), TEXT("IN PROGRESS"), Caps(8.f), Info, Info, 10.f, 5.f), A(Info, 0.10f), FMargin(8.f, 3.f))]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(14.f, 0.f, 0.f, 0.f)[IconButton(TEXT("arrow-left"), [this]() { Open(EDCPanel::Missions); }, Tip(TEXT("Back to the board")), EBtn::Ghost, 14.f)];
	return Shell(M.Icon, M.DisplayName, Body, HR, M.Flavour);
}

// ─────────────────────────────────────────────────────────────────────────────
// Reports
// ─────────────────────────────────────────────────────────────────────────────

TSharedRef<SWidget> SDCRoot::BuildReports()
{
	UDCRunSubsystem* R = Run();
	if (!R) { return SNullWidget::NullWidget; }
	TSharedRef<SVerticalBox> List = SNew(SVerticalBox);
	const double T = Now();
	int32 Stagger = 0;
	// Oldest unseen reveals first.
	const TArray<FDCMissionReport>& Reports = R->GetReports();
	for (int32 i = Reports.Num() - 1; i >= 0; --i)
	{
		const FDCMissionReport& Rep = Reports[i];
		if (!ReportAnimT0.Contains(Rep.ReportId)) { ReportAnimT0.Add(Rep.ReportId, Rep.bSeen ? -100.0 : T + 0.15 + 0.9 * Stagger++); }
	}
	TArray<int32> ToMark;
	for (const FDCMissionReport& Rep : Reports)
	{
		List->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 10.f)[ReportCard(Rep)];
		if (!Rep.bSeen) { ToMark.Add(Rep.ReportId); }
		if (List->NumSlots() >= 30) { break; }
	}
	if (Reports.Num() == 0)
	{
		List->AddSlot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 60.f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[Icon(TEXT("inbox"), 30.f, TextFaint)]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 10.f, 0.f, 0.f)[Txt(TEXT("No reports yet"), Font(11.f), TextDim)]
		];
	}
	for (int32 Id : ToMark) { R->MarkReportSeen(Id); }
	if (ToMark.Num()) { LastSig = ComputeSignature(); }
	return Shell(TEXT("scroll-text"), TEXT("Reports"), Scroll(List));
}

TSharedRef<SWidget> SDCRoot::ReportCard(const FDCMissionReport& Rep)
{
	const UDCRunSubsystem* R = Run();
	const double T0 = ReportAnimT0.Contains(Rep.ReportId) ? ReportAnimT0[Rep.ReportId] : -100.0;
	const int32 RId = Rep.ReportId;
	const FDCMission& M = Rep.Mission;
	const FLinearColor OC = OutcomeColor(Rep.Outcome);
	const float Chance = M.LockedChance;
	const float Roll = Rep.Roll;
	const EDCMissionOutcome Outcome = Rep.Outcome;
	// Timeline (s): 0..1 marker slides; 1.0 badge; 1.0..1.8 payout ticks; 1.2.. members.
	auto Phase = [T0]() { return (float)(Now() - T0); };
	auto Sting = [this, RId, Outcome]()
	{
		if (ReportStung.Contains(RId)) { return; }
		ReportStung.Add(RId);
		Sfx(Outcome == EDCMissionOutcome::Success ? DCSfx::Success : (Outcome == EDCMissionOutcome::Busted ? DCSfx::Busted : DCSfx::Fail));
	};
	if (T0 < 0.0) { ReportStung.Add(RId); }

	TSharedRef<SWidget> Badge_ = SNew(SBorder)
		.BorderImage(NoBrush())
		.Padding(0.f)
		.RenderTransformPivot(FVector2D(0.5f, 0.5f))
		.RenderTransform_Lambda([Phase]() { const float P = Phase() - 1.0f; const float S = P < 0.f ? 0.6f : 1.f + 0.25f * FMath::Max(0.f, 1.f - P / 0.25f) * FMath::Sin(FMath::Clamp(P / 0.25f, 0.f, 1.f) * PI); return TOptional<FSlateRenderTransform>(FSlateRenderTransform(S)); })
		.ColorAndOpacity_Lambda([Phase, Sting]() { const float P = Phase() - 1.0f; if (P >= 0.f) { Sting(); } return FLinearColor(1, 1, 1, EaseOut(P / 0.15f)); })
		[
			SNew(SBorder)
			.BorderImage(Box(A(OC, 0.14f), 6.f, A(OC, 0.6f), 1.f))
			.Padding(FMargin(10.f, 3.f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 6.f, 0.f)
				[Icon(Outcome == EDCMissionOutcome::Success ? TEXT("circle-check") : (Outcome == EDCMissionOutcome::Busted ? TEXT("siren") : TEXT("circle-x")), 11.f, OC)]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Txt(DC::OutcomeName(Outcome), Font(9.5f, EW::Bold, 120), OC)]
			]
		];

	const int32 Take = Rep.DirectorTake;
	TSharedRef<SWidget> Payout = SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right)[Label(TEXT("Your take"), TextFaint, 7.5f)]
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right)
		[TxtA(TAttribute<FText>::CreateLambda([Phase, Take]() { const float P = FMath::Clamp((Phase() - 1.0f) / 0.8f, 0.f, 1.f); return FText::FromString(Money(FMath::RoundToInt(Take * EaseOut(P)))); }), Mono(16.f), FSlateColor(Take > 0 ? Accent : TextFaint))];

	// Members
	TSharedRef<SWrapBox> Members = SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(10.f, 8.f));
	int32 Idx = 0;
	for (const FDCCrewMemberResult& MR : Rep.Members)
	{
		const FDCCriminal* C = R ? R->FindCriminal(MR.CriminalId) : nullptr;
		const float Delay = 1.2f + 0.12f * Idx++;
		TSharedRef<SHorizontalBox> Icons = SNew(SHorizontalBox);
		auto AddIcon = [&Icons](FName N, const FLinearColor& Col, const FString& TipT)
		{
			TSharedRef<SBox> B = SNew(SBox)[Icon(N, 12.f, Col)];
			B->SetToolTip(Tip(TipT, FString(), N, Col));
			Icons->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 6.f, 0.f)[B];
		};
		if (MR.bLevelledUp) { AddIcon(TEXT("chevrons-up"), Accent, FString::Printf(TEXT("Level %d"), MR.NewLevel)); }
		if (MR.bInjured) { AddIcon(TEXT("bandage"), Bad, FString::Printf(TEXT("Injured · %s"), *DC::Clock(MR.InjurySeconds))); }
		if (MR.bArrested) { AddIcon(TEXT("siren"), Bad, TEXT("Arrested")); }
		if (MR.bArrestPrevented) { AddIcon(TEXT("shield-check"), Good, TEXT("Arrest prevented")); }
		if (MR.LoyaltyDelta != 0) { AddIcon(MR.LoyaltyDelta > 0 ? TEXT("heart") : TEXT("heart-crack"), MR.LoyaltyDelta > 0 ? Good : Bad, FString::Printf(TEXT("Loyalty %+d"), MR.LoyaltyDelta)); }

		const float XPFrac = C && C->XPToNext > 0 ? (float)C->XP / (float)C->XPToNext : 0.f;
		const float XPFrom = C && C->XPToNext > 0 ? FMath::Max(0.f, (float)(C->XP - MR.XPGained) / (float)C->XPToNext) : 0.f;
		const bool bLv = MR.bLevelledUp;
		TSharedRef<SWidget> MCard = SNew(SBox).WidthOverride(240.f)
			[
				SNew(SBorder).BorderImage(NoBrush()).Padding(0.f)
				.ColorAndOpacity_Lambda([Phase, Delay]() { return FLinearColor(1, 1, 1, EaseOut((Phase() - Delay) / 0.2f)); })
				[
				SNew(SBorder).BorderImage(Box(Inset, 8.f)).Padding(FMargin(8.f, 7.f))
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 8.f, 0.f)
					[C ? Avatar(*C, 28.f, MR.bArrested) : AvatarType(EDCCriminalType::Muscle, EDCQuality::Street, 28.f, true)]
					+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
							[SNew(STextBlock).Text(FText::FromString(C ? C->GetShortName() : MR.Name)).Font(Font(10.f, EW::SemiBold)).ColorAndOpacity(MR.bArrested ? Bad : Text).OverflowPolicy(ETextOverflowPolicy::Ellipsis)]
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Icons]
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Txt(FString::Printf(TEXT("+%d"), MR.XPGained), Mono(9.f), TextDim)]
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 5.f, 0.f, 0.f)
						[Bar(TAttribute<float>::CreateLambda([Phase, Delay, XPFrom, XPFrac, bLv]()
							{
								const float P = EaseOut((Phase() - Delay - 0.1f) / 0.7f);
								if (bLv) { return P < 0.5f ? FMath::Lerp(XPFrom, 1.f, P * 2.f) : FMath::Lerp(0.f, XPFrac, (P - 0.5f) * 2.f); }
								return FMath::Lerp(XPFrom, XPFrac, P);
							}), FSlateColor(bLv ? Accent : Info), 3.f)]
					]
				]
				]
			];
		Members->AddSlot()[MCard];
	}

	TSharedRef<SHorizontalBox> Extras = SNew(SHorizontalBox);
	if (Rep.bBonus) { Extras->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 8.f, 0.f)[Pill(IconText(TEXT("sparkles"), TEXT("BONUS"), Caps(8.f), Accent, Accent, 10.f, 5.f), A(Accent, 0.12f), FMargin(8.f, 2.f))]; }
	if (!Rep.LootGearId.IsNone())
	{
		const FDCGearItem* G = R ? R->FindGear(Rep.LootGearId) : nullptr;
		TSharedRef<SWidget> Loot = Pill(IconText(G ? G->Icon : FName(TEXT("package")), G ? G->DisplayName : Rep.LootGearId.ToString(), Font(9.5f, EW::Medium), Accent, Text, 11.f, 6.f), A(FLinearColor::White, 0.05f), FMargin(8.f, 2.f), Line);
		Loot->SetToolTip(Tip(TEXT("Loot"), TEXT("Added to your stash"), TEXT("package")));
		Extras->AddSlot().AutoWidth().VAlign(VAlign_Center)[Loot];
	}

	const bool bNew = T0 > 0.0;
	TSharedRef<SWidget> Content = SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 12.f, 0.f)[IconTile(M.Icon, 38.f, M.bFinal ? Accent : TierColor(M.Tier))]
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()[Txt(M.DisplayName, Font(12.5f, EW::SemiBold), Text)]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 3.f, 0.f, 0.f)[Extras]
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(12.f, 0.f)[Badge_]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[SNew(SBox).MinDesiredWidth(100.f).HAlign(HAlign_Right)[Payout]]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 12.f, 0.f, 0.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 10.f, 0.f)[Icon(TEXT("dices"), 12.f, TextFaint)]
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
			[
				SNew(SDCRollBar).Height(6.f).Chance(FMath::Min(Chance, 1.f)).ZoneColor(A(Good, 0.55f))
				.Roll_Lambda([Phase, Roll]() { return Roll * EaseInOut(Phase() / 0.95f); })
				.MarkerColor_Lambda([Phase, OC]() { return Phase() >= 1.f ? OC : Text; })
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(12.f, 0.f, 0.f, 0.f)
			[Txt(FString::Printf(TEXT("%d vs %d"), FMath::RoundToInt(Roll * 100.f), FMath::RoundToInt(Chance * 100.f)), Mono(9.f), TextDim)]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 12.f, 0.f, 0.f)[Members];

	TSharedRef<SWidget> CardW = SNew(SBorder)
		.BorderImage(Box(DCUI::Card, 10.f, bNew ? A(Accent, 0.35f) : Line, 1.f))
		.Padding(FMargin(16.f, 14.f))
		.ColorAndOpacity_Lambda([T0]() { return FLinearColor(1, 1, 1, T0 < 0.0 ? 1.f : FMath::Max(0.25f, EaseOut((float)(Now() - T0 + 0.15) / 0.2f))); })
		[Content];
	CardW->SetToolTip(Tip(FString::Printf(TEXT("Rolled %d, needed under %d"), FMath::RoundToInt(Roll * 100.f), FMath::RoundToInt(FMath::Min(Chance, 1.f) * 100.f)), FString::Printf(TEXT("Total payout %s · cut %s"), *Money(Rep.PayoutTotal), *Pct(M.Cut)), TEXT("dices")));
	return CardW;
}

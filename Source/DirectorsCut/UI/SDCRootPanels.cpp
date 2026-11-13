// Non-modal panels: job board, crew planner, workbench, roster, desk, blueprint, room panels.
#include "UI/SDCRoot.h"
#include "UI/SDCRootInternal.h"

namespace
{
	FString TypeAbbrev(EDCStat S)
	{
		switch (S)
		{
		case EDCStat::Driving: return TEXT("DRV");
		case EDCStat::Muscle: return TEXT("MUS");
		case EDCStat::Tech: return TEXT("TEC");
		case EDCStat::Stealth: return TEXT("STL");
		case EDCStat::Charm: return TEXT("CHA");
		case EDCStat::Finesse: return TEXT("FIN");
		default: return TEXT("?");
		}
	}

	TArray<TPair<EDCStat, float>> SortedWeights(const FDCJob& J)
	{
		TArray<TPair<EDCStat, float>> W;
		for (const TPair<EDCStat, float>& P : J.StatWeights) { if (P.Value > 0.f) { W.Add(P); } }
		W.Sort([](const TPair<EDCStat, float>& A, const TPair<EDCStat, float>& B) { return A.Value > B.Value; });
		return W;
	}

	TSharedRef<SWidget> Labeled(const FString& Label, TSharedRef<SWidget> Value, bool bOnPaper = true)
	{
		return SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()[Txt(Label.ToUpper(), Sans(12), bOnPaper ? InkSoft : Muted)]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, -2.f, 0.f, 0.f)[Value];
	}

}

// ─────────────────────────────────────────────────────────────────────────────
// Job board
// ─────────────────────────────────────────────────────────────────────────────

TSharedRef<SWidget> SDCRoot::JobCard(const FDCJob& J)
{
	UDCRunSubsystem* R = Run();
	// Castability: does the roster have someone available for each required role?
	TArray<EDCCriminalType> Missing;
	{
		TArray<int32> Used;
		for (EDCCriminalType Role : J.RequiredRoles)
		{
			bool bFound = false;
			for (const FDCCriminal& C : R->GetRoster())
			{
				if (C.Type == Role && C.IsAvailable() && !Used.Contains(C.Id)) { Used.Add(C.Id); bFound = true; break; }
			}
			if (!bFound) { Missing.AddUnique(Role); }
		}
	}

	TSharedRef<SWrapBox> Roles = SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(4.f, 4.f));
	for (EDCCriminalType T : J.RequiredRoles) { Roles->AddSlot()[TypeChip(T, 13.f)]; }
	for (EDCCriminalType T : J.BonusRoles) { Roles->AddSlot()[Chip(FString::Printf(TEXT("+ %s"), DC::TypeName(T)), WithAlpha(TypeCol(T), 0.35f), 13.f)]; }

	FString Keys;
	for (const TPair<EDCStat, float>& W : SortedWeights(J)) { Keys += FString::Printf(TEXT("%s%s %d%%"), Keys.IsEmpty() ? TEXT("") : TEXT("  "), DC::StatName(W.Key), FMath::RoundToInt(W.Value * 100.f)); }

	TSharedRef<SWidget> Footer = SNullWidget::NullWidget;
	if (J.bCommitted) { Footer = Stamp(TEXT("CAST"), Red, 26.f, -6.f); }
	else if (Missing.Num())
	{
		TArray<FString> Names; for (EDCCriminalType T : Missing) { Names.Add(DC::TypeName(T)); }
		Footer = Txt(FString::Printf(TEXT("No %s available - hire one at the Desk"), *FString::Join(Names, TEXT(" / "))), Sans(14), Red);
	}
	else { Footer = Txt(TEXT("CLICK TO CAST  >"), Sans(14), Green); }

	const int32 Committed = R->GetCommittedPlans().Num();
	const bool bFull = !J.bCommitted && Committed >= R->GetMaxCrewsPerNight();
	const int32 JobId = J.Id;

	return ButtonContent(
		SNew(SBorder).BorderImage(FStyleDefaults::GetNoBrush()).Padding(FMargin(16.f, 12.f))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[Txt(J.DisplayName, Display(22), J.bFinalCut ? Rgb(0x8a, 0x5a, 0x00) : Ink, true)]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8.f, 0.f, 0.f, 0.f)
				[Chip(J.bFinalCut ? FString(TEXT("THE FINAL CUT")) : FString::Printf(TEXT("TIER %s"), *Roman(J.Tier)), J.bFinalCut ? Gold : Ink, 13.f)]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f, 0.f, 8.f)[Txt(J.Flavour, Type(14), InkSoft, true)]
			+ SVerticalBox::Slot().AutoHeight()[Roles]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 10.f, 0.f, 0.f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.2f)[Labeled(TEXT("Payout"), Txt(Money(J.Payout), Big(32), Ink))]
				+ SHorizontalBox::Slot().FillWidth(0.7f)[Labeled(TEXT("Crew"), Txt(J.MinCrew == J.MaxCrew ? FString::FromInt(J.MinCrew) : FString::Printf(TEXT("%d-%d"), J.MinCrew, J.MaxCrew), Big(32), Ink))]
				+ SHorizontalBox::Slot().FillWidth(0.8f)[Labeled(TEXT("Difficulty"), Txt(FString::Printf(TEXT("%.0f"), J.Difficulty), Big(32), Ink))]
				+ SHorizontalBox::Slot().FillWidth(0.8f)[Labeled(TEXT("Heat"), Txt(Pct(J.ArrestRisk), Big(32), J.ArrestRisk >= 0.3f ? Red : Ink))]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f, 0.f, 0.f)[Txt(FString::Printf(TEXT("Key stats: %s"), *Keys), Sans(13), InkSoft, true)]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 8.f, 0.f, 0.f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[Footer]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[bFull ? Txt(TEXT("All crews committed tonight"), Sans(13), InkSoft) : SNullWidget::NullWidget]
			]
		],
		[this, JobId]() { OpenPlanner(JobId); }, EBtn::Paper, true,
		TEXT("Open the Crew Planner for this job"));
}

TSharedRef<SWidget> SDCRoot::BuildJobBoard()
{
	UDCRunSubsystem* R = Run();
	TSharedRef<SUniformGridPanel> Grid = SNew(SUniformGridPanel).SlotPadding(FMargin(7.f));
	const TArray<FDCJob>& Jobs = R->GetJobs();
	for (int32 i = 0; i < Jobs.Num(); ++i) { Grid->AddSlot(i % 2, i / 2)[JobCard(Jobs[i])]; }

	TSharedRef<SWidget> Body = Jobs.Num() == 0
		? Txt(TEXT("The board is bare tonight. Roll Camera to see what tomorrow brings."), Type(18), Muted, true)
		: Scroll(Grid);

	return PanelShell(TEXT("The Job Board"),
		FString::Printf(TEXT("Night %d  -  pick a production, cast it, set your cut. Crews tonight: %d of %d.  Jobs up to tier %s."),
			R->GetNight(), R->GetCommittedPlans().Num(), R->GetMaxCrewsPerNight(), *Roman(R->GetUnlockedTier())),
		Body);
}

// ─────────────────────────────────────────────────────────────────────────────
// Crew planner
// ─────────────────────────────────────────────────────────────────────────────

void SDCRoot::ToggleCrew(int32 Id)
{
	UDCRunSubsystem* R = Run();
	const FDCJob* J = R ? R->FindJob(PlanJobId) : nullptr;
	if (!J || J->bCommitted) { return; }
	if (PlanCrew.Contains(Id)) { PlanCrew.Remove(Id); Sfx(DCSfx::PanelClose, 0.6f); }
	else
	{
		if (PlanCrew.Num() >= J->MaxCrew) { Toast(FString::Printf(TEXT("%s takes at most %d crew."), *J->DisplayName, J->MaxCrew), true); return; }
		PlanCrew.Add(Id);
		Sfx(DCSfx::Typewriter, 0.7f);
	}
	RecomputePlan();
	Rebuild();
}

void SDCRoot::RecomputePlan()
{
	UDCRunSubsystem* R = Run();
	if (!R) { return; }
	// Drop anyone who left the roster or is no longer available (unless the plan is committed).
	const FDCJob* J = R->FindJob(PlanJobId);
	if (J && !J->bCommitted)
	{
		PlanCrew.RemoveAll([R](int32 Id) { const FDCCriminal* C = R->FindCriminal(Id); return !C || !C->IsAvailable(); });
	}
	FDCCrewPlan P; P.JobId = PlanJobId; P.CriminalIds = PlanCrew; P.Cut = PlanCut;
	PlanCache->B = R->PreviewPlan(P);

	if (!BreakdownBox.IsValid()) { return; }
	BreakdownBox->ClearChildren();
	const FDCSuccessBreakdown& B = PlanCache->B;
	if (PlanCrew.Num() == 0)
	{
		BreakdownBox->AddSlot().AutoHeight()[Txt(TEXT("Cast someone to see the odds."), Type(15), Muted, true)];
		return;
	}
	for (int32 i = 0; i < B.Lines.Num(); ++i)
	{
		const FDCBreakdownLine& L = B.Lines[i];
		const FString V = L.bIsMultiplier ? FString::Printf(TEXT("x%.2f"), L.Value) : (i == 0 ? Pct(L.Value) : SignedPct(L.Value));
		const FLinearColor LabelCol = L.bIsWarning ? RedBright : Cream;
		BreakdownBox->AddSlot().AutoHeight().Padding(0.f, 3.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()[Txt(L.Label, Sans(15), LabelCol)]
				+ SVerticalBox::Slot().AutoHeight()[Txt(L.Detail, Type(13), L.bIsWarning ? WithAlpha(RedBright, 0.8f) : Muted, true)]
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(10.f, 0.f, 0.f, 0.f)
			[Txt(V, Big(26), L.bIsWarning ? RedBright : (L.bIsMultiplier && L.Value < 1.f ? Rgb(0xe0, 0x8a, 0x70) : Gold))]
		];
	}
	BreakdownBox->AddSlot().AutoHeight().Padding(0.f, 6.f, 0.f, 2.f)[HLine(WithAlpha(Gold, 0.4f))];
	BreakdownBox->AddSlot().AutoHeight()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[Txt(TEXT("= Chance of success"), Sans(16), Gold)]
		+ SHorizontalBox::Slot().AutoWidth()[Txt(Pct(B.SuccessChance), Big(28), Gold)]
	];
}

TSharedRef<SWidget> SDCRoot::BuildPlanner()
{
	UDCRunSubsystem* R = Run();
	const FDCJob* JP = R->FindJob(PlanJobId);
	if (!JP)
	{
		return PanelShell(TEXT("Crew Planner"), TEXT("No job selected."),
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()[Txt(TEXT("There's nothing on the board to plan. Roll Camera to move to the next night."), Type(18), Muted, true)]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Left).Padding(0.f, 14.f)[Button(TEXT("Job Board"), [this]() { Open(EDCPanel::JobBoard); })]);
	}
	const FDCJob J = *JP;
	const bool bLocked = J.bCommitted;
	RecomputePlan();
	const FDCSuccessBreakdown& B = PlanCache->B;
	TSharedPtr<FPlanCache> Cache = PlanCache;

	// ── Job summary ──
	TSharedRef<SWrapBox> Roles = SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(4.f, 4.f));
	for (EDCCriminalType T : J.RequiredRoles) { Roles->AddSlot()[TypeChip(T, 13.f)]; }
	for (EDCCriminalType T : J.BonusRoles) { Roles->AddSlot()[Chip(FString::Printf(TEXT("+ %s"), DC::TypeName(T)), WithAlpha(TypeCol(T), 0.35f), 13.f)]; }
	FString Keys;
	const TArray<TPair<EDCStat, float>> Weights = SortedWeights(J);
	for (const TPair<EDCStat, float>& W : Weights) { Keys += FString::Printf(TEXT("%s%s %d%%"), Keys.IsEmpty() ? TEXT("") : TEXT("  "), DC::StatName(W.Key), FMath::RoundToInt(W.Value * 100.f)); }

	TSharedRef<SWidget> JobSummary = PaperCard(
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f)[Txt(J.DisplayName, Display(24), Ink, true)]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Chip(FString::Printf(TEXT("TIER %s"), *Roman(J.Tier)), Ink, 13.f)]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f, 0.f, 6.f)[Txt(J.Flavour, Type(14), InkSoft, true)]
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[Roles]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(10.f, 0.f, 0.f, 0.f)
			[Txt(FString::Printf(TEXT("Payout %s   Crew %d-%d"), *Money(J.Payout), J.MinCrew, J.MaxCrew), Sans(15), Ink)]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f, 0.f, 0.f)[Txt(FString::Printf(TEXT("Key stats: %s"), *Keys), Sans(13), InkSoft)],
		FMargin(14.f, 10.f));

	// ── The cast (slots) ──
	TArray<EDCCriminalType> OpenRoles = J.RequiredRoles;
	for (int32 Id : PlanCrew) { if (const FDCCriminal* C = R->FindCriminal(Id)) { OpenRoles.RemoveSingle(C->Type); } }
	TSharedRef<SVerticalBox> Slots = SNew(SVerticalBox);
	int32 RoleCursor = 0;
	for (int32 i = 0; i < J.MaxCrew; ++i)
	{
		TSharedRef<SWidget> Row = SNullWidget::NullWidget;
		if (PlanCrew.IsValidIndex(i))
		{
			const int32 Id = PlanCrew[i];
			const FDCCriminal* C = R->FindCriminal(Id);
			if (!C) { continue; }
			Row = SNew(SBorder).BorderImage(Brush(EBrush::PaperCard)).Padding(FMargin(0.f, 0.f, 8.f, 0.f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(6.f)[SNew(SImage).Image(Brush(EBrush::White)).ColorAndOpacity(TypeCol(C->Type))]]
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center).Padding(10.f, 6.f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Txt(C->GetDisplayName(), Sans(17), Ink)]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8.f, 0.f)[TypeChip(C->Type, 12.f)]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Txt(FString::Printf(TEXT("Lv %d"), C->Level), Sans(14), InkSoft)]
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(6.f, 0.f)
				[
					SNew(STextBlock).Font(Sans(14))
					.Text_Lambda([Cache, Id]()
					{
						const FDCSuccessBreakdown& BB = Cache->B;
						if (BB.RefusingCriminalIds.Contains(Id)) { return FText::FromString(TEXT("REFUSES AT THIS CUT")); }
						const float* W = BB.Willingness.Find(Id); const float* F = BB.FairCut.Find(Id);
						return FText::FromString(FString::Printf(TEXT("Willing %s   fair cut %s"), W ? *Pct(*W) : TEXT("-"), F ? *Pct(*F) : TEXT("-")));
					})
					.ColorAndOpacity_Lambda([Cache, Id]()
					{
						const FDCSuccessBreakdown& BB = Cache->B;
						const float* W = BB.Willingness.Find(Id);
						return FSlateColor(BB.RefusingCriminalIds.Contains(Id) ? Red : (W && *W < 0.75f ? Rgb(0xa0, 0x5a, 0x10) : Green));
					})
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[bLocked ? SNullWidget::NullWidget : SNew(SBox).WidthOverride(34.f).HeightOverride(30.f)[Button(TEXT("X"), [this, Id]() { ToggleCrew(Id); }, EBtn::Ghost, true, 14.f, TEXT("Remove from the cast"))]]
			];
		}
		else
		{
			FString Need; FLinearColor NeedCol = InkSoft;
			if (OpenRoles.IsValidIndex(RoleCursor)) { Need = FString::Printf(TEXT("REQUIRED: %s"), DC::TypeName(OpenRoles[RoleCursor])); NeedCol = TypeCol(OpenRoles[RoleCursor]); ++RoleCursor; }
			else if (i < J.MinCrew) { Need = TEXT("NEEDED: anyone"); NeedCol = Red; }
			else { Need = TEXT("OPTIONAL: extra hands"); }
			Row = SNew(SBorder).BorderImage(Brush(EBrush::Outline)).BorderBackgroundColor(WithAlpha(Gold, 0.45f)).Padding(FMargin(12.f, 8.f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[SNew(SBox).WidthOverride(10.f).HeightOverride(10.f)[SNew(SImage).Image(Brush(EBrush::Round)).ColorAndOpacity(NeedCol)]]
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center).Padding(10.f, 0.f)[Txt(Need, Sans(15), NeedCol == InkSoft ? Muted : FLinearColor::LerpUsingHSV(NeedCol, Cream, 0.25f))]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Txt(TEXT("pick from the talent below"), Type(13), WithAlpha(Muted, 0.7f))]
			];
		}
		Slots->AddSlot().AutoHeight().Padding(0.f, 3.f)[Row];
	}

	// ── Talent list ──
	TSharedRef<SVerticalBox> Talent = SNew(SVerticalBox);
	TArray<FDCCriminal> Sorted = R->GetRoster();
	// Useful first: available + matching an open role, then available, then the rest.
	Sorted.StableSort([&](const FDCCriminal& A, const FDCCriminal& Bc)
	{
		auto Score = [&](const FDCCriminal& C) { return (C.IsAvailable() ? 2 : 0) + (J.RequiredRoles.Contains(C.Type) ? 1 : 0); };
		return Score(A) > Score(Bc);
	});
	for (const FDCCriminal& C : Sorted)
	{
		const bool bIn = PlanCrew.Contains(C.Id);
		const bool bCan = !bLocked && (bIn || C.IsAvailable());
		const FDCStatBlock Eff = R->GetEffectiveStats(C.Id);
		TSharedRef<SHorizontalBox> StatsRow = SNew(SHorizontalBox);
		for (int32 w = 0; w < FMath::Min(3, Weights.Num()); ++w)
		{
			StatsRow->AddSlot().AutoWidth().Padding(0.f, 0.f, 10.f, 0.f).VAlign(VAlign_Center)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom)[Txt(TypeAbbrev(Weights[w].Key), Sans(12), InkSoft)]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom).Padding(3.f, 0.f, 0.f, 0.f)[Txt(FString::FromInt(Eff.Get(Weights[w].Key)), Big(24), Ink)]
			];
		}
		const bool bRoleMatch = J.RequiredRoles.Contains(C.Type);
		const int32 Id = C.Id;
		TSharedRef<SWidget> Status = bIn ? Chip(TEXT("IN THE CAST"), Gold, 12.f) : Txt(StatusText(C), Sans(14), StatusColor(C));
		Talent->AddSlot().AutoHeight().Padding(0.f, 3.f)
		[
			ButtonContent(
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(6.f)[SNew(SImage).Image(Brush(EBrush::White)).ColorAndOpacity(TypeCol(C.Type))]]
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center).Padding(10.f, 6.f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Txt(C.GetDisplayName(), Sans(16), Ink)]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8.f, 0.f, 0.f, 0.f)[TypeChip(C.Type, 12.f)]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(6.f, 0.f, 0.f, 0.f)
						[bRoleMatch ? Txt(TEXT("role match"), Type(12), Green) : SNullWidget::NullWidget]
					]
					+ SVerticalBox::Slot().AutoHeight()
					[
						Txt(FString::Printf(TEXT("Lv %d   Luck %d   Loyalty %d"), C.Level, R->GetEffectiveLuck(C.Id), C.Loyalty), Sans(13), InkSoft)
					]
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4.f, 0.f)[StatsRow]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(6.f, 0.f, 12.f, 0.f)[SNew(SBox).MinDesiredWidth(110.f).HAlign(HAlign_Right)[Status]],
				[this, Id]() { ToggleCrew(Id); }, EBtn::Paper, bCan,
				bIn ? TEXT("Click to drop from the cast") : (C.IsAvailable() ? TEXT("Click to cast") : *StatusText(C)))
		];
	}

	// ── Right column: odds, cut, breakdown ──
	BreakdownBox = SNew(SVerticalBox);
	RecomputePlan();

	auto ChanceCol = [Cache]()
	{
		const float S = Cache->B.SuccessChance;
		return FSlateColor(S >= 0.7f ? GreenBright : S >= 0.45f ? Gold : RedBright);
	};

	float MinFair = 1.f;
	for (const TPair<int32, float>& F : B.FairCut) { MinFair = FMath::Min(MinFair, F.Value); }

	TSharedRef<SWidget> Odds = SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()[Txt(TEXT("ODDS OF A WRAP"), Sans(14), Muted)]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, -12.f, 0.f, -8.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(STextBlock).Font(Big(104))
				.Text_Lambda([this, Cache]() { return FText::FromString(PlanCrew.Num() ? Pct(Cache->B.SuccessChance) : FString(TEXT("--"))); })
				.ColorAndOpacity_Lambda([this, ChanceCol]() { return PlanCrew.Num() ? ChanceCol() : FSlateColor(WithAlpha(Muted, 0.5f)); })
			]
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center).Padding(16.f, 0.f, 0.f, 0.f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()[TxtA(TAttribute<FText>::CreateLambda([Cache]() { return FText::FromString(FString::Printf(TEXT("Clean wrap  %s"), *Pct(Cache->B.CleanThreshold))); }), Sans(16), Cream)]
				+ SVerticalBox::Slot().AutoHeight()[TxtA(TAttribute<FText>::CreateLambda([Cache]() { return FText::FromString(FString::Printf(TEXT("Lucky break  %s"), *Pct(Cache->B.LuckyBreakChance))); }), Sans(16), Cream)]
				+ SVerticalBox::Slot().AutoHeight()[TxtA(TAttribute<FText>::CreateLambda([Cache]() { return FText::FromString(FString::Printf(TEXT("Arrest risk if it fails  %s"), *Pct(Cache->B.ArrestChanceOnFail))); }), Sans(16), FSlateColor(Rgb(0xe0, 0x8a, 0x70)))]
			]
		];

	TSharedRef<SWidget> CutBlock = SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Bottom)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()[Txt(TEXT("THE DIRECTOR'S CUT"), Sans(14), Muted)]
				+ SVerticalBox::Slot().AutoHeight()[Txt(MinFair < 1.f ? FString::Printf(TEXT("This crew thinks %s is fair. Greed costs willingness."), *Pct(MinFair)) : FString(TEXT("Your share of the payout. Greed costs willingness.")), Type(13), Muted, true)]
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom)
			[TxtA(TAttribute<FText>::CreateLambda([this]() { return FText::FromString(Pct(PlanCut)); }), Big(48), Gold)]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f)
		[
			SNew(SSlider)
			.Style(&SliderStyle())
			.IsFocusable(false)
			.MinValue(0.10f).MaxValue(0.90f).StepSize(0.05f).MouseUsesStep(true)
			.IsEnabled(!bLocked)
			.Value_Lambda([this]() { return PlanCut; })
			.OnMouseCaptureBegin_Lambda([this]() { bSliderDragging = true; })
			.OnMouseCaptureEnd_Lambda([this]() { bSliderDragging = false; })
			.OnValueChanged_Lambda([this](float V)
			{
				const float Snapped = FMath::Clamp(FMath::RoundToFloat(V * 20.f) / 20.f, 0.10f, 0.90f);
				if (!FMath::IsNearlyEqual(Snapped, PlanCut))
				{
					PlanCut = Snapped;
					RecomputePlan();
					Sfx(DCSfx::CounterTick, 0.5f);
				}
			})
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f)[Txt(TEXT("10%  generous"), Sans(12), Muted)]
			+ SHorizontalBox::Slot().AutoWidth()[Txt(TEXT("greedy  90%"), Sans(12), Muted)]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f, 0.f, 0.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[Txt(TEXT("Your take if it works"), Sans(16), Cream)]
			+ SHorizontalBox::Slot().AutoWidth()[TxtA(TAttribute<FText>::CreateLambda([Cache]() { return FText::FromString(FString::Printf(TEXT("%s of %s"), *Money(Cache->B.DirectorTake), *Money(Cache->B.Payout))); }), Big(30), Gold)]
		];

	TSharedRef<SWidget> Actions = SNullWidget::NullWidget;
	if (bLocked)
	{
		Actions = SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 12.f, 0.f)[Stamp(TEXT("CAST"), RedBright, 30.f, -5.f)]
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[Txt(TEXT("This crew is committed for tonight."), Type(14), Cream, true)]
			+ SHorizontalBox::Slot().AutoWidth().Padding(8.f, 0.f, 0.f, 0.f)
			[Button(TEXT("Recast"), [this]() { if (UDCRunSubsystem* RR = Run()) { RR->CancelPlan(PlanJobId); Toast(TEXT("Plan pulled. Recast as you like."), false); } }, EBtn::Ghost, true, 16.f, TEXT("Cancel this plan and free the crew"))];
	}
	else
	{
		Actions = SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 8.f)
			[
				SNew(STextBlock).Font(Sans(15)).ColorAndOpacity(RedBright).AutoWrapText(true)
				.Text_Lambda([this, Cache]()
				{
					const UDCRunSubsystem* RR = Run();
					if (Cache->B.bValid && RR && RR->GetCommittedPlans().Num() >= RR->GetMaxCrewsPerNight())
					{
						return FText::FromString(TEXT("You can't run more crews tonight. Build the Garage for more."));
					}
					return FText::FromString(Cache->B.bValid ? FString() : Cache->B.InvalidReason);
				})
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.f)
				[
					SNew(SBox).HeightOverride(52.f)
					[
						Button(TEXT("Commit - that's a take"), [this]()
						{
							UDCRunSubsystem* RR = Run();
							if (!RR) { return; }
							FDCCrewPlan P; P.JobId = PlanJobId; P.CriminalIds = PlanCrew; P.Cut = PlanCut;
							FString Err;
							if (RR->CommitPlan(P, &Err))
							{
								Sfx(DCSfx::Commit);
								const FDCJob* JJ = RR->FindJob(P.JobId);
								Toast(FString::Printf(TEXT("%s is cast. Roll Camera when you're ready."), JJ ? *JJ->DisplayName : TEXT("The job")), false);
								PlanCrew.Reset();
								if (RR->GetCommittedPlans().Num() < RR->GetMaxCrewsPerNight()) { Open(EDCPanel::JobBoard); }
								else { Close(); }
							}
							else { Toast(Err.IsEmpty() ? FString(TEXT("Can't commit that plan.")) : Err, true); }
						}, EBtn::Gold, TAttribute<bool>::CreateLambda([Cache]() { return Cache->B.bValid; }), 20.f)
					]
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(10.f, 0.f, 0.f, 0.f)
				[SNew(SBox).HeightOverride(52.f)[Button(TEXT("Back to jobs"), [this]() { Open(EDCPanel::JobBoard); }, EBtn::Ghost, true, 16.f)]]
			];
	}

	TSharedRef<SWidget> Right = SNew(SBorder).BorderImage(Brush(EBrush::PanelInner)).Padding(16.f)
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()[Odds]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 10.f)[HLine(WithAlpha(Gold, 0.4f))]
		+ SVerticalBox::Slot().AutoHeight()[CutBlock]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 14.f, 0.f, 6.f)[Header(TEXT("Why this %"), false, 16.f)]
		+ SVerticalBox::Slot().FillHeight(1.f)[Scroll(BreakdownBox.ToSharedRef())]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 10.f, 0.f, 0.f)[Actions]
	];

	TSharedRef<SWidget> Left = SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()[JobSummary]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 12.f, 0.f, 4.f)[Header(FString::Printf(TEXT("The cast  %d / %d"), PlanCrew.Num(), J.MaxCrew), false, 16.f)]
		+ SVerticalBox::Slot().AutoHeight()[Slots]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 12.f, 0.f, 4.f)[Header(bLocked ? TEXT("Talent (plan locked)") : TEXT("Available talent - click to cast"), false, 16.f)]
		+ SVerticalBox::Slot().FillHeight(1.f)[Scroll(Talent)];

	return PanelShell(TEXT("Crew Planner"),
		TEXT("Casting is everything. Fill the required roles, pick a fair cut, and read the odds before you commit."),
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(0.56f).Padding(0.f, 0.f, 14.f, 0.f)[Left]
		+ SHorizontalBox::Slot().FillWidth(0.44f)[Right]);
}

// ─────────────────────────────────────────────────────────────────────────────
// Criminal card (roster, recruits)
// ─────────────────────────────────────────────────────────────────────────────

TSharedRef<SWidget> SDCRoot::CriminalCard(const FDCCriminal& Cr, bool bRecruit)
{
	UDCRunSubsystem* R = Run();
	const bool bInRoster = R->FindCriminal(Cr.Id) != nullptr;
	const FDCStatBlock Eff = bInRoster ? R->GetEffectiveStats(Cr.Id) : Cr.BaseStats;
	const int32 Luck = bInRoster ? R->GetEffectiveLuck(Cr.Id) : Cr.Luck;

	TSharedRef<SVerticalBox> Stats = SNew(SVerticalBox);
	for (int32 i = 0; i < (int32)EDCStat::MAX; ++i)
	{
		const EDCStat S = (EDCStat)i;
		Stats->AddSlot().AutoHeight().Padding(0.f, 1.f)[StatRow(DC::StatName(S), Eff.Get(S), Eff.Get(S) - Cr.BaseStats.Get(S), TypeCol(Cr.Type), true, 62.f)];
	}

	TSharedRef<SVerticalBox> Gear = SNew(SVerticalBox);
	if (!bRecruit)
	{
		for (int32 s = 0; s < (int32)EDCGearSlot::MAX; ++s)
		{
			const FName* Item = Cr.Equipped.Find((EDCGearSlot)s);
			const FDCGearItem* G = (Item && !Item->IsNone()) ? R->FindGear(*Item) : nullptr;
			Gear->AddSlot().AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(58.f)[Txt(DC::SlotName((EDCGearSlot)s), Sans(13), InkSoft)]]
				+ SHorizontalBox::Slot().FillWidth(1.f)[Txt(G ? G->DisplayName : FString(TEXT("-")), Sans(13), G ? Ink : InkSoft)]
			];
		}
	}

	const FLinearColor LoyCol = Cr.Loyalty < 35 ? Red : Cr.Loyalty > 70 ? Green : Rgb(0xa0, 0x7a, 0x20);
	const float XPFrac = Cr.XPToNext > 0 ? (float)Cr.XP / (float)Cr.XPToNext : 0.f;

	return SNew(SBox).WidthOverride(360.f)
	[
		SNew(SBorder).BorderImage(Brush(EBrush::PaperCard)).Padding(0.f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()[SNew(SBox).HeightOverride(5.f)[SNew(SImage).Image(Brush(EBrush::White)).ColorAndOpacity(TypeCol(Cr.Type))]]
			+ SVerticalBox::Slot().AutoHeight().Padding(14.f, 8.f, 14.f, 12.f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()[Txt(FString::Printf(TEXT("%s %s"), *Cr.FirstName, *Cr.LastName), Sans(21), Ink)]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.f, -3.f, 0.f, 0.f)[Txt(FString::Printf(TEXT("\"%s\""), *Cr.Nickname), Type(14), InkSoft)]
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right)[TypeChip(Cr.Type, 13.f)]
						+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right).Padding(0.f, 2.f, 0.f, 0.f)[Txt(FString::Printf(TEXT("LEVEL %d"), Cr.Level), Big(22), Ink)]
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f, 0.f, 0.f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[SNew(SBox).WidthOverride(62.f)[Txt(TEXT("XP"), Sans(13), InkSoft)]]
					+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[Bar(XPFrac, Gold, WithAlpha(Ink, 0.12f), 6.f)]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8.f, 0.f, 0.f, 0.f)[Txt(FString::Printf(TEXT("%d/%d"), Cr.XP, Cr.XPToNext), Sans(13), InkSoft)]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f)[HLine(WithAlpha(GoldDim, 0.5f))]
				+ SVerticalBox::Slot().AutoHeight()[Stats]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f)[HLine(WithAlpha(GoldDim, 0.5f))]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[SNew(SBox).WidthOverride(62.f)[Txt(TEXT("Luck"), Sans(13), InkSoft)]]
					+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[Pips(Luck, 10, Gold, WithAlpha(Ink, 0.15f), 8.f)]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Txt(FString::FromInt(Luck), Sans(14), Ink)]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 3.f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[SNew(SBox).WidthOverride(62.f)[Txt(TEXT("Loyalty"), Sans(13), InkSoft)]]
					+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[Bar(Cr.Loyalty / 100.f, LoyCol, WithAlpha(Ink, 0.12f), 6.f)]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8.f, 0.f, 0.f, 0.f)[Txt(FString::FromInt(Cr.Loyalty), Sans(14), Ink)]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f, 0.f, 0.f)
				[
					bRecruit ? SNullWidget::NullWidget :
					StaticCastSharedRef<SWidget>(SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.f)[Txt(StatusText(Cr), Sans(15), StatusColor(Cr))]
					+ SHorizontalBox::Slot().AutoWidth()[Txt(FString::Printf(TEXT("%d heist%s pulled"), Cr.HeistsPulled, Cr.HeistsPulled == 1 ? TEXT("") : TEXT("s")), Sans(13), InkSoft)])
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f, 0.f, 0.f)[Gear]
			]
		]
	];
}

TSharedRef<SWidget> SDCRoot::BuildRoster()
{
	UDCRunSubsystem* R = Run();
	TSharedRef<SWrapBox> Wrap = SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(12.f, 12.f));
	for (const FDCCriminal& C : R->GetRoster()) { Wrap->AddSlot()[CriminalCard(C, false)]; }
	const bool bLounge = R->IsRoomBuilt(EDCRoomType::Lounge);
	return PanelShell(TEXT("The Lounge"),
		FString::Printf(TEXT("Your cast: %d of %d bunks.%s"), R->GetRoster().Num(), R->GetRosterCap(),
			bLounge ? TEXT(" Idle crew relax here and grow more loyal.") : TEXT(" Build the Lounge for more bunks.")),
		R->GetRoster().Num() ? Scroll(Wrap) : Txt(TEXT("Nobody's left. Hire someone at the Desk."), Type(18), Muted, true));
}

// ─────────────────────────────────────────────────────────────────────────────
// Workbench
// ─────────────────────────────────────────────────────────────────────────────

TSharedRef<SWidget> SDCRoot::BuildWorkbench()
{
	UDCRunSubsystem* R = Run();
	const TArray<FDCCriminal>& Roster = R->GetRoster();
	if (!R->FindCriminal(BenchCriminal)) { BenchCriminal = Roster.Num() ? Roster[0].Id : INDEX_NONE; }
	const FDCCriminal* Sel = R->FindCriminal(BenchCriminal);

	// ── Who ──
	TSharedRef<SVerticalBox> Who = SNew(SVerticalBox);
	for (const FDCCriminal& C : Roster)
	{
		const int32 Id = C.Id;
		const bool bSel = Id == BenchCriminal;
		int32 NumGear = 0; for (const TPair<EDCGearSlot, FName>& E : C.Equipped) { if (!E.Value.IsNone()) { ++NumGear; } }
		Who->AddSlot().AutoHeight().Padding(0.f, 2.f)
		[
			ButtonContent(
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(bSel ? 10.f : 5.f)[SNew(SImage).Image(Brush(EBrush::White)).ColorAndOpacity(TypeCol(C.Type))]]
				+ SHorizontalBox::Slot().FillWidth(1.f).Padding(10.f, 6.f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()[Txt(FString::Printf(TEXT("%s %s"), *C.FirstName, *C.LastName), Sans(16), Ink)]
					+ SVerticalBox::Slot().AutoHeight()[Txt(FString::Printf(TEXT("%s  -  %d/3 gear"), DC::TypeName(C.Type), NumGear), Sans(13), InkSoft)]
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 10.f, 0.f)[bSel ? Txt(TEXT(">"), Big(26), Gold) : SNullWidget::NullWidget],
				[this, Id]() { BenchCriminal = Id; Rebuild(); }, EBtn::Paper, true)
		];
	}

	// ── Loadout ──
	TSharedRef<SVerticalBox> Loadout = SNew(SVerticalBox);
	if (Sel)
	{
		const FDCStatBlock Eff = R->GetEffectiveStats(Sel->Id);
		TSharedRef<SVerticalBox> Stats = SNew(SVerticalBox);
		for (int32 i = 0; i < (int32)EDCStat::MAX; ++i)
		{
			const EDCStat S = (EDCStat)i;
			Stats->AddSlot().AutoHeight().Padding(0.f, 1.f)[StatRow(DC::StatName(S), Eff.Get(S), Eff.Get(S) - Sel->BaseStats.Get(S), TypeCol(Sel->Type), false, 62.f)];
		}
		Loadout->AddSlot().AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f)[Txt(Sel->GetDisplayName(), Sans(19), Cream, true)]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[TypeChip(Sel->Type, 12.f)]
		];
		Loadout->AddSlot().AutoHeight().Padding(0.f, 8.f)[Stats];
		Loadout->AddSlot().AutoHeight().Padding(0.f, 6.f, 0.f, 4.f)[Header(TEXT("Loadout"), false, 15.f)];
		for (int32 s = 0; s < (int32)EDCGearSlot::MAX; ++s)
		{
			const EDCGearSlot Slot = (EDCGearSlot)s;
			const FName* Item = Sel->Equipped.Find(Slot);
			const FDCGearItem* G = (Item && !Item->IsNone()) ? R->FindGear(*Item) : nullptr;
			const int32 Id = Sel->Id;
			Loadout->AddSlot().AutoHeight().Padding(0.f, 3.f)
			[
				SNew(SBorder).BorderImage(Brush(G ? EBrush::PaperCard : EBrush::Outline)).BorderBackgroundColor(G ? FLinearColor::White : WithAlpha(Gold, 0.4f)).Padding(FMargin(10.f, 6.f))
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.f)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()[Txt(FString(DC::SlotName(Slot)).ToUpper(), Sans(12), G ? InkSoft : Muted)]
						+ SVerticalBox::Slot().AutoHeight()[Txt(G ? G->DisplayName : FString(TEXT("empty slot")), Sans(16), G ? Ink : Muted)]
						+ SVerticalBox::Slot().AutoHeight()[G ? Txt(GearSummary(*G), Sans(13), Green, true) : SNullWidget::NullWidget]
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[G ? Button(TEXT("Unequip"), [this, Id, Slot]() { if (UDCRunSubsystem* RR = Run()) { RR->Unequip(Id, Slot); } }, EBtn::Ghost, true, 13.f, TEXT("Gear goes back in the crate... well, it's gone.")) : SNullWidget::NullWidget]
				]
			];
		}
	}

	// ── Catalogue ──
	TSharedRef<SVerticalBox> Cat = SNew(SVerticalBox);
	TArray<FDCGearItem> Items = R->GetGearCatalogue();
	Items.StableSort([](const FDCGearItem& A, const FDCGearItem& B) { return A.Slot != B.Slot ? (uint8)A.Slot < (uint8)B.Slot : (A.bAvailable != B.bAvailable ? A.bAvailable : A.Price < B.Price); });
	EDCGearSlot LastSlot = EDCGearSlot::MAX;
	const int32 Cash = R->GetCash();
	for (const FDCGearItem& G : Items)
	{
		if (G.Slot != LastSlot)
		{
			LastSlot = G.Slot;
			Cat->AddSlot().AutoHeight().Padding(0.f, 8.f, 0.f, 4.f)[Header(FString::Printf(TEXT("%ss"), DC::SlotName(G.Slot)), false, 15.f)];
		}
		// Delta vs what the selected criminal currently wears in that slot.
		TSharedRef<SWrapBox> Deltas = SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(4.f, 2.f));
		bool bEquipped = false;
		if (Sel)
		{
			const FName* Cur = Sel->Equipped.Find(G.Slot);
			bEquipped = Cur && *Cur == G.Id;
			const FDCGearItem* CurItem = (Cur && !Cur->IsNone()) ? R->FindGear(*Cur) : nullptr;
			for (int32 i = 0; i < (int32)EDCStat::MAX; ++i)
			{
				const int32 D = G.StatBonus.Get((EDCStat)i) - (CurItem ? CurItem->StatBonus.Get((EDCStat)i) : 0);
				if (D != 0 && !bEquipped) { Deltas->AddSlot()[Chip(FString::Printf(TEXT("%+d %s"), D, DC::StatName((EDCStat)i)), D > 0 ? Green : Red, 12.f)]; }
			}
			const int32 DL = G.LuckBonus - (CurItem ? CurItem->LuckBonus : 0);
			if (DL != 0 && !bEquipped) { Deltas->AddSlot()[Chip(FString::Printf(TEXT("%+d Luck"), DL), DL > 0 ? Green : Red, 12.f)]; }
		}
		FString Reason;
		if (!G.bAvailable)
		{
			Reason = G.Tier >= 3 ? TEXT("Needs Workshop level 2") : G.Tier >= 2 ? TEXT("Needs the Workshop") : TEXT("Not available yet");
		}
		else if (!Sel) { Reason = TEXT("Pick a crew member"); }
		else if (bEquipped) { Reason = TEXT("Equipped"); }
		else if (Cash < G.Price) { Reason = FString::Printf(TEXT("Short %s"), *Money(G.Price - Cash)); }
		const bool bCanBuy = Reason.IsEmpty();
		const FName ItemId = G.Id;
		const int32 SelId = Sel ? Sel->Id : INDEX_NONE;
		const FString ItemName = G.DisplayName;

		Cat->AddSlot().AutoHeight().Padding(0.f, 3.f)
		[
			SNew(SBorder).BorderImage(Brush(G.bAvailable ? EBrush::PaperCard : EBrush::PaperShade)).Padding(FMargin(12.f, 8.f))
			.ColorAndOpacity(G.bAvailable ? FLinearColor::White : FLinearColor(1, 1, 1, 0.6f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Txt(G.DisplayName, Sans(17), Ink)]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8.f, 0.f)[Txt(GearSummary(G), Sans(13), InkSoft)]
					]
					+ SVerticalBox::Slot().AutoHeight()[Txt(G.Description, Type(13), InkSoft, true)]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 3.f, 0.f, 0.f)[Deltas]
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(10.f, 0.f, 0.f, 0.f)
				[
					SNew(SBox).WidthOverride(150.f)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right)[Txt(Money(G.Price), Big(26), Ink)]
						+ SVerticalBox::Slot().AutoHeight()
						[
							bCanBuy
							? Button(TEXT("Buy & equip"), [this, ItemId, SelId, ItemName]()
								{
									UDCRunSubsystem* RR = Run(); if (!RR) { return; }
									FString Err;
									if (RR->BuyGear(ItemId, SelId, &Err)) { Sfx(DCSfx::Cash); Toast(FString::Printf(TEXT("Bought %s"), *ItemName), false); }
									else { Toast(Err.IsEmpty() ? FString(TEXT("Can't buy that.")) : Err, true); }
								}, EBtn::Gold, true, 14.f)
							: Txt(Reason, Sans(13), Reason == TEXT("Equipped") ? Green : Red, true, ETextJustify::Right)
						]
					]
				]
			]
		];
	}

	return PanelShell(TEXT("The Workbench"),
		R->IsRoomBuilt(EDCRoomType::Workshop) ? TEXT("Fence catalogue. Gear is permanent - unless the wearer gets pinched.")
		: TEXT("The fence crate: basic gear only. Build the Workshop for the full catalogue and better kit."),
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(250.f)[Scroll(Who)]]
		+ SHorizontalBox::Slot().AutoWidth().Padding(14.f, 0.f)[SNew(SBox).WidthOverride(320.f)[DarkCard(Scroll(Loadout))]]
		+ SHorizontalBox::Slot().FillWidth(1.f)[Scroll(Cat)]);
}

// ─────────────────────────────────────────────────────────────────────────────
// Desk
// ─────────────────────────────────────────────────────────────────────────────

TSharedRef<SWidget> SDCRoot::BuildDesk()
{
	UDCRunSubsystem* R = Run();
	int32 RoomUpkeep = 0; int32 RoomsBuilt = 0;
	for (const FDCRoom& Room : R->GetRooms()) { if (Room.State == EDCRoomState::Built) { RoomUpkeep += Room.UpkeepPerNight; ++RoomsBuilt; } }
	const FDCNightReport& Last = R->GetLastNightReport();

	TSharedRef<SVerticalBox> Ledger = SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()[Header(TEXT("The ledger"), false, 16.f)]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 8.f, 0.f, 2.f)[KeyValue(TEXT("Cash on hand"), Money(R->GetCash()), Gold, false, 22.f)]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f)[KeyValue(TEXT("Upkeep due tonight"), Money(-R->GetUpkeepDue()), Rgb(0xe0, 0x8a, 0x70), false, 20.f)]
		+ SVerticalBox::Slot().AutoHeight().Padding(12.f, 2.f, 0.f, 2.f)[KeyValue(FString::Printf(TEXT("Crew on payroll (%d of %d bunks)"), R->GetRoster().Num(), R->GetRosterCap()), FString::Printf(TEXT("%d"), R->GetRoster().Num()), Cream, false, 15.f)]
		+ SVerticalBox::Slot().AutoHeight().Padding(12.f, 2.f, 0.f, 2.f)[KeyValue(FString::Printf(TEXT("Rooms built (%d)"), RoomsBuilt), Money(-RoomUpkeep) + TEXT(" / night"), Cream, false, 15.f)]
		+ SVerticalBox::Slot().AutoHeight().Padding(12.f, 2.f, 0.f, 2.f)[Txt(TEXT("Wages grow with the roster, and the heat costs more as your legend grows."), Type(13), Muted, true)]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f)[R->GetCash() < R->GetUpkeepDue() ? Txt(TEXT("You can't cover tonight's upkeep. Pull a job or the picture gets cancelled."), Sans(15), RedBright, true) : SNullWidget::NullWidget]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 12.f, 0.f, 4.f)[Header(TEXT("Last night"), false, 16.f)];
	if (Last.Night > 0)
	{
		Ledger->AddSlot().AutoHeight().Padding(0.f, 2.f)[KeyValue(FString::Printf(TEXT("Night %d take"), Last.Night), Money(Last.TotalTake), GreenBright)];
		Ledger->AddSlot().AutoHeight().Padding(0.f, 2.f)[KeyValue(TEXT("Upkeep paid"), Money(-Last.UpkeepPaid), Rgb(0xe0, 0x8a, 0x70))];
		Ledger->AddSlot().AutoHeight().Padding(0.f, 2.f)[KeyValue(TEXT("Net"), Money(Last.CashAfter - Last.CashBefore), (Last.CashAfter >= Last.CashBefore) ? GreenBright : RedBright)];
		for (const FDCHeistResult& H : Last.Results)
		{
			Ledger->AddSlot().AutoHeight().Padding(12.f, 1.f, 0.f, 1.f)[KeyValue(FString::Printf(TEXT("%s - %s"), *H.JobName, DC::OutcomeStamp(H.Outcome)), Money(H.DirectorTake), Cream, false, 14.f)];
		}
	}
	else { Ledger->AddSlot().AutoHeight()[Txt(TEXT("Opening night is still ahead of you."), Type(14), Muted, true)]; }

	const TArray<FDCPerk> Perks = R->GetPerksTaken();
	Ledger->AddSlot().AutoHeight().Padding(0.f, 12.f, 0.f, 4.f)[Header(TEXT("Director's perks"), false, 16.f)];
	if (Perks.Num() == 0) { Ledger->AddSlot().AutoHeight()[Txt(TEXT("Every 3 successful heists you pick a perk."), Type(14), Muted, true)]; }
	for (const FDCPerk& P : Perks)
	{
		Ledger->AddSlot().AutoHeight().Padding(0.f, 2.f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()[Txt(P.DisplayName, Sans(16), Gold)]
			+ SVerticalBox::Slot().AutoHeight()[Txt(P.Description, Sans(13), Cream, true)]
		];
	}
	Ledger->AddSlot().AutoHeight().Padding(0.f, 14.f, 0.f, 0.f)[Txt(FString::Printf(TEXT("Run seed  %d"), R->GetSeed()), Sans(13), Muted)];

	// ── Recruits ──
	TSharedRef<SWrapBox> Recruits = SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(12.f, 12.f));
	const bool bFull = R->GetRoster().Num() >= R->GetRosterCap();
	for (const FDCCriminal& Rc : R->GetRecruits())
	{
		const int32 Id = Rc.Id;
		const bool bAfford = R->GetCash() >= Rc.HireCost;
		FString Why = bFull ? TEXT("No bunks free - build or upgrade the Lounge") : (!bAfford ? FString::Printf(TEXT("Short %s"), *Money(Rc.HireCost - R->GetCash())) : FString());
		Recruits->AddSlot()
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()[CriminalCard(Rc, true)]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f, 0.f, 0.f)
			[
				SNew(SBox).WidthOverride(360.f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Txt(Money(Rc.HireCost), Big(30), Gold)]
					+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center).Padding(10.f, 0.f)[Txt(Why, Sans(13), RedBright, true)]
					+ SHorizontalBox::Slot().AutoWidth()
					[Button(TEXT("Hire"), [this, Id]()
					{
						UDCRunSubsystem* RR = Run(); if (!RR) { return; }
						FString Err;
						if (RR->Hire(Id, &Err)) { Sfx(DCSfx::Cash); }
						else { Toast(Err.IsEmpty() ? FString(TEXT("Can't hire right now.")) : Err, true); }
					}, EBtn::Gold, Why.IsEmpty(), 16.f)]
				]
			]
		];
	}

	return PanelShell(TEXT("The Director's Desk"),
		TEXT("Money in, money out, and new faces looking for work."),
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(360.f)[DarkCard(Scroll(Ledger))]]
		+ SHorizontalBox::Slot().FillWidth(1.f).Padding(14.f, 0.f, 0.f, 0.f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()[Header(FString::Printf(TEXT("Casting call  -  new faces in %d night%s"), R->GetRecruitRefreshIn(), R->GetRecruitRefreshIn() == 1 ? TEXT("") : TEXT("s")), false, 16.f)]
			+ SVerticalBox::Slot().FillHeight(1.f).Padding(0.f, 8.f, 0.f, 0.f)
			[R->GetRecruits().Num() ? Scroll(Recruits) : Txt(TEXT("No one's knocking. Check back in a few nights."), Type(16), Muted)]
		]);
}

// ─────────────────────────────────────────────────────────────────────────────
// Blueprint
// ─────────────────────────────────────────────────────────────────────────────

TSharedRef<SWidget> SDCRoot::BuildBlueprint()
{
	UDCRunSubsystem* R = Run();
	TSharedRef<SWrapBox> Wrap = SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(12.f, 12.f));
	for (const FDCRoom& Room : R->GetRooms())
	{
		const bool bBuilt = Room.State == EDCRoomState::Built;
		const bool bMax = bBuilt && Room.Level >= 3;
		const EDCRoomType RoomT = Room.Type;
		const FString Name = Room.DisplayName.IsEmpty() ? FString(DC::RoomName(Room.Type)) : Room.DisplayName;

		TSharedRef<SVerticalBox> Effects = SNew(SVerticalBox);
		for (int32 i = 0; i < Room.LevelEffects.Num(); ++i)
		{
			const bool bHave = bBuilt && Room.Level >= i + 1;
			const bool bNext = (bBuilt ? Room.Level : 0) == i;
			Effects->AddSlot().AutoHeight().Padding(0.f, 1.f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 6.f, 0.f)[Txt(FString::Printf(TEXT("L%d"), i + 1), Sans(13), bHave ? Green : (bNext ? Ink : InkSoft))]
				+ SHorizontalBox::Slot().FillWidth(1.f)[Txt(Room.LevelEffects[i], Sans(13), bHave ? Ink : (bNext ? Ink : WithAlpha(InkSoft, 0.8f)), true)]
			];
		}

		FString Action = bMax ? TEXT("Max level") : (bBuilt ? FString::Printf(TEXT("Upgrade  %s"), *Money(Room.NextCost)) : FString::Printf(TEXT("Build  %s"), *Money(Room.NextCost)));
		EDCPanel Visit = EDCPanel::None;
		switch (RoomT)
		{
		case EDCRoomType::Lounge: Visit = EDCPanel::Roster; break;
		case EDCRoomType::Workshop: Visit = EDCPanel::Workbench; break;
		case EDCRoomType::ScreeningRoom: Visit = EDCPanel::ScreeningRoom; break;
		case EDCRoomType::Clinic: Visit = EDCPanel::Clinic; break;
		case EDCRoomType::ServerCloset: Visit = EDCPanel::ServerCloset; break;
		case EDCRoomType::LawyerOffice: Visit = EDCPanel::Lawyer; break;
		case EDCRoomType::Office: Visit = EDCPanel::Desk; break;
		default: break;
		}

		TSharedRef<SWidget> StateChip = bBuilt ? Chip(bMax ? TEXT("MAX") : FString::Printf(TEXT("BUILT  L%d"), Room.Level), Green, 12.f)
			: Chip(TEXT("UNDER CONSTRUCTION"), Ink, 12.f);

		Wrap->AddSlot()
		[
			SNew(SBox).WidthOverride(360.f).MinDesiredHeight(300.f)
			[
				SNew(SBorder).BorderImage(Brush(bBuilt ? EBrush::PaperCard : EBrush::PaperShade)).Padding(FMargin(14.f, 12.f))
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[Txt(Name, Display(21), Ink, true)]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[StateChip]
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f)[Pips(bBuilt ? Room.Level : 0, 3, Gold, WithAlpha(Ink, 0.18f), 10.f)]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f, 0.f, 6.f)[Txt(Room.Description, Type(13), InkSoft, true)]
					+ SVerticalBox::Slot().AutoHeight()[Effects]
					+ SVerticalBox::Slot().FillHeight(1.f)[SNew(SSpacer)]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 8.f, 0.f, 4.f)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().FillWidth(1.f)[Txt(Room.UpkeepPerNight > 0 ? FString::Printf(TEXT("Upkeep %s / night"), *Money(Room.UpkeepPerNight)) : FString(), Sans(13), InkSoft)]
						+ SHorizontalBox::Slot().AutoWidth()[(!bBuilt && Room.RequiredHeists > 0) ? Txt(FString::Printf(TEXT("Needs %d heists (%d done)"), Room.RequiredHeists, R->GetHeistsCompleted()), Sans(13), R->GetHeistsCompleted() >= Room.RequiredHeists ? Green : Red) : SNullWidget::NullWidget]
					]
					+ SVerticalBox::Slot().AutoHeight()
					[(!Room.bCanAct && !bMax && !Room.BlockedReason.IsEmpty()) ? Txt(Room.BlockedReason, Sans(14), Red, true) : SNullWidget::NullWidget]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f, 0.f, 0.f)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().FillWidth(1.f)
						[
							Button(Action, [this, RoomT, bBuilt]()
							{
								UDCRunSubsystem* RR = Run(); if (!RR) { return; }
								FString Err;
								const bool bOk = bBuilt ? RR->UpgradeRoom(RoomT, &Err) : RR->BuildRoom(RoomT, &Err);
								if (bOk) { Sfx(DCSfx::Cash); } else { Toast(Err.IsEmpty() ? FString(TEXT("Can't do that yet.")) : Err, true); }
							}, EBtn::Gold, Room.bCanAct && !bMax, 15.f)
						]
						+ SHorizontalBox::Slot().AutoWidth().Padding(6.f, 0.f, 0.f, 0.f)
						[(bBuilt && Visit != EDCPanel::None) ? Button(TEXT("Visit"), [this, Visit]() { Open(Visit); }, EBtn::Ghost, true, 14.f) : SNullWidget::NullWidget]
					]
				]
			]
		];
	}
	return PanelShell(TEXT("The Blueprint Table"),
		TEXT("The old cinema has room to grow. Every room costs upkeep - expand when the money's there."),
		Scroll(Wrap));
}

// ─────────────────────────────────────────────────────────────────────────────
// Room panels
// ─────────────────────────────────────────────────────────────────────────────

TSharedRef<SWidget> SDCRoot::RoomNotBuilt(EDCRoomType Room)
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()[Txt(FString::Printf(TEXT("The %s is still under dust sheets."), DC::RoomName(Room)), Type(18), Cream, true)]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f)[Txt(TEXT("Build it at the Blueprint Table."), Sans(16), Muted)]
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Left).Padding(0.f, 10.f)[Button(TEXT("Open blueprints"), [this]() { Open(EDCPanel::Blueprint); })];
}

TSharedRef<SWidget> SDCRoot::BuildScreening()
{
	UDCRunSubsystem* R = Run();
	const bool bBuilt = R->IsRoomBuilt(EDCRoomType::ScreeningRoom);
	TSharedRef<SVerticalBox> Body = SNew(SVerticalBox);
	if (!bBuilt) { Body->AddSlot().AutoHeight()[RoomNotBuilt(EDCRoomType::ScreeningRoom)]; }
	else
	{
		Body->AddSlot().AutoHeight()[Header(TEXT("Retakes"), false, 16.f)];
		Body->AddSlot().AutoHeight().Padding(0.f, 6.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Txt(FString::FromInt(R->GetRetakesRemaining()), Big(72), Gold)]
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center).Padding(16.f, 0.f)
			[Txt(TEXT("Retakes left tonight. When a heist goes wrong, the Dailies offer a RETAKE: re-shoot it with a fresh roll."), Type(16), Cream, true)]
		];
	}
	Body->AddSlot().AutoHeight().Padding(0.f, 18.f, 0.f, 6.f)[Header(TEXT("The Final Cut"), false, 16.f)];
	Body->AddSlot().AutoHeight()[Txt(TEXT("\"The Premiere\": rob the Grand Palace Theatre on opening night. Pull it off and the picture wraps for good."), Type(16), Cream, true)];
	const FDCJob* Final = R->GetJobs().FindByPredicate([](const FDCJob& J) { return J.bFinalCut; });
	if (Final)
	{
		const int32 Id = Final->Id;
		Body->AddSlot().AutoHeight().Padding(0.f, 12.f).HAlign(HAlign_Left)[SNew(SBox).HeightOverride(52.f)[Button(TEXT("Cast The Premiere"), [this, Id]() { OpenPlanner(Id); }, EBtn::Red, true, 20.f)]];
	}
	else
	{
		const int32 H = R->GetHeistsCompleted();
		Body->AddSlot().AutoHeight().Padding(0.f, 10.f, 0.f, 0.f)[KeyValue(TEXT("Screening Room built"), bBuilt ? TEXT("YES") : TEXT("NO"), bBuilt ? GreenBright : RedBright)];
		Body->AddSlot().AutoHeight().Padding(0.f, 2.f)[KeyValue(TEXT("Successful heists"), FString::Printf(TEXT("%d / 12"), H), H >= 12 ? GreenBright : RedBright)];
		Body->AddSlot().AutoHeight().Padding(0.f, 2.f)[Txt(TEXT("Needs a crew of 5: Face, Ghost, Safecracker, Hacker and Wheelman."), Sans(15), Muted, true)];
	}
	return PanelShell(TEXT("The Screening Room"), TEXT("Velvet seats, a flickering screen, and second chances."), Scroll(Body));
}

TSharedRef<SWidget> SDCRoot::BuildClinic()
{
	UDCRunSubsystem* R = Run();
	TSharedRef<SVerticalBox> Body = SNew(SVerticalBox);
	if (!R->IsRoomBuilt(EDCRoomType::Clinic)) { Body->AddSlot().AutoHeight()[RoomNotBuilt(EDCRoomType::Clinic)]; }
	else
	{
		Body->AddSlot().AutoHeight()[Header(TEXT("On the gurney"), false, 16.f)];
		int32 N = 0;
		for (const FDCCriminal& C : R->GetRoster())
		{
			if (C.Status != EDCCriminalStatus::Injured) { continue; }
			++N;
			const int32 Cost = R->GetPatchUpCost(C.Id);
			const int32 Id = C.Id;
			Body->AddSlot().AutoHeight().Padding(0.f, 4.f)
			[
				PaperCard(
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()[Txt(C.GetDisplayName(), Sans(17), Ink)]
						+ SVerticalBox::Slot().AutoHeight()[Txt(StatusText(C), Sans(14), Red)]
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[
						Cost >= 0
						? Button(FString::Printf(TEXT("Patch up  %s"), *Money(Cost)), [this, Id]()
							{
								UDCRunSubsystem* RR = Run(); if (!RR) { return; }
								FString Err; if (RR->PatchUp(Id, &Err)) { Sfx(DCSfx::Cash); } else { Toast(Err.IsEmpty() ? FString(TEXT("Can't patch them up.")) : Err, true); }
							}, EBtn::Gold, R->GetCash() >= Cost, 15.f)
						: Txt(TEXT("Patch-ups need Clinic level 2"), Sans(14), InkSoft)
					], FMargin(12.f, 8.f))
			];
		}
		if (N == 0) { Body->AddSlot().AutoHeight().Padding(0.f, 6.f)[Txt(TEXT("Nobody's hurt. Keep it that way."), Type(16), Muted)]; }
		Body->AddSlot().AutoHeight().Padding(0.f, 14.f, 0.f, 0.f)[Txt(TEXT("Injuries heal twice as fast with the Clinic built."), Sans(14), Muted, true)];
	}
	return PanelShell(TEXT("Back-Alley Clinic"), TEXT("No questions asked. Cash only."), Scroll(Body));
}

TSharedRef<SWidget> SDCRoot::BuildLawyer()
{
	UDCRunSubsystem* R = Run();
	TSharedRef<SVerticalBox> Body = SNew(SVerticalBox);
	if (!R->IsRoomBuilt(EDCRoomType::LawyerOffice)) { Body->AddSlot().AutoHeight()[RoomNotBuilt(EDCRoomType::LawyerOffice)]; }
	else
	{
		Body->AddSlot().AutoHeight()[Header(TEXT("In the can"), false, 16.f)];
		for (const FDCCriminal& C : R->GetFallen())
		{
			const int32 Cost = R->GetBailCost(C.Id);
			const int32 Id = C.Id;
			Body->AddSlot().AutoHeight().Padding(0.f, 4.f)
			[
				PaperCard(
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()[Txt(C.GetDisplayName(), Sans(17), Ink)]
						+ SVerticalBox::Slot().AutoHeight()[Txt(FString::Printf(TEXT("%s, level %d"), DC::TypeName(C.Type), C.Level), Sans(14), InkSoft)]
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[
						Cost >= 0
						? Button(FString::Printf(TEXT("Post bail  %s"), *Money(Cost)), [this, Id]()
							{
								UDCRunSubsystem* RR = Run(); if (!RR) { return; }
								FString Err; if (RR->BailOut(Id, &Err)) { Sfx(DCSfx::Cash); Toast(TEXT("Charges dropped. Welcome back."), false); } else { Toast(Err.IsEmpty() ? FString(TEXT("The judge isn't budging.")) : Err, true); }
							}, EBtn::Gold, R->GetCash() >= Cost, 15.f)
						: Txt(TEXT("Too late - already sentenced"), Sans(14), Red)
					], FMargin(12.f, 8.f))
			];
		}
		if (R->GetFallen().Num() == 0) { Body->AddSlot().AutoHeight().Padding(0.f, 6.f)[Txt(TEXT("Nobody's behind bars. Your lawyer is bored."), Type(16), Muted)]; }
		Body->AddSlot().AutoHeight().Padding(0.f, 14.f, 0.f, 0.f)[Txt(TEXT("Bail someone out within a night of the arrest. The Lawyer also cuts arrest chances."), Sans(14), Muted, true)];
	}
	return PanelShell(TEXT("Lawyer's Office"), TEXT("Filing cabinets, a typewriter, and a man who knows a judge."), Scroll(Body));
}

TSharedRef<SWidget> SDCRoot::BuildServer()
{
	UDCRunSubsystem* R = Run();
	TSharedRef<SVerticalBox> Body = SNew(SVerticalBox);
	if (!R->IsRoomBuilt(EDCRoomType::ServerCloset)) { Body->AddSlot().AutoHeight()[RoomNotBuilt(EDCRoomType::ServerCloset)]; }
	else
	{
		Body->AddSlot().AutoHeight()[Txt(TEXT("INTEL ACTIVE: exact arrest risk and difficulty on every job. Hacker jobs pay 20% more."), Sans(16), GreenBright, true)];
		Body->AddSlot().AutoHeight().Padding(0.f, 12.f, 0.f, 6.f)[Header(TEXT("Tonight's wiretaps"), false, 16.f)];
		for (const FDCJob& J : R->GetJobs())
		{
			Body->AddSlot().AutoHeight().Padding(0.f, 3.f)
			[
				DarkCard(
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.f)[Txt(J.DisplayName, Sans(17), Cream)]
					+ SHorizontalBox::Slot().AutoWidth().Padding(12.f, 0.f)[Txt(FString::Printf(TEXT("Difficulty %.1f"), J.Difficulty), Sans(15), Muted)]
					+ SHorizontalBox::Slot().AutoWidth().Padding(12.f, 0.f)[Txt(FString::Printf(TEXT("Arrest risk %.1f%%"), J.ArrestRisk * 100.f), Sans(15), Rgb(0xe0, 0x8a, 0x70))]
					+ SHorizontalBox::Slot().AutoWidth()[Txt(Money(J.Payout), Sans(15), Gold)], FMargin(12.f, 6.f))
			];
		}
	}
	return PanelShell(TEXT("Server Closet"), TEXT("Blinking racks and a police scanner that never sleeps."), Scroll(Body));
}

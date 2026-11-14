// Director's Cut — bottom-right mission-result cards (Agent C).
#include "UI/SDCHudNotify.h"
#include "UI/SDCRootInternal.h"
#include "Misc/CommandLine.h"
#include "DirectorsCut.h"

namespace
{
	// Card timeline (seconds of card age).
	constexpr float InTime    = 0.25f;   // slide / fade in
	constexpr float SweepFrom = 0.2f;    // marker starts moving
	constexpr float SweepTime = 0.9f;    // marker settles on the roll
	constexpr float Reveal    = SweepFrom + SweepTime;
	constexpr float OutTime   = 0.35f;   // fade out at the end of the lifetime
	constexpr float CardWidth = 320.f;

	FName OutcomeIcon(EDCMissionOutcome O)
	{
		return O == EDCMissionOutcome::Success ? FName(TEXT("circle-check")) : (O == EDCMissionOutcome::Busted ? FName(TEXT("siren")) : FName(TEXT("circle-x")));
	}

	/** "2 injured · 1 arrested" (empty when nobody was hurt, caught or quit). */
	FString Consequences(const FDCMissionReport& Rep)
	{
		int32 Injured = 0, Arrested = 0, Quit = 0;
		for (const FDCCrewMemberResult& MR : Rep.Members)
		{
			if (MR.bArrested) { ++Arrested; }
			else if (MR.bQuit) { ++Quit; }
			else if (MR.bInjured) { ++Injured; }
		}
		TArray<FString> Parts;
		if (Injured)  { Parts.Add(FString::Printf(TEXT("%d injured"), Injured)); }
		if (Arrested) { Parts.Add(FString::Printf(TEXT("%d arrested"), Arrested)); }
		if (Quit)     { Parts.Add(FString::Printf(TEXT("%d walked out"), Quit)); }
		return FString::Join(Parts, TEXT(" · "));
	}
}

void SDCHudNotify::Construct(const FArguments& InArgs)
{
	Suspended = InArgs._Suspended;
	OnOpenReport = InArgs._OnOpenReport;
	SetVisibility(EVisibility::SelfHitTestInvisible);

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(NoBrush())
		.Padding(0.f)
		.ColorAndOpacity_Lambda([this]() { return FLinearColor(1, 1, 1, ShowAlpha); })
		.Visibility_Lambda([this]() { return ShowAlpha > 0.99f ? EVisibility::SelfHitTestInvisible : EVisibility::HitTestInvisible; })
		[
			SNew(SBox).WidthOverride(CardWidth)
			[
				SAssignNew(Stack, SVerticalBox)
			]
		]
	];

	bDemoPending = !UE_BUILD_SHIPPING && FParse::Param(FCommandLine::Get(), TEXT("DCNotifyDemo"));
	DemoAt = 0.0;
}

void SDCHudNotify::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	if (bDemoPending)
	{
		if (DemoAt <= 0.0) { DemoAt = Now() + 2.0; }
		else if (Now() >= DemoAt) { bDemoPending = false; AddDemoCards(); }
	}

	const bool bHold = Suspended.Get();
	const float Dt = FMath::Min(InDeltaTime, 0.1f);
	ShowAlpha = FMath::Clamp(ShowAlpha + (bHold ? -Dt / 0.12f : Dt / 0.2f), 0.f, 1.f);

	// Reports opened elsewhere (bell, N, a card click) or a new run: those cards have done their job.
	const UDCRunSubsystem* R = UDCRunSubsystem::Get(GetContext());
	for (int32 i = Cards.Num() - 1; i >= 0; --i)
	{
		const int32 Id = Cards[i].ReportId;
		if (Id < 0) { continue; }   // demo cards
		const FDCMissionReport* Rep = (R && R->HasActiveRun()) ? R->GetReports().FindByPredicate([Id](const FDCMissionReport& X) { return X.ReportId == Id; }) : nullptr;
		if (!Rep || Rep->bSeen) { Remove(i); }
	}

	if (bHold) { return; }
	for (int32 i = Cards.Num() - 1; i >= 0; --i)
	{
		FCard& C = Cards[i];
		// Hovering a revealed card holds it so it can be read or clicked.
		if (C.HoverTarget.IsValid() && C.HoverTarget->IsHovered() && *C.Age >= Reveal) { continue; }
		*C.Age += Dt;
		if (*C.Age >= Lifetime) { Remove(i); }
	}
}

void SDCHudNotify::Push(const FDCMissionReport& Report)
{
	UE_LOG(LogDC, Log, TEXT("[Notify] %s · %s · roll %d vs %d"), *Report.Mission.DisplayName, DC::OutcomeName(Report.Outcome),
		FMath::RoundToInt(Report.Roll * 100.f), FMath::RoundToInt(FMath::Min(Report.Mission.LockedChance, 1.f) * 100.f));
	if (!Stack.IsValid()) { return; }

	// Several missions landing together: stagger the rolls so each one reads on its own.
	const double T = Now();
	const float Delay = (T - LastPushAt < 0.3) ? FMath::Min(LastPushDelay + 0.35f, 1.4f) : 0.f;
	LastPushAt = T;
	LastPushDelay = Delay;

	TSharedRef<float> Age = MakeShared<float>(-Delay);
	TSharedPtr<SWidget> Hover;
	TSharedRef<SWidget> W = BuildCard(Report, Age, Hover);
	Stack->AddSlot().AutoHeight().Padding(0.f, 8.f, 0.f, 0.f)[W];
	Cards.Add({ Report.ReportId, W, Hover, Age });
	while (Cards.Num() > MaxVisible) { Remove(0); }
}

void SDCHudNotify::Remove(int32 Index)
{
	if (!Cards.IsValidIndex(Index)) { return; }
	if (Stack.IsValid() && Cards[Index].Widget.IsValid()) { Stack->RemoveSlot(Cards[Index].Widget.ToSharedRef()); }
	Cards.RemoveAt(Index);
}

TSharedRef<SWidget> SDCHudNotify::BuildCard(const FDCMissionReport& Rep, TSharedRef<float> Age, TSharedPtr<SWidget>& OutHoverTarget)
{
	const FDCMission& M = Rep.Mission;
	const EDCMissionOutcome Outcome = Rep.Outcome;
	const bool bOk = Outcome == EDCMissionOutcome::Success;
	const FLinearColor OC = OutcomeColor(Outcome);
	const float Chance = FMath::Clamp(M.LockedChance, 0.f, 1.f);
	const float Roll = FMath::Clamp(Rep.Roll, 0.f, 1.f);
	const int32 RId = Rep.ReportId;
	const int32 Take = Rep.DirectorTake;
	const bool bCalm = Prefs().bReduceMotion;

	// Animation clock. Reduce motion: no sweep or pop, the result is there from the first frame.
	auto P = [Age, bCalm]() { return bCalm ? FMath::Max(*Age, Reveal) : *Age; };
	auto Revealed = [P]() { return P() >= Reveal; };
	auto MarkerPos = [P, Roll]()
	{
		const float S = FMath::Clamp((P() - SweepFrom) / SweepTime, 0.f, 1.f);
		if (S >= 1.f) { return Roll; }
		// Runs past the roll and rattles back onto it, like a ball settling.
		const float Wobble = 0.16f * FMath::Sin(S * PI * 3.f) * (1.f - S);
		return FMath::Clamp(Roll * EaseOut(S) + Wobble, 0.f, 1.f);
	};

	// ── Header: mission icon, name, outcome badge ──
	TSharedRef<SWidget> OutcomeBadge = SNew(SBorder)
		.BorderImage(NoBrush())
		.Padding(0.f)
		.RenderTransformPivot(FVector2D(0.5f, 0.5f))
		.RenderTransform_Lambda([P, bCalm]()
		{
			if (bCalm) { return TOptional<FSlateRenderTransform>(); }
			const float X = P() - Reveal;
			const float S = X < 0.f ? 0.6f : 1.f + 0.25f * FMath::Max(0.f, 1.f - X / 0.25f) * FMath::Sin(FMath::Clamp(X / 0.25f, 0.f, 1.f) * PI);
			return TOptional<FSlateRenderTransform>(FSlateRenderTransform(S));
		})
		.ColorAndOpacity_Lambda([P]() { return FLinearColor(1, 1, 1, EaseOut((P() - Reveal) / 0.15f)); })
		[
			SNew(SBorder)
			.BorderImage(Box(A(OC, 0.14f), 6.f, A(OC, 0.6f), 1.f))
			.Padding(FMargin(8.f, 2.f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 5.f, 0.f)[Icon(OutcomeIcon(Outcome), 10.f, OC)]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Txt(FString(DC::OutcomeName(Outcome)).ToUpper(), Font(8.5f, EW::Bold, 120), OC)]
			]
		];

	TSharedRef<SWidget> Header = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 10.f, 0.f)[IconTile(M.Icon, 30.f, M.bFinal ? Accent : TierColor(M.Tier))]
		+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()[Label(TEXT("MISSION COMPLETE"), TextFaint, 7.5f)]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 1.f, 0.f, 0.f)
			[SNew(STextBlock).Text(FText::FromString(M.DisplayName)).Font(Font(11.5f, EW::SemiBold)).ColorAndOpacity(Text).OverflowPolicy(ETextOverflowPolicy::Ellipsis)]
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8.f, 0.f, 0.f, 0.f)[OutcomeBadge];

	// ── Roll bar: success zone up to the chance, marker sweeps and settles on the roll ──
	TSharedRef<SWidget> RollRow = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 8.f, 0.f)[Icon(TEXT("dices"), 11.f, TextFaint)]
		+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
		[
			SNew(SDCRollBar).Height(6.f).Chance(Chance).ZoneColor(A(Good, 0.55f))
			.Roll_Lambda(MarkerPos)
			.MarkerColor_Lambda([Revealed, OC]() { return Revealed() ? OC : Text; })
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(10.f, 0.f, 0.f, 0.f)
		[
			SNew(SBox).MinDesiredWidth(54.f).HAlign(HAlign_Right)
			[
				TxtA(TAttribute<FText>::CreateLambda([MarkerPos, Chance]() { return FText::FromString(FString::Printf(TEXT("%d / %d"), FMath::RoundToInt(MarkerPos() * 100.f), FMath::RoundToInt(Chance * 100.f))); }),
					Mono(9.f), TAttribute<FSlateColor>::CreateLambda([Revealed, OC]() { return FSlateColor(Revealed() ? OC : TextDim); }))
			]
		];

	// ── Result line: payout on success, what it cost on failure ──
	TSharedRef<SHorizontalBox> Result = SNew(SHorizontalBox);
	if (bOk)
	{
		Result->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 8.f, 0.f)[Label(TEXT("YOUR TAKE"), TextFaint, 7.5f)];
		Result->AddSlot().AutoWidth().VAlign(VAlign_Center)
		[TxtA(TAttribute<FText>::CreateLambda([P, Take]() { const float X = FMath::Clamp((P() - Reveal) / 0.6f, 0.f, 1.f); return FText::FromString(FString(TEXT("+")) + Money(FMath::RoundToInt(Take * EaseOut(X)))); }),
			Mono(12.f), FSlateColor(Take > 0 ? Accent : TextDim))];
		if (Rep.bBonus) { Result->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(8.f, 0.f, 0.f, 0.f)[Pill(IconText(TEXT("sparkles"), TEXT("BONUS"), Caps(7.5f), Accent, Accent, 9.f, 4.f), A(Accent, 0.12f), FMargin(6.f, 1.f))]; }
		if (!Rep.LootGearId.IsNone()) { Result->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(8.f, 0.f, 0.f, 0.f)[IconText(TEXT("package"), TEXT("Loot"), Font(9.f, EW::Medium), Accent, TextDim, 10.f, 4.f)]; }
	}
	else
	{
		const FString Cost = Consequences(Rep);
		Result->AddSlot().AutoWidth().VAlign(VAlign_Center)[IconText(TEXT("ban"), TEXT("No payout"), Font(9.5f, EW::Medium), TextFaint, TextDim, 10.f, 5.f)];
		if (!Cost.IsEmpty())
		{
			Result->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(10.f, 0.f, 0.f, 0.f)
			[IconText(Outcome == EDCMissionOutcome::Busted ? TEXT("siren") : TEXT("bandage"), Cost, Font(9.5f, EW::Medium), Bad, Bad, 10.f, 5.f)];
		}
	}
	Result->AddSlot().FillWidth(1.f)[SNullWidget::NullWidget];
	Result->AddSlot().AutoWidth().VAlign(VAlign_Center)[IconText(TEXT("scroll-text"), TEXT("View"), Font(8.5f, EW::Medium), TextFaint, TextFaint, 9.f, 4.f)];

	TSharedRef<SWidget> ResultRow = SNew(SBorder)
		.BorderImage(NoBrush())
		.Padding(0.f)
		.ColorAndOpacity_Lambda([P]() { return FLinearColor(1, 1, 1, EaseOut((P() - Reveal - 0.05f) / 0.2f)); })
		[Result];

	// ── Remaining-time hairline ──
	TSharedRef<SWidget> TimeLine = Bar(TAttribute<float>::CreateLambda([Age]() { return 1.f - FMath::Clamp(*Age / Lifetime, 0.f, 1.f); }), FSlateColor(A(OC, 0.5f)), 2.f, FLinearColor(1, 1, 1, 0.04f));

	TSharedRef<SWidget> Content = SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()[Header]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 10.f, 0.f, 0.f)[RollRow]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 8.f, 0.f, 0.f)[ResultRow]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 10.f, 0.f, 0.f)[TimeLine];

	TFunction<void(int32)> Open = OnOpenReport;
	TSharedRef<SWidget> Btn = Button(Content, [Open, RId]() { if (Open) { Open(RId); } }, EBtn::Card, true,
		Tip(FString::Printf(TEXT("Rolled %d, needed under %d"), FMath::RoundToInt(Roll * 100.f), FMath::RoundToInt(Chance * 100.f)), TEXT("Click to open Reports (N)"), TEXT("dices")),
		FMargin(12.f, 10.f, 12.f, 8.f));
	OutHoverTarget = Btn;

	// Glass behind the card so it reads over the world; outcome-tinted edge once revealed.
	return SNew(SBorder)
		.BorderImage(NoBrush())
		.Padding(0.f)
		.Visibility(EVisibility::SelfHitTestInvisible)
		.ColorAndOpacity_Lambda([Age]()
		{
			const float X = *Age;
			return FLinearColor(1, 1, 1, FMath::Min(EaseOut(X / (Prefs().bReduceMotion ? 0.15f : InTime)), 1.f - FMath::Clamp((X - (Lifetime - OutTime)) / OutTime, 0.f, 1.f)));
		})
		.RenderTransform_Lambda([Age, bCalm]()
		{
			if (bCalm) { return TOptional<FSlateRenderTransform>(); }
			return TOptional<FSlateRenderTransform>(FSlateRenderTransform(FVector2f((1.f - EaseOut(*Age / InTime)) * 40.f, 0.f)));
		})
		[
			SNew(SBorder)
			.BorderImage_Lambda([Revealed, OC]() { return Box(Hex(0x0E1115, 0.94f), 11.f, Revealed() ? A(OC, 0.45f) : LineHi, 1.f); })
			.Padding(1.f)
			[Btn]
		];
}

void SDCHudNotify::AddDemoCards()
{
	auto Make = [](int32 Id, const TCHAR* Name, FName IconName, int32 Tier, float Chance, float Roll, EDCMissionOutcome O, int32 Take)
	{
		FDCMissionReport Rep;
		Rep.ReportId = Id;
		Rep.Mission.DisplayName = Name;
		Rep.Mission.Icon = IconName;
		Rep.Mission.Tier = Tier;
		Rep.Mission.LockedChance = Chance;
		Rep.Roll = Roll;
		Rep.Outcome = O;
		Rep.DirectorTake = Take;
		Rep.PayoutTotal = Take * 3;
		return Rep;
	};
	FDCMissionReport A1 = Make(-1001, TEXT("Loop the Feed"), TEXT("cctv"), 1, 0.72f, 0.41f, EDCMissionOutcome::Success, 1850);
	A1.bBonus = true;
	FDCMissionReport A2 = Make(-1002, TEXT("Night Shift at the Depot"), TEXT("warehouse"), 2, 0.55f, 0.83f, EDCMissionOutcome::Busted, 0);
	{ FDCCrewMemberResult MR; MR.bArrested = true; A2.Members.Add(MR); MR.bArrested = false; MR.bInjured = true; A2.Members.Add(MR); }
	FDCMissionReport A3 = Make(-1003, TEXT("Lift the Ledger"), TEXT("book-lock"), 1, 0.64f, 0.70f, EDCMissionOutcome::Failed, 0);
	{ FDCCrewMemberResult MR; MR.bInjured = true; A3.Members.Add(MR); }
	Push(A1); Push(A2); Push(A3);
}

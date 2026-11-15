// Director's Cut — always-visible crew list (HUD, bottom-left): a floating header over one card per crew member.
#include "UI/SDCRootInternal.h"
#include "DirectorsCut.h"

namespace
{
	// Width is capped so the list always clears the bottom-centre dock at 1280 px and up.
	constexpr float HudCrewWidth = 360.f;
	constexpr float HudCrewMaxListHeight = 440.f;

	FLinearColor HudLoyaltyColor(int32 L) { return L >= 60 ? Good : (L >= 30 ? Warn : Bad); }
}

uint32 SDCRoot::ComputeHudCrewSignature() const
{
	const UDCRunSubsystem* R = Run();
	if (!R || !R->HasActiveRun()) { return 0; }
	uint32 H = 53;
	auto Mix = [&H](uint32 V) { H = HashCombine(H, V); };
	for (const FDCCriminal& C : R->GetRoster())
	{
		Mix(C.Id); Mix((uint32)C.Status); Mix(C.Level); Mix(C.Loyalty); Mix(C.ActiveMissionId); Mix((uint32)C.Quality);
		Mix(C.Abilities.Num()); Mix(C.Traits.Num()); Mix(C.XP);
	}
	Mix(R->GetRosterCap()); Mix(R->GetUpkeepDue());
	return H;
}

void SDCRoot::TickHudCrew()
{
	const uint32 Sig = ComputeHudCrewSignature();
	if (Sig != LastHudCrewSig) { LastHudCrewSig = Sig; RebuildHudCrew(); }
}

TSharedRef<SWidget> SDCRoot::BuildHudCrew()
{
	SAssignNew(HudCrewBox, SVerticalBox);
	LastHudCrewSig = ComputeHudCrewSignature();
	RebuildHudCrew();

	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SBox).WidthOverride(HudCrewWidth)[HudCrewBox.ToSharedRef()]
		]
		// Leave room for the FPS readout, which shares this corner.
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SBox).HeightOverride(22.f).Visibility_Lambda([]() { return Prefs().bShowFPS ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
		];
}

void SDCRoot::RebuildHudCrew()
{
	if (!HudCrewBox.IsValid()) { return; }
	HudCrewBox->ClearChildren();
	const UDCRunSubsystem* R = Run();
	if (!R || !R->HasActiveRun()) { return; }
	const TArray<FDCCriminal>& Roster = R->GetRoster();

	if (Roster.Num() == 0)
	{
		TSharedRef<SWidget> Hint = Pill(IconText(TEXT("user-plus"), TEXT("No crew — hire from Recruits"), Font(11.f), TextFaint, TextDim, 13.f, 7.f),
			A(GlassSoft, 0.75f), FMargin(14.f, 8.f), Line);
		HudCrewBox->AddSlot().AutoHeight().HAlign(HAlign_Left)
		[
			Button(Hint, [this]() { Open(EDCPanel::Recruits); }, EBtn::Ghost, true, Tip(TEXT("Recruits"), TEXT("Hire your first crew member"), TEXT("user-plus")), FMargin(0.f))
		];
		return;
	}

	int32 Ready = 0;
	for (const FDCCriminal& C : Roster) { if (C.IsAvailable()) { ++Ready; } }

	TSharedRef<SWidget> Header = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Icon(TEXT("users"), 13.f, TextDim)]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8.f, 0.f, 0.f, 0.f)[Txt(TEXT("CREW"), Caps(9.5f), TextDim)]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8.f, 0.f, 0.f, 0.f)
		[Txt(FString::Printf(TEXT("%d/%d"), Roster.Num(), R->GetRosterCap()), Mono(11.f), TextFaint)]
		+ SHorizontalBox::Slot().FillWidth(1.f)[SNullWidget::NullWidget]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[
			SNew(SBox).ToolTip(Tip(TEXT("Ready"), TEXT("Crew free to take a job right now"), TEXT("circle-check"), Good))
			[IconText(TEXT("circle-check"), FString::FromInt(Ready), Mono(11.f), Good, Text, 12.f, 5.f)]
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(14.f, 0.f, 0.f, 0.f)
		[
			SNew(SBox).ToolTip(Tip(TEXT("Daily upkeep"), TEXT("Crew wages and building upkeep, paid at 06:00"), TEXT("wallet"), Accent))
			[IconText(TEXT("wallet"), FString::Printf(TEXT("%s/day"), *MoneyShort(R->GetUpkeepDue())), Mono(11.f), TextDim, TextDim, 12.f, 5.f)]
		];

	TSharedRef<SVerticalBox> Rows = SNew(SVerticalBox);
	for (const FDCCriminal& C : Roster) { Rows->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 6.f)[HudCrewRow(C)]; }

	// Keep the scroll position across rebuilds (the scroll box is new each time).
	TSharedRef<SScrollBox> ScrollBox = SNew(SScrollBox)
		.ScrollBarStyle(&ScrollBarStyle())
		.ScrollBarThickness(FVector2D(4.f, 4.f))
		.ScrollBarPadding(FMargin(6.f, 0.f, 0.f, 0.f))
		.OnUserScrolled_Lambda([this](float Offset) { HudCrewScroll = Offset; })
		+ SScrollBox::Slot()[Rows];
	ScrollBox->SetScrollOffset(HudCrewScroll);

	// No container: the header floats over the cards, and each crew member is its own card.
	HudCrewBox->AddSlot().AutoHeight().Padding(4.f, 0.f, 4.f, 10.f)[Pill(Header, A(GlassSoft, 0.85f), FMargin(12.f, 7.f), Line)];
	HudCrewBox->AddSlot().AutoHeight()[SNew(SBox).MaxDesiredHeight(HudCrewMaxListHeight)[ScrollBox]];
}

TSharedRef<SWidget> SDCRoot::HudCrewRow(const FDCCriminal& C)
{
	const int32 CId = C.Id;
	auto RR = [this]() { return Run(); };
	const UDCRunSubsystem* R = Run();
	const FLinearColor SC = StatusColor(C);
	const FLinearColor LC = HudLoyaltyColor(C.Loyalty);
	const float Eff = R ? R->GetEffectiveLevel(CId) : (float)C.Level;
	const bool bBoosted = !FMath::IsNearlyEqual(Eff, (float)C.Level);

	// Live status line: "Available" / "<mission> · 1:23" / "Injured · 0:45".
	auto StatusText = [RR, CId]() -> FText
	{
		const UDCRunSubsystem* X = RR();
		const FDCCriminal* Q = X ? X->FindCriminal(CId) : nullptr;
		if (!Q) { return FText::GetEmpty(); }
		switch (Q->Status)
		{
		case EDCCriminalStatus::OnMission:
		{
			const FDCMission* M = X->FindMission(Q->ActiveMissionId);
			return FText::FromString(FString::Printf(TEXT("%s · %s"), M ? *M->DisplayName : TEXT("On a job"), *DC::Clock(X->GetMissionTimeRemaining(Q->ActiveMissionId))));
		}
		case EDCCriminalStatus::Injured:  return FText::FromString(FString::Printf(TEXT("Injured · %s"), *DC::Clock(Q->InjuredUntil - X->GetRunTime())));
		case EDCCriminalStatus::Arrested: return FText::FromString(TEXT("Arrested"));
		default:                          return FText::FromString(TEXT("Available"));
		}
	};

	TSharedRef<SHorizontalBox> Abilities = SNew(SHorizontalBox);
	for (FName Id : C.Abilities)
	{
		const FDCAbility* Ab = R ? R->FindAbility(Id) : nullptr;
		Abilities->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(5.f, 0.f, 0.f, 0.f)[Icon(Ab ? Ab->Icon : FName(TEXT("zap")), 11.f, A(DC::TypeColor(C.Type), 0.85f))];
	}

	TSharedRef<SWidget> Content = SNew(SHorizontalBox)
		// Avatar with a status dot
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 12.f, 0.f)
		[
			SNew(SOverlay)
			+ SOverlay::Slot()[Avatar(C, 40.f, !C.IsAvailable())]
			+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Bottom).Padding(FMargin(0.f, 0.f, -2.f, -2.f))
			[
				SNew(SBox).WidthOverride(12.f).HeightOverride(12.f)
				[SNew(SBorder).BorderImage(Box(SC, -1.f, Hex(0x0B0D10), 2.f)).Padding(0.f)]
			]
		]
		// Name + live status
		+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
				[SNew(STextBlock).Text(FText::FromString(C.GetShortName())).Font(Font(13.f, EW::SemiBold)).ColorAndOpacity(Text).OverflowPolicy(ETextOverflowPolicy::Ellipsis)]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Abilities]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f, 0.f, 0.f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 5.f, 0.f)[Icon(StatusIcon(C), 11.f, SC)]
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
				[SNew(STextBlock).Text_Lambda(StatusText).Font(Font(11.f)).ColorAndOpacity(SC).OverflowPolicy(ETextOverflowPolicy::Ellipsis)]
			]
		]
		// Level
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(12.f, 0.f, 0.f, 0.f)
		[
			SNew(SBox).WidthOverride(38.f).HAlign(HAlign_Center)
			.ToolTip(Tip(TEXT("Level"), bBoosted ? FString::Printf(TEXT("Level %d · effective %.1f with gear and perks"), C.Level, Eff) : FString::Printf(TEXT("Level %d"), C.Level), TEXT("chevrons-up"), Info))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[Txt(TEXT("LV"), Caps(8.f), TextFaint)]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
				[Txt(bBoosted ? FString::Printf(TEXT("%.1f"), Eff) : FString::FromInt(C.Level), Mono(13.f), bBoosted ? Accent : Text)]
			]
		]
		// Loyalty: heart + number over a small bar
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(10.f, 0.f, 2.f, 0.f)
		[
			SNew(SBox).WidthOverride(48.f)
			.ToolTip(Tip(TEXT("Loyalty"), FString::Printf(TEXT("%d / 100. Loyal crew accept a bigger cut. Below 10 they walk."), C.Loyalty), TEXT("heart"), LC))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[IconText(TEXT("heart"), FString::FromInt(C.Loyalty), Mono(13.f), LC, Text, 12.f, 4.f)]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 5.f, 0.f, 0.f)[Bar(FMath::Clamp(C.Loyalty / 100.f, 0.f, 1.f), FSlateColor(LC), 4.f)]
			]
		];

	TSharedRef<SWidget> TipBody = SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()[CrewTip(C)]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 8.f, 0.f, 0.f)[Txt(TEXT("Click to open in Crew"), Font(9.f), TextFaint)];

	return Button(Content, [this, CId]()
	{
		CrewSel = CId;
		DismissArmed = INDEX_NONE;
		Open(EDCPanel::Crew);
	}, EBtn::Card, true, TipWidget(TipBody), FMargin(14.f, 12.f));
}

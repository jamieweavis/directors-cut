// Director's Cut — crew roster, workbench (gear) and recruits desk (Agent C).
#include "UI/SDCRootInternal.h"
#include "DirectorsCut.h"

// ─────────────────────────────────────────────────────────────────────────────
// Small crew widgets
// ─────────────────────────────────────────────────────────────────────────────

static FLinearColor LoyaltyColor(int32 L) { return L >= 60 ? Good : (L >= 30 ? Warn : Bad); }

TSharedRef<SWidget> SDCRoot::AbilityChips(const FDCCriminal& C, float Size, const TArray<EDCThreat>* Highlight)
{
	const UDCRunSubsystem* R = Run();
	TSharedRef<SHorizontalBox> H = SNew(SHorizontalBox);
	for (FName Id : C.Abilities)
	{
		const FDCAbility* Ab = R ? R->FindAbility(Id) : nullptr;
		const EDCThreat T = Ab ? Ab->Counters : EDCThreat::MAX;
		const bool bHi = Highlight && Highlight->Contains(T);
		const FLinearColor Col = bHi ? Good : DC::TypeColor(C.Type);
		TSharedRef<SWidget> Chip = SNew(SBorder)
			.BorderImage(Box(A(Col, 0.10f), 6.f, A(Col, 0.30f), 1.f))
			.Padding(FMargin(5.f, 2.f, 6.f, 2.f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Icon(Ab ? Ab->Icon : FName(TEXT("zap")), Size, Col)]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(3.f, 0.f)[Icon(TEXT("chevron-right"), Size * 0.7f, TextFaint)]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Icon(DC::ThreatIcon(T), Size, TextDim)]
			];
		Chip->SetToolTip(Tip(Ab ? Ab->DisplayName : Id.ToString(), FString::Printf(TEXT("Counters %s"), DC::ThreatName(T)), Ab ? Ab->Icon : FName(TEXT("zap")), Col));
		H->AddSlot().AutoWidth().Padding(0.f, 0.f, 4.f, 0.f)[Chip];
	}
	return H;
}

TSharedRef<SWidget> SDCRoot::TraitChips(const TArray<FName>& Traits, float Size)
{
	const UDCRunSubsystem* R = Run();
	TSharedRef<SHorizontalBox> H = SNew(SHorizontalBox);
	for (FName Id : Traits)
	{
		const FDCTrait* Tr = R ? R->FindTrait(Id) : nullptr;
		const bool bNeg = Tr && Tr->bNegative;
		const FLinearColor Col = bNeg ? Bad : Hex(0xC4B5FD);
		TSharedRef<SWidget> Tile = SNew(SBox).WidthOverride(Size * 1.9f).HeightOverride(Size * 1.9f)
			[
				SNew(SBorder).BorderImage(Box(A(Col, 0.10f), 6.f, A(Col, 0.30f), 1.f)).HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(0.f)
				[Icon(Tr ? Tr->Icon : FName(TEXT("sparkle")), Size, Col)]
			];
		Tile->SetToolTip(Tip(Tr ? Tr->DisplayName : Id.ToString(), Tr ? Tr->Description : FString(), Tr ? Tr->Icon : FName(TEXT("sparkle")), Col));
		H->AddSlot().AutoWidth().Padding(0.f, 0.f, 4.f, 0.f)[Tile];
	}
	return H;
}

TSharedRef<SWidget> SDCRoot::GearSlots(const FDCCriminal& C, float Size)
{
	const UDCRunSubsystem* R = Run();
	const int32 N = R ? FMath::Clamp(R->GetGearSlotCount(), 0, (int32)EDCGearSlot::MAX) : 2;
	TSharedRef<SHorizontalBox> H = SNew(SHorizontalBox);
	for (int32 i = 0; i < N; ++i)
	{
		const EDCGearSlot S = (EDCGearSlot)i;
		const FName* Item = C.Equipped.Find(S);
		const bool bHas = Item && !Item->IsNone();
		const FDCGearItem* G = (bHas && R) ? R->FindGear(*Item) : nullptr;
		TSharedRef<SWidget> Tile = SNew(SBox).WidthOverride(Size * 1.9f).HeightOverride(Size * 1.9f)
			[
				SNew(SBorder).BorderImage(bHas ? Box(A(Accent, 0.10f), 6.f, A(Accent, 0.35f), 1.f) : Box(FLinearColor::Transparent, 6.f, A(FLinearColor::White, 0.10f), 1.f))
				.HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(0.f)
				[Icon(bHas ? (G ? G->Icon : FName(TEXT("package"))) : DC::SlotIcon(S), Size, bHas ? Accent : TextFaint)]
			];
		Tile->SetToolTip(Tip(bHas ? (G ? G->DisplayName : Item->ToString()) : FString::Printf(TEXT("%s slot · empty"), DC::SlotName(S)), G ? G->Description : FString(), DC::SlotIcon(S), bHas ? Accent : TextDim));
		H->AddSlot().AutoWidth().Padding(0.f, 0.f, 4.f, 0.f)[Tile];
	}
	return H;
}

TSharedRef<SWidget> SDCRoot::StatusChip(const FDCCriminal& C)
{
	const int32 CId = C.Id;
	auto RR = [this]() { return Run(); };
	if (C.IsAvailable())
	{
		return Pill(IconText(TEXT("circle-check"), TEXT("READY"), Caps(7.5f), Good, Good, 9.f, 4.f), A(Good, 0.10f), FMargin(7.f, 2.f));
	}
	const FLinearColor SC = StatusColor(C);
	return Pill(
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 4.f, 0.f)[Icon(StatusIcon(C), 10.f, SC)]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[TxtA(TAttribute<FText>::CreateLambda([RR, CId]()
			{
				const UDCRunSubsystem* X = RR(); const FDCCriminal* Q = X ? X->FindCriminal(CId) : nullptr;
				if (!Q) { return FText::GetEmpty(); }
				if (Q->Status == EDCCriminalStatus::Injured) { return FText::FromString(DC::Clock(Q->InjuredUntil - X->GetRunTime())); }
				if (Q->Status == EDCCriminalStatus::OnMission) { return FText::FromString(DC::Clock(X->GetMissionTimeRemaining(Q->ActiveMissionId))); }
				return FText::FromString(TEXT("—"));
			}), Mono(9.f), FSlateColor(SC))],
		A(SC, 0.12f), FMargin(7.f, 2.f));
}

TSharedRef<SWidget> SDCRoot::CrewTip(const FDCCriminal& C) const
{
	const UDCRunSubsystem* R = Run();
	TSharedRef<SVerticalBox> V = SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()[Txt(C.GetDisplayName(), Font(11.f, EW::SemiBold), Text)]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f, 0.f, 6.f)
		[Txt(FString::Printf(TEXT("%s %s · level %d"), DC::QualityName(C.Quality), DC::TypeName(C.Type), C.Level), Font(9.5f), DC::QualityColor(C.Quality))];
	if (R)
	{
		for (FName Id : C.Abilities)
		{
			if (const FDCAbility* Ab = R->FindAbility(Id))
			{
				V->AddSlot().AutoHeight().Padding(0.f, 1.f)[IconText(Ab->Icon, FString::Printf(TEXT("%s  ›  %s"), *Ab->DisplayName, DC::ThreatName(Ab->Counters)), Font(9.5f), DC::TypeColor(C.Type), TextDim, 10.f, 6.f)];
			}
		}
		for (FName Id : R->GetAllTraits(C.Id))
		{
			if (const FDCTrait* Tr = R->FindTrait(Id))
			{
				V->AddSlot().AutoHeight().Padding(0.f, 1.f)[IconText(Tr->Icon, FString::Printf(TEXT("%s · %s"), *Tr->DisplayName, *Tr->Description), Font(9.5f), Tr->bNegative ? Bad : Hex(0xC4B5FD), TextDim, 10.f, 6.f)];
			}
		}
	}
	return V;
}

TSharedRef<SWidget> SDCRoot::CrewCard(const FDCCriminal& C, bool bRecruit)
{
	const UDCRunSubsystem* R = Run();
	const int32 CId = C.Id;
	const FLinearColor QC = DC::QualityColor(C.Quality);
	const FLinearColor TC = DC::TypeColor(C.Type);
	const float XPFrac = C.XPToNext > 0 ? (float)C.XP / (float)C.XPToNext : 0.f;

	TSharedRef<SWidget> Head = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 12.f, 0.f)
		[
			SNew(SOverlay)
			+ SOverlay::Slot()[Avatar(C, 52.f, !bRecruit && C.Status == EDCCriminalStatus::Injured)]
			+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Bottom).Padding(FMargin(0.f, 0.f, -6.f, -3.f))
			[SNew(SBorder).BorderImage(Box(Hex(0x0B0D10), -1.f, A(QC, 0.8f), 1.f)).Padding(FMargin(5.f, 0.f))[Txt(FString::FromInt(C.Level), Mono(9.f), Text)]]
		]
		+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[SNew(STextBlock).Text(FText::FromString(C.GetShortName())).Font(Font(13.f, EW::SemiBold)).ColorAndOpacity(Text).OverflowPolicy(ETextOverflowPolicy::Ellipsis)]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f, 0.f, 0.f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Txt(FString(DC::TypeName(C.Type)).ToUpper(), Caps(7.5f), TC)]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(6.f, 0.f)[Txt(TEXT("·"), Font(9.f), TextFaint)]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Txt(FString(DC::QualityName(C.Quality)).ToUpper(), Caps(7.5f), QC)]
			]
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top)
		[
			bRecruit ? SNullWidget::NullWidget : StatusChip(C)
		];

	TSharedRef<SWidget> XP = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 8.f, 0.f)[Label(TEXT("XP"), TextFaint, 7.f)]
		+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[Bar(XPFrac, FSlateColor(Info), 3.f)]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(12.f, 0.f, 0.f, 0.f)
		[
			SNew(SBox).ToolTip(Tip(TEXT("Loyalty"), TEXT("Loyal crew accept a bigger cut. Below 10 they walk."), TEXT("heart"), LoyaltyColor(C.Loyalty)))
			[IconText(TEXT("heart"), FString::FromInt(C.Loyalty), Mono(9.5f), LoyaltyColor(C.Loyalty), Text, 10.f, 4.f)]
		];

	TSharedRef<SWidget> Kit = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[AbilityChips(C, 10.f)]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4.f, 0.f, 0.f, 0.f)[TraitChips(R ? R->GetAllTraits(CId) : C.Traits, 10.f)]
		+ SHorizontalBox::Slot().FillWidth(1.f)[SNullWidget::NullWidget]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[bRecruit ? SNullWidget::NullWidget : GearSlots(C, 10.f)];

	TSharedRef<SVerticalBox> Content = SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()[Head]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 12.f, 0.f, 0.f)[XP]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 12.f, 0.f, 0.f)[Kit];

	if (bRecruit)
	{
		const int32 Cost = C.HireCost;
		auto RR = [this]() { return Run(); };
		Content->AddSlot().AutoHeight().Padding(0.f, 14.f, 0.f, 0.f)[HLine()];
		Content->AddSlot().AutoHeight().Padding(0.f, 12.f, 0.f, 0.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()[Label(TEXT("Hire"), TextFaint, 7.5f)]
				+ SVerticalBox::Slot().AutoHeight()[TxtA(FText::FromString(Money(Cost)), Mono(14.f), TAttribute<FSlateColor>::CreateLambda([RR, Cost]() { const UDCRunSubsystem* X = RR(); return FSlateColor((X && X->GetCash() < Cost) ? Bad : Text); }))]
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				TextButton(TEXT("user-plus"), TEXT("Hire"), [this, CId]()
				{
					UDCRunSubsystem* X = Run();
					FString Err;
					if (X && X->Hire(CId, &Err)) { Sfx(DCSfx::Confirm); }
					else { Sfx(DCSfx::Error); Toast(Err.IsEmpty() ? TEXT("Can't hire") : Err, TEXT("user-x"), true); }
				}, EBtn::Primary, TAttribute<bool>::CreateLambda([RR, Cost]() { const UDCRunSubsystem* X = RR(); return X && X->GetCash() >= Cost && X->GetRoster().Num() < X->GetRosterCap(); }),
				Tip(TEXT("Hire"), TEXT("Needs cash and a free bunk"), TEXT("user-plus")))
			]
		];
		TSharedRef<SWidget> W = CardBox(Content, FMargin(16.f));
		W->SetToolTip(TipWidget(CrewTip(C)));
		return W;
	}

	const bool bSel = CrewSel == CId;
	TSharedRef<SWidget> B = Button(Content, [this, CId]() { CrewSel = CId; DismissArmed = INDEX_NONE; Rebuild(); }, bSel ? EBtn::CardSelected : EBtn::Card, true, nullptr, FMargin(14.f));
	B->SetToolTip(TipWidget(CrewTip(C)));
	return B;
}

TSharedRef<SWidget> SDCRoot::CrewDetail(const FDCCriminal& C)
{
	UDCRunSubsystem* R = Run();
	const int32 CId = C.Id;
	const FLinearColor QC = DC::QualityColor(C.Quality);
	const float Eff = R ? R->GetEffectiveLevel(CId) : (float)C.Level;

	TSharedRef<SVerticalBox> Ab = SNew(SVerticalBox);
	for (FName Id : C.Abilities)
	{
		const FDCAbility* A_ = R ? R->FindAbility(Id) : nullptr;
		Ab->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 6.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 10.f, 0.f)[IconTile(A_ ? A_->Icon : FName(TEXT("zap")), 28.f, DC::TypeColor(C.Type))]
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[Txt(A_ ? A_->DisplayName : Id.ToString(), Font(10.5f, EW::Medium), Text)]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[IconText(DC::ThreatIcon(A_ ? A_->Counters : EDCThreat::MAX), A_ ? DC::ThreatName(A_->Counters) : TEXT("?"), Font(9.5f), TextDim, TextDim, 11.f, 5.f)]
		];
	}
	TSharedRef<SVerticalBox> Tr = SNew(SVerticalBox);
	for (FName Id : (R ? R->GetAllTraits(CId) : C.Traits))
	{
		const FDCTrait* T = R ? R->FindTrait(Id) : nullptr;
		const FLinearColor Col = (T && T->bNegative) ? Bad : Hex(0xC4B5FD);
		Tr->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 6.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 10.f, 0.f)[IconTile(T ? T->Icon : FName(TEXT("sparkle")), 28.f, Col)]
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()[Txt(T ? T->DisplayName : Id.ToString(), Font(10.5f, EW::Medium), Text)]
				+ SVerticalBox::Slot().AutoHeight()[Txt(T ? T->Description : FString(), Font(9.f), TextDim)]
			]
		];
	}
	const bool bArmed = DismissArmed == CId;
	TSharedRef<SWidget> Actions = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(1.f).Padding(0.f, 0.f, 6.f, 0.f)
		[TextButton(TEXT("wrench"), TEXT("Gear"), [this, CId]() { GearSel = CId; Open(EDCPanel::Workbench); }, EBtn::Secondary)]
		+ SHorizontalBox::Slot().FillWidth(1.f).Padding(6.f, 0.f, 0.f, 0.f)
		[TextButton(bArmed ? TEXT("triangle-alert") : TEXT("user-minus"), bArmed ? TEXT("Confirm") : TEXT("Dismiss"), [this, CId, bArmed]()
			{
				if (!bArmed) { DismissArmed = CId; Sfx(DCSfx::Click); Rebuild(); return; }
				if (UDCRunSubsystem* X = Run()) { X->Dismiss(CId); }
				DismissArmed = INDEX_NONE; CrewSel = INDEX_NONE; Sfx(DCSfx::Confirm);
			}, EBtn::Danger, C.IsAvailable(), Tip(TEXT("Dismiss"), TEXT("Let them go. Saves their wages. Only when available."), TEXT("user-minus"), Bad))];

	const float XPFrac = C.XPToNext > 0 ? (float)C.XP / (float)C.XPToNext : 0.f;
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[Avatar(C, 84.f)]
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 12.f, 0.f, 0.f)[Txt(C.GetShortName(), Font(16.f, EW::SemiBold), Text)]
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 2.f, 0.f, 0.f)[Txt(FString::Printf(TEXT("%s %s"), *C.FirstName, *C.LastName), Font(10.f), TextDim)]
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 8.f, 0.f, 0.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Pill(IconText(DC::TypeIcon(C.Type), FString(DC::TypeName(C.Type)).ToUpper(), Caps(7.5f), DC::TypeColor(C.Type), DC::TypeColor(C.Type), 10.f, 5.f), A(DC::TypeColor(C.Type), 0.10f), FMargin(8.f, 2.f))]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(6.f, 0.f, 0.f, 0.f)[Pill(Txt(FString(DC::QualityName(C.Quality)).ToUpper(), Caps(7.5f), QC), A(QC, 0.10f), FMargin(8.f, 2.f))]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(6.f, 0.f, 0.f, 0.f)[StatusChip(C)]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 18.f, 0.f, 0.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()[Label(TEXT("Level"), TextFaint, 7.5f)]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom)[Txt(FString::FromInt(C.Level), Mono(20.f), Text)]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom).Padding(6.f, 0.f, 0.f, 4.f)
					[!FMath::IsNearlyEqual(Eff, (float)C.Level) ? Txt(FString::Printf(TEXT("eff %.1f"), Eff), Mono(9.f), Accent) : SNullWidget::NullWidget]
				]
			]
			+ SHorizontalBox::Slot().FillWidth(1.f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()[Label(TEXT("Loyalty"), TextFaint, 7.5f)]
				+ SVerticalBox::Slot().AutoHeight()[IconText(TEXT("heart"), FString::FromInt(C.Loyalty), Mono(20.f), LoyaltyColor(C.Loyalty), Text, 14.f, 6.f)]
			]
			+ SHorizontalBox::Slot().FillWidth(1.f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()[Label(TEXT("Jobs"), TextFaint, 7.5f)]
				+ SVerticalBox::Slot().AutoHeight()[Txt(FString::FromInt(C.MissionsRun), Mono(20.f), Text)]
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 10.f, 0.f, 0.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[Bar(XPFrac, FSlateColor(Info), 4.f)]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(10.f, 0.f, 0.f, 0.f)[Txt(FString::Printf(TEXT("%d/%d"), C.XP, C.XPToNext), Mono(9.f), TextDim)]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 20.f, 0.f, 8.f)[Label(TEXT("Abilities"))]
		+ SVerticalBox::Slot().AutoHeight()[Ab]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 12.f, 0.f, 8.f)[Label(TEXT("Traits"))]
		+ SVerticalBox::Slot().AutoHeight()[Tr]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 12.f, 0.f, 8.f)[Label(TEXT("Gear"))]
		+ SVerticalBox::Slot().AutoHeight()[GearSlots(C, 13.f)]
		+ SVerticalBox::Slot().FillHeight(1.f)[SNullWidget::NullWidget]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 18.f, 0.f, 0.f)[Actions];
}

// ─────────────────────────────────────────────────────────────────────────────
// Crew panel
// ─────────────────────────────────────────────────────────────────────────────

TSharedRef<SWidget> SDCRoot::BuildCrew()
{
	UDCRunSubsystem* R = Run();
	if (!R) { return SNullWidget::NullWidget; }
	const TArray<FDCCriminal>& Roster = R->GetRoster();
	if (!R->FindCriminal(CrewSel) && Roster.Num()) { CrewSel = Roster[0].Id; }

	TSharedRef<SUniformGridPanel> Grid = SNew(SUniformGridPanel).SlotPadding(FMargin(5.f));
	for (int32 i = 0; i < Roster.Num(); ++i) { Grid->AddSlot(i % 2, i / 2)[CrewCard(Roster[i], false)]; }
	if (Roster.Num() == 1) { Grid->AddSlot(1, 0)[SNew(SSpacer)]; }

	TSharedRef<SVerticalBox> Left = SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(-5.f, -5.f, -5.f, 0.f)[Grid];
	if (R->GetFallen().Num())
	{
		TSharedRef<SHorizontalBox> Fallen = SNew(SHorizontalBox);
		for (const FDCCriminal& C : R->GetFallen())
		{
			TSharedRef<SWidget> W = Avatar(C, 30.f, true);
			TSharedRef<SBox> B = SNew(SBox)[W]; B->SetToolTip(Tip(C.GetDisplayName(), TEXT("Arrested"), TEXT("siren"), Bad));
			Fallen->AddSlot().AutoWidth().Padding(0.f, 0.f, 6.f, 0.f)[B];
		}
		Left->AddSlot().AutoHeight().Padding(0.f, 18.f, 0.f, 8.f)[IconText(TEXT("siren"), TEXT("ARRESTED"), Caps(8.f), Bad, TextFaint, 10.f, 6.f)];
		Left->AddSlot().AutoHeight()[Fallen];
	}

	const FDCCriminal* Sel = R->FindCriminal(CrewSel);
	TSharedRef<SWidget> Body = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(1.f)[Scroll(Left)]
		+ SHorizontalBox::Slot().AutoWidth().Padding(18.f, 0.f)[VLine()]
		+ SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(320.f)[Sel ? CrewDetail(*Sel) : SNullWidget::NullWidget]];

	int32 Avail = 0; for (const FDCCriminal& C : Roster) { if (C.IsAvailable()) { ++Avail; } }
	TSharedRef<SWidget> HR = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[IconText(TEXT("circle-check"), FString::Printf(TEXT("%d"), Avail), Mono(11.f), Good, Text, 12.f, 5.f)]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(14.f, 0.f, 0.f, 0.f)[IconText(TEXT("bed-double"), FString::Printf(TEXT("%d/%d"), Roster.Num(), R->GetRosterCap()), Mono(11.f), TextDim, Text, 12.f, 5.f)]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(14.f, 0.f, 0.f, 0.f)[IconText(TEXT("wallet"), FString::Printf(TEXT("%s/day"), *Money(R->GetUpkeepDue())), Mono(11.f), TextDim, Text, 12.f, 5.f)];
	return Shell(TEXT("users"), TEXT("Crew"), Body, HR);
}

// ─────────────────────────────────────────────────────────────────────────────
// Workbench
// ─────────────────────────────────────────────────────────────────────────────

TSharedRef<SWidget> SDCRoot::GearCard(const FDCGearItem& G, bool bStash)
{
	const FName Id = G.Id;
	const int32 Target = GearSel;
	auto RR = [this]() { return Run(); };
	TSharedRef<SHorizontalBox> Stats = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[SNew(SBox).ToolTip(Tip(FString::Printf(TEXT("%s slot"), DC::SlotName(G.Slot)), FString(), DC::SlotIcon(G.Slot)))[Icon(DC::SlotIcon(G.Slot), 10.f, TextFaint)]];
	if (G.Power > 0.f)
	{
		Stats->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(8.f, 0.f, 0.f, 0.f)
		[SNew(SBox).ToolTip(Tip(TEXT("Power"), TEXT("Adds to effective level"), TEXT("chevrons-up"), Info))[IconText(TEXT("chevrons-up"), FString::Printf(TEXT("+%.1f"), G.Power), Mono(9.f), Info, Info, 10.f, 3.f)]];
	}
	if (G.Counters != EDCThreat::MAX)
	{
		Stats->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(8.f, 0.f, 0.f, 0.f)
		[SNew(SBox).ToolTip(Tip(FString::Printf(TEXT("Counters %s"), DC::ThreatName(G.Counters)), FString(), DC::ThreatIcon(G.Counters), Good))
			[IconText(TEXT("shield-check"), FString(), Mono(9.f), Good, Good, 10.f, 0.f)]];
		Stats->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(2.f, 0.f, 0.f, 0.f)[Icon(DC::ThreatIcon(G.Counters), 10.f, Good)];
	}
	if (!G.TraitId.IsNone())
	{
		const UDCRunSubsystem* R = Run();
		const FDCTrait* Tr = R ? R->FindTrait(G.TraitId) : nullptr;
		Stats->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(8.f, 0.f, 0.f, 0.f)
		[SNew(SBox).ToolTip(Tip(Tr ? Tr->DisplayName : G.TraitId.ToString(), Tr ? Tr->Description : FString(), Tr ? Tr->Icon : FName(TEXT("sparkle")), Hex(0xC4B5FD)))
			[Icon(Tr ? Tr->Icon : FName(TEXT("sparkle")), 10.f, (Tr && Tr->bNegative) ? Bad : Hex(0xC4B5FD))]];
	}

	TSharedRef<SWidget> Action = SNullWidget::NullWidget;
	if (bStash)
	{
		Action = SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth()[IconButton(TEXT("arrow-up-from-line"), [this, Id, Target]()
				{
					UDCRunSubsystem* X = Run(); FString Err;
					if (X && X->EquipFromStash(Id, Target, &Err)) { Sfx(DCSfx::SlotFill); } else { Sfx(DCSfx::Error); Toast(Err.IsEmpty() ? TEXT("Can't equip") : Err, TEXT("circle-x"), true); }
				}, Tip(TEXT("Equip"), TEXT("On the selected crew member")), EBtn::Secondary, 11.f, Target != INDEX_NONE)]
			+ SHorizontalBox::Slot().AutoWidth().Padding(4.f, 0.f, 0.f, 0.f)[IconButton(TEXT("hand-coins"), [this, Id]()
				{
					UDCRunSubsystem* X = Run();
					if (X && X->SellFromStash(Id)) { Sfx(DCSfx::Cash); } else { Sfx(DCSfx::Error); }
				}, Tip(TEXT("Sell"), FString::Printf(TEXT("+%s"), *Money(G.Price / 2))), EBtn::Ghost, 11.f)];
	}
	else
	{
		const bool bAvail = G.bAvailable;
		Action = SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 8.f, 0.f)
			[TxtA(FText::FromString(Money(G.Price)), Mono(11.f), TAttribute<FSlateColor>::CreateLambda([RR, Price = G.Price]() { const UDCRunSubsystem* X = RR(); return FSlateColor((X && X->GetCash() < Price) ? Bad : Text); }))]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[IconButton(bAvail ? FName(TEXT("shopping-cart")) : FName(TEXT("lock")), [this, Id, Target]()
				{
					UDCRunSubsystem* X = Run(); FString Err;
					if (X && X->BuyGear(Id, Target, &Err)) { Sfx(DCSfx::Confirm); } else { Sfx(DCSfx::Error); Toast(Err.IsEmpty() ? TEXT("Can't buy") : Err, TEXT("circle-x"), true); }
				}, bAvail ? Tip(TEXT("Buy & equip"), TEXT("On the selected crew member")) : Tip(TEXT("Unavailable"), G.UnavailableReason, TEXT("lock"), Bad),
				bAvail ? EBtn::Primary : EBtn::Secondary, 11.f, bAvail && Target != INDEX_NONE)];
	}

	TSharedRef<SWidget> Content = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 10.f, 0.f)[IconTile(G.Icon, 36.f, bStash ? Accent : TierColor(G.Tier))]
		+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(FText::FromString(G.DisplayName)).Font(Font(10.5f, EW::SemiBold)).ColorAndOpacity(G.bAvailable || bStash ? Text : TextDim).OverflowPolicy(ETextOverflowPolicy::Ellipsis)]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f, 0.f, 0.f)[Stats]
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8.f, 0.f, 0.f, 0.f)[Action];
	TSharedRef<SWidget> W = CardBox(Content, FMargin(10.f, 8.f));
	W->SetToolTip(Tip(G.DisplayName, G.Description + (!G.bAvailable && !bStash && !G.UnavailableReason.IsEmpty() ? FString::Printf(TEXT("\nNeeds: %s"), *G.UnavailableReason) : FString()), G.Icon));
	return W;
}

TSharedRef<SWidget> SDCRoot::BuildWorkbench()
{
	UDCRunSubsystem* R = Run();
	if (!R) { return SNullWidget::NullWidget; }
	const TArray<FDCCriminal>& Roster = R->GetRoster();
	if (!R->FindCriminal(GearSel) && Roster.Num()) { GearSel = Roster[0].Id; }
	const FDCCriminal* Sel = R->FindCriminal(GearSel);

	// Crew strip
	TSharedRef<SHorizontalBox> Strip = SNew(SHorizontalBox);
	for (const FDCCriminal& C : Roster)
	{
		const int32 CId = C.Id;
		const bool bSel = CId == GearSel;
		TSharedRef<SWidget> B = Button(Avatar(C, 40.f, !C.IsAvailable() && !bSel), [this, CId]() { GearSel = CId; Rebuild(); }, bSel ? EBtn::CardSelected : EBtn::Ghost, true,
			Tip(C.GetShortName(), FString::Printf(TEXT("%s · level %d"), DC::TypeName(C.Type), C.Level), DC::TypeIcon(C.Type), DC::TypeColor(C.Type)), FMargin(5.f));
		Strip->AddSlot().AutoWidth().Padding(0.f, 0.f, 6.f, 0.f)[B];
	}

	// Loadout
	TSharedRef<SVerticalBox> Loadout = SNew(SVerticalBox);
	const int32 NSlots = FMath::Clamp(R->GetGearSlotCount(), 0, (int32)EDCGearSlot::MAX);
	if (Sel)
	{
		for (int32 i = 0; i < (int32)EDCGearSlot::MAX; ++i)
		{
			const EDCGearSlot S = (EDCGearSlot)i;
			const bool bLocked = i >= NSlots;
			const FName* Item = Sel->Equipped.Find(S);
			const bool bHas = Item && !Item->IsNone();
			const FDCGearItem* G = bHas ? R->FindGear(*Item) : nullptr;
			const int32 CId = Sel->Id;
			TSharedRef<SWidget> Row = SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 10.f, 0.f)
				[bHas ? IconTile(G ? G->Icon : FName(TEXT("package")), 36.f, Accent) : IconTile(bLocked ? FName(TEXT("lock")) : DC::SlotIcon(S), 36.f, TextFaint, 0.45f, 0.05f)]
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()[Label(DC::SlotName(S), TextFaint, 7.5f)]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f, 0.f, 0.f)
					[Txt(bLocked ? TEXT("Workshop") : (bHas ? (G ? G->DisplayName : Item->ToString()) : TEXT("Empty")), Font(10.5f, bHas ? EW::SemiBold : EW::Regular), bHas ? Text : TextFaint)]
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[bHas ? IconButton(TEXT("arrow-down-to-line"), [this, CId, S]() { if (UDCRunSubsystem* X = Run()) { X->Unequip(CId, S); Sfx(DCSfx::Click); } }, Tip(TEXT("Unequip"), TEXT("Back to the stash")), EBtn::Ghost, 11.f) : SNullWidget::NullWidget];
			Loadout->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 6.f)[CardBox(Row, FMargin(10.f, 8.f), bHas ? A(Accent, 0.3f) : Line)];
		}
	}

	// Stash
	TSharedRef<SVerticalBox> Stash = SNew(SVerticalBox);
	for (const FName& Id : R->GetStash())
	{
		const FDCGearItem* G = R->FindGear(Id);
		FDCGearItem Fake;
		if (!G) { Fake.Id = Id; Fake.DisplayName = Id.ToString(); Fake.Icon = TEXT("package"); G = &Fake; }
		Stash->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 6.f)[GearCard(*G, true)];
	}
	if (R->GetStash().Num() == 0) { Stash->AddSlot().AutoHeight()[IconText(TEXT("package-open"), TEXT("Stash is empty"), Font(10.f), TextFaint, TextFaint, 12.f, 6.f)]; }

	// Shop
	TArray<FDCGearItem> Cat = R->GetGearCatalogue();
	Cat.StableSort([](const FDCGearItem& X, const FDCGearItem& Y) { return X.Slot != Y.Slot ? (int32)X.Slot < (int32)Y.Slot : X.Price < Y.Price; });
	TSharedRef<SUniformGridPanel> Shop = SNew(SUniformGridPanel).SlotPadding(FMargin(3.f));
	for (int32 i = 0; i < Cat.Num(); ++i) { Shop->AddSlot(i % 2, i / 2)[GearCard(Cat[i], false)]; }
	if (Cat.Num() == 1) { Shop->AddSlot(1, 0)[SNew(SSpacer)]; }

	TSharedRef<SWidget> LeftCol = SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 10.f)
		[
			Sel ? (TSharedRef<SWidget>)(SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Label(TEXT("Loadout"))]
				+ SHorizontalBox::Slot().FillWidth(1.f).HAlign(HAlign_Right).VAlign(VAlign_Center)
				[IconText(TEXT("chevrons-up"), FString::Printf(TEXT("eff. LV %.1f"), R->GetEffectiveLevel(Sel->Id)), Mono(9.f), Info, TextDim, 10.f, 4.f)])
			: Label(TEXT("Loadout"))
		]
		+ SVerticalBox::Slot().AutoHeight()[Loadout]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 16.f, 0.f, 10.f)[Label(TEXT("Stash"))]
		+ SVerticalBox::Slot().FillHeight(1.f)[Scroll(Stash)];

	TSharedRef<SWidget> Body = SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()[Strip]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 14.f, 0.f, 16.f)[HLine()]
		+ SVerticalBox::Slot().FillHeight(1.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(340.f)[LeftCol]]
			+ SHorizontalBox::Slot().AutoWidth().Padding(18.f, 0.f)[VLine()]
			+ SHorizontalBox::Slot().FillWidth(1.f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 10.f)[Label(TEXT("Shop"))]
				+ SVerticalBox::Slot().FillHeight(1.f)[Scroll(Cat.Num() ? (TSharedRef<SWidget>)(SNew(SBox).Padding(FMargin(-3.f, -3.f, -3.f, 0.f))[Shop]) : IconText(TEXT("store"), TEXT("Nothing for sale"), Font(10.f), TextFaint, TextFaint, 12.f, 6.f))]
			]
		];

	TSharedRef<SWidget> HR = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[SNew(SBox).ToolTip(Tip(TEXT("Gear slots"), TEXT("Per crew member. The Workshop adds one."), TEXT("wrench")))[IconText(TEXT("layout-grid"), FString::Printf(TEXT("%d slots"), NSlots), Mono(10.f), TextDim, TextDim, 11.f, 5.f)]];
	return Shell(TEXT("wrench"), TEXT("Workbench"), Body, HR);
}

// ─────────────────────────────────────────────────────────────────────────────
// Recruits
// ─────────────────────────────────────────────────────────────────────────────

TSharedRef<SWidget> SDCRoot::BuildRecruits()
{
	UDCRunSubsystem* R = Run();
	if (!R) { return SNullWidget::NullWidget; }
	auto RR = [this]() { return Run(); };

	TSharedRef<SUniformGridPanel> Grid = SNew(SUniformGridPanel).SlotPadding(FMargin(5.f));
	const TArray<FDCCriminal>& Rec = R->GetRecruits();
	for (int32 i = 0; i < Rec.Num(); ++i) { Grid->AddSlot(i % 2, i / 2)[CrewCard(Rec[i], true)]; }
	if (Rec.Num() == 1) { Grid->AddSlot(1, 0)[SNew(SSpacer)]; }

	auto Fin = [](FName IconName, const FString& Name, TAttribute<FText> V, TAttribute<FSlateColor> Col)
	{
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 10.f, 0.f)[IconTile(IconName, 28.f, TextDim, 0.5f, 0.06f)]
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[Label(Name, TextDim, 8.f)]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[TxtA(V, Mono(12.f), Col)];
	};
	TSharedRef<SWidget> Finances = SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 12.f)[Label(TEXT("Finances"))]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 10.f)[Fin(TEXT("banknote"), TEXT("Cash"), TAttribute<FText>::CreateLambda([RR]() { const UDCRunSubsystem* X = RR(); return FText::FromString(X ? Money(X->GetCash()) : TEXT("")); }), FSlateColor(Accent))]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 10.f)[Fin(TEXT("wallet"), TEXT("Upkeep / day"), TAttribute<FText>::CreateLambda([RR]() { const UDCRunSubsystem* X = RR(); return FText::FromString(X ? Money(-X->GetUpkeepDue()) : TEXT("")); }), FSlateColor(Text))]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 10.f)[Fin(TEXT("sunrise"), TEXT("Payday in"), TAttribute<FText>::CreateLambda([RR]() { const UDCRunSubsystem* X = RR(); return FText::FromString(X ? DC::Clock(X->GetSecondsUntilPayday()) : TEXT("")); }), FSlateColor(Text))]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 10.f)[Fin(TEXT("trophy"), TEXT("Successes"), TAttribute<FText>::CreateLambda([RR]() { const UDCRunSubsystem* X = RR(); return FText::FromString(X ? FString::FromInt(X->GetMissionsSucceeded()) : TEXT("")); }), FSlateColor(Text))]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 10.f)[Fin(TEXT("layers"), TEXT("Tier"), TAttribute<FText>::CreateLambda([RR]() { const UDCRunSubsystem* X = RR(); return FText::FromString(X ? FString::Printf(TEXT("%d/4"), X->GetUnlockedTier()) : TEXT("")); }), FSlateColor(Text))]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 10.f)[Fin(TEXT("bed-double"), TEXT("Bunks"), TAttribute<FText>::CreateLambda([RR]() { const UDCRunSubsystem* X = RR(); return FText::FromString(X ? FString::Printf(TEXT("%d/%d"), X->GetRoster().Num(), X->GetRosterCap()) : TEXT("")); }),
			TAttribute<FSlateColor>::CreateLambda([RR]() { const UDCRunSubsystem* X = RR(); return FSlateColor((X && X->GetRoster().Num() >= X->GetRosterCap()) ? Warn : Text); }))];

	const int32 RefreshCost = R->GetRecruitRefreshCost();
	TSharedRef<SWidget> Refresh = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 10.f, 0.f)
		[
			SNew(SBox).ToolTip(Tip(TEXT("New faces"), TEXT("The recruit list refreshes on its own. Everyone listed now moves on when it does."), TEXT("timer")))
			[IconTextA(TEXT("timer"), TAttribute<FText>::CreateLambda([RR]() { const UDCRunSubsystem* X = RR(); return FText::FromString(X ? DC::Clock(X->GetRecruitRefreshIn()) : TEXT("")); }), Mono(10.f),
				TAttribute<FSlateColor>::CreateLambda([this]() { return FSlateColor(UrgencyColor(RecruitExpiry(), TextDim)); }),
				TAttribute<FSlateColor>::CreateLambda([this]() { return FSlateColor(UrgencyColor(RecruitExpiry(), TextDim)); }), 11.f, 5.f)]
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[TextButton(TEXT("refresh-cw"), Money(RefreshCost), [this]()
			{
				UDCRunSubsystem* X = Run();
				if (X && X->RefreshRecruits()) { Sfx(DCSfx::Confirm); } else { Sfx(DCSfx::Error); }
			}, EBtn::Secondary, TAttribute<bool>::CreateLambda([RR, RefreshCost]() { const UDCRunSubsystem* X = RR(); return X && X->GetCash() >= RefreshCost; }),
			Tip(TEXT("Refresh now"), TEXT("Pay to see new recruits"), TEXT("refresh-cw")), 10.f)];

	TSharedRef<SWidget> Left = Rec.Num()
		? Scroll(SNew(SBox).Padding(FMargin(-5.f, -5.f, -5.f, 0.f))[Grid])
		: (TSharedRef<SWidget>)(SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[Icon(TEXT("user-search"), 30.f, TextFaint)]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 10.f, 0.f, 0.f)[Txt(TEXT("Nobody's looking for work"), Font(11.f), TextDim)]
			]);
	// Escalating notice above the cards while the pool is about to roll over.
	auto Urg = [this]() { return Prefs().bExpiryWarnings ? Urgency(RecruitExpiry()) : EUrgency::Calm; };
	TSharedRef<SWidget> Notice = SNew(SBox)
		.Visibility_Lambda([Urg]() { return Urg() != EUrgency::Calm ? EVisibility::Visible : EVisibility::Collapsed; })
		.Padding(FMargin(0.f, 0.f, 8.f, 10.f))
		[
			SNew(SBorder)
			.BorderImage_Lambda([Urg]() { const FLinearColor C = Urg() == EUrgency::Critical ? Bad : Accent; return Box(A(C, 0.10f), 9.f, A(C, Urg() == EUrgency::Critical ? 0.3f + 0.4f * Pulse() : 0.35f), 1.f); })
			.Padding(FMargin(12.f, 8.f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 10.f, 0.f)
				[IconA(TEXT("hourglass"), 13.f, TAttribute<FSlateColor>::CreateLambda([Urg]() { return FSlateColor(Urg() == EUrgency::Critical ? Bad : Accent); }))]
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
				[TxtA(TAttribute<FText>::CreateLambda([Urg]() { return FText::FromString(Urg() == EUrgency::Critical ? TEXT("Expiring · these recruits move on any moment") : TEXT("These recruits move on soon")); }), Font(10.5f, EW::Medium), FSlateColor(Text))]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[TxtA(TAttribute<FText>::CreateLambda([this]() { return FText::FromString(DC::Clock(RecruitExpiry())); }), Mono(12.f), TAttribute<FSlateColor>::CreateLambda([this]() { return FSlateColor(UrgencyColor(RecruitExpiry(), Text)); }))]
			]
		];
	TSharedRef<SWidget> Body = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(1.f)[SNew(SVerticalBox) + SVerticalBox::Slot().AutoHeight()[Notice] + SVerticalBox::Slot().FillHeight(1.f)[Left]]
		+ SHorizontalBox::Slot().AutoWidth().Padding(18.f, 0.f)[VLine()]
		+ SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(260.f)[Finances]];
	return Shell(TEXT("user-plus"), TEXT("Recruits"), Body, Refresh);
}

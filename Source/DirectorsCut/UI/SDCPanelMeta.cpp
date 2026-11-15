// Director's Cut — build table, room panels, perk draft, title, pause, run end (Agent C).
#include "UI/SDCRootInternal.h"
#include "Core/DCDevTools.h"
#include "Kismet/KismetSystemLibrary.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "DirectorsCut.h"
#include "Widgets/SLeafWidget.h"
#include "Rendering/DrawElements.h"

/** Title-screen scrim: solid dark on the left 40%, easing out to clear on the right. */
class SDCTitleShade : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SDCTitleShade) {}
	SLATE_END_ARGS()
	void Construct(const FArguments&) { SetVisibility(EVisibility::HitTestInvisible); }
	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(8.f, 8.f); }
	virtual int32 OnPaint(const FPaintArgs&, const FGeometry& Geo, const FSlateRect&, FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle& Style, bool) const override
	{
		const float W = Geo.GetLocalSize().X;
		const float Tint = Style.GetColorAndOpacityTint().A;
		TArray<FSlateGradientStop> Stops;
		const int32 N = 32;
		for (int32 i = 0; i <= N; ++i)
		{
			const float X = (float)i / N;
			const float Alpha = X < 0.4f ? 0.84f : 0.84f * FMath::Pow(1.f - (X - 0.4f) / 0.6f, 1.6f);
			Stops.Emplace(FVector2f(X * W, 0.f), FLinearColor(0.01f, 0.012f, 0.016f, Alpha * Tint));
		}
		FSlateDrawElement::MakeGradient(Out, Layer, Geo.ToPaintGeometry(), MoveTemp(Stops), Orient_Vertical);
		return Layer;
	}
};

// ─────────────────────────────────────────────────────────────────────────────
// Build
// ─────────────────────────────────────────────────────────────────────────────

TSharedRef<SWidget> SDCRoot::RoomTile(const FDCRoom& Room)
{
	auto RR = [this]() { return Run(); };
	const EDCRoomType Type = Room.Type;
	const bool bBuilt = Room.Level >= 1;
	const bool bBuilding = Room.State == EDCRoomState::Building;
	const FLinearColor Col = bBuilding ? Warn : (bBuilt ? Accent : TextDim);
	const FString Name = Room.DisplayName.IsEmpty() ? FString(DC::RoomName(Type)) : Room.DisplayName;
	const FName IconName = Room.Icon.IsNone() ? DC::RoomIcon(Type) : Room.Icon;

	// Level effects: current (dim) + next (bright)
	TSharedRef<SVerticalBox> Effects = SNew(SVerticalBox);
	for (int32 i = 0; i < Room.LevelEffects.Num() && i < 3; ++i)
	{
		const bool bHave = i < Room.Level;
		const bool bNext = i == Room.Level;
		Effects->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 3.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 6.f, 0.f)
			[Icon(bHave ? TEXT("check") : (bNext ? TEXT("chevron-right") : TEXT("minus")), 9.f, bHave ? Good : (bNext ? Accent : TextFaint))]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 6.f, 0.f)
			[SNew(SBox).MinDesiredWidth(24.f)[Txt(FString::Printf(TEXT("LV %d"), i + 1), Caps(7.f), bNext ? Accent : TextFaint)]]
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
			[SNew(STextBlock).Text(FText::FromString(Room.LevelEffects[i])).Font(Font(9.f, bNext ? EW::Medium : EW::Regular)).ColorAndOpacity(bHave ? TextDim : (bNext ? Text : TextFaint)).OverflowPolicy(ETextOverflowPolicy::Ellipsis)]
		];
	}

	TSharedRef<SWidget> Footer = SNullWidget::NullWidget;
	if (bBuilding)
	{
		auto Progress = [RR, Type]()
		{
			const UDCRunSubsystem* X = RR(); if (!X) { return 0.f; }
			const FDCRoom Q = X->GetRoom(Type); const double Len = Q.BuildEndsAt - Q.BuildStartedAt;
			return Len > 0.0 ? (float)FMath::Clamp((X->GetRunTime() - Q.BuildStartedAt) / Len, 0.0, 1.0) : 0.f;
		};
		Footer = SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[IconText(TEXT("hammer"), TEXT("BUILDING"), Caps(7.5f), Warn, Warn, 10.f, 5.f)]
				+ SHorizontalBox::Slot().FillWidth(1.f).HAlign(HAlign_Right).VAlign(VAlign_Center)
				[TxtA(TAttribute<FText>::CreateLambda([RR, Type]() { const UDCRunSubsystem* X = RR(); return FText::FromString(X ? DC::Clock(X->GetRoom(Type).BuildEndsAt - X->GetRunTime()) : TEXT("")); }), Mono(11.f), FSlateColor(Text))]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f, 0.f, 0.f)[Bar(TAttribute<float>::CreateLambda(Progress), FSlateColor(Warn), 4.f)];
	}
	else if (Room.bMaxLevel)
	{
		Footer = IconText(TEXT("badge-check"), TEXT("MAX LEVEL"), Caps(7.5f), Good, Good, 11.f, 5.f);
	}
	else
	{
		const EDCPlanBlock Block = Room.Block;
		FString BlockTip;
		if (Block == EDCPlanBlock::NoCash) { BlockTip = TEXT("Not enough cash"); }
		else if (Block == EDCPlanBlock::Locked) { BlockTip = Room.RequiredSuccesses > 0 ? FString::Printf(TEXT("Needs %d successful jobs"), Room.RequiredSuccesses) : TEXT("Locked"); }
		else if (!Room.bCanAct)
		{
			const UDCRunSubsystem* X = Run();
			const int32 Slots = X ? X->GetBuildSlots() : 1;
			BlockTip = Slots > 1 ? FString::Printf(TEXT("%d extensions at a time"), Slots) : TEXT("One extension at a time");
		}
		TSharedRef<SHorizontalBox> Req = SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[TxtA(FText::FromString(Money(Room.NextCost)), Mono(12.f), TAttribute<FSlateColor>::CreateLambda([RR, C = Room.NextCost]() { const UDCRunSubsystem* X = RR(); return FSlateColor((X && X->GetCash() < C) ? Bad : Text); }))]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(10.f, 0.f, 0.f, 0.f)[IconText(TEXT("clock"), DC::Clock(Room.NextBuildTime), Mono(9.5f), TextFaint, TextDim, 10.f, 4.f)];
		if (Room.RequiredSuccesses > 0)
		{
			const bool bOk = Run() && Run()->GetMissionsSucceeded() >= Room.RequiredSuccesses;
			Req->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(10.f, 0.f, 0.f, 0.f)
			[SNew(SBox).ToolTip(Tip(TEXT("Successful jobs needed"), FString::Printf(TEXT("%d"), Room.RequiredSuccesses), TEXT("trophy")))[IconText(TEXT("trophy"), FString::FromInt(Room.RequiredSuccesses), Mono(9.5f), bOk ? TextFaint : Bad, bOk ? TextDim : Bad, 10.f, 4.f)]];
		}
		const bool bCan = Room.bCanAct;
		Footer = SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[Req]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				TextButton(bCan ? (bBuilt ? FName(TEXT("arrow-up")) : FName(TEXT("hammer"))) : (Block == EDCPlanBlock::None ? FName(TEXT("hourglass")) : BlockIcon(Block)), bBuilt ? TEXT("Upgrade") : TEXT("Build"), [this, Type]()
				{
					UDCRunSubsystem* X = Run(); EDCPlanBlock B = EDCPlanBlock::None;
					if (X && !X->IsRoomBuilt(Type) && X->GetRoom(Type).bCanAct && Owner.IsValid())
					{
						// A new room: pick where it goes on the build grid first.
						Sfx(DCSfx::Click);
						Owner->RequestRoomPlacement(Type);
					}
					else if (X && X->BuildRoom(Type, &B)) { Sfx(DCSfx::BuildStart); }
					else { Sfx(DCSfx::Error); Toast(BlockText(B), BlockIcon(B), true); }
				}, bCan ? EBtn::Primary : EBtn::Secondary, bCan, bCan ? nullptr : TSharedPtr<IToolTip>(Tip(TEXT("Can't build yet"), BlockTip, Block == EDCPlanBlock::None ? FName(TEXT("hourglass")) : BlockIcon(Block), Bad)), 10.f)
			];
	}

	TSharedRef<SWidget> Content = SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 12.f, 0.f)[IconTile(IconName, 42.f, Col, 0.48f, bBuilt ? 0.14f : 0.06f)]
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()[Txt(Name, Font(12.f, EW::SemiBold), bBuilt ? Text : TextDim)]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 5.f, 0.f, 0.f)[Pips(Room.Level, 3, Accent, 5.f)]
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top)
			[Room.UpkeepPerDay > 0 ? (TSharedRef<SWidget>)(SNew(SBox).ToolTip(Tip(TEXT("Upkeep"), TEXT("Per day, paid at dawn"), TEXT("wallet")))[IconText(TEXT("wallet"), Money(Room.UpkeepPerDay), Mono(8.5f), TextFaint, TextFaint, 9.f, 4.f)]) : SNullWidget::NullWidget]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 12.f, 0.f, 0.f)
		[SNew(SBox).ToolTip(Tip(TEXT("Room levels"), TEXT("Each line is the room's total bonus at that level. Upgrading replaces the line above."), IconName))[Effects]]
		+ SVerticalBox::Slot().FillHeight(1.f)[SNullWidget::NullWidget]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 10.f, 0.f, 10.f)[HLine()]
		+ SVerticalBox::Slot().AutoHeight()[SNew(SBox).MinDesiredHeight(32.f).VAlign(VAlign_Center)[Footer]];

	TSharedRef<SWidget> W = CardBox(SNew(SBox).MinDesiredHeight(176.f)[Content], FMargin(14.f), bBuilding ? A(Warn, 0.4f) : (bBuilt ? A(Accent, 0.22f) : Line));
	W->SetToolTip(Tip(Name, Room.Description, IconName, Col));
	return W;
}

TSharedRef<SWidget> SDCRoot::BuildBuild()
{
	UDCRunSubsystem* R = Run();
	if (!R) { return SNullWidget::NullWidget; }
	const TArray<FDCRoom> Rooms = R->GetRooms();
	TSharedRef<SUniformGridPanel> Grid = SNew(SUniformGridPanel).SlotPadding(FMargin(5.f));
	for (int32 i = 0; i < Rooms.Num(); ++i) { Grid->AddSlot(i % 3, i / 3)[RoomTile(Rooms[i])]; }

	// Wraps onto new rows so a long run's perks stay inside the panel.
	TSharedRef<SWrapBox> Perks = SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(6.f, 6.f));
	for (const FDCPerk& P : R->GetPerksTaken())
	{
		TSharedRef<SWidget> W = IconTile(P.Icon, 34.f, Accent);
		W->SetToolTip(Tip(P.DisplayName, P.Description, P.Icon));
		Perks->AddSlot()[W];
	}
	if (R->GetPerksTaken().Num() == 0) { Perks->AddSlot().VAlign(VAlign_Center)[Txt(TEXT("Every 3 successful jobs, pick a perk."), Font(9.5f), TextFaint)]; }
	TSharedRef<SWidget> PickPerk = R->HasPendingPerkOffer()
		? TextButton(TEXT("sparkles"), TEXT("Pick a perk"), [this]() { Open(EDCPanel::PerkDraft); }, EBtn::Primary)
		: SNullWidget::NullWidget;

	TSharedRef<SWidget> Body = SNew(SVerticalBox)
		+ SVerticalBox::Slot().FillHeight(1.f)[Scroll(SNew(SBox).Padding(FMargin(-5.f, -5.f, -5.f, 0.f))[Grid])]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 14.f, 0.f, 10.f)[HLine()]
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 14.f, 0.f)[Label(TEXT("Perks"))]
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[Perks]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(R->HasPendingPerkOffer() ? 10.f : 0.f, 0.f, 0.f, 0.f)[PickPerk]
		];

	auto RR = [this]() { return Run(); };
	TSharedRef<SWidget> HR = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[IconTextA(TEXT("trophy"), TAttribute<FText>::CreateLambda([RR]() { const UDCRunSubsystem* X = RR(); return FText::FromString(X ? FString::FromInt(X->GetMissionsSucceeded()) : TEXT("")); }), Mono(11.f), FSlateColor(TextDim), FSlateColor(Text), 12.f, 5.f)]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(14.f, 0.f, 0.f, 0.f)
		[IconTextA(TEXT("banknote"), TAttribute<FText>::CreateLambda([RR]() { const UDCRunSubsystem* X = RR(); return FText::FromString(X ? Money(X->GetCash()) : TEXT("")); }), Mono(11.f), FSlateColor(Accent), FSlateColor(Text), 12.f, 5.f)];
	return Shell(TEXT("hammer"), TEXT("Expand the house"), Body, HR);
}

// ─────────────────────────────────────────────────────────────────────────────
// Room panels
// ─────────────────────────────────────────────────────────────────────────────

TSharedRef<SWidget> SDCRoot::RoomLocked(EDCRoomType Room)
{
	return SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(40.f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[IconTile(TEXT("lock"), 56.f, TextDim, 0.45f, 0.06f)]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 14.f, 0.f, 4.f)[Txt(FString::Printf(TEXT("No %s wing yet"), DC::RoomName(Room)), Font(12.f, EW::SemiBold), Text)]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 0.f, 0.f, 16.f)[Txt(TEXT("Add this room to the house from the Expand menu."), Font(10.f), TextDim)]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[TextButton(TEXT("hammer"), TEXT("Build"), [this]() { Open(EDCPanel::Build); }, EBtn::Primary)]
		];
}

TSharedRef<SWidget> SDCRoot::BuildRoomPanel(EDCPanel P)
{
	UDCRunSubsystem* R = Run();
	if (!R) { return SNullWidget::NullWidget; }
	auto RR = [this]() { return Run(); };
	EDCRoomType Type = EDCRoomType::OpsRoom;
	switch (P)
	{
	case EDCPanel::Clinic:     Type = EDCRoomType::Clinic; break;
	case EDCPanel::Lawyer:     Type = EDCRoomType::LawyerOffice; break;
	case EDCPanel::ServerRoom: Type = EDCRoomType::ServerRoom; break;
	default: break;
	}
	const FDCRoom Room = R->GetRoom(Type);
	const FString Name = Room.DisplayName.IsEmpty() ? FString(DC::RoomName(Type)) : Room.DisplayName;
	const FName IconName = Room.Icon.IsNone() ? DC::RoomIcon(Type) : Room.Icon;
	if (Room.Level < 1) { return Shell(IconName, Name, RoomLocked(Type)); }

	TSharedRef<SVerticalBox> Body = SNew(SVerticalBox);
	// Room effects summary
	TSharedRef<SVerticalBox> Eff = SNew(SVerticalBox);
	// Each level's line is the room's total bonus, so only the current level applies.
	if (Room.LevelEffects.IsValidIndex(Room.Level - 1))
	{
		Eff->AddSlot().AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 8.f, 0.f)[Txt(FString::Printf(TEXT("LV %d"), Room.Level), Caps(7.5f), Accent)]
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[IconText(TEXT("check"), Room.LevelEffects[Room.Level - 1], Font(10.f), Good, TextDim, 10.f, 6.f)]
		];
	}
	Body->AddSlot().AutoHeight()[CardBox(Eff, FMargin(14.f, 12.f))];
	Body->AddSlot().AutoHeight().Padding(0.f, 18.f, 0.f, 10.f)[SNullWidget::NullWidget];

	auto Row = [](TSharedRef<SWidget> L, TSharedRef<SWidget> Rt)
	{
		return CardBox(SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[L]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Rt], FMargin(12.f, 9.f));
	};
	auto Person = [](const FDCCriminal& C, bool bDim) -> TSharedRef<SWidget>
	{
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 10.f, 0.f)[Avatar(C, 34.f, bDim)]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()[Txt(C.GetShortName(), Font(11.f, EW::SemiBold), Text)]
				+ SVerticalBox::Slot().AutoHeight()[Txt(FString::Printf(TEXT("%s · LV %d"), DC::TypeName(C.Type), C.Level), Font(9.f), TextDim)]
			];
	};

	if (P == EDCPanel::Clinic)
	{
		Body->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 10.f)[Label(TEXT("Injured"))];
		int32 N = 0;
		for (const FDCCriminal& C : R->GetRoster())
		{
			if (C.Status != EDCCriminalStatus::Injured) { continue; }
			++N;
			const int32 CId = C.Id;
			const int32 Cost = R->GetPatchUpCost(CId);
			TSharedRef<SWidget> Right = SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 12.f, 0.f)
				[IconTextA(TEXT("bandage"), TAttribute<FText>::CreateLambda([RR, CId]() { const UDCRunSubsystem* X = RR(); const FDCCriminal* Q = X ? X->FindCriminal(CId) : nullptr; return FText::FromString(Q ? DC::Clock(Q->InjuredUntil - X->GetRunTime()) : TEXT("")); }), Mono(10.f), FSlateColor(Bad), FSlateColor(Text), 11.f, 5.f)]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[Cost >= 0
					? TextButton(TEXT("heart-pulse"), Money(Cost), [this, CId]() { UDCRunSubsystem* X = Run(); if (X && X->PatchUp(CId)) { Sfx(DCSfx::Confirm); } else { Sfx(DCSfx::Error); } }, EBtn::Primary,
						TAttribute<bool>::CreateLambda([RR, Cost]() { const UDCRunSubsystem* X = RR(); return X && X->GetCash() >= Cost; }), Tip(TEXT("Patch up"), TEXT("Back on their feet now"), TEXT("heart-pulse")), 10.f)
					: (TSharedRef<SWidget>)(SNew(SBox).ToolTip(Tip(TEXT("Patch-ups"), TEXT("Clinic level 2"), TEXT("lock")))[Icon(TEXT("lock"), 12.f, TextFaint)])];
			Body->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 6.f)[Row(Person(C, true), Right)];
		}
		if (N == 0) { Body->AddSlot().AutoHeight()[IconText(TEXT("heart"), TEXT("Everyone's healthy"), Font(10.f), Good, TextDim, 12.f, 6.f)]; }
	}
	else if (P == EDCPanel::Lawyer)
	{
		Body->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 10.f)[Label(TEXT("Arrested"))];
		for (const FDCCriminal& C : R->GetFallen())
		{
			const int32 CId = C.Id;
			const int32 Cost = R->GetBailCost(CId);
			TSharedRef<SWidget> Right = Cost >= 0
				? TextButton(TEXT("gavel"), Money(Cost), [this, CId]() { UDCRunSubsystem* X = Run(); if (X && X->BailOut(CId)) { Sfx(DCSfx::Confirm); } else { Sfx(DCSfx::Error); } }, EBtn::Primary,
					TAttribute<bool>::CreateLambda([RR, Cost]() { const UDCRunSubsystem* X = RR(); return X && X->GetCash() >= Cost; }), Tip(TEXT("Bail out"), TEXT("They come back to the crew"), TEXT("gavel")), 10.f)
				: (TSharedRef<SWidget>)(SNew(SBox).ToolTip(Tip(TEXT("Too late"), TEXT("Bail window has passed"), TEXT("lock")))[Icon(TEXT("lock"), 12.f, TextFaint)]);
			Body->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 6.f)[Row(Person(C, true), Right)];
		}
		if (R->GetFallen().Num() == 0) { Body->AddSlot().AutoHeight()[IconText(TEXT("scale"), TEXT("Nobody's inside"), Font(10.f), Good, TextDim, 12.f, 6.f)]; }
	}
	else if (P == EDCPanel::ServerRoom)
	{
		int32 Board = 0; for (const FDCMission& M : R->GetMissions()) { if (!M.bInProgress) { ++Board; } }
		Body->AddSlot().AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f).Padding(0.f, 0.f, 6.f, 0.f)[StatTile(TEXT("clipboard-list"), FString::FromInt(Board), TEXT("On the board"), Text)]
			+ SHorizontalBox::Slot().FillWidth(1.f).Padding(6.f, 0.f, 0.f, 0.f)
			[
				CardBox(SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()[Icon(TEXT("radio"), 16.f, Accent)]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 8.f, 0.f, 0.f)[TxtA(TAttribute<FText>::CreateLambda([RR]() {
						const UDCRunSubsystem* X = RR(); if (!X) { return FText::GetEmpty(); }
						const TArray<double> Pend = X->GetPendingPostingTimes();
						return FText::FromString(Pend.Num() ? DC::Clock(Pend[0]) : TEXT("FULL")); }), Mono(20.f), FSlateColor(Text))]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f, 0.f, 0.f)[TxtA(TAttribute<FText>::CreateLambda([RR]() { const UDCRunSubsystem* X = RR(); return FText::FromString(X && X->GetPendingPostingTimes().Num() == 0 ? TEXT("BOARD FULL") : TEXT("NEXT POSTING")); }), Caps(7.5f), FSlateColor(TextFaint))], FMargin(14.f))
			]
		];
	}
	else // Ops room
	{
		const int32 Tier = R->GetUnlockedTier();
		const bool bFinal = R->GetMissions().ContainsByPredicate([](const FDCMission& M) { return M.bFinal; });
		TSharedRef<SHorizontalBox> Tiers = SNew(SHorizontalBox);
		for (int32 t = 1; t <= 4; ++t)
		{
			const bool bOn = t <= Tier;
			Tiers->AddSlot().FillWidth(1.f).Padding(t > 1 ? 6.f : 0.f, 0.f, 0.f, 0.f)
			[
				CardBox(SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()[Pips(t, 4, bOn ? TierColor(t) : TextFaint, 5.f)]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 8.f, 0.f, 0.f)[IconText(bOn ? TEXT("lock-open") : TEXT("lock"), FString::Printf(TEXT("TIER %d"), t), Caps(8.f), bOn ? Good : TextFaint, bOn ? Text : TextFaint, 10.f, 5.f)],
					FMargin(12.f), bOn ? A(TierColor(t), 0.35f) : Line)
			];
		}
		Body->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 10.f)[Label(TEXT("Job tiers"))];
		Body->AddSlot().AutoHeight()[Tiers];
		Body->AddSlot().AutoHeight().Padding(0.f, 16.f, 0.f, 0.f)
		[
			CardBox(SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 12.f, 0.f)[IconTile(TEXT("crown"), 40.f, Accent)]
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()[Txt(TEXT("The Big Score"), Font(12.f, EW::SemiBold), Text)]
					+ SVerticalBox::Slot().AutoHeight()[Txt(bFinal ? TEXT("It's on the board. Pull it off to win.") : TEXT("Five crew, twenty minutes. Win the run."), Font(9.5f), TextDim)]
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[bFinal ? TextButton(TEXT("clipboard-list"), TEXT("Board"), [this]() { Open(EDCPanel::Missions); }, EBtn::Primary) : IconText(TEXT("trophy"), FString::Printf(TEXT("%d"), R->GetMissionsSucceeded()), Mono(11.f), TextDim, Text, 12.f, 5.f)],
				FMargin(14.f), A(Accent, 0.3f))
		];
	}
	return Shell(IconName, Name, Scroll(Body), Pips(Room.Level, 3, Accent, 5.f));
}

// ─────────────────────────────────────────────────────────────────────────────
// Modals
// ─────────────────────────────────────────────────────────────────────────────

TSharedRef<SWidget> SDCRoot::BuildPerkDraft()
{
	UDCRunSubsystem* R = Run();
	const TArray<FDCPerk> Offer = R ? R->GetPerkOffer() : TArray<FDCPerk>();
	const double T0 = PerkDraftT0; // set on open, so background rebuilds don't replay the card intro
	TSharedRef<SHorizontalBox> Cards = SNew(SHorizontalBox);
	for (int32 i = 0; i < Offer.Num(); ++i)
	{
		const FDCPerk P = Offer[i];
		const float Delay = 0.08f * i;
		TSharedRef<SWidget> Content = SNew(SBox).WidthOverride(230.f).HeightOverride(270.f).Padding(FMargin(18.f, 24.f))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
				[
					SNew(SOverlay)
					+ SOverlay::Slot()[SNew(SDCRing).Size(84.f).Thickness(2.f).Fill(1.f).Color(A(Accent, 0.5f)).Track(FLinearColor::Transparent)]
					+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)[Icon(P.Icon, 32.f, Accent)]
				]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 20.f, 0.f, 0.f)[Txt(P.DisplayName, Font(14.f, EW::SemiBold), Text)]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 8.f, 0.f, 0.f)
				[SNew(STextBlock).Text(FText::FromString(P.Description)).Font(Font(10.f)).ColorAndOpacity(TextDim).AutoWrapText(true).Justification(ETextJustify::Center)]
			];
		const FName Id = P.Id;
		Cards->AddSlot().AutoWidth().Padding(10.f, 0.f)
		[
			SNew(SBorder).BorderImage(NoBrush()).Padding(0.f)
			.ColorAndOpacity_Lambda([T0, Delay]() { return FLinearColor(1, 1, 1, EaseOut((float)(Now() - T0 - Delay) / 0.25f)); })
			.RenderTransform_Lambda([T0, Delay]() { return TOptional<FSlateRenderTransform>(FSlateRenderTransform(FVector2f(0.f, (1.f - EaseOut((float)(Now() - T0 - Delay) / 0.3f)) * 24.f))); })
			[
				Button(Content, [this, Id]()
				{
					if (PerkSel == Id) { return; }
					PerkSel = Id; Sfx(DCSfx::Click); Rebuild();
				}, PerkSel == Id ? EBtn::CardSelected : EBtn::Card, true, nullptr, FMargin(0.f))
			]
		];
	}
	if (Offer.Num() == 0) { Cards->AddSlot().AutoWidth()[Txt(TEXT("No perks on offer"), Font(11.f), TextDim)]; }
	return SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[IconTile(TEXT("sparkles"), 44.f, Accent)]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 14.f, 0.f, 4.f)[Txt(TEXT("Pick a perk"), Font(22.f, EW::SemiBold), Text)]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 0.f, 0.f, 28.f)[Txt(TEXT("Your reputation is growing. Choose one."), Font(11.f), TextDim)]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[Cards]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 28.f, 0.f, 0.f)
			[
				Offer.Num() ? TextButton(TEXT("check"), TEXT("Confirm"), [this]()
				{
					UDCRunSubsystem* X = Run();
					if (X && PerkSel != NAME_None && X->PickPerk(PerkSel)) { PerkSel = NAME_None; Sfx(DCSfx::Perk); Close(true); }
					else { Sfx(DCSfx::Error); }
				}, EBtn::Primary, TAttribute<bool>::CreateLambda([this]() { return PerkSel != NAME_None; }), nullptr, 13.f)
				: SNullWidget::NullWidget
			]
		];
}

void SDCRoot::NewRun()
{
	if (UDCRunSubsystem* R = Run())
	{
		bHasEndSummary = false;
		R->StartNewRun(0);
		Sfx(DCSfx::Confirm);
		Close(true);
	}
}

void SDCRoot::QuitGame()
{
	UObject* Ctx = GetContext();
	UWorld* W = (Ctx && GEngine) ? GEngine->GetWorldFromContextObject(Ctx, EGetWorldErrorMode::ReturnNull) : nullptr;
	UKismetSystemLibrary::QuitGame(W, W ? W->GetFirstPlayerController() : nullptr, EQuitPreference::Quit, false);
}

TSharedRef<SWidget> SDCRoot::BuildTitle()
{
	const double T0 = TitleT0;
	auto Fade = [T0](float Delay) { return TAttribute<FLinearColor>::CreateLambda([T0, Delay]() { return FLinearColor(1, 1, 1, EaseOut((float)(Now() - T0 - Delay) / 0.5f)); }); };

	// Slots: Continue = most recent unfinished run.
	const TArray<FDCSaveSlotInfo> Infos = SlotInfos();
	bool bAnyUsed = false;
	const FDCSaveSlotInfo* Recent = nullptr;
	for (const FDCSaveSlotInfo& I : Infos)
	{
		bAnyUsed |= I.bUsed;
		if (I.bUsed && !I.bFinished && (!Recent || I.SavedAt > Recent->SavedAt)) { Recent = &I; }
	}
	if (const UDCRunSubsystem* R = Run(); R && !bFakeData)
	{
		const int32 MR = R->GetMostRecentSlot(true);
		if (MR >= 0 && Infos.IsValidIndex(MR)) { Recent = &Infos[MR]; }
	}
	const int32 RecentSlot = Recent ? Recent->Slot : -1;
	const FString RecentSub = Recent ? FString::Printf(TEXT("Slot %d · Day %d · %s"), Recent->Slot + 1, Recent->Day, *HourMin(Recent->Hour)) : FString();

	auto MenuBtn = [](FName IconName, const FString& LabelText, const FString& Sub, TFunction<void()> OnClick, EBtn Kind, bool bEnabled)
	{
		const FLinearColor Fg = Kind == EBtn::Primary ? AccentInk : Text;
		TSharedRef<SVerticalBox> L = SNew(SVerticalBox) + SVerticalBox::Slot().AutoHeight()[Txt(LabelText, Font(12.f, EW::SemiBold, 40), Fg)];
		if (!Sub.IsEmpty()) { L->AddSlot().AutoHeight().Padding(0.f, 1.f, 0.f, 0.f)[Txt(Sub, Font(9.f, EW::Medium), A(Fg, 0.7f))]; }
		return Button(
			SNew(SBox).WidthOverride(260.f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 12.f, 0.f)[Icon(IconName, 14.f, Fg)]
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[L]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Icon(TEXT("chevron-right"), 12.f, A(Fg, 0.5f))]
			], OnClick, Kind, bEnabled, nullptr, FMargin(18.f, Sub.IsEmpty() ? 12.f : 8.f));
	};

	TSharedRef<SVerticalBox> Menu = SNew(SVerticalBox);
	if (RecentSlot >= 0)
	{
		Menu->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 10.f)[MenuBtn(TEXT("play"), TEXT("Continue"), RecentSub, [this, RecentSlot]() { ActOnSlot(RecentSlot, EView::SlotsLoad); }, EBtn::Primary, true)];
	}
	Menu->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 10.f)[MenuBtn(TEXT("plus"), TEXT("New run"), FString(), [this, bAnyUsed]()
	{
		if (bAnyUsed) { SetView(EView::SlotsNew); } else { ActOnSlot(0, EView::SlotsNew); }
	}, RecentSlot >= 0 ? EBtn::Secondary : EBtn::Primary, true)];
	Menu->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 10.f)[MenuBtn(TEXT("folder-open"), TEXT("Load"), FString(), [this]() { SetView(EView::SlotsLoad); }, EBtn::Secondary, bAnyUsed)];
	Menu->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 10.f)[MenuBtn(TEXT("settings"), TEXT("Options"), FString(), [this]() { SetView(EView::Options); }, EBtn::Secondary, true)];
	Menu->AddSlot().AutoHeight()[MenuBtn(TEXT("power"), TEXT("Quit"), FString(), [this]() { QuitGame(); }, EBtn::Ghost, true)];

	// Smooth left-to-right shade: one real gradient (slices leave visible steps and seams over a bright scene).
	TSharedRef<SWidget> Shade = SNew(SDCTitleShade);

	return SNew(SOverlay)
		// left-side shade so the wordmark reads over the scene
		+ SOverlay::Slot().HAlign(HAlign_Left)
		[
			SNew(SBox).WidthOverride(1100.f)[Shade]
		]
		+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Center).Padding(FMargin(120.f, 0.f, 0.f, 40.f))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Left)
			[SNew(SBorder).BorderImage(NoBrush()).Padding(0.f).ColorAndOpacity(Fade(0.f))[SNew(SBox).WidthOverride(56.f).HeightOverride(3.f)[SNew(SImage).Image(WhiteBrush()).ColorAndOpacity(Accent)]]]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 22.f, 0.f, 0.f)
			[SNew(SBorder).BorderImage(NoBrush()).Padding(0.f).ColorAndOpacity(Fade(0.05f))[Txt(TEXT("DIRECTOR'S"), Font(46.f, EW::Bold, 420), Text)]]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, -8.f, 0.f, 0.f)
			[SNew(SBorder).BorderImage(NoBrush()).Padding(0.f).ColorAndOpacity(Fade(0.12f))[Txt(TEXT("CUT"), Font(46.f, EW::Bold, 420), Accent)]]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 14.f, 0.f, 48.f)
			[SNew(SBorder).BorderImage(NoBrush()).Padding(0.f).ColorAndOpacity(Fade(0.2f))[Txt(TEXT("Run the crew. Plan the jobs. Take your cut."), Font(12.f), TextDim)]]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Left)
			[SNew(SBorder).BorderImage(NoBrush()).Padding(0.f).ColorAndOpacity(Fade(0.3f))[Menu]]
		]
		+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Bottom).Padding(FMargin(120.f, 0.f, 0.f, 40.f))
		[Txt(TEXT("EPIC MEGAJAM 2026"), Caps(8.f), TextFaint)]
		// leaderboard of finished runs (right)
		+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Center).Padding(FMargin(0.f, 0.f, 90.f, 40.f))
		[
			SNew(SBorder).BorderImage(NoBrush()).Padding(0.f).ColorAndOpacity(Fade(0.4f))
			.Visibility(View == EView::None ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed)
			[BuildLeaderboard()]
		]
		// sub-view (options / slots) over a dimmed title
		+ SOverlay::Slot()
		[
			SNew(SBorder).BorderImage(WhiteBrush()).BorderBackgroundColor(FLinearColor(0.01f, 0.012f, 0.016f, 0.62f)).Padding(0.f)
			.Visibility(View == EView::None ? EVisibility::Collapsed : EVisibility::Visible)
			[BuildView()]
		];
}

TSharedRef<SWidget> SDCRoot::BuildPause()
{
	if (View != EView::None) { return BuildView(); }
	UDCAudioSubsystem* Au = UDCAudioSubsystem::Get(GetContext());
	auto VolRow = [Au](FName IconName, const FString& Name, EDCVolumeChannel Ch)
	{
		TWeakObjectPtr<UDCAudioSubsystem> WAu = Au;
		auto Get = [WAu, Ch]() { return WAu.IsValid() ? WAu->GetVolume(Ch) : 1.f; };
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 10.f, 0.f)[SNew(SBox).WidthOverride(18.f)[Icon(IconName, 13.f, TextDim)]]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[SNew(SBox).WidthOverride(90.f)[Label(Name, TextDim, 8.f)]]
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
			[
				SNew(SBox).HeightOverride(24.f)
				[
					SNew(SOverlay)
					+ SOverlay::Slot().VAlign(VAlign_Center).Padding(FMargin(9.f, 0.f))[Bar(TAttribute<float>::CreateLambda(Get), FSlateColor(Accent), 4.f)]
					+ SOverlay::Slot()
					[
						SNew(SSlider).Style(&SliderStyle()).IsFocusable(false).MinValue(0.f).MaxValue(1.f)
						.Value_Lambda(Get)
						.OnValueChanged_Lambda([WAu, Ch](float V) { if (WAu.IsValid()) { WAu->SetVolume(Ch, V); } })
						.OnMouseCaptureEnd_Lambda([]() { Sfx(DCSfx::Click); })
					]
				]
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(10.f, 0.f, 0.f, 0.f)
			[SNew(SBox).WidthOverride(34.f).HAlign(HAlign_Right)[TxtA(TAttribute<FText>::CreateLambda([Get]() { return FText::FromString(FString::FromInt(FMath::RoundToInt(Get() * 100.f))); }), Mono(10.f), FSlateColor(Text))]];
	};

	const bool bArmed = bAbandonArmed;
	TSharedRef<SWidget> CardW = GlassPanel(
		SNew(SBox).WidthOverride(420.f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 12.f, 0.f)[IconTile(TEXT("pause"), 38.f, Accent)]
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()[Txt(TEXT("Paused"), Font(17.f, EW::SemiBold), Text)]
					+ SVerticalBox::Slot().AutoHeight()[Txt(TEXT("The clock is stopped"), Font(10.f), TextDim)]
				]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 18.f)[HLine()]
			+ SVerticalBox::Slot().AutoHeight()[TextButton(TEXT("play"), TEXT("Resume"), [this]() { Close(); }, EBtn::Primary, true, Tip(TEXT("Resume"), TEXT("Esc")), 12.f)]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 8.f, 0.f, 0.f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.f).Padding(0.f, 0.f, 4.f, 0.f)[TextButton(TEXT("save"), TEXT("Save"), [this]() { SetView(EView::SlotsSave); }, EBtn::Secondary, true, Tip(TEXT("Save to a slot"), TEXT("The run also autosaves to its own slot as you play"), TEXT("save")), 10.5f)]
				+ SHorizontalBox::Slot().FillWidth(1.f).Padding(4.f, 0.f)[TextButton(TEXT("folder-open"), TEXT("Load"), [this]() { SetView(EView::SlotsLoad); }, EBtn::Secondary, true, Tip(TEXT("Load a slot"), TEXT("This run is saved first"), TEXT("folder-open")), 10.5f)]
				+ SHorizontalBox::Slot().FillWidth(1.f).Padding(4.f, 0.f, 0.f, 0.f)[TextButton(TEXT("settings"), TEXT("Options"), [this]() { SetView(EView::Options); }, EBtn::Secondary, true, Tip(TEXT("Options"), TEXT("Audio, graphics, accessibility, camera"), TEXT("settings")), 10.5f)]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 22.f, 0.f, 12.f)[Label(TEXT("Audio"))]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 6.f)[VolRow(TEXT("volume-2"), TEXT("Master"), EDCVolumeChannel::Master)]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 6.f)[VolRow(TEXT("mouse-pointer-click"), TEXT("Effects"), EDCVolumeChannel::SFX)]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 6.f)[VolRow(TEXT("music"), TEXT("Music"), EDCVolumeChannel::Music)]
			+ SVerticalBox::Slot().AutoHeight()[VolRow(TEXT("building-2"), TEXT("Ambience"), EDCVolumeChannel::Ambience)]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 22.f, 0.f, 16.f)[HLine()]
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.f).Padding(0.f, 0.f, 5.f, 0.f)
				[TextButton(TEXT("log-out"), TEXT("Save & exit"), [this]()
				{
					if (UDCRunSubsystem* R = Run()) { R->SetPaused(false); R->ExitToTitle(); }
					Open(EDCPanel::Title);
				}, EBtn::Secondary, true, Tip(TEXT("Save & exit"), TEXT("Continue from the title screen. Time stays frozen."), TEXT("save")), 10.5f)]
				+ SHorizontalBox::Slot().FillWidth(1.f).Padding(5.f, 0.f, 0.f, 0.f)
				[TextButton(bArmed ? TEXT("triangle-alert") : TEXT("trash-2"), bArmed ? TEXT("Really abandon?") : TEXT("Abandon run"), [this, bArmed]()
				{
					if (!bArmed) { bAbandonArmed = true; Sfx(DCSfx::Click); Rebuild(); return; }
					if (UDCRunSubsystem* R = Run()) { R->SetPaused(false); R->AbandonRun(); }
					bAbandonArmed = false;
					Open(EDCPanel::Title);
				}, EBtn::Danger, true, Tip(TEXT("Abandon run"), TEXT("Deletes the save"), TEXT("trash-2"), Bad), 10.5f)]
			]
		], FMargin(26.f, 22.f));
	return SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center)[CardW];
}

TSharedRef<SWidget> SDCRoot::StatTile(FName IconName, const FString& Value, const FString& Caption, const FLinearColor& Col)
{
	return CardBox(
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()[Icon(IconName, 16.f, Col)]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 8.f, 0.f, 0.f)[Txt(Value, Mono(20.f), Text)]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f, 0.f, 0.f)[Label(Caption, TextFaint, 7.5f)],
		FMargin(14.f));
}

TSharedRef<SWidget> SDCRoot::BuildEnd(bool bWon)
{
	if (View != EView::None) { return BuildView(); }
	const UDCRunSubsystem* R = Run();
	const FDCRunSummary S = bHasEndSummary ? EndSummary : (R ? R->GetRunSummary() : FDCRunSummary());
	const FLinearColor Col = bWon ? Accent : Bad;

	TSharedRef<SUniformGridPanel> Tiles = SNew(SUniformGridPanel).SlotPadding(FMargin(5.f));
	Tiles->AddSlot(0, 0)[StatTile(TEXT("calendar"), FString::FromInt(S.DaysSurvived), TEXT("Days"), Col)];
	Tiles->AddSlot(1, 0)[StatTile(TEXT("trophy"), FString::Printf(TEXT("%d/%d"), S.MissionsSucceeded, S.MissionsAttempted), TEXT("Jobs"), Col)];
	Tiles->AddSlot(2, 0)[StatTile(TEXT("banknote"), MoneyShort(S.TotalTake), TEXT("Total take"), Col)];
	Tiles->AddSlot(3, 0)[StatTile(TEXT("hammer"), FString::FromInt(S.RoomsBuilt), TEXT("Buildings"), Col)];
	Tiles->AddSlot(4, 0)[StatTile(TEXT("clock"), DC::Clock(S.PlayTime), TEXT("Play time"), Col)];

	auto NameList = [](const TArray<FString>& Names, FName IconName, const FLinearColor& IC)
	{
		TSharedRef<SWrapBox> W = SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(6.f, 6.f));
		for (const FString& N : Names) { W->AddSlot()[Pill(IconText(IconName, N, Font(9.5f, EW::Medium), IC, Text, 10.f, 5.f), A(FLinearColor::White, 0.05f), FMargin(9.f, 3.f), Line)]; }
		if (Names.Num() == 0) { W->AddSlot()[Txt(TEXT("—"), Font(10.f), TextFaint)]; }
		return W;
	};
	// Wraps onto new rows: a long run can collect more perks than fit across the card.
	TSharedRef<SWrapBox> Perks = SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(6.f, 6.f));
	for (FName Id : S.Perks)
	{
		const FDCPerk* P = R ? R->FindPerk(Id) : nullptr;
		TSharedRef<SWidget> W = IconTile(P ? P->Icon : FName(TEXT("sparkles")), 30.f, Accent);
		W->SetToolTip(Tip(P ? P->DisplayName : Id.ToString(), P ? P->Description : FString(), P ? P->Icon : FName(TEXT("sparkles"))));
		Perks->AddSlot()[W];
	}

	TSharedRef<SWidget> CardW = GlassPanel(
		SNew(SBox).WidthOverride(780.f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[IconTile(bWon ? TEXT("crown") : TEXT("skull"), 60.f, Col, 0.48f, 0.14f)]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 16.f, 0.f, 4.f)[Txt(bWon ? TEXT("THE BIG SCORE") : TEXT("BROKE"), Font(26.f, EW::Bold, 300), Col)]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[Txt(!S.EndReason.IsEmpty() ? S.EndReason : (bWon ? TEXT("You pulled it off. Retire rich.") : TEXT("Couldn't make payroll. The crew walked.")), Font(11.f), TextDim)]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 24.f, 0.f, 0.f)[SNew(SBox).Padding(FMargin(-5.f, 0.f))[Tiles]]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 18.f, 0.f, 8.f)[Label(TEXT("Crew"))]
			+ SVerticalBox::Slot().AutoHeight()[NameList(S.Crew, TEXT("user-round"), Good)]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 14.f, 0.f, 8.f)[S.Lost.Num() ? Label(TEXT("Arrested")) : SNullWidget::NullWidget]
			+ SVerticalBox::Slot().AutoHeight()[S.Lost.Num() ? (TSharedRef<SWidget>)NameList(S.Lost, TEXT("siren"), Bad) : SNullWidget::NullWidget]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 14.f, 0.f, 8.f)[S.Perks.Num() ? Label(TEXT("Perks")) : SNullWidget::NullWidget]
			+ SVerticalBox::Slot().AutoHeight()[Perks]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 20.f, 0.f, 16.f)[HLine()]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 8.f, 0.f)[TextButton(TEXT("plus"), TEXT("New run"), [this]() { SetView(EView::SlotsNew); }, EBtn::Primary, true, nullptr, 12.f)]
				+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 8.f, 0.f)[TextButton(TEXT("save"), TEXT("Save this run"), [this]() { SetView(EView::SlotsSave); }, EBtn::Secondary, true,
					Tip(TEXT("Save this run"), TEXT("Keep a copy of this finished run in a slot. It stays on the leaderboard either way."), TEXT("save")), 12.f)]
				+ SHorizontalBox::Slot().AutoWidth()[TextButton(TEXT("house"), TEXT("Title"), [this]()
				{
					bHasEndSummary = false;
					if (UDCRunSubsystem* X = Run()) { if (X->GetPhase() != EDCGamePhase::Title) { X->ExitToTitle(); } }
					Open(EDCPanel::Title);
				}, EBtn::Secondary, true, nullptr, 12.f)]
			]
		], FMargin(30.f, 28.f));
	return SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center)[CardW];
}

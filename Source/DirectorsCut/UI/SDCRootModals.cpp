// Modal screens: title, settings, dailies, perk draft, game over, credits.
#include "UI/SDCRoot.h"
#include "UI/SDCRootInternal.h"
#include "GameFramework/GameUserSettings.h"
#include "Engine/Engine.h"
#include "HAL/PlatformMisc.h"

namespace
{
	struct FTimeline
	{
		float BarStart = 0.35f, BarEnd = 1.15f;
		float Roll1Start = 1.25f, Roll1End = 2.55f;
		float LuckyAt = -1.f, Roll2Start = -1.f, Roll2End = -1.f;
		float Land = 2.55f, StampAt = 2.7f, MembersAt = 3.2f;
	};
	FTimeline MakeTimeline(const FDCHeistResult& R)
	{
		FTimeline T;
		if (R.bLuckyBreak)
		{
			T.LuckyAt = T.Roll1End + 0.1f;
			T.Roll2Start = T.LuckyAt + 0.9f;
			T.Roll2End = T.Roll2Start + 1.1f;
			T.Land = T.Roll2End;
		}
		T.StampAt = T.Land + 0.15f;
		T.MembersAt = T.StampAt + 0.55f;
		return T;
	}
	float MarkerPos(const FDCHeistResult& R, const FTimeline& T, float t)
	{
		const float First = R.bLuckyBreak ? R.FirstRoll : R.Roll;
		auto Sweep = [](float From, float To, float u)
		{
			const float W = FMath::Square(1.f - u) * 0.22f * FMath::Sin(u * 26.f);
			return FMath::Clamp(FMath::Lerp(From, To, EaseOut(u)) + W, 0.f, 1.f);
		};
		if (t < T.Roll1Start) { return -1.f; }
		if (t < T.Roll1End) { return Sweep(0.f, First, (t - T.Roll1Start) / (T.Roll1End - T.Roll1Start)); }
		if (!R.bLuckyBreak) { return R.Roll; }
		if (t < T.Roll2Start) { return First; }
		if (t < T.Roll2End) { return Sweep(First, R.Roll, (t - T.Roll2Start) / (T.Roll2End - T.Roll2Start)); }
		return R.Roll;
	}
	FString CastNames(UDCRunSubsystem* R, const FDCHeistResult& H)
	{
		TArray<FString> N;
		for (const FDCCrewMemberResult& M : H.Members) { N.Add(M.Name); }
		return FString::Join(N, TEXT(", "));
	}
	const FDCCriminal* FindAnyCriminal(UDCRunSubsystem* R, int32 Id)
	{
		if (const FDCCriminal* C = R->FindCriminal(Id)) { return C; }
		return R->GetFallen().FindByPredicate([Id](const FDCCriminal& C) { return C.Id == Id; });
	}
	TSharedRef<SWidget> CenterModal(TSharedRef<SWidget> Content, float Width)
	{
		return SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(20.f)
			[SNew(SBox).WidthOverride(Width)[Content]];
	}
	float Flicker(double T)
	{
		const float Base = 0.9f + 0.06f * FMath::Sin((float)T * 13.f) * FMath::Sin((float)T * 7.3f);
		const float Dip = FMath::Frac((float)T * 0.31f) < 0.025f ? 0.55f : 1.f;
		const float Dip2 = FMath::Frac((float)T * 0.73f + 0.4f) < 0.012f ? 0.75f : 1.f;
		return Base * Dip * Dip2;
	}
	struct FResOption { int32 W, H; };
	const FResOption GRes[] = { { 1280, 720 }, { 1600, 900 }, { 1920, 1080 }, { 2560, 1440 } };
}

// ─────────────────────────────────────────────────────────────────────────────
// Title
// ─────────────────────────────────────────────────────────────────────────────

void SDCRoot::StartNewRun(int32 Seed)
{
	UDCRunSubsystem* R = Run();
	if (!R) { return; }
	bHasEndSummary = false;
	bPerkPending = false;
	PlanJobId = INDEX_NONE;
	PlanCrew.Reset();
	PlanCut = 0.30f;
	BenchCriminal = INDEX_NONE;
	Dailies = MakeShared<FDailiesState>();
	Close(true);
	if (Current != EDCPanel::None) { Close(true); }
	R->StartNewRun(Seed);
	bCashInit = false;
	bool bDone = false;
	GConfig->GetBool(TEXT("/Script/DirectorsCut.DCUI"), TEXT("TutorialDone"), bDone, GGameUserSettingsIni);
	TutorialStep = bDone ? -1 : 0;
	Sfx(DCSfx::Clapper);
	Toast(FString::Printf(TEXT("Night 1. Quiet on set... (seed %d)"), R->GetSeed()), false);
}

TSharedRef<SWidget> SDCRoot::BuildTitle()
{
	const double T0 = Now();
	const bool bCanContinue = HasRun() && Run()->GetPhase() == EDCGamePhase::Planning;

	TSharedRef<SVerticalBox> Buttons = SNew(SVerticalBox);
	if (bCanContinue)
	{
		Buttons->AddSlot().AutoHeight().Padding(0.f, 5.f)[SNew(SBox).HeightOverride(54.f)[Button(TEXT("Continue"), [this]() { Close(true); }, EBtn::Gold, true, 22.f)]];
	}
	Buttons->AddSlot().AutoHeight().Padding(0.f, 5.f)[SNew(SBox).HeightOverride(58.f)[Button(TEXT("New Run"), [this]() { StartNewRun(); }, EBtn::Red, true, 24.f)]];
	Buttons->AddSlot().AutoHeight().Padding(0.f, 5.f)[SNew(SBox).HeightOverride(48.f)[Button(TEXT("Settings"), [this]() { Open(EDCPanel::Settings); }, EBtn::Gold, true, 18.f)]];
	Buttons->AddSlot().AutoHeight().Padding(0.f, 5.f)[SNew(SBox).HeightOverride(44.f)[Button(TEXT("Quit"), []() { FPlatformMisc::RequestExit(false, TEXT("DCTitleQuit")); }, EBtn::Ghost, true, 16.f)]];

	auto FilmEdge = []()
	{
		return SNew(SBorder).BorderImage(Brush(EBrush::White)).BorderBackgroundColor(Rgb(0x0a, 0x08, 0x07)).Padding(FMargin(0.f, 8.f))[Sprockets(48, 12.f)];
	};

	return SNew(SOverlay)
		+ SOverlay::Slot()
		[
			SNew(SImage).Image(Brush(EBrush::White))
			.ColorAndOpacity_Lambda([]() { return FSlateColor(FLinearColor(0.012f, 0.009f, 0.007f, 0.93f + 0.03f * Flicker(Now()))); })
		]
		// projector beam glow
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
		[
			SNew(SBox).WidthOverride(1300.f).HeightOverride(560.f)
			[
				SNew(SImage).Image(Brush(EBrush::Chip))
				.ColorAndOpacity_Lambda([]() { return FSlateColor(WithAlpha(Rgb(0x5a, 0x48, 0x2a), 0.10f * Flicker(Now()))); })
			]
		]
		+ SOverlay::Slot().VAlign(VAlign_Top)[FilmEdge()]
		+ SOverlay::Slot().VAlign(VAlign_Bottom)[FilmEdge()]
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[Txt(TEXT("A  NIGHTLY  PRODUCTION  IN  CRIME"), Sans(20), Muted)]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 6.f, 0.f, 0.f)
			[
				SNew(STextBlock).Text(FText::FromString(TEXT("DIRECTOR'S CUT"))).Font(Display(132))
				.ColorAndOpacity_Lambda([T0]()
				{
					const float Fade = EaseOut((float)(Now() - T0) / 1.4f);
					return FSlateColor(WithAlpha(Gold, Fade * Flicker(Now())));
				})
				.RenderTransform_Lambda([]()
				{
					const float J = FMath::Frac((float)Now() * 0.31f) < 0.025f ? 2.f : 0.f;
					return TOptional<FSlateRenderTransform>(FSlateRenderTransform(FVector2f(J, -J * 0.5f)));
				})
			]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 0.f, 0.f, 6.f)[SNew(SBox).WidthOverride(620.f)[HLine(WithAlpha(Gold, 0.7f))]]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 8.f, 0.f, 36.f)
			[Txt(TEXT("Every heist is a production. Every crew is a cast. You get final cut."), Type(24), Cream)]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[SNew(SBox).WidthOverride(340.f)[Buttons]]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 30.f, 0.f, 0.f)
			[Txt(TEXT("Click stations in the hideout  -  Space: Roll Camera  -  Esc: back / menu"), Sans(14), WithAlpha(Muted, 0.8f))]
		];
}

// ─────────────────────────────────────────────────────────────────────────────
// Settings (the projector)
// ─────────────────────────────────────────────────────────────────────────────

TSharedRef<SWidget> SDCRoot::BuildSettings()
{
	UGameUserSettings* GS = GEngine ? GEngine->GetGameUserSettings() : nullptr;
	if (ResIndex < 0 && GS)
	{
		const FIntPoint Cur = GS->GetScreenResolution();
		ResIndex = 1;
		for (int32 i = 0; i < UE_ARRAY_COUNT(GRes); ++i) { if (GRes[i].W == Cur.X && GRes[i].H == Cur.Y) { ResIndex = i; } }
	}

	TSharedRef<SVerticalBox> Body = SNew(SVerticalBox);
	Body->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 6.f)[Header(TEXT("Sound"), false, 16.f)];
	static const TCHAR* Names[] = { TEXT("Master"), TEXT("Music"), TEXT("Effects"), TEXT("Ambience") };
	for (int32 i = 0; i < 4; ++i)
	{
		const EDCVolumeChannel Ch = (EDCVolumeChannel)i;
		auto GetV = [Ch]() { UDCAudioSubsystem* A = UDCAudioSubsystem::Get(GetContext()); return A ? A->GetVolume(Ch) : 1.f; };
		Body->AddSlot().AutoHeight().Padding(0.f, 4.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[SNew(SBox).WidthOverride(110.f)[Txt(Names[i], Sans(17), Cream)]]
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
			[
				SNew(SSlider).Style(&SliderStyle()).IsFocusable(false).MinValue(0.f).MaxValue(1.f).StepSize(0.05f)
				.Value_Lambda(GetV)
				.OnValueChanged_Lambda([Ch](float V) { if (UDCAudioSubsystem* A = UDCAudioSubsystem::Get(GetContext())) { A->SetVolume(Ch, V); } })
				.OnMouseCaptureEnd_Lambda([]() { Sfx(DCSfx::Click); })
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(12.f, 0.f, 0.f, 0.f)
			[SNew(SBox).WidthOverride(50.f)[TxtA(TAttribute<FText>::CreateLambda([GetV]() { return FText::FromString(Pct(GetV())); }), Sans(16), Gold, ETextJustify::Right)]]
		];
	}

	Body->AddSlot().AutoHeight().Padding(0.f, 16.f, 0.f, 6.f)[Header(TEXT("Picture"), false, 16.f)];
	Body->AddSlot().AutoHeight().Padding(0.f, 4.f)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[SNew(SBox).WidthOverride(110.f)[Txt(TEXT("Display"), Sans(17), Cream)]]
		+ SHorizontalBox::Slot().FillWidth(1.f)
		[
			Button(TEXT("Toggle fullscreen"), []()
			{
				if (UGameUserSettings* G = GEngine ? GEngine->GetGameUserSettings() : nullptr)
				{
					G->SetFullscreenMode(G->GetFullscreenMode() == EWindowMode::Windowed ? EWindowMode::WindowedFullscreen : EWindowMode::Windowed);
					G->ApplySettings(false);
				}
			}, EBtn::Gold, GS != nullptr, 15.f)
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(12.f, 0.f, 0.f, 0.f)
		[
			SNew(SBox).WidthOverride(150.f)
			[TxtA(TAttribute<FText>::CreateLambda([]()
			{
				UGameUserSettings* G = GEngine ? GEngine->GetGameUserSettings() : nullptr;
				return FText::FromString(!G ? FString(TEXT("-")) : G->GetFullscreenMode() == EWindowMode::Windowed ? FString(TEXT("Windowed")) : FString(TEXT("Fullscreen")));
			}), Sans(16), Gold, ETextJustify::Right)]
		]
	];
	Body->AddSlot().AutoHeight().Padding(0.f, 4.f)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[SNew(SBox).WidthOverride(110.f)[Txt(TEXT("Resolution"), Sans(17), Cream)]]
		+ SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(44.f)[Button(TEXT("<"), [this]() { ResIndex = (ResIndex + UE_ARRAY_COUNT(GRes) - 1) % UE_ARRAY_COUNT(GRes); }, EBtn::Ghost)]]
		+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
		[TxtA(TAttribute<FText>::CreateLambda([this]() { const int32 I = FMath::Clamp(ResIndex, 0, (int32)UE_ARRAY_COUNT(GRes) - 1); return FText::FromString(FString::Printf(TEXT("%d x %d"), GRes[I].W, GRes[I].H)); }), Sans(18), Gold, ETextJustify::Center)]
		+ SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(44.f)[Button(TEXT(">"), [this]() { ResIndex = (ResIndex + 1) % UE_ARRAY_COUNT(GRes); }, EBtn::Ghost)]]
		+ SHorizontalBox::Slot().AutoWidth().Padding(12.f, 0.f, 0.f, 0.f)
		[
			Button(TEXT("Apply"), [this]()
			{
				if (UGameUserSettings* G = GEngine ? GEngine->GetGameUserSettings() : nullptr)
				{
					const int32 I = FMath::Clamp(ResIndex, 0, (int32)UE_ARRAY_COUNT(GRes) - 1);
					G->SetScreenResolution(FIntPoint(GRes[I].W, GRes[I].H));
					G->ApplySettings(false);
				}
			}, EBtn::Gold, GS != nullptr, 15.f)
		]
	];

	if (HasRun())
	{
		Body->AddSlot().AutoHeight().Padding(0.f, 16.f, 0.f, 6.f)[Header(TEXT("This run"), false, 16.f)];
		Body->AddSlot().AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[Txt(TEXT("Run seed (replay the same deal of the cards)"), Sans(16), Muted)]
			+ SHorizontalBox::Slot().AutoWidth()[Txt(FString::FromInt(Run()->GetSeed()), Big(34), Gold)]
		];
	}

	TSharedRef<SHorizontalBox> Actions = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(1.f).Padding(0.f, 0.f, 8.f, 0.f)[SNew(SBox).HeightOverride(48.f)[Button(TEXT("Resume"), [this]() { Close(); }, EBtn::Gold, true, 18.f)]];
	if (HasRun() && ReturnPanel == EDCPanel::None)
	{
		Actions->AddSlot().FillWidth(1.f).Padding(0.f, 0.f, 8.f, 0.f)[SNew(SBox).HeightOverride(48.f)[Button(TEXT("Title screen"), [this]() { ReturnPanel = EDCPanel::None; Open(EDCPanel::Title); }, EBtn::Ghost, true, 16.f)]];
	}
	Actions->AddSlot().FillWidth(1.f)[SNew(SBox).HeightOverride(48.f)[Button(TEXT("Quit game"), []() { FPlatformMisc::RequestExit(false, TEXT("DCSettingsQuit")); }, EBtn::Red, true, 16.f)]];
	Body->AddSlot().AutoHeight().Padding(0.f, 22.f, 0.f, 0.f)[Actions];

	return CenterModal(DecoFrame(
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()[Txt(TEXT("THE PROJECTOR"), Display(38), Gold)]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 12.f)[Txt(TEXT("Adjust the picture and the sound. The film keeps rolling."), Type(15), Muted)]
		+ SVerticalBox::Slot().AutoHeight()[Body]), 680.f);
}

// ─────────────────────────────────────────────────────────────────────────────
// Dailies
// ─────────────────────────────────────────────────────────────────────────────

TSharedRef<SWidget> SDCRoot::BuildDailies()
{
	// Opened directly (e.g. dev shots) without a fresh night: replay last night's report.
	UDCRunSubsystem* R = Run();
	if (R && Dailies->Report.Night == 0 && R->GetLastNightReport().Night > 0)
	{
		Dailies->Report = R->GetLastNightReport();
		Dailies->Index = 0;
		Dailies->T0 = Now();
		Dailies->bSummary = Dailies->Report.Results.Num() == 0;
		Dailies->bLive = R->GetPhase() == EDCGamePhase::Dailies;
	}
	TSharedPtr<FDailiesState> D = Dailies;

	// Clapperboard: striped arm snaps shut at the start of each take.
	TSharedRef<SHorizontalBox> Stripes = SNew(SHorizontalBox);
	for (int32 i = 0; i < 7; ++i)
	{
		Stripes->AddSlot().FillWidth(1.f)[SNew(SImage).Image(Brush(EBrush::White)).ColorAndOpacity(i % 2 ? Ink : Cream)];
	}
	TSharedRef<SWidget> Clapper = SNew(SBox).WidthOverride(120.f).HeightOverride(70.f)
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SBox).HeightOverride(16.f)
			.RenderTransformPivot(FVector2D(0.f, 1.f))
			.RenderTransform_Lambda([D]()
			{
				const float t = (float)(Now() - D->T0);
				const float A = t < 0.3f ? -24.f * (1.f - EaseOut(t / 0.3f)) : 0.f;
				return TOptional<FSlateRenderTransform>(FSlateRenderTransform(FQuat2D(FMath::DegreesToRadians(A))));
			})
			[Stripes]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f, 0.f, 0.f)
		[
			SNew(SBorder).BorderImage(Brush(EBrush::White)).BorderBackgroundColor(Ink).Padding(FMargin(6.f, 2.f))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()[TxtA(TAttribute<FText>::CreateLambda([D]() { return FText::FromString(FString::Printf(TEXT("NIGHT %d"), D->Report.Night)); }), Sans(13), Cream)]
				+ SVerticalBox::Slot().AutoHeight()[TxtA(TAttribute<FText>::CreateLambda([D]()
				{
					return FText::FromString(D->bSummary ? FString(TEXT("WRAP")) : FString::Printf(TEXT("TAKE %d"), D->Index + 1));
				}), Sans(13), Cream)]
			]
		]
	];

	return CenterModal(DecoFrame(
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()[Sprockets(36, 9.f)]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 10.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Clapper]
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center).Padding(18.f, 0.f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()[Txt(TEXT("THE DAILIES"), Display(40), Gold)]
				+ SVerticalBox::Slot().AutoHeight()[Txt(TEXT("Tonight's footage, straight from the lab."), Type(15), Muted)]
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				TxtA(TAttribute<FText>::CreateLambda([D]()
				{
					const int32 N = D->Report.Results.Num();
					return FText::FromString(D->bSummary ? FString(TEXT("NIGHT SUMMARY")) : FString::Printf(TEXT("HEIST %d OF %d"), D->Index + 1, N));
				}), Sans(18), Muted)
			]
		]
		+ SVerticalBox::Slot().AutoHeight()[HLine(WithAlpha(Gold, 0.6f))]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 14.f)
		[
			SAssignNew(DailiesSlot, SBox).MinDesiredHeight(520.f)
			[
				D->bSummary ? BuildDailiesSummary() : BuildDailiesCard()
			]
		]
		+ SVerticalBox::Slot().AutoHeight()[Sprockets(36, 9.f)], FMargin(22.f, 14.f)), 1040.f);
}

TSharedRef<SWidget> SDCRoot::BuildDailiesCard()
{
	TSharedPtr<FDailiesState> D = Dailies;
	if (!D->Report.Results.IsValidIndex(D->Index)) { return BuildDailiesSummary(); }
	const FDCHeistResult H = D->Report.Results[D->Index];
	const FTimeline TL = MakeTimeline(H);
	UDCRunSubsystem* R = Run();
	const bool bWin = H.Outcome == EDCHeistOutcome::Clean || H.Outcome == EDCHeistOutcome::Messy;
	const float W = 900.f;
	auto LocalT = [D]() { return (float)(Now() - D->T0); };

	// ── Chance bar ──
	auto Fill = [D, TL]() { return EaseOut(((float)(Now() - D->T0) - TL.BarStart) / (TL.BarEnd - TL.BarStart)); };
	const float S = FMath::Clamp(H.SuccessChance, 0.f, 1.f);
	const float Cl = FMath::Clamp(H.CleanThreshold, 0.f, S);
	TSharedRef<SWidget> ChanceBar = SNew(SBox).WidthOverride(W).HeightOverride(56.f)
	[
		SNew(SOverlay)
		+ SOverlay::Slot().VAlign(VAlign_Center)
		[
			SNew(SBox).HeightOverride(30.f)
			[
				SNew(SOverlay)
				+ SOverlay::Slot()[SNew(SImage).Image(Brush(EBrush::White)).ColorAndOpacity(Rgb(0x4a, 0x12, 0x10))]
				+ SOverlay::Slot()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(TAttribute<float>::CreateLambda([Fill, S]() { return FMath::Max(0.0001f, S * Fill()); }))[SNew(SImage).Image(Brush(EBrush::White)).ColorAndOpacity(Rgb(0x6f, 0x8f, 0x3a))]
					+ SHorizontalBox::Slot().FillWidth(TAttribute<float>::CreateLambda([Fill, S]() { return FMath::Max(0.0001f, 1.f - S * Fill()); }))[SNew(SSpacer)]
				]
				+ SOverlay::Slot()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(TAttribute<float>::CreateLambda([Fill, Cl]() { return FMath::Max(0.0001f, Cl * Fill()); }))[SNew(SImage).Image(Brush(EBrush::White)).ColorAndOpacity(Rgb(0x9c, 0xc8, 0x5c))]
					+ SHorizontalBox::Slot().FillWidth(TAttribute<float>::CreateLambda([Fill, Cl]() { return FMath::Max(0.0001f, 1.f - Cl * Fill()); }))[SNew(SSpacer)]
				]
				+ SOverlay::Slot()[SNew(SBorder).BorderImage(Brush(EBrush::Outline)).BorderBackgroundColor(Gold)]
			]
		]
		// the roll needle
		+ SOverlay::Slot().HAlign(HAlign_Left)
		[
			SNew(SBox)
			.Visibility_Lambda([D, H, TL]() { return MarkerPos(H, TL, (float)(Now() - D->T0)) >= 0.f ? EVisibility::HitTestInvisible : EVisibility::Hidden; })
			.RenderTransform_Lambda([D, H, TL, W]()
			{
				const float P = FMath::Max(0.f, MarkerPos(H, TL, (float)(Now() - D->T0)));
				return TOptional<FSlateRenderTransform>(FSlateRenderTransform(FVector2f(P * W - 3.f, 0.f)));
			})
			[
				SNew(SBox).WidthOverride(6.f)[SNew(SImage).Image(Brush(EBrush::White)).ColorAndOpacity(Cream)]
			]
		]
	];

	TSharedRef<SWidget> BarLegend = SNew(SBox).WidthOverride(W)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(1.f)
		[Txt(FString::Printf(TEXT("CLEAN  under %s     SUCCESS  under %s     the rest is trouble"), *Pct(Cl), *Pct(S)), Sans(14), Muted)]
		+ SHorizontalBox::Slot().AutoWidth()
		[
			SNew(STextBlock).Font(Big(28))
			.Text_Lambda([D, H, TL]()
			{
				const float P = MarkerPos(H, TL, (float)(Now() - D->T0));
				return FText::FromString(P < 0.f ? FString() : FString::Printf(TEXT("ROLL  %02d"), FMath::Clamp(FMath::FloorToInt(P * 100.f), 0, 99)));
			})
			.ColorAndOpacity(Cream)
		]
	];

	// ── Lucky break callout ──
	TSharedRef<SWidget> Lucky = SNew(SBox).HAlign(HAlign_Center)
		.Visibility_Lambda([D, TL]() { const float t = (float)(Now() - D->T0); return (TL.LuckyAt > 0.f && t >= TL.LuckyAt) ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Txt(TEXT("LUCKY BREAK!"), Big(38), Gold)]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(12.f, 0.f)[Txt(TEXT("the first take blew it, but fortune called for a free re-roll"), Type(15), Cream)]
		];

	// ── Stamp ──
	const FLinearColor StampCol = FLinearColor::LerpUsingHSV(OutcomeColor(H.Outcome), FLinearColor::White, 0.12f);
	TSharedRef<SWidget> StampW = SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center).HeightOverride(150.f)
	[
		SNew(SBorder).BorderImage(FStyleDefaults::GetNoBrush()).Padding(0.f)
		.Visibility_Lambda([D, TL]() { return (float)(Now() - D->T0) >= TL.StampAt ? EVisibility::HitTestInvisible : EVisibility::Hidden; })
		.ColorAndOpacity_Lambda([D, TL]() { return FLinearColor(1, 1, 1, EaseOut(((float)(Now() - D->T0) - TL.StampAt) / 0.08f)); })
		.RenderTransformPivot(FVector2D(0.5f, 0.5f))
		.RenderTransform_Lambda([D, TL]()
		{
			const float u = FMath::Clamp(((float)(Now() - D->T0) - TL.StampAt) / 0.16f, 0.f, 1.f);
			return TOptional<FSlateRenderTransform>(FSlateRenderTransform(1.f + 1.6f * FMath::Square(1.f - u)));
		})
		[
			Stamp(DC::OutcomeStamp(H.Outcome), StampCol, H.Outcome == EDCHeistOutcome::Messy ? 54.f : 72.f, bWin ? -6.f : -9.f)
		]
	];

	// ── Members ──
	TSharedRef<SVerticalBox> Members = SNew(SVerticalBox);
	for (const FDCCrewMemberResult& M : H.Members)
	{
		const FDCCriminal* Cr = R ? FindAnyCriminal(R, M.CriminalId) : nullptr;
		const FLinearColor TC = Cr ? TypeCol(Cr->Type) : Muted;
		TSharedRef<SHorizontalBox> Tags = SNew(SHorizontalBox);
		auto AddTag = [&Tags](TSharedRef<SWidget> W) { Tags->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 6.f, 0.f)[W]; };
		if (M.XPGained > 0) { AddTag(Chip(FString::Printf(TEXT("+%d XP"), M.XPGained), Gold, 13.f)); }
		if (M.bLevelledUp)
		{
			AddTag(SNew(SBox).RenderTransformPivot(FVector2D(0.5f, 0.5f))
				.RenderTransform_Lambda([D, TL]() { const float t = (float)(Now() - D->T0) - TL.MembersAt; return TOptional<FSlateRenderTransform>(FSlateRenderTransform(1.f + 0.12f * FMath::Max(0.f, FMath::Sin(t * 6.f)) * FMath::Clamp(3.f - t, 0.f, 1.f))); })
				[Chip(FString::Printf(TEXT("LEVEL UP!  LV %d"), M.NewLevel), Rgb(0xff, 0xd8, 0x6a), 14.f)]);
		}
		if (M.LoyaltyDelta != 0) { AddTag(Chip(FString::Printf(TEXT("%+d LOYALTY"), M.LoyaltyDelta), M.LoyaltyDelta > 0 ? Green : Rgb(0x9a, 0x50, 0x20), 13.f)); }
		if (M.bInjured) { AddTag(Chip(FString::Printf(TEXT("INJURED %d NIGHT%s"), M.InjuryNights, M.InjuryNights == 1 ? TEXT("") : TEXT("S")), Red, 13.f)); }
		if (M.bArrested) { AddTag(Chip(TEXT("ARRESTED"), Rgb(0x60, 0x08, 0x08), 13.f)); }
		if (M.bArrestPrevented) { AddTag(Chip(TEXT("ARREST DODGED"), Gold, 13.f)); }
		if (!M.bInjured && !M.bArrested && !M.bLevelledUp && M.XPGained <= 0) { AddTag(Txt(TEXT("walked away"), Type(13), Muted)); }
		Members->AddSlot().AutoHeight().Padding(0.f, 3.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[SNew(SBox).WidthOverride(10.f).HeightOverride(10.f)[SNew(SImage).Image(Brush(EBrush::Round)).ColorAndOpacity(TC)]]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8.f, 0.f)[SNew(SBox).MinDesiredWidth(250.f)[Txt(Cr ? Cr->GetDisplayName() : M.Name, Sans(17), Cream)]]
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[Tags]
		];
	}

	// ── Flavour (typed out) ──
	const FString Flav = H.FlavourLine;
	TSharedRef<SWidget> Flavour = SNew(STextBlock).Font(Type(19)).ColorAndOpacity(Cream).AutoWrapText(true).Justification(ETextJustify::Center)
		.Text_Lambda([D, TL, Flav]()
		{
			const float t = (float)(Now() - D->T0) - TL.StampAt - 0.2f;
			const int32 N = t <= 0.f ? 0 : FMath::Min(Flav.Len(), FMath::FloorToInt(t * 55.f));
			return FText::FromString(Flav.Left(N));
		});

	auto MembersAlpha = [D, TL]() { return FLinearColor(1, 1, 1, EaseOut(((float)(Now() - D->T0) - TL.MembersAt) / 0.4f)); };

	const bool bLast = D->Index >= D->Report.Results.Num() - 1;
	const bool bRetake = H.bRetakeAvailable && !H.bRetaken && !bWin;

	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()[Txt(H.JobName, Display(34), Cream)]
				+ SVerticalBox::Slot().AutoHeight()[Txt(FString::Printf(TEXT("Starring %s.  Your cut: %s%s"), *CastNames(R, H), *Pct(H.Plan.Cut), H.bRetaken ? TEXT("   (RETAKE)") : TEXT("")), Type(15), Muted, true)]
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right)[Txt(TEXT("ODDS"), Sans(13), Muted)]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right).Padding(0.f, -6.f, 0.f, 0.f)[Txt(Pct(H.SuccessChance), Big(46), Gold)]
			]
		]
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 14.f, 0.f, 0.f)[ChanceBar]
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[BarLegend]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f, 0.f, 0.f)[Lucky]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f)[StampW]
		+ SVerticalBox::Slot().AutoHeight().Padding(40.f, 0.f, 40.f, 10.f)[Flavour]
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SBorder).BorderImage(FStyleDefaults::GetNoBrush()).Padding(0.f).ColorAndOpacity_Lambda(MembersAlpha)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.f)[Members]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(16.f, 0.f, 0.f, 0.f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right)[Txt(TEXT("YOUR TAKE"), Sans(14), Muted)]
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right).Padding(0.f, -8.f, 0.f, 0.f)
					[Txt(H.DirectorTake > 0 ? FString(TEXT("+")) + Money(H.DirectorTake) : FString(TEXT("NOTHING")), Big(52), H.DirectorTake > 0 ? GreenBright : RedBright)]
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right)[Txt(H.PayoutTotal > 0 ? FString::Printf(TEXT("of a %s haul"), *Money(H.PayoutTotal)) : FString(TEXT("no payout")), Sans(14), Muted)]
				]
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 16.f, 0.f, 0.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth()
			[
				bRetake
				? StaticCastSharedRef<SWidget>(SNew(SBox).HeightOverride(50.f)
					.Visibility_Lambda([D, TL]() { return (float)(Now() - D->T0) >= TL.MembersAt ? EVisibility::Visible : EVisibility::Hidden; })
					[Button(TEXT("Retake! Re-shoot it"), [this]() { DailiesRetake(); }, EBtn::Gold, true, 18.f, TEXT("Screening Room: re-roll this failed heist"))])
				: SNullWidget::NullWidget
			]
			+ SHorizontalBox::Slot().FillWidth(1.f)[SNew(SSpacer)]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SBox).HeightOverride(50.f).MinDesiredWidth(260.f)
				[
					ButtonContent(
						SNew(SBox).HAlign(HAlign_Center)
						[
							SNew(STextBlock).Font(Sans(18))
							.Text_Lambda([D, TL, bLast]()
							{
								if ((float)(Now() - D->T0) < TL.MembersAt) { return FText::FromString(TEXT("SKIP  >>")); }
								return FText::FromString(bLast ? TEXT("SEE THE NIGHT'S TOTALS  >") : TEXT("NEXT HEIST  >"));
							})
						],
						[this]() { DailiesNext(); }, EBtn::Red, true)
				]
			]
		];
}

TSharedRef<SWidget> SDCRoot::BuildDailiesSummary()
{
	UDCRunSubsystem* R = Run();
	// Prefer the live report (retakes can change it), as long as it's the same night.
	FDCNightReport Rep = Dailies->Report;
	if (R && R->GetLastNightReport().Night == Rep.Night) { Rep = R->GetLastNightReport(); }

	TSharedRef<SVerticalBox> Body = SNew(SVerticalBox);
	Body->AddSlot().AutoHeight()[Txt(Rep.Results.Num() ? FString::Printf(TEXT("THAT'S A WRAP ON NIGHT %d"), Rep.Night) : FString::Printf(TEXT("NIGHT %d: A QUIET ONE"), Rep.Night), Display(32), Cream)];
	if (Rep.Results.Num() == 0)
	{
		Body->AddSlot().AutoHeight().Padding(0.f, 6.f)[Txt(TEXT("No cameras rolled tonight. The crew played cards, and the bills still came in."), Type(17), Muted, true)];
	}
	for (const FDCHeistResult& H : Rep.Results)
	{
		Body->AddSlot().AutoHeight().Padding(0.f, 4.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[Txt(H.JobName, Sans(19), Cream)]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(12.f, 0.f)[Chip(DC::OutcomeStamp(H.Outcome), OutcomeColor(H.Outcome), 14.f)]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[SNew(SBox).MinDesiredWidth(130.f)[Txt(Money(H.DirectorTake), Big(30), H.DirectorTake > 0 ? GreenBright : Muted, false, ETextJustify::Right)]]
		];
	}
	Body->AddSlot().AutoHeight().Padding(0.f, 10.f)[HLine(WithAlpha(Gold, 0.45f))];
	Body->AddSlot().AutoHeight().Padding(0.f, 2.f)[KeyValue(TEXT("Director's take"), FString(TEXT("+")) + Money(Rep.TotalTake), GreenBright)];
	Body->AddSlot().AutoHeight().Padding(0.f, 2.f)[KeyValue(TEXT("Upkeep paid"), Money(-Rep.UpkeepPaid), Rgb(0xe0, 0x8a, 0x70))];
	Body->AddSlot().AutoHeight().Padding(0.f, 2.f)[KeyValue(TEXT("Cash"), FString::Printf(TEXT("%s  >  %s"), *Money(Rep.CashBefore), *Money(Rep.CashAfter)), Rep.CashAfter >= 0 ? Gold : RedBright)];
	if (Rep.Events.Num())
	{
		Body->AddSlot().AutoHeight().Padding(0.f, 12.f, 0.f, 4.f)[Header(TEXT("Also tonight"), false, 15.f)];
		for (const FString& E : Rep.Events) { Body->AddSlot().AutoHeight().Padding(0.f, 1.f)[Txt(FString(TEXT("-  ")) + E, Type(16), Cream, true)]; }
	}
	if (Rep.bGameOver) { Body->AddSlot().AutoHeight().Padding(0.f, 12.f)[Txt(TEXT("The money's run out. The studio is pulling the plug..."), Sans(20), RedBright, true)]; }
	if (Rep.bWon) { Body->AddSlot().AutoHeight().Padding(0.f, 12.f)[Txt(TEXT("The Premiere was a smash. Roll the credits."), Sans(20), Gold, true)]; }

	const FString Label = Rep.bGameOver ? TEXT("Face the music") : Rep.bWon ? TEXT("Roll credits") : TEXT("Continue to night ") + FString::FromInt(Rep.Night + 1);
	Body->AddSlot().AutoHeight().HAlign(HAlign_Right).Padding(0.f, 18.f, 0.f, 0.f)
	[
		SNew(SBox).HeightOverride(52.f).MinDesiredWidth(280.f)[Button(Label, [this]() { FinishDailiesFlow(); }, EBtn::Red, true, 20.f)]
	];
	return Body;
}

void SDCRoot::DailiesTick()
{
	TSharedPtr<FDailiesState> D = Dailies;
	if (!D.IsValid() || D->bSummary || !D->Report.Results.IsValidIndex(D->Index)) { return; }
	const FDCHeistResult& H = D->Report.Results[D->Index];
	const FTimeline TL = MakeTimeline(H);
	const float t = (float)(Now() - D->T0);
	const bool bWin = H.Outcome == EDCHeistOutcome::Clean || H.Outcome == EDCHeistOutcome::Messy;
	if (!D->bClap) { D->bClap = true; Sfx(DCSfx::Clapper); }
	if (!D->bDice && t >= TL.Roll1Start && t < TL.Land) { D->bDice = true; Sfx(DCSfx::DiceRoll); }
	if (!D->bLuckySfx && TL.LuckyAt > 0.f && t >= TL.LuckyAt && t < TL.StampAt) { D->bLuckySfx = true; Sfx(DCSfx::StingLevelUp, 0.8f); }
	if (!D->bDice2 && TL.Roll2Start > 0.f && t >= TL.Roll2Start && t < TL.Land) { D->bDice2 = true; Sfx(DCSfx::DiceRoll); }
	if (!D->bStamp && t >= TL.StampAt)
	{
		D->bStamp = true;
		if (t < TL.StampAt + 1.f) { Sfx(DCSfx::Stamp); Sfx(bWin ? DCSfx::StingWrap : DCSfx::StingCut); }
	}
	if (!D->bMembers && t >= TL.MembersAt)
	{
		D->bMembers = true;
		if (t < TL.MembersAt + 1.f)
		{
			if (H.DirectorTake > 0) { Sfx(DCSfx::Cash); }
			for (const FDCCrewMemberResult& M : H.Members) { if (M.bLevelledUp) { Sfx(DCSfx::StingLevelUp); break; } }
		}
	}
}

void SDCRoot::DailiesNext()
{
	TSharedPtr<FDailiesState> D = Dailies;
	if (D->bSummary) { FinishDailiesFlow(); return; }
	if (D->Report.Results.IsValidIndex(D->Index))
	{
		const FTimeline TL = MakeTimeline(D->Report.Results[D->Index]);
		if ((float)(Now() - D->T0) < TL.MembersAt)
		{
			// Skip the animation to its end state (sfx flags fire once, quietly).
			D->T0 = Now() - (TL.MembersAt + 0.5f);
			return;
		}
	}
	D->Index++;
	D->T0 = Now();
	D->bClap = D->bDice = D->bDice2 = D->bStamp = D->bMembers = D->bLuckySfx = false;
	if (D->Index >= D->Report.Results.Num()) { D->bSummary = true; }
	Sfx(DCSfx::PanelOpen);
	if (DailiesSlot.IsValid()) { DailiesSlot->SetContent(D->bSummary ? BuildDailiesSummary() : BuildDailiesCard()); }
}

void SDCRoot::DailiesRetake()
{
	TSharedPtr<FDailiesState> D = Dailies;
	UDCRunSubsystem* R = Run();
	if (!R || !D->Report.Results.IsValidIndex(D->Index)) { return; }
	FDCHeistResult New;
	if (R->UseRetake(D->Report.Results[D->Index].ResultId, &New))
	{
		New.bRetaken = true;
		D->Report.Results[D->Index] = New;
		D->T0 = Now();
		D->bClap = D->bDice = D->bDice2 = D->bStamp = D->bMembers = D->bLuckySfx = false;
		Toast(TEXT("Quiet on set... RETAKE!"), false);
		if (DailiesSlot.IsValid()) { DailiesSlot->SetContent(BuildDailiesCard()); }
	}
	else { Toast(TEXT("No retakes left tonight."), true); }
}

void SDCRoot::FinishDailiesFlow()
{
	UDCRunSubsystem* R = Run();
	const bool bLive = R && R->GetPhase() == EDCGamePhase::Dailies;
	Dailies->Report = FDCNightReport();
	Close(true);
	if (!R) { return; }
	if (bLive) { R->FinishDailies(); }
	const EDCGamePhase P = R->GetPhase();
	if (P == EDCGamePhase::GameOver)
	{
		if (Current != EDCPanel::GameOver) { if (!bHasEndSummary) { EndSummary = R->GetRunSummary(); bHasEndSummary = true; } Open(EDCPanel::GameOver); }
	}
	else if (P == EDCGamePhase::Won)
	{
		if (Current != EDCPanel::Credits) { if (!bHasEndSummary) { EndSummary = R->GetRunSummary(); bHasEndSummary = true; } Open(EDCPanel::Credits); }
	}
	else if (R->HasPendingPerkOffer() && Current != EDCPanel::PerkDraft)
	{
		Open(EDCPanel::PerkDraft);
	}
	bPerkPending = false;
}

// ─────────────────────────────────────────────────────────────────────────────
// Perk draft
// ─────────────────────────────────────────────────────────────────────────────

TSharedRef<SWidget> SDCRoot::BuildPerkDraft()
{
	UDCRunSubsystem* R = Run();
	const TArray<FDCPerk> Offer = R ? R->GetPerkOffer() : TArray<FDCPerk>();
	TSharedRef<SHorizontalBox> Cards = SNew(SHorizontalBox);
	for (const FDCPerk& P : Offer)
	{
		const FName Id = P.Id;
		const FString Name = P.DisplayName;
		Cards->AddSlot().FillWidth(1.f).Padding(8.f)
		[
			SNew(SBox).HeightOverride(330.f)
			[
				ButtonContent(
					SNew(SBorder).BorderImage(FStyleDefaults::GetNoBrush()).Padding(FMargin(18.f, 16.f))
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[Pips(3, 3, Gold, Gold, 8.f)]
						+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 10.f)[Txt(P.DisplayName, Display(28), Ink, true, ETextJustify::Center)]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 10.f)[HLine(WithAlpha(GoldDim, 0.6f))]
						+ SVerticalBox::Slot().AutoHeight()[Txt(P.Description, Sans(18), Ink, true, ETextJustify::Center)]
						+ SVerticalBox::Slot().FillHeight(1.f)[SNew(SSpacer)]
						+ SVerticalBox::Slot().AutoHeight()[Txt(P.Flavour, Type(15), InkSoft, true, ETextJustify::Center)]
						+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 12.f, 0.f, 0.f)[Chip(TEXT("SIGN IT"), Red, 15.f)]
					],
					[this, Id, Name]()
					{
						if (UDCRunSubsystem* RR = Run())
						{
							if (RR->PickPerk(Id)) { Sfx(DCSfx::StingPerk); Toast(FString::Printf(TEXT("Perk signed: %s"), *Name), false); Close(true); }
							else { Toast(TEXT("That perk isn't on offer any more."), true); }
						}
					}, EBtn::Paper, true)
			]
		];
	}
	TSharedRef<SWidget> Body = Offer.Num() ? StaticCastSharedRef<SWidget>(Cards)
		: StaticCastSharedRef<SWidget>(SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()[Txt(TEXT("No perks on offer right now. Every 3 successful heists, the studio sends a new contract."), Type(18), Cream, true)]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right).Padding(0.f, 16.f)[Button(TEXT("Close"), [this]() { Close(true); })]);

	return CenterModal(DecoFrame(
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[Txt(TEXT("THE STUDIO CALLS"), Display(44), Gold)]
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 4.f, 0.f, 14.f)
		[Txt(TEXT("Three heists in the can and the money men are impressed. Pick one perk for the rest of the run."), Type(17), Cream)]
		+ SVerticalBox::Slot().AutoHeight()[Body]), 1180.f);
}

// ─────────────────────────────────────────────────────────────────────────────
// Game over & credits
// ─────────────────────────────────────────────────────────────────────────────

TSharedRef<SWidget> SDCRoot::BuildGameOver()
{
	UDCRunSubsystem* R = Run();
	const FDCRunSummary S = bHasEndSummary ? EndSummary : (R ? R->GetRunSummary() : FDCRunSummary());
	const int32 Seed = S.Seed;
	auto Row = [](const FString& K, const FString& V)
	{
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Bottom)[Txt(K.ToUpper(), Sans(16), Muted)]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom)[Txt(V, Big(34), Cream)];
	};
	const double T0 = Now();

	return SNew(SOverlay)
		+ SOverlay::Slot()[SNew(SImage).Image(Brush(EBrush::White)).ColorAndOpacity(Rgb(0x12, 0x06, 0x04, 0xf4))]
		+ SOverlay::Slot().VAlign(VAlign_Top)[SNew(SBorder).BorderImage(Brush(EBrush::White)).BorderBackgroundColor(Rgb(0x05, 0x02, 0x01)).Padding(FMargin(0.f, 8.f))[Sprockets(48, 12.f)]]
		+ SOverlay::Slot().VAlign(VAlign_Bottom)[SNew(SBorder).BorderImage(Brush(EBrush::White)).BorderBackgroundColor(Rgb(0x05, 0x02, 0x01)).Padding(FMargin(0.f, 8.f))[Sprockets(48, 12.f)]]
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
		[
			SNew(SBox).WidthOverride(820.f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
				[
					SNew(SBox).RenderTransformPivot(FVector2D(0.5f, 0.5f))
					.RenderTransform_Lambda([T0]() { const float u = FMath::Clamp((float)(Now() - T0) / 0.25f, 0.f, 1.f); return TOptional<FSlateRenderTransform>(FSlateRenderTransform(1.f + 1.2f * FMath::Square(1.f - u))); })
					[Stamp(TEXT("CANCELLED"), RedBright, 130.f, -5.f)]
				]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 18.f, 0.f, 4.f)
				[Txt(S.EndReason.IsEmpty() ? FString(TEXT("The studio pulled the plug.")) : S.EndReason, Type(26), Cream, true, ETextJustify::Center)]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 0.f, 0.f, 22.f)
				[Txt(TEXT("Every picture ends. Some just end sooner."), Type(16), Muted)]
				+ SVerticalBox::Slot().AutoHeight()
				[
					DecoFrame(
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f)[Row(TEXT("Nights survived"), FString::FromInt(S.NightsSurvived))]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f)[Row(TEXT("Heists completed"), FString::Printf(TEXT("%d of %d"), S.HeistsCompleted, S.HeistsAttempted))]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f)[Row(TEXT("Total take"), Money(S.TotalTake))]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f)[Row(TEXT("Best heist"), S.BestHeistName.IsEmpty() ? FString(TEXT("-")) : FString::Printf(TEXT("%s  %s"), *S.BestHeistName, *Money(S.BestHeistTake)))]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f)[Row(TEXT("Rooms built"), FString::FromInt(S.RoomsBuilt))]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f)[Row(TEXT("Behind bars"), FString::FromInt(S.InMemoriam.Num()))]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 8.f, 0.f, 0.f)[HLine(WithAlpha(Gold, 0.4f))]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f, 0.f, 0.f)[Row(TEXT("Run seed"), FString::FromInt(Seed))], FMargin(22.f, 14.f))
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 24.f, 0.f, 0.f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.f).Padding(0.f, 0.f, 8.f, 0.f)[SNew(SBox).HeightOverride(56.f)[Button(TEXT("New Run"), [this]() { StartNewRun(); }, EBtn::Red, true, 22.f)]]
					+ SHorizontalBox::Slot().FillWidth(1.f).Padding(0.f, 0.f, 8.f, 0.f)
					[SNew(SBox).HeightOverride(56.f)[Button(TEXT("Replay this seed"), [this, Seed]()
					{
						StartNewRun(Seed);
					}, EBtn::Gold, true, 18.f)]]
					+ SHorizontalBox::Slot().FillWidth(0.7f)[SNew(SBox).HeightOverride(56.f)[Button(TEXT("Quit"), []() { FPlatformMisc::RequestExit(false, TEXT("DCGameOverQuit")); }, EBtn::Ghost, true, 16.f)]]
				]
			]
		];
}

TSharedRef<SWidget> SDCRoot::BuildCredits()
{
	UDCRunSubsystem* R = Run();
	const FDCRunSummary S = bHasEndSummary ? EndSummary : (R ? R->GetRunSummary() : FDCRunSummary());
	const double T0 = Now();

	TSharedRef<SVerticalBox> Roll = SNew(SVerticalBox);
	auto Line = [&Roll](TSharedRef<SWidget> W, float Pad = 4.f) { Roll->AddSlot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, Pad)[W]; };
	Line(Txt(TEXT("A  DIRECTOR'S  CUT  PRODUCTION"), Sans(22), Muted));
	Line(Txt(TEXT("THE PREMIERE"), Display(96), Gold), 10.f);
	Line(Txt(S.EndReason.IsEmpty() ? FString(TEXT("The Premiere was a smash.")) : S.EndReason, Type(24), Cream), 6.f);
	Line(SNew(SBox).HeightOverride(60.f), 0.f);
	Line(Txt(TEXT("STARRING"), Display(34), Gold), 10.f);
	for (const FString& N : S.Starring) { Line(Txt(N, Sans(26), Cream), 3.f); }
	if (S.Starring.Num() == 0) { Line(Txt(TEXT("(nobody walked away clean)"), Type(18), Muted)); }
	if (S.InMemoriam.Num())
	{
		Line(SNew(SBox).HeightOverride(40.f), 0.f);
		Line(Txt(TEXT("IN MEMORIAM"), Display(30), Muted), 10.f);
		Line(Txt(TEXT("currently serving time"), Type(16), Muted), 0.f);
		for (const FString& N : S.InMemoriam) { Line(Txt(N, Sans(22), WithAlpha(Cream, 0.7f)), 3.f); }
	}
	Line(SNew(SBox).HeightOverride(40.f), 0.f);
	Line(Txt(TEXT("BY THE NUMBERS"), Display(30), Gold), 10.f);
	Line(Txt(FString::Printf(TEXT("%d nights   -   %d of %d heists   -   %s total take"), S.NightsSurvived, S.HeistsCompleted, S.HeistsAttempted, *Money(S.TotalTake)), Sans(22), Cream));
	if (!S.BestHeistName.IsEmpty()) { Line(Txt(FString::Printf(TEXT("Best heist: %s (%s)"), *S.BestHeistName, *Money(S.BestHeistTake)), Sans(20), Cream)); }
	Line(Txt(FString::Printf(TEXT("Rooms built: %d"), S.RoomsBuilt), Sans(20), Cream));
	if (S.PerksTaken.Num())
	{
		Line(SNew(SBox).HeightOverride(30.f), 0.f);
		Line(Txt(TEXT("DIRECTOR'S PERKS"), Display(26), Gold), 8.f);
		for (const FString& P : S.PerksTaken) { Line(Txt(P, Sans(20), Cream), 2.f); }
	}
	Line(SNew(SBox).HeightOverride(40.f), 0.f);
	Line(Txt(FString::Printf(TEXT("Seed %d"), S.Seed), Sans(18), Muted));
	Line(SNew(SBox).HeightOverride(80.f), 0.f);
	Line(Txt(TEXT("THE END"), Display(64), Gold), 10.f);

	return SNew(SOverlay)
		+ SOverlay::Slot()[SNew(SImage).Image(Brush(EBrush::White)).ColorAndOpacity(Rgb(0x03, 0x02, 0x02, 0xfa))]
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Top)
		[
			SNew(SBox)
			.RenderTransform_Lambda([T0, Roll]()
			{
				// Roll up from below the screen; stop with THE END near the middle.
				const float H = Roll->GetDesiredSize().Y;
				const float Y = FMath::Max(760.f - (float)(Now() - T0) * 70.f, 420.f - H);
				return TOptional<FSlateRenderTransform>(FSlateRenderTransform(FVector2f(0.f, Y)));
			})
			[Roll]
		]
		+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Bottom).Padding(30.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 10.f, 0.f)[SNew(SBox).HeightOverride(52.f).MinDesiredWidth(200.f)[Button(TEXT("New Run"), [this]() { StartNewRun(); }, EBtn::Red, true, 20.f)]]
			+ SHorizontalBox::Slot().AutoWidth()[SNew(SBox).HeightOverride(52.f)[Button(TEXT("Quit"), []() { FPlatformMisc::RequestExit(false, TEXT("DCCreditsQuit")); }, EBtn::Ghost, true, 16.f)]]
		];
}

// Director's Cut — boot loading screen (see SDCLoading.h).
#include "UI/SDCLoading.h"
#include "UI/DCUIStyle.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SDPIScaler.h"
#include "UObject/UObjectGlobals.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "ContentStreaming.h"
#include "PipelineStateCache.h"
#include "ShaderPipelineCache.h"
#if WITH_EDITOR
#include "ShaderCompiler.h"
#endif
#include "DirectorsCut.h"

using namespace DCUI;

namespace
{
	constexpr double MinShow = 1.2;        // never flash the loading screen for a frame or two
	constexpr double QuietFor = 0.8;       // everything idle this long = ready (Lumen / shadows settle meanwhile)
	constexpr double TextureGrace = 8.0;   // textures alone still pending this long after the rest: go anyway
	constexpr double MaxWait = 120.0;      // hard cap (a cold shader cache in the editor can take a while)
	constexpr int32 MinFrames = 20;        // the world has to have actually rendered for streaming to know what it needs
	constexpr double ContentFade = 0.25;
	constexpr double BackdropFade = 0.6;

	const FLinearColor Backdrop(0.01f, 0.012f, 0.016f, 1.f);   // same ink as the title shade
}

SDCLoading::FPending SDCLoading::Poll()
{
	FPending P;
	P.AsyncLoading = IsAsyncLoading() ? 1 : 0;
#if WITH_EDITOR
	if (GShaderCompilingManager) { P.Shaders = GShaderCompilingManager->GetNumRemainingJobs(); }
#endif
	P.Pipelines = (int32)PipelineStateCache::NumActivePrecacheRequests() + (int32)FShaderPipelineCache::NumPrecompilesRemaining();
	P.Textures = IStreamingManager::Get().GetNumWantingResources();
	return P;
}

void SDCLoading::Construct(const FArguments& InArgs)
{
	OnReveal = InArgs._OnReveal;
	OnFinished = InArgs._OnFinished;
	T0 = Now();
	bHold = FParse::Param(FCommandLine::Get(), TEXT("DCHoldLoading"));
	Status = TEXT("Loading");
	SetVisibility(EVisibility::Visible);   // swallow clicks until the reveal

	auto Fade = [this]() { return FLinearColor(1, 1, 1, ContentAlpha()); };
	const bool bCalm = Prefs().bReduceMotion;

	ChildSlot
	[
		SNew(SBorder).BorderImage(WhiteBrush()).Padding(0.f)
		.BorderBackgroundColor_Lambda([this]() { return A(Backdrop, Alpha()); })
		[
			SNew(SDPIScaler).DPIScale_Lambda([]() { return Prefs().UIScale; })
			[
				SNew(SOverlay)
				// wordmark + progress, placed where the title screen puts its wordmark
				+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Center).Padding(FMargin(120.f, 0.f, 0.f, 40.f))
				[
					SNew(SBorder).BorderImage(NoBrush()).Padding(0.f).ColorAndOpacity_Lambda(Fade)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Left)
						[
							SNew(SBox).WidthOverride(56.f).HeightOverride(3.f)
							[
								SNew(SImage).Image(WhiteBrush())
								.ColorAndOpacity_Lambda([bCalm]() { return FSlateColor(A(Accent, bCalm ? 1.f : 0.55f + 0.45f * Pulse(0.8f))); })
							]
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 22.f, 0.f, 0.f)[Txt(TEXT("DIRECTOR'S"), Font(46.f, EW::Bold, 420), Text)]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.f, -8.f, 0.f, 0.f)[Txt(TEXT("CUT"), Font(46.f, EW::Bold, 420), Accent)]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 40.f, 0.f, 10.f)
						[
							SNew(SBox).WidthOverride(260.f)
							[
								SNew(SHorizontalBox)
								+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
								[TxtA(TAttribute<FText>::CreateLambda([this]() { return FText::FromString(Status.ToUpper()); }), Caps(8.f), FSlateColor(TextDim))]
								+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
								[TxtA(TAttribute<FText>::CreateLambda([this]() { return FText::FromString(FString::Printf(TEXT("%d%%"), FMath::FloorToInt(Progress01() * 100.f))); }), Mono(9.f), FSlateColor(TextFaint))]
							]
						]
						+ SVerticalBox::Slot().AutoHeight()
						[
							SNew(SBox).WidthOverride(260.f)[Bar(TAttribute<float>::CreateLambda([this]() { return Progress01(); }), FSlateColor(Accent), 3.f)]
						]
					]
				]
				+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Bottom).Padding(FMargin(120.f, 0.f, 0.f, 40.f))
				[
					SNew(SBorder).BorderImage(NoBrush()).Padding(0.f).ColorAndOpacity_Lambda(Fade)
					[Txt(TEXT("EPIC MEGAJAM 2026"), Caps(8.f), TextFaint)]
				]
			]
		]
	];
}

float SDCLoading::Progress01() const
{
	return RevealT0 >= 0.0 ? 1.f : Shown;
}

float SDCLoading::Alpha() const
{
	if (RevealT0 < 0.0) { return 1.f; }
	const float T = (float)((Now() - RevealT0 - ContentFade * 0.6) / BackdropFade);
	return 1.f - EaseInOut(FMath::Clamp(T, 0.f, 1.f));
}

float SDCLoading::ContentAlpha() const
{
	const float In = EaseOut(FMath::Clamp((float)(Now() - T0) / 0.35f, 0.f, 1.f));
	if (RevealT0 < 0.0) { return In; }
	return In * (1.f - FMath::Clamp((float)((Now() - RevealT0) / ContentFade), 0.f, 1.f));
}

void SDCLoading::Reveal(const TCHAR* Why)
{
	if (RevealT0 >= 0.0) { return; }
	RevealT0 = Now();
	Status = TEXT("Ready");
	SetVisibility(EVisibility::HitTestInvisible);
	UE_LOG(LogDC, Display, TEXT("[Loading] ready after %.2fs, %d frames (%s); peaks: shaders %d, pipelines %d, textures %d"),
		RevealT0 - T0, Frames, Why, Peak.Shaders, Peak.Pipelines, Peak.Textures);
	OnReveal.ExecuteIfBound();
}

void SDCLoading::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
	const double T = Now();

	if (RevealT0 >= 0.0)
	{
		if (!bFinished && T - RevealT0 > ContentFade * 0.6 + BackdropFade)
		{
			bFinished = true;
			OnFinished.ExecuteIfBound();
		}
		return;
	}

	++Frames;
	Cur = Poll();
	Peak.AsyncLoading = FMath::Max(Peak.AsyncLoading, Cur.AsyncLoading);
	Peak.Shaders = FMath::Max(Peak.Shaders, Cur.Shaders);
	Peak.Pipelines = FMath::Max(Peak.Pipelines, Cur.Pipelines);
	Peak.Textures = FMath::Max(Peak.Textures, Cur.Textures);

	// Status: the most expensive thing still going.
	if (Cur.AsyncLoading) { Status = TEXT("Loading assets"); }
	else if (Cur.Shaders) { Status = FString::Printf(TEXT("Compiling shaders · %d"), Cur.Shaders); }
	else if (Cur.Pipelines) { Status = TEXT("Preparing materials"); }
	else if (Cur.Textures) { Status = TEXT("Streaming textures"); }
	else { Status = TEXT("Setting the scene"); }

	// Quiet = nothing pending (textures may be left over if the rest has long been idle: a small pool can keep them wanting).
	const bool bOthersQuiet = Cur.AsyncLoading + Cur.Shaders + Cur.Pipelines == 0;
	if (bOthersQuiet && Cur.Textures == 0) { if (QuietSince < 0.0) { QuietSince = T; } }
	else { QuietSince = -1.0; }
	if (bOthersQuiet && Cur.Textures > 0) { if (TextureStallT0 < 0.0) { TextureStallT0 = T; } }
	else { TextureStallT0 = -1.0; }

	// Progress: each stage's share done (vs its peak), plus the settle time and a little for elapsed frames.
	auto Done = [](int32 C, int32 P) { return P <= 0 ? 1.f : 1.f - (float)C / (float)P; };
	const float Work = (Done(Cur.Shaders, Peak.Shaders) * 3.f + Done(Cur.Pipelines, Peak.Pipelines) + Done(Cur.Textures, Peak.Textures) * 2.f + (Cur.AsyncLoading ? 0.f : 1.f)) / 7.f;
	const float Warm = FMath::Clamp((float)Frames / (float)MinFrames, 0.f, 1.f);
	const float Settle = QuietSince >= 0.0 ? FMath::Clamp((float)((T - QuietSince) / QuietFor), 0.f, 1.f) : 0.f;
	const float Target = FMath::Min(0.98f, 0.1f * Warm + 0.75f * Work * Warm + 0.15f * Settle);
	Shown = FMath::Max(Shown, FMath::FInterpTo(Shown, Target, InDeltaTime, 6.f));

	const double Elapsed = T - T0;
	if (bHold) { return; }
	if (Elapsed >= MinShow && Frames >= MinFrames && QuietSince >= 0.0 && T - QuietSince >= QuietFor) { Reveal(TEXT("idle")); }
	else if (Elapsed >= MaxWait) { Reveal(TEXT("timed out")); }
	else if (Elapsed >= MinShow && Frames >= MinFrames && TextureStallT0 >= 0.0 && T - TextureStallT0 >= TextureGrace) { Reveal(TEXT("textures still streaming")); }
}

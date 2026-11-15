// Director's Cut — video reel player (-DCReel=<file>). Part of UDCDevTools (see DCDevTools.h).
//
// Plays a shot list for the trailer / jam video: starts on the title screen, then jumps through a seeded run, holding
// each clip for a few seconds with an optional smooth camera move and scripted UI actions. Every clip fades in from
// black and out again; between clips (behind the black) the bot plays the run forward and the world settles. The
// recorder (Scripts/record_video_*) films the window; reel_cues.csv says where each clip's clean part is.
//
// Reel file (one step per line, # starts a comment): see Scripts/video_reel.txt for the full reference.
//   seconds 10                        clean length of every following clip
//   fade 0.6                          fade in/out seconds (0 = hard cuts)
//   newrun 7                          start a run with this seed (from the title, like New Run)
//   play 6000 15                      bot plays until run time 6000 s, then on to 15:00
//   endrun win|lose                   end the run (the end screen opens)
//   clip <name> <camera> [ui=<Panel>] [hud=off] [len=12] [<t>:<action>[=<arg>]]...
//        camera = <focus>[@zoom] or <from>><to> (an eased move across the whole clip)
//        focus  = wide | far | yard | house | crew | follow | <room type> (as -DCShots; follow tracks a walking crew member)
//        zoom   = 0 overview .. 1 closest; below 0 zooms out past the overview (-1 = as far as the player may)
//        action at <t> seconds into the clean part: open=<Panel> close autofill start cut=<x> crew=<i> gear=<i> perk=<i>
//               pickperk perks hire view=<sub view> speed=<i> hud=on|off look=<focus>[@zoom] lapse=<game hours>
//               place=<Room|any> click
#include "Core/DCDevTools.h"
#include "Core/DCRunSubsystem.h"
#include "Core/DCUISubsystem.h"
#include "Core/DCSiteLayout.h"
#include "World/DCPlayerController.h"
#include "World/DCCameraRig.h"
#include "World/DCBuildPlacer.h"
#include "World/DCCriminalPawn.h"
#include "World/DCRoomModule.h"
#include "World/DCWorldDirector.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/SWindow.h"
#include "DirectorsCut.h"

namespace
{
	// Ease in and out with zero velocity and acceleration at both ends (camera moves, cursor glides).
	float Smoother(float X) { X = FMath::Clamp(X, 0.f, 1.f); return X * X * X * (X * (X * 6.f - 15.f) + 10.f); }

	// Distances blend in log space so a zoom reads as an even push-in rather than one that rushes at the end.
	float BlendDistance(float A, float B, float T) { return FMath::Exp(FMath::Lerp(FMath::Loge(FMath::Max(A, 1.f)), FMath::Loge(FMath::Max(B, 1.f)), T)); }

	constexpr double LookSeconds = 2.2;   // look=<focus> camera move
	constexpr double PlaceHop = 1.15;      // placement cursor: seconds per cell
	constexpr double PlaceDwell = 0.45;    // ... and the pause on each
}

ADCCameraRig* UDCDevTools::GetRig() const
{
	UWorld* W = GetGameWorld();
	ADCPlayerController* PC = W ? Cast<ADCPlayerController>(W->GetFirstPlayerController()) : nullptr;
	return PC ? PC->GetCameraRig() : nullptr;
}

// ─────────────────────────────────────────────────────────────────────────────
// Reel file
// ─────────────────────────────────────────────────────────────────────────────

bool UDCDevTools::LoadReel()
{
	TArray<FString> Lines;
	if (!FFileHelper::LoadFileToStringArray(Lines, *ReelPath))
	{
		UE_LOG(LogDC, Error, TEXT("[Reel] can't read %s"), *ReelPath);
		return false;
	}
	int32 ReelSeed = 0;
	FParse::Value(FCommandLine::Get(), TEXT("DCReelSeed="), ReelSeed);
	int32 LineNo = 0;
	for (FString L : Lines)
	{
		++LineNo;
		int32 Hash = INDEX_NONE;
		if (L.FindChar(TEXT('#'), Hash)) { L.LeftInline(Hash); }
		TArray<FString> W;
		L.ParseIntoArrayWS(W);
		if (W.Num() == 0) { continue; }
		FReelStep S;
		S.Kind = W[0].ToLower();
		S.Line = LineNo;
		S.Args = TArray<FString>(W.GetData() + 1, W.Num() - 1);
		if (S.Kind == TEXT("clip"))
		{
			if (S.Args.Num() < 2) { UE_LOG(LogDC, Warning, TEXT("[Reel] line %d: clip needs a name and a camera"), LineNo); continue; }
			S.Name = S.Args[0];
			if (!S.Args[1].Split(TEXT(">"), &S.CamFrom, &S.CamTo)) { S.CamFrom = S.Args[1]; }
			for (int32 i = 2; i < S.Args.Num(); ++i)
			{
				const FString& T = S.Args[i];
				FString K, V;
				if (T.StartsWith(TEXT("ui="))) { S.Panel = T.Mid(3); }
				else if (T.StartsWith(TEXT("hud="))) { S.bHudOff = T.Mid(4) == TEXT("off"); }
				else if (T.StartsWith(TEXT("len="))) { S.Length = FCString::Atof(*T.Mid(4)); }
				else if (T.Split(TEXT(":"), &K, &V) && K.IsNumeric())
				{
					FReelAction A;
					A.At = FCString::Atof(*K);
					if (!V.Split(TEXT("="), &A.Verb, &A.Arg)) { A.Verb = V; }
					S.Actions.Add(A);
				}
				else { UE_LOG(LogDC, Warning, TEXT("[Reel] line %d: don't know '%s'"), LineNo, *T); }
			}
			S.Actions.StableSort([](const FReelAction& A, const FReelAction& B) { return A.At < B.At; });
		}
		else if (S.Kind == TEXT("newrun") && ReelSeed != 0) { S.Args = { FString::FromInt(ReelSeed) }; }
		else if (S.Kind != TEXT("seconds") && S.Kind != TEXT("fade") && S.Kind != TEXT("newrun") && S.Kind != TEXT("play") && S.Kind != TEXT("endrun"))
		{
			UE_LOG(LogDC, Warning, TEXT("[Reel] line %d: unknown step '%s'"), LineNo, *S.Kind);
			continue;
		}
		Reel.Add(S);
	}
	int32 Clips = 0;
	for (const FReelStep& S : Reel) { Clips += S.Kind == TEXT("clip") ? 1 : 0; }
	UE_LOG(LogDC, Display, TEXT("[Reel] %s: %d steps, %d clips"), *ReelPath, Reel.Num(), Clips);
	if (Clips == 0) { Reel.Reset(); }
	return Reel.Num() > 0;
}

// ─────────────────────────────────────────────────────────────────────────────
// Output: window rect, cues, fade
// ─────────────────────────────────────────────────────────────────────────────

void UDCDevTools::ReelWriteWindowRect()
{
	TSharedPtr<SWindow> Win = GEngine && GEngine->GameViewport ? GEngine->GameViewport->GetWindow() : nullptr;
	if (!Win.IsValid()) { return; }
	const FSlateRect R = Win->GetClientRectInScreen();
	const float Dpi = Win->GetDPIScaleFactor();
	// Client area in desktop pixels: x y width height dpi-scale fullscreen
	const FString Text = FString::Printf(TEXT("%d %d %d %d %.3f %d\n"), FMath::RoundToInt(R.Left), FMath::RoundToInt(R.Top),
		FMath::RoundToInt(R.Right - R.Left), FMath::RoundToInt(R.Bottom - R.Top), Dpi, Win->GetWindowMode() != EWindowMode::Windowed ? 1 : 0);
	FFileHelper::SaveStringToFile(Text, *(ShotDir / TEXT("reel_window.txt")));
	UE_LOG(LogDC, Display, TEXT("[Reel] window %s"), *Text.TrimEnd());
}

void UDCDevTools::ReelWriteCues()
{
	FString Text = TEXT("kind,name,start,end\n");
	for (const FString& C : Cues) { Text += C + TEXT("\n"); }
	FFileHelper::SaveStringToFile(Text, *(ShotDir / TEXT("reel_cues.csv")));
}

void UDCDevTools::ReelSetFade(float Alpha)
{
	FadeAlpha = FMath::Clamp(Alpha, 0.f, 1.f);
	if (FadeWidget.IsValid() || !GEngine || !GEngine->GameViewport) { return; }
	TWeakObjectPtr<UDCDevTools> WeakThis(this);
	FadeWidget = SNew(SImage)
		.Image(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
		.ColorAndOpacity_Lambda([WeakThis]() { return FSlateColor(FLinearColor(0.f, 0.f, 0.f, WeakThis.IsValid() ? WeakThis->FadeAlpha : 1.f)); })
		.Visibility(EVisibility::HitTestInvisible);
	GEngine->GameViewport->AddViewportWidgetContent(FadeWidget.ToSharedRef(), 1000);
}

void UDCDevTools::ReelParkCursor()
{
	// Keep the mouse off the game: hover highlights and tooltips would show up in the video.
	if (!FSlateApplication::IsInitialized() || !GEngine || !GEngine->GameViewport) { return; }
	TSharedPtr<SWindow> Win = GEngine->GameViewport->GetWindow();
	if (!Win.IsValid()) { return; }
	const FSlateRect R = Win->GetClientRectInScreen();
	const FVector2D P = FSlateApplication::Get().GetCursorPos();
	if (P.X >= R.Left - 4 && P.X <= R.Right + 4 && P.Y >= R.Top - 4 && P.Y <= R.Bottom + 4)
	{
		FSlateApplication::Get().SetCursorPos(FVector2D(R.Right + 80.f, R.Bottom + 80.f));
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Camera
// ─────────────────────────────────────────────────────────────────────────────

bool UDCDevTools::ReelResolvePose(const FString& Spec, FCamPose& Out, bool* bOutFollow)
{
	ADCCameraRig* Rig = GetRig();
	if (!Rig) { return false; }
	FString Focus = Spec, ZoomStr;
	float Zoom = 0.f;
	const bool bZoom = Spec.Split(TEXT("@"), &Focus, &ZoomStr);
	if (bOutFollow) { *bOutFollow = false; }
	if (Focus.Equals(TEXT("wide"), ESearchCase::IgnoreCase)) { Out.Pivot = Rig->GetOverviewPivot(); Zoom = 0.f; }
	else if (Focus.Equals(TEXT("far"), ESearchCase::IgnoreCase)) { Out.Pivot = Rig->GetOverviewPivot(); Zoom = -1.f; }
	else if (Focus.Equals(TEXT("follow"), ESearchCase::IgnoreCase))
	{
		// Someone on the move, preferably indoors near the middle of the house.
		UWorld* W = GetGameWorld();
		ADCCriminalPawn* Best = nullptr;
		float BestScore = -1.f;
		for (TActorIterator<ADCCriminalPawn> It(W); It; ++It)
		{
			if (It->IsHidden() || It->IsSleeping()) { continue; }
			const float Score = It->GetVelocity().Size2D() + (It->IsWalkingOut() ? 0.f : 50.f) - It->GetActorLocation().Size2D() * 0.01f;
			if (Score > BestScore) { BestScore = Score; Best = *It; }
		}
		if (!Best) { return ReelResolvePose(TEXT("house") + (bZoom ? TEXT("@") + ZoomStr : FString()), Out); }
		FollowActor = Best;
		Out.Pivot = Best->GetActorLocation() * FVector(1, 1, 0);
		Zoom = 0.9f;
		if (bOutFollow) { *bOutFollow = true; }
	}
	else if (!ResolveFocus(Focus, Out.Pivot, Zoom))
	{
		UE_LOG(LogDC, Warning, TEXT("[Reel] unknown camera focus '%s'"), *Spec);
		Out.Pivot = Rig->GetOverviewPivot(); Zoom = 0.f;
	}
	if (bZoom) { Zoom = FMath::Clamp(FCString::Atof(*ZoomStr), -1.f, 1.f); }
	Out.Distance = Rig->DistanceForZoom(Zoom);
	return true;
}

void UDCDevTools::ReelApplyCamera()
{
	ADCCameraRig* Rig = GetRig();
	if (!Rig) { return; }
	const FReelStep& Clip = Reel[ReelIndex];
	const double Span = FMath::Max(0.1, (double)ReelFade * 2.0 + (Clip.Length > 0.f ? Clip.Length : ReelSeconds));
	FCamPose P = PoseFrom;
	if (bFollow)
	{
		// Glide after the walker (a soft spring, so steps and turns don't jerk the frame).
		if (AActor* A = FollowActor.Get()) { FollowPivot = FMath::Lerp(FollowPivot, A->GetActorLocation() * FVector(1, 1, 0), 1.f - FMath::Exp(-FMath::Min((float)FApp::GetDeltaTime(), 0.1f) * 1.8f)); }
		P.Pivot = FollowPivot;
	}
	else if (bPan)
	{
		const float T = Smoother((float)(ClipClock / Span));
		P.Pivot = FMath::Lerp(PoseFrom.Pivot, PoseTo.Pivot, T);
		P.Distance = BlendDistance(PoseFrom.Distance, PoseTo.Distance, T);
	}
	if (bLook)
	{
		const float T = Smoother((float)((ClipClock - LookT0) / LookSeconds));
		P.Pivot = FMath::Lerp(LookFrom.Pivot, LookTo.Pivot, T);
		P.Distance = BlendDistance(LookFrom.Distance, LookTo.Distance, T);
	}
	LastPose = P;
	Rig->DevSetView(P.Pivot, P.Distance);
}

// ─────────────────────────────────────────────────────────────────────────────
// Steps
// ─────────────────────────────────────────────────────────────────────────────

void UDCDevTools::ReelSetup()
{
	UWorld* W = GetGameWorld();
	UDCRunSubsystem* Run = UDCRunSubsystem::Get(W);
	ReelSettle = 0.6;
	while (Reel.IsValidIndex(ReelIndex) && Reel[ReelIndex].Kind != TEXT("clip"))
	{
		const FReelStep& S = Reel[ReelIndex];
		const FString A0 = S.Args.IsValidIndex(0) ? S.Args[0] : FString();
		const FString A1 = S.Args.IsValidIndex(1) ? S.Args[1] : FString();
		if (S.Kind == TEXT("seconds")) { ReelSeconds = FMath::Max(1.f, FCString::Atof(*A0)); }
		else if (S.Kind == TEXT("fade")) { ReelFade = FMath::Max(0.f, FCString::Atof(*A0)); }
		else if (S.Kind == TEXT("newrun") && Run)
		{
			Run->StartNewRun(FCString::Atoi(*A0));
			ReelSettle = FMath::Max(ReelSettle, 2.5);
			UE_LOG(LogDC, Display, TEXT("[Reel] new run, seed %d"), Run->GetSeed());
		}
		else if (S.Kind == TEXT("play") && Run)
		{
			if (!Run->HasActiveRun()) { Run->StartNewRun(0); }
			const double Until = FCString::Atod(*A0);
			RunAutoPlay(FMath::Max(0.0, Until - Run->GetRunTime()), A1.IsEmpty() ? -1.f : FCString::Atof(*A1), true);
			// Crew walk to where the rebuilt world puts them, lights and streaming catch up.
			ReelSettle = FMath::Max(ReelSettle, 3.0);
		}
		else if (S.Kind == TEXT("endrun") && Run)
		{
			if (Run->HasActiveRun()) { Run->DevEndRun(!A0.Equals(TEXT("lose"), ESearchCase::IgnoreCase)); }
			ReelSettle = FMath::Max(ReelSettle, 1.5);
		}
		++ReelIndex;
	}
	// A payday during a clip mustn't end the run either.
	if (Run && Run->HasActiveRun() && Run->GetPhase() == EDCGamePhase::Playing && Run->GetCash() < Run->GetUpkeepDue() + 500)
	{
		Run->DevAddCash(Run->GetUpkeepDue() + 500 - Run->GetCash());
	}
	if (Reel.IsValidIndex(ReelIndex)) { ReelPrepareClip(Reel[ReelIndex]); }
}

void UDCDevTools::ReelPrepareClip(FReelStep& Clip)
{
	UWorld* W = GetGameWorld();
	UDCRunSubsystem* Run = UDCRunSubsystem::Get(W);
	UDCUISubsystem* UI = UDCUISubsystem::Get(W);
	for (FReelAction& A : Clip.Actions) { A.bDone = false; }
	bLook = false; bLapse = false; bPlacePending = false; PlaceT0 = -1.0; PlacePath.Reset();
	ClipClock = 0.0;

	// A perk offer that turned up on its own would pop over a clip that isn't about it.
	const bool bPerkClip = Clip.Panel == TEXT("PerkDraft") || Clip.Actions.ContainsByPredicate([](const FReelAction& A) { return A.Verb == TEXT("perks"); });
	if (Run && !bPerkClip) { while (Run->HasPendingPerkOffer()) { const TArray<FDCPerk> O = Run->GetPerkOffer(); if (!O.Num() || !Run->PickPerk(O[0].Id)) { break; } } }

	// A clip that places a room needs a free build slot: let a build in progress finish first (behind the black).
	if (Run && Run->HasActiveRun() && Clip.Actions.ContainsByPredicate([](const FReelAction& A) { return A.Verb == TEXT("place"); }) && Run->IsBuildCapacityFull())
	{
		for (int32 i = 0; i < 400 && Run->IsBuildCapacityFull() && Run->GetPhase() == EDCGamePhase::Playing; ++i) { Run->AdvanceTime(5.0); }
		for (TActorIterator<ADCWorldDirector> It(W); It; ++It) { It->DevResync(); }
		ReelSettle = FMath::Max(ReelSettle, 3.0);
	}

	if (UI)
	{
		UI->DevAction(TEXT("hud"), Clip.bHudOff ? TEXT("off") : TEXT("on"));
		if (Clip.Panel.IsEmpty()) { UI->ClosePanel(); }
		else if (Clip.Panel == TEXT("MissionPlanner") && Run)
		{
			// A job the crew can actually take on, so autofill/start in the clip work.
			const FDCMission* Pick = nullptr;
			for (const FDCMission& M : Run->GetMissions())
			{
				if (M.bInProgress || M.bFinal) { continue; }
				if (!Pick) { Pick = &M; }
				const FDCSuccessBreakdown B = Run->PreviewPlan(Run->SuggestPlan(M.Id, 0.3f));
				if (B.bValid && B.SuccessChance >= 0.5f) { Pick = &M; break; }
			}
			if (Pick) { UI->OpenMissionPlanner(Pick->Id); }
		}
		else
		{
			const int64 P = StaticEnum<EDCPanel>()->GetValueByNameString(Clip.Panel);
			if (P == INDEX_NONE) { UE_LOG(LogDC, Warning, TEXT("[Reel] %s: unknown panel '%s'"), *Clip.Name, *Clip.Panel); }
			else if ((EDCPanel)P != UI->GetOpenPanel()) { UI->OpenPanel((EDCPanel)P); }
		}
	}

	ReelResolveClipCamera(false);
	UE_LOG(LogDC, Display, TEXT("[Reel] clip %s (%s%s%s)"), *Clip.Name, *Clip.CamFrom, bPan ? TEXT(" > ") : TEXT(""), *Clip.CamTo);
}

void UDCDevTools::ReelResolveClipCamera(bool bLog)
{
	const FReelStep& Clip = Reel[ReelIndex];
	ReelResolvePose(Clip.CamFrom, PoseFrom, &bFollow);
	FollowPivot = PoseFrom.Pivot;
	bPan = !bFollow && !Clip.CamTo.IsEmpty() && ReelResolvePose(Clip.CamTo, PoseTo);
	ReelApplyCamera();
	if (const ADCCameraRig* Rig = GetRig(); Rig && bLog)
	{
		UE_LOG(LogDC, Display, TEXT("[Reel]   camera: overview %.0f (min %.0f), from %.0f at %s, to %.0f at %s"), Rig->GetOverviewDistance(), Rig->MinDistance,
			PoseFrom.Distance, *PoseFrom.Pivot.ToCompactString(), bPan ? PoseTo.Distance : 0.f, bPan ? *PoseTo.Pivot.ToCompactString() : TEXT("-"));
	}
}

void UDCDevTools::ReelEndClip()
{
	UWorld* W = GetGameWorld();
	if (UDCRunSubsystem* Run = UDCRunSubsystem::Get(W))
	{
		if (bLapse) { Run->SetTimeScale(SavedTimeScale); bLapse = false; }
		if (Run->HasActiveRun() && (Run->GetGameSpeed() != 0 || Run->IsSpeedPaused())) { Run->SetGameSpeed(0); }
	}
	if (W)
	{
		if (ADCPlayerController* PC = Cast<ADCPlayerController>(W->GetFirstPlayerController()))
		{
			if (ADCBuildPlacer* Placer = PC->GetBuildPlacer()) { Placer->DevSetCursor({}); if (Placer->IsActive()) { Placer->Cancel(true); } }
		}
	}
	bPlacePending = false;
	// Behind the black: leave no options card or confirm dialog over the next clip.
	if (UDCUISubsystem* UI = UDCUISubsystem::Get(W)) { UI->DevAction(TEXT("view"), TEXT("none")); }
}

void UDCDevTools::ReelRunAction(FReelAction& A)
{
	A.bDone = true;
	UWorld* W = GetGameWorld();
	UDCRunSubsystem* Run = UDCRunSubsystem::Get(W);
	UDCUISubsystem* UI = UDCUISubsystem::Get(W);
	ADCPlayerController* PC = W ? Cast<ADCPlayerController>(W->GetFirstPlayerController()) : nullptr;
	const FReelStep& Clip = Reel[ReelIndex];
	bool bOk = true;
	if (A.Verb == TEXT("open") && UI)
	{
		const int64 P = StaticEnum<EDCPanel>()->GetValueByNameString(A.Arg);
		bOk = P != INDEX_NONE;
		if (bOk) { UI->OpenPanel((EDCPanel)P); }
	}
	else if (A.Verb == TEXT("hire") && Run)
	{
		const TArray<FDCCriminal>& Recruits = Run->GetRecruits();
		const FDCCriminal* Pick = nullptr;
		for (const FDCCriminal& C : Recruits) { if (C.HireCost <= Run->GetCash()) { Pick = &C; break; } }
		if (!Pick && Recruits.Num()) { Pick = &Recruits[0]; Run->DevAddCash(Pick->HireCost - Run->GetCash() + 50); }
		bOk = Pick && Run->Hire(Pick->Id);
	}
	else if (A.Verb == TEXT("perks") && Run) { Run->DevOfferPerks(); if (UI && UI->GetOpenPanel() != EDCPanel::PerkDraft) { UI->OpenPanel(EDCPanel::PerkDraft); } }
	else if (A.Verb == TEXT("look"))
	{
		LookFrom = LastPose;
		bOk = ReelResolvePose(A.Arg, LookTo);
		bLook = bOk; bFollow = false; LookT0 = ClipClock;
	}
	else if (A.Verb == TEXT("lapse") && Run)
	{
		// Squeeze <arg> game hours into what is left of the clip.
		const float Left = FMath::Max(1.f, (Clip.Length > 0.f ? Clip.Length : ReelSeconds) + ReelFade - A.At);
		const float RunSeconds = FCString::Atof(*A.Arg) * 60.f / FMath::Max(0.01f, Run->GetGameMinutesPerSecond());
		if (!bLapse) { SavedTimeScale = Run->GetTimeScale(); }
		Run->SetTimeScale(RunSeconds / Left / FMath::Max(0.01f, Run->GetGameSpeedMultiplier()));
		bLapse = true;
	}
	else if (A.Verb == TEXT("place") && Run && UI)
	{
		// Pick the room, top the bank up if it's short (this is a demo of the placement, not of saving up), start placing.
		EDCRoomType Room = EDCRoomType::Office;
		bool bFound = false;
		const int64 V = StaticEnum<EDCRoomType>()->GetValueByNameString(A.Arg);
		for (const FDCRoom& R : Run->GetRooms())
		{
			const bool bWanted = V != INDEX_NONE ? (int64)R.Type == V : true;
			if (bWanted && R.Level == 0 && R.State == EDCRoomState::Locked && (R.bCanAct || R.Block == EDCPlanBlock::NoCash)) { Room = R.Type; bFound = true; break; }
		}
		bOk = bFound && !Run->IsBuildCapacityFull();
		if (!bOk)
		{
			for (const FDCRoom& R : Run->GetRooms())
			{
				UE_LOG(LogDC, Display, TEXT("[Reel]   %s: level %d state %d block %d cost $%d (cash $%d, build slots full %d)"), *R.DisplayName, R.Level, (int32)R.State, (int32)R.Block, R.NextCost, Run->GetCash(), Run->IsBuildCapacityFull() ? 1 : 0);
			}
		}
		if (bOk)
		{
			const FDCRoom R = Run->GetRoom(Room);
			if (R.NextCost > Run->GetCash()) { Run->DevAddCash(R.NextCost - Run->GetCash() + 250); }
			UI->RequestRoomPlacement(Room);
			bPlacePending = true;
			PlaceT0 = ClipClock;
		}
	}
	else if (A.Verb == TEXT("click"))
	{
		ADCBuildPlacer* Placer = PC ? PC->GetBuildPlacer() : nullptr;
		bOk = Placer && Placer->IsActive();
		if (bOk) { Placer->Click(); Placer->DevSetCursor({}); bPlacePending = false; }
	}
	else if (UI)
	{
		// autofill start cut crew gear perk pickperk view speed hud close
		bOk = UI->DevAction(A.Verb, A.Arg);
	}
	if (!bOk) { UE_LOG(LogDC, Warning, TEXT("[Reel] %s: action %s%s%s at %.1fs did nothing"), *Clip.Name, *A.Verb, A.Arg.IsEmpty() ? TEXT("") : TEXT("="), *A.Arg, A.At); }
}

void UDCDevTools::ReelTickPlace()
{
	UWorld* W = GetGameWorld();
	ADCPlayerController* PC = W ? Cast<ADCPlayerController>(W->GetFirstPlayerController()) : nullptr;
	ADCBuildPlacer* Placer = PC ? PC->GetBuildPlacer() : nullptr;
	if (!bPlacePending || !Placer || !Placer->IsActive()) { return; }
	if (PlacePath.Num() == 0)
	{
		// The grid fills in on the placer's first update. Glide over a few free cells and settle on the default spot.
		const TArray<FIntPoint>& Valid = Placer->GetValidCells();
		if (Valid.Num() == 0) { return; }
		const UDCRunSubsystem* Run = UDCRunSubsystem::Get(W);
		const FIntPoint Final = Run ? DCSite::Grid::AutoPick(Run->GetOccupiedCells()) : Valid[0];
		const FIntPoint Target = Valid.Contains(Final) ? Final : Valid[0];
		TArray<FIntPoint> Others = Valid;
		Others.Remove(Target);
		const FVector2D TC = DCSite::Grid::Centre(Target);
		Others.Sort([&TC](const FIntPoint& A, const FIntPoint& B) { return FVector2D::DistSquared(DCSite::Grid::Centre(A), TC) > FVector2D::DistSquared(DCSite::Grid::Centre(B), TC); });
		if (Others.Num()) { PlacePath.Add(Others[0]); }                 // start on the far side
		if (Others.Num() > 2) { PlacePath.Add(Others[Others.Num() / 2]); } // drift past one in between
		PlacePath.Add(Target);
		PlaceT0 = ClipClock;
	}
	// Hop from cell to cell (eased), pausing on each.
	const double T = ClipClock - PlaceT0;
	const double Leg = PlaceHop + PlaceDwell;
	const int32 Seg = FMath::Clamp((int32)(T / Leg), 0, PlacePath.Num() - 1);
	const FVector2D From = DCSite::Grid::Centre(PlacePath[Seg]);
	const FVector2D To = DCSite::Grid::Centre(PlacePath[FMath::Min(Seg + 1, PlacePath.Num() - 1)]);
	const float U = Smoother((float)((T - Seg * Leg - PlaceDwell) / PlaceHop));
	Placer->DevSetCursor(FMath::Lerp(From, To, U));
}

void UDCDevTools::ReelTickClip(float DeltaTime)
{
	ClipClock += DeltaTime;
	const double CleanT = ClipClock - ReelFade; // actions are timed from the clean part
	for (FReelAction& A : Reel[ReelIndex].Actions) { if (!A.bDone && CleanT >= A.At) { ReelRunAction(A); } }
	ReelTickPlace();
	ReelApplyCamera();
}

// ─────────────────────────────────────────────────────────────────────────────
// State machine
// ─────────────────────────────────────────────────────────────────────────────

bool UDCDevTools::TickReel(float DeltaTime)
{
	UWorld* W = GetGameWorld();
	UDCUISubsystem* UI = UDCUISubsystem::Get(W);
	const float Dt = FMath::Min(DeltaTime, 0.1f);
	ReelT += Dt;
	if (ReelState != EReelState::Boot) { ReelParkCursor(); }
	const double Now = FPlatformTime::Seconds();
	const auto Go = [this](EReelState S) { ReelState = S; ReelT = 0.0; };
	const auto ClipLength = [this]() { const float L = Reel[ReelIndex].Length; return L > 0.f ? L : ReelSeconds; };

	switch (ReelState)
	{
	case EReelState::Boot:
		// Wait for the loading screen to lift and the title to settle, then go black and tell the recorder where to look.
		if (!W || !W->HasBegunPlay() || !UI || UI->IsLoading()) { ReelT = 0.0; return true; }
		if (ReelT < 1.5) { return true; }
		ReelSetFade(1.f);
		if (TSharedPtr<SWindow> Win = GEngine && GEngine->GameViewport ? GEngine->GameViewport->GetWindow() : nullptr) { Win->HACK_ForceToFront(); }
		ReelWriteWindowRect();
		IFileManager::Get().Delete(*(ShotDir / TEXT("reel_go")), false, true, true);
		Go(EReelState::WaitGo);
		return true;

	case EReelState::WaitGo:
	{
		ReelSetFade(1.f);
		const bool bGo = IFileManager::Get().FileExists(*(ShotDir / TEXT("reel_go")));
		if (ReelWait > 0.0 && !bGo && ReelT < ReelWait) { return true; }
		if (ReelWait <= 0.0 && ReelT < 2.0) { return true; }
		if (ReelWait > 0.0 && !bGo) { UE_LOG(LogDC, Warning, TEXT("[Reel] no reel_go after %.0fs: starting anyway"), ReelWait); }
		ReelGoAt = Now;
		Cues.Reset();
		Go(EReelState::Setup);
		return true;
	}

	case EReelState::Setup:
		// Keep the game on top of whatever else is open: the recorder films the screen.
		if (GEngine && GEngine->GameViewport) { if (TSharedPtr<SWindow> Win = GEngine->GameViewport->GetWindow()) { Win->HACK_ForceToFront(); } }
		ReelSetup();
		if (!Reel.IsValidIndex(ReelIndex)) { Go(EReelState::Done); return true; }
		Go(EReelState::Settle);
		return true;

	case EReelState::Settle:
		// Right after a jump the world is still rebuilding and the overview framing can briefly span the whole map:
		// keep aiming from the world as it is now, so the camera (and texture streaming) is on the real shot by the fade-in.
		ReelResolveClipCamera(false);
		if (ReelT < ReelSettle) { return true; }
		ReelResolveClipCamera(true);
		if (Cues.Num() == 0) { Cues.Add(FString::Printf(TEXT("sync,first-fade-in,%.3f,%.3f"), Now - ReelGoAt, Now - ReelGoAt)); }
		if (Reel[ReelIndex].Panel == TEXT("Title") && UI) { UI->DevAction(TEXT("titleintro")); }
		Go(EReelState::FadeIn);
		return true;

	case EReelState::FadeIn:
		ReelTickClip(Dt);
		// Snappy: the picture comes up fast and settles in (ease-out); it drops out the same way in reverse (ease-in).
		ReelSetFade(ReelFade > 0.f ? FMath::Pow(1.f - FMath::Clamp((float)(ReelT / ReelFade), 0.f, 1.f), 3.f) : 0.f);
		if (ReelT < ReelFade) { return true; }
		ReelSetFade(0.f);
		ClipStartAt = Now;
		Go(EReelState::Hold);
		return true;

	case EReelState::Hold:
		ReelTickClip(Dt);
		if (ReelT < ClipLength()) { return true; }
		Cues.Add(FString::Printf(TEXT("clip,%s,%.3f,%.3f"), *Reel[ReelIndex].Name, ClipStartAt - ReelGoAt, Now - ReelGoAt));
		ReelWriteCues();
		UE_LOG(LogDC, Display, TEXT("[Reel] %s: %.2fs .. %.2fs"), *Reel[ReelIndex].Name, ClipStartAt - ReelGoAt, Now - ReelGoAt);
		Go(EReelState::FadeOut);
		return true;

	case EReelState::FadeOut:
		ReelTickClip(Dt);
		ReelSetFade(ReelFade > 0.f ? FMath::Pow(FMath::Clamp((float)(ReelT / ReelFade), 0.f, 1.f), 3.f) : 1.f);
		if (ReelT < ReelFade) { return true; }
		ReelSetFade(1.f);
		ReelEndClip();
		++ReelIndex;
		Go(EReelState::Setup);
		return true;

	case EReelState::Done:
		ReelSetFade(1.f);
		if (ReelT < 1.0) { return true; }
		Cues.Add(FString::Printf(TEXT("end,done,%.3f,%.3f"), Now - ReelGoAt, Now - ReelGoAt));
		ReelWriteCues();
		UE_LOG(LogDC, Display, TEXT("[Reel] done, quitting"));
		if (GEngine) { GEngine->Exec(W, TEXT("QUIT")); }
		return false;
	}
	return true;
}

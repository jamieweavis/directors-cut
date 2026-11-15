// Director's Cut — dev/automation helpers driven by the command line (LEAD-OWNED).
//
//   -DCShots=name:focus[:panel],name2:focus2   take screenshots then quit.
//        focus = wide | Office | Lounge | Workshop | Garage | OpsRoom | Clinic | ServerRoom | LawyerOffice
//                (aliases Bunkroom, Ops, Lawyer) | crew (close-up on a crew member, a sleeping one if any)
//                | yard (the parked cars + driveway) | house (the built house) | far (zoomed right out over the landscape)
//        panel = optional EDCPanel name to open before the shot (e.g. Missions, MissionPlanner, Reports)
//        focus@Z = same focus at zoom Z (0 = the overview distance .. 1 = closest), e.g. house@0.5, wide@0.2, crew@0.8
//   -DCShotDir=/abs/dir     where PNGs go (default <Project>/Saved/DCShots)
//   -DCShotDelay=6          seconds before the first shot (default 6; shaders/Lumen settle)
//   -DCBuildAll             show every room as Built level 3 (scene review)
//   -DCSeed=1234            start the run with this seed (skips the title)
//   -DCAutoPlay=S           bot plays S run-seconds instantly before the shots (starts every staffable mission >= 50%, hires crew and
//                           builds/upgrades rooms when it can afford them), then snaps the world to the result; logs results.
//                           With -DCHour=H the bot keeps playing until the clock reads H.
//   -DCEndRun=win|lose      end the run right after start / after -DCAutoPlay (end screen shots)
//   -DCSaveSlot=Prefix      use Prefix0..2 + Prefix_Leaderboard as save files instead of the player's real saves (screenshot runs)
//   -DCTimeScale=N          run the clock N x faster (testing)
//   -DCHour=H               jump the clock forward to hour H (0..24) shortly after start (day/night screenshots)
//   -DCQuitAfter=S          quit after S seconds (when not taking shots)
//   -DCDebugView            draw the generated layout (build grid, rooms, doorways, furniture, nav tile map, crew routes);
//                           same as Options > Gameplay > Debug view
//   -DCHireCrew=N           hire the first N recruits right after start (crew close-ups; stops when cash runs out)
//   -DCCrewBody=Name        every crew member wears SK_Crew_<Name> (checking a newly imported body)
//   -DCPortraitDump         also write every baked crew portrait onto a contact sheet, <DCShotDir>/portraits.png
//   -DCPerkOffer          put a perk offer on the table right after start (shoot it with panel PerkDraft)
//   -DCPerkSel=N            with -DCShots: pre-select perk card N (0-based) when the PerkDraft panel opens
//   -DCContinue            load the saved run (UDCRunSubsystem::ContinueRun) right after start instead of a new run
//
// Video reels (DCDevReel.cpp; driven by Scripts/record_video_*.{ps1,sh}):
//   -DCReel=/abs/reel.txt   play the shot list in that file (format: Scripts/video_reel.txt) from the title screen, then quit.
//                           Each clip fades in from black, holds, and fades out; between clips the run jumps forward
//                           behind the black. Writes reel_window.txt (the game's client rect on screen) and reel_cues.csv
//                           (clip times) into -DCShotDir.
//   -DCReelWait=S           wait up to S seconds (default 60) for <ShotDir>/reel_go (the recorder is running) before the
//                           first clip; 0 = start straight away
//   -DCReelSeed=N           overrides the seed of every "newrun" line
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Containers/Ticker.h"
#include "DCDevTools.generated.h"

UCLASS()
class DIRECTORSCUT_API UDCDevTools : public UGameInstanceSubsystem
{
	GENERATED_BODY()
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	static bool IsBuildAll();
	static int32 GetCmdSeed(); // 0 if not given
	/** A video reel is playing (-DCReel): the mouse is kept out of the picture. */
	static bool IsReel();

private:
	struct FShot { FString Name; FString Focus; FString Panel; };
	TArray<FShot> Shots;
	FString ShotDir;
	float Delay = 6.f;
	float QuitAfter = -1.f;
	float AutoPlaySeconds = 0.f;
	bool bClockFlags = false;
	bool bContinue = false;
	int32 HireCrew = 0;
	bool bPerkOffer = false;
	FString EndRun;             // -DCEndRun: "win" / "lose"
	double Elapsed = 0.0;
	int32 Step = 0;
	double NextAt = 0.0;
	bool bPreparedShot = false;
	FTSTicker::FDelegateHandle TickHandle;

	bool Tick(float DeltaTime);
	void PrepareShot(const FShot& Shot);
	/** Bot plays Seconds of run time instantly (then on to clock hour Hour when >= 0), and the world snaps to the result.
	 *  bKeepAfloat (video reels): never let the bank run dry, so the run can't end before the shots it is played for. */
	void RunAutoPlay(double Seconds, float Hour, bool bKeepAfloat = false);
	/** A named camera target (yard, house, crew, a room type...): the point and its default zoom. False if unknown. */
	bool ResolveFocus(const FString& Name, FVector& OutPoint, float& OutZoom) const;
	void ApplyClockFlags();
	void ApplyContinue();
	void ApplyHireCrew();
	void ApplyEndRun();
	UWorld* GetGameWorld() const;

	// ── Video reel (DCDevReel.cpp) ──────────────────────────────────────────
	struct FReelAction { float At = 0.f; FString Verb; FString Arg; bool bDone = false; };
	struct FReelStep
	{
		FString Kind;               // clip | newrun | play | endrun | seconds | fade
		TArray<FString> Args;       // the raw words after the kind
		// clip
		FString Name, CamFrom, CamTo, Panel;
		bool bHudOff = false;
		float Length = -1.f;        // < 0 = the reel's "seconds"
		TArray<FReelAction> Actions;
		int32 Line = 0;
	};
	struct FCamPose { FVector Pivot = FVector::ZeroVector; float Distance = 6000.f; };
	enum class EReelState : uint8 { Boot, WaitGo, Setup, Settle, FadeIn, Hold, FadeOut, Done };
	TArray<FReelStep> Reel;
	FString ReelPath;
	int32 ReelIndex = 0;
	EReelState ReelState = EReelState::Boot;
	double ReelT = 0.0;            // seconds in the current state
	double ReelGoAt = 0.0;         // FPlatformTime at the go signal (cue times are relative to it)
	double ReelWait = 60.0;
	float ReelSeconds = 10.f;
	float ReelFade = 0.5f;
	double ReelSettle = 0.0;
	double ClipStartAt = 0.0;      // FPlatformTime when the clean part of the clip began
	double ClipClock = 0.0;        // seconds since the clip's fade-in began (camera moves, actions)
	FCamPose PoseFrom, PoseTo;
	bool bPan = false;
	bool bFollow = false;
	TWeakObjectPtr<AActor> FollowActor;
	FVector FollowPivot = FVector::ZeroVector;
	bool bLook = false;
	double LookT0 = 0.0;
	FCamPose LookFrom, LookTo;
	FCamPose LastPose;
	float SavedTimeScale = 1.f;
	bool bLapse = false;
	TArray<FIntPoint> PlacePath;    // cells the placement cursor glides over (the last one is clicked)
	double PlaceT0 = -1.0;
	bool bPlacePending = false;
	float FadeAlpha = 1.f;
	TSharedPtr<class SWidget> FadeWidget;
	TArray<FString> Cues;

	bool LoadReel();
	bool TickReel(float DeltaTime);
	void ReelSetup();               // runs directives until the next clip, then prepares it
	void ReelPrepareClip(FReelStep& Clip);
	void ReelEndClip();
	void ReelTickClip(float DeltaTime);
	void ReelRunAction(FReelAction& A);
	bool ReelResolvePose(const FString& Spec, FCamPose& Out, bool* bOutFollow = nullptr);
	void ReelApplyCamera();
	void ReelResolveClipCamera(bool bLog);     // the clip's camera poses, from the world as it is now
	void ReelTickPlace();
	void ReelSetFade(float Alpha);
	void ReelParkCursor();
	void ReelWriteWindowRect();
	void ReelWriteCues();
	class ADCCameraRig* GetRig() const;
};

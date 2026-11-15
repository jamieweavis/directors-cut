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
//   -DCPerkOffer            put a perk offer on the table right after start (shoot it with panel PerkDraft)
//   -DCPerkSel=N            with -DCShots: pre-select perk card N (0-based) when the PerkDraft panel opens
//   -DCContinue            load the saved run (UDCRunSubsystem::ContinueRun) right after start instead of a new run
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
	void RunAutoPlay();
	void ApplyClockFlags();
	void ApplyContinue();
	void ApplyHireCrew();
	void ApplyEndRun();
	UWorld* GetGameWorld() const;
};

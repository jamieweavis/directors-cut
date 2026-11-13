// Director's Cut — dev/automation helpers driven by the command line (LEAD-OWNED).
//
//   -DCShots=name:focus[:panel],name2:focus2   take screenshots then quit.
//        focus = wide | Office | Lounge | Workshop | Garage | ScreeningRoom | Clinic | ServerCloset | LawyerOffice
//        panel = optional EDCPanel name to open before the shot (e.g. JobBoard, CrewPlanner, Dailies)
//   -DCShotDir=/abs/dir     where PNGs go (default <Project>/Saved/DCShots)
//   -DCShotDelay=6          seconds before the first shot (default 6; shaders/Lumen settle)
//   -DCBuildAll             show every room as Built level 3 (scene review)
//   -DCSeed=1234            start the run with this seed (skips the title)
//   -DCAutoPlay=N           bot plays N nights (commit best job, end night) before the shots; logs results
//   -DCQuitAfter=S          quit after S seconds (when not taking shots)
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
	int32 AutoPlayNights = 0;
	double Elapsed = 0.0;
	int32 Step = 0;
	double NextAt = 0.0;
	bool bPreparedShot = false;
	FTSTicker::FDelegateHandle TickHandle;

	bool Tick(float DeltaTime);
	void PrepareShot(const FShot& Shot);
	void RunAutoPlay();
	UWorld* GetGameWorld() const;
};

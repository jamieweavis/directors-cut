// Director's Cut: UDCAudioSubsystem implementation (Agent H, v2).
//
//  - SFX by canonical name (DCSfx::*) via soft paths under /Game/DC/Audio, 2D, with per-sound gain, small pitch
//    jitter, round-robin variants (Click, Hover) and per-name throttling. The WAVs are loudness-normalised by
//    Tools/audio_gen.py, so the gains here are only small trims.
//  - Ambience: four looping beds (interior room tone, day countryside, night countryside, rain) crossfaded continuously from the
//    run clock (DCAudio::ComputeAmbienceMix), ducked while paused; plus quiet random distant one-shots every
//    30-120 s (day: crow / cattle / tractor; night: owl / dog / train horn). Title: room tone + night low.
//  - Scores the game automatically from UDCRunSubsystem delegates through a small timed queue, with burst
//    throttling (e.g. many completions after a bot AdvanceTime jump play one sequence, not twenty).
//  - Music: a lounge score of four 32-bar sets (DayA/DayB/NightA/NightB) x five looping stems. All stems of a set
//    start in the same frame and stay locked; every 8-bar phrase the director re-mixes them (DCAudio::ComputeMusicLayers)
//    from the clock (day/night, with hysteresis) and the run's progress (unlocked tier = chapter 1..4): early runs get
//    keys, bass and a light groove with the odd melody; later chapters bring the second sets, both melodies and the
//    Color layer. Set changes wait for a phrase boundary and crossfade; title <-> run changes crossfade at once.
//    Paused: ducked and low-passed. Game over: fades out.
//  - Volume channels Master/SFX/Ambience/Music applied live and persisted in GameUserSettings.ini.
//  - Missing assets are logged once and stay silent. Everything is rebuilt when the game instance's world changes.
#include "Core/DCAudioSubsystem.h"
#include "Core/DCRunSubsystem.h"
#include "Audio/DCAudioAssets.h"
#include "DirectorsCut.h"

#include "Components/AudioComponent.h"
#include "Containers/Ticker.h"
#include "Engine/AssetManager.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/StreamableManager.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/ConfigCacheIni.h"
#include "Sound/SoundBase.h"

namespace
{
	const TCHAR* ConfigSection = TEXT("/Script/DirectorsCut.DCAudio");
	constexpr int32 NumVolumeChannels = 4;
	const TCHAR* ChannelKeys[NumVolumeChannels] = { TEXT("MasterVolume"), TEXT("SFXVolume"), TEXT("AmbienceLevel"), TEXT("MusicVolume") };

	// The Ambience slider spans a quieter range: full slider = 25% of the raw bed gain (default 20%).
	// The config key changed with the range so saved old-scale values reset.
	constexpr float AmbienceRangeScale = 0.25f;

	constexpr float BedFadeTau = 1.2f;          // seconds; crossfade smoothing for bed gains
	constexpr float OneShotGain = 0.8f;
	constexpr float OneShotMin = 30.f, OneShotMax = 120.f;

	constexpr double MusicPhraseLead = 0.6;   // seconds: phrase re-mixes start just before the downbeat
	constexpr float MusicLayerTau = 0.9f;     // seconds: layer in/out smoothing
	constexpr float MusicFadeOutTau = 1.2f;   // seconds: outgoing set
	constexpr float MusicPausedLpfHz = 1400.f;

	struct FSfxTuning
	{
		float Gain = 1.f;
		float PitchJitter = 0.f;  // +/- fraction
		float MinInterval = 0.f;  // real seconds between plays of the same name
	};

	FSfxTuning TuningFor(FName N)
	{
		if (N == DCSfx::Click)           return { 0.90f, 0.03f, 0.035f };
		if (N == DCSfx::Hover)           return { 0.80f, 0.04f, 0.05f };
		if (N == DCSfx::PanelOpen)       return { 0.90f, 0.04f, 0.08f };
		if (N == DCSfx::PanelClose)      return { 0.90f, 0.04f, 0.08f };
		if (N == DCSfx::Confirm)         return { 1.00f, 0.00f, 0.10f };
		if (N == DCSfx::Error)           return { 1.00f, 0.00f, 0.20f };
		if (N == DCSfx::SlotFill)        return { 1.00f, 0.04f, 0.06f };
		if (N == DCSfx::Cash)            return { 1.00f, 0.03f, 0.25f };
		if (N == DCSfx::MissionStart)    return { 1.00f, 0.02f, 1.50f };
		if (N == DCSfx::MissionComplete) return { 1.00f, 0.00f, 1.20f };
		if (N == DCSfx::Success || N == DCSfx::Fail || N == DCSfx::Busted) return { 1.00f, 0.00f, 1.20f };
		if (N == DCSfx::LevelUp)         return { 1.00f, 0.00f, 1.00f };
		if (N == DCSfx::BuildStart)      return { 1.00f, 0.03f, 1.00f };
		if (N == DCSfx::RoomBuilt)       return { 1.00f, 0.00f, 1.00f };
		if (N == DCSfx::Perk)            return { 1.00f, 0.02f, 1.00f };
		if (N == DCSfx::Payday)          return { 1.00f, 0.00f, 2.00f };
		if (N == DCSfx::GarageDoor)      return { 0.85f, 0.03f, 4.00f };
		if (N == DCSfx::CarDepart)       return { 0.80f, 0.04f, 5.00f };
		if (N == DCSfx::CarArrive)       return { 0.80f, 0.04f, 5.00f };
		return {};
	}
}

struct FDCAudioRuntime
{
	struct FQueued { double At; FName Sfx; float Vol; };

	FTSTicker::FDelegateHandle TickHandle;
	TSharedPtr<FStreamableHandle> PreloadHandle;
	TWeakObjectPtr<UWorld> BoundWorld;
	TWeakObjectPtr<UDCRunSubsystem> Run;
	TSet<FSoftObjectPath> MissingLogged;
	TMap<FName, double> LastPlay;
	TMap<FName, int32> LastVariant;
	TArray<FQueued> Queue;
	FRandomStream Rand{ (int32)FPlatformTime::Cycles() };
	float BedGain[DCAudio::BedCount] = { 0.f, 0.f, 0.f, 0.f };   // smoothed, before channel volumes
	float AppliedVol[DCAudio::BedCount] = { -1.f, -1.f, -1.f, -1.f };
	double Now = 0.0;
	double NextOneShot = 0.0;
	FString LastOneShot;
	bool bVolumesDirty = false;
	double VolumesDirtySince = 0.0;
	FDelegateHandle RunHandles[7];

	// Music director
	int32 MusicSet = -1;
	bool bMusicTitle = false;
	bool bMusicDay = true;
	double MusicStart = 0.0;     // FPlatformTime::Seconds() when the set's stems started
	double MusicLoopSec = 80.0;
	int32 MusicPhrase = 0;       // phrase index (from the set's start) the current mix is for
	int32 PassesOnSet = 0;       // completed 32-bar passes of the current set
	DCAudio::FMusicPass Pass;
	float LayerTarget[DCAudio::MusicStemCount] = {};
	float LayerGain[DCAudio::MusicStemCount] = {};
	float LayerApplied[DCAudio::MusicStemCount] = { -1.f, -1.f, -1.f, -1.f, -1.f };
	float FadeBase[DCAudio::MusicStemCount] = {};
	float FadeGain = 0.f;
	float MusicLevel = 0.f;      // 0 when silenced (game over)
	float MusicDuck = 1.f;       // paused
	float LpfHz = 20000.f;
	float LpfApplied = -1.f;
};

// ─────────────────────────────────────────────────────────────────────────────
// Statics
// ─────────────────────────────────────────────────────────────────────────────

UDCAudioSubsystem* UDCAudioSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* W = (WorldContext && GEngine) ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	UGameInstance* GI = W ? W->GetGameInstance() : nullptr;
	return GI ? GI->GetSubsystem<UDCAudioSubsystem>() : nullptr;
}

void UDCAudioSubsystem::Play(const UObject* WorldContext, FName SfxName)
{
	if (UDCAudioSubsystem* A = Get(WorldContext)) { A->PlaySFX(SfxName); }
}

// ─────────────────────────────────────────────────────────────────────────────
// Lifecycle
// ─────────────────────────────────────────────────────────────────────────────

void UDCAudioSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	RT = MakeShared<FDCAudioRuntime>();
	LoadVolumes();

	// Warm everything up asynchronously so the first click doesn't hitch.
	TArray<FSoftObjectPath> All;
	for (const FString& A : DCAudio::AllAssetNames()) { All.Add(DCAudio::MakePath(A)); }
	if (UAssetManager* AM = UAssetManager::GetIfInitialized())
	{
		RT->PreloadHandle = AM->GetStreamableManager().RequestAsyncLoad(All, FStreamableDelegate(), FStreamableManager::AsyncLoadHighPriority);
	}

	if (UDCRunSubsystem* Run = Collection.InitializeDependency<UDCRunSubsystem>())
	{
		RT->Run = Run;
		RT->RunHandles[0] = Run->OnMissionStarted.AddUObject(this, &UDCAudioSubsystem::HandleMissionStarted);
		RT->RunHandles[1] = Run->OnMissionCompleted.AddUObject(this, &UDCAudioSubsystem::HandleMissionCompleted);
		RT->RunHandles[2] = Run->OnCriminalLevelUp.AddUObject(this, &UDCAudioSubsystem::HandleLevelUp);
		RT->RunHandles[3] = Run->OnRoomChanged.AddUObject(this, &UDCAudioSubsystem::HandleRoomChanged);
		RT->RunHandles[4] = Run->OnPerkOffered.AddUObject(this, &UDCAudioSubsystem::HandlePerkOffered);
		RT->RunHandles[5] = Run->OnPayday.AddUObject(this, &UDCAudioSubsystem::HandlePayday);
		RT->RunHandles[6] = Run->OnPhaseChanged.AddUObject(this, &UDCAudioSubsystem::HandlePhaseChanged);
	}

	// Core ticker: runs every frame regardless of game pause.
	RT->TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UDCAudioSubsystem::TickAudio), 0.f);
}

void UDCAudioSubsystem::Deinitialize()
{
	if (RT.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(RT->TickHandle);
		if (UDCRunSubsystem* Run = RT->Run.Get())
		{
			Run->OnMissionStarted.Remove(RT->RunHandles[0]);
			Run->OnMissionCompleted.Remove(RT->RunHandles[1]);
			Run->OnCriminalLevelUp.Remove(RT->RunHandles[2]);
			Run->OnRoomChanged.Remove(RT->RunHandles[3]);
			Run->OnPerkOffered.Remove(RT->RunHandles[4]);
			Run->OnPayday.Remove(RT->RunHandles[5]);
			Run->OnPhaseChanged.Remove(RT->RunHandles[6]);
		}
		if (RT->bVolumesDirty) { SaveVolumes(); }
		if (RT->PreloadHandle.IsValid()) { RT->PreloadHandle->CancelHandle(); }
	}
	TearDownComponents();
	SoundCache.Empty();
	RT.Reset();
	Super::Deinitialize();
}

UWorld* UDCAudioSubsystem::GetAudioWorld() const
{
	const UGameInstance* GI = GetGameInstance();
	UWorld* W = GI ? GI->GetWorld() : nullptr;
	return (W && !W->bIsTearingDown && W->IsGameWorld()) ? W : nullptr;
}

bool UDCAudioSubsystem::TickAudio(float DeltaTime)
{
	if (!RT.IsValid()) { return true; }
	DeltaTime = FMath::Clamp(DeltaTime, 0.f, 0.25f);
	RT->Now += DeltaTime;

	if (RT->bVolumesDirty && RT->Now - RT->VolumesDirtySince > 1.0) { SaveVolumes(); }

	const bool bFresh = RT->BoundWorld.Get() != GetAudioWorld();
	if (!EnsureComponents()) { return true; }
	UpdateAmbience(DeltaTime, bFresh);
	UpdateMusic(DeltaTime);
	TickQueue();
	TickAmbienceOneShots();
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Components & loading
// ─────────────────────────────────────────────────────────────────────────────

UAudioComponent* UDCAudioSubsystem::MakeComponent(USoundBase* Sound)
{
	UWorld* W = GetAudioWorld();
	if (!W || !Sound) { return nullptr; }
	UAudioComponent* C = UGameplayStatics::CreateSound2D(W, Sound, 1.f, 1.f, 0.f, nullptr, /*bPersist*/ false, /*bAutoDestroy*/ false);
	if (C)
	{
		C->bIsUISound = true;      // keep playing while the game is paused
		C->bAllowSpatialization = false;
	}
	return C;
}

bool UDCAudioSubsystem::EnsureComponents()
{
	UWorld* W = GetAudioWorld();
	if (!W)
	{
		if (RT->BoundWorld.IsValid() || BedComps.Num() > 0) { TearDownComponents(); }
		return false;
	}
	if (RT->BoundWorld.Get() == W) { return true; }

	// New world (first world, map travel, PIE restart): rebuild against it.
	TearDownComponents();
	RT->BoundWorld = W;
	BedComps.SetNum(DCAudio::BedCount);
	for (int32 i = 0; i < DCAudio::BedCount; ++i)
	{
		USoundBase* S = LoadSound(DCAudio::MakePath(DCAudio::BedAssetName(i)));
		UAudioComponent* C = S ? MakeComponent(S) : nullptr;
		BedComps[i] = C;
		RT->AppliedVol[i] = -1.f;
		if (C)
		{
			C->SetVolumeMultiplier(0.001f);
			// Random start offsets so the beds don't phase-lock on every restart.
			C->Play(RT->Rand.FRandRange(0.f, 20.f));
		}
	}
	if (RT->NextOneShot <= RT->Now) { RT->NextOneShot = RT->Now + RT->Rand.FRandRange(15.f, 40.f); }
	int32 Live = 0;
	for (UAudioComponent* C : BedComps) { Live += C ? 1 : 0; }
	UE_LOG(LogDC, Display, TEXT("DCAudio: bound to world %s, %d/%d ambience beds playing"), *W->GetName(), Live, (int32)DCAudio::BedCount);
	return true;
}

void UDCAudioSubsystem::TearDownComponents()
{
	StopMusic(false);
	for (UAudioComponent* C : BedComps)
	{
		if (IsValid(C))
		{
			C->Stop();
			C->DestroyComponent();
		}
	}
	BedComps.Reset();
	if (RT.IsValid())
	{
		RT->BoundWorld = nullptr;
		for (float& G : RT->BedGain) { G = 0.f; }
	}
}

USoundBase* UDCAudioSubsystem::LoadSound(const FSoftObjectPath& Path)
{
	if (Path.IsNull()) { return nullptr; }
	const FName Key(*Path.ToString());
	if (TObjectPtr<USoundBase>* Found = SoundCache.Find(Key)) { return Found->Get(); }
	USoundBase* S = Cast<USoundBase>(Path.TryLoad());
	if (!S)
	{
		if (RT.IsValid() && !RT->MissingLogged.Contains(Path))
		{
			RT->MissingLogged.Add(Path);
			UE_LOG(LogDC, Warning, TEXT("DCAudio: missing sound asset %s (staying silent)"), *Path.ToString());
		}
		return nullptr;
	}
	SoundCache.Add(Key, S);
	return S;
}

// ─────────────────────────────────────────────────────────────────────────────
// SFX
// ─────────────────────────────────────────────────────────────────────────────

void UDCAudioSubsystem::PlaySFX(FName SfxName, float VolumeMultiplier, float PitchMultiplier)
{
	if (!RT.IsValid() || SfxName.IsNone()) { return; }
	UWorld* W = GetAudioWorld();
	if (!W) { return; }

	const FSfxTuning Tune = TuningFor(SfxName);
	const double Now = FPlatformTime::Seconds();
	if (double* Last = RT->LastPlay.Find(SfxName); Last && Now - *Last < Tune.MinInterval) { return; }
	RT->LastPlay.Add(SfxName, Now);

	// Round-robin-ish variant choice (never the same twice in a row).
	int32 Variant = 0;
	const int32 Count = DCAudio::SfxVariantCount(SfxName);
	if (Count > 1)
	{
		const int32 Prev = RT->LastVariant.FindRef(SfxName);
		Variant = (Prev + 1 + RT->Rand.RandHelper(Count - 1)) % Count;
		RT->LastVariant.Add(SfxName, Variant);
	}
	USoundBase* Sound = LoadSound(DCAudio::SfxPath(SfxName, Variant));
	if (!Sound && Variant != 0) { Sound = LoadSound(DCAudio::SfxPath(SfxName, 0)); }
	if (!Sound) { return; }

	const float Vol = Tune.Gain * VolumeMultiplier * Volumes[(int32)EDCVolumeChannel::Master] * Volumes[(int32)EDCVolumeChannel::SFX];
	if (Vol <= 0.001f) { return; }
	const float Pitch = PitchMultiplier * (Tune.PitchJitter > 0.f ? RT->Rand.FRandRange(1.f - Tune.PitchJitter, 1.f + Tune.PitchJitter) : 1.f);
	UGameplayStatics::PlaySound2D(W, Sound, Vol, Pitch, 0.f, nullptr, nullptr, /*bIsUISound*/ true);
}

void UDCAudioSubsystem::Schedule(FName SfxName, float Delay, float VolumeMultiplier)
{
	if (!RT.IsValid()) { return; }
	const double At = RT->Now + FMath::Max(0.f, Delay);
	const float Window = FMath::Max(TuningFor(SfxName).MinInterval, 0.3f);
	for (const FDCAudioRuntime::FQueued& Q : RT->Queue)
	{
		if (Q.Sfx == SfxName && FMath::Abs(Q.At - At) < Window) { return; }
	}
	if (RT->Queue.Num() >= 24) { return; }
	RT->Queue.Add({ At, SfxName, VolumeMultiplier });
	UE_LOG(LogDC, Verbose, TEXT("DCAudio: queued %s in %.2fs"), *SfxName.ToString(), Delay);
}

void UDCAudioSubsystem::TickQueue()
{
	for (int32 i = 0; i < RT->Queue.Num();)
	{
		if (RT->Queue[i].At <= RT->Now)
		{
			const FDCAudioRuntime::FQueued Q = RT->Queue[i];
			RT->Queue.RemoveAt(i);
			PlaySFX(Q.Sfx, Q.Vol);
		}
		else { ++i; }
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Ambience
// ─────────────────────────────────────────────────────────────────────────────

void UDCAudioSubsystem::UpdateAmbience(float DeltaTime, bool bSnap)
{
	const UDCRunSubsystem* Run = RT->Run.Get();
	const bool bActive = Run && Run->HasActiveRun();
	const float Hour = bActive ? Run->GetHourOfDay() : 0.f;
	const float Rain = bActive ? DCAudio::RainAmount(Run->GetSeed(), Run->GetDay(), Hour) : 0.f;
	const DCAudio::FAmbienceMix Mix = DCAudio::ComputeAmbienceMix(bActive, Hour, Rain, bActive && Run->IsPaused());

	const float K = bSnap ? 1.f : 1.f - FMath::Exp(-DeltaTime / BedFadeTau);
	const float Channel = Volumes[(int32)EDCVolumeChannel::Master] * Volumes[(int32)EDCVolumeChannel::Ambience] * AmbienceRangeScale;
	for (int32 i = 0; i < DCAudio::BedCount && i < BedComps.Num(); ++i)
	{
		RT->BedGain[i] += (Mix.Bed[i] - RT->BedGain[i]) * K;
		UAudioComponent* C = BedComps[i];
		if (!C) { continue; }
		// Floor at 0.001 rather than 0 so silent beds keep their playback position.
		const float V = FMath::Max(RT->BedGain[i] * Channel, 0.001f);
		if (FMath::Abs(V - RT->AppliedVol[i]) > 0.002f)
		{
			C->SetVolumeMultiplier(V);
			RT->AppliedVol[i] = V;
		}
	}
}

void UDCAudioSubsystem::TickAmbienceOneShots()
{
	if (RT->Now < RT->NextOneShot) { return; }
	RT->NextOneShot = RT->Now + RT->Rand.FRandRange(OneShotMin, OneShotMax);

	const UDCRunSubsystem* Run = RT->Run.Get();
	const bool bActive = Run && Run->HasActiveRun();
	if (bActive && Run->IsPaused()) { return; }
	const float Hour = bActive ? Run->GetHourOfDay() : 0.f;
	const bool bDay = bActive && DCAudio::DayFactor(Hour) > 0.5f;

	TArray<FString> Pool = bDay ? DCAudio::DayOneShots() : DCAudio::NightOneShots();
	if (bDay && (Hour < 7.5f || Hour > 18.f)) { Pool.Remove(TEXT("AMB_Tractor")); }   // the farm only works the fields in working hours
	if (Pool.Num() > 1) { Pool.Remove(RT->LastOneShot); }
	if (Pool.Num() == 0) { return; }
	const FString Pick = Pool[RT->Rand.RandHelper(Pool.Num())];
	RT->LastOneShot = Pick;

	const float Vol = OneShotGain * (bActive ? 1.f : 0.6f) * RT->Rand.FRandRange(0.7f, 1.f)
		* Volumes[(int32)EDCVolumeChannel::Master] * Volumes[(int32)EDCVolumeChannel::Ambience] * AmbienceRangeScale;
	if (Vol <= 0.001f) { return; }
	if (USoundBase* S = LoadSound(DCAudio::MakePath(Pick)))
	{
		UGameplayStatics::PlaySound2D(GetAudioWorld(), S, Vol, RT->Rand.FRandRange(0.94f, 1.05f), 0.f, nullptr, nullptr, true);
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Music
// ─────────────────────────────────────────────────────────────────────────────

void UDCAudioSubsystem::UpdateMusic(float DeltaTime)
{
	const UDCRunSubsystem* Run = RT->Run.Get();
	const EDCGamePhase Phase = Run ? Run->GetPhase() : EDCGamePhase::Title;
	const bool bSilent = Phase == EDCGamePhase::GameOver;
	const bool bTitle = !Run || Phase == EDCGamePhase::Title || (!Run->HasActiveRun() && Phase != EDCGamePhase::Won);
	const float Hour = bTitle ? 0.f : Run->GetHourOfDay();
	RT->bMusicDay = DCAudio::MusicIsDay(Hour, RT->bMusicDay);
	const int32 Chapter = bTitle ? 1 : (Phase == EDCGamePhase::Won ? 4 : DCAudio::MusicChapter(Run->GetUnlockedTier()));

	if (!bSilent)
	{
		if (RT->MusicSet < 0 || bTitle != RT->bMusicTitle)
		{
			// Nothing playing yet, or title <-> run: change straight away.
			StartMusicSet(DCAudio::ChooseMusicSet(bTitle, RT->bMusicDay, Chapter, -1, 0, RT->Rand.FRand()), bTitle, Chapter);
		}
		else
		{
			const double PhraseSec = RT->MusicLoopSec / DCAudio::MusicPhrasesPerLoop;
			const double Pos = FPlatformTime::Seconds() - RT->MusicStart;
			const int32 Next = FMath::FloorToInt32((Pos + MusicPhraseLead) / PhraseSec);
			if (Next > RT->MusicPhrase)
			{
				const bool bLoopTop = Next % DCAudio::MusicPhrasesPerLoop == 0;
				const bool bWrongTime = !bTitle && DCAudio::IsDaySet(RT->MusicSet) != RT->bMusicDay;
				if (bLoopTop) { ++RT->PassesOnSet; }
				int32 Want = RT->MusicSet;
				if (bLoopTop || bWrongTime)
				{
					Want = DCAudio::ChooseMusicSet(bTitle, RT->bMusicDay, Chapter, RT->MusicSet, RT->PassesOnSet, RT->Rand.FRand());
				}
				if (Want != RT->MusicSet)
				{
					StartMusicSet(Want, bTitle, Chapter);
				}
				else
				{
					RT->MusicPhrase = Next;
					if (bLoopTop)
					{
						RT->Pass = DCAudio::PlanMusicPass(bTitle, Chapter, RT->Pass.Melody, false, RT->Rand.FRand(), RT->Rand.FRand(), RT->Rand.FRand());
					}
					PlanMusicPhrase(bTitle, Chapter);
				}
			}
		}
	}
	else if (RT->MusicSet >= 0 && RT->MusicLevel < 0.005f)
	{
		StopMusic(false);
	}
	ApplyMusicVolumes(DeltaTime);
}

void UDCAudioSubsystem::StartMusicSet(int32 Set, bool bTitle, int32 Chapter)
{
	StopMusic(true);
	RT->MusicSet = Set;
	RT->bMusicTitle = bTitle;
	RT->PassesOnSet = 0;
	RT->MusicPhrase = 0;
	RT->MusicLoopSec = DCAudio::MusicLoopSeconds(Set);
	RT->LpfApplied = -1.f;
	for (int32 i = 0; i < DCAudio::MusicStemCount; ++i) { RT->LayerGain[i] = 0.f; RT->LayerApplied[i] = -1.f; }

	MusicComps.SetNum(DCAudio::MusicStemCount);
	int32 Live = 0;
	for (int32 i = 0; i < DCAudio::MusicStemCount; ++i)
	{
		USoundBase* S = LoadSound(DCAudio::MakePath(DCAudio::MusicAssetName(Set, i)));
		UAudioComponent* C = S ? MakeComponent(S) : nullptr;
		MusicComps[i] = C;
		if (C)
		{
			C->bOverridePriority = true;
			C->Priority = 3.f;
			C->SetVolumeMultiplier(0.001f);
			++Live;
			if (i == DCAudio::StemCore && S->GetDuration() > 10.f && S->GetDuration() < 1000.f) { RT->MusicLoopSec = S->GetDuration(); }
		}
	}
	// Start every stem in the same frame: same length + sample-accurate looping keeps them locked from here on.
	for (UAudioComponent* C : MusicComps) { if (C) { C->Play(0.f); } }
	RT->MusicStart = FPlatformTime::Seconds();

	RT->Pass = DCAudio::PlanMusicPass(bTitle, Chapter, RT->Pass.Melody, true, RT->Rand.FRand(), RT->Rand.FRand(), RT->Rand.FRand());
	PlanMusicPhrase(bTitle, Chapter);
	UE_LOG(LogDC, Display, TEXT("DCAudio: music -> %s (%s, chapter %d, %.1fs loop, %d/%d stems)"), DCAudio::MusicSetName(Set),
		bTitle ? TEXT("title") : (RT->bMusicDay ? TEXT("day") : TEXT("night")), Chapter, RT->MusicLoopSec, Live, (int32)DCAudio::MusicStemCount);
}

void UDCAudioSubsystem::PlanMusicPhrase(bool bTitle, int32 Chapter)
{
	const DCAudio::FMusicLayers L = DCAudio::ComputeMusicLayers(bTitle, Chapter, RT->Pass, RT->MusicPhrase, RT->PassesOnSet == 0);
	for (int32 i = 0; i < DCAudio::MusicStemCount; ++i) { RT->LayerTarget[i] = L.Gain[i]; }
	UE_LOG(LogDC, Verbose, TEXT("DCAudio: %s phrase %d: core %.2f groove %.2f melA %.2f melB %.2f color %.2f"), DCAudio::MusicSetName(RT->MusicSet),
		RT->MusicPhrase % DCAudio::MusicPhrasesPerLoop, L.Gain[0], L.Gain[1], L.Gain[2], L.Gain[3], L.Gain[4]);
}

void UDCAudioSubsystem::StopMusic(bool bFadeOut)
{
	auto Kill = [](TArray<TObjectPtr<UAudioComponent>>& Comps)
	{
		for (UAudioComponent* C : Comps)
		{
			if (IsValid(C)) { C->Stop(); C->DestroyComponent(); }
		}
		Comps.Reset();
	};
	Kill(MusicFadeComps);
	if (bFadeOut && RT.IsValid() && MusicComps.Num() > 0)
	{
		MusicFadeComps = MusicComps;
		MusicComps.Reset();
		for (int32 i = 0; i < DCAudio::MusicStemCount; ++i) { RT->FadeBase[i] = RT->LayerGain[i]; }
		RT->FadeGain = 1.f;
	}
	else
	{
		Kill(MusicComps);
	}
	if (RT.IsValid())
	{
		if (!bFadeOut) { RT->FadeGain = 0.f; }
		RT->MusicSet = -1;
	}
}

void UDCAudioSubsystem::ApplyMusicVolumes(float DeltaTime)
{
	const UDCRunSubsystem* Run = RT->Run.Get();
	const EDCGamePhase Phase = Run ? Run->GetPhase() : EDCGamePhase::Title;
	const bool bPaused = Run && Run->HasActiveRun() && Run->IsPaused();
	const float Dt = FMath::Max(DeltaTime, 0.f);
	auto Ease = [Dt](float Tau) { return 1.f - FMath::Exp(-Dt / Tau); };

	RT->MusicLevel += ((Phase == EDCGamePhase::GameOver ? 0.f : 1.f) - RT->MusicLevel) * Ease(1.0f);
	RT->MusicDuck += ((bPaused ? 0.6f : 1.f) - RT->MusicDuck) * Ease(0.35f);
	const float LpfTarget = bPaused ? MusicPausedLpfHz : 20000.f;
	RT->LpfHz = FMath::Exp(FMath::Lerp(FMath::Loge(RT->LpfHz), FMath::Loge(LpfTarget), Ease(0.35f)));

	const float Channel = Volumes[(int32)EDCVolumeChannel::Master] * Volumes[(int32)EDCVolumeChannel::Music] * RT->MusicLevel * RT->MusicDuck;
	const float KLayer = Ease(MusicLayerTau);
	for (int32 i = 0; i < DCAudio::MusicStemCount; ++i) { RT->LayerGain[i] += (RT->LayerTarget[i] - RT->LayerGain[i]) * KLayer; }
	RT->FadeGain *= FMath::Exp(-Dt / MusicFadeOutTau);

	const bool bLpfChanged = RT->LpfApplied < 0.f || FMath::Abs(RT->LpfHz - RT->LpfApplied) > RT->LpfApplied * 0.02f;
	auto Filter = [this](UAudioComponent* C)
	{
		const bool bOn = RT->LpfHz < 18000.f;
		C->SetLowPassFilterEnabled(bOn);
		if (bOn) { C->SetLowPassFilterFrequency(RT->LpfHz); }
	};
	for (int32 i = 0; i < MusicComps.Num(); ++i)
	{
		UAudioComponent* C = MusicComps[i];
		if (!C) { continue; }
		// Floor at 0.001 rather than 0 so resting layers keep their place in the loop.
		const float V = FMath::Max(RT->LayerGain[i] * Channel, 0.001f);
		if (FMath::Abs(V - RT->LayerApplied[i]) > 0.0015f) { C->SetVolumeMultiplier(V); RT->LayerApplied[i] = V; }
		if (bLpfChanged) { Filter(C); }
	}
	if (MusicFadeComps.Num() > 0)
	{
		if (RT->FadeGain < 0.01f)
		{
			for (UAudioComponent* C : MusicFadeComps) { if (IsValid(C)) { C->Stop(); C->DestroyComponent(); } }
			MusicFadeComps.Reset();
		}
		else
		{
			for (int32 i = 0; i < MusicFadeComps.Num() && i < DCAudio::MusicStemCount; ++i)
			{
				if (UAudioComponent* C = MusicFadeComps[i])
				{
					C->SetVolumeMultiplier(FMath::Max(RT->FadeBase[i] * RT->FadeGain * Channel, 0.001f));
					if (bLpfChanged) { Filter(C); }
				}
			}
		}
	}
	if (bLpfChanged) { RT->LpfApplied = RT->LpfHz; }
}

// ─────────────────────────────────────────────────────────────────────────────
// Volumes
// ─────────────────────────────────────────────────────────────────────────────

void UDCAudioSubsystem::SetVolume(EDCVolumeChannel Channel, float Volume01)
{
	const int32 I = FMath::Clamp((int32)Channel, 0, NumVolumeChannels - 1);
	Volumes[I] = FMath::Clamp(Volume01, 0.f, 1.f);
	if (RT.IsValid())
	{
		// Beds pick the new channel volume up on the next tick (UpdateAmbience applies it immediately).
		RT->bVolumesDirty = true;
		RT->VolumesDirtySince = RT->Now;
		if (RT->BoundWorld.IsValid()) { UpdateAmbience(0.f, false); ApplyMusicVolumes(0.f); }
	}
}

float UDCAudioSubsystem::GetVolume(EDCVolumeChannel Channel) const { return Volumes[FMath::Clamp((int32)Channel, 0, NumVolumeChannels - 1)]; }

void UDCAudioSubsystem::LoadVolumes()
{
	if (!GConfig) { return; }
	for (int32 i = 0; i < NumVolumeChannels; ++i)
	{
		float V = Volumes[i];
		if (GConfig->GetFloat(ConfigSection, ChannelKeys[i], V, GGameUserSettingsIni)) { Volumes[i] = FMath::Clamp(V, 0.f, 1.f); }
	}
}

void UDCAudioSubsystem::SaveVolumes()
{
	if (RT.IsValid()) { RT->bVolumesDirty = false; }
	if (!GConfig) { return; }
	for (int32 i = 0; i < NumVolumeChannels; ++i) { GConfig->SetFloat(ConfigSection, ChannelKeys[i], Volumes[i], GGameUserSettingsIni); }
	GConfig->Flush(false, GGameUserSettingsIni);
}

// ─────────────────────────────────────────────────────────────────────────────
// Run delegates → sound
// ─────────────────────────────────────────────────────────────────────────────

void UDCAudioSubsystem::HandleMissionStarted(const FDCMission&)
{
	Schedule(DCSfx::MissionStart, 0.f);
	Schedule(DCSfx::GarageDoor, 0.4f);
	Schedule(DCSfx::CarDepart, 1.5f);
}

void UDCAudioSubsystem::HandleMissionCompleted(const FDCMissionReport& Report)
{
	Schedule(DCSfx::MissionComplete, 0.f);
	Schedule(DCSfx::CarArrive, 0.15f, 0.8f);
	switch (Report.Outcome)
	{
	case EDCMissionOutcome::Success: Schedule(DCSfx::Success, 0.75f); break;
	case EDCMissionOutcome::Failed:  Schedule(DCSfx::Fail, 0.75f); break;
	case EDCMissionOutcome::Busted:  Schedule(DCSfx::Busted, 0.75f); break;
	default: break;
	}
	if (Report.Outcome == EDCMissionOutcome::Success && Report.DirectorTake > 0) { Schedule(DCSfx::Cash, 1.5f, 0.8f); }
}

void UDCAudioSubsystem::HandleLevelUp(int32, int32) { Schedule(DCSfx::LevelUp, 1.6f); }

void UDCAudioSubsystem::HandleRoomChanged(const FDCRoom& Room)
{
	if (Room.State == EDCRoomState::Building) { Schedule(DCSfx::BuildStart, 0.f); }
	else if (Room.State == EDCRoomState::Built) { Schedule(DCSfx::RoomBuilt, 0.f); }
}

void UDCAudioSubsystem::HandlePerkOffered(const TArray<FDCPerk>&) { Schedule(DCSfx::Perk, 2.2f); }
void UDCAudioSubsystem::HandlePayday(int32, int32) { Schedule(DCSfx::Payday, 0.f); }

void UDCAudioSubsystem::HandlePhaseChanged(EDCGamePhase Phase)
{
	if (Phase == EDCGamePhase::GameOver) { Schedule(DCSfx::Fail, 0.3f); }
	else if (Phase == EDCGamePhase::Won) { Schedule(DCSfx::Success, 0.3f); Schedule(DCSfx::LevelUp, 1.2f); }
	else if (Phase == EDCGamePhase::Title && RT.IsValid()) { RT->Queue.Reset(); }
}

// ─────────────────────────────────────────────────────────────────────────────
// Test access
// ─────────────────────────────────────────────────────────────────────────────

int32 FDCAudioTestAccess::QueuedCount(const UDCAudioSubsystem* A, FName Sfx)
{
	if (!A || !A->RT.IsValid()) { return 0; }
	int32 N = 0;
	for (const FDCAudioRuntime::FQueued& Q : A->RT->Queue) { N += Q.Sfx == Sfx ? 1 : 0; }
	return N;
}

void FDCAudioTestAccess::ClearQueue(UDCAudioSubsystem* A)
{
	if (A && A->RT.IsValid()) { A->RT->Queue.Reset(); }
}

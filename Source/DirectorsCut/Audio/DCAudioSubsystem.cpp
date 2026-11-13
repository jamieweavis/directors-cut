// Director's Cut: UDCAudioSubsystem implementation (Agent H).
//
//  - SFX by canonical name (DCSfx::*) via soft paths under /Game/DC/Audio, 2D, with per-sound base gain,
//    small random pitch variation, round-robin variants and throttling of spammy UI sounds.
//  - Music state machine on two UAudioComponents with ~1.5 s crossfades. Stingers are SFX: they never
//    interrupt the music, they only duck it briefly.
//  - Ambience bed (rain + projector + neon) that ducks during Dailies, plus a distant siren every 40-90 s.
//  - Volume channels Master/Music/SFX/Ambience applied live and persisted in GameUserSettings.ini.
//  - Scores the game automatically from UDCRunSubsystem delegates.
//  - Missing assets are logged once and stay silent.
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
	constexpr float MusicCrossfade = 1.5f;
	constexpr float MusicBaseGain = 0.6f;
	const TCHAR* ConfigSection = TEXT("/Script/DirectorsCut.DCAudio");
	const TCHAR* ChannelKeys[4] = { TEXT("MasterVolume"), TEXT("MusicVolume"), TEXT("SFXVolume"), TEXT("AmbienceVolume") };

	// Ambience bed base gains: rain, projector, neon.
	constexpr float AmbienceBase[3] = { 0.55f, 0.28f, 0.09f };
	constexpr float SirenBase = 0.45f;
	constexpr float AmbienceDuckLevel = 0.6f;

	struct FSfxTuning
	{
		float Gain = 0.8f;
		float PitchJitter = 0.f;  // +/- fraction
		float MinInterval = 0.f;  // seconds between plays of the same name
	};

	FSfxTuning TuningFor(FName N)
	{
		if (N == DCSfx::Click)        return { 0.75f, 0.04f, 0.035f };
		if (N == DCSfx::Hover)        return { 0.40f, 0.05f, 0.06f };
		if (N == DCSfx::PanelOpen)    return { 0.65f, 0.05f, 0.08f };
		if (N == DCSfx::PanelClose)   return { 0.60f, 0.05f, 0.08f };
		if (N == DCSfx::Typewriter)   return { 0.65f, 0.07f, 0.03f };
		if (N == DCSfx::Stamp)        return { 0.95f, 0.03f, 0.10f };
		if (N == DCSfx::Cash)         return { 0.80f, 0.02f, 0.15f };
		if (N == DCSfx::CounterTick)  return { 0.40f, 0.03f, 0.04f };
		if (N == DCSfx::Clapper)      return { 0.95f, 0.02f, 0.15f };
		if (N == DCSfx::DiceRoll)     return { 0.75f, 0.03f, 0.10f };
		if (N == DCSfx::Commit)       return { 0.80f, 0.f,   0.15f };
		if (N == DCSfx::Error)        return { 0.70f, 0.f,   0.15f };
		if (N == DCSfx::GarageDoor)   return { 0.70f, 0.f,   0.50f };
		if (N == DCSfx::CarDepart)    return { 0.50f, 0.f,   0.50f };
		if (DCAudio::IsStinger(N))    return { 0.85f, 0.f,   0.25f };
		return {};
	}
}

struct FDCAudioRuntime
{
	FTSTicker::FDelegateHandle TickHandle;
	TSharedPtr<FStreamableHandle> PreloadHandle;
	TWeakObjectPtr<UWorld> BoundWorld;
	TSet<FSoftObjectPath> MissingLogged;
	TMap<FName, double> LastPlay;
	TMap<FName, int32> LastVariant;
	FRandomStream Rand{ (int32)FPlatformTime::Cycles() };
	bool bActiveIsA = true;
	EDCMusicState PlayingState = EDCMusicState::None;
	bool bAmbienceDucked = false;
	double Now = 0.0;
	double NextSirenTime = 0.0;
	double MusicDuckEnd = 0.0;
	bool bMusicDucked = false;
	bool bVolumesDirty = false;
	double VolumesDirtySince = 0.0;
	FDelegateHandle RunHandles[6];
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

	// Warm everything up asynchronously so the first click / crossfade doesn't hitch.
	TArray<FSoftObjectPath> All;
	for (const FName& N : DCAudio::AllSfxNames())
	{
		for (int32 V = 0; V < DCAudio::SfxVariantCount(N); ++V) { All.Add(DCAudio::SfxPath(N, V)); }
	}
	for (EDCMusicState S : { EDCMusicState::Title, EDCMusicState::Planning, EDCMusicState::Dailies, EDCMusicState::Win, EDCMusicState::GameOver })
	{
		All.Add(DCAudio::MusicPath(S));
	}
	for (const FString& A : DCAudio::AmbienceBedNames()) { All.Add(DCAudio::MakePath(A)); }
	All.Add(DCAudio::SirenPath());
	if (UAssetManager* AM = UAssetManager::GetIfInitialized())
	{
		RT->PreloadHandle = AM->GetStreamableManager().RequestAsyncLoad(All, FStreamableDelegate(), FStreamableManager::AsyncLoadHighPriority);
	}

	// Bind to the run so the game is scored automatically.
	if (UDCRunSubsystem* Run = Collection.InitializeDependency<UDCRunSubsystem>())
	{
		RT->RunHandles[0] = Run->OnPhaseChanged.AddUObject(this, &UDCAudioSubsystem::HandlePhaseChanged);
		RT->RunHandles[1] = Run->OnPlanCommitted.AddUObject(this, &UDCAudioSubsystem::HandlePlanCommitted);
		RT->RunHandles[2] = Run->OnNightResolved.AddUObject(this, &UDCAudioSubsystem::HandleNightResolved);
		RT->RunHandles[3] = Run->OnCriminalLevelUp.AddUObject(this, &UDCAudioSubsystem::HandleLevelUp);
		RT->RunHandles[4] = Run->OnRoomChanged.AddUObject(this, &UDCAudioSubsystem::HandleRoomChanged);
		RT->RunHandles[5] = Run->OnPerkOffered.AddUObject(this, &UDCAudioSubsystem::HandlePerkOffered);
		if (Run->GetPhase() == EDCGamePhase::Title) { MusicState = EDCMusicState::Title; }
		else { HandlePhaseChanged(Run->GetPhase()); }
	}
	else
	{
		MusicState = EDCMusicState::Title;
	}

	RT->TickHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(this, &UDCAudioSubsystem::TickAudio), 0.1f);
}

void UDCAudioSubsystem::Deinitialize()
{
	if (RT.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(RT->TickHandle);
		if (UGameInstance* GI = GetGameInstance())
		{
			if (UDCRunSubsystem* Run = GI->GetSubsystem<UDCRunSubsystem>())
			{
				Run->OnPhaseChanged.Remove(RT->RunHandles[0]);
				Run->OnPlanCommitted.Remove(RT->RunHandles[1]);
				Run->OnNightResolved.Remove(RT->RunHandles[2]);
				Run->OnCriminalLevelUp.Remove(RT->RunHandles[3]);
				Run->OnRoomChanged.Remove(RT->RunHandles[4]);
				Run->OnPerkOffered.Remove(RT->RunHandles[5]);
			}
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
	RT->Now += DeltaTime;

	if (RT->bVolumesDirty && RT->Now - RT->VolumesDirtySince > 1.0) { SaveVolumes(); }

	if (!EnsureComponents()) { return true; }

	// Stinger duck release.
	if (RT->bMusicDucked && RT->Now >= RT->MusicDuckEnd)
	{
		RT->bMusicDucked = false;
		for (UAudioComponent* C : { MusicA.Get(), MusicB.Get() })
		{
			if (C && C->IsPlaying()) { C->AdjustVolume(1.0f, 1.0f); }
		}
	}

	// Distant siren every 40-90 s.
	if (RT->NextSirenTime <= 0.0)
	{
		RT->NextSirenTime = RT->Now + RT->Rand.FRandRange(20.f, 45.f);
	}
	else if (RT->Now >= RT->NextSirenTime)
	{
		RT->NextSirenTime = RT->Now + RT->Rand.FRandRange(40.f, 90.f);
		const float Vol = SirenBase * Volumes[(int32)EDCVolumeChannel::Master] * Volumes[(int32)EDCVolumeChannel::Ambience]
			* (RT->bAmbienceDucked ? AmbienceDuckLevel : 1.f);
		if (USoundBase* Siren = LoadSound(DCAudio::SirenPath()); Siren && Vol > 0.001f)
		{
			UGameplayStatics::PlaySound2D(GetAudioWorld(), Siren, Vol, RT->Rand.FRandRange(0.92f, 1.05f), 0.f, nullptr, nullptr, true);
		}
	}
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Components
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
		if (RT->BoundWorld.IsValid() || MusicA || MusicB) { TearDownComponents(); }
		return false;
	}
	if (RT->BoundWorld.Get() == W) { return true; }

	// New world (first world, map travel, PIE restart): rebuild against it.
	TearDownComponents();
	RT->BoundWorld = W;
	StartAmbience();
	StartMusic(MusicState);
	return true;
}

void UDCAudioSubsystem::TearDownComponents()
{
	auto Kill = [](UAudioComponent* C)
	{
		if (IsValid(C))
		{
			C->Stop();
			C->DestroyComponent();
		}
	};
	Kill(MusicA);
	Kill(MusicB);
	for (UAudioComponent* C : AmbienceComps) { Kill(C); }
	MusicA = nullptr;
	MusicB = nullptr;
	AmbienceComps.Reset();
	if (RT.IsValid())
	{
		RT->BoundWorld = nullptr;
		RT->PlayingState = EDCMusicState::None;
		RT->bMusicDucked = false;
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

	if (DCAudio::IsStinger(SfxName)) { DuckMusic(Sound->GetDuration() > 0.f ? FMath::Min(Sound->GetDuration() * 0.6f, 3.f) : 2.f); }
}

void UDCAudioSubsystem::DuckMusic(float Seconds)
{
	if (!RT.IsValid()) { return; }
	RT->bMusicDucked = true;
	RT->MusicDuckEnd = FMath::Max(RT->MusicDuckEnd, RT->Now + Seconds);
	for (UAudioComponent* C : { MusicA.Get(), MusicB.Get() })
	{
		if (C && C->IsPlaying()) { C->AdjustVolume(0.15f, 0.45f); }
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Music
// ─────────────────────────────────────────────────────────────────────────────

void UDCAudioSubsystem::SetMusicState(EDCMusicState State)
{
	MusicState = State;
	if (!RT.IsValid()) { return; }
	SetAmbienceDucked(State == EDCMusicState::Dailies);
	if (RT->BoundWorld.IsValid()) { StartMusic(State); }
	// else: EnsureComponents() starts it as soon as a world exists.
}

EDCMusicState UDCAudioSubsystem::GetMusicState() const { return MusicState; }

UAudioComponent* UDCAudioSubsystem::ActiveMusic() const
{
	return RT.IsValid() ? (RT->bActiveIsA ? MusicA.Get() : MusicB.Get()) : nullptr;
}

float UDCAudioSubsystem::MusicGain() const
{
	return MusicBaseGain * Volumes[(int32)EDCVolumeChannel::Master] * Volumes[(int32)EDCVolumeChannel::Music];
}

void UDCAudioSubsystem::StartMusic(EDCMusicState State)
{
	UAudioComponent* Old = ActiveMusic();
	if (State == RT->PlayingState && Old && Old->IsPlaying()) { return; }

	const bool bCue = (State == EDCMusicState::Win || State == EDCMusicState::GameOver);
	if (Old && Old->IsPlaying()) { Old->FadeOut(bCue ? 0.6f : MusicCrossfade, 0.f); }
	RT->PlayingState = State;
	if (State == EDCMusicState::None) { return; }

	USoundBase* Sound = LoadSound(DCAudio::MusicPath(State));
	if (!Sound) { return; }

	RT->bActiveIsA = !RT->bActiveIsA;
	TObjectPtr<UAudioComponent>& Slot = RT->bActiveIsA ? MusicA : MusicB;
	if (!IsValid(Slot)) { Slot = MakeComponent(Sound); }
	if (!Slot) { return; }
	Slot->Stop();
	Slot->SetSound(Sound);
	Slot->SetVolumeMultiplier(FMath::Max(MusicGain(), 0.0001f));
	Slot->AdjustVolume(0.f, RT->bMusicDucked ? 0.45f : 1.f);
	Slot->FadeIn(bCue ? 0.05f : MusicCrossfade, 1.f);
}

// ─────────────────────────────────────────────────────────────────────────────
// Ambience
// ─────────────────────────────────────────────────────────────────────────────

float UDCAudioSubsystem::AmbienceGain(int32 Index) const
{
	return AmbienceBase[FMath::Clamp(Index, 0, 2)] * Volumes[(int32)EDCVolumeChannel::Master] * Volumes[(int32)EDCVolumeChannel::Ambience];
}

void UDCAudioSubsystem::StartAmbience()
{
	const TArray<FString>& Names = DCAudio::AmbienceBedNames();
	AmbienceComps.SetNum(Names.Num());
	for (int32 i = 0; i < Names.Num(); ++i)
	{
		USoundBase* S = LoadSound(DCAudio::MakePath(Names[i]));
		UAudioComponent* C = S ? MakeComponent(S) : nullptr;
		AmbienceComps[i] = C;
		if (!C) { continue; }
		C->SetVolumeMultiplier(FMath::Max(AmbienceGain(i), 0.0001f));
		C->AdjustVolume(0.f, RT->bAmbienceDucked ? AmbienceDuckLevel : 1.f);
		// Stagger start offsets so the beds don't phase-lock with each other on every restart.
		C->FadeIn(3.0f, 1.f, RT->Rand.FRandRange(0.f, 8.f));
	}
}

void UDCAudioSubsystem::SetAmbienceDucked(bool bDuck)
{
	if (RT->bAmbienceDucked == bDuck) { return; }
	RT->bAmbienceDucked = bDuck;
	for (UAudioComponent* C : AmbienceComps)
	{
		if (C) { C->AdjustVolume(1.0f, bDuck ? AmbienceDuckLevel : 1.f); }
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Volumes
// ─────────────────────────────────────────────────────────────────────────────

void UDCAudioSubsystem::SetVolume(EDCVolumeChannel Channel, float Volume01)
{
	const int32 I = FMath::Clamp((int32)Channel, 0, 3);
	Volumes[I] = FMath::Clamp(Volume01, 0.f, 1.f);
	ApplyVolumes();
	if (RT.IsValid())
	{
		RT->bVolumesDirty = true;
		RT->VolumesDirtySince = RT->Now;
	}
}

float UDCAudioSubsystem::GetVolume(EDCVolumeChannel Channel) const { return Volumes[FMath::Clamp((int32)Channel, 0, 3)]; }

void UDCAudioSubsystem::ApplyVolumes()
{
	// 0.0001 rather than 0 so the components keep their playback position instead of virtualising/stopping.
	for (UAudioComponent* C : { MusicA.Get(), MusicB.Get() })
	{
		if (C) { C->SetVolumeMultiplier(FMath::Max(MusicGain(), 0.0001f)); }
	}
	for (int32 i = 0; i < AmbienceComps.Num(); ++i)
	{
		if (AmbienceComps[i]) { AmbienceComps[i]->SetVolumeMultiplier(FMath::Max(AmbienceGain(i), 0.0001f)); }
	}
}

void UDCAudioSubsystem::LoadVolumes()
{
	if (!GConfig) { return; }
	for (int32 i = 0; i < 4; ++i)
	{
		float V = Volumes[i];
		if (GConfig->GetFloat(ConfigSection, ChannelKeys[i], V, GGameUserSettingsIni)) { Volumes[i] = FMath::Clamp(V, 0.f, 1.f); }
	}
}

void UDCAudioSubsystem::SaveVolumes()
{
	if (RT.IsValid()) { RT->bVolumesDirty = false; }
	if (!GConfig) { return; }
	for (int32 i = 0; i < 4; ++i) { GConfig->SetFloat(ConfigSection, ChannelKeys[i], Volumes[i], GGameUserSettingsIni); }
	GConfig->Flush(false, GGameUserSettingsIni);
}

// ─────────────────────────────────────────────────────────────────────────────
// Run delegates
// ─────────────────────────────────────────────────────────────────────────────

void UDCAudioSubsystem::HandlePhaseChanged(EDCGamePhase Phase)
{
	switch (Phase)
	{
	case EDCGamePhase::Title:    SetMusicState(EDCMusicState::Title); break;
	case EDCGamePhase::Planning: SetMusicState(EDCMusicState::Planning); break;
	case EDCGamePhase::Dailies:  SetMusicState(EDCMusicState::Dailies); break;
	case EDCGamePhase::GameOver: SetMusicState(EDCMusicState::GameOver); break;
	case EDCGamePhase::Won:      SetMusicState(EDCMusicState::Win); break;
	default: break;
	}
}

void UDCAudioSubsystem::HandlePlanCommitted(const FDCCrewPlan&) { PlaySFX(DCSfx::Commit); }
void UDCAudioSubsystem::HandleNightResolved(const FDCNightReport&) { PlaySFX(DCSfx::Clapper); }
void UDCAudioSubsystem::HandleLevelUp(int32, int32) { PlaySFX(DCSfx::StingLevelUp); }
void UDCAudioSubsystem::HandleRoomChanged(const FDCRoom&) { PlaySFX(DCSfx::StingRoom); }
void UDCAudioSubsystem::HandlePerkOffered(const TArray<FDCPerk>&) { PlaySFX(DCSfx::StingPerk); }

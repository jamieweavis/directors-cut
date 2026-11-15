// Director's Cut: audio tests (Agent H). DirectorsCut.Audio.*
#include "Misc/AutomationTest.h"
#include "Audio/DCAudioAssets.h"
#include "Core/DCAudioSubsystem.h"
#include "Core/DCRunSubsystem.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Sound/SoundWave.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDCAudioAssetsTest, "DirectorsCut.Audio.AssetsResolve",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDCAudioAssetsTest::RunTest(const FString& Parameters)
{
	auto Check = [this](const FSoftObjectPath& Path, bool bExpectLoop)
	{
		USoundWave* W = Cast<USoundWave>(Path.TryLoad());
		if (TestNotNull(*FString::Printf(TEXT("Loadable sound %s"), *Path.ToString()), W))
		{
			TestEqual(*FString::Printf(TEXT("Looping flag on %s"), *Path.ToString()), (bool)W->bLooping, bExpectLoop);
			TestTrue(*FString::Printf(TEXT("Non-zero duration %s"), *Path.ToString()), W->Duration > 0.f);
		}
	};

	TestEqual(TEXT("Every DCSfx name is listed"), DCAudio::AllSfxNames().Num(), 21);
	for (const FName& N : DCAudio::AllSfxNames())
	{
		for (int32 V = 0; V < DCAudio::SfxVariantCount(N); ++V) { Check(DCAudio::SfxPath(N, V), false); }
	}
	for (int32 B = 0; B < DCAudio::BedCount; ++B) { Check(DCAudio::MakePath(DCAudio::BedAssetName(B)), true); }
	for (const FString& S : DCAudio::DayOneShots()) { Check(DCAudio::MakePath(S), false); }
	for (const FString& S : DCAudio::NightOneShots()) { Check(DCAudio::MakePath(S), false); }
	for (const FString& S : DCAudio::AllMusicAssetNames()) { Check(DCAudio::MakePath(S), true); }

	// Every stem of a set has the same length (they loop locked together), matching its tempo.
	for (int32 Set = 0; Set < DCAudio::MusicSetCount; ++Set)
	{
		for (int32 Stem = 0; Stem < DCAudio::MusicStemCount; ++Stem)
		{
			if (USoundWave* W = Cast<USoundWave>(DCAudio::MakePath(DCAudio::MusicAssetName(Set, Stem)).TryLoad()))
			{
				TestTrue(*FString::Printf(TEXT("%s is one 32-bar loop"), *DCAudio::MusicAssetName(Set, Stem)),
					FMath::Abs(W->Duration - (float)DCAudio::MusicLoopSeconds(Set)) < 0.01f);
			}
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDCAudioAssetSetTest, "DirectorsCut.Audio.AssetSet",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDCAudioAssetSetTest::RunTest(const FString& Parameters)
{
	IAssetRegistry& AR = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	AR.ScanPathsSynchronous({ FString(DCPaths::AudioRoot) }, /*bForceRescan*/ true);
	TArray<FAssetData> Assets;
	AR.GetAssetsByPath(FName(DCPaths::AudioRoot), Assets, /*bRecursive*/ true);
	TestTrue(TEXT("Audio folder has assets"), Assets.Num() > 0);
	TestEqual(TEXT("Twenty music stems"), DCAudio::AllMusicAssetNames().Num(), 20);

	const TArray<FString> Expected = DCAudio::AllAssetNames();
	for (const FAssetData& A : Assets)
	{
		const FString Name = A.AssetName.ToString();
		TestFalse(*FString::Printf(TEXT("No obsolete asset: %s"), *Name), DCAudio::ObsoleteAssetNames().Contains(Name));
		TestTrue(*FString::Printf(TEXT("Asset is in the v2 set: %s"), *Name), Expected.Contains(Name));
	}

	TArray<FString> Wavs;
	IFileManager::Get().FindFiles(Wavs, *FPaths::Combine(FPaths::ProjectContentDir(), DCPaths::RawAudioDir, TEXT("MUS_*.wav")), true, false);
	TestEqual(TEXT("A MUS_*.wav source per music stem"), Wavs.Num(), DCAudio::AllMusicAssetNames().Num());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDCAudioMixTest, "DirectorsCut.Audio.AmbienceMix",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDCAudioMixTest::RunTest(const FString& Parameters)
{
	using namespace DCAudio;
	TestTrue(TEXT("Night at 02:00"), DayFactor(2.f) < 0.01f);
	TestTrue(TEXT("Day at 12:00"), DayFactor(12.f) > 0.99f);
	TestTrue(TEXT("Dawn ramps"), DayFactor(6.5f) > 0.2f && DayFactor(6.5f) < 0.8f);
	TestTrue(TEXT("Night at 22:00"), DayFactor(22.f) < 0.01f);

	const FAmbienceMix Noon = ComputeAmbienceMix(true, 12.f, 0.f, false);
	TestTrue(TEXT("Noon: day bed up"), Noon.Bed[BedRuralDay] > 0.95f);
	TestTrue(TEXT("Noon: night bed down"), Noon.Bed[BedRuralNight] < 0.05f);
	TestTrue(TEXT("Room tone always"), Noon.Bed[BedInterior] > 0.5f);
	const FAmbienceMix Night = ComputeAmbienceMix(true, 1.f, 0.f, false);
	TestTrue(TEXT("Night: night bed up"), Night.Bed[BedRuralNight] > 0.95f && Night.Bed[BedRuralDay] < 0.05f);
	const FAmbienceMix Dusk = ComputeAmbienceMix(true, 19.75f, 0.f, false);
	const float Power = FMath::Square(Dusk.Bed[BedRuralDay]) + FMath::Square(Dusk.Bed[BedRuralNight]);
	TestTrue(TEXT("Dusk crossfade is equal-power"), FMath::IsNearlyEqual(Power, 1.f, 0.01f));
	const FAmbienceMix Paused = ComputeAmbienceMix(true, 12.f, 0.f, true);
	TestTrue(TEXT("Pause ducks"), Paused.Bed[BedRuralDay] < Noon.Bed[BedRuralDay] && Paused.Bed[BedInterior] < Noon.Bed[BedInterior]);
	const FAmbienceMix Title = ComputeAmbienceMix(false, 12.f, 0.f, false);
	TestTrue(TEXT("Title: room tone + night low, no day"), Title.Bed[BedInterior] > 0.5f && Title.Bed[BedRuralDay] == 0.f
		&& Title.Bed[BedRuralNight] > 0.f && Title.Bed[BedRuralNight] < 0.6f);
	const FAmbienceMix Wet = ComputeAmbienceMix(true, 12.f, 1.f, false);
	TestTrue(TEXT("Rain bed follows rain"), Wet.Bed[BedRain] > 0.5f && Noon.Bed[BedRain] == 0.f);

	int32 Rainy = 0;
	for (int32 Day = 1; Day <= 60; ++Day)
	{
		TestEqual(TEXT("Rain is deterministic"), IsRainyDay(42, Day), IsRainyDay(42, Day));
		Rainy += IsRainyDay(42, Day) ? 1 : 0;
		float MaxRain = 0.f;
		for (float H = 0.f; H < 24.f; H += 0.25f) { MaxRain = FMath::Max(MaxRain, RainAmount(42, Day, H)); }
		TestEqual(TEXT("Rain amount only on rainy days"), MaxRain > 0.f, IsRainyDay(42, Day));
	}
	TestFalse(TEXT("Never rains on day 1"), IsRainyDay(42, 1) || IsRainyDay(7, 1));
	TestTrue(TEXT("Some but not most days rain"), Rainy >= 5 && Rainy <= 30);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDCAudioMusicPlanTest, "DirectorsCut.Audio.MusicPlan",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDCAudioMusicPlanTest::RunTest(const FString& Parameters)
{
	using namespace DCAudio;
	TestEqual(TEXT("Chapter clamps low"), MusicChapter(0), 1);
	TestEqual(TEXT("Chapter clamps high"), MusicChapter(9), 4);
	TestTrue(TEXT("Noon is day"), MusicIsDay(12.f, false));
	TestFalse(TEXT("2am is night"), MusicIsDay(2.f, true));
	// Hysteresis: a dusk hour in the middle of the ramp keeps whatever was playing.
	const float MidDusk = 19.75f;
	TestEqual(TEXT("Hysteresis keeps day"), MusicIsDay(MidDusk, true), true);
	TestEqual(TEXT("Hysteresis keeps night"), MusicIsDay(MidDusk, false), false);

	TestEqual(TEXT("Title plays NightA"), ChooseMusicSet(true, true, 3, -1, 0, 0.f), (int32)SetNightA);
	for (float Roll = 0.f; Roll < 1.f; Roll += 0.05f)
	{
		TestEqual(TEXT("Chapter 1 day is DayA"), ChooseMusicSet(false, true, 1, -1, 0, Roll), (int32)SetDayA);
		TestEqual(TEXT("Chapter 1 night is NightA"), ChooseMusicSet(false, false, 1, SetNightA, 5, Roll), (int32)SetNightA);
		TestTrue(TEXT("Night never picks a day set"), !IsDaySet(ChooseMusicSet(false, false, 4, SetDayB, 0, Roll)));
		TestEqual(TEXT("A set gets two passes"), ChooseMusicSet(false, true, 4, SetDayA, 1, Roll), (int32)SetDayA);
	}
	TestEqual(TEXT("Late chapters lean on the B sets"), ChooseMusicSet(false, true, 4, -1, 0, 0.5f), (int32)SetDayB);
	TestEqual(TEXT("...with the opening tune coming back"), ChooseMusicSet(false, true, 4, -1, 0, 0.95f), (int32)SetDayA);

	// Passes: a new set opens without a melody early on; melodies alternate; title has no melody.
	TestEqual(TEXT("New set, chapter 1: groove first"), PlanMusicPass(false, 1, -1, true, 0.f, 0.f, 0.f).Melody, -1);
	TestEqual(TEXT("Melodies alternate"), PlanMusicPass(false, 2, 0, false, 0.f, 0.f, 0.f).Melody, 1);
	TestEqual(TEXT("Title: no melody"), PlanMusicPass(true, 4, -1, false, 0.f, 0.f, 0.f).Melody, -1);
	TestFalse(TEXT("Chapter 1: no Color"), PlanMusicPass(false, 1, -1, false, 0.f, 0.f, 0.f).bColor);

	// Layers grow with the chapter.
	FMusicPass Full; Full.Melody = 0; Full.bColor = true;
	float Prev = -1.f;
	for (int32 Ch = 1; Ch <= 4; ++Ch)
	{
		float Sum = 0.f;
		for (int32 Ph = 0; Ph < MusicPhrasesPerLoop; ++Ph)
		{
			const FMusicLayers L = ComputeMusicLayers(false, Ch, Full, Ph, false);
			TestEqual(TEXT("Core always on"), L.Gain[StemCore], 1.f);
			TestEqual(TEXT("One melody at a time"), L.Gain[StemMelB], 0.f);
			for (float G : L.Gain) { Sum += G; }
		}
		TestTrue(*FString::Printf(TEXT("Chapter %d is fuller than chapter %d"), Ch, Ch - 1), Sum > Prev);
		Prev = Sum;
	}
	const FMusicLayers T = ComputeMusicLayers(true, 1, FMusicPass(), 0, false);
	TestTrue(TEXT("Title: no drums, no melody"), T.Gain[StemGroove] == 0.f && T.Gain[StemMelA] == 0.f && T.Gain[StemCore] > 0.f);
	const FMusicLayers Ch1Bridge = ComputeMusicLayers(false, 1, Full, 2, false);
	TestEqual(TEXT("Chapter 1 leaves the bridge to the band"), Ch1Bridge.Gain[StemMelA], 0.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDCAudioVolumeTest, "DirectorsCut.Audio.VolumePersistence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDCAudioVolumeTest::RunTest(const FString& Parameters)
{
	float Old[4];
	{
		UGameInstance* GI = NewObject<UGameInstance>(GEngine);
		GI->InitializeStandalone();
		UDCAudioSubsystem* A = GI->GetSubsystem<UDCAudioSubsystem>();
		if (!TestNotNull(TEXT("Audio subsystem"), A)) { GI->Shutdown(); return false; }
		for (int32 i = 0; i < 4; ++i) { Old[i] = A->GetVolume((EDCVolumeChannel)i); }
		A->SetVolume(EDCVolumeChannel::Master, 0.81f);
		A->SetVolume(EDCVolumeChannel::SFX, 0.37f);
		A->SetVolume(EDCVolumeChannel::Ambience, 2.f);
		TestEqual(TEXT("Volume clamps"), A->GetVolume(EDCVolumeChannel::Ambience), 1.f);
		A->SetVolume(EDCVolumeChannel::Ambience, -1.f);
		TestEqual(TEXT("Volume clamps low"), A->GetVolume(EDCVolumeChannel::Ambience), 0.f);
		A->SetVolume(EDCVolumeChannel::Ambience, 0.55f);
		A->SetVolume(EDCVolumeChannel::Music, 0.42f);
		GI->Shutdown(); // persists
	}
	{
		UGameInstance* GI = NewObject<UGameInstance>(GEngine);
		GI->InitializeStandalone();
		UDCAudioSubsystem* A = GI->GetSubsystem<UDCAudioSubsystem>();
		if (TestNotNull(TEXT("Audio subsystem (2)"), A))
		{
			TestTrue(TEXT("Master persisted"), FMath::IsNearlyEqual(A->GetVolume(EDCVolumeChannel::Master), 0.81f, 0.001f));
			TestTrue(TEXT("SFX persisted"), FMath::IsNearlyEqual(A->GetVolume(EDCVolumeChannel::SFX), 0.37f, 0.001f));
			TestTrue(TEXT("Ambience persisted"), FMath::IsNearlyEqual(A->GetVolume(EDCVolumeChannel::Ambience), 0.55f, 0.001f));
			TestTrue(TEXT("Music persisted"), FMath::IsNearlyEqual(A->GetVolume(EDCVolumeChannel::Music), 0.42f, 0.001f));
			for (int32 i = 0; i < 4; ++i) { A->SetVolume((EDCVolumeChannel)i, Old[i]); } // restore the user's settings
		}
		GI->Shutdown();
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDCAudioRunBindingTest, "DirectorsCut.Audio.RunBinding",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDCAudioRunBindingTest::RunTest(const FString& Parameters)
{
	UGameInstance* GI = NewObject<UGameInstance>(GEngine);
	GI->InitializeStandalone();
	UDCAudioSubsystem* Audio = GI->GetSubsystem<UDCAudioSubsystem>();
	UDCRunSubsystem* Run = GI->GetSubsystem<UDCRunSubsystem>();
	if (TestNotNull(TEXT("Audio subsystem"), Audio) && TestNotNull(TEXT("Run subsystem"), Run))
	{
		// Must be safe without an audio device / with missing names.
		for (const FName& N : DCAudio::AllSfxNames()) { Audio->PlaySFX(N); }
		Audio->PlaySFX(FName(TEXT("NoSuchSound")));
		UDCAudioSubsystem::Play(nullptr, DCSfx::Click);

		FDCAudioTestAccess::ClearQueue(Audio);
		FDCMission M;
		Run->OnMissionStarted.Broadcast(M);
		TestEqual(TEXT("MissionStart queued"), FDCAudioTestAccess::QueuedCount(Audio, DCSfx::MissionStart), 1);
		TestEqual(TEXT("GarageDoor queued"), FDCAudioTestAccess::QueuedCount(Audio, DCSfx::GarageDoor), 1);
		TestEqual(TEXT("CarDepart queued"), FDCAudioTestAccess::QueuedCount(Audio, DCSfx::CarDepart), 1);

		// Burst: 20 completions in the same frame (bot AdvanceTime jump) play one sequence.
		FDCAudioTestAccess::ClearQueue(Audio);
		for (int32 i = 0; i < 20; ++i)
		{
			FDCMissionReport R;
			R.Outcome = (i % 3 == 0) ? EDCMissionOutcome::Success : (i % 3 == 1) ? EDCMissionOutcome::Failed : EDCMissionOutcome::Busted;
			Run->OnMissionCompleted.Broadcast(R);
		}
		TestEqual(TEXT("Burst: one MissionComplete"), FDCAudioTestAccess::QueuedCount(Audio, DCSfx::MissionComplete), 1);
		TestEqual(TEXT("Burst: one CarArrive"), FDCAudioTestAccess::QueuedCount(Audio, DCSfx::CarArrive), 1);
		TestEqual(TEXT("Burst: one Success"), FDCAudioTestAccess::QueuedCount(Audio, DCSfx::Success), 1);

		FDCAudioTestAccess::ClearQueue(Audio);
		FDCRoom Room;
		Room.State = EDCRoomState::Building;
		Run->OnRoomChanged.Broadcast(Room);
		Room.State = EDCRoomState::Built;
		Run->OnRoomChanged.Broadcast(Room);
		Run->OnPayday.Broadcast(2, 100);
		Run->OnCriminalLevelUp.Broadcast(1, 2);
		Run->OnPerkOffered.Broadcast(TArray<FDCPerk>());
		TestEqual(TEXT("BuildStart"), FDCAudioTestAccess::QueuedCount(Audio, DCSfx::BuildStart), 1);
		TestEqual(TEXT("RoomBuilt"), FDCAudioTestAccess::QueuedCount(Audio, DCSfx::RoomBuilt), 1);
		TestEqual(TEXT("Payday"), FDCAudioTestAccess::QueuedCount(Audio, DCSfx::Payday), 1);
		TestEqual(TEXT("LevelUp"), FDCAudioTestAccess::QueuedCount(Audio, DCSfx::LevelUp), 1);
		TestEqual(TEXT("Perk"), FDCAudioTestAccess::QueuedCount(Audio, DCSfx::Perk), 1);
		FDCAudioTestAccess::ClearQueue(Audio);
	}
	GI->Shutdown();
	return true;
}

#endif

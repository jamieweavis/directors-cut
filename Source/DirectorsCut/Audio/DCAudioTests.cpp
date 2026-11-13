// Director's Cut: audio asset resolution tests (Agent H).
#include "Misc/AutomationTest.h"
#include "Audio/DCAudioAssets.h"
#include "Sound/SoundWave.h"
#include "Core/DCRunSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"

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

	for (const FName& N : DCAudio::AllSfxNames())
	{
		for (int32 V = 0; V < DCAudio::SfxVariantCount(N); ++V) { Check(DCAudio::SfxPath(N, V), false); }
	}
	Check(DCAudio::MusicPath(EDCMusicState::Title), true);
	Check(DCAudio::MusicPath(EDCMusicState::Planning), true);
	Check(DCAudio::MusicPath(EDCMusicState::Dailies), true);
	Check(DCAudio::MusicPath(EDCMusicState::Win), false);
	Check(DCAudio::MusicPath(EDCMusicState::GameOver), false);
	for (const FString& A : DCAudio::AmbienceBedNames()) { Check(DCAudio::MakePath(A), true); }
	Check(DCAudio::SirenPath(), false);
	TestTrue(TEXT("None music state has no asset"), DCAudio::MusicPath(EDCMusicState::None).IsNull());
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
		TestEqual(TEXT("Title music before a run"), Audio->GetMusicState(), EDCMusicState::Title);
		Run->StartNewRun(1234);
		TestEqual(TEXT("Planning music after StartNewRun"), Audio->GetMusicState(), EDCMusicState::Planning);
		// Must be safe without an audio device / with missing components.
		Audio->PlaySFX(DCSfx::Click);
		Audio->PlaySFX(DCSfx::StingRoom);
		Audio->PlaySFX(FName(TEXT("NoSuchSound")));
		Audio->SetMusicState(EDCMusicState::Dailies);
		TestEqual(TEXT("Explicit Dailies"), Audio->GetMusicState(), EDCMusicState::Dailies);
		const float Old = Audio->GetVolume(EDCVolumeChannel::Music);
		Audio->SetVolume(EDCVolumeChannel::Music, 2.f);
		TestEqual(TEXT("Volume clamps"), Audio->GetVolume(EDCVolumeChannel::Music), 1.f);
		Audio->SetVolume(EDCVolumeChannel::Music, Old);
	}
	GI->Shutdown();
	return true;
}

#endif

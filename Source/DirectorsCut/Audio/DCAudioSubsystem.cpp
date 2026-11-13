// Wave 0 STUB of UDCAudioSubsystem (logs only). Agent H replaces this.
#include "Core/DCAudioSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "DirectorsCut.h"

UDCAudioSubsystem* UDCAudioSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* W = WorldContext ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	UGameInstance* GI = W ? W->GetGameInstance() : nullptr;
	return GI ? GI->GetSubsystem<UDCAudioSubsystem>() : nullptr;
}

void UDCAudioSubsystem::Play(const UObject* WorldContext, FName SfxName)
{
	if (UDCAudioSubsystem* A = Get(WorldContext)) { A->PlaySFX(SfxName); }
}

void UDCAudioSubsystem::Initialize(FSubsystemCollectionBase& Collection) { Super::Initialize(Collection); }
void UDCAudioSubsystem::Deinitialize() { Super::Deinitialize(); }
void UDCAudioSubsystem::PlaySFX(FName SfxName, float VolumeMultiplier, float PitchMultiplier) { UE_LOG(LogDC, Verbose, TEXT("SFX %s"), *SfxName.ToString()); }
void UDCAudioSubsystem::SetMusicState(EDCMusicState State) { MusicState = State; }
EDCMusicState UDCAudioSubsystem::GetMusicState() const { return MusicState; }
void UDCAudioSubsystem::SetVolume(EDCVolumeChannel Channel, float Volume01) { Volumes[(int32)Channel] = FMath::Clamp(Volume01, 0.f, 1.f); }
float UDCAudioSubsystem::GetVolume(EDCVolumeChannel Channel) const { return Volumes[(int32)Channel]; }

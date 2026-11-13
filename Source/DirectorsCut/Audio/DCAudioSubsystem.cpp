// Director's Cut — UDCAudioSubsystem. R0 STUB (lead): plays SFX_<Name> from /Game/DC/Audio if present, otherwise silent.
// Agent H replaces this with the v2 implementation (no music, day/night ambience).
#include "Core/DCAudioSubsystem.h"
#include "Core/DCAssetPaths.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

UDCAudioSubsystem* UDCAudioSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* W = WorldContext ? WorldContext->GetWorld() : nullptr;
	const UGameInstance* GI = W ? W->GetGameInstance() : nullptr;
	return GI ? GI->GetSubsystem<UDCAudioSubsystem>() : nullptr;
}

void UDCAudioSubsystem::Play(const UObject* WorldContext, FName SfxName)
{
	if (UDCAudioSubsystem* A = Get(WorldContext)) { A->PlaySFX(SfxName); }
}

void UDCAudioSubsystem::Initialize(FSubsystemCollectionBase& Collection) { Super::Initialize(Collection); }
void UDCAudioSubsystem::Deinitialize() { SoundCache.Empty(); Super::Deinitialize(); }

USoundBase* UDCAudioSubsystem::LoadSound(FName SfxName)
{
	if (TObjectPtr<USoundBase>* Found = SoundCache.Find(SfxName)) { return *Found; }
	const FString N = TEXT("SFX_") + SfxName.ToString();
	USoundBase* S = LoadObject<USoundBase>(nullptr, *FString::Printf(TEXT("%s/%s.%s"), DCPaths::AudioRoot, *N, *N), nullptr, LOAD_NoWarn | LOAD_Quiet);
	SoundCache.Add(SfxName, S);
	return S;
}

void UDCAudioSubsystem::PlaySFX(FName SfxName, float VolumeMultiplier, float PitchMultiplier)
{
	UWorld* W = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	if (USoundBase* S = W ? LoadSound(SfxName) : nullptr)
	{
		UGameplayStatics::PlaySound2D(W, S, VolumeMultiplier * Volumes[0] * Volumes[1], PitchMultiplier);
	}
}

void UDCAudioSubsystem::SetVolume(EDCVolumeChannel Channel, float Volume01) { Volumes[(int32)Channel] = FMath::Clamp(Volume01, 0.f, 1.f); }
float UDCAudioSubsystem::GetVolume(EDCVolumeChannel Channel) const { return Volumes[(int32)Channel]; }

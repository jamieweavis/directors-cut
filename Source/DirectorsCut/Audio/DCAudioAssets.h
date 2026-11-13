// Director's Cut: audio asset naming (Agent H). Shared by UDCAudioSubsystem and the audio automation test.
#pragma once

#include "CoreMinimal.h"
#include "UObject/SoftObjectPath.h"
#include "Core/DCTypes.h"
#include "Core/DCAudioSubsystem.h"
#include "Core/DCAssetPaths.h"

namespace DCAudio
{
	/** Every canonical SFX name (DCSfx::*). */
	inline const TArray<FName>& AllSfxNames()
	{
		static const TArray<FName> Names = {
			DCSfx::Click, DCSfx::Hover, DCSfx::PanelOpen, DCSfx::PanelClose, DCSfx::Typewriter, DCSfx::Stamp,
			DCSfx::Cash, DCSfx::CounterTick, DCSfx::Clapper, DCSfx::DiceRoll, DCSfx::Commit, DCSfx::Error,
			DCSfx::GarageDoor, DCSfx::CarDepart, DCSfx::StingWrap, DCSfx::StingCut, DCSfx::StingLevelUp,
			DCSfx::StingRoom, DCSfx::StingPerk };
		return Names;
	}

	inline FSoftObjectPath MakePath(const FString& AssetName)
	{
		return FSoftObjectPath(FString::Printf(TEXT("%s/%s.%s"), DCPaths::AudioRoot, *AssetName, *AssetName));
	}

	/** Number of recorded variants (SFX_<Name>, SFX_<Name>_2, ...). */
	inline int32 SfxVariantCount(FName Name)
	{
		return (Name == DCSfx::Click || Name == DCSfx::Typewriter) ? 3 : 1;
	}

	/** Variant 0 = SFX_<Name>; variant N>0 = SFX_<Name>_<N+1>. */
	inline FSoftObjectPath SfxPath(FName Name, int32 Variant = 0)
	{
		return Variant <= 0 ? MakePath(TEXT("SFX_") + Name.ToString())
		                    : MakePath(FString::Printf(TEXT("SFX_%s_%d"), *Name.ToString(), Variant + 1));
	}

	inline bool IsStinger(FName Name)
	{
		return Name.ToString().StartsWith(TEXT("Sting"));
	}

	inline FSoftObjectPath MusicPath(EDCMusicState State)
	{
		switch (State)
		{
		case EDCMusicState::Title:    return MakePath(TEXT("MUS_Title"));
		case EDCMusicState::Planning: return MakePath(TEXT("MUS_Planning"));
		case EDCMusicState::Dailies:  return MakePath(TEXT("MUS_Dailies"));
		case EDCMusicState::Win:      return MakePath(TEXT("MUS_Win"));
		case EDCMusicState::GameOver: return MakePath(TEXT("MUS_GameOver"));
		default:                      return FSoftObjectPath();
		}
	}

	/** Looping beds (rain, projector, neon) and the one-shot siren. */
	inline const TArray<FString>& AmbienceBedNames()
	{
		static const TArray<FString> Names = { TEXT("AMB_Rain"), TEXT("AMB_Projector"), TEXT("AMB_Neon") };
		return Names;
	}
	inline FSoftObjectPath SirenPath() { return MakePath(TEXT("AMB_Siren")); }
}

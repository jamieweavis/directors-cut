// Director's Cut — game mode. Spawns the procedural hideout before BeginPlay is dispatched.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "DCGameMode.generated.h"

UCLASS()
class DIRECTORSCUT_API ADCGameMode : public AGameModeBase
{
	GENERATED_BODY()
public:
	ADCGameMode();
	virtual void StartPlay() override;
};

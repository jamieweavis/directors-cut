#include "World/DCGameMode.h"
#include "World/DCPlayerController.h"
#include "World/Scene/DCHideoutBuilder.h"
#include "GameFramework/SpectatorPawn.h"
#include "EngineUtils.h"

ADCGameMode::ADCGameMode()
{
	PlayerControllerClass = ADCPlayerController::StaticClass();
	DefaultPawnClass = nullptr; // camera rig is the view target; no pawn needed
	HUDClass = nullptr;
}

void ADCGameMode::StartPlay()
{
	// Build the diorama once (unless a builder was placed in the map).
	ADCHideoutBuilder* Builder = nullptr;
	TActorIterator<ADCHideoutBuilder> It(GetWorld());
	if (It) { Builder = *It; }
	if (!Builder)
	{
		FActorSpawnParameters P;
		P.Name = TEXT("HideoutBuilder");
		Builder = GetWorld()->SpawnActor<ADCHideoutBuilder>(ADCHideoutBuilder::StaticClass(), FTransform::Identity, P);
	}
	if (Builder && !Builder->IsBuilt())
	{
		Builder->BuildHideout();
	}
	Super::StartPlay();
}

#include "World/DCRoomModule.h"
#include "World/DCStation.h"
#include "Core/DCWorldTags.h"
#include "Components/PrimitiveComponent.h"
#include "Components/LightComponent.h"

ADCRoomModule::ADCRoomModule()
{
	PrimaryActorTick.bCanEverTick = false;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
}

void ADCRoomModule::AddToGroup(USceneComponent* Component, FName Group)
{
	if (Component)
	{
		Groups.FindOrAdd(Group).Add(Component);
	}
}

void ADCRoomModule::RegisterStation(ADCStation* Station)
{
	if (Station)
	{
		Stations.AddUnique(Station);
		Station->OwningRoom = RoomType;
	}
}

void ADCRoomModule::SetGroupVisible(FName Group, bool bVisible)
{
	if (TArray<TWeakObjectPtr<USceneComponent>>* Comps = Groups.Find(Group))
	{
		for (const TWeakObjectPtr<USceneComponent>& C : *Comps)
		{
			if (USceneComponent* SC = C.Get())
			{
				SC->SetVisibility(bVisible, true);
				if (UPrimitiveComponent* P = Cast<UPrimitiveComponent>(SC))
				{
					P->SetCollisionEnabled(bVisible ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
				}
			}
		}
	}
}

void ADCRoomModule::ApplyState(EDCRoomState State, int32 Level)
{
	CurrentState = State;
	CurrentLevel = Level;
	const bool bBuilt = State == EDCRoomState::Built;
	SetGroupVisible(DCGroups::Shell, true);
	SetGroupVisible(DCGroups::Locked, State == EDCRoomState::Locked);
	SetGroupVisible(DCGroups::Building, State == EDCRoomState::Building);
	SetGroupVisible(DCGroups::Level1, bBuilt && Level >= 1);
	SetGroupVisible(DCGroups::Level2, bBuilt && Level >= 2);
	SetGroupVisible(DCGroups::Level3, bBuilt && Level >= 3);
	for (ADCStation* S : Stations)
	{
		if (S) { S->SetStationEnabled(bBuilt); }
	}
}

void ADCRoomModule::PlayReveal(int32 NewLevel)
{
	// Wave 0 stub: instant. Agent E implements the choreography.
	ApplyState(EDCRoomState::Built, NewLevel);
}

FBox ADCRoomModule::GetWorldBounds() const
{
	return LocalBounds.TransformBy(GetActorTransform());
}

FVector ADCRoomModule::GetFocusPoint() const
{
	return GetWorldBounds().GetCenter();
}

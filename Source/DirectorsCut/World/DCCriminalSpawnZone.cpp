#include "World/DCCriminalSpawnZone.h"
#include "Components/BoxComponent.h"

ADCCriminalSpawnZone::ADCCriminalSpawnZone()
{
	Zone = CreateDefaultSubobject<UBoxComponent>(TEXT("Zone"));
	Zone->SetBoxExtent(FVector(300, 200, 50));
	Zone->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Zone->SetHiddenInGame(true);
	SetRootComponent(Zone);
}

FVector ADCCriminalSpawnZone::GetRandomPoint(FRandomStream& Rng) const
{
	const FVector E = Zone->GetScaledBoxExtent();
	const FVector Local(Rng.FRandRange(-E.X, E.X), Rng.FRandRange(-E.Y, E.Y), -E.Z);
	return GetActorTransform().TransformPosition(Local);
}

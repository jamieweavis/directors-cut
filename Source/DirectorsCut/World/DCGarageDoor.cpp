#include "World/DCGarageDoor.h"

ADCGarageDoor::ADCGarageDoor()
{
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	DoorPanel = CreateDefaultSubobject<USceneComponent>(TEXT("DoorPanel"));
	DoorPanel->SetupAttachment(Root);
}

void ADCGarageDoor::Open() {}
void ADCGarageDoor::Close() {}

#include "World/DCRoomModule.h"
#include "World/DCStation.h"
#include "World/DCCameraRig.h"
#include "World/DCPlayerController.h"
#include "Core/DCWorldTags.h"
#include "Core/DCAssetPaths.h"
#include "Core/DCSettings.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/LightComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Components/WidgetComponent.h"
#include "World/DCWorldLabel.h"

namespace
{
	// Reveal timeline (seconds from start).
	constexpr float DustAt = 0.55f;
	constexpr float SwapAt = 0.85f;
	constexpr float LightStagger = 0.22f;
	constexpr float LightRamp = 0.6f;
	constexpr float DustLife = 1.3f;
}

ADCRoomModule::ADCRoomModule()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
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

bool ADCRoomModule::GroupHasContent(FName Group) const
{
	if (const TArray<TWeakObjectPtr<USceneComponent>>* Comps = Groups.Find(Group))
	{
		for (const TWeakObjectPtr<USceneComponent>& C : *Comps) { if (C.IsValid()) { return true; } }
	}
	return false;
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
					// Hidden prims never block clicks; visible ones get back whatever collision D gave them.
					const TWeakObjectPtr<UPrimitiveComponent> Key(P);
					if (!OriginalCollision.Contains(Key)) { OriginalCollision.Add(Key, P->GetCollisionEnabled()); }
					P->SetCollisionEnabled(bVisible ? OriginalCollision[Key].GetValue() : ECollisionEnabled::NoCollision);
				}
			}
		}
	}
}

float ADCRoomModule::GetLightBase(ULightComponent* L)
{
	if (!L) { return 0.f; }
	if (const float* F = LightBaseIntensity.Find(L)) { return *F; }
	const float I = L->Intensity;
	LightBaseIntensity.Add(L, I);
	return I;
}

void ADCRoomModule::ApplyState(EDCRoomState State, int32 Level)
{
	// An instant apply cancels any running reveal.
	if (bRevealing)
	{
		bRevealing = false;
		SetActorTickEnabled(false);
		ClearTempScaffold();
		if (DustPuff) { DustPuff->DestroyComponent(); DustPuff = nullptr; }
	}
	for (FRevealLight& RL : RevealLights)
	{
		if (ULightComponent* L = RL.Light.Get()) { L->SetIntensity(GetLightBase(L)); }
	}
	RevealLights.Reset();

	CurrentState = State;
	CurrentLevel = Level;
	// Upgrading keeps the current level visible (and usable) under the scaffolding.
	const bool bHasLevel = State == EDCRoomState::Built || (State == EDCRoomState::Building && Level >= 1);
	SetGroupVisible(DCGroups::Shell, true);
	SetGroupVisible(DCGroups::Locked, State == EDCRoomState::Locked);
	SetGroupVisible(DCGroups::Building, State == EDCRoomState::Building);
	SetGroupVisible(DCGroups::Level1, bHasLevel && Level >= 1);
	SetGroupVisible(DCGroups::Level2, bHasLevel && Level >= 2);
	SetGroupVisible(DCGroups::Level3, bHasLevel && Level >= 3);
	if (State == EDCRoomState::Building)
	{
		if (!GroupHasContent(DCGroups::Building) && TempScaffold.Num() == 0) { SpawnFallbackScaffold(); }
	}
	else
	{
		ClearTempScaffold();
		ClearBuildProgress();
	}
	for (ADCStation* S : Stations)
	{
		if (S) { S->SetStationEnabled(bHasLevel); }
	}
}

static ADCCameraRig* FindRig(const UWorld* W)
{
	const ADCPlayerController* PC = W ? Cast<ADCPlayerController>(W->GetFirstPlayerController()) : nullptr;
	return PC ? PC->GetCameraRig() : nullptr;
}

void ADCRoomModule::PlayReveal(int32 NewLevel)
{
	if (bRevealing) { FinishReveal(); }
	if (!GetWorld() || !GetWorld()->HasBegunPlay())
	{
		ApplyState(EDCRoomState::Built, NewLevel);
		return;
	}
	RevealFromState = CurrentState == EDCRoomState::Building && CurrentLevel < 1 ? EDCRoomState::Locked : CurrentState;
	if (CurrentState == EDCRoomState::Building && CurrentLevel >= 1) { RevealFromState = EDCRoomState::Built; }
	RevealLevel = FMath::Clamp(NewLevel, 1, 3);
	RevealTime = 0.f;
	bRevealing = true;
	bQuickReveal = false;
	ActiveRevealDuration = RevealDuration;
	ClearBuildProgress();
	bRevealSwapped = false;
	bRevealCameraReturned = false;

	// Phase 1: scaffolding & work lights.
	SetGroupVisible(DCGroups::Locked, false);
	if (GroupHasContent(DCGroups::Building)) { SetGroupVisible(DCGroups::Building, true); }
	else if (TempScaffold.Num() == 0) { SpawnFallbackScaffold(); }
	CurrentState = EDCRoomState::Building;

	if (bRevealMovesCamera && !DCSettings::ReduceMotion())
	{
		if (ADCCameraRig* Rig = FindRig(GetWorld()))
		{
			Rig->BeginCinematic(GetFocusPoint(), 0.35f, RevealDuration + 1.f);
		}
	}
	SetActorTickEnabled(true);
}

void ADCRoomModule::SpawnFallbackScaffold()
{
	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, DCPaths::Cube);
	if (!Cube) { return; }
	UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, DCPaths::MatSurface);
	const bool bSurface = Base != nullptr;
	if (!Base) { Base = LoadObject<UMaterialInterface>(nullptr, DCPaths::BasicShapeMaterial); }
	UMaterialInstanceDynamic* M = Base ? UMaterialInstanceDynamic::Create(Base, this) : nullptr;
	if (M)
	{
		M->SetVectorParameterValue(bSurface ? TEXT("BaseColor") : TEXT("Color"), FLinearColor(0.55f, 0.38f, 0.05f));
		if (bSurface) { M->SetScalarParameterValue(TEXT("Metallic"), 0.7f); M->SetScalarParameterValue(TEXT("Roughness"), 0.45f); }
	}
	const FBox B = LocalBounds;
	const float H = FMath::Max(200.f, B.Max.Z - B.Min.Z - 20.f);
	auto AddBar = [&](const FVector& Center, const FVector& Size)
	{
		UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
		C->SetStaticMesh(Cube);
		C->SetupAttachment(Root);
		C->SetRelativeLocation(Center);
		C->SetRelativeScale3D(Size / 100.f);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetCastShadow(true);
		if (M) { C->SetMaterial(0, M); }
		C->RegisterComponent();
		TempScaffold.Add(C);
	};
	const float In = 70.f;
	const FVector Mn = B.Min + FVector(In, In, 0.f), Mx = B.Max - FVector(In, In, 0.f);
	for (int32 i = 0; i < 4; ++i)
	{
		const FVector P((i & 1) ? Mx.X : Mn.X, (i & 2) ? Mx.Y : Mn.Y, B.Min.Z + H * 0.5f);
		AddBar(P, FVector(10.f, 10.f, H));
	}
	for (float Z : { H * 0.45f, H * 0.9f })
	{
		AddBar(FVector((Mn.X + Mx.X) * 0.5f, Mx.Y, B.Min.Z + Z), FVector(Mx.X - Mn.X, 8.f, 8.f));
		AddBar(FVector(Mx.X, (Mn.Y + Mx.Y) * 0.5f, B.Min.Z + Z), FVector(8.f, Mx.Y - Mn.Y, 8.f));
		AddBar(FVector((Mn.X + Mx.X) * 0.5f, Mn.Y, B.Min.Z + Z), FVector(Mx.X - Mn.X, 8.f, 8.f));
		AddBar(FVector(Mn.X, (Mn.Y + Mx.Y) * 0.5f, B.Min.Z + Z), FVector(8.f, Mx.Y - Mn.Y, 8.f));
	}
	// Harsh work light.
	UPointLightComponent* WL = NewObject<UPointLightComponent>(this);
	WL->SetupAttachment(Root);
	WL->SetRelativeLocation(FVector(B.GetCenter().X, B.GetCenter().Y, B.Min.Z + H * 0.8f));
	WL->SetIntensity(9000.f);
	WL->SetAttenuationRadius(FMath::Max(B.GetSize().X, B.GetSize().Y));
	WL->SetLightColor(FLinearColor(0.75f, 0.85f, 1.f));
	WL->SetCastShadows(false);
	WL->RegisterComponent();
	TempScaffold.Add(WL);
}

void ADCRoomModule::ClearTempScaffold()
{
	for (USceneComponent* C : TempScaffold) { if (C) { C->DestroyComponent(); } }
	TempScaffold.Reset();
}

void ADCRoomModule::SpawnDustPuff()
{
	UStaticMesh* Sphere = LoadObject<UStaticMesh>(nullptr, DCPaths::Sphere);
	if (!Sphere) { return; }
	UMaterialInterface* Glass = LoadObject<UMaterialInterface>(nullptr, DCPaths::MatGlass);
	if (!Glass) { return; } // an opaque puff would look wrong; skip the effect
	DustMID = UMaterialInstanceDynamic::Create(Glass, this);
	DustMID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.55f, 0.5f, 0.44f));
	DustParam = TEXT("Opacity");
	DustBaseValue = 0.32f;
	DustMID->SetScalarParameterValue(DustParam, DustBaseValue);
	DustPuff = NewObject<UStaticMeshComponent>(this);
	DustPuff->SetStaticMesh(Sphere);
	DustPuff->SetupAttachment(Root);
	const FVector C = LocalBounds.GetCenter();
	DustPuff->SetRelativeLocation(FVector(C.X, C.Y, LocalBounds.Min.Z + 20.f));
	DustPuff->SetRelativeScale3D(FVector(1.f, 1.f, 0.35f));
	DustPuff->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	DustPuff->SetCastShadow(false);
	DustPuff->SetMaterial(0, DustMID);
	DustPuff->RegisterComponent();
}

void ADCRoomModule::RevealSwapToBuilt()
{
	bRevealSwapped = true;
	ClearTempScaffold();
	SetGroupVisible(DCGroups::Building, false);

	// Lights already on before this reveal (upgrade) keep shining; new ones flicker on.
	TSet<ULightComponent*> AlreadyOn;
	if (RevealFromState == EDCRoomState::Built)
	{
		TArray<ULightComponent*> Ls; GetComponents<ULightComponent>(Ls);
		for (ULightComponent* L : Ls) { if (L && L->IsVisible()) { AlreadyOn.Add(L); } }
	}

	const bool bWasRevealing = bRevealing;
	bRevealing = false; // so ApplyState doesn't cancel us
	ApplyState(EDCRoomState::Built, RevealLevel);
	bRevealing = bWasRevealing;
	StartGlow(RevealLevel >= 3 ? DCGroups::Level3 : RevealLevel == 2 ? DCGroups::Level2 : DCGroups::Level1);

	TArray<ULightComponent*> Lights; GetComponents<ULightComponent>(Lights);
	int32 Index = 0;
	for (ULightComponent* L : Lights)
	{
		if (!L || !L->ComponentHasTag(DCTags::RoomLight) || !L->IsVisible() || AlreadyOn.Contains(L)) { continue; }
		FRevealLight RL;
		RL.Light = L;
		RL.Target = GetLightBase(L);
		RL.Start = RevealTime + 0.15f + Index * LightStagger;
		RL.Seed = Index * 7.31f + 1.7f;
		L->SetIntensity(0.f);
		RevealLights.Add(RL);
		++Index;
	}
}

void ADCRoomModule::FinishReveal()
{
	if (!bRevealCameraReturned && bRevealMovesCamera && !bQuickReveal)
	{
		if (ADCCameraRig* Rig = FindRig(GetWorld())) { Rig->EndCinematic(); }
		bRevealCameraReturned = true;
	}
	if (DustPuff) { DustPuff->DestroyComponent(); DustPuff = nullptr; }
	bRevealing = false;
	ApplyState(EDCRoomState::Built, RevealLevel); // restores light intensities
	SetActorTickEnabled(GlowTime >= 0.f);
}

void ADCRoomModule::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	TickGlow(FMath::Min(DeltaSeconds, 0.1f));
	if (!bRevealing) { if (GlowTime < 0.f) { SetActorTickEnabled(false); } return; }
	const float Prev = RevealTime;
	RevealTime += FMath::Min(DeltaSeconds, 0.1f);

	const float Swap = bQuickReveal ? 0.15f : SwapAt;
	if (!bQuickReveal && Prev < DustAt && RevealTime >= DustAt) { SpawnDustPuff(); }
	if (!bRevealSwapped && RevealTime >= Swap) { RevealSwapToBuilt(); }

	// Dust puff: expands and fades.
	if (DustPuff)
	{
		const float A = FMath::Clamp((RevealTime - DustAt) / DustLife, 0.f, 1.f);
		const float E = 1.f - FMath::Pow(1.f - A, 3.f);
		const FVector Sz = LocalBounds.GetSize();
		const float R = FMath::Lerp(0.6f, FMath::Max(Sz.X, Sz.Y) * 0.85f / 100.f, E);
		DustPuff->SetRelativeScale3D(FVector(R, R, R * 0.28f));
		if (DustMID) { DustMID->SetScalarParameterValue(DustParam, DustBaseValue * (1.f - A) * (1.f - A)); }
		if (A >= 1.f) { DustPuff->DestroyComponent(); DustPuff = nullptr; }
	}

	// Lights: staggered flicker-on.
	for (FRevealLight& RL : RevealLights)
	{
		ULightComponent* L = RL.Light.Get();
		if (!L) { continue; }
		const float T = (RevealTime - RL.Start) / LightRamp;
		float I = 0.f;
		if (T <= 0.f) { I = 0.f; }
		else if (T < 0.65f && DCSettings::ReduceFlashing()) { I = RL.Target * FMath::Lerp(0.f, 0.8f, T / 0.65f); } // smooth fade, no flicker
		else if (T < 0.65f)
		{
			const float N = FMath::PerlinNoise1D(RevealTime * 22.f + RL.Seed);
			I = RL.Target * (N > -0.05f ? FMath::Lerp(0.35f, 0.9f, T) : 0.03f);
		}
		else { I = RL.Target * FMath::Clamp(FMath::Lerp(0.8f, 1.f, (T - 0.65f) / 0.35f), 0.f, 1.f); }
		L->SetIntensity(I);
	}

	if (!bRevealCameraReturned && RevealTime >= ActiveRevealDuration - 0.7f)
	{
		bRevealCameraReturned = true;
		if (bRevealMovesCamera && !bQuickReveal) { if (ADCCameraRig* Rig = FindRig(GetWorld())) { Rig->EndCinematic(); } }
	}
	if (RevealTime >= ActiveRevealDuration) { FinishReveal(); }
}

void ADCRoomModule::PlayUpgradeReveal(int32 NewLevel)
{
	if (!GetWorld() || !GetWorld()->HasBegunPlay() || CurrentLevel < 1) { PlayReveal(NewLevel); return; }
	if (bRevealing) { FinishReveal(); }
	RevealFromState = EDCRoomState::Built;
	RevealLevel = FMath::Clamp(NewLevel, 1, 3);
	RevealTime = 0.f;
	bRevealing = true;
	bQuickReveal = true;
	ActiveRevealDuration = 1.6f;
	bRevealSwapped = false;
	bRevealCameraReturned = true; // no camera move
	ClearBuildProgress();
	SetActorTickEnabled(true);
}

void ADCRoomModule::StartGlow(FName Group)
{
	EndGlow();
	if (!GlowMID)
	{
		if (UMaterialInterface* H = LoadObject<UMaterialInterface>(nullptr, DCPaths::MatHighlight, nullptr, LOAD_NoWarn | LOAD_Quiet))
		{
			GlowMID = UMaterialInstanceDynamic::Create(H, this);
			GlowMID->SetVectorParameterValue(TEXT("Color"), FLinearColor(1.f, 0.78f, 0.42f));
		}
	}
	if (!GlowMID) { return; }
	if (const TArray<TWeakObjectPtr<USceneComponent>>* Comps = Groups.Find(Group))
	{
		for (const TWeakObjectPtr<USceneComponent>& C : *Comps)
		{
			UMeshComponent* MC = Cast<UMeshComponent>(C.Get());
			if (!MC || MC->GetOverlayMaterial()) { continue; }
			MC->SetOverlayMaterial(GlowMID);
			Glows.Add({ MC });
		}
	}
	GlowTime = Glows.Num() ? 0.f : -1.f;
	GlowMID->SetScalarParameterValue(TEXT("Strength"), 0.f);
	if (GlowTime >= 0.f) { SetActorTickEnabled(true); }
}

void ADCRoomModule::TickGlow(float Dt)
{
	if (GlowTime < 0.f) { return; }
	GlowTime += Dt;
	constexpr float Len = 1.6f;
	const float A = FMath::Clamp(GlowTime / Len, 0.f, 1.f);
	const float Env = A < 0.2f ? A / 0.2f : FMath::Square(1.f - (A - 0.2f) / 0.8f);
	if (GlowMID) { GlowMID->SetScalarParameterValue(TEXT("Strength"), 1.6f * Env); }
	if (A >= 1.f) { EndGlow(); }
}

void ADCRoomModule::EndGlow()
{
	for (const FGlow& G : Glows) { if (UMeshComponent* MC = G.Mesh.Get()) { if (MC->GetOverlayMaterial() == GlowMID) { MC->SetOverlayMaterial(nullptr); } } }
	Glows.Reset();
	GlowTime = -1.f;
}

void ADCRoomModule::SetBuildProgress(float Alpha, double SecondsLeft, int32 TargetLevel)
{
	if (!BuildLabel)
	{
		const FBox B = LocalBounds;
		BuildLabel = UDCWorldLabel::CreateLabelComponent(this, Root, FVector(B.GetCenter().X, B.GetCenter().Y, B.Min.Z + 260.f));
	}
	if (!BuildLabel) { return; }
	FDCLabelSpec S;
	S.Icon = TEXT("construction");
	S.Text = TargetLevel > 1 ? FString::Printf(TEXT("%s  Lv %d"), DC::RoomName(RoomType), TargetLevel) : FString(DC::RoomName(RoomType));
	S.Sub = DC::Clock(SecondsLeft);
	S.Accent = FLinearColor(1.f, 0.66f, 0.18f);
	S.Progress = FMath::Clamp(Alpha, 0.f, 1.f);
	S.Size = 12.f;
	UDCWorldLabel::SetComponentSpec(BuildLabel, S);
	BuildLabel->SetVisibility(true);
}

void ADCRoomModule::ClearBuildProgress()
{
	if (BuildLabel) { BuildLabel->SetVisibility(false); }
}

void ADCRoomModule::EndPlay(const EEndPlayReason::Type Reason)
{
	ClearTempScaffold();
	Super::EndPlay(Reason);
}

FBox ADCRoomModule::GetWorldBounds() const
{
	return LocalBounds.TransformBy(GetActorTransform());
}

FVector ADCRoomModule::GetFocusPoint() const
{
	return GetWorldBounds().GetCenter();
}

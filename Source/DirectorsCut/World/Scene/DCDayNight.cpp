// Director's Cut — day/night lighting rig (Agent D).
#include "World/Scene/DCDayNight.h"
#include "Core/DCRunSubsystem.h"
#include "DirectorsCut.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/LocalFogVolumeComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/TextureCube.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace
{
	constexpr float DawnH = 6.f;
	constexpr float DuskH = 20.f;

	/** Piecewise-linear colour/scalar key track over the 24 h clock (wraps). */
	template <typename T>
	T Track(const TArray<TPair<float, T>>& Keys, float H)
	{
		const int32 N = Keys.Num();
		for (int32 i = 0; i < N; ++i)
		{
			const TPair<float, T>& A = Keys[i];
			const TPair<float, T>& B = Keys[(i + 1) % N];
			const float BH = (i + 1 < N) ? B.Key : B.Key + 24.f;
			float HH = H;
			if (HH < A.Key) { HH += 24.f; }
			if (HH >= A.Key && HH <= BH)
			{
				const float A01 = (BH - A.Key) > KINDA_SMALL_NUMBER ? (HH - A.Key) / (BH - A.Key) : 0.f;
				const float S = A01 * A01 * (3.f - 2.f * A01);
				return A.Value + (B.Value - A.Value) * S;
			}
		}
		return Keys[0].Value;
	}

	float SunAlphaAt(float H) { return FMath::Clamp(FMath::Sin((H - DawnH) / (DuskH - DawnH) * PI), 0.f, 1.f); }
}

ADCDayNight::ADCDayNight()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));

	Sun = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("Sun"));
	Sun->SetupAttachment(GetRootComponent());
	Sun->SetMobility(EComponentMobility::Movable);
	Sun->Intensity = 8.f;
	Sun->bUseTemperature = true;
	Sun->Temperature = 5200.f;
	Sun->SetCastShadows(true);
	Sun->LightSourceAngle = 0.6f;          // fairly crisp window-pane edges
	Sun->SetVolumetricScatteringIntensity(3.f);
	Sun->bAtmosphereSunLight = false;
	Sun->DynamicShadowDistanceMovableLight = 40000.f;
	Sun->ShadowBias = 0.4f;

	Sky = CreateDefaultSubobject<USkyLightComponent>(TEXT("Sky"));
	Sky->SetupAttachment(GetRootComponent());
	Sky->SetMobility(EComponentMobility::Movable);
	Sky->SourceType = ESkyLightSourceType::SLS_SpecifiedCubemap;
	Sky->bLowerHemisphereIsBlack = true;
	Sky->Intensity = 0.2f;
	Sky->SetCastShadows(true);
}

void ADCDayNight::BeginPlay()
{
	Super::BeginPlay();
	if (UTextureCube* Cube = LoadObject<UTextureCube>(nullptr, TEXT("/Engine/MapTemplates/Sky/DaylightAmbientCubemap.DaylightAmbientCubemap")))
	{
		Sky->SetCubemap(Cube);
		UE_LOG(LogDC, Display, TEXT("[DayNight] sky cubemap %s"), *Cube->GetName());
	}
	const TCHAR* Cmd = FCommandLine::Get();
	FParse::Value(Cmd, TEXT("DCFixedHour="), FixedHour);
	FParse::Value(Cmd, TEXT("DCSunLux="), SunLux);
	FParse::Value(Cmd, TEXT("DCMoonLux="), MoonLux);
	FParse::Value(Cmd, TEXT("DCEVDay="), EVDay);
	FParse::Value(Cmd, TEXT("DCEVNight="), EVNight);
	FParse::Value(Cmd, TEXT("DCSkyDay="), SkyDay);
	FParse::Value(Cmd, TEXT("DCBeamK="), BeamK);
	FParse::Value(Cmd, TEXT("DCSkyNight="), SkyNight);
	Sky->RecaptureSky();
}

void ADCDayNight::AddShaft(const FVector& Centre, const FVector& Along, float W, float H)
{
	static UStaticMesh* Cyl = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (!BeamMat)
	{
		if (UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/DC/Materials/M_DC_Beam.M_DC_Beam")))
		{
			BeamMat = UMaterialInstanceDynamic::Create(Base, this);
			BeamMat->SetScalarParameterValue("Flicker", 0.f);
		}
	}
	if (!Cyl || !BeamMat) { return; }
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
	C->SetStaticMesh(Cyl);
	C->SetMaterial(0, BeamMat);
	C->SetMobility(EComponentMobility::Movable);
	C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	C->SetCastShadow(false);
	C->bAffectDistanceFieldLighting = false;
	C->bVisibleInRayTracing = false;
	C->SetupAttachment(GetRootComponent());
	C->RegisterComponent();
	AddInstanceComponent(C);
	Beams.Add({ C, Centre, Along, W, H });
}

void ADCDayNight::AddShaftFog(ULocalFogVolumeComponent* Fog)
{
	if (Fog) { Shafts.Add({ Fog, Fog->RadialFogExtinction }); }
}

void ADCDayNight::AddStreetLight(const FVector& WorldPos, const FVector& WorldTarget, const FLinearColor& Color, float Candelas, float Radius, float ConeOuter, bool bShadows)
{
	USpotLightComponent* L = NewObject<USpotLightComponent>(this);
	L->SetMobility(EComponentMobility::Movable);
	L->SetupAttachment(GetRootComponent());
	L->SetWorldLocationAndRotation(WorldPos, (WorldTarget - WorldPos).Rotation());
	L->IntensityUnits = ELightUnits::Candelas;
	L->Intensity = 0.f;
	L->SetLightColor(Color);
	L->AttenuationRadius = Radius;
	L->InnerConeAngle = ConeOuter * 0.4f;
	L->OuterConeAngle = ConeOuter;
	L->SourceRadius = 6.f;
	L->SetCastShadows(bShadows);
	L->SetVolumetricScatteringIntensity(2.f);
	L->RegisterComponent();
	AddInstanceComponent(L);
	Streets.Add({ L, Candelas, (float)Streets.Num() * 1.37f });
}

void ADCDayNight::SetFixedHour(float Hour, bool bSnap)
{
	FixedHour = Hour;
	if (bSnap && Hour >= 0.f)
	{
		DisplayHour = FMath::Fmod(Hour + 24.f, 24.f);
		bHaveHour = true;
		bSnapLamps = true;
	}
}

void ADCDayNight::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Time += DeltaSeconds;

	float Target = 17.5f; // title screen / no run: hold golden hour
	if (FixedHour >= 0.f) { Target = FixedHour; }
	else if (UDCRunSubsystem* Run = UDCRunSubsystem::Get(this))
	{
		if (Run->HasActiveRun()) { Target = Run->GetHourOfDay(); }
	}
	if (!bHaveHour) { DisplayHour = Target; bHaveHour = true; bSnapLamps = true; }
	else
	{
		// Smooth toward the clock along the shortest way round (clock jumps / continue never pop).
		float D = FMath::Fmod(Target - DisplayHour + 36.f, 24.f) - 12.f;
		const float Step = (FMath::Abs(D) > 0.25f) ? D * FMath::Min(1.f, DeltaSeconds * 2.5f) : D;
		DisplayHour = FMath::Fmod(DisplayHour + Step + 24.f, 24.f);
	}
	Apply(DisplayHour, DeltaSeconds);
}

void ADCDayNight::Apply(float H, float DeltaSeconds)
{
	const float SunA = SunAlphaAt(H);
	SunAlphaNow = SunA;
	const bool bDay = H >= DawnH && H <= DuskH;
	auto Bell = [](float X, float C, float W) { const float D = (X - C) / W; return FMath::Exp(-D * D); };
	// twilight: the hour either side of sunrise / sunset
	const float Twilight = FMath::Max(Bell(H, DuskH + 0.3f, 0.9f), Bell(H, DawnH - 0.2f, 0.8f));

	// ---------------------------------------------------------------- sun / moon direction
	// World yaw of the light *source*: 0 = +X, 90 = +Y. The camera sits at -X-Y looking +X+Y, so the sun rises on
	// the upper-left of the screen (yaw -40), passes high behind the house (yaw 45) and sets on the right (yaw 135):
	// long side-light at golden hour, short shadows falling towards the camera at noon.
	float Yaw, El;
	if (bDay)
	{
		const float Frac = (H - DawnH) / (DuskH - DawnH);
		Yaw = FMath::Lerp(-40.f, 135.f, Frac);
		El = FMath::Max(2.f, 60.f * FMath::Sin(Frac * PI));
	}
	else
	{
		const float Frac = FMath::Fmod(H - DuskH + 24.f, 24.f) / (24.f - (DuskH - DawnH));
		Yaw = FMath::Lerp(-55.f, 150.f, Frac);
		El = 14.f + 36.f * FMath::Sin(Frac * PI);
	}
	Sun->SetWorldRotation(FRotator(-El, Yaw + 180.f, 0.f));

	// Intensity: sun fades out into the dusk dip, moon fades in after it, so the direction swap is invisible.
	const float MoonA = bDay ? 0.f : FMath::Clamp(FMath::Min(FMath::Fmod(H - DuskH + 24.f, 24.f), FMath::Fmod(DawnH - H + 24.f, 24.f)) / 1.0f, 0.f, 1.f);
	const float SunI = SunLux * FMath::Pow(SunA, 0.7f);
	const float MoonI = MoonLux * MoonA;
	Sun->SetIntensity(bDay ? SunI : MoonI);
	// Colour temperature: deep amber low sun, neutral noon; cool blue moon.
	Sun->SetTemperature(bDay ? FMath::Lerp(2600.f, 5900.f, FMath::Pow(SunA, 1.1f)) : 9500.f);
	Sun->SetLightColor(bDay ? FLinearColor::White : FLinearColor(0.5f, 0.66f, 1.f));

	// ---------------------------------------------------------------- sky light (ambient fill), window glow
	static const TArray<TPair<float, FLinearColor>> SkyKeys = {
		{ 0.0f,  FLinearColor(0.12f, 0.18f, 0.40f) },
		{ 4.6f,  FLinearColor(0.13f, 0.18f, 0.40f) },
		{ 5.6f,  FLinearColor(0.42f, 0.36f, 0.62f) },
		{ 6.5f,  FLinearColor(0.95f, 0.60f, 0.48f) },
		{ 8.0f,  FLinearColor(0.92f, 0.85f, 0.78f) },
		{ 11.f,  FLinearColor(0.78f, 0.86f, 1.00f) },
		{ 15.f,  FLinearColor(0.80f, 0.86f, 0.98f) },
		{ 17.6f, FLinearColor(1.00f, 0.80f, 0.60f) },
		{ 19.4f, FLinearColor(1.00f, 0.55f, 0.42f) },
		{ 20.4f, FLinearColor(0.40f, 0.36f, 0.75f) },
		{ 21.6f, FLinearColor(0.14f, 0.20f, 0.46f) },
	};
	const FLinearColor SkyCol = Track(SkyKeys, H);
	Sky->SetLightColor(SkyCol);
	Sky->SetIntensity(FMath::Lerp(SkyNight, SkyDay, FMath::Pow(SunA, 0.5f)) + SkyNight * 1.6f * Twilight);
	const float WinStrength = FMath::Lerp(0.12f, 1.1f, FMath::Pow(SunA, 0.45f));
	for (UMaterialInstanceDynamic* W : WindowMats)
	{
		if (!W) { continue; }
		W->SetVectorParameterValue("SkyColor", SkyCol);
		W->SetScalarParameterValue("Strength", WinStrength);
	}

	// ---------------------------------------------------------------- shafts, dust
	for (const FShaft& S : Shafts)
	{
		if (ULocalFogVolumeComponent* F = S.Fog.Get())
		{
			const float K = 0.25f + 0.75f * SunA;
			F->SetRadialFogExtinction(S.Base * K);
		}
	}
	// fake god rays: soft additive volumes from each opening along the light direction (cheap: ~30 meshes)
	{
		const FVector Dir = Sun->GetForwardVector();
		const float SinEl = FMath::Max(-Dir.Z, 0.12f);
		const float Beam = BeamK * (bDay ? 0.75f * FMath::Pow(SunA, 0.6f) : 0.06f * MoonA);
		const FLinearColor BeamCol = bDay ? FMath::Lerp(FLinearColor(1.f, 0.55f, 0.25f), FLinearColor(1.f, 0.93f, 0.82f), FMath::Pow(SunA, 1.2f)) : FLinearColor(0.5f, 0.65f, 1.f);
		if (BeamMat)
		{
			BeamMat->SetScalarParameterValue("Strength", Beam);
			BeamMat->SetVectorParameterValue("Color", BeamCol);
		}
		for (const FBeam& B : Beams)
		{
			UStaticMeshComponent* C = B.Mesh.Get();
			if (!C) { continue; }
			C->SetVisibility(Beam > 0.002f);
			if (Beam <= 0.002f) { continue; }
			const float Len = FMath::Min(B.Centre.Z / SinEl, 750.f);
			const FVector Mid = B.Centre + Dir * (Len * 0.5f);
			C->SetWorldLocationAndRotation(Mid, FRotationMatrix::MakeFromZX(Dir, B.Along).Rotator());
			C->SetWorldScale3D(FVector(B.W * 0.9f, B.H * 0.6f, Len) / 100.f);
		}
	}
	if (DustMat) { DustMat->SetScalarParameterValue("Strength", 1.f + 9.f * SunA); }

	// ---------------------------------------------------------------- exterior / night lights: on at dusk, off after dawn
	const bool bLampsOn = H >= DuskH - 0.9f || H < DawnH + 0.6f;
	LampAlpha = bSnapLamps ? (bLampsOn ? 1.f : 0.f) : FMath::FInterpConstantTo(LampAlpha, bLampsOn ? 1.f : 0.f, DeltaSeconds, 0.6f);
	bSnapLamps = false;
	for (const FStreet& S : Streets)
	{
		if (ULightComponent* L = S.Light.Get())
		{
			// warm-up: a little stutter while coming on
			float K = LampAlpha;
			if (LampAlpha > 0.f && LampAlpha < 1.f) { K *= 0.6f + 0.4f * FMath::Abs(FMath::Sin((float)Time * 23.f + S.Seed * 5.f)); }
			L->SetIntensity(S.Base * K);
			L->SetVisibility(K > 0.001f);
		}
	}
	if (LampMat) { LampMat->SetScalarParameterValue("Strength", 0.15f + 40.f * LampAlpha); }

	// ---------------------------------------------------------------- exposure, grade, bloom
	if (Post)
	{
		const float W = FMath::Pow(SunA, 0.5f);
		Post->Settings.AutoExposureBias = FMath::Lerp(EVNight, EVDay, W) + 0.35f * Twilight;
		Post->Settings.bOverride_WhiteTemp = true;
		Post->Settings.WhiteTemp = FMath::Lerp(6300.f, 6500.f, W);
		Post->Settings.BloomIntensity = FMath::Lerp(0.9f, 0.45f, W);
	}

	// ---------------------------------------------------------------- haze + ground mist
	static const TArray<TPair<float, FLinearColor>> HazeKeys = {
		{ 0.0f,  FLinearColor(0.035f, 0.05f, 0.09f) },
		{ 4.8f,  FLinearColor(0.04f, 0.055f, 0.10f) },
		{ 6.2f,  FLinearColor(0.45f, 0.38f, 0.40f) },
		{ 8.0f,  FLinearColor(0.42f, 0.45f, 0.45f) },
		{ 12.f,  FLinearColor(0.30f, 0.38f, 0.42f) },
		{ 17.5f, FLinearColor(0.42f, 0.36f, 0.26f) },
		{ 19.6f, FLinearColor(0.42f, 0.25f, 0.22f) },
		{ 20.6f, FLinearColor(0.10f, 0.10f, 0.20f) },
		{ 22.0f, FLinearColor(0.035f, 0.05f, 0.09f) },
	};
	HazeColor = Track(HazeKeys, H);
	if (HeightFog)
	{
		static const TArray<TPair<float, float>> MistKeys = {
			{ 0.f, 0.22f }, { 4.5f, 0.3f }, { 6.3f, 0.5f }, { 8.2f, 0.12f }, { 10.f, 0.f }, { 18.8f, 0.f }, { 20.8f, 0.14f }, { 23.f, 0.22f } };
		static const TArray<TPair<float, float>> HazeDensityKeys = {
			{ 0.f, 0.006f }, { 5.5f, 0.012f }, { 7.5f, 0.012f }, { 11.f, 0.006f }, { 17.f, 0.008f }, { 19.5f, 0.012f }, { 21.5f, 0.006f } };
		// inscattering is a luminance: scale the haze albedo by roughly how much light is around
		const float Ambient = 0.06f * SunI + 0.5f * MoonI + 0.35f * Twilight + 0.25f;
		HeightFog->SetFogInscatteringColor(HazeColor * Ambient);
		HeightFog->SetFogDensity(Track(HazeDensityKeys, H));
		HeightFog->SetSecondFogDensity(Track(MistKeys, H));
		const float Gold = bDay ? FMath::Clamp(1.f - SunA * 2.2f, 0.f, 1.f) : 0.f;
		HeightFog->SetDirectionalInscatteringColor(FLinearColor(1.f, 0.55f, 0.25f) * (0.02f * SunI * Gold));
	}
}

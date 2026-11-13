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
	}
	const TCHAR* Cmd = FCommandLine::Get();
	FParse::Value(Cmd, TEXT("DCFixedHour="), FixedHour);
	FParse::Value(Cmd, TEXT("DCSunLux="), SunLux);
	FParse::Value(Cmd, TEXT("DCMoonLux="), MoonLux);
	FParse::Value(Cmd, TEXT("DCEVDay="), EVDay);
	FParse::Value(Cmd, TEXT("DCEVNight="), EVNight);
	FParse::Value(Cmd, TEXT("DCSkyDay="), SkyDay);
	FParse::Value(Cmd, TEXT("DCSkyNight="), SkyNight);
	Sky->RecaptureSky();
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
	if (!bHaveHour) { DisplayHour = Target; bHaveHour = true; }
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
	const bool bDay = H >= DawnH && H <= DuskH;

	// ---------------------------------------------------------------- sun / moon direction
	// World yaw of the light *source*: 0 = +X, 90 = +Y. The sweep stays behind the +X/+Y back walls (camera sits at -X-Y).
	float Frac, PeakEl;
	if (bDay) { Frac = (H - DawnH) / (DuskH - DawnH); PeakEl = 50.f; }
	else { Frac = FMath::Fmod(H - DuskH + 24.f, 24.f) / (24.f - (DuskH - DawnH)); PeakEl = 38.f; }
	const float Yaw = FMath::Lerp(6.f, 84.f, Frac);
	const float El = 6.f + (PeakEl - 6.f) * FMath::Sin(Frac * PI);
	Sun->SetWorldRotation(FRotator(-El, Yaw + 180.f, 0.f));

	// Intensity: sun fades out into the dusk dip, moon fades in after it, so the direction swap is invisible.
	const float MoonA = bDay ? 0.f : FMath::Clamp(FMath::Min(FMath::Fmod(H - DuskH + 24.f, 24.f), FMath::Fmod(DawnH - H + 24.f, 24.f)) / 1.2f, 0.f, 1.f);
	const float SunI = SunLux * FMath::Pow(SunA, 0.7f);
	const float MoonI = MoonLux * MoonA;
	Sun->SetIntensity(bDay ? SunI : MoonI);
	// Colour temperature: warm low sun, neutral noon; cool moon.
	Sun->SetTemperature(bDay ? FMath::Lerp(2300.f, 5600.f, FMath::Pow(SunA, 0.6f)) : 9000.f);
	Sun->SetLightColor(bDay ? FLinearColor::White : FLinearColor(0.55f, 0.7f, 1.f));

	// ---------------------------------------------------------------- sky light (ambient fill), window glow
	static const TArray<TPair<float, FLinearColor>> SkyKeys = {
		{ 0.0f,  FLinearColor(0.10f, 0.16f, 0.36f) },
		{ 4.8f,  FLinearColor(0.12f, 0.16f, 0.34f) },
		{ 6.0f,  FLinearColor(0.75f, 0.45f, 0.38f) },
		{ 7.5f,  FLinearColor(0.95f, 0.78f, 0.62f) },
		{ 10.f,  FLinearColor(0.80f, 0.86f, 0.95f) },
		{ 15.f,  FLinearColor(0.82f, 0.86f, 0.92f) },
		{ 17.8f, FLinearColor(1.00f, 0.72f, 0.45f) },
		{ 19.6f, FLinearColor(0.85f, 0.40f, 0.35f) },
		{ 20.6f, FLinearColor(0.25f, 0.24f, 0.48f) },
		{ 21.8f, FLinearColor(0.10f, 0.16f, 0.36f) },
	};
	const FLinearColor SkyCol = Track(SkyKeys, H);
	Sky->SetLightColor(SkyCol);
	Sky->SetIntensity(FMath::Lerp(SkyNight, SkyDay, FMath::Pow(SunA, 0.5f)));
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
	if (DustMat) { DustMat->SetScalarParameterValue("Strength", 1.f + 9.f * SunA); }

	// ---------------------------------------------------------------- street lamps: on at dusk, off after dawn
	const bool bLampsOn = H >= DuskH - 0.75f || H < DawnH + 0.5f;
	LampAlpha = FMath::FInterpConstantTo(LampAlpha, bLampsOn ? 1.f : 0.f, DeltaSeconds, 0.6f);
	for (const FStreet& S : Streets)
	{
		if (ULightComponent* L = S.Light.Get())
		{
			// sodium/LED warm-up: a little stutter while coming on
			float K = LampAlpha;
			if (LampAlpha > 0.f && LampAlpha < 1.f) { K *= 0.6f + 0.4f * FMath::Abs(FMath::Sin((float)Time * 23.f + S.Seed * 5.f)); }
			L->SetIntensity(S.Base * K);
			L->SetVisibility(K > 0.001f);
		}
	}
	if (LampMat) { LampMat->SetScalarParameterValue("Strength", 0.15f + 40.f * LampAlpha); }

	// ---------------------------------------------------------------- exposure + height fog
	if (Post)
	{
		Post->Settings.AutoExposureBias = FMath::Lerp(EVNight, EVDay, FMath::Pow(SunA, 0.5f));
		// cooler grade at night, a touch warmer by day
		const float W = FMath::Pow(SunA, 0.5f);
		Post->Settings.bOverride_WhiteTemp = true;
		Post->Settings.WhiteTemp = FMath::Lerp(5700.f, 6700.f, W);
	}
	if (HeightFog)
	{
	}
}

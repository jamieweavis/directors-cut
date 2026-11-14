// Director's Cut — rural landscape + global lighting. See DCLandscape.h.
#include "World/Scene/DCLandscape.h"
#include "World/Scene/DCDayNight.h"
#include "DirectorsCut.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

ADCLandscape::ADCLandscape()
{
	PrimaryActorTick.bCanEverTick = false;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

ADCLandscape* ADCLandscape::Get(const UWorld* World)
{
	if (!World) { return nullptr; }
	TActorIterator<ADCLandscape> It(World);
	return It ? *It : nullptr;
}

void ADCLandscape::BuildLandscape()
{
	if (bBuilt) { return; }
	bBuilt = true;
	BuildLightingAndPost();
	// TODO(Landscape agent): terrain, sky, lane, yard, trees, fields, fences, outdoor night lights.
}

void ADCLandscape::SetCVars()
{
	auto Set = [](const TCHAR* Name, const TCHAR* Value)
	{
		if (IConsoleVariable* V = IConsoleManager::Get().FindConsoleVariable(Name)) { V->Set(Value, ECVF_SetByCode); }
	};
	// Manual exposure lives around EV ~ -3..-1.5. Centre Lumen's cached-lighting range on it so nothing clips.
	Set(TEXT("r.EyeAdaptation.CachedLightingPreExposure"), TEXT("4"));
	Set(TEXT("r.LocalFogVolume.RenderIntoVolumetricFog"), TEXT("1"));
	Set(TEXT("r.LocalFogVolume.MaxDensityIntoVolumetricFog"), TEXT("0.5"));
	Set(TEXT("r.VolumetricFog.GridPixelSize"), TEXT("16"));
	Set(TEXT("r.VolumetricFog.GridSizeZ"), TEXT("64"));
	Set(TEXT("r.VolumetricFog.HistoryWeight"), TEXT("0.9"));
	Set(TEXT("r.MotionBlurQuality"), TEXT("0"));
	FString Extra;
	if (FParse::Value(FCommandLine::Get(), TEXT("DCCVars="), Extra, false))
	{
		// -DCCVars=name=value;name=value  (perf experiments)
		TArray<FString> Pairs; Extra.ParseIntoArray(Pairs, TEXT(";"));
		for (const FString& P : Pairs) { FString K, V; if (P.Split(TEXT("="), &K, &V)) { Set(*K, *V); } }
	}
}

void ADCLandscape::BuildLightingAndPost()
{
	SetCVars();

	APostProcessVolume* PPV = GetWorld()->SpawnActor<APostProcessVolume>();
	Post = PPV;
	PPV->bUnbound = true;
	PPV->Priority = -1.f; // gameplay (camera / reveal) volumes can override
	FPostProcessSettings& S = PPV->Settings;
	S.bOverride_AutoExposureMethod = true; S.AutoExposureMethod = AEM_Manual;
	S.bOverride_AutoExposureBias = true; S.AutoExposureBias = -2.f; // driven by ADCDayNight
	S.bOverride_AutoExposureApplyPhysicalCameraExposure = true; S.AutoExposureApplyPhysicalCameraExposure = false;
	S.bOverride_LocalExposureMethod = true; S.LocalExposureMethod = ELocalExposureMethod::Bilateral;
	S.bOverride_LocalExposureHighlightContrastScale = true; S.LocalExposureHighlightContrastScale = 0.8f;
	S.bOverride_LocalExposureShadowContrastScale = true; S.LocalExposureShadowContrastScale = 0.95f;
	S.bOverride_BloomMethod = true; S.BloomMethod = BM_SOG;
	S.bOverride_BloomIntensity = true; S.BloomIntensity = 0.4f;
	S.bOverride_BloomThreshold = true; S.BloomThreshold = -1.f;
	S.bOverride_FilmGrainIntensity = true; S.FilmGrainIntensity = 0.f;
	S.bOverride_VignetteIntensity = true; S.VignetteIntensity = 0.45f;
	S.bOverride_SceneFringeIntensity = true; S.SceneFringeIntensity = 0.f;
	S.bOverride_MotionBlurAmount = true; S.MotionBlurAmount = 0.f;
	S.bOverride_LensFlareIntensity = true; S.LensFlareIntensity = 0.f;
	// contemporary, slightly desaturated grade; cool shadows, neutral highlights
	S.bOverride_ColorSaturation = true; S.ColorSaturation = FVector4(1.f, 1.f, 1.f, 0.86f);
	S.bOverride_ColorContrast = true; S.ColorContrast = FVector4(1.f, 1.f, 1.f, 1.08f);
	S.bOverride_ColorGainShadows = true; S.ColorGainShadows = FVector4(0.94f, 1.0f, 1.06f, 1.f);
	S.bOverride_ColorSaturationHighlights = true; S.ColorSaturationHighlights = FVector4(1.f, 1.f, 1.f, 0.9f);
	S.bOverride_ToneCurveAmount = true; S.ToneCurveAmount = 1.f;
	S.bOverride_LumenSceneLightingQuality = true; S.LumenSceneLightingQuality = 1.f;
	S.bOverride_LumenFinalGatherQuality = true; S.LumenFinalGatherQuality = 1.f;
	S.bOverride_LumenSkylightLeaking = true; S.LumenSkylightLeaking = 0.35f;
	S.bOverride_IndirectLightingIntensity = true; S.IndirectLightingIntensity = 1.2f;

	// Height fog with ~zero density: only here because Volumetric Fog (sun shafts in local fog volumes) hangs off it.
	AExponentialHeightFog* Fog = GetWorld()->SpawnActor<AExponentialHeightFog>();
	UExponentialHeightFogComponent* F = Fog ? Fog->GetComponent() : nullptr;
	if (F)
	{
		F->SetFogDensity(0.0002f);
		F->SetSecondFogDensity(0.f);
		F->SetFogHeightFalloff(0.001f);
		F->SetFogInscatteringColor(FLinearColor::Black);
		F->SetDirectionalInscatteringColor(FLinearColor::Black);
		F->SetVolumetricFog(true);
		F->SetVolumetricFogScatteringDistribution(0.6f);
		F->SetVolumetricFogAlbedo(FColor::White);
		F->SetVolumetricFogExtinctionScale(1.f);
		F->SetVolumetricFogStartDistance(2500.f);
		F->SetVolumetricFogDistance(26000.f);
	}

	// Day/night rig
	FActorSpawnParameters P; P.Name = TEXT("DayNight");
	DayNight = GetWorld()->SpawnActor<ADCDayNight>(ADCDayNight::StaticClass(), FTransform::Identity, P);
	if (DayNight)
	{
		DayNight->Post = PPV;
		DayNight->HeightFog = F;
	}
	HeightFog = F;
}

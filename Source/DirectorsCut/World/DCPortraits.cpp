#include "World/DCPortraits.h"
#include "World/DCCrewBody.h"
#include "Core/DCAssetPaths.h"
#include "Animation/AnimSequence.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Styling/SlateBrush.h"
#include "TextureResource.h"
#include "RHIGPUReadback.h"
#include "RenderingThread.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "DirectorsCut.h"

UDCPortraits* UDCPortraits::Instance = nullptr;

// A capture's pixels on their way back from the GPU. The render thread polls the fence and fills Px; the game thread
// only looks at bDone, so nothing ever waits on the GPU.
struct FDCPortraitReadback
{
	FRHIGPUTextureReadback Readback { TEXT("DCPortrait") };
	TArray<FFloat16Color> Px;
	std::atomic<bool> bPolling { false };
	std::atomic<bool> bDone { false };
	std::atomic<bool> bFailed { false };
};

namespace
{
	template <typename T>
	T* LoadQuiet(const TCHAR* Path) { return LoadObject<T>(nullptr, Path, nullptr, LOAD_NoWarn | LOAD_Quiet); }

	constexpr int32 OutSize = 128;            // baked texture (the largest avatar is 84 px)
	constexpr int32 Supersample = 4;          // captured at 512 and box-filtered down: clean edges, no TAA needed
	constexpr int32 CapSize = OutSize * Supersample;
	constexpr int32 MaxBooths = 4;            // portraits rendered side by side
	constexpr int32 SettleFrames = 3;         // frames for the idle pose to reach the body before capturing
	const FVector StudioLocation(0.f, 0.f, -20000.f); // far under the map, out of every light's reach
	constexpr float BoothSpacing = 800.f;

	// Framing: a slightly long lens, turned a little off-axis, level with the face.
	constexpr float Fov = 17.f;
	constexpr float FrameHeight = 50.f;       // cm visible top to bottom (crown to collarbones)
	constexpr float FrameCentreZ = 4.f;       // above the mannequin's head bone (base of the skull)
	constexpr float CamYaw = 14.f;
	constexpr float CamPitch = 2.f;
	constexpr float Exposure = 0.2f;

	uint32 MakeKey(bool bFemale, int32 Body, EDCCriminalType Type) { return (bFemale ? 1u : 0u) | ((uint32)Body << 1) | ((uint32)Type << 9); }
	void SplitKey(uint32 Key, bool& bFemale, int32& Body, EDCCriminalType& Type)
	{
		bFemale = (Key & 1u) != 0;
		Body = (Key >> 1) & 0xFF;
		Type = (EDCCriminalType)((Key >> 9) & 0xFF);
	}

	// -DCPortraitDump (dev): every baked portrait also goes on a contact sheet, <DCShotDir>/portraits.png.
	void DumpToSheet(const TArray<FColor>& Px)
	{
		static const bool bDump = FParse::Param(FCommandLine::Get(), TEXT("DCPortraitDump"));
		if (!bDump) { return; }
		constexpr int32 Cols = 6, Rows = 4, W = Cols * OutSize, H = Rows * OutSize;
		static TArray<FColor> Sheet;
		static int32 Count = 0;
		if (Sheet.Num() == 0) { Sheet.Init(FColor(40, 40, 40, 255), W * H); }
		if (Count >= Cols * Rows) { return; }
		const int32 Ox = (Count % Cols) * OutSize, Oy = (Count / Cols) * OutSize;
		++Count;
		for (int32 y = 0; y < OutSize; ++y)
		{
			for (int32 x = 0; x < OutSize; ++x)
			{
				const FColor& C = Px[y * OutSize + x];
				FColor& D = Sheet[(Oy + y) * W + Ox + x];
				const float A = C.A / 255.f;
				D = FColor((uint8)FMath::Lerp(40.f, (float)C.R, A), (uint8)FMath::Lerp(40.f, (float)C.G, A), (uint8)FMath::Lerp(40.f, (float)C.B, A), 255);
			}
		}
		FString Dir = FPaths::ProjectSavedDir() / TEXT("DCShots");
		FParse::Value(FCommandLine::Get(), TEXT("DCShotDir="), Dir);
		TArray64<uint8> Png;
		FImageUtils::PNGCompressImageArray(W, H, TArrayView64<const FColor>(Sheet.GetData(), Sheet.Num()), Png);
		FFileHelper::SaveArrayToFile(Png, *(Dir / TEXT("portraits.png")));
	}

	FVector3f Aces(FVector3f X)
	{
		auto F = [](float V) { V = FMath::Max(V, 0.f); return FMath::Clamp((V * (2.51f * V + 0.03f)) / (V * (2.43f * V + 0.59f) + 0.14f), 0.f, 1.f); };
		return FVector3f(F(X.X), F(X.Y), F(X.Z));
	}
}

uint32 UDCPortraits::KeyFor(const FDCCriminal& C)
{
	const int32 Num = C.bFemale ? UE_ARRAY_COUNT(DCPaths::CrewBodiesFemale) : UE_ARRAY_COUNT(DCPaths::CrewBodies);
	const int32 Body = (C.Body >= 0 && C.Body < Num) ? C.Body : FMath::Abs(C.Id) % Num; // same pick as the pawn
	return MakeKey(C.bFemale, Body, C.Type);
}

const FSlateBrush* UDCPortraits::Find(uint32 Key)
{
	UDCPortraits* P = Instance;
	if (!P) { return nullptr; }
	if (const TSharedPtr<FSlateBrush>* B = P->Brushes.Find(Key)) { return B->Get(); }
	if (!P->Failed.Contains(Key) && !P->Pending.Contains(Key) && !P->Booths.ContainsByPredicate([Key](const FBooth& B) { return B.Frames >= 0 && B.Key == Key; }))
	{
		P->Pending.Add(Key);
	}
	return nullptr;
}

void UDCPortraits::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Instance = this;
}

void UDCPortraits::Deinitialize()
{
	if (Instance == this) { Instance = nullptr; }
	Brushes.Empty();
	Pending.Empty();
	Booths.Empty();
	Super::Deinitialize();
}

bool UDCPortraits::Busy() const
{
	return Booths.ContainsByPredicate([](const FBooth& B) { return B.Frames >= 0; });
}

bool UDCPortraits::EnsureStudio(UWorld* World)
{
	if (IsValid(Studio) && Studio->GetWorld() == World) { return true; }
	Booths.Empty(); // the old studio went with its world
	FActorSpawnParameters SP;
	SP.ObjectFlags |= RF_Transient;
	SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Studio = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform(StudioLocation), SP);
	if (!Studio) { return false; }
	USceneComponent* Root = NewObject<USceneComponent>(Studio, TEXT("Root"));
	Studio->SetRootComponent(Root);
	Root->RegisterComponent();
	Root->SetWorldLocation(StudioLocation);

	for (int32 i = 0; i < MaxBooths; ++i)
	{
		FBooth& B = Booths.AddDefaulted_GetRef();
		B.Origin = StudioLocation + FVector(i * BoothSpacing, 0.f, 0.f);

		if (!Targets.IsValidIndex(i))
		{
			UTextureRenderTarget2D* T = NewObject<UTextureRenderTarget2D>(this);
			T->RenderTargetFormat = RTF_RGBA16f;
			T->ClearColor = FLinearColor(0.f, 0.f, 0.f, 1.f);
			T->InitAutoFormat(CapSize, CapSize);
			T->UpdateResourceImmediate(true);
			Targets.Add(T);
		}
		B.Target = Targets[i];

		USceneCaptureComponent2D* Cap = NewObject<USceneCaptureComponent2D>(Studio);
		Cap->SetupAttachment(Root);
		Cap->bCaptureEveryFrame = false;
		Cap->bCaptureOnMovement = false;
		Cap->bAlwaysPersistRenderingState = false;
		Cap->CaptureSource = SCS_SceneColorHDR;
		Cap->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
		Cap->FOVAngle = Fov;
		Cap->TextureTarget = Targets[i];
		// Only our lights on only the body: no sun, sky, fog or screen-space effects, so every portrait matches.
		FEngineShowFlags& F = Cap->ShowFlags;
		F.SetAtmosphere(false); F.SetFog(false); F.SetVolumetricFog(false); F.SetCloud(false);
		F.SetSkyLighting(false); F.SetDirectionalLights(false); F.SetDynamicShadows(false); F.SetContactShadows(false);
		F.SetAmbientOcclusion(false); F.SetDistanceFieldAO(false); F.SetScreenSpaceReflections(false);
		F.SetLumenGlobalIllumination(false); F.SetLumenReflections(false);
		F.SetAntiAliasing(false); F.SetTemporalAA(false); F.SetMotionBlur(false); F.SetBloom(false); F.SetEyeAdaptation(false);
		F.SetPostProcessMaterial(false);
		FPostProcessSettings& PP = Cap->PostProcessSettings;
		PP.bOverride_DynamicGlobalIlluminationMethod = true; PP.DynamicGlobalIlluminationMethod = EDynamicGlobalIlluminationMethod::None;
		PP.bOverride_ReflectionMethod = true; PP.ReflectionMethod = EReflectionMethod::None;
		PP.bOverride_AutoExposureMethod = true; PP.AutoExposureMethod = AEM_Manual;
		PP.bOverride_AutoExposureBias = true; PP.AutoExposureBias = 0.f;
		Cap->RegisterComponent();
		B.Capture = Cap;

		// Warm key high on the camera side, cool fill low on the other, and a bright rim from behind for the outline.
		struct FL { FVector Offset; FLinearColor Color; float Intensity; float Radius; };
		const FL Lights[] = {
			{ FVector(-85.f, 130.f, 190.f), FLinearColor(1.f, 0.91f, 0.80f), 13.f, 45.f },
			{ FVector(100.f, 120.f, 160.f), FLinearColor(0.72f, 0.82f, 1.f), 10.f, 60.f },
			{ FVector(70.f, -110.f, 210.f), FLinearColor(0.85f, 0.92f, 1.f), 55.f, 10.f },
		};
		for (const FL& L : Lights)
		{
			UPointLightComponent* P = NewObject<UPointLightComponent>(Studio);
			P->SetupAttachment(Root);
			P->SetIntensityUnits(ELightUnits::Candelas);
			P->SetIntensity(L.Intensity);
			P->SetLightColor(L.Color);
			P->SetSourceRadius(L.Radius);
			P->SetSoftSourceRadius(L.Radius);
			P->SetAttenuationRadius(450.f);
			P->SetCastShadows(false);
			P->SetVisibility(false);
			P->RegisterComponent();
			P->SetWorldLocation(B.Origin + L.Offset);
			B.Lights.Add(P);
		}
	}
	return true;
}

bool UDCPortraits::StartJob(FBooth& B, uint32 Key)
{
	bool bFemale; int32 BodyIdx; EDCCriminalType Type;
	SplitKey(Key, bFemale, BodyIdx, Type);
	const DCPaths::FCrewBody* Bodies = bFemale ? DCPaths::CrewBodiesFemale : DCPaths::CrewBodies;
	const int32 Num = bFemale ? UE_ARRAY_COUNT(DCPaths::CrewBodiesFemale) : UE_ARRAY_COUNT(DCPaths::CrewBodies);
	USkeletalMesh* DriverMesh = bFemale ? LoadQuiet<USkeletalMesh>(DCPaths::QuinnMesh) : nullptr;
	if (!DriverMesh) { DriverMesh = LoadQuiet<USkeletalMesh>(DCPaths::MannyMesh); }
	USkeletalMesh* BodyMesh = BodyIdx < Num ? LoadQuiet<USkeletalMesh>(Bodies[BodyIdx].Mesh) : nullptr;
	UMaterialInterface* Skin = BodyIdx < Num ? LoadQuiet<UMaterialInterface>(Bodies[BodyIdx].Skin) : nullptr;
	if (!DriverMesh || !BodyMesh) { return false; }

	USceneComponent* Root = Studio->GetRootComponent();
	USkeletalMeshComponent* D = NewObject<USkeletalMeshComponent>(Studio);
	D->SetupAttachment(Root);
	D->SetSkeletalMesh(DriverMesh);
	D->SetVisibility(false, false);
	D->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	D->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	D->SetTickableWhenPaused(true);
	D->RegisterComponent();
	D->SetWorldLocationAndRotation(B.Origin, FRotator::ZeroRotator);
	if (UAnimSequence* Idle = LoadQuiet<UAnimSequence>(DCPaths::AnimIdle))
	{
		D->PlayAnimation(Idle, false);
		D->SetPosition(0.4f, false);
		D->Stop();
	}

	UDCCrewBodyComponent* Body = NewObject<UDCCrewBodyComponent>(Studio);
	if (!Body->Init(BodyMesh, D))
	{
		Body->DestroyComponent();
		D->DestroyComponent();
		return false;
	}
	Body->SetVisibleInSceneCaptureOnly(true); // never drawn in the game view
	Body->SetCastShadow(false);
	Body->bAffectDistanceFieldLighting = false;
	Body->SetTickableWhenPaused(true);
	Body->RegisterComponent();
	if (Skin)
	{
		const FLinearColor Accent = DC::TypeColor(Type);
		for (int32 i = 0; i < Body->GetNumMaterials(); ++i)
		{
			UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Skin, Studio);
			MID->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(0.035f, 0.033f, 0.032f));
			MID->SetVectorParameterValue(TEXT("AccentColor"), Accent);
			MID->SetVectorParameterValue(TEXT("Tint"), FLinearColor::White);
			Body->SetMaterial(i, MID);
		}
	}
	for (const TWeakObjectPtr<UPointLightComponent>& L : B.Lights) { if (L.IsValid()) { L->SetVisibility(true); } }
	B.Capture->ClearShowOnlyComponents();
	B.Capture->ShowOnlyComponent(Body);
	B.Driver = D;
	B.Body = Body;
	B.Key = Key;
	B.Frames = 0;
	return true;
}

void UDCPortraits::ClearBooth(FBooth& B)
{
	if (B.Body.IsValid()) { B.Body->DestroyComponent(); }
	if (B.Driver.IsValid()) { B.Driver->DestroyComponent(); }
	if (B.Capture.IsValid()) { B.Capture->ClearShowOnlyComponents(); }
	for (const TWeakObjectPtr<UPointLightComponent>& L : B.Lights) { if (L.IsValid()) { L->SetVisibility(false); } }
	B.Body = nullptr;
	B.Driver = nullptr;
	B.Frames = -1;
	B.bCaptured = false;
	B.Readback.Reset(); // a poll still queued keeps its own reference
}

void UDCPortraits::CaptureJob(FBooth& B)
{
	USceneCaptureComponent2D* Cap = B.Capture.Get();
	USkeletalMeshComponent* D = B.Driver.Get();
	if (!Cap || !D || !B.Body.IsValid() || !B.Target.IsValid()) { Failed.Add(B.Key); ClearBooth(B); return; }

	// Frame the face from the driver's head bone (the body is sized to stand in the same place).
	const FVector Head = D->GetBoneLocation(TEXT("head"));
	const FVector Focus = Head + FVector(0.f, 0.f, FrameCentreZ);
	const float Dist = (FrameHeight * 0.5f) / FMath::Tan(FMath::DegreesToRadians(Fov * 0.5f));
	const FVector Dir = FRotator(CamPitch, 90.f + CamYaw, 0.f).Vector(); // the mannequin faces +Y
	const FVector CamLoc = Focus + Dir * Dist;
	Cap->SetWorldLocationAndRotation(CamLoc, (-Dir).Rotation());
	Cap->CaptureScene();
	++Captures;
	B.bCaptured = true;

	// Copy the target into a staging texture behind the capture; Tick polls for it.
	TSharedPtr<FDCPortraitReadback, ESPMode::ThreadSafe> R = MakeShared<FDCPortraitReadback, ESPMode::ThreadSafe>();
	B.Readback = R;
	FTextureRenderTargetResource* RT = B.Target->GameThread_GetRenderTargetResource();
	ENQUEUE_RENDER_COMMAND(DCPortraitCopy)([R, RT](FRHICommandListImmediate& RHICmdList)
	{
		FRHITexture* Tex = RT ? RT->GetRenderTargetTexture() : nullptr;
		if (!Tex) { R->bFailed = true; return; }
		RHICmdList.Transition(FRHITransitionInfo(Tex, ERHIAccess::Unknown, ERHIAccess::CopySrc));
		R->Readback.EnqueueCopy(RHICmdList, Tex);
		RHICmdList.Transition(FRHITransitionInfo(Tex, ERHIAccess::CopySrc, ERHIAccess::SRVMask));
	});
}

void UDCPortraits::PollReadback(FBooth& B)
{
	TSharedPtr<FDCPortraitReadback, ESPMode::ThreadSafe> R = B.Readback;
	if (!R || R->bPolling || R->bDone || R->bFailed) { return; }
	R->bPolling = true;
	ENQUEUE_RENDER_COMMAND(DCPortraitPoll)([R](FRHICommandListImmediate&)
	{
		if (R->Readback.IsReady())
		{
			int32 Pitch = 0;
			const FFloat16Color* Src = (const FFloat16Color*)R->Readback.Lock(Pitch);
			if (Src && Pitch >= CapSize)
			{
				R->Px.SetNumUninitialized(CapSize * CapSize);
				for (int32 y = 0; y < CapSize; ++y) { FMemory::Memcpy(&R->Px[y * CapSize], Src + y * Pitch, CapSize * sizeof(FFloat16Color)); }
				R->bDone = true;
			}
			else { R->bFailed = true; }
			R->Readback.Unlock();
		}
		R->bPolling = false;
	});
}

void UDCPortraits::FinishJob(FBooth& B)
{
	const double T0 = FPlatformTime::Seconds();
	const TArray<FFloat16Color>& Px = B.Readback->Px;
	if (Px.Num() != CapSize * CapSize) { Failed.Add(B.Key); ClearBooth(B); return; }

	bool bFemale; int32 BodyIdx; EDCCriminalType Type;
	SplitKey(B.Key, bFemale, BodyIdx, Type);
	const FLinearColor TC = DC::TypeColor(Type);

	// Backdrop: a soft studio gradient carrying a hint of the type colour, glowing behind the head.
	const FVector3f Top = FMath::Lerp(FVector3f(0.040f, 0.046f, 0.058f), FVector3f(TC.R, TC.G, TC.B) * 0.30f, 0.30f);
	const FVector3f Bottom(0.010f, 0.012f, 0.016f);
	TArray<FColor> Out;
	Out.SetNumUninitialized(OutSize * OutSize);
	const float R = OutSize * 0.5f;
	for (int32 y = 0; y < OutSize; ++y)
	{
		for (int32 x = 0; x < OutSize; ++x)
		{
			FVector3f Sum(0.f);
			float Cover = 0.f;
			for (int32 sy = 0; sy < Supersample; ++sy)
			{
				for (int32 sx = 0; sx < Supersample; ++sx)
				{
					const FFloat16Color& P = Px[(y * Supersample + sy) * CapSize + (x * Supersample + sx)];
					const float Op = 1.f - FMath::Clamp(P.A.GetFloat(), 0.f, 1.f); // capture alpha is inverted
					Sum += FVector3f(P.R.GetFloat(), P.G.GetFloat(), P.B.GetFloat());
					Cover += Op;
				}
			}
			const float N = (float)(Supersample * Supersample);
			Cover /= N;
			Sum /= N;
			const FVector3f Fig = Cover > 0.001f ? Aces(Sum / Cover * Exposure) : FVector3f(0.f);

			const float U = (x + 0.5f) / OutSize, V = (y + 0.5f) / OutSize;
			const float Glow = FMath::Exp(-(FMath::Square(U - 0.5f) + FMath::Square(V - 0.36f)) / 0.07f);
			FVector3f Bg = FMath::Lerp(Top, Bottom, FMath::SmoothStep(0.f, 1.f, V)) + FVector3f(TC.R, TC.G, TC.B) * (0.05f * Glow);
			const FVector3f C = FMath::Lerp(Bg, Fig, Cover);

			// Round mask with a one-pixel soft edge (the UI ring sits on top of it).
			const float Dc = FMath::Sqrt(FMath::Square(x + 0.5f - R) + FMath::Square(y + 0.5f - R));
			const float Mask = FMath::Clamp(R - Dc, 0.f, 1.f);
			Out[y * OutSize + x] = FLinearColor(C.X, C.Y, C.Z, Mask).ToFColor(true);
		}
	}

	UE_LOG(LogDC, Log, TEXT("Portrait %u (%s body %d, type %d): baked, capture #%d, %.1f ms on the game thread"),
		B.Key, bFemale ? TEXT("female") : TEXT("male"), BodyIdx, (int32)Type, Captures, (FPlatformTime::Seconds() - T0) * 1000.0);

	UTexture2D* Tex = UTexture2D::CreateTransient(OutSize, OutSize, PF_B8G8R8A8, NAME_None,
		TConstArrayView64<uint8>((const uint8*)Out.GetData(), Out.Num() * sizeof(FColor)));
	DumpToSheet(Out);
	if (Tex)
	{
		Tex->SRGB = true;
		Tex->Filter = TF_Trilinear;
		Tex->UpdateResource();
		Textures.Add(Tex);
		TSharedPtr<FSlateBrush> Brush = MakeShared<FSlateBrush>();
		Brush->SetResourceObject(Tex);
		Brush->ImageSize = FVector2D(OutSize, OutSize);
		Brush->DrawAs = ESlateBrushDrawType::Image;
		Brushes.Add(B.Key, Brush);
	}
	else { Failed.Add(B.Key); }
	ClearBooth(B);
}

void UDCPortraits::Tick(float DeltaTime)
{
	UGameInstance* GI = GetGameInstance();
	UWorld* World = GI ? GI->GetWorld() : nullptr;
	if (!World || !World->HasBegunPlay() || !EnsureStudio(World)) { return; }

	bool bBaked = false; // at most one bake per frame
	for (FBooth& B : Booths)
	{
		if (B.Frames >= 0)
		{
			if (!B.Body.IsValid() || !B.Driver.IsValid()) { Failed.Add(B.Key); ClearBooth(B); continue; }
			++B.Frames;
			if (!B.bCaptured && B.Frames >= SettleFrames) { CaptureJob(B); }
			else if (B.bCaptured && B.Readback)
			{
				if (B.Readback->bFailed) { Failed.Add(B.Key); ClearBooth(B); }
				else if (B.Readback->bDone && !bBaked) { FinishJob(B); bBaked = true; }
				else { PollReadback(B); }
			}
		}
		while (B.Frames < 0 && Pending.Num() > 0)
		{
			const uint32 Key = Pending[0];
			Pending.RemoveAt(0);
			if (!StartJob(B, Key)) { Failed.Add(Key); UE_LOG(LogDC, Warning, TEXT("Portrait %u: body assets missing"), Key); }
		}
	}
}

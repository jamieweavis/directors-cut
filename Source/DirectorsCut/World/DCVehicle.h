// Director's Cut — a crew car built from generated rounded shells (DCLandMesh::Loft) and a few primitives (World owns). One per mission slot, parked
// nose first in an angled bay (DCSite::ParkingSlot). Drives the car park's one-way loop (DCSite::CarRoute) forwards
// only: the route is the REAR axle's path and the body turns about it like a real car, with a smooth speed profile
// (gentle pull away, slowing for corners, a soft stop in the bay), front wheels steered from the path's curvature,
// body pitch/roll, indicators before turns, headlights at night, brake glow, and doors that pop open for boarding.
// Every drive is planned up front (FDCCarPlan) and played back on a fixed timeline, so the world director can predict
// where each moving car will be and hold a start back until it can't clash with any of them.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DCVehicle.generated.h"

class UStaticMeshComponent;
class UMaterialInstanceDynamic;
class USpotLightComponent;
class UPointLightComponent;

UENUM()
enum class EDCCarStyle : uint8 { Hatch, Estate, Pickup, Van };

UENUM()
enum class EDCCarState : uint8 { Parked, Driving, Gone };

/** One planned drive: dense rear-axle samples with heading, curvature, speed and the time each is reached. */
struct DIRECTORSCUT_API FDCCarPlan
{
	struct FSample
	{
		FVector2D P = FVector2D::ZeroVector; // rear axle
		FVector2D T = FVector2D(1.f, 0.f);   // heading (unit)
		float S = 0.f, Curv = 0.f, V = 0.f, A = 0.f, Time = 0.f;
		int8 Signal = 0;                     // indicator: -1 left, +1 right
	};
	TArray<FSample> Samples;
	float WheelBase = 250.f, Length = 400.f, Width = 170.f;
	float HideAtTime = -1.f;   // departures vanish off-screen at this time
	int32 Slot = INDEX_NONE;
	int32 Bay = INDEX_NONE;
	bool bArrive = false;

	bool IsValid() const { return Samples.Num() > 1; }
	float Duration() const { return IsValid() ? Samples.Last().Time : 0.f; }
	/** Interpolated sample at time T (clamped to the ends). */
	FSample At(float T) const;
	/** Car centre and heading at time T. */
	void PoseAt(float T, FVector2D& OutCentre, FVector2D& OutFwd) const;
};

UCLASS()
class DIRECTORSCUT_API ADCVehicle : public AActor
{
	GENERATED_BODY()
public:
	ADCVehicle();

	/** Build the body (call once after spawning). Style/colour picked from Seed. */
	void Setup(int32 Seed);
	/** Teleport to a parking slot (visible, parked). */
	void ParkAt(const FTransform& Slot);
	/** Plan (without starting) a drive for parking slot SlotIndex: out of its bay and away, or in from the lane. */
	FDCCarPlan PlanDrive(int32 SlotIndex, bool bArrive) const;
	/** Play a planned drive from its start (arrivals appear off-screen on the lane). */
	void StartDrive(const FDCCarPlan& Plan);
	/** Leave slot I forwards along the one-way loop and disappear off-screen. */
	void DriveAway(int32 SlotIndex) { StartDrive(PlanDrive(SlotIndex, false)); }
	/** Appear off-screen on the lane, drive round the loop and park nose first in slot I. */
	void DriveIn(int32 SlotIndex) { StartDrive(PlanDrive(SlotIndex, true)); }
	/** Hidden, off-screen (on a job). */
	void SetGone();
	/** Crew aboard, waiting for a gap: engine ticking over, tail lights on. */
	void SetEngineRunning(bool bOn) { bEngine = bOn; }

	/** Doors: 0 = closed, 1 = open (animated). Side: -1 = left (-Y local), +1 = right, 0 = both. */
	void SetDoorsOpen(bool bOpen, int32 Side = 0);

	EDCCarState GetCarState() const { return CarState; }
	bool IsParked() const { return CarState == EDCCarState::Parked; }
	bool IsGone() const { return CarState == EDCCarState::Gone; }
	bool IsDriving() const { return CarState == EDCCarState::Driving; }
	/** World-space footprint (XY) while parked, for pedestrian avoidance (axis aligned). */
	FBox2D GetFootprint() const;
	/** Oriented footprint: centre, half extents (along, across), yaw in radians. */
	void GetFootprintOriented(FVector2D& OutCentre, FVector2D& OutHalf, float& OutYawRad) const;
	EDCCarStyle GetStyle() const { return Style; }
	float GetLength() const { return Length; }
	float GetWidth() const { return Width; }
	/** Where this car will be Ahead seconds from now (car time) while it is driving. False once it's parked at the end
	 *  of its drive or gone off-screen (or isn't driving at all). */
	bool PredictPose(float Ahead, FVector2D& OutCentre, FVector2D& OutFwd) const;
	/** Seconds (car time) a drive in to slot I takes, from appearing on the lane to parked. */
	float ArriveSeconds(int32 SlotIndex) const;

	virtual void Tick(float DeltaSeconds) override;

protected:
	UPROPERTY() TObjectPtr<USceneComponent> Root;
	UPROPERTY() TObjectPtr<USceneComponent> BodyPivot;   // pitch/roll/bob
	UPROPERTY() TArray<TObjectPtr<USceneComponent>> WheelSteer; // FL, FR, RL, RR (yaw)
	UPROPERTY() TArray<TObjectPtr<USceneComponent>> WheelSpin;  // (pitch)
	UPROPERTY() TArray<TObjectPtr<USceneComponent>> DoorHinge;  // left, right
	UPROPERTY() TArray<TObjectPtr<USpotLightComponent>> HeadLights;
	UPROPERTY() TObjectPtr<UPointLightComponent> TailLight;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> HeadMID;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> TailMID;
	UPROPERTY() TArray<TObjectPtr<UMaterialInstanceDynamic>> Owned;

	EDCCarStyle Style = EDCCarStyle::Hatch;
	EDCCarState CarState = EDCCarState::Parked;
	float Length = 400.f, Width = 176.f, WheelBase = 250.f, WheelR = 31.f;

	FDCCarPlan Plan;
	float DriveTime = 0.f;
	float Speed = 0.f, PrevSpeed = 0.f;
	float GroundZ = -3.f;
	float Steer = 0.f;
	float SpinAngle = 0.f;
	float Pitch = 0.f, Roll = 0.f, BobT = 0.f;
	float Brake = 0.f;       // 0..1 brake glow
	float DoorTarget[2] = { 0.f, 0.f };
	float DoorAlpha[2] = { 0.f, 0.f };
	float LightsAlpha = 0.f;
	float BlinkT = 0.f;
	int8 Signal = 0;
	bool bEngine = false;
	int32 TargetSlot = INDEX_NONE;
	mutable TMap<int32, float> ArriveSecondsCache;
	UPROPERTY() TArray<TObjectPtr<UMaterialInstanceDynamic>> IndMID; // left, right indicators

	UMaterialInstanceDynamic* MakeMID(const TCHAR* Path, const FLinearColor& Color, float A, float B);
	UStaticMeshComponent* Part(USceneComponent* Parent, UStaticMesh* Mesh, const FVector& Loc, const FVector& Size, UMaterialInterface* Mat, const FRotator& Rot = FRotator::ZeroRotator, bool bShadow = true);
	void BuildBody(int32 Seed);
	/** Headlight spots + tail point light. */
	void BuildLights(float HL, float HW, float LampZ);
	void UpdateIndicators(float Dt);
	/** Once per slot and direction: logs the car body's closest approach to the gateway (posts, open gate, mailbox). */
	void CheckGateClearance(const FDCCarPlan& P, int32 SlotIndex) const;
	void UpdateLights(float Dt, bool bDriving);
	float TraceGround(const FVector& P) const;
};

// Director's Cut — a crew car built from primitives (World owns). One per mission slot, parked in the yard
// (DCSite::ParkingSlot). Drives smooth spline routes (DCSite::DepartRoute) with acceleration, cornering, braking,
// steering wheels that spin, body pitch/roll, headlights at night, brake glow, and doors that pop open for boarding.
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
	/** Leave slot I along DCSite::DepartRoute and disappear off-screen. */
	void DriveAway(int32 SlotIndex);
	/** Appear off-screen on the lane, drive in and reverse into slot I. */
	void DriveIn(int32 SlotIndex);
	/** Hidden, off-screen (on a job). */
	void SetGone();

	/** Doors: 0 = closed, 1 = open (animated). Side: -1 = left (-Y local), +1 = right, 0 = both. */
	void SetDoorsOpen(bool bOpen, int32 Side = 0);

	EDCCarState GetCarState() const { return CarState; }
	bool IsParked() const { return CarState == EDCCarState::Parked; }
	bool IsGone() const { return CarState == EDCCarState::Gone; }
	/** World-space footprint (XY) while parked, for pedestrian avoidance. */
	FBox2D GetFootprint() const;
	EDCCarStyle GetStyle() const { return Style; }

	/** Rough real seconds a full DriveIn takes (for scheduling the return). */
	static float EstimateDriveInSeconds() { return 11.5f; }

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

	// Route: dense samples of a centripetal Catmull-Rom spline (+ arc length, speed limit, gear).
	struct FSample { FVector P; float S = 0.f; float VMax = 0.f; float Curv = 0.f; bool bReverse = false; };
	TArray<FSample> Route;
	float RouteS = 0.f;      // distance travelled
	float Speed = 0.f;       // cm/s along the route (>= 0)
	float PrevSpeed = 0.f;
	float HideAtS = -1.f;    // vanish when passing this distance (departures)
	float GroundZ = -3.f;
	float Steer = 0.f;
	float SpinAngle = 0.f;
	float Pitch = 0.f, Roll = 0.f, BobT = 0.f;
	float Brake = 0.f;       // 0..1 brake glow
	float DoorTarget[2] = { 0.f, 0.f };
	float DoorAlpha[2] = { 0.f, 0.f };
	float LightsAlpha = 0.f;
	// Axle model: the leading axle (front going forward, rear reversing) follows the route; the other is dragged
	// behind at wheelbase distance, so the body turns about its trailing axle like a real car.
	bool bAxles = false;
	FVector2D FrontAx = FVector2D::ZeroVector, RearAx = FVector2D::ZeroVector;
	FVector2D LeadOffset = FVector2D::ZeroVector; // eases out the jump when the leading axle swaps at a gear change
	bool bPrevReverse = false;
	// Parking bay this route leaves from / ends in (near it the car follows the tuned route pose, see Tick).
	FVector2D BayPos = FVector2D::ZeroVector;
	bool bBayAtStart = false, bBayAtEnd = false;
	float AxleHalf() const { return Length * 0.3f; }
	int32 TargetSlot = INDEX_NONE;
	float StuckTime = 0.f;
	float DriveTime = 0.f;

	UMaterialInstanceDynamic* MakeMID(const TCHAR* Path, const FLinearColor& Color, float A, float B);
	UStaticMeshComponent* Part(USceneComponent* Parent, UStaticMesh* Mesh, const FVector& Loc, const FVector& Size, UMaterialInterface* Mat, const FRotator& Rot = FRotator::ZeroRotator, bool bShadow = true);
	void BuildBody(int32 Seed);
	/** Headlight spots + tail point light. */
	void BuildLights(float HL, float HW, float LampZ);
	void BuildRoute(const TArray<TArray<FVector>>& Segments, const TArray<bool>& Reverse, float StartSpeed, float EndSpeed);
	FSample SampleAt(float S, FVector* OutTangent) const;
	/** Once per slot and direction: logs the car body's closest approach to the gateway (posts, open gate, mailbox). */
	void CheckGateClearance(int32 SlotIndex, bool bArriving) const;
	void UpdateLights(float Dt, bool bDriving);
	float TraceGround(const FVector& P) const;
};

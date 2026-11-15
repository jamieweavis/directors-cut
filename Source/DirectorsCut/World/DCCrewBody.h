// Director's Cut — a crew body mesh with its own skeleton, posed every frame from the (hidden) mannequin.
// The mannequin keeps running ABP_Unarmed, so every existing animation drives the new body with no retarget assets.
#pragma once

#include "CoreMinimal.h"
#include "Components/PoseableMeshComponent.h"
#include "DCCrewBody.generated.h"

UCLASS()
class DIRECTORSCUT_API UDCCrewBodyComponent : public UPoseableMeshComponent
{
	GENERATED_BODY()
public:
	UDCCrewBodyComponent();

	/** Attach to Source (the animated mannequin), size the body to match it and build the bone map. False if no bones map. */
	bool Init(USkeletalMesh* BodyMesh, USkeletalMeshComponent* Source);

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	TWeakObjectPtr<USkeletalMeshComponent> Source;
	TArray<int32> SrcIndex;        // per target bone: mapped source bone, or INDEX_NONE (follows its parent)
	TArray<FQuat> SrcRefInvCS;     // per target bone: inverse of the mapped source bone's ref rotation (component space)
	TArray<FTransform> TgtRefCS;   // target ref pose, component space
	TArray<FVector> SrcRefLocCS;   // per target bone: the mapped source bone's ref location (component space)
	int32 HipBone = INDEX_NONE;    // target bone that also takes the source pelvis translation
	FQuat RelRot = FQuat::Identity;
	float Scale = 1.f;
	TArray<FTransform> CS;         // scratch

	void CopyPose();
};

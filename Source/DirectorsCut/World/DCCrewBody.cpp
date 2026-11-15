#include "World/DCCrewBody.h"
#include "AnimationRuntime.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "ReferenceSkeleton.h"
#include "DirectorsCut.h"

namespace
{
	// Crew body bone -> mannequin bone. Unlisted bones (twists, the body's own pelvis) keep their ref pose relative to
	// their parent. The first entry also takes the mannequin pelvis translation (bob, crouch, lying down).
	const TPair<const TCHAR*, const TCHAR*> BoneMap[] = {
		{ TEXT("Hip"), TEXT("pelvis") },
		{ TEXT("Waist"), TEXT("spine_01") },
		{ TEXT("Spine01"), TEXT("spine_03") },
		{ TEXT("Spine02"), TEXT("spine_05") },
		{ TEXT("NeckTwist01"), TEXT("neck_01") },
		{ TEXT("NeckTwist02"), TEXT("neck_02") },
		{ TEXT("Head"), TEXT("head") },
		{ TEXT("L_Clavicle"), TEXT("clavicle_l") },
		{ TEXT("L_Upperarm"), TEXT("upperarm_l") },
		{ TEXT("L_Forearm"), TEXT("lowerarm_l") },
		{ TEXT("L_Hand"), TEXT("hand_l") },
		{ TEXT("R_Clavicle"), TEXT("clavicle_r") },
		{ TEXT("R_Upperarm"), TEXT("upperarm_r") },
		{ TEXT("R_Forearm"), TEXT("lowerarm_r") },
		{ TEXT("R_Hand"), TEXT("hand_r") },
		{ TEXT("L_Thigh"), TEXT("thigh_l") },
		{ TEXT("L_Calf"), TEXT("calf_l") },
		{ TEXT("L_Foot"), TEXT("foot_l") },
		{ TEXT("R_Thigh"), TEXT("thigh_r") },
		{ TEXT("R_Calf"), TEXT("calf_r") },
		{ TEXT("R_Foot"), TEXT("foot_r") },
	};

	// Yaw of the body mesh relative to the mannequin mesh (both face +Y in their own component space).
	constexpr float BodyYaw = 0.f;

	float MeshHeight(const USkeletalMesh* M) { return M ? M->GetImportedBounds().BoxExtent.Z * 2.f : 0.f; }
}

UDCCrewBodyComponent::UDCCrewBodyComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

bool UDCCrewBodyComponent::Init(USkeletalMesh* BodyMesh, USkeletalMeshComponent* InSource)
{
	USkeletalMesh* SrcMesh = InSource ? InSource->GetSkeletalMeshAsset() : nullptr;
	if (!BodyMesh || !SrcMesh) { return false; }
	Source = InSource;
	SetSkinnedAssetAndUpdate(BodyMesh);

	const FReferenceSkeleton& T = BodyMesh->GetRefSkeleton();
	const FReferenceSkeleton& S = SrcMesh->GetRefSkeleton();
	const int32 N = T.GetNum();

	static TSet<FName> Logged;
	if (!Logged.Contains(BodyMesh->GetFName()))
	{
		Logged.Add(BodyMesh->GetFName());
		FString Tree;
		for (int32 i = 0; i < N; ++i)
		{
			const int32 P = T.GetParentIndex(i);
			Tree += FString::Printf(TEXT(" %s<%s"), *T.GetBoneName(i).ToString(), P >= 0 ? *T.GetBoneName(P).ToString() : TEXT("-"));
		}
		UE_LOG(LogDC, Log, TEXT("Crew body %s: %d bones:%s"), *BodyMesh->GetName(), N, *Tree);
	}

	SrcIndex.Init(INDEX_NONE, N);
	SrcRefInvCS.Init(FQuat::Identity, N);
	SrcRefLocCS.Init(FVector::ZeroVector, N);
	TgtRefCS.SetNum(N);
	for (int32 i = 0; i < N; ++i) { TgtRefCS[i] = FAnimationRuntime::GetComponentSpaceTransformRefPose(T, i); }

	int32 Mapped = 0;
	HipBone = INDEX_NONE;
	for (const TPair<const TCHAR*, const TCHAR*>& Pair : BoneMap)
	{
		const int32 Ti = T.FindBoneIndex(Pair.Key);
		const int32 Si = S.FindBoneIndex(Pair.Value);
		if (Ti == INDEX_NONE || Si == INDEX_NONE) { continue; } // bodies with mannequin names map by name below
		const FTransform SrcRef = FAnimationRuntime::GetComponentSpaceTransformRefPose(S, Si);
		SrcIndex[Ti] = Si;
		SrcRefInvCS[Ti] = SrcRef.GetRotation().Inverse();
		SrcRefLocCS[Ti] = SrcRef.GetLocation();
		if (Mapped == 0) { HipBone = Ti; }
		++Mapped;
	}
	// Bodies exported with mannequin bone names (fingers and all) map one-to-one; pelvis takes the translation.
	const bool bByName = T.FindBoneIndex(TEXT("spine_01")) != INDEX_NONE;
	if (bByName) { HipBone = INDEX_NONE; }
	for (int32 Ti = 0; bByName && Ti < N; ++Ti)
	{
		if (SrcIndex[Ti] != INDEX_NONE) { continue; }
		const int32 Si = S.FindBoneIndex(T.GetBoneName(Ti));
		if (Si == INDEX_NONE || Si == 0) { continue; } // leave the root alone
		const FTransform SrcRef = FAnimationRuntime::GetComponentSpaceTransformRefPose(S, Si);
		SrcIndex[Ti] = Si;
		SrcRefInvCS[Ti] = SrcRef.GetRotation().Inverse();
		SrcRefLocCS[Ti] = SrcRef.GetLocation();
		if (HipBone == INDEX_NONE && T.GetBoneName(Ti) == TEXT("pelvis")) { HipBone = Ti; }
		++Mapped;
	}

	UE_LOG(LogDC, Log, TEXT("Crew body %s: %d of %d bones follow the mannequin"), *BodyMesh->GetName(), Mapped, N);

	// The bodies rest in a different pose from the mannequin (arms straight out vs angled down), so a rotation change
	// copied as-is swings their limbs somewhere else (hands behind the back). Swing each mapped bone in the body's ref
	// pose to point where the mannequin's bone points; the copied changes then start from the same pose. Bones with
	// no single mapped child (hands, pelvis, upper spine) keep their parent's swing.
	TgtRefRotCS.SetNum(N);
	TArray<FQuat> Align;
	Align.Init(FQuat::Identity, N);
	for (int32 i = 0; i < N; ++i)
	{
		const int32 P = T.GetParentIndex(i);
		Align[i] = P >= 0 ? Align[P] : FQuat::Identity;
		if (SrcIndex[i] != INDEX_NONE)
		{
			int32 Child = INDEX_NONE, NumChildren = 0;
			for (int32 j = i + 1; j < N; ++j)
			{
				if (T.GetParentIndex(j) == i && SrcIndex[j] != INDEX_NONE) { Child = j; ++NumChildren; }
			}
			if (NumChildren == 1)
			{
				const FVector TDir = (TgtRefCS[Child].GetLocation() - TgtRefCS[i].GetLocation()).GetSafeNormal();
				const FVector SDir = (SrcRefLocCS[Child] - SrcRefLocCS[i]).GetSafeNormal();
				if (!TDir.IsNearlyZero() && !SDir.IsNearlyZero()) { Align[i] = FQuat::FindBetweenNormals(TDir, SDir); }
			}
		}
		TgtRefRotCS[i] = (Align[i] * TgtRefCS[i].GetRotation()).GetNormalized();
	}

	// Same standing height as the mannequin, feet on its feet.
	const float BodyH = MeshHeight(BodyMesh);
	Scale = BodyH > 1.f ? MeshHeight(SrcMesh) / BodyH : 1.f;
	RelRot = FQuat(FRotator(0.f, BodyYaw, 0.f));
	SetupAttachment(InSource);
	SetRelativeTransform(FTransform(RelRot, FVector::ZeroVector, FVector(Scale)));
	AddTickPrerequisiteComponent(InSource);
	return Mapped > 0;
}

void UDCCrewBodyComponent::CopyPose()
{
	USkeletalMeshComponent* Src = Source.Get();
	const USkinnedAsset* BodyMesh = GetSkinnedAsset();
	if (!Src || !BodyMesh) { return; }
	const TArray<FTransform>& SrcCS = Src->GetComponentSpaceTransforms();
	const FReferenceSkeleton& T = BodyMesh->GetRefSkeleton();
	const TArray<FTransform>& RefLocal = T.GetRefBonePose();
	const int32 N = T.GetNum();
	if (BoneSpaceTransforms.Num() != N || SrcIndex.Num() != N) { return; }
	CS.SetNum(N, EAllowShrinking::No);

	const FQuat RelInv = RelRot.Inverse();
	for (int32 i = 0; i < N; ++i)
	{
		const int32 P = T.GetParentIndex(i);
		const FTransform& Parent = P >= 0 ? CS[P] : FTransform::Identity;
		// Ref pose under the posed parent (keeps bone scale: the body's root bone carries a metres -> cm scale).
		FTransform B = RefLocal[i] * Parent;
		const int32 Si = SrcIndex[i];
		if (Si != INDEX_NONE && SrcCS.IsValidIndex(Si))
		{
			// Same component-space rotation change as the mannequin bone made from its ref pose.
			const FQuat Delta = RelInv * (SrcCS[Si].GetRotation() * SrcRefInvCS[i]) * RelRot;
			B.SetRotation((Delta * TgtRefRotCS[i]).GetNormalized());
			if (i == HipBone)
			{
				B.SetTranslation(TgtRefCS[i].GetLocation() + RelInv.RotateVector(SrcCS[Si].GetLocation() - SrcRefLocCS[i]) / Scale);
			}
		}
		CS[i] = B;
		BoneSpaceTransforms[i] = B.GetRelativeTransform(Parent);
	}
	MarkRefreshTransformDirty();
}

void UDCCrewBodyComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	if (IsVisible()) { CopyPose(); }
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

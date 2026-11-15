// Director's Cut — room furnishing toolkit (private to World/Scene). The layout solver's types, the context a room
// recipe works in and the shared prop kit. See DCHideoutFurnish.cpp for how a layout is found; each room's recipe is
// in its own file (DCHideoutFurnishOffice.cpp, ...).
#pragma once
#include "CoreMinimal.h"
#include "World/Scene/DCHideoutBuilder.h"
#include "World/DCRoomModule.h"
#include "Core/DCSiteLayout.h"
#include "Core/DCWorldTags.h"
#include "DirectorsCut.h"

namespace DCFurnish
{
	using FV2 = FVector2D;

	constexpr float KInsetWall = 30.f;   // inner face of a tall wall (masonry 24 + skin)
	constexpr float KInsetPart = 14.f;   // a partition stub is centred on the edge
	constexpr float KInsetOpen = 30.f;   // collapsed front: stub + foundation lip
	constexpr float KDoorW = 150.f;      // must match DCHideoutGrid.cpp
	constexpr float KTall = 1.0e4f;
	inline const FV2 KToCam = FV2(-1.f, -1.f).GetSafeNormal(); // the iso camera looks from -X-Y

	inline FV2 Dir(float YawDeg) { const float R = FMath::DegreesToRadians(YawDeg); return FV2(FMath::Cos(R), FMath::Sin(R)); }
	inline float YawOf(const FV2& D) { return FMath::RadiansToDegrees(FMath::Atan2(D.Y, D.X)); }
	inline FV2 Rot(const FV2& V, float YawDeg) { const FV2 X = Dir(YawDeg); return FV2(V.X * X.X - V.Y * X.Y, V.X * X.Y + V.Y * X.X); }
	inline float Cross(const FV2& A, const FV2& B) { return A.X * B.Y - A.Y * B.X; }
	inline FV2 LineX(const FV2& P0, const FV2& D0, const FV2& P1, const FV2& D1)
	{
		const float Den = Cross(D0, D1);
		if (FMath::Abs(Den) < 1e-4f) { return P0; }
		return P0 + D0 * (Cross(P1 - P0, D1) / Den);
	}

	/** Oriented box on the floor plan. Yaw = direction of the local +X axis. */
	struct FObb
	{
		FV2 C = FV2::ZeroVector;
		FV2 H = FV2(1.f, 1.f);
		float Yaw = 0.f;
		void Corners(FV2 O[4]) const
		{
			const FV2 X = Dir(Yaw), Y(-X.Y, X.X);
			O[0] = C - X * H.X - Y * H.Y; O[1] = C + X * H.X - Y * H.Y; O[2] = C + X * H.X + Y * H.Y; O[3] = C - X * H.X + Y * H.Y;
		}
		float DistTo(const FV2& P) const
		{
			const FV2 X = Dir(Yaw), Y(-X.Y, X.X), D = P - C;
			const float DX = FMath::Max(FMath::Abs(FV2::DotProduct(D, X)) - H.X, 0.f), DY = FMath::Max(FMath::Abs(FV2::DotProduct(D, Y)) - H.Y, 0.f);
			return FMath::Sqrt(DX * DX + DY * DY);
		}
	};

	/** True unless the boxes are at least Gap apart (separating axis test). */
	inline bool Overlap(const FObb& A, const FObb& B, float Gap)
	{
		FV2 CA[4], CB[4];
		A.Corners(CA); B.Corners(CB);
		const FV2 Axes[4] = { Dir(A.Yaw), Dir(A.Yaw + 90.f), Dir(B.Yaw), Dir(B.Yaw + 90.f) };
		for (const FV2& Ax : Axes)
		{
			float A0 = 1e9f, A1 = -1e9f, B0 = 1e9f, B1 = -1e9f;
			for (int32 k = 0; k < 4; ++k)
			{
				const float PA = FV2::DotProduct(CA[k], Ax), PB = FV2::DotProduct(CB[k], Ax);
				A0 = FMath::Min(A0, PA); A1 = FMath::Max(A1, PA); B0 = FMath::Min(B0, PB); B1 = FMath::Max(B1, PB);
			}
			if (A1 + Gap <= B0 || B1 + Gap <= A0) { return false; }
		}
		return true;
	}

	enum class EKind : uint8 { Wall = 0, Partition = 1, Open = 2 };
	/** Forbidden height range [Z0, Z1] for anything within Reach (from the edge line) of the stretch [T0, T1]. */
	struct FBand { float T0, T1, Z0, Z1, Reach; int32 Win = -1; }; // Win: the window it guards (furniture may brick it up)
	struct FWin { float C, W, Bottom, Top; int32 Idx; };

	struct FEdge
	{
		EKind Kind = EKind::Open;
		bool bBack = false;   // +X / +Y: faces the camera, may carry tall things
		bool bExit = false;   // pawns walk out over it
		FV2 A, B, D, N;       // outer line, direction, inward normal
		float L = 0.f;
		float Inset = 30.f;
		TArray<FBand> Bands;
		TArray<FWin> Windows;
		FV2 At(float T, float Depth) const { return A + D * T + N * Depth; }
		float Proj(const FV2& P) const { return FV2::DotProduct(P - A, D); }
		float Depth(const FV2& P) const { return FV2::DotProduct(P - A, N); }
	};

	/** A placed box with a height range. Rugs only collide with rugs; non-solid parts reserve space but pawns may
	 *  stand there (chair space, the gap in front of the board). */
	struct FPart { FObb Box; float Z0 = 0.f, Z1 = 100.f; bool bRug = false; bool bSolid = true; uint8 Tag = 0; int32 Owner = -1; };
	/** A part in a group's local frame (+X into the room). */
	struct FLocal
	{
		FV2 C; FV2 H; float Z0 = 0.f; float Z1 = 100.f; bool bRug = false; bool bSolid = true;
		uint8 Tag = 0; // TagSeat: a spot held for a chair; a chair (also TagSeat) may take it
		static FLocal Solid(float X, float Y, float HX, float HY, float Z1, float Z0 = 0.f) { FLocal L; L.C = FV2(X, Y); L.H = FV2(HX, HY); L.Z0 = Z0; L.Z1 = Z1; return L; }
		static FLocal Space(float X, float Y, float HX, float HY, float Z1) { FLocal L = Solid(X, Y, HX, HY, Z1); L.bSolid = false; return L; }
		static FLocal Rug(float X, float Y, float HX, float HY) { FLocal L = Solid(X, Y, HX, HY, 2.f); L.bRug = true; L.bSolid = false; return L; }
	};
	constexpr uint8 TagSeat = 1;
	inline bool GDebug = false;
	struct FFrame { FV2 O = FV2::ZeroVector; float Yaw = 0.f; int32 Edge = -1; float T = 0.f; bool bValid = false; };
	struct FCap { FV2 A, B; float R; };

	struct FPlan
	{
		FV2 In[4];
		FEdge E[4];
		FV2 Hub;
		float RoomYaw = 0.f;
		FBox2D InnerBB = FBox2D(ForceInit);
		TArray<FPart> Parts;
		TArray<FObb> Keep;
		TArray<FCap> Caps;
		bool bSoftWindows = false; // while set, window bands don't block (see WindowsHit / Brickup)
		float CapScale = 1.f;      // relaxation: narrower walking lanes...
		bool bHubKeep = true;      // ...and no clear spot in the middle
		int32 HubKeepIdx = -1;
		int32 WindowsLeft = 0;

		void Init(const FV2 Q[4], const int32 Kind[4], const bool bExit[4])
		{
			for (int32 e = 0; e < 4; ++e)
			{
				FEdge& Ed = E[e];
				Ed.Kind = (EKind)FMath::Clamp(Kind[e], 0, 2);
				Ed.bBack = !DCSite::Grid::IsFrontEdge(e);
				Ed.bExit = bExit[e];
				Ed.A = Q[e]; Ed.B = Q[(e + 1) & 3];
				Ed.L = FV2::Distance(Ed.A, Ed.B);
				Ed.D = (Ed.B - Ed.A) / FMath::Max(Ed.L, 1.f);
				Ed.N = FV2(-Ed.D.Y, Ed.D.X);
				Ed.Inset = Ed.Kind == EKind::Wall ? KInsetWall : Ed.Kind == EKind::Partition ? KInsetPart : KInsetOpen;
			}
			for (int32 k = 0; k < 4; ++k)
			{
				const FEdge& P = E[(k + 3) & 3];
				const FEdge& C = E[k];
				In[k] = LineX(P.A + P.N * P.Inset, P.D, C.A + C.N * C.Inset, C.D);
				InnerBB += In[k];
			}
			Hub = (In[0] + In[1] + In[2] + In[3]) * 0.25f;
			// the room's own axis: the four edge directions folded onto edge 0
			FV2 Acc = FV2::ZeroVector;
			for (int32 e = 0; e < 4; ++e) { Acc += Rot(E[e].D, -90.f * e); }
			RoomYaw = YawOf(Acc);

			for (int32 e = 0; e < 4; ++e)
			{
				FEdge& Ed = E[e];
				const float NY = YawOf(Ed.N);
				if (!Ed.bBack || Ed.Kind == EKind::Open) { Ed.Bands.Add({ -KTall, KTall, 86.f, KTall, 100.f }); } // keep the view in clear (desk height is fine)
				else if (Ed.Kind == EKind::Partition) { Ed.Bands.Add({ -KTall, KTall, 150.f, KTall, 75.f }); }
				if (Ed.Kind == EKind::Partition)
				{
					FObb Door; Door.C = Ed.At(Ed.L * 0.5f, 60.f); Door.H = FV2(60.f, KDoorW * 0.5f + 14.f); Door.Yaw = NY;
					Keep.Add(Door);
					Caps.Add({ Ed.At(Ed.L * 0.5f, 70.f), Hub, 42.f });
				}
				if (Ed.bExit)
				{
					for (float T : { 0.3f, 0.5f, 0.7f })
					{
						FObb X; X.C = Ed.At(Ed.L * T, Ed.Inset + 40.f); X.H = FV2(42.f, 40.f); X.Yaw = NY;
						Keep.Add(X);
					}
					Caps.Add({ Ed.At(Ed.L * 0.5f, Ed.Inset + 70.f), Hub, 40.f });
				}
			}
			FObb HubBox; HubBox.C = Hub; HubBox.H = FV2(48.f, 48.f); HubBox.Yaw = RoomYaw;
			HubKeepIdx = Keep.Add(HubBox);
		}

		bool Inside(const FV2& P, float Margin) const
		{
			for (int32 e = 0; e < 4; ++e)
			{
				const FV2 D = (In[(e + 1) & 3] - In[e]).GetSafeNormal();
				if (Cross(D, P - In[e]) < Margin) { return false; }
			}
			return true;
		}

		bool PartOk(const FPart& P, float Pad) const { return WhyNot(P, Pad) == nullptr; }
		/** nullptr if the part fits, else what stops it (for -DCFurnishDebug). */
		const TCHAR* WhyNot(const FPart& P, float Pad) const
		{
			FV2 C[4];
			P.Box.Corners(C);
			for (int32 k = 0; k < 4; ++k) { if (!Inside(C[k], P.bRug ? 6.f : 0.5f)) { return TEXT("outside"); } }
			if (!P.bRug)
			{
				for (int32 e = 0; e < 4; ++e)
				{
					const FEdge& Ed = E[e];
					float T0 = 1e9f, T1 = -1e9f, D0 = 1e9f;
					for (int32 k = 0; k < 4; ++k) { const float T = Ed.Proj(C[k]); T0 = FMath::Min(T0, T); T1 = FMath::Max(T1, T); D0 = FMath::Min(D0, Ed.Depth(C[k])); }
					for (const FBand& B : Ed.Bands)
					{
						if (bSoftWindows && B.Win >= 0) { continue; }
						if (P.Z1 <= B.Z0 || P.Z0 >= B.Z1 || D0 > B.Reach || T1 < B.T0 || T0 > B.T1) { continue; }
						return B.Win >= 0 ? TEXT("window") : B.Z0 < 100.f ? TEXT("front band") : TEXT("height band");
					}
				}
				if (P.Z0 < 120.f)
				{
					for (int32 k = 0; k < Keep.Num(); ++k) { if ((bHubKeep || k != HubKeepIdx) && Overlap(P.Box, Keep[k], 0.f)) { return k == HubKeepIdx ? TEXT("hub") : TEXT("door/exit"); } }
					for (const FCap& Cp : Caps)
					{
						const float Len = FV2::Distance(Cp.A, Cp.B);
						const int32 N = FMath::Max(1, FMath::CeilToInt(Len / 15.f));
						for (int32 i = 0; i <= N; ++i) { if (P.Box.DistTo(FMath::Lerp(Cp.A, Cp.B, (float)i / N)) < Cp.R * CapScale) { return TEXT("lane"); } }
					}
				}
			}
			for (const FPart& O : Parts)
			{
				if (O.bRug != P.bRug || P.Z1 <= O.Z0 || P.Z0 >= O.Z1 || (P.Tag == TagSeat && O.Tag == TagSeat)) { continue; }
				if (Overlap(P.Box, O.Box, P.bRug ? 10.f : Pad))
				{
					if (!GDebug) { return TEXT("furniture"); }
					static FString Buf[64]; static int32 BI = 0;
					FString& S = Buf[++BI & 63]; S = FString::Printf(TEXT("furniture#%d"), O.Owner);
					return *S;
				}
			}
			return nullptr;
		}

		FPart Realise(const FLocal& L, const FFrame& F) const
		{
			FPart P;
			P.Box.C = F.O + Rot(L.C, F.Yaw); P.Box.H = L.H; P.Box.Yaw = F.Yaw;
			P.Z0 = L.Z0; P.Z1 = L.Z1; P.bRug = L.bRug; P.bSolid = L.bSolid; P.Tag = L.Tag;
			return P;
		}
		bool Fits(const TArray<FLocal>& Ls, const FFrame& F, float Pad) const
		{
			for (const FLocal& L : Ls) { if (!PartOk(Realise(L, F), Pad)) { return false; } }
			return true;
		}
		int32 NextOwner = 0; // which group a part belongs to (debug)
		void Commit(const TArray<FLocal>& Ls, const FFrame& F) { for (const FLocal& L : Ls) { FPart P = Realise(L, F); P.Owner = NextOwner; Parts.Add(P); } ++NextOwner; }

		/** Window bands (edge, band index) the group would stand in front of. */
		void WindowsHit(const TArray<FLocal>& Ls, const FFrame& F, TArray<FIntPoint>& Out) const
		{
			Out.Reset();
			for (const FLocal& L : Ls)
			{
				if (L.bRug) { continue; }
				const FPart P = Realise(L, F);
				FV2 C[4];
				P.Box.Corners(C);
				for (int32 e = 0; e < 4; ++e)
				{
					const FEdge& Ed = E[e];
					float T0 = 1e9f, T1 = -1e9f, D0 = 1e9f;
					for (int32 k = 0; k < 4; ++k) { const float T = Ed.Proj(C[k]); T0 = FMath::Min(T0, T); T1 = FMath::Max(T1, T); D0 = FMath::Min(D0, Ed.Depth(C[k])); }
					for (int32 b = 0; b < Ed.Bands.Num(); ++b)
					{
						const FBand& B = Ed.Bands[b];
						if (B.Win < 0 || P.Z1 <= B.Z0 || P.Z0 >= B.Z1 || D0 > B.Reach || T1 < B.T0 || T0 > B.T1) { continue; }
						Out.AddUnique(FIntPoint(e, b));
					}
				}
			}
		}
		/** The group stands against those windows: the wall gets built without them. */
		void Brickup(const TArray<FIntPoint>& Hit, uint8 Omit[4])
		{
			TArray<FIntPoint> Sorted = Hit;
			Sorted.Sort([](const FIntPoint& A, const FIntPoint& B) { return A.X != B.X ? A.X < B.X : A.Y > B.Y; });
			for (const FIntPoint& H : Sorted)
			{
				FEdge& Ed = E[H.X];
				const int32 W = Ed.Bands[H.Y].Win;
				Omit[H.X] |= (uint8)(1 << W);
				for (int32 i = 0; i < Ed.Windows.Num(); ++i) { if (Ed.Windows[i].Idx == W) { Ed.Windows.RemoveAt(i); break; } }
				Ed.Bands.RemoveAt(H.Y);
				--WindowsLeft;
			}
		}

		/** Frames sliding along the inner face of every edge whose kind is in KindMask (bit per EKind). */
		void WallFrames(uint8 KindMask, bool bBackOnly, float Step, TArray<FFrame>& Out) const
		{
			for (int32 e = 0; e < 4; ++e)
			{
				const FEdge& Ed = E[e];
				if (!(KindMask & (1 << (uint8)Ed.Kind)) || (bBackOnly && !Ed.bBack)) { continue; }
				for (float T = 10.f; T <= Ed.L - 10.f; T += Step)
				{
					FFrame F; F.O = Ed.At(T, Ed.Inset + 1.f); F.Yaw = YawOf(Ed.N); F.Edge = e; F.T = T; F.bValid = true;
					Out.Add(F);
				}
			}
		}
		void FreeFrames(float Step, TArray<FFrame>& Out) const
		{
			for (float X = InnerBB.Min.X + 20.f; X < InnerBB.Max.X - 20.f; X += Step)
			{
				for (float Y = InnerBB.Min.Y + 20.f; Y < InnerBB.Max.Y - 20.f; Y += Step)
				{
					if (!Inside(FV2(X, Y), 30.f)) { continue; }
					FFrame F; F.O = FV2(X, Y); F.Yaw = RoomYaw; F.bValid = true;
					Out.Add(F);
				}
			}
		}
		FFrame Best(const TArray<FLocal>& Ls, const TArray<FFrame>& Frames, TFunctionRef<float(const FFrame&)> Score, float Pad, const TCHAR* Name = nullptr) const
		{
			FFrame Win; float BestS = -1e8f;
			TMap<FString, int32> Why;
			for (const FFrame& F : Frames)
			{
				const float S0 = Score(F);
				if (S0 < -1e7f) { if (Name && GDebug) { Why.FindOrAdd(TEXT("score"))++; } continue; }
				if (S0 <= BestS && !(Name && GDebug)) { continue; }
				const TCHAR* No = nullptr;
				for (const FLocal& L : Ls) { No = WhyNot(Realise(L, F), Pad); if (No) { break; } }
				if (No) { if (Name && GDebug) { Why.FindOrAdd(No)++; } continue; }
				if (S0 > BestS) { BestS = S0; Win = F; }
			}
			if (Name && GDebug)
			{
				FString W; for (const auto& K : Why) { W += FString::Printf(TEXT("%s %d, "), *K.Key, K.Value); }
				UE_LOG(LogDC, Log, TEXT("[Furnish] %s: %s of %d frames. Rejected: %s"), Name, Win.bValid ? TEXT("placed") : TEXT("NONE"), Frames.Num(), *W);
			}
			return Win;
		}

		float CornerDist(const FV2& P) const { float D = 1e9f; for (int32 k = 0; k < 4; ++k) { D = FMath::Min(D, FV2::Distance(P, In[k])); } return D; }
		float NearestSolid(const FV2& P) const { float D = 1e9f; for (const FPart& O : Parts) { if (O.bSolid) { D = FMath::Min(D, O.Box.DistTo(P)); } } return D; }
		bool InWindow(int32 Edge, float T, float HalfW) const
		{
			if (Edge < 0) { return false; }
			for (const FWin& W : E[Edge].Windows) { if (FMath::Abs(T - W.C) < W.W * 0.5f + HalfW * 0.3f) { return true; } }
			return false;
		}
		float Coverage() const
		{
			float A = 0.f;
			for (const FPart& O : Parts) { if (!O.bRug && O.Z0 < 50.f) { A += O.Box.H.X * O.Box.H.Y * 4.f; } }
			const float Room = FMath::Abs(Cross(In[2] - In[0], In[3] - In[1])) * 0.5f;
			return A / FMath::Max(Room, 1.f);
		}
	};

	/** Draws primitives in a local frame (+X forward, Z up from the floor). */
	using FEmitFn = TFunction<void(EDCShape, const FVector&, const FVector&, FName, const FRotator&, bool)>;
	struct FPen
	{
		const FEmitFn* Emit = nullptr;
		FV2 O = FV2::ZeroVector;
		float Yaw = 0.f;
		float Z = 0.f;
		FVector P(float X, float Y, float Zl) const { const FV2 W = O + Rot(FV2(X, Y), Yaw); return FVector(W.X, W.Y, Z + Zl); }
		FRotator R(float Pitch, float YawOff, float Roll) const { return (FRotator(0.f, Yaw, 0.f).Quaternion() * FRotator(Pitch, YawOff, Roll).Quaternion()).Rotator(); }
		void S(EDCShape Sh, float X, float Y, float Zc, float SX, float SY, float SZ, FName M, float YawOff = 0.f, float Pitch = 0.f, float Roll = 0.f, bool bSh = true) const
		{
			(*Emit)(Sh, P(X, Y, Zc), FVector(SX, SY, SZ), M, R(Pitch, YawOff, Roll), bSh);
		}
		void Box(float X, float Y, float Zc, float SX, float SY, float SZ, FName M, float YawOff = 0.f, float Pitch = 0.f, float Roll = 0.f, bool bSh = true) const { S(EDCShape::Cube, X, Y, Zc, SX, SY, SZ, M, YawOff, Pitch, Roll, bSh); }
		void Cyl(float X, float Y, float Zc, float Dia, float H, FName M, float Pitch = 0.f, float YawOff = 0.f, bool bSh = true) const { S(EDCShape::Cylinder, X, Y, Zc, Dia, Dia, H, M, YawOff, Pitch, 0.f, bSh); }
		void Ball(float X, float Y, float Zc, float SX, float SY, float SZ, FName M, bool bSh = true) const { S(EDCShape::Sphere, X, Y, Zc, SX, SY, SZ, M, 0.f, 0.f, 0.f, bSh); }
		void Rod(const FVector& LA, const FVector& LB, float Dia, FName M, bool bSh = true) const
		{
			const FVector A = P(LA.X, LA.Y, LA.Z), B = P(LB.X, LB.Y, LB.Z);
			const FVector D = B - A;
			const float Len = D.Size();
			if (Len < 0.1f) { return; }
			(*Emit)(EDCShape::Cylinder, (A + B) * 0.5f, FVector(Dia, Dia, Len), M, FRotationMatrix::MakeFromZ(D / Len).Rotator(), bSh);
		}
		FPen At(float X, float Y, float YawOff = 0.f, float Zl = 0.f) const { FPen N = *this; N.O = O + Rot(FV2(X, Y), Yaw); N.Yaw = Yaw + YawOff; N.Z = Z + Zl; return N; }
	};

	inline float J(int32 Seed, int32 K) { return DCSite::Grid::Hash01(Seed, K, 57) * 2.f - 1.f; }
	inline float U(int32 Seed, int32 K) { return DCSite::Grid::Hash01(Seed, K, 57); }

	// ------------------------------------------------------------------------------------------------- recipes
	/** Draws one placed group. The pen sits at the group's frame; Seed varies per placement. Runs after the recipe
	 *  has returned, so capture by value. */
	using FDrawFn = TFunction<void(const FPen&, int32 /*Seed*/)>;
	struct FPlaced { const TCHAR* Name = TEXT("?"); FFrame F; FName Group; FDrawFn Draw; };

	constexpr uint8 MWall = 1 << (uint8)EKind::Wall, MPart = 1 << (uint8)EKind::Partition, MAny = 0x7;

	/** What a room recipe works with: the solved room, candidate frames and the placing helpers. A recipe places
	 *  its groups in priority order (the room's anchor piece first, fillers last); FurnishGridRoom then draws them,
	 *  lights the room and hands footprints + use spots to the room. */
	struct FCtx
	{
		FPlan Plan;
		ADCRoomModule* Room = nullptr;
		int32 Seed = 0;
		float H = 300.f;      // wall height
		float LampZ = 220.f;  // height of the room's bracket lamp
		bool bDebug = false;  // -DCFurnishDebug
		uint8* Omit = nullptr;
		TArray<FPlaced> Out;
		TArray<ADCRoomModule::FUseSpot> Spots;
		/** Candidate frames (+X into the room): along tall walls; along tall walls and partitions at the back;
		 *  along every edge; a free grid over the floor (yaw = the room's axis). */
		TArray<FFrame> WallF, WallPartF, AnyWallF, FreeF;
		int32 LampEdge = -1;
		float LampT = 0.f;
		TArray<FIntPoint> Hit;

		float Jit(int32 K) const { return J(Seed, 500 + K); }
		FV2 Fwd(const FFrame& F) const { return Dir(F.Yaw); }
		float FacingHub(const FFrame& F) const { return FV2::DotProduct(Dir(F.Yaw), (Plan.Hub - F.O).GetSafeNormal()); }
		float FacingCam(const FFrame& F) const { return FV2::DotProduct(Dir(F.Yaw), KToCam); }

		/** A place where crew look busy: (X, Y) in F's frame, facing back towards the frame origin. */
		void Spot(const FFrame& F, float X, float Y);
		/** Claims the group's boxes and queues its drawing. Group: DCGroups::Level2 / Level3 for upgrade pieces
		 *  (default: always there). bCentreOnFirst: draw around the first box's centre (rugs). */
		void Place(const TCHAR* Name, const TArray<FLocal>& Ls, const FFrame& F, FDrawFn Draw, FName Group = NAME_None, bool bCentreOnFirst = false);
		/** Best frame for tall wall furniture that may stand against a window (at Penalty per window, and never the
		 *  room's last one). Pair with PlaceSoft, which has the wall built without those windows. */
		FFrame BestSoft(const TArray<FLocal>& Ls, const TArray<FFrame>& Frames, TFunctionRef<float(const FFrame&)> Score, float Pad, float Penalty);
		void PlaceSoft(const TCHAR* Name, const TArray<FLocal>& Ls, const FFrame& F, FDrawFn Draw, FName Group = NAME_None);
		/** A rug (LX deep, LY wide, centred at (CX, 0) in F's frame) if it fits, shrinking through Scales. */
		bool PlaceRug(const FFrame& F, float CX, float LX, float LY, FName Field, std::initializer_list<float> Scales = { 1.f, 0.85f, 0.7f });

		/** The room light: a bracket lamp on Edge at T (nothing tall goes under it). Call before placing tall
		 *  furniture. Without a lamp the room gets a bare light over its middle. */
		void SetLamp(int32 Edge, float T);
		/** A bracket lamp on a standing back wall, if there is one. */
		void DefaultLamp();

		// shared steps, usable by any room
		/** A runner from the main way in to the middle of the room. */
		void Runner();
		/** Crates, bags, a plant, cans, papers: gathered in corners and beside furniture until the floor is
		 *  MaxCoverage full. */
		void Clutter(float MaxCoverage = 0.42f);
		/** Pictures, a rack, a clock, a shelf: on tall walls, best above low furniture. */
		void WallDecor();
	};

	// ------------------------------------------------------------------------------------------------- props
	// Each prop draws around its frame origin (on the floor). Wall props: origin on the wall's inner face.
	void DrawWoodChair(const FPen& P, int32 Seed);
	void DrawCaptainChair(const FPen& P);
	void DrawArmchair(const FPen& P, FName Mat, int32 Seed);
	void DrawSofa(const FPen& P, float W, FName Mat, int32 Seed);
	void DrawCoffeeTable(const FPen& P, float W, float D, int32 Seed);
	void DrawRug(const FPen& P, float LX, float LY, FName Field, FName Border);
	void DrawLaptop(const FPen& P, float Zt, FName Screen);
	void DrawPlanningTable(const FPen& P, float W, float D, int32 Seed);
	void DrawBoardFace(const FPen& P, float W, float CentreZ, float H, int32 Seed);
	void DrawBoard(const FPen& P, float W, float Top, int32 Seed);
	void DrawEaselBoard(const FPen& P, float W, int32 Seed);
	void DrawDesk(const FPen& P, float W, float D, int32 Seed);
	void DrawStove(const FPen& P, float FlueTop, int32 Seed);
	void DrawLogBasket(const FPen& P, int32 Seed);
	void DrawBookcase(const FPen& P, float W, int32 Seed);
	void DrawSideboard(const FPen& P, float W, int32 Seed);
	void DrawFilingCabinets(const FPen& P, int32 N, int32 Seed);
	void DrawSafe(const FPen& P);
	void DrawFloorLamp(const FPen& P);
	void DrawCrates(const FPen& P, int32 N, int32 Seed);
	void DrawDuffels(const FPen& P, int32 Seed);
	void DrawPlant(const FPen& P, int32 Seed);
	void DrawJerrycans(const FPen& P, int32 Seed);
	void DrawNewspapers(const FPen& P, int32 Seed);
	void DrawPictures(const FPen& P, float W, int32 Seed);
	void DrawClock(const FPen& P);
	void DrawShotgunRack(const FPen& P);
	void DrawWallShelf(const FPen& P, float W, int32 Seed);
	void DrawWallScreen(const FPen& P);
}

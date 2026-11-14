// Director's Cut — room furnishing on the build grid. A small layout solver fits furniture to the room's real
// (jittered) outline and to its connections, then draws it from primitives. Only the Office is furnished so far.
//
// How a layout is found (all room-local, cm):
//  1. Every edge is classified: tall wall (its windows and broken-down ends come from TallEdgePlan), low partition
//     with a centred doorway, or collapsed open front. Edges add height BANDS: nothing tall near an open front (the
//     camera looks in over it), in front of a window or a broken-down stretch of wall, or under the wall lamp.
//  2. Circulation is reserved: a clear box at each doorway and way out, clear lanes from each of them to the room's
//     hub, and the hub itself, so crew still walk where they always did.
//  3. Furniture groups go in by priority (mission board first, small fillers last). A group is a few oriented boxes,
//     each with a Z range (a rug, a wall-hung picture and a sofa can share floor), in a local frame whose +X faces
//     into the room. Candidate frames slide along every eligible wall, sweep a free grid, or hang off something
//     already placed (the table in front of the board, the lamp beside the sofa). Every candidate that fits is scored
//     (centred on its wall, tucked into a corner, facing the room, beside related furniture, ...) and the best wins.
//     A group that can't fit falls back to a smaller or freestanding variant, or is left out.
//  4. The winners are drawn, their solid footprints go to the room (pawns avoid them, and walks that would cut
//     through furniture detour via the room's middle) and their use spots become places where crew look busy.
//
// Tall wall furniture may stand against a window; the wall is then built without it (so the furnisher runs before
// the walls). -DCFurnishDebug logs why each key piece was rejected where it was.
#include "World/Scene/DCHideoutBuilder.h"
#include "World/DCRoomModule.h"
#include "Core/DCSiteLayout.h"
#include "Core/DCWorldTags.h"
#include "DirectorsCut.h"
#include "Components/PointLightComponent.h"

namespace DCFurnish
{
	using FV2 = FVector2D;

	constexpr float KInsetWall = 30.f;   // inner face of a tall wall (masonry 24 + skin)
	constexpr float KInsetPart = 14.f;   // a partition stub is centred on the edge
	constexpr float KInsetOpen = 30.f;   // collapsed front: stub + foundation lip
	constexpr float KDoorW = 150.f;      // must match DCHideoutGrid.cpp
	constexpr float KTall = 1.0e4f;
	const FV2 KToCam = FV2(-1.f, -1.f).GetSafeNormal(); // the iso camera looks from -X-Y

	FV2 Dir(float YawDeg) { const float R = FMath::DegreesToRadians(YawDeg); return FV2(FMath::Cos(R), FMath::Sin(R)); }
	float YawOf(const FV2& D) { return FMath::RadiansToDegrees(FMath::Atan2(D.Y, D.X)); }
	FV2 Rot(const FV2& V, float YawDeg) { const FV2 X = Dir(YawDeg); return FV2(V.X * X.X - V.Y * X.Y, V.X * X.Y + V.Y * X.X); }
	float Cross(const FV2& A, const FV2& B) { return A.X * B.Y - A.Y * B.X; }
	FV2 LineX(const FV2& P0, const FV2& D0, const FV2& P1, const FV2& D1)
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
	bool Overlap(const FObb& A, const FObb& B, float Gap)
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
	bool GDebug = false;
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
			for (int32 k = 0; k < 4; ++k) { if (!Inside(C[k], P.bRug ? 6.f : 1.f)) { return TEXT("outside"); } }
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

	float J(int32 Seed, int32 K) { return DCSite::Grid::Hash01(Seed, K, 57) * 2.f - 1.f; }
	float U(int32 Seed, int32 K) { return DCSite::Grid::Hash01(Seed, K, 57); }

	// ------------------------------------------------------------------------------------------------- props
	// Each prop draws around its frame origin (on the floor). Wall props: origin on the wall's inner face.

	void DrawWoodChair(const FPen& P, int32 Seed)
	{
		for (int32 i = 0; i < 4; ++i)
		{
			const float X = (i % 2) ? 18.f : -18.f, Y = (i / 2) ? 18.f : -18.f;
			P.Box(X, Y, 23.f, 4.f, 4.f, 46.f, "TimberDark");
		}
		P.Box(0.f, -18.f, 13.f, 36.f, 2.5f, 2.5f, "TimberDark");
		P.Box(0.f, 18.f, 13.f, 36.f, 2.5f, 2.5f, "TimberDark");
		P.Box(1.f, 0.f, 47.5f, 46.f, 45.f, 4.f, "Timber");
		for (float Y : { -18.f, 18.f }) { P.Box(-20.f, Y, 72.f, 4.5f, 4.5f, 48.f, "TimberDark", 0.f, -5.f); }
		P.Box(-21.5f, 0.f, 90.f, 3.5f, 40.f, 9.f, "Timber", 0.f, -5.f);
		P.Box(-20.5f, 0.f, 74.f, 2.5f, 36.f, 5.f, "Timber", 0.f, -5.f);
		if (U(Seed, 1) > 0.6f) { P.Box(2.f, 0.f, 50.5f, 38.f, 38.f, 2.f, "Blanket", J(Seed, 2) * 10.f); } // a folded cushion
	}

	void DrawCaptainChair(const FPen& P)
	{
		for (int32 i = 0; i < 4; ++i) { P.Box(0.f, 0.f, 3.f, 52.f, 5.f, 3.f, "Black", 45.f + i * 90.f); }
		P.Cyl(0.f, 0.f, 22.f, 7.f, 36.f, "Black");
		P.Box(0.f, 0.f, 45.f, 50.f, 52.f, 10.f, "LeatherBrn");
		P.Box(2.f, 0.f, 51.f, 42.f, 44.f, 4.f, "LeatherBrn");
		P.Box(-25.f, 0.f, 76.f, 10.f, 52.f, 54.f, "LeatherBrn", 0.f, -9.f);
		for (float Y : { -27.f, 27.f })
		{
			P.Box(-2.f, Y, 63.f, 44.f, 6.f, 5.f, "Walnut");
			P.Box(12.f, Y, 56.f, 4.f, 4.f, 14.f, "Walnut");
		}
	}

	void DrawArmchair(const FPen& P, FName Mat, int32 Seed)
	{
		for (int32 i = 0; i < 4; ++i) { P.Box((i % 2) ? 32.f : -32.f, (i / 2) ? 34.f : -34.f, 3.f, 6.f, 6.f, 6.f, "Walnut"); }
		P.Box(2.f, 0.f, 22.f, 76.f, 80.f, 26.f, Mat);
		P.Box(10.f, 0.f, 41.f, 60.f, 54.f, 12.f, Mat, J(Seed, 1) * 2.f);
		P.Box(-30.f, 0.f, 57.f, 20.f, 80.f, 48.f, Mat, 0.f, -8.f);
		P.Box(-29.f, 0.f, 81.f, 17.f, 78.f, 8.f, Mat, 0.f, -8.f);
		for (float Y : { -34.f, 34.f }) { P.Box(2.f, Y, 47.f, 76.f, 13.f, 26.f, Mat); P.Box(4.f, Y, 61.f, 72.f, 15.f, 4.f, Mat); }
		if (U(Seed, 2) > 0.4f) { P.Box(-12.f, J(Seed, 3) * 12.f, 60.f, 12.f, 34.f, 30.f, "Linen", J(Seed, 4) * 25.f, -22.f); }
	}

	void DrawSofa(const FPen& P, float W, FName Mat, int32 Seed)
	{
		const float HW = W * 0.5f;
		for (float X : { 10.f, 80.f }) { for (float Y : { -HW + 8.f, HW - 8.f }) { P.Box(X, Y, 3.f, 6.f, 6.f, 6.f, "Walnut"); } }
		P.Box(46.f, 0.f, 22.f, 80.f, W, 26.f, Mat);
		P.Box(14.f, 0.f, 57.f, 22.f, W - 6.f, 52.f, Mat, 0.f, -6.f);
		for (float S : { -1.f, 1.f })
		{
			P.Box(46.f, S * (HW - 11.f), 47.f, 82.f, 22.f, 30.f, Mat);
			P.Box(48.f, S * (HW - 11.f), 63.f, 78.f, 24.f, 5.f, Mat);
		}
		const int32 N = W > 180.f ? 3 : 2;
		const float CW = (W - 44.f) / N;
		for (int32 i = 0; i < N; ++i)
		{
			const float Y = -HW + 22.f + CW * (i + 0.5f);
			P.Box(52.f + J(Seed, 10 + i) * 2.f, Y, 41.5f + U(Seed, 20 + i) * 1.5f, 62.f, CW - 2.f, 13.f, Mat, J(Seed, 30 + i) * 2.f);
			P.Box(30.f, Y, 66.f, 15.f, CW - 4.f, 34.f, Mat, J(Seed, 40 + i) * 3.f, -14.f);
		}
		const float Side = U(Seed, 1) > 0.5f ? 1.f : -1.f;
		// a throw blanket over one arm, and a scatter cushion in the other corner
		P.Box(48.f, Side * (HW - 11.f), 66.5f, 56.f, 28.f, 2.f, "Blanket", J(Seed, 2) * 6.f);
		P.Box(48.f, Side * (HW + 0.8f), 54.f, 54.f, 2.f, 26.f, "Blanket", J(Seed, 2) * 6.f);
		P.Box(40.f, -Side * (HW - 34.f), 62.f, 12.f, 38.f, 34.f, "Linen", -Side * 18.f, -18.f);
	}

	void DrawCoffeeTable(const FPen& P, float W, float D, int32 Seed)
	{
		P.Box(0.f, 0.f, 40.f, D, W, 4.f, "Walnut");
		P.Box(0.f, 0.f, 12.f, D - 10.f, W - 10.f, 2.f, "Walnut");
		for (int32 i = 0; i < 4; ++i) { P.Box((i % 2) ? D * 0.5f - 4.f : -D * 0.5f + 4.f, (i / 2) ? W * 0.5f - 4.f : -W * 0.5f + 4.f, 20.f, 5.f, 5.f, 38.f, "Walnut"); }
		// magazines, mugs, an ashtray, a candle in a bottle, cards
		P.Box(-6.f, -W * 0.22f, 42.6f, 26.f, 20.f, 1.f, "Paper", 12.f + J(Seed, 1) * 8.f);
		P.Box(-4.f, -W * 0.24f, 43.4f, 26.f, 20.f, 0.6f, "Photo", -6.f + J(Seed, 2) * 8.f);
		P.Cyl(10.f, W * 0.05f, 46.5f, 8.f, 9.f, "Enamel");
		P.Cyl(-12.f, W * 0.12f, 46.5f, 8.f, 9.f, "Enamel");
		P.Cyl(6.f, W * 0.3f, 43.f, 13.f, 2.f, "Glass");
		P.Cyl(-8.f, W * 0.34f, 53.f, 7.f, 22.f, "Bottle");
		P.Cyl(-8.f, W * 0.34f, 66.f, 2.5f, 5.f, "Paper");
		P.Ball(-8.f, W * 0.34f, 70.f, 1.6f, 1.6f, 3.f, "Candle", false);
		for (int32 i = 0; i < 4; ++i) { P.Box(14.f + J(Seed, 5 + i) * 6.f, -W * 0.02f + J(Seed, 9 + i) * 10.f, 42.3f, 9.f, 6.f, 0.4f, "Paper", J(Seed, 13 + i) * 60.f); }
		P.Box(-12.f, -W * 0.05f, 12.f + 3.5f, 24.f, 30.f, 5.f, "Paper", J(Seed, 20) * 10.f); // newspapers on the shelf
	}

	void DrawRug(const FPen& P, float LX, float LY, FName Field, FName Border)
	{
		P.Box(0.f, 0.f, 0.6f, LX, LY, 1.2f, Border, 0.f, 0.f, 0.f, false);
		P.Box(0.f, 0.f, 1.3f, LX - 16.f, LY - 16.f, 0.4f, Field, 0.f, 0.f, 0.f, false);
		if (LX < LY * 2.2f) { P.Box(0.f, 0.f, 1.6f, LX * 0.38f, LY * 0.3f, 0.3f, Border, 45.f, 0.f, 0.f, false); } // centre medallion (not on runners)
		else { for (float S : { -1.f, 1.f }) { P.Box(0.f, S * (LY * 0.5f - 14.f), 1.6f, LX - 30.f, 3.f, 0.3f, Border, 0.f, 0.f, 0.f, false); } } // runner stripes
		for (float S : { -1.f, 1.f }) { P.Box(S * (LX * 0.5f + 2.5f), 0.f, 0.4f, 5.f, LY - 8.f, 0.6f, "Linen", 0.f, 0.f, 0.f, false); } // fringes
	}

	void DrawLaptop(const FPen& P, float Zt, FName Screen)
	{
		P.Box(0.f, 0.f, Zt + 0.8f, 24.f, 34.f, 1.6f, "Alu");
		P.Box(0.6f, 0.f, Zt + 1.7f, 12.f, 30.f, 0.2f, "PlasticBlack", 0.f, 0.f, 0.f, false);
		P.Box(-12.5f + 2.6f, 0.f, Zt + 11.2f, 1.4f, 34.f, 22.f, "Alu", 0.f, 14.f);
		P.Box(-11.4f + 2.6f, 0.f, Zt + 11.4f, 0.3f, 31.f, 19.f, Screen, 0.f, 14.f, 0.f, false);
	}

	void DrawPlanningTable(const FPen& P, float W, float D, int32 Seed)
	{
		P.Box(0.f, 0.f, 78.f, D, W, 5.f, "Timber");
		P.Box(0.f, 0.f, 71.f, D - 12.f, W - 16.f, 9.f, "TimberDark");
		for (int32 i = 0; i < 4; ++i) { P.Box((i % 2) ? D * 0.5f - 7.f : -D * 0.5f + 7.f, (i / 2) ? W * 0.5f - 8.f : -W * 0.5f + 8.f, 35.f, 7.5f, 7.5f, 70.f, "TimberDark"); }
		P.Box(0.f, 0.f, 13.f, 5.f, W - 22.f, 5.f, "TimberDark");
		// the job: a map spread out with a route drawn on, plans, photos of the target
		P.Box(2.f, -W * 0.1f, 80.8f, D * 0.66f, W * 0.44f, 0.5f, "Paper", 3.f + J(Seed, 1) * 3.f);
		for (int32 i = 0; i < 4; ++i)
		{
			const FV2 A(J(Seed, 10 + i) * D * 0.25f, -W * 0.1f + J(Seed, 20 + i) * W * 0.18f);
			const FV2 B(J(Seed, 11 + i) * D * 0.25f, -W * 0.1f + J(Seed, 21 + i) * W * 0.18f);
			P.Rod(FVector(A.X, A.Y, 81.2f), FVector(B.X, B.Y, 81.2f), 0.7f, "Pin", false);
		}
		P.Box(-D * 0.12f, W * 0.28f, 80.9f, D * 0.5f, W * 0.24f, 0.4f, "Blueprint", -7.f + J(Seed, 2) * 4.f);
		P.S(EDCShape::Cylinder, -D * 0.33f, W * 0.3f, 84.f, 7.f, 7.f, W * 0.3f, "Blueprint", 90.f, 90.f, 0.f);
		P.S(EDCShape::Cylinder, -D * 0.3f, W * 0.18f, 84.f, 6.f, 6.f, W * 0.24f, "Paper", 98.f, 90.f, 0.f);
		for (int32 i = 0; i < 5; ++i) { P.Box(D * 0.28f + J(Seed, 30 + i) * 6.f, -W * 0.36f + i * 11.f, 80.9f + i * 0.15f, 11.f, 14.f, 0.3f, "Photo", J(Seed, 40 + i) * 18.f); }
		P.Cyl(D * 0.3f, W * 0.18f, 85.f, 8.f, 9.f, "Enamel");
		P.Cyl(-D * 0.28f, -W * 0.38f, 85.f, 8.f, 9.f, "Enamel");
		DrawLaptop(P.At(D * 0.18f, W * 0.36f, 180.f + J(Seed, 3) * 15.f), 80.5f, "Screen1");
		// walkie-talkies and a box of shells
		P.Box(-D * 0.3f, -W * 0.02f, 85.f, 6.f, 4.f, 14.f, "PlasticBlack", 20.f);
		P.Rod(FVector(-D * 0.3f, -W * 0.02f, 92.f), FVector(-D * 0.3f, -W * 0.02f, 104.f), 0.8f, "PlasticBlack");
		P.Box(-D * 0.22f, -W * 0.06f, 81.5f, 13.f, 6.f, 3.f, "PlasticBlack", -35.f);
		P.Box(D * 0.05f, W * 0.08f, 83.f, 10.f, 14.f, 5.f, "PaintRed", 12.f);
	}

	/** Cork board content: map, photos, cards, pins and string. Origin on the board's face plane, CentreZ = middle. */
	void DrawBoardFace(const FPen& P, float W, float CentreZ, float H, int32 Seed)
	{
		P.Box(1.5f, 0.f, CentreZ, 3.f, W, H, "Cork");
		P.Box(2.5f, 0.f, CentreZ + H * 0.5f + 2.5f, 5.f, W + 10.f, 5.f, "TimberDark");
		P.Box(2.5f, 0.f, CentreZ - H * 0.5f - 2.5f, 5.f, W + 10.f, 5.f, "TimberDark");
		for (float S : { -1.f, 1.f }) { P.Box(2.5f, S * (W * 0.5f + 2.5f), CentreZ, 5.f, 5.f, H + 10.f, "TimberDark"); }
		P.Box(3.3f, -W * 0.13f, CentreZ + 4.f, 0.5f, W * 0.44f, H * 0.62f, "Paper");
		P.Box(3.4f, W * 0.27f, CentreZ + H * 0.2f, 0.5f, W * 0.26f, H * 0.34f, "Blueprint");
		TArray<FV2> Pins;
		const int32 NPhotos = FMath::Clamp(FMath::RoundToInt(W / 28.f), 4, 9);
		for (int32 i = 0; i < NPhotos; ++i)
		{
			const float Y = (-0.42f + 0.84f * (i + 0.5f) / NPhotos) * W + J(Seed, 50 + i) * 8.f;
			const float Zp = CentreZ + ((i % 2) ? -1.f : 1.f) * H * (0.2f + 0.12f * U(Seed, 60 + i));
			const bool bCard = U(Seed, 70 + i) > 0.6f;
			P.Box(3.6f + i * 0.02f, Y, Zp, 0.4f, bCard ? 12.f : 14.f, bCard ? 8.f : 11.f, bCard ? FName("Paper") : FName("Photo"), 0.f, 0.f, J(Seed, 80 + i) * 9.f);
			Pins.Add(FV2(Y, Zp + 4.f));
			P.Ball(4.4f, Y, Zp + 4.f, 1.8f, 1.8f, 1.8f, "Pin", false);
		}
		for (int32 i = 0; i + 1 < Pins.Num(); i += (U(Seed, 90 + i) > 0.7f ? 2 : 1))
		{
			const FV2 A = Pins[i], B = Pins[(i + 1 + (i % 3 == 0 ? 1 : 0)) % Pins.Num()];
			P.Rod(FVector(4.2f, A.X, A.Y), FVector(4.2f, B.X, B.Y), 0.45f, "Pin", false);
		}
		// one photo circled: the target
		const int32 T = (int32)(U(Seed, 99) * NPhotos) % NPhotos;
		P.Box(3.5f, Pins[T].X, Pins[T].Y - 4.f, 0.3f, 19.f, 15.f, "PaintRed");
	}

	void DrawBoard(const FPen& P, float W, float Top, int32 Seed)
	{
		const float H = 112.f;
		DrawBoardFace(P, W, Top - H * 0.5f - 5.f, H, Seed);
		// a ledge with marker pens and a mug under it
		P.Box(5.f, 0.f, Top - H - 12.f, 9.f, W * 0.7f, 2.5f, "TimberDark");
		for (int32 i = 0; i < 3; ++i) { P.Box(5.f, -W * 0.2f + i * 6.f, Top - H - 10.f, 1.6f, 13.f, 1.6f, i == 1 ? FName("PaintRed") : FName("PlasticBlack")); }
	}

	void DrawEaselBoard(const FPen& P, float W, int32 Seed)
	{
		// a board on timber trestle legs, kept low (rooms with no tall wall: it mustn't hide the room behind).
		// Origin: the back of its footprint; the face looks along +X.
		for (float S : { -1.f, 1.f })
		{
			const float Y = S * (W * 0.5f - 6.f);
			P.Rod(FVector(50.f, Y, 0.f), FVector(36.f, Y, 146.f), 5.f, "Timber");
			P.Rod(FVector(3.f, Y, 0.f), FVector(33.f, Y, 143.f), 4.f, "TimberDark");
			P.Rod(FVector(46.f, Y, 34.f), FVector(10.f, Y, 34.f), 3.f, "TimberDark");
		}
		DrawBoardFace(P.At(37.f, 0.f), W, 98.f, 80.f, Seed);
		P.Box(44.f, 0.f, 55.f, 12.f, W * 0.8f, 2.5f, "Timber");
	}

	void DrawDesk(const FPen& P, float W, float D, int32 Seed)
	{
		// origin: desk centre. Sitter on -X, the room (and the camera) on +X. W 152 = partners' desk with two
		// pedestals; narrower = a writing desk with one pedestal and legs.
		const float HW = W * 0.5f, HD = D * 0.5f, Ky = W / 152.f;
		P.Box(0.f, 0.f, 76.f, D, W, 4.f, "Walnut");
		P.Box(-1.f, 0.f, 78.2f, D * 0.63f, W * 0.5f, 0.4f, "LeatherBrn", 0.f, 0.f, 0.f, false);
		const bool bTwo = W > 130.f;
		for (float S : { -1.f, 1.f })
		{
			if (!bTwo && S < 0.f)
			{
				for (float X : { -HD + 4.f, HD - 4.f }) { P.Box(X, -HW + 5.f, 37.f, 5.f, 5.f, 74.f, "Walnut"); }
				P.Box(-HD + 6.f, -HW + 22.f, 70.f, 2.f, 30.f, 8.f, "Wood");
				continue;
			}
			const float Y = S * (HW - 24.f);
			P.Box(0.f, Y, 37.f, D - 6.f, 44.f, 74.f, "Walnut");
			for (int32 k = 0; k < 3; ++k)
			{
				const float Z = 15.f + k * 23.f;
				P.Box(-HD + 2.5f, Y, Z, 1.2f, 38.f, 19.f, "Wood");
				P.Box(-HD + 1.4f, Y, Z + 3.f, 1.2f, 8.f, 2.f, "Brass");
			}
		}
		P.Box(HD - 4.f, bTwo ? 0.f : -12.f, 46.f, 3.f, bTwo ? W - 92.f : W - 60.f, 56.f, "Walnut");
		// banker's lamp
		const FPen L = P.At(HD * 0.37f, HW - 22.f, 0.f);
		L.Cyl(0.f, 0.f, 79.f, 14.f, 2.f, "Brass");
		L.Cyl(0.f, 0.f, 93.f, 2.f, 28.f, "Brass");
		L.Box(-4.f, 0.f, 108.f, 13.f, 30.f, 7.f, "PaintGreen", 0.f, -8.f);
		L.Box(-4.f, 0.f, 104.4f, 10.f, 26.f, 0.4f, "Tungsten", 0.f, -8.f, 0.f, false);
		DrawLaptop(P.At(-HD * 0.2f, -8.f * Ky, 180.f + J(Seed, 1) * 12.f), 78.f, "Screen0");
		// paperwork, a telephone, whisky, cash
		P.Box(HD * 0.1f, -HW * 0.58f, 79.5f, 22.f, 30.f, 3.f, "Paper", J(Seed, 2) * 10.f);
		if (bTwo) { P.Box(-14.f, 44.f, 79.f, 20.f, 26.f, 1.f, "Paper", J(Seed, 3) * 10.f); }
		P.Box(HD * 0.47f, -8.f * Ky, 81.f, 16.f, 20.f, 6.f, "PlasticBlack", 25.f);
		P.Box(HD * 0.47f, -8.f * Ky, 85.f, 6.f, 20.f, 3.f, "PlasticBlack", 25.f);
		P.Cyl(HD * 0.58f, -HW + 12.f, 90.f, 8.f, 24.f, "Bottle");
		P.Cyl(HD * 0.58f, -HW + 12.f, 104.f, 3.f, 5.f, "Bottle");
		P.Cyl(HD * 0.26f, -HW + 16.f, 82.f, 6.f, 8.f, "Glass");
		if (bTwo) { for (int32 i = 0; i < 4; ++i) { P.Box(-20.f + (i % 2) * 9.f, -60.f + (i / 2) * 17.f, 79.5f + (i % 2) * 2.5f, 7.5f, 16.f, 3.f + (i % 2) * 2.5f, "Cash", J(Seed, 10 + i) * 6.f); } }
	}

	void DrawStove(const FPen& P, float FlueTop, int32 Seed)
	{
		P.Box(44.f, 0.f, 2.f, 88.f, 100.f, 4.f, "StoneDark");
		P.Box(44.f, 0.f, 4.3f, 84.f, 96.f, 0.6f, "Stone", 0.f, 0.f, 0.f, false);
		for (int32 i = 0; i < 4; ++i) { P.Box((i % 2) ? 56.f : 16.f, (i / 2) ? 24.f : -24.f, 11.f, 6.f, 6.f, 14.f, "CastIron"); }
		P.Box(36.f, 0.f, 46.f, 46.f, 54.f, 52.f, "CastIron");
		P.Box(36.f, 0.f, 73.5f, 54.f, 62.f, 4.f, "CastIron");
		P.Box(36.f, 0.f, 18.5f, 50.f, 58.f, 3.f, "CastIron");
		P.Box(59.6f, 0.f, 45.f, 2.f, 34.f, 30.f, "CastIron");
		P.Box(60.8f, 0.f, 46.f, 0.6f, 24.f, 16.f, "Ember", 0.f, 0.f, 0.f, false);
		P.Box(61.4f, 15.f, 46.f, 2.f, 2.f, 10.f, "Brass");
		// the flue up and into the wall
		P.Cyl(36.f, 0.f, 78.f, 18.f, 6.f, "CastIron");
		P.Cyl(36.f, 0.f, (80.f + FlueTop) * 0.5f, 13.f, FlueTop - 80.f, "Black");
		P.Cyl(36.f, 0.f, 140.f, 15.f, 3.f, "CastIron");
		P.Rod(FVector(36.f, 0.f, FlueTop - 7.f), FVector(-6.f, 0.f, FlueTop + 6.f), 13.f, "Black");
		P.Ball(36.f, 0.f, FlueTop - 6.f, 14.f, 14.f, 14.f, "Black");
		// a kettle and a poker
		P.Cyl(30.f, 12.f, 83.f, 18.f, 14.f, "Enamel");
		P.Ball(30.f, 12.f, 90.f, 16.f, 16.f, 6.f, "Enamel");
		P.Rod(FVector(36.f, 12.f, 84.f), FVector(46.f, 12.f, 92.f), 2.f, "Enamel");
		P.Rod(FVector(62.f, -38.f, 2.f), FVector(52.f, -42.f, 70.f), 1.6f, "Black");
		(void)Seed;
	}

	void DrawLogBasket(const FPen& P, int32 Seed)
	{
		P.Cyl(0.f, 0.f, 16.f, 44.f, 32.f, "Canvas");
		P.Cyl(0.f, 0.f, 31.f, 46.f, 3.f, "TimberDark");
		for (int32 i = 0; i < 6; ++i)
		{
			const float A = i * 60.f + J(Seed, i) * 15.f;
			const FV2 O = Rot(FV2(9.f, 0.f), A);
			P.S(EDCShape::Cylinder, O.X, O.Y, 36.f + (i % 2) * 6.f, 9.f, 9.f, 44.f, (i % 3) ? FName("TimberDark") : FName("Wood"), A, 90.f, 0.f);
		}
	}

	void DrawBookcase(const FPen& P, float W, int32 Seed)
	{
		const float D = 34.f, X = D * 0.5f;
		P.Box(1.f, 0.f, 96.f, 2.f, W, 190.f, "Wood");
		for (float S : { -1.f, 1.f }) { P.Box(X, S * (W * 0.5f - 1.5f), 96.f, D, 3.f, 192.f, "Walnut"); }
		P.Box(X, 0.f, 191.f, D + 2.f, W + 4.f, 3.f, "Walnut");
		P.Box(X + 1.f, 0.f, 5.f, D - 2.f, W - 4.f, 10.f, "Walnut");
		static const FName Books[] = { "BookRed", "BookBlue", "BookGreen", "BookTan", "LeatherBrn", "Walnut", "BookRed", "PaintOlive", "BookTan" };
		const float Shelves[] = { 10.f, 48.f, 86.f, 124.f, 158.f };
		for (int32 s = 0; s < 5; ++s)
		{
			const float Z = Shelves[s];
			P.Box(X, 0.f, Z, D - 2.f, W - 6.f, 2.5f, "Walnut");
			if (s == 4 && U(Seed, 200) > 0.5f) { continue; }
			float Y = -W * 0.5f + 5.f;
			int32 k = Seed * 7 + s * 31;
			const float Room = (s < 4 ? Shelves[s + 1] : 189.f) - Z - 4.f;
			while (Y < W * 0.5f - 6.f)
			{
				const float R = U(k, 1);
				if (R < 0.07f) { Y += 10.f + U(k, 2) * 10.f; ++k; continue; } // a gap
				if (R < 0.15f)
				{
					// a little stack lying flat
					const int32 N = 2 + (int32)(U(k, 3) * 3.f);
					for (int32 i = 0; i < N; ++i) { P.Box(X + 2.f, Y + 11.f, Z + 3.f + i * 3.6f, 22.f, 20.f, 3.4f, Books[(k + i) % 9], J(k, 4 + i) * 6.f); }
					Y += 24.f; ++k; continue;
				}
				if (R < 0.2f && Y < W * 0.5f - 20.f)
				{
					// an odd object: a bottle, a box, a jar
					if (U(k, 5) > 0.5f) { P.Cyl(X, Y + 5.f, Z + 13.f, 8.f, 22.f, "Bottle"); }
					else { P.Box(X, Y + 7.f, Z + 8.f, 18.f, 14.f, 12.f, "Cardboard"); }
					Y += 16.f; ++k; continue;
				}
				const float BW = 3.f + U(k, 6) * 3.5f, BH = FMath::Min(Room - 2.f, 20.f + U(k, 7) * 12.f), BD = 18.f + U(k, 8) * 6.f;
				P.Box(X + 1.f + (D - BD) * 0.3f, Y + BW * 0.5f, Z + 1.25f + BH * 0.5f, BD, BW, BH, Books[k % 9]);
				Y += BW + 0.4f; ++k;
			}
		}
	}

	void DrawSideboard(const FPen& P, float W, int32 Seed)
	{
		const float D = 46.f;
		for (float S : { -1.f, 1.f }) { for (float X : { 4.f, D - 4.f }) { P.Box(X, S * (W * 0.5f - 4.f), 6.f, 5.f, 5.f, 12.f, "Walnut"); } }
		P.Box(D * 0.5f, 0.f, 46.f, D, W, 68.f, "Wood");
		P.Box(D * 0.5f + 1.f, 0.f, 82.f, D + 4.f, W + 4.f, 4.f, "Walnut");
		const int32 Doors = W > 110.f ? 3 : 2;
		for (int32 i = 0; i < Doors; ++i)
		{
			const float Y = -W * 0.5f + W * (i + 0.5f) / Doors;
			P.Box(D + 0.6f, Y, 44.f, 1.2f, W / Doors - 6.f, 56.f, "Walnut");
			P.Ball(D + 1.8f, Y + (i % 2 ? -1.f : 1.f) * (W / Doors * 0.32f), 48.f, 2.6f, 2.6f, 2.6f, "Brass");
		}
		// an old valve radio, bottles and a photo in a frame
		const FPen R = P.At(22.f, -W * 0.22f, J(Seed, 1) * 6.f);
		R.Box(0.f, 0.f, 94.f, 22.f, 36.f, 20.f, "Walnut");
		R.Box(11.2f, -5.f, 94.f, 0.6f, 18.f, 14.f, "Felt");
		R.Cyl(11.5f, 11.f, 96.f, 6.f, 1.2f, "Brass", 90.f);
		R.Cyl(11.5f, 11.f, 89.f, 5.f, 1.2f, "Brass", 90.f);
		R.Box(11.4f, 11.f, 102.f, 0.4f, 8.f, 2.f, "LedA", 0.f, 0.f, 0.f, false);
		for (int32 i = 0; i < 3; ++i) { P.Cyl(20.f + J(Seed, 2 + i) * 6.f, W * 0.18f + i * 9.f, 84.f + 12.f + i * 1.5f, 7.f, 24.f + i * 3.f, i == 1 ? FName("Glass") : FName("Bottle")); }
		P.Box(14.f, W * 0.4f, 96.f, 2.f, 14.f, 18.f, "Brass", -20.f, -8.f);
		P.Box(15.2f, W * 0.4f, 96.f, 0.4f, 11.f, 14.f, "Photo", -20.f, -8.f);
	}

	void DrawFilingCabinets(const FPen& P, int32 N, int32 Seed)
	{
		const float CW = 46.f, D = 62.f, H = 132.f;
		for (int32 c = 0; c < N; ++c)
		{
			const float Y = (c - (N - 1) * 0.5f) * (CW + 1.f);
			const FName M = (c + Seed) % 2 ? FName("PaintOlive") : FName("PaintGrey");
			P.Box(D * 0.5f, Y, H * 0.5f, D, CW, H, M);
			const int32 Open = (U(Seed, 10 + c) > 0.65f) ? (int32)(U(Seed, 20 + c) * 4.f) : -1;
			for (int32 k = 0; k < 4; ++k)
			{
				const float Z = 18.f + k * 31.f;
				const float Out = k == Open ? 26.f : 0.f;
				if (Out > 0.f)
				{
					P.Box(D * 0.5f + Out, Y, Z, D, CW - 4.f, 26.f, M);
					for (int32 f = 0; f < 6; ++f) { P.Box(D * 0.5f + Out + 18.f - f * 7.f, Y, Z + 13.f, 1.2f, CW - 10.f, 8.f + U(Seed, 30 + f) * 4.f, f % 2 ? FName("Cardboard") : FName("Paper")); }
				}
				P.Box(D + 0.6f + Out, Y, Z, 1.2f, CW - 4.f, 27.f, M);
				P.Box(D + 2.f + Out, Y, Z + 7.f, 2.f, 12.f, 2.f, "Chrome");
				P.Box(D + 1.4f + Out, Y, Z + 2.f, 0.4f, 8.f, 3.f, "Paper", 0.f, 0.f, 0.f, false);
			}
		}
		P.Box(D * 0.5f, -10.f, H + 6.f, 30.f, 36.f, 12.f, "Cardboard", J(Seed, 1) * 10.f);
		P.Box(D * 0.4f, 12.f, H + 2.f, 26.f, 30.f, 4.f, "Paper", J(Seed, 2) * 15.f);
	}

	void DrawSafe(const FPen& P)
	{
		P.Box(31.f, 0.f, 46.f, 62.f, 62.f, 92.f, "PaintGreen");
		P.Box(62.4f, 0.f, 48.f, 1.f, 52.f, 80.f, "CastIron");
		P.Cyl(63.5f, -6.f, 60.f, 12.f, 2.f, "Chrome", 90.f);
		P.Box(64.f, 14.f, 50.f, 3.f, 3.f, 20.f, "Chrome");
		P.Box(62.6f, 0.f, 84.f, 0.4f, 30.f, 4.f, "Brass", 0.f, 0.f, 0.f, false);
	}

	void DrawFloorLamp(const FPen& P)
	{
		P.Cyl(0.f, 0.f, 1.5f, 28.f, 3.f, "Brass");
		P.Cyl(0.f, 0.f, 78.f, 2.6f, 152.f, "Brass");
		P.Cyl(0.f, 0.f, 160.f, 40.f, 28.f, "LampShade");
		P.Cyl(0.f, 0.f, 147.f, 34.f, 0.4f, "Tungsten", 0.f, 0.f, false);
	}

	void DrawCrates(const FPen& P, int32 N, int32 Seed)
	{
		for (int32 i = 0; i < N; ++i)
		{
			const float SX = 48.f - i * 4.f, SY = 62.f - i * 6.f, SZ = 38.f;
			const FName M = U(Seed, i) > 0.5f ? FName("Wood") : FName("Plywood");
			const float Yaw = J(Seed, 10 + i) * 7.f;
			P.Box(0.f, 0.f, i * SZ + SZ * 0.5f, SX, SY, SZ, M, Yaw);
			P.Box(0.f, 0.f, i * SZ + SZ * 0.5f, SX + 0.6f, 5.f, SZ - 6.f, "TimberDark", Yaw);
			P.Box(0.f, 0.f, i * SZ + SZ * 0.5f, 5.f, SY + 0.6f, SZ - 6.f, "TimberDark", Yaw);
		}
		if (U(Seed, 30) > 0.4f) { P.Box(J(Seed, 31) * 6.f, J(Seed, 32) * 6.f, N * 38.f + 9.f, 30.f, 22.f, 18.f, "PaintOlive", 30.f); } // ammo tin on top
	}

	void DrawDuffels(const FPen& P, int32 Seed)
	{
		// soft holdalls: squashed spheres with a zip line and carry straps
		struct FBag { float X, Y, Z, L, W, H, Yaw; FName M; };
		const FBag Bags[] = {
			{ 0.f, -15.f, 14.f, 68.f, 32.f, 28.f, 90.f + J(Seed, 1) * 10.f, "NylonOlive" },
			{ 3.f, 17.f, 13.f, 60.f, 30.f, 26.f, 96.f + J(Seed, 2) * 14.f, "Canvas" },
			{ 1.f, 0.f, 36.f, 56.f, 28.f, 22.f, 84.f + J(Seed, 3) * 12.f, "Tarp" } };
		for (const FBag& B : Bags)
		{
			P.S(EDCShape::Sphere, B.X, B.Y, B.Z, B.L, B.W, B.H, B.M, B.Yaw);
			P.Box(B.X, B.Y, B.Z + B.H * 0.5f - 0.5f, B.L * 0.7f, 1.2f, 1.f, "PlasticBlack", B.Yaw, 0.f, 0.f, false);
			P.Box(B.X, B.Y, B.Z + B.H * 0.5f + 1.5f, 5.f, 14.f, 1.5f, "PlasticBlack", B.Yaw + 90.f);
		}
	}

	void DrawPlant(const FPen& P, int32 Seed)
	{
		P.Cyl(0.f, 0.f, 18.f, 34.f, 36.f, "Terracotta");
		P.Cyl(0.f, 0.f, 36.f, 38.f, 4.f, "Terracotta");
		P.Cyl(0.f, 0.f, 37.f, 30.f, 1.f, "Dirt", 0.f, 0.f, false);
		// a leafy mound: overlapping blobs, bigger and lower at the rim, a few sprigs poking out
		P.Rod(FVector(0.f, 0.f, 36.f), FVector(J(Seed, 1) * 4.f, J(Seed, 2) * 4.f, 62.f), 3.f, "TimberDark");
		for (int32 i = 0; i < 14; ++i)
		{
			const float A = i * 137.5f + J(Seed, i) * 20.f, Rr = (i < 8 ? 16.f : 7.f) + U(Seed, 20 + i) * 6.f;
			const FV2 D = Dir(A) * Rr;
			const float Sz = (i < 8 ? 26.f : 30.f) + U(Seed, 40 + i) * 10.f;
			const float Zc = (i < 8 ? 56.f : 72.f) + U(Seed, 60 + i) * 12.f;
			P.Ball(D.X, D.Y, Zc, Sz, Sz * 0.9f, Sz * 0.8f, "Plant");
		}
		for (int32 i = 0; i < 4; ++i)
		{
			const FV2 D = Dir(i * 90.f + 40.f + J(Seed, 80 + i) * 20.f) * 24.f;
			P.Rod(FVector(D.X * 0.4f, D.Y * 0.4f, 66.f), FVector(D.X, D.Y, 92.f + U(Seed, 90 + i) * 10.f), 1.2f, "Plant");
		}
	}

	void DrawJerrycans(const FPen& P, int32 Seed)
	{
		for (int32 i = 0; i < 2; ++i)
		{
			const FPen C = P.At(0.f, (i - 0.5f) * 22.f, J(Seed, i) * 8.f);
			const FName M = i == 0 ? FName("PaintRed") : FName("PaintOlive");
			C.Box(0.f, 0.f, 23.f, 34.f, 17.f, 46.f, M);
			C.Box(-6.f, 0.f, 48.f, 14.f, 3.f, 4.f, M);
			C.Cyl(11.f, 0.f, 48.f, 5.f, 5.f, M);
		}
	}

	void DrawNewspapers(const FPen& P, int32 Seed)
	{
		const int32 N = 6 + (int32)(U(Seed, 1) * 5.f);
		for (int32 i = 0; i < N; ++i) { P.Box(J(Seed, 10 + i) * 2.f, J(Seed, 20 + i) * 2.f, 1.6f + i * 3.f, 30.f, 40.f, 3.f, i % 3 ? FName("Paper") : FName("Cardboard"), J(Seed, 30 + i) * 8.f); }
		P.Rod(FVector(-14.f, 0.f, N * 3.f + 1.f), FVector(14.f, 0.f, N * 3.f + 1.f), 0.6f, "Rope");
	}

	void DrawPictures(const FPen& P, float W, int32 Seed)
	{
		const int32 N = W > 100.f ? 3 : 2;
		for (int32 i = 0; i < N; ++i)
		{
			const float Y = (i - (N - 1) * 0.5f) * (W / N) + J(Seed, i) * 3.f;
			const bool bTall = (i + Seed) % 2 == 0;
			const float FW = bTall ? 26.f : 36.f, FH = bTall ? 36.f : 26.f;
			const float Z = 160.f + J(Seed, 10 + i) * 10.f;
			const FName Frame = (i == 1) ? FName("Brass") : FName("TimberDark");
			P.Box(1.5f, Y, Z, 3.f, FW + 5.f, FH + 5.f, Frame, 0.f, 0.f, J(Seed, 20 + i) * 2.f);
			P.Box(3.1f, Y, Z, 0.4f, FW, FH, i == 1 ? FName("Photo") : FName("Paper"), 0.f, 0.f, J(Seed, 20 + i) * 2.f);
			if (i != 1) { P.Box(3.4f, Y, Z - FH * 0.15f, 0.3f, FW * 0.7f, FH * 0.4f, (Seed + i) % 2 ? FName("PaintGreen") : FName("BookTan"), 0.f, 0.f, J(Seed, 20 + i) * 2.f, false); }
		}
	}

	void DrawClock(const FPen& P)
	{
		P.Cyl(2.f, 0.f, 172.f, 34.f, 4.f, "Walnut", 90.f);
		P.Cyl(4.3f, 0.f, 172.f, 28.f, 0.6f, "Paper", 90.f);
		P.Box(4.9f, 0.f, 175.f, 0.4f, 1.4f, 9.f, "Black", 0.f, 0.f, 20.f);
		P.Box(4.9f, 3.f, 171.f, 0.4f, 8.f, 1.2f, "Black", 0.f, 0.f, -15.f);
		P.Box(2.f, 0.f, 148.f, 3.f, 10.f, 22.f, "Walnut");
		P.Ball(3.f, 0.f, 140.f, 7.f, 2.f, 7.f, "Brass");
	}

	void DrawShotgunRack(const FPen& P)
	{
		for (float Y : { -38.f, 38.f }) { P.Box(4.f, Y, 166.f, 8.f, 4.f, 10.f, "Walnut"); }
		P.Box(7.f, 0.f, 170.f, 4.f, 104.f, 3.f, "CastIron", 0.f, 0.f, 0.f);
		P.Box(7.f, 46.f, 168.f, 5.f, 34.f, 7.f, "Walnut", 0.f, 0.f, -6.f);
		P.Box(7.f, 18.f, 168.5f, 4.f, 24.f, 4.f, "Walnut");
	}

	void DrawWallShelf(const FPen& P, float W, int32 Seed)
	{
		P.Box(11.f, 0.f, 158.f, 20.f, W, 2.5f, "Walnut");
		for (float Y : { -W * 0.35f, W * 0.35f }) { P.Box(4.f, Y, 150.f, 8.f, 2.5f, 14.f, "CastIron"); }
		float Y = -W * 0.5f + 8.f;
		int32 k = 0;
		while (Y < W * 0.5f - 8.f)
		{
			const float R = U(Seed, k);
			if (R < 0.45f) { P.Cyl(11.f, Y, 170.f, 8.f, 20.f + U(Seed, 50 + k) * 6.f, R < 0.2f ? FName("Glass") : FName("Bottle")); Y += 11.f; }
			else if (R < 0.75f) { P.Cyl(11.f, Y, 165.f, 11.f, 12.f, "Glass"); P.Cyl(11.f, Y, 172.f, 11.5f, 2.f, "Rust"); Y += 14.f; }
			else { P.Box(11.f, Y + 6.f, 168.f, 16.f, 12.f, 16.f, "Cardboard", J(Seed, 70 + k) * 8.f); Y += 15.f; }
			++k;
		}
	}

	void DrawWallScreen(const FPen& P)
	{
		P.Box(2.f, 0.f, 168.f, 4.f, 92.f, 55.f, "PlasticBlack");
		P.Box(4.2f, 0.f, 168.f, 0.4f, 86.f, 49.f, "Screen2", 0.f, 0.f, 0.f, false);
		P.Rod(FVector(1.f, 30.f, 140.f), FVector(1.f, 34.f, 2.f), 1.2f, "Rubber");
	}
}

using namespace DCFurnish;

// =====================================================================================================================

bool ADCHideoutBuilder::FurnishGridRoom(ADCRoomModule* Room, const FVector2D Q[4], const int32 EdgeKind[4], const bool bExit[4], const bool bStepStart[4], const bool bStepEnd[4], float H, int32 WallSeed, int32 Seed, uint8 OutOmitWindows[4])
{
	if (!Room || Room->RoomType != EDCRoomType::Office) { return false; }
	const double T0 = FPlatformTime::Seconds();
	static const bool bDebug = FParse::Param(FCommandLine::Get(), TEXT("DCFurnishDebug"));
	DCFurnish::GDebug = bDebug;

	// ---------------------------------------------------------------- the room as the solver sees it
	DCFurnish::FPlan Plan;
	Plan.Init(Q, EdgeKind, bExit);
	for (int32 e = 0; e < 4; ++e)
	{
		DCFurnish::FEdge& Ed = Plan.E[e];
		if (Ed.Kind != DCFurnish::EKind::Wall) { continue; }
		TArray<FOpening> Win;
		TArray<FVector> Steps;
		TallEdgePlan(Ed.L, H, WallSeed + e * 7, bStepStart[e], bStepEnd[e], Win, Steps);
		for (int32 w = 0; w < Win.Num(); ++w)
		{
			const FOpening& O = Win[w];
			Ed.Windows.Add({ O.Center, O.Width, O.Bottom, O.Top, w });
			Ed.Bands.Add({ O.Center - O.Width * 0.5f - 8.f, O.Center + O.Width * 0.5f + 8.f, O.Bottom - 2.f, O.Top + 14.f, 50.f, w });
			++Plan.WindowsLeft;
		}
		for (const FVector& S : Steps) { Ed.Bands.Add({ (float)S.X - 10.f, (float)S.Y + 10.f, (float)S.Z - 10.f, KTall, 60.f }); }
	}
	if (Plan.E[1].Kind == DCFurnish::EKind::Wall && Plan.E[2].Kind == DCFurnish::EKind::Wall)
	{
		// the corner pier where the two back walls meet
		DCFurnish::FPart Pier;
		Pier.Box.C = Q[2] + (-Q[2]).GetSafeNormal() * 14.f; Pier.Box.H = FV2(24.f, 24.f); Pier.Box.Yaw = YawOf(Plan.E[1].D);
		Pier.Z0 = 0.f; Pier.Z1 = KTall;
		Plan.Parts.Add(Pier);
	}
	const float LampZ = FMath::Max(FMath::Min(H - 35.f, 235.f), 214.f);
	const float BoardTop = FMath::Min(206.f, LampZ - 18.f);

	auto Jit = [&](int32 K) { return J(Seed, 500 + K); };
	auto Fwd = [](const DCFurnish::FFrame& F) { return Dir(F.Yaw); };
	auto FacingHub = [&](const DCFurnish::FFrame& F) { return FV2::DotProduct(Fwd(F), (Plan.Hub - F.O).GetSafeNormal()); };
	auto FacingCam = [&](const DCFurnish::FFrame& F) { return FV2::DotProduct(Fwd(F), KToCam); };
	const uint8 MWall = 1 << (uint8)DCFurnish::EKind::Wall, MPart = 1 << (uint8)DCFurnish::EKind::Partition, MAny = 0x7;

	// What gets drawn once the layout is settled.
	struct FPlaced { int32 Kind; DCFurnish::FFrame F; float A = 0.f, B = 0.f; FName Group; };
	enum { PBoard, PEasel, PTable, PChair, PRug, PStove, PLogs, PDesk, PSofa, PCoffee, PArmchair, PLamp, PBookcase, PSideboard, PFiling, PSafe, PCrates, PDuffels, PPlant, PCans, PPapers, PPictures, PClock, PShotgun, PShelf, PScreen };
	TArray<FPlaced> Out;
	TArray<ADCRoomModule::FUseSpot> Spots;
	auto Spot = [&](const DCFurnish::FFrame& F, float X, float Y)
	{
		const FV2 P = F.O + Rot(FV2(X, Y), F.Yaw);
		if (Plan.Inside(P, 25.f)) { Spots.Add({ P, F.Yaw + 180.f }); }
	};
	auto Place = [&](int32 Kind, const TArray<DCFurnish::FLocal>& Ls, const DCFurnish::FFrame& F, float A = 0.f, float B = 0.f, FName Group = NAME_None)
	{
		Plan.Commit(Ls, F);
		DCFurnish::FFrame D = F;
		if (Kind == PRug && Ls.Num() > 0) { D.O = F.O + Rot(Ls[0].C, F.Yaw); } // a rug is drawn around its own centre
		Out.Add({ Kind, D, A, B, Group == NAME_None ? DCGroups::Shell : Group });
	};
	TArray<DCFurnish::FFrame> WallF, WallPartF, AnyWallF, FreeF;
	Plan.WallFrames(MWall, false, 6.f, WallF);
	Plan.WallFrames(MWall | MPart, true, 6.f, WallPartF);
	Plan.WallFrames(MAny, false, 8.f, AnyWallF);
	Plan.FreeFrames(12.f, FreeF);

	// ---------------------------------------------------------------- 1. the mission board (the room's station)
	// Tall wall furniture may stand against a window (the wall is then built without it) at a price, as long as the
	// room keeps a window.
	for (int32 e = 0; e < 4; ++e) { OutOmitWindows[e] = 0; }
	TArray<FIntPoint> Hit;
	auto BestSoft = [&](const TArray<DCFurnish::FLocal>& Ls, const TArray<DCFurnish::FFrame>& Frames, TFunctionRef<float(const DCFurnish::FFrame&)> Score, float Pad, float Penalty)
	{
		Plan.bSoftWindows = true;
		const DCFurnish::FFrame F = Plan.Best(Ls, Frames, [&](const DCFurnish::FFrame& C) -> float
		{
			Plan.WindowsHit(Ls, C, Hit);
			if (Hit.Num() > 0 && Plan.WindowsLeft - Hit.Num() < 1) { return -1e9f; }
			return Score(C) - Penalty * Hit.Num();
		}, Pad);
		Plan.bSoftWindows = false;
		return F;
	};
	auto PlaceSoft = [&](int32 Kind, const TArray<DCFurnish::FLocal>& Ls, const DCFurnish::FFrame& F, float A = 0.f, float B = 0.f, FName Group = NAME_None)
	{
		Plan.WindowsHit(Ls, F, Hit);
		Plan.Brickup(Hit, OutOmitWindows);
		Place(Kind, Ls, F, A, B, Group);
	};

	DCFurnish::FFrame Board;
	float BoardW = 0.f;
	{
		float BestS = -1e8f;
		for (float W : { 230.f, 200.f, 170.f, 145.f })
		{
			TArray<DCFurnish::FLocal> Ls = { DCFurnish::FLocal::Space(5.f, 0.f, 5.f, W * 0.5f + 6.f, BoardTop + 4.f) };
			Ls[0].Z0 = BoardTop - 125.f; // the board only claims the wall above the floor
			const TArray<DCFurnish::FLocal> Probe = { DCFurnish::FLocal::Solid(8.f + 95.f + 44.f, 0.f, 44.f, 75.f, 82.f) }; // room for the table
			auto Score = [&](const DCFurnish::FFrame& C)
			{
				const DCFurnish::FEdge& Ed = Plan.E[C.Edge];
				return (float)(W * 0.5f - FMath::Abs(C.T - Ed.L * 0.5f) * 0.12f + Ed.L * 0.02f + Jit(C.Edge) * 3.f + (Plan.Fits(Probe, C, 4.f) ? 0.f : -60.f));
			};
			const DCFurnish::FFrame F = BestSoft(Ls, WallF, Score, 4.f, 55.f);
			if (!F.bValid) { continue; }
			Plan.WindowsHit(Ls, F, Hit);
			const float S = Score(F) - 55.f * Hit.Num();
			if (S > BestS) { BestS = S; Board = F; BoardW = W; }
		}
		if (Board.bValid)
		{
			TArray<DCFurnish::FLocal> Ls = { DCFurnish::FLocal::Space(5.f, 0.f, 5.f, BoardW * 0.5f + 6.f, BoardTop + 4.f) };
			Ls[0].Z0 = BoardTop - 125.f;
			PlaceSoft(PBoard, Ls, Board, BoardW);
		}
		else
		{
			// no tall wall: a low board on legs, backed onto a partition if possible, else standing at the back of the
			// room facing the camera
			for (float W : { 170.f, 140.f, 115.f })
			{
				const TArray<DCFurnish::FLocal> Ls = { DCFurnish::FLocal::Solid(27.f, 0.f, 27.f, W * 0.5f + 4.f, 146.f) };
				Board = Plan.Best(Ls, WallPartF, [&](const DCFurnish::FFrame& F) { return (float)(-FMath::Abs(F.T - Plan.E[F.Edge].L * 0.5f) * 0.05f + FacingCam(F) * 10.f); }, 4.f);
				if (!Board.bValid)
				{
					TArray<DCFurnish::FFrame> Fr;
					for (const DCFurnish::FFrame& F0 : FreeF)
					{
						for (float Y : { YawOf(KToCam), Plan.RoomYaw - 90.f, Plan.RoomYaw + 180.f }) { DCFurnish::FFrame F = F0; F.Yaw = Y; Fr.Add(F); }
					}
					Board = Plan.Best(Ls, Fr, [&](const DCFurnish::FFrame& F)
					{
						if (FV2::DotProduct(F.O - Plan.Hub, KToCam) > 0.f) { return -1e9f; } // the back half only
						return (float)(-FV2::Distance(F.O, Plan.Hub - KToCam * 150.f) * 0.5f + FacingCam(F) * 20.f);
					}, 4.f);
				}
				if (Board.bValid) { BoardW = W; Place(PEasel, Ls, Board, BoardW); Board.Edge = -1; break; }
			}
		}
	}

	// the room light: a bracket lamp over the board, else on a standing back wall, else a bare bulb
	int32 LampEdge = -1; float LampT = 0.f;
	if (Board.bValid && Board.Edge >= 0) { LampEdge = Board.Edge; LampT = Board.T; }
	else
	{
		for (int32 e : { 2, 1 })
		{
			if (Plan.E[e].Kind == DCFurnish::EKind::Wall) { LampEdge = e; LampT = Plan.E[e].L * (0.5f + Jit(10) * 0.08f); break; }
		}
	}
	if (LampEdge >= 0) { Plan.E[LampEdge].Bands.Add({ LampT - 30.f, LampT + 30.f, LampZ - 40.f, LampZ + 25.f, 75.f }); }

	// ---------------------------------------------------------------- 2+3. the planning table and the Director's desk
	// Solved together: the biggest table in front of the board that still leaves the desk a place. Failing that the
	// table stands free, and as a last resort the walking lanes narrow.
	DCFurnish::FFrame Table;
	float TableW = 0.f, TableD = 0.f;
	auto TryTable = [&](const FV2& Sz) -> bool
	{
		if (!Board.bValid) { return false; }
		const float Front = Board.Edge >= 0 ? 8.f : 56.f;
		TArray<DCFurnish::FFrame> Fr;
		for (float Gap = 70.f; Gap <= 130.f; Gap += 10.f)
		{
			for (float Lat = -40.f; Lat <= 40.f; Lat += 10.f)
			{
				DCFurnish::FFrame F = Board; F.O = Board.O + Rot(FV2(Front + Gap + Sz.Y * 0.5f, Lat), Board.Yaw); F.T = Gap; F.Edge = -1;
				Fr.Add(F);
			}
		}
		const TArray<DCFurnish::FLocal> Ls = { DCFurnish::FLocal::Solid(0.f, 0.f, Sz.Y * 0.5f, Sz.X * 0.5f, 84.f) };
		Table = Plan.Best(Ls, Fr, [&](const DCFurnish::FFrame& F)
		{
			const FV2 L = Rot(F.O - Board.O, -Board.Yaw);
			return (float)(-FMath::Abs(F.T - 90.f) * 0.5f - FMath::Abs(L.Y) * 0.3f);
		}, 4.f);
		if (!Table.bValid) { return false; }
		TableW = Sz.X; TableD = Sz.Y;
		const float Gap = Table.T;
		// keep the standing strip between the board and the table clear
		TArray<DCFurnish::FLocal> All = Ls;
		All.Add(DCFurnish::FLocal::Space(-(Gap * 0.5f + Sz.Y * 0.5f), 0.f, Gap * 0.5f - 2.f, FMath::Min(Sz.X, BoardW) * 0.5f, 60.f));
		for (float S : { -1.f, 1.f })
		{
			// hold the end seats (if they're free now) so the desk doesn't take them
			DCFurnish::FLocal Seat = DCFurnish::FLocal::Space(0.f, S * (Sz.X * 0.5f + 30.f), 26.f, 26.f, 60.f);
			Seat.Tag = DCFurnish::TagSeat;
			if (Plan.PartOk(Plan.Realise(Seat, Table), 2.f)) { All.Add(Seat); }
		}
		Place(PTable, All, Table, TableW, TableD);
		Spot(Table, -(Sz.Y * 0.5f + Gap * 0.5f), -Sz.X * 0.18f);
		Spot(Table, -(Sz.Y * 0.5f + Gap * 0.5f), Sz.X * 0.22f);
		return true;
	};
	auto TryFreeTable = [&]() -> bool
	{
		for (const FV2& Sz : { FV2(150.f, 86.f), FV2(124.f, 80.f) })
		{
			const TArray<DCFurnish::FLocal> Ls = { DCFurnish::FLocal::Solid(0.f, 0.f, Sz.Y * 0.5f, Sz.X * 0.5f, 84.f) };
			TArray<DCFurnish::FFrame> Fr = FreeF;
			for (const DCFurnish::FFrame& F : FreeF) { DCFurnish::FFrame G = F; G.Yaw += 90.f; Fr.Add(G); }
			const FV2 Want = Board.bValid ? Board.O + Dir(Board.Yaw) * 170.f : Plan.Hub - KToCam * 110.f;
			Table = Plan.Best(Ls, Fr, [&](const DCFurnish::FFrame& F)
			{
				if (Board.bValid && FV2::DotProduct(F.O - Board.O, Dir(Board.Yaw)) < -20.f) { return -1e9f; } // never behind the board
				return (float)(-FV2::Distance(F.O, Want) * 0.4f + FV2::DotProduct(F.O - Plan.Hub, -KToCam) * 0.1f);
			}, 6.f);
			if (Table.bValid) { TableW = Sz.X; TableD = Sz.Y; Place(PTable, Ls, Table, TableW, TableD); Spot(Table, -(TableD * 0.5f + 34.f), 0.f); return true; }
		}
		return false;
	};

	// The desk faces the room with the chair against the wall; failing that it is pushed against the wall with
	// the chair out front.
	DCFurnish::FFrame Desk;
	bool bDeskToWall = false;
	float DeskW = 152.f, DeskD = 76.f;
	auto TryDesk = [&]() -> bool
	{
		auto Score = [&](const DCFurnish::FFrame& F)
		{
			// leave the back corner for the stove; the desk reads best further forward on a side wall
			return (float)(FacingHub(F) * 12.f + FacingCam(F) * 8.f + FMath::Min(FV2::Distance(F.O, Plan.In[2]), 300.f) * 0.06f + (F.Edge == Board.Edge ? -40.f : 0.f)
				+ (Plan.E[F.Edge].Kind == DCFurnish::EKind::Wall ? 6.f : 0.f) + Jit(30 + F.Edge) * 5.f);
		};
		TArray<DCFurnish::FFrame> Fr;
		for (const DCFurnish::FFrame& F0 : FreeF)
		{
			for (float Y : { YawOf(KToCam), Plan.RoomYaw - 90.f, Plan.RoomYaw + 180.f }) { DCFurnish::FFrame F = F0; F.Yaw = Y; Fr.Add(F); }
		}
		for (const FV2& Sz : { FV2(152.f, 76.f), FV2(112.f, 62.f) })
		{
			DeskW = Sz.X; DeskD = Sz.Y;
			const float HW = Sz.X * 0.5f + 1.f, HD = Sz.Y * 0.5f + 1.f;
			// facing the room, chair against the wall
			const float DX = 80.f + HD;
			const TArray<DCFurnish::FLocal> Ls = {
				DCFurnish::FLocal::Space(50.f, 0.f, 28.f, 38.f, 84.f),
				DCFurnish::FLocal::Solid(DX, 0.f, HD, HW, 82.f),
				DCFurnish::FLocal::Space(DX + HD + 16.f, 0.f, 16.f, 38.f, 50.f) };
			Desk = Plan.Best(Ls, WallPartF, Score, 6.f, TEXT("desk"));
			if (Desk.bValid) { bDeskToWall = false; Place(PDesk, Ls, Desk); Spot(Desk, DX + HD + 20.f, 0.f); return true; }
			// pushed against the wall, chair out front
			const TArray<DCFurnish::FLocal> L2 = {
				DCFurnish::FLocal::Solid(HD + 2.f, 0.f, HD, HW, 84.f),
				DCFurnish::FLocal::Space(Sz.Y + 36.f, 0.f, 30.f, 38.f, 84.f) };
			Desk = Plan.Best(L2, WallPartF, Score, 6.f);
			if (Desk.bValid) { bDeskToWall = true; Place(PDesk, L2, Desk); Spot(Desk, Sz.Y + 36.f, 0.f); return true; }
			// a room of doorways: the desk stands free, facing the camera, towards the back
			Desk = Plan.Best(Ls, Fr, [&](const DCFurnish::FFrame& F) { return (float)(FacingCam(F) * 10.f - FV2::Distance(F.O + Dir(F.Yaw) * DX, Plan.Hub - KToCam * 70.f) * 0.1f); }, 6.f, TEXT("desk free"));
			if (Desk.bValid) { bDeskToWall = false; Place(PDesk, Ls, Desk); Spot(Desk, DX + HD + 20.f, 0.f); return true; }
		}
		return false;
	};

	{
		const int32 P0 = Plan.Parts.Num(), O0 = Out.Num(), S0 = Spots.Num();
		auto Rewind = [&]() { Plan.Parts.SetNum(P0); Out.SetNum(O0); Spots.SetNum(S0); Table = DCFurnish::FFrame(); Desk = DCFurnish::FFrame(); };
		bool bDone = false;
		for (int32 Relax = 0; Relax < 2 && !bDone; ++Relax)
		{
			Plan.CapScale = Relax ? 0.7f : 1.f;
			Plan.bHubKeep = Relax == 0;
			for (const FV2& Sz : { FV2(170.f, 90.f), FV2(150.f, 86.f), FV2(128.f, 80.f), FV2(0.f, 0.f) })
			{
				Rewind();
				const bool bT = Sz.X > 0.f ? TryTable(Sz) : TryFreeTable();
				if (!bT) { continue; }
				if (TryDesk()) { bDone = true; break; }
			}
		}
		if (!bDone)
		{
			// the desk first, then the best table we can still get
			Rewind();
			Plan.CapScale = 0.7f; Plan.bHubKeep = false;
			TryDesk();
			Plan.CapScale = 1.f; Plan.bHubKeep = true;
			if (!TryTable(FV2(170.f, 90.f)) && !TryTable(FV2(128.f, 80.f))) { TryFreeTable(); }
		}
		Plan.CapScale = 1.f; Plan.bHubKeep = true;
	}
	if (Table.bValid)
	{
		// mismatched chairs: the two ends first (side-on to the camera reads best), then the far side
		struct FSeat { float X, Y, Yaw; };
		const TArray<FSeat> Seats = {
			{ 0.f, -(TableW * 0.5f + 24.f), 90.f }, { 0.f, TableW * 0.5f + 24.f, -90.f },
			{ TableD * 0.5f + 24.f, -TableW * 0.22f, 180.f }, { TableD * 0.5f + 24.f, TableW * 0.22f, 180.f } };
		int32 NChairs = 0;
		Plan.CapScale = 0.6f; Plan.bHubKeep = false; // a chair may stand in the way a little
		for (int32 i = 0; i < Seats.Num() && NChairs < 3; ++i)
		{
			const float Pull = 4.f + U(Seed, 600 + i) * 10.f, Twist = J(Seed, 610 + i) * 14.f;
			const FV2 Out2 = FV2(Seats[i].X, Seats[i].Y) + Dir(Seats[i].Yaw + 180.f) * Pull;
			DCFurnish::FFrame F = Table; F.O = Table.O + Rot(Out2, Table.Yaw); F.Yaw = Table.Yaw + Seats[i].Yaw + Twist; F.Edge = -1;
			TArray<DCFurnish::FLocal> Ls = { DCFurnish::FLocal::Solid(0.f, 0.f, 24.f, 24.f, 70.f) }; // slatted backs: see-through
			Ls[0].Tag = DCFurnish::TagSeat;
			if (!Plan.Fits(Ls, F, 3.f))
			{
				if (bDebug) { UE_LOG(LogDC, Log, TEXT("[Furnish] chair %d: %s"), i, Plan.WhyNot(Plan.Realise(Ls[0], F), 3.f)); }
				continue;
			}
			Place(PChair, Ls, F, (float)i);
			++NChairs;
		}
		Plan.CapScale = 1.f; Plan.bHubKeep = true;
		Plan.Parts.RemoveAll([](const DCFurnish::FPart& P) { return P.Tag == DCFurnish::TagSeat && !P.bSolid; }); // seats nobody took
		// a rug under the lot
		for (float K : { 1.f, 0.85f, 0.72f })
		{
			const TArray<DCFurnish::FLocal> Ls = { DCFurnish::FLocal::Rug(0.f, 0.f, (TableD * 0.5f + 70.f) * K, (TableW * 0.5f + 62.f) * K) };
			if (Plan.Fits(Ls, Table, 0.f)) { Place(PRug, Ls, Table, Ls[0].H.X * 2.f, Ls[0].H.Y * 2.f); break; }
		}
	}

	// ---------------------------------------------------------------- 4. the stove in a back corner
	DCFurnish::FFrame Stove;
	{
		const TArray<DCFurnish::FLocal> Ls = { DCFurnish::FLocal::Solid(46.f, 0.f, 46.f, 52.f, KTall) };
		Stove = BestSoft(Ls, WallF, [&](const DCFurnish::FFrame& F) { return (float)(-Plan.CornerDist(F.O) * 0.4f + Jit(20 + F.Edge) * 6.f); }, 6.f, 30.f);
		if (!Stove.bValid && bDebug) { Plan.bSoftWindows = true; Plan.Best(Ls, WallF, [&](const DCFurnish::FFrame&) { return 0.f; }, 6.f, TEXT("stove")); Plan.bSoftWindows = false; }
		if (Stove.bValid)
		{
			PlaceSoft(PStove, Ls, Stove);
			Spot(Stove, 125.f, 0.f);
			// logs beside it, on whichever side has room
			for (float S : { 1.f, -1.f })
			{
				DCFurnish::FFrame F = Stove; F.O = Stove.O + Rot(FV2(34.f, S * 82.f), Stove.Yaw); F.Edge = -1;
				const TArray<DCFurnish::FLocal> LL = { DCFurnish::FLocal::Solid(0.f, 0.f, 25.f, 25.f, 50.f) };
				if (Plan.Fits(LL, F, 2.f)) { Place(PLogs, LL, F); break; }
			}
		}
	}

	// ---------------------------------------------------------------- 5. somewhere to sit: a sofa group, else armchairs
	DCFurnish::FFrame Sofa;
	float SofaW = 0.f;
	// the full group (sofa + coffee table) first; a sofa on its own if the table won't fit too
	for (int32 Pass = 0; Pass < 2 && !Sofa.bValid; ++Pass)
	{
		for (float W : { 200.f, 176.f, 150.f })
		{
			TArray<DCFurnish::FLocal> Ls = {
				DCFurnish::FLocal::Solid(46.f, 0.f, 44.f, W * 0.5f, 86.f),
				DCFurnish::FLocal::Space(110.f, 0.f, 20.f, W * 0.5f - 24.f, 40.f) };
			if (Pass == 0) { Ls.Add(DCFurnish::FLocal::Solid(158.f, 0.f, 28.f, 50.f, 46.f)); }
			Sofa = Plan.Best(Ls, WallPartF, [&](const DCFurnish::FFrame& F)
			{
				return FacingHub(F) * 10.f + (F.Edge == Board.Edge ? -30.f : 0.f) + (F.Edge == Desk.Edge ? -12.f : 0.f) + Jit(40 + F.Edge) * 5.f
					- FMath::Abs(F.T - Plan.E[F.Edge].L * 0.5f) * 0.03f;
			}, 6.f, TEXT("sofa"));
			if (!Sofa.bValid) { continue; }
			SofaW = W;
			Place(PSofa, { Ls[0], Ls[1] }, Sofa, W);
			if (Pass == 0)
			{
				DCFurnish::FFrame CT = Sofa; CT.O = Sofa.O + Rot(FV2(158.f, 0.f), Sofa.Yaw); CT.Edge = -1;
				Place(PCoffee, { DCFurnish::FLocal::Solid(0.f, 0.f, 28.f, 50.f, 46.f) }, CT);
			}
			for (float K : { 1.f, 0.85f, 0.7f })
			{
				const float Deep = Pass == 0 ? 92.f : 62.f;
				const TArray<DCFurnish::FLocal> R = { DCFurnish::FLocal::Rug(46.f + Deep, 0.f, Deep * K, (W * 0.5f + 28.f) * K) };
				if (Plan.Fits(R, Sofa, 0.f)) { Place(PRug, R, Sofa, R[0].H.X * 2.f, R[0].H.Y * 2.f, "Blue"); break; }
			}
			break;
		}
	}
	TArray<DCFurnish::FFrame> Seats;
	if (!Sofa.bValid)
	{
		// two armchairs turned towards the stove (or the table), never with their backs to the camera
		const FV2 Focus = Stove.bValid ? Stove.O + Dir(Stove.Yaw) * 90.f : (Table.bValid ? Table.O : Plan.Hub);
		for (int32 n = 0; n < 2; ++n)
		{
			// each spot may turn towards the fire, the room or the camera: whichever reads best
			TArray<DCFurnish::FFrame> Fr;
			for (const DCFurnish::FFrame& F0 : FreeF)
			{
				for (const FV2& T : { Focus, Plan.Hub, F0.O + KToCam * 100.f })
				{
					DCFurnish::FFrame F = F0; F.Yaw = YawOf(T - F0.O) + J(Seed, 640 + n) * 10.f; Fr.Add(F);
				}
			}
			// or backed onto a wall / partition, turned a little towards the room
			for (const DCFurnish::FFrame& W0 : WallPartF)
			{
				DCFurnish::FFrame F = W0; F.O = W0.O + Dir(W0.Yaw) * 46.f; F.Edge = -1;
				const float Turn = FMath::Clamp(FMath::FindDeltaAngleDegrees(W0.Yaw, YawOf(Plan.Hub - F.O)), -25.f, 25.f);
				F.Yaw = W0.Yaw + Turn;
				Fr.Add(F);
			}
			const TArray<DCFurnish::FLocal> Ls = { DCFurnish::FLocal::Solid(0.f, 0.f, 42.f, 42.f, 86.f) }; // a low club chair
			const DCFurnish::FFrame F = Plan.Best(Ls, Fr, [&](const DCFurnish::FFrame& C)
			{
				if (FacingCam(C) < -0.3f) { return -1e9f; }
				float WallD = 1e9f;
				for (int32 e = 0; e < 4; ++e) { if (Plan.E[e].bBack) { WallD = FMath::Min(WallD, (float)Plan.E[e].Depth(C.O) - Plan.E[e].Inset); } }
				if (WallD > 130.f) { return -1e9f; } // not adrift in the middle of the floor
				const float FireD = FV2::Distance(C.O, Focus);
				return (float)(-FMath::Max(FireD - 130.f, 0.f) * 0.25f + FV2::DotProduct(Dir(C.Yaw), (Focus - C.O).GetSafeNormal()) * 6.f
					+ FacingHub(C) * 6.f + FacingCam(C) * 5.f - FMath::Min(Plan.NearestSolid(C.O), 150.f) * 0.05f);
			}, 10.f, TEXT("armchair"));
			if (!F.bValid) { break; }
			Place(PArmchair, Ls, F);
			Seats.Add(F);
		}
	}

	// a reading lamp beside the seating
	{
		TArray<DCFurnish::FFrame> Fr;
		if (Sofa.bValid) { for (float S : { 1.f, -1.f }) { DCFurnish::FFrame F = Sofa; F.O = Sofa.O + Rot(FV2(30.f, S * (SofaW * 0.5f + 24.f)), Sofa.Yaw); F.Edge = -1; Fr.Add(F); } }
		for (const DCFurnish::FFrame& A : Seats) { for (float S : { 1.f, -1.f }) { DCFurnish::FFrame F = A; F.O = A.O + Rot(FV2(-22.f, S * 62.f), A.Yaw); Fr.Add(F); } }
		const TArray<DCFurnish::FLocal> Ls = { DCFurnish::FLocal::Solid(0.f, 0.f, 20.f, 20.f, 176.f) };
		const DCFurnish::FFrame F = Plan.Best(Ls, Fr, [&](const DCFurnish::FFrame& C) { return -FV2::DotProduct(C.O - Plan.Hub, KToCam) * 0.05f; }, 3.f);
		if (F.bValid) { Place(PLamp, Ls, F); }
	}

	// a runner along the main way in: the walking lane becomes part of the room
	for (int32 e : { 0, 3 })
	{
		const DCFurnish::FEdge& Ed = Plan.E[e];
		if (!Ed.bExit) { continue; }
		const FV2 A = Ed.At(Ed.L * 0.5f, Ed.Inset + 18.f);
		const FV2 D = Plan.Hub - A;
		const float Len = (float)D.Size() + 30.f;
		DCFurnish::FFrame F; F.O = A; F.Yaw = YawOf(D); F.bValid = true;
		bool bDone = false;
		for (float K : { 1.f, 0.8f, 0.62f })
		{
			const TArray<DCFurnish::FLocal> Ls = { DCFurnish::FLocal::Rug(Len * K * 0.5f, 0.f, Len * K * 0.5f, 42.f) };
			if (!Plan.Fits(Ls, F, 0.f)) { continue; }
			// it's a walkway: nothing may stand on it
			DCFurnish::FLocal Clear = Ls[0]; Clear.bRug = false; Clear.Z1 = 200.f;
			const DCFurnish::FPart CP = Plan.Realise(Clear, F);
			bool bFree = true;
			for (const DCFurnish::FPart& O : Plan.Parts) { if (!O.bRug && O.Z0 < 100.f && DCFurnish::Overlap(CP.Box, O.Box, 6.f)) { bFree = false; break; } }
			if (bFree) { Place(PRug, Ls, F, Len * K, 84.f, "Blue"); Plan.Keep.Add(CP.Box); bDone = true; break; }
		}
		if (bDone) { break; }
	}

	// ---------------------------------------------------------------- 6. storage along what's left of the walls
	auto Adjacent = [&](const DCFurnish::FFrame& F) { return -FMath::Min(Plan.NearestSolid(F.O), 200.f) * 0.15f - Plan.CornerDist(F.O) * 0.04f; };
	{
		const TArray<DCFurnish::FLocal> Ls = { DCFurnish::FLocal::Solid(17.f, 0.f, 17.f, 49.f, 194.f) };
		const DCFurnish::FFrame F = BestSoft(Ls, WallF, [&](const DCFurnish::FFrame& C) { return (float)(Adjacent(C) + Jit(50 + C.Edge) * 4.f); }, 5.f, 30.f);
		if (F.bValid) { PlaceSoft(PBookcase, Ls, F, 96.f); Spot(F, 72.f, 0.f); }
	}
	{
		const TArray<DCFurnish::FLocal> Ls = { DCFurnish::FLocal::Solid(31.f, 0.f, 31.f, 48.f, 136.f) };
		const DCFurnish::FFrame F = Plan.Best(Ls, WallPartF, [&](const DCFurnish::FFrame& C) { return (Desk.bValid ? -FV2::Distance(C.O, Desk.O) * 0.08f : 0.f) + Adjacent(C) + Jit(60 + C.Edge) * 4.f; }, 5.f);
		if (F.bValid) { Place(PFiling, Ls, F, 2.f); }
	}
	{
		const TArray<DCFurnish::FLocal> Ls = { DCFurnish::FLocal::Solid(24.f, 0.f, 25.f, 62.f, 88.f) };
		const DCFurnish::FFrame F = Plan.Best(Ls, WallPartF, [&](const DCFurnish::FFrame& C) { return (Plan.InWindow(C.Edge, C.T, 62.f) ? 14.f : 0.f) + Adjacent(C) * 0.5f + Jit(70 + C.Edge) * 4.f; }, 5.f);
		if (F.bValid) { Place(PSideboard, Ls, F, 124.f); }
	}
	// upgrades bring more: a second bookcase and a safe
	{
		const TArray<DCFurnish::FLocal> Ls = { DCFurnish::FLocal::Solid(17.f, 0.f, 17.f, 49.f, 194.f) };
		const DCFurnish::FFrame F = Plan.Best(Ls, WallF, [&](const DCFurnish::FFrame& C) { return Adjacent(C) + Jit(80 + C.Edge) * 4.f; }, 5.f);
		if (F.bValid) { Place(PBookcase, Ls, F, 96.f, 0.f, DCGroups::Level2); }
	}
	{
		const TArray<DCFurnish::FLocal> Ls = { DCFurnish::FLocal::Solid(31.f, 0.f, 31.f, 31.f, 94.f) };
		const DCFurnish::FFrame F = Plan.Best(Ls, WallPartF, [&](const DCFurnish::FFrame& C) { return (Desk.bValid ? -FV2::Distance(C.O, Desk.O) * 0.12f : 0.f) + Adjacent(C) * 0.5f; }, 5.f);
		if (F.bValid) { Place(PSafe, Ls, F, 0.f, 0.f, DCGroups::Level3); }
	}

	// ---------------------------------------------------------------- 7. clutter, in clusters (corners, beside furniture)
	{
		struct FFill { int32 Kind; FV2 Half; float Z1; };
		const FFill Fills[] = {
			{ PCrates, FV2(30.f, 36.f), 90.f }, { PPlant, FV2(26.f, 26.f), 70.f }, { PDuffels, FV2(30.f, 40.f), 54.f },
			{ PCans, FV2(20.f, 26.f), 52.f }, { PPapers, FV2(24.f, 24.f), 40.f }, { PCrates, FV2(30.f, 36.f), 58.f } };
		for (int32 i = 0; i < UE_ARRAY_COUNT(Fills) && Plan.Coverage() < 0.42f; ++i)
		{
			const FFill& Fl = Fills[i];
			const TArray<DCFurnish::FLocal> Ls = { DCFurnish::FLocal::Solid(Fl.Half.X + 2.f, 0.f, Fl.Half.X, Fl.Half.Y, Fl.Z1) };
			const DCFurnish::FFrame F = Plan.Best(Ls, AnyWallF, [&](const DCFurnish::FFrame& C)
			{
				const float Near = Plan.NearestSolid(C.O), Corner = Plan.CornerDist(C.O);
				if (Near > 70.f && Corner > 90.f) { return -1e9f; } // clutter gathers, it doesn't float mid-wall
				return (float)(-Near * 0.2f - Corner * 0.12f + J(Seed, 700 + i * 13 + C.Edge) * 10.f + U(Seed, 800 + (int32)C.T) * 4.f
					- FV2::DotProduct(C.O - Plan.Hub, KToCam) * 0.06f);
			}, 6.f);
			if (F.bValid)
			{
				DCFurnish::FFrame G = F; G.Yaw += J(Seed, 720 + i) * 12.f; // set down a bit askew
				Place(Fl.Kind, Ls, G, Fl.Z1 > 80.f ? 2.f : 1.f);
			}
		}
	}

	// ---------------------------------------------------------------- 8. on the walls
	{
		struct FDecor { int32 Kind; float W; float Z0, Z1, D; };
		const FDecor Decor[] = { { PPictures, 120.f, 135.f, 190.f, 6.f }, { PShotgun, 116.f, 158.f, 180.f, 12.f }, { PClock, 40.f, 128.f, 192.f, 8.f }, { PShelf, 86.f, 150.f, 186.f, 22.f } };
		for (int32 i = 0; i < UE_ARRAY_COUNT(Decor); ++i)
		{
			const FDecor& Dc = Decor[i];
			TArray<DCFurnish::FLocal> Ls = { DCFurnish::FLocal::Solid(Dc.D * 0.5f, 0.f, Dc.D * 0.5f, Dc.W * 0.5f, Dc.Z1, Dc.Z0) };
			Ls[0].bSolid = false;
			const DCFurnish::FFrame F = Plan.Best(Ls, WallF, [&](const DCFurnish::FFrame& C)
			{
				// best above low furniture (a sofa, the desk, the sideboard), centred on it
				float Above = 0.f;
				DCFurnish::FObb Probe; Probe.C = C.O + Dir(C.Yaw) * 45.f; Probe.H = FV2(40.f, Dc.W * 0.4f); Probe.Yaw = C.Yaw;
				for (const DCFurnish::FPart& P : Plan.Parts)
				{
					if (P.bSolid && !P.bRug && P.Z1 < 110.f && Overlap(Probe, P.Box, 0.f)) { Above = FMath::Max(Above, 20.f - FV2::Distance(P.Box.C, Probe.C) * 0.05f); }
				}
				return Above + J(Seed, 900 + i * 7 + C.Edge) * 6.f - FMath::Abs(C.T - Plan.E[C.Edge].L * 0.5f) * 0.01f;
			}, 22.f);
			if (F.bValid) { Place(Dc.Kind, Ls, F, Dc.W); }
		}
		const TArray<DCFurnish::FLocal> Ls = { DCFurnish::FLocal::Space(4.f, 0.f, 4.f, 50.f, 198.f) };
		TArray<DCFurnish::FLocal> L2 = Ls; L2[0].Z0 = 138.f;
		const DCFurnish::FFrame F = Plan.Best(L2, WallF, [&](const DCFurnish::FFrame& C) { return Board.bValid ? -FV2::Distance(C.O, Board.O) * 0.1f : 0.f; }, 20.f);
		if (F.bValid) { Place(PScreen, L2, F, 0.f, 0.f, DCGroups::Level2); }
	}

	// ---------------------------------------------------------------- draw
	const FName Group0 = CurGroup;
	DCFurnish::FEmitFn Emit = [this](EDCShape S, const FVector& L, const FVector& Sz, FName M, const FRotator& R, bool bSh) { Deco(S, L, Sz, M, R, bSh); };
	auto PenAt = [&](const DCFurnish::FFrame& F) { DCFurnish::FPen P; P.Emit = &Emit; P.O = F.O; P.Yaw = F.Yaw; return P; };
	const FLinearColor Warm(1.f, 0.62f, 0.32f);
	for (const FPlaced& Pl : Out)
	{
		CurGroup = Pl.Group == FName("Blue") ? Group0 : Pl.Group;
		const DCFurnish::FPen P = PenAt(Pl.F);
		const int32 S = Seed + (int32)(Pl.F.O.X * 0.37f) + (int32)(Pl.F.O.Y * 0.71f);
		switch (Pl.Kind)
		{
		case PBoard: DrawBoard(P, Pl.A, BoardTop, S); break;
		case PEasel: DrawEaselBoard(P, Pl.A, S); break;
		case PTable: DrawPlanningTable(P, Pl.A, Pl.B, S); break;
		case PChair: DrawWoodChair(P, S); break;
		case PRug: DrawRug(P, Pl.A, Pl.B, Pl.Group == FName("Blue") ? FName("RugBlue") : FName("Rug"), "RugBorder"); break;
		case PStove:
		{
			DrawStove(P, FMath::Min(H - 50.f, 250.f), S);
			UPointLightComponent* L = Point(P.P(68.f, 0.f, 40.f), FLinearColor(1.f, 0.38f, 0.1f), 16.f, 380.f, false, true, 8.f);
			AddFlicker(L, 7.f, 0.35f);
			break;
		}
		case PLogs: DrawLogBasket(P, S); break;
		case PDesk:
		{
			const float DX = bDeskToWall ? DeskD * 0.5f + 2.f : 80.f + DeskD * 0.5f + 1.f, CX = bDeskToWall ? DeskD + 36.f : 50.f, Turn = bDeskToWall ? 180.f : 0.f;
			const DCFurnish::FPen DP = P.At(DX, 0.f, Turn);
			DrawCaptainChair(P.At(CX, J(S, 1) * 6.f, Turn + J(S, 2) * 18.f));
			DrawDesk(DP, DeskW, DeskD, S);
			Point(DP.P(DeskD * 0.5f * 0.37f, DeskW * 0.5f - 22.f, 100.f), Warm, 8.f, 300.f, false, true, 3.f);
			break;
		}
		case PSofa: DrawSofa(P, Pl.A, U(S, 3) > 0.5f ? FName("LeatherBrn") : FName("Velvet"), S); break;
		case PCoffee: DrawCoffeeTable(P, 100.f, 56.f, S); break;
		case PArmchair: DrawArmchair(P, U(S, 4) > 0.5f ? FName("Velvet") : FName("LeatherBrn"), S); break;
		case PLamp:
			DrawFloorLamp(P);
			Point(P.P(0.f, 0.f, 150.f), Warm, 12.f, 400.f, false, true, 10.f);
			break;
		case PBookcase: DrawBookcase(P, Pl.A, S); break;
		case PSideboard: DrawSideboard(P, Pl.A, S); break;
		case PFiling: DrawFilingCabinets(P, 2, S); break;
		case PSafe: DrawSafe(P); break;
		case PCrates: DrawCrates(P.At(32.f, 0.f), FMath::Max(1, (int32)Pl.A), S); break;
		case PDuffels: DrawDuffels(P.At(32.f, 0.f), S); break;
		case PPlant: DrawPlant(P.At(28.f, 0.f), S); break;
		case PCans: DrawJerrycans(P.At(22.f, 0.f), S); break;
		case PPapers: DrawNewspapers(P.At(26.f, 0.f), S); break;
		case PPictures: DrawPictures(P, Pl.A, S); break;
		case PClock: DrawClock(P); break;
		case PShotgun: DrawShotgunRack(P); break;
		case PShelf: DrawWallShelf(P, Pl.A, S); break;
		case PScreen: DrawWallScreen(P); break;
		default: break;
		}
	}
	CurGroup = Group0;

	// the room light
	if (LampEdge >= 0)
	{
		BracketLamp(Q[LampEdge], Q[(LampEdge + 1) & 3], LampT / Plan.E[LampEdge].L, LampZ, FLinearColor(1.f, 0.66f, 0.38f), 30.f, 760.f);
	}
	else
	{
		Point(FVector(Plan.Hub.X, Plan.Hub.Y, FMath::Min(H - 40.f, 230.f)), FLinearColor(1.f, 0.66f, 0.38f), 26.f, 760.f, true, true, 4.f);
	}

	// hand the layout to the room: pawns keep out of the furniture and use the spots
	for (const DCFurnish::FPart& P : Plan.Parts)
	{
		if (!P.bSolid || P.bRug || P.Z0 > 60.f || (P.Z1 > 5000.f && P.Box.H.X < 26.f)) { continue; } // skip the corner pier
		Room->Footprints.Add({ P.Box.C, P.Box.H, P.Box.Yaw });
	}
	Room->UseSpots = Spots;
	if (OutOmitWindows[1] || OutOmitWindows[2]) { UE_LOG(LogDC, Log, TEXT("[Scene] furnishing bricked up windows: edge1 %d edge2 %d"), OutOmitWindows[1], OutOmitWindows[2]); }
	{
		static const TCHAR* Names[] = { TEXT("board"), TEXT("easel"), TEXT("table"), TEXT("chair"), TEXT("rug"), TEXT("stove"), TEXT("logs"), TEXT("desk"), TEXT("sofa"), TEXT("coffee"),
			TEXT("armchair"), TEXT("lamp"), TEXT("bookcase"), TEXT("sideboard"), TEXT("filing"), TEXT("safe"), TEXT("crates"), TEXT("bags"), TEXT("plant"), TEXT("cans"), TEXT("papers"),
			TEXT("pictures"), TEXT("clock"), TEXT("shotgun"), TEXT("shelf"), TEXT("screen") };
		FString Got;
		for (const FPlaced& Pl : Out) { Got += (Pl.Kind >= 0 && Pl.Kind < UE_ARRAY_COUNT(Names)) ? Names[Pl.Kind] : TEXT("?"); Got += TEXT(" "); }
		UE_LOG(LogDC, Log, TEXT("[Scene] furnished: %s"), *Got);
	}
	UE_LOG(LogDC, Log, TEXT("[Scene] furnished %s: %d groups, %d footprints, %d use spots, coverage %.0f%%, %.2f ms"),
		DC::RoomName(Room->RoomType), Out.Num(), Room->Footprints.Num(), Spots.Num(), Plan.Coverage() * 100.f, (FPlatformTime::Seconds() - T0) * 1000.0);
	return true;
}

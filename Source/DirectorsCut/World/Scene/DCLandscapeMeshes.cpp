// Director's Cut — runtime landscape meshes. See DCLandscapeMeshes.h.
#include "World/Scene/DCLandscapeMeshes.h"
#include "Engine/StaticMesh.h"
#include "MeshDescription.h"
#include "StaticMeshAttributes.h"
#include "Math/RandomStream.h"

namespace
{
	/** Winding flip (set once by eye: rocks must not render inside-out). */
	constexpr bool bFrontIsNegativeCross = true;

	struct FBuilder
	{
		TArray<FVector3f> P;
		TArray<FVector3f> N;
		TArray<FLinearColor> C;
		TArray<int32> I;

		int32 V(const FVector3f& Pos, const FVector3f& Nrm, const FLinearColor& Col)
		{
			P.Add(Pos); N.Add(Nrm.GetSafeNormal()); C.Add(Col);
			return P.Num() - 1;
		}
		/** Triangle facing along the average vertex normal. */
		void Tri(int32 A, int32 B, int32 Cc)
		{
			const FVector3f X = FVector3f::CrossProduct(P[B] - P[A], P[Cc] - P[A]);
			const float D = FVector3f::DotProduct(X, N[A] + N[B] + N[Cc]);
			const bool bKeep = bFrontIsNegativeCross ? (D < 0.f) : (D > 0.f);
			if (bKeep) { I.Add(A); I.Add(B); I.Add(Cc); }
			else { I.Add(A); I.Add(Cc); I.Add(B); }
		}

		UStaticMesh* Build(UObject* Outer, const TCHAR* Name) const
		{
			FMeshDescription MD;
			FStaticMeshAttributes Attr(MD);
			Attr.Register();
			TVertexAttributesRef<FVector3f> VPos = Attr.GetVertexPositions();
			TVertexInstanceAttributesRef<FVector3f> VNrm = Attr.GetVertexInstanceNormals();
			TVertexInstanceAttributesRef<FVector3f> VTan = Attr.GetVertexInstanceTangents();
			TVertexInstanceAttributesRef<float> VSign = Attr.GetVertexInstanceBinormalSigns();
			TVertexInstanceAttributesRef<FVector4f> VCol = Attr.GetVertexInstanceColors();
			TVertexInstanceAttributesRef<FVector2f> VUV = Attr.GetVertexInstanceUVs();
			VUV.SetNumChannels(1);
			MD.ReserveNewVertices(P.Num());
			MD.ReserveNewVertexInstances(P.Num());
			MD.ReserveNewTriangles(I.Num() / 3);
			const FPolygonGroupID PG = MD.CreatePolygonGroup();
			Attr.GetPolygonGroupMaterialSlotNames()[PG] = FName("M0");
			TArray<FVertexInstanceID> VI; VI.Reserve(P.Num());
			for (int32 k = 0; k < P.Num(); ++k)
			{
				const FVertexID Vid = MD.CreateVertex();
				VPos[Vid] = P[k];
				const FVertexInstanceID Id = MD.CreateVertexInstance(Vid);
				const FVector3f Nn = N[k];
				FVector3f T = FVector3f::CrossProduct(FMath::Abs(Nn.Z) < 0.9f ? FVector3f::UpVector : FVector3f::ForwardVector, Nn).GetSafeNormal();
				VNrm[Id] = Nn;
				VTan[Id] = T;
				VSign[Id] = 1.f;
				VCol[Id] = FVector4f(C[k].R, C[k].G, C[k].B, C[k].A);
				VUV.Set(Id, 0, FVector2f(P[k].X * 0.01f, P[k].Y * 0.01f));
				VI.Add(Id);
			}
			for (int32 k = 0; k + 2 < I.Num(); k += 3)
			{
				const FVertexInstanceID Tri[3] = { VI[I[k]], VI[I[k + 1]], VI[I[k + 2]] };
				MD.CreateTriangle(PG, Tri);
			}
			UStaticMesh* SM = NewObject<UStaticMesh>(Outer, FName(Name));
			SM->GetStaticMaterials().Add(FStaticMaterial(nullptr, FName("M0"), FName("M0")));
			UStaticMesh::FBuildMeshDescriptionsParams Params;
			Params.bMarkPackageDirty = false;
			Params.bBuildSimpleCollision = false;
			Params.bCommitMeshDescription = false;
			Params.bFastBuild = true;
			TArray<const FMeshDescription*> List; List.Add(&MD);
			SM->BuildFromMeshDescriptions(List, Params);
			return SM;
		}
	};

	float Hash3(const FVector3f& V, int32 Seed)
	{
		const float D = V.X * 12.9898f + V.Y * 78.233f + V.Z * 37.719f + Seed * 3.17f;
		return FMath::Frac(FMath::Sin(D) * 43758.5453f);
	}
	/** Smooth value noise on the unit sphere (a few octaves of hashed lattice noise). */
	float Noise3(const FVector3f& X, int32 Seed)
	{
		const FVector3f F(FMath::Floor(X.X), FMath::Floor(X.Y), FMath::Floor(X.Z));
		FVector3f T = X - F;
		T = T * T * (FVector3f(3.f) - 2.f * T);
		auto H = [&](float a, float b, float c) { return Hash3(F + FVector3f(a, b, c), Seed); };
		const float X00 = FMath::Lerp(H(0, 0, 0), H(1, 0, 0), T.X), X10 = FMath::Lerp(H(0, 1, 0), H(1, 1, 0), T.X);
		const float X01 = FMath::Lerp(H(0, 0, 1), H(1, 0, 1), T.X), X11 = FMath::Lerp(H(0, 1, 1), H(1, 1, 1), T.X);
		return FMath::Lerp(FMath::Lerp(X00, X10, T.Y), FMath::Lerp(X01, X11, T.Y), T.Z);
	}

	void Icosphere(int32 Subdiv, TArray<FVector3f>& OutV, TArray<FIntVector>& OutT)
	{
		const float t = (1.f + FMath::Sqrt(5.f)) * 0.5f;
		OutV = { {-1, t, 0}, {1, t, 0}, {-1, -t, 0}, {1, -t, 0}, {0, -1, t}, {0, 1, t}, {0, -1, -t}, {0, 1, -t}, {t, 0, -1}, {t, 0, 1}, {-t, 0, -1}, {-t, 0, 1} };
		for (FVector3f& V : OutV) { V.Normalize(); }
		OutT = { {0,11,5},{0,5,1},{0,1,7},{0,7,10},{0,10,11},{1,5,9},{5,11,4},{11,10,2},{10,7,6},{7,1,8},
		         {3,9,4},{3,4,2},{3,2,6},{3,6,8},{3,8,9},{4,9,5},{2,4,11},{6,2,10},{8,6,7},{9,8,1} };
		for (int32 s = 0; s < Subdiv; ++s)
		{
			TMap<uint64, int32> Mid;
			auto M = [&](int32 a, int32 b)
			{
				const uint64 K = ((uint64)FMath::Min(a, b) << 32) | (uint64)FMath::Max(a, b);
				if (const int32* F = Mid.Find(K)) { return *F; }
				const int32 Id = OutV.Add(((OutV[a] + OutV[b]) * 0.5f).GetSafeNormal());
				Mid.Add(K, Id);
				return Id;
			};
			TArray<FIntVector> NT;
			for (const FIntVector& T : OutT)
			{
				const int32 a = M(T.X, T.Y), b = M(T.Y, T.Z), c = M(T.Z, T.X);
				NT.Add({ T.X, a, c }); NT.Add({ T.Y, b, a }); NT.Add({ T.Z, c, b }); NT.Add({ a, b, c });
			}
			OutT = MoveTemp(NT);
		}
	}
}

namespace DCLandMesh
{
	UStaticMesh* GrassTuft(UObject* Outer, int32 Blades, float Lean, int32 Seed)
	{
		FRandomStream R(Seed);
		FBuilder B;
		constexpr int32 Segs = 3;
		for (int32 b = 0; b < Blades; ++b)
		{
			const float Ang = R.FRandRange(0.f, 2.f * PI);
			const FVector3f Out(FMath::Cos(Ang), FMath::Sin(Ang), 0.f);
			const FVector3f Side(-Out.Y, Out.X, 0.f);
			const FVector3f Base = Out * R.FRandRange(0.f, 9.f);
			const float H = R.FRandRange(55.f, 100.f);
			const float W = R.FRandRange(4.5f, 7.f);
			const float L = Lean * R.FRandRange(0.4f, 1.1f) * H;
			int32 Prev[2] = { -1, -1 };
			for (int32 s = 0; s <= Segs; ++s)
			{
				const float u = (float)s / Segs;
				const FVector3f C = Base + Out * (L * u * u) + FVector3f(0, 0, H * u * (1.f - 0.25f * Lean * u));
				// soft normals: mostly up so tufts shade like the ground they stand on
				const FVector3f Nrm = (FVector3f::UpVector * 0.75f + Out * 0.25f);
				const FLinearColor Col(u, u * u, 0.f, 1.f);
				if (s == Segs)
				{
					const int32 Tip = B.V(C, Nrm, Col);
					B.Tri(Prev[0], Prev[1], Tip);
				}
				else
				{
					const float w = W * (1.f - u * 0.85f);
					const int32 a = B.V(C - Side * w, Nrm, Col), c = B.V(C + Side * w, Nrm, Col);
					if (Prev[0] >= 0) { B.Tri(Prev[0], Prev[1], c); B.Tri(Prev[0], c, a); }
					Prev[0] = a; Prev[1] = c;
				}
			}
		}
		return B.Build(Outer, *FString::Printf(TEXT("DCGrassTuft_%d"), Seed));
	}

	UStaticMesh* FlowerHeads(UObject* Outer, int32 Seed)
	{
		FRandomStream R(Seed);
		FBuilder B;
		const int32 Heads = 5;
		for (int32 h = 0; h < Heads; ++h)
		{
			const float Ang = R.FRandRange(0.f, 2.f * PI);
			const FVector3f C(FMath::Cos(Ang) * R.FRandRange(2.f, 14.f), FMath::Sin(Ang) * R.FRandRange(2.f, 14.f), R.FRandRange(30.f, 62.f));
			const float S = R.FRandRange(3.5f, 5.5f);
			const FLinearColor Col(1.f, 1.f, 0.f, 1.f);
			// flat-ish octahedron: wide petals, short height
			const FVector3f Ax[6] = { {S,0,0}, {-S,0,0}, {0,S,0}, {0,-S,0}, {0,0,S * 0.55f}, {0,0,-S * 0.35f} };
			const int32 F[8][3] = { {0,2,4},{2,1,4},{1,3,4},{3,0,4},{2,0,5},{1,2,5},{3,1,5},{0,3,5} };
			for (const auto& f : F)
			{
				const FVector3f Nrm = (Ax[f[0]] + Ax[f[1]] + Ax[f[2]]).GetSafeNormal() * 0.5f + FVector3f::UpVector * 0.5f;
				const int32 a = B.V(C + Ax[f[0]], Nrm, Col), b = B.V(C + Ax[f[1]], Nrm, Col), c = B.V(C + Ax[f[2]], Nrm, Col);
				B.Tri(a, b, c);
			}
		}
		return B.Build(Outer, *FString::Printf(TEXT("DCFlowers_%d"), Seed));
	}

	UStaticMesh* Blob(UObject* Outer, int32 Subdiv, float Rough, bool bFlat, int32 Seed, float Wind)
	{
		TArray<FVector3f> Vs; TArray<FIntVector> Ts;
		Icosphere(Subdiv, Vs, Ts);
		TArray<FVector3f> Disp; Disp.SetNum(Vs.Num());
		for (int32 k = 0; k < Vs.Num(); ++k)
		{
			const FVector3f& D = Vs[k];
			const float n = Noise3(D * 1.7f, Seed) * 0.6f + Noise3(D * 3.9f + FVector3f(5.f), Seed) * 0.3f + Noise3(D * 8.3f, Seed + 3) * 0.1f;
			Disp[k] = D * 50.f * (1.f + Rough * (n * 2.f - 1.f));
		}
		FBuilder B;
		auto ColAt = [&](const FVector3f& D) { const float h = FMath::Clamp(D.Z * 0.5f + 0.5f, 0.f, 1.f); return FLinearColor(0.35f + 0.65f * h, Wind * h, 0.f, 1.f); };
		if (bFlat)
		{
			for (const FIntVector& T : Ts)
			{
				const FVector3f A = Disp[T.X], Bb = Disp[T.Y], C = Disp[T.Z];
				FVector3f Nrm = FVector3f::CrossProduct(Bb - A, C - A).GetSafeNormal();
				if (FVector3f::DotProduct(Nrm, A + Bb + C) < 0.f) { Nrm = -Nrm; }
				const int32 a = B.V(A, Nrm, ColAt(Vs[T.X])), b = B.V(Bb, Nrm, ColAt(Vs[T.Y])), c = B.V(C, Nrm, ColAt(Vs[T.Z]));
				B.Tri(a, b, c);
			}
		}
		else
		{
			// smooth: shared vertices, normals = blend of sphere direction and displaced shape (keeps clumps soft)
			TArray<FVector3f> Acc; Acc.Init(FVector3f::ZeroVector, Vs.Num());
			for (const FIntVector& T : Ts)
			{
				FVector3f Nrm = FVector3f::CrossProduct(Disp[T.Y] - Disp[T.X], Disp[T.Z] - Disp[T.X]);
				if (FVector3f::DotProduct(Nrm, Disp[T.X]) < 0.f) { Nrm = -Nrm; }
				Acc[T.X] += Nrm; Acc[T.Y] += Nrm; Acc[T.Z] += Nrm;
			}
			for (int32 k = 0; k < Vs.Num(); ++k) { B.V(Disp[k], Acc[k].GetSafeNormal() * 0.5f + Vs[k] * 0.5f, ColAt(Vs[k])); }
			for (const FIntVector& T : Ts) { B.Tri(T.X, T.Y, T.Z); }
		}
		return B.Build(Outer, *FString::Printf(TEXT("DCBlob_%d_%d_%d"), Subdiv, Seed, bFlat ? 1 : 0));
	}

	UStaticMesh* Tube(UObject* Outer, int32 Sides, float TopScale)
	{
		FBuilder B;
		const FLinearColor Lo(0.4f, 0.f, 0.f, 1.f), Hi(1.f, 0.f, 0.f, 1.f);
		for (int32 s = 0; s < Sides; ++s)
		{
			const float A0 = 2.f * PI * s / Sides, A1 = 2.f * PI * (s + 1) / Sides;
			const FVector3f D0(FMath::Cos(A0), FMath::Sin(A0), 0.f), D1(FMath::Cos(A1), FMath::Sin(A1), 0.f);
			const int32 a = B.V(D0 * 50.f, D0, Lo), b = B.V(D1 * 50.f, D1, Lo);
			const int32 c = B.V(D1 * 50.f * TopScale + FVector3f(0, 0, 100), D1, Hi), d = B.V(D0 * 50.f * TopScale + FVector3f(0, 0, 100), D0, Hi);
			B.Tri(a, b, c); B.Tri(a, c, d);
			// top cap
			const int32 t0 = B.V(FVector3f(0, 0, 100), FVector3f::UpVector, Hi), t1 = B.V(D0 * 50.f * TopScale + FVector3f(0, 0, 100), FVector3f::UpVector, Hi), t2 = B.V(D1 * 50.f * TopScale + FVector3f(0, 0, 100), FVector3f::UpVector, Hi);
			B.Tri(t0, t1, t2);
		}
		return B.Build(Outer, *FString::Printf(TEXT("DCTube_%d_%d"), Sides, FMath::RoundToInt(TopScale * 100.f)));
	}

	UStaticMesh* Skirt(UObject* Outer, int32 Sides, int32 Seed)
	{
		FRandomStream R(Seed);
		FBuilder B;
		const FVector3f Top(0, 0, 100);
		TArray<FVector3f> Rim;
		for (int32 s = 0; s < Sides; ++s)
		{
			const float A = 2.f * PI * s / Sides + R.FRandRange(-0.1f, 0.1f);
			Rim.Add(FVector3f(FMath::Cos(A), FMath::Sin(A), 0.f) * 50.f * R.FRandRange(0.85f, 1.1f) + FVector3f(0, 0, R.FRandRange(-6.f, 6.f)));
		}
		for (int32 s = 0; s < Sides; ++s)
		{
			const FVector3f& P0 = Rim[s]; const FVector3f& P1 = Rim[(s + 1) % Sides];
			const FVector3f Mid = (P0 + P1) * 0.5f;
			const FVector3f Nrm = (FVector3f(Mid.X, Mid.Y, 0.f).GetSafeNormal() + FVector3f(0, 0, 0.9f));
			const int32 a = B.V(P0, Nrm, FLinearColor(0.55f, 0.6f, 0.f, 1.f)), b = B.V(P1, Nrm, FLinearColor(0.55f, 0.6f, 0.f, 1.f));
			const int32 t = B.V(Top, FVector3f::UpVector, FLinearColor(1.f, 0.2f, 0.f, 1.f));
			B.Tri(a, b, t);
			// underside (dark)
			const int32 c = B.V(P0, -FVector3f::UpVector, FLinearColor(0.15f, 0.6f, 0.f, 1.f)), d = B.V(P1, -FVector3f::UpVector, FLinearColor(0.15f, 0.6f, 0.f, 1.f));
			const int32 u = B.V(FVector3f(0, 0, 30), -FVector3f::UpVector, FLinearColor(0.1f, 0.f, 0.f, 1.f));
			B.Tri(c, d, u);
		}
		return B.Build(Outer, *FString::Printf(TEXT("DCSkirt_%d"), Seed));
	}

	UStaticMesh* Disc(UObject* Outer, int32 Sides, float Wobble, int32 Seed)
	{
		FRandomStream R(Seed);
		FBuilder B;
		const FLinearColor Col(1.f, 0.f, 0.f, 1.f);
		const int32 C = B.V(FVector3f::ZeroVector, FVector3f::UpVector, Col);
		TArray<int32> Rim;
		const float Ph1 = R.FRandRange(0.f, 6.f), Ph2 = R.FRandRange(0.f, 6.f);
		for (int32 s = 0; s < Sides; ++s)
		{
			const float A = 2.f * PI * s / Sides;
			const float Rad = 50.f * (1.f + Wobble * (0.6f * FMath::Sin(A * 2.f + Ph1) + 0.4f * FMath::Sin(A * 5.f + Ph2)));
			Rim.Add(B.V(FVector3f(FMath::Cos(A) * Rad, FMath::Sin(A) * Rad, 0.f), FVector3f::UpVector, Col));
		}
		for (int32 s = 0; s < Sides; ++s) { B.Tri(C, Rim[s], Rim[(s + 1) % Sides]); }
		return B.Build(Outer, *FString::Printf(TEXT("DCDisc_%d_%d"), Sides, Seed));
	}

	UStaticMesh* Prism(UObject* Outer, const TArray<FVector2D>& Poly, float Z0, float Z1)
	{
		FBuilder B;
		const FLinearColor Col(1.f, 0.f, 0.f, 1.f);
		const int32 N = Poly.Num();
		if (N < 3) { return nullptr; }
		FVector2D Mid = FVector2D::ZeroVector;
		for (const FVector2D& P : Poly) { Mid += P; }
		Mid /= N;
		// top fan
		const int32 C = B.V(FVector3f(Mid.X, Mid.Y, Z1), FVector3f::UpVector, Col);
		TArray<int32> Top;
		for (const FVector2D& P : Poly) { Top.Add(B.V(FVector3f(P.X, P.Y, Z1), FVector3f::UpVector, Col)); }
		for (int32 k = 0; k < N; ++k) { B.Tri(C, Top[k], Top[(k + 1) % N]); }
		// sides (flat shaded)
		for (int32 k = 0; k < N; ++k)
		{
			const FVector2D A = Poly[k], D = Poly[(k + 1) % N];
			const FVector2D E = (D - A).GetSafeNormal();
			const FVector3f Nrm(E.Y, -E.X, 0.f); // outward for a counter-clockwise outline
			const int32 a0 = B.V(FVector3f(A.X, A.Y, Z0), Nrm, Col), a1 = B.V(FVector3f(A.X, A.Y, Z1), Nrm, Col);
			const int32 d0 = B.V(FVector3f(D.X, D.Y, Z0), Nrm, Col), d1 = B.V(FVector3f(D.X, D.Y, Z1), Nrm, Col);
			B.Tri(a0, d0, d1);
			B.Tri(a0, d1, a1);
		}
		static int32 Counter = 0;
		return B.Build(Outer, *MakeUniqueObjectName(Outer, UStaticMesh::StaticClass(), FName(*FString::Printf(TEXT("DCPrism_%d"), ++Counter))).ToString());
	}
}

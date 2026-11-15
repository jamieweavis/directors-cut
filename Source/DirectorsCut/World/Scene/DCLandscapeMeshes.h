// Director's Cut — tiny runtime-generated meshes for the rural landscape (grass tufts, foliage clumps, rocks, trunks,
// discs). Built once per world from FMeshDescription (fast build path, no assets on disk). Vertex colour channels:
// R = height / ambient occlusion (0 base .. 1 top), G = wind weight (0 rigid .. 1 free tip). See M_DC_Foliage.
#pragma once

#include "CoreMinimal.h"

class UStaticMesh;
class UMaterialInterface;

namespace DCLandMesh
{
	/** Clump of curved grass blades, ~1 m tall at scale 1 (scale per instance). */
	UStaticMesh* GrassTuft(UObject* Outer, int32 Blades, float Lean, int32 Seed);
	/** Little cluster of flower heads on invisible stems (heads only, around 25..60 cm high). */
	UStaticMesh* FlowerHeads(UObject* Outer, int32 Seed);
	/** Lumpy sphere of radius 50 (icosphere, noise displaced). Rough = displacement amount (0.1 soft clump, 0.35 rock).
	 *  bFlat = faceted normals (rocks). Vertex R darkens the underside. */
	UStaticMesh* Blob(UObject* Outer, int32 Subdiv, float Rough, bool bFlat, int32 Seed, float Wind);
	/** Tapered n-gon tube from z=0 to z=100, base radius 50, top radius 50*TopScale. */
	UStaticMesh* Tube(UObject* Outer, int32 Sides, float TopScale);
	/** Drooping cone skirt (pine tier): radius 50 at z=0 rising to a point at z=100. */
	UStaticMesh* Skirt(UObject* Outer, int32 Sides, int32 Seed);
	/** Flat irregular disc of radius ~50 at z=0 (top face only), wobble 0..0.3. */
	UStaticMesh* Disc(UObject* Outer, int32 Sides, float Wobble, int32 Seed);
	/** Extruded polygon (counter-clockwise outline, cm): top face at Z1 + side walls down to Z0. No bottom. */
	UStaticMesh* Prism(UObject* Outer, const TArray<FVector2D>& Poly, float Z0, float Z1);

	/** One cross-section of a Loft: a rounded rectangle in the YZ plane at X, from Z0 up to Z1, half-width W0 at the
	 *  bottom and W1 at the top (sides lean in when W1 < W0), corner radius R. Flags apply to the span from this section
	 *  to the next one. Bevel = how far (cm) the corner between the two neighbouring spans is rounded off (0 = sharp,
	 *  section used as-is). Y0 / Y1 shift the centre line at the bottom / top (leaning slabs). */
	struct FLoftSection
	{
		float X = 0.f, Z0 = 0.f, Z1 = 0.f, W0 = 0.f, W1 = 0.f, R = 0.f;
		uint8 Flags = 0;
		float Bevel = 6.f;
		float Y0 = 0.f, Y1 = 0.f;
	};
	enum : uint8
	{
		LoftTopAlt = 1,  // the flat top of this span goes in material slot 1 (windscreens)
		LoftSideAlt = 2, // the flat sides of this span go in material slot 1 (side windows)
	};
	/** Smooth skin through the sections (X all ascending or all descending), closed at both ends. Rounded corners and
	 *  everything else use material slot 0; spans flagged above put their flat faces in slot 1 (only present if used). */
	UStaticMesh* Loft(UObject* Outer, const TArray<FLoftSection>& Sections, int32 CornerSegs = 4);
	/** Box centred on the origin with rounded long edges (Radius, around X) and softened ends (EndRound, default Radius). */
	UStaticMesh* RoundBox(UObject* Outer, const FVector& Size, float Radius, float EndRound = -1.f);
}

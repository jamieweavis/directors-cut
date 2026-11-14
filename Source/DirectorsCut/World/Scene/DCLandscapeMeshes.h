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
}

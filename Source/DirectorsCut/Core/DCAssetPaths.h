// Director's Cut — soft paths for every asset code references (LEAD-OWNED contract).
// Code MUST load these softly and fall back to engine primitives if missing.
#pragma once

#include "CoreMinimal.h"

namespace DCPaths
{
	// Maps
	inline const TCHAR* HideoutMap = TEXT("/Game/DC/Maps/L_Hideout");

	// Engine primitives (always present)
	inline const TCHAR* Cube     = TEXT("/Engine/BasicShapes/Cube.Cube");           // 100uu cube, pivot centre
	inline const TCHAR* Sphere   = TEXT("/Engine/BasicShapes/Sphere.Sphere");
	inline const TCHAR* Cylinder = TEXT("/Engine/BasicShapes/Cylinder.Cylinder");
	inline const TCHAR* Cone     = TEXT("/Engine/BasicShapes/Cone.Cone");
	inline const TCHAR* Plane    = TEXT("/Engine/BasicShapes/Plane.Plane");
	inline const TCHAR* BasicShapeMaterial = TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"); // vector param "Color"

	// Project master materials (created in Wave 0 by Tools/create_materials.py)
	//   M_DC_Surface : opaque lit. Params: BaseColor(vec), Roughness, Metallic, Specular, EmissiveColor(vec), EmissiveStrength
	//   M_DC_Emissive: opaque unlit-look emissive. Params: Color(vec), Strength
	//   M_DC_Glass   : translucent. Params: Color(vec), Opacity
	inline const TCHAR* MatSurface  = TEXT("/Game/DC/Materials/M_DC_Surface.M_DC_Surface");
	inline const TCHAR* MatEmissive = TEXT("/Game/DC/Materials/M_DC_Emissive.M_DC_Emissive");
	inline const TCHAR* MatGlass    = TEXT("/Game/DC/Materials/M_DC_Glass.M_DC_Glass");
	//   M_DC_Highlight: translucent ADDITIVE fresnel glow for UMeshComponent::SetOverlayMaterial (hover). Params: Color(vec), Strength
	//   (created by Agent D in Wave 1; until merged, fall back to M_DC_Glass with low Opacity)
	inline const TCHAR* MatHighlight = TEXT("/Game/DC/Materials/M_DC_Highlight.M_DC_Highlight");

	// Characters (copied from the UE template; references are internal to /Game/Characters/Mannequins)
	inline const TCHAR* MannyMesh     = TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple");
	inline const TCHAR* QuinnMesh     = TEXT("/Game/Characters/Mannequins/Meshes/SKM_Quinn_Simple.SKM_Quinn_Simple"); // same skeleton
	inline const TCHAR* UnarmedAnimBP = TEXT("/Game/Characters/Mannequins/Anims/Unarmed/ABP_Unarmed.ABP_Unarmed_C");
	inline const TCHAR* AnimIdle      = TEXT("/Game/Characters/Mannequins/Anims/Unarmed/MM_Idle.MM_Idle");
	inline const TCHAR* AnimWalkFwd   = TEXT("/Game/Characters/Mannequins/Anims/Unarmed/Walk/MF_Unarmed_Walk_Fwd.MF_Unarmed_Walk_Fwd");
	// Crew body (Tools/crew_import.py from Art/Crew). Posed from the hidden mannequin by UDCCrewBodyComponent.
	//   M_DC_CrewSkin: textured. Params: BaseTex, NormalTex, Tint(vec), Roughness, AccentColor(vec), RimStrength
	//   Each body has an MI_Crew_<Name> of it with its own textures. Crew pick a body by id (CrewBodies[Id % Num]);
	//   female crew pick from CrewBodiesFemale, which are built for (and driven by) the hidden Quinn mannequin.
	struct FCrewBody { const TCHAR* Mesh; const TCHAR* Skin; };
	inline const FCrewBody CrewBodies[] = {
		{ TEXT("/Game/DC/Characters/Crew/SK_Crew_Hooded.SK_Crew_Hooded"), TEXT("/Game/DC/Characters/Crew/MI_Crew_Hooded.MI_Crew_Hooded") },
		{ TEXT("/Game/DC/Characters/Crew/SK_Crew_Criminal.SK_Crew_Criminal"), TEXT("/Game/DC/Characters/Crew/MI_Crew_Criminal.MI_Crew_Criminal") },
		{ TEXT("/Game/DC/Characters/Crew/SK_Crew_Hoodie.SK_Crew_Hoodie"), TEXT("/Game/DC/Characters/Crew/MI_Crew_Hoodie.MI_Crew_Hoodie") },
		{ TEXT("/Game/DC/Characters/Crew/SK_Crew_LeatherJacket.SK_Crew_LeatherJacket"), TEXT("/Game/DC/Characters/Crew/MI_Crew_LeatherJacket.MI_Crew_LeatherJacket") },
		{ TEXT("/Game/DC/Characters/Crew/SK_Crew_Tracksuit.SK_Crew_Tracksuit"), TEXT("/Game/DC/Characters/Crew/MI_Crew_Tracksuit.MI_Crew_Tracksuit") },
		{ TEXT("/Game/DC/Characters/Crew/SK_Crew_Casual.SK_Crew_Casual"), TEXT("/Game/DC/Characters/Crew/MI_Crew_Casual.MI_Crew_Casual") },
		{ TEXT("/Game/DC/Characters/Crew/SK_Crew_NavySweats.SK_Crew_NavySweats"), TEXT("/Game/DC/Characters/Crew/MI_Crew_NavySweats.MI_Crew_NavySweats") },
		{ TEXT("/Game/DC/Characters/Crew/SK_Crew_Businessman.SK_Crew_Businessman"), TEXT("/Game/DC/Characters/Crew/MI_Crew_Businessman.MI_Crew_Businessman") },
		{ TEXT("/Game/DC/Characters/Crew/SK_Crew_BrownJacket.SK_Crew_BrownJacket"), TEXT("/Game/DC/Characters/Crew/MI_Crew_BrownJacket.MI_Crew_BrownJacket") },
		{ TEXT("/Game/DC/Characters/Crew/SK_Crew_Shearling.SK_Crew_Shearling"), TEXT("/Game/DC/Characters/Crew/MI_Crew_Shearling.MI_Crew_Shearling") },
	};
	inline const FCrewBody CrewBodiesFemale[] = {
		{ TEXT("/Game/DC/Characters/Crew/SK_Crew_RedTop.SK_Crew_RedTop"), TEXT("/Game/DC/Characters/Crew/MI_Crew_RedTop.MI_Crew_RedTop") },
		{ TEXT("/Game/DC/Characters/Crew/SK_Crew_Blouse.SK_Crew_Blouse"), TEXT("/Game/DC/Characters/Crew/MI_Crew_Blouse.MI_Crew_Blouse") },
		{ TEXT("/Game/DC/Characters/Crew/SK_Crew_Puffer.SK_Crew_Puffer"), TEXT("/Game/DC/Characters/Crew/MI_Crew_Puffer.MI_Crew_Puffer") },
		{ TEXT("/Game/DC/Characters/Crew/SK_Crew_PlumJacket.SK_Crew_PlumJacket"), TEXT("/Game/DC/Characters/Crew/MI_Crew_PlumJacket.MI_Crew_PlumJacket") },
		{ TEXT("/Game/DC/Characters/Crew/SK_Crew_Businesswoman.SK_Crew_Businesswoman"), TEXT("/Game/DC/Characters/Crew/MI_Crew_Businesswoman.MI_Crew_Businesswoman") },
		{ TEXT("/Game/DC/Characters/Crew/SK_Crew_Denim.SK_Crew_Denim"), TEXT("/Game/DC/Characters/Crew/MI_Crew_Denim.MI_Crew_Denim") },
		{ TEXT("/Game/DC/Characters/Crew/SK_Crew_Streetwear.SK_Crew_Streetwear"), TEXT("/Game/DC/Characters/Crew/MI_Crew_Streetwear.MI_Crew_Streetwear") },
		{ TEXT("/Game/DC/Characters/Crew/SK_Crew_Blazer.SK_Crew_Blazer"), TEXT("/Game/DC/Characters/Crew/MI_Crew_Blazer.MI_Crew_Blazer") },
		{ TEXT("/Game/DC/Characters/Crew/SK_Crew_TankTop.SK_Crew_TankTop"), TEXT("/Game/DC/Characters/Crew/MI_Crew_TankTop.MI_Crew_TankTop") },
		{ TEXT("/Game/DC/Characters/Crew/SK_Crew_CreamKnit.SK_Crew_CreamKnit"), TEXT("/Game/DC/Characters/Crew/MI_Crew_CreamKnit.MI_Crew_CreamKnit") },
	};
	inline const TCHAR* MatCrewSkin   = TEXT("/Game/DC/Materials/M_DC_CrewSkin.M_DC_CrewSkin");

	// Audio (imported later; Audio subsystem falls back to runtime WAV loading from Content/RawAudio)
	inline const TCHAR* AudioRoot = TEXT("/Game/DC/Audio");

	// Raw (non-asset) content directories, relative to FPaths::ProjectContentDir(). Staged as non-UFS files.
	inline const TCHAR* DataDir     = TEXT("Data");      // *.json
	inline const TCHAR* FontsDir    = TEXT("Fonts");     // *.ttf
	inline const TCHAR* RawAudioDir = TEXT("RawAudio");  // *.wav + manifest.json
}

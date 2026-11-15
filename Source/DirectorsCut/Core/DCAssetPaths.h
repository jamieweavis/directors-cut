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
	//   M_DC_Criminal: dark matte body + fresnel rim accent. Params: BaseColor(vec), AccentColor(vec), RimStrength, Roughness
	//   M_DC_Glass   : translucent. Params: Color(vec), Opacity
	inline const TCHAR* MatSurface  = TEXT("/Game/DC/Materials/M_DC_Surface.M_DC_Surface");
	inline const TCHAR* MatEmissive = TEXT("/Game/DC/Materials/M_DC_Emissive.M_DC_Emissive");
	inline const TCHAR* MatCriminal = TEXT("/Game/DC/Materials/M_DC_Criminal.M_DC_Criminal");
	inline const TCHAR* MatGlass    = TEXT("/Game/DC/Materials/M_DC_Glass.M_DC_Glass");
	//   M_DC_Highlight: translucent ADDITIVE fresnel glow for UMeshComponent::SetOverlayMaterial (hover). Params: Color(vec), Strength
	//   (created by Agent D in Wave 1; until merged, fall back to M_DC_Glass with low Opacity)
	inline const TCHAR* MatHighlight = TEXT("/Game/DC/Materials/M_DC_Highlight.M_DC_Highlight");

	// Characters (copied from the UE template; references are internal to /Game/Characters/Mannequins)
	inline const TCHAR* MannyMesh     = TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple");
	inline const TCHAR* QuinnMesh     = TEXT("/Game/Characters/Mannequins/Meshes/SKM_Quinn_Simple.SKM_Quinn_Simple");
	inline const TCHAR* UnarmedAnimBP = TEXT("/Game/Characters/Mannequins/Anims/Unarmed/ABP_Unarmed.ABP_Unarmed_C");
	inline const TCHAR* AnimIdle      = TEXT("/Game/Characters/Mannequins/Anims/Unarmed/MM_Idle.MM_Idle");
	inline const TCHAR* AnimWalkFwd   = TEXT("/Game/Characters/Mannequins/Anims/Unarmed/Walk/MF_Unarmed_Walk_Fwd.MF_Unarmed_Walk_Fwd");
	// Crew body (Tools/crew_import.py from Art/Crew). Posed from the hidden mannequin by UDCCrewBodyComponent.
	//   M_DC_CrewSkin: textured. Params: Tint(vec), Roughness, AccentColor(vec), RimStrength
	inline const TCHAR* CrewBodyMesh  = TEXT("/Game/DC/Characters/Crew/SK_Crew_Hooded.SK_Crew_Hooded");
	inline const TCHAR* MatCrewSkin   = TEXT("/Game/DC/Materials/M_DC_CrewSkin.M_DC_CrewSkin");

	// Audio (imported later; Audio subsystem falls back to runtime WAV loading from Content/RawAudio)
	inline const TCHAR* AudioRoot = TEXT("/Game/DC/Audio");

	// Raw (non-asset) content directories, relative to FPaths::ProjectContentDir(). Staged as non-UFS files.
	inline const TCHAR* DataDir     = TEXT("Data");      // *.json
	inline const TCHAR* FontsDir    = TEXT("Fonts");     // *.ttf
	inline const TCHAR* RawAudioDir = TEXT("RawAudio");  // *.wav + manifest.json
}

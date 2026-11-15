// Director's Cut — the stylised "toybox diorama" look, in one place.
//   Tone()  : palette remap applied to every authored surface colour as its material is created (cleaner, brighter,
//             more saturated; olive greens pulled to a fresher green). Emissive colours are never toned.
//   Dirt()  : scales the procedural grime so surfaces read as clean painted shapes.
//   Toon*() : the ink-outline + cel-band post process (M_DC_Toon, built by Tools/style_materials.py).
// Dev flags:  -DCToon=0|1 force the outline pass off/on    -DCTone=0 raw authored palette (before/after review)
#pragma once

#include "CoreMinimal.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Core/DCSettings.h"

namespace DCStyle
{
	inline const TCHAR* ToonMaterialPath = TEXT("/Game/DC/Materials/M_DC_Toon.M_DC_Toon");

	inline bool ToneEnabled()
	{
		static const bool b = [] { int32 V = 1; FParse::Value(FCommandLine::Get(), TEXT("DCTone="), V); return V != 0; }();
		return b;
	}

	/** Ink outlines + cel banding. Player setting "ToonOutlines" (default on); -DCToon= overrides. */
	inline bool ToonEnabled()
	{
		int32 V = -1;
		if (FParse::Value(FCommandLine::Get(), TEXT("DCToon="), V)) { return V != 0; }
		return DCSettings::GetBool(TEXT("ToonOutlines"), true);
	}

	inline FLinearColor Tone(const FLinearColor& In)
	{
		if (!ToneEnabled()) { return In; }
		FLinearColor H = In.LinearRGBToHSV(); // R = hue 0..360, G = saturation, B = value
		const bool bGreen = H.G > 0.2f && H.R > 50.f && H.R < 130.f;
		if (bGreen) { H.R = FMath::Lerp(H.R, 104.f, 0.25f); } // olive -> fresher green, keeping the meadow's variation
		H.G = bGreen ? H.G * 0.86f : FMath::Clamp(FMath::Pow(H.G, 0.9f) * 1.04f, 0.f, 1.f); // greens fill the screen: keep them calm
		H.B = FMath::Pow(FMath::Max(H.B, 0.f), 0.88f); // lift the darks, leave the lights
		FLinearColor Out = H.HSVToLinearRGB();
		Out.A = In.A;
		return Out;
	}

	inline float Dirt(float D) { return ToneEnabled() ? D * 0.4f : D; }
}

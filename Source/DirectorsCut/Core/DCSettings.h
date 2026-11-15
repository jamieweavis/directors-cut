// Director's Cut — player settings readable from any module (v3 options screen). LEAD-OWNED contract; the UI agent
// implements the options screen and may ADD keys here. Values persist in GGameUserSettingsIni under [DirectorsCut.Settings].
// Graphics settings go through UGameUserSettings directly; audio volumes through UDCAudioSubsystem.
#pragma once

#include "CoreMinimal.h"
#include "Misc/ConfigCacheIni.h"

namespace DCSettings
{
	inline const TCHAR* Section() { return TEXT("DirectorsCut.Settings"); }

	inline bool GetBool(const TCHAR* Key, bool Default)
	{
		bool V = Default;
		if (GConfig) { GConfig->GetBool(Section(), Key, V, GGameUserSettingsIni); }
		return V;
	}
	inline float GetFloat(const TCHAR* Key, float Default)
	{
		float V = Default;
		if (GConfig) { GConfig->GetFloat(Section(), Key, V, GGameUserSettingsIni); }
		return V;
	}
	inline int32 GetInt(const TCHAR* Key, int32 Default)
	{
		int32 V = Default;
		if (GConfig) { GConfig->GetInt(Section(), Key, V, GGameUserSettingsIni); }
		return V;
	}
	inline void SetBool(const TCHAR* Key, bool V)    { if (GConfig) { GConfig->SetBool(Section(), Key, V, GGameUserSettingsIni); GConfig->Flush(false, GGameUserSettingsIni); } }
	inline void SetFloat(const TCHAR* Key, float V)  { if (GConfig) { GConfig->SetFloat(Section(), Key, V, GGameUserSettingsIni); GConfig->Flush(false, GGameUserSettingsIni); } }
	inline void SetInt(const TCHAR* Key, int32 V)    { if (GConfig) { GConfig->SetInt(Section(), Key, V, GGameUserSettingsIni); GConfig->Flush(false, GGameUserSettingsIni); } }

	// Accessibility (read by World/Scene/UI)
	inline bool ReduceMotion()        { return GetBool(TEXT("ReduceMotion"), false); }   // no camera pans/shake, gentler flicker
	inline bool ReduceFlashing()      { return GetBool(TEXT("ReduceFlashing"), false); } // no light flicker / strobe
	inline float UIScale()            { return GetFloat(TEXT("UIScale"), 1.f); }         // 0.8 .. 1.5
	inline int32 ColorVisionMode()    { return GetInt(TEXT("ColorVision"), 0); }         // 0 off, 1 protanope, 2 deuteranope, 3 tritanope
	inline bool HighContrastUI()      { return GetBool(TEXT("HighContrast"), false); }
	inline bool ShowCrewNames()       { return GetBool(TEXT("ShowCrewNames"), true); }
	inline bool ExpiryWarnings()      { return GetBool(TEXT("ExpiryWarnings"), true); }  // pulsing + sound when leads/recruits expire
	inline bool PauseOnFocusLoss()    { return GetBool(TEXT("PauseOnFocusLoss"), true); }
	inline int32 ColorVisionSeverity(){ return GetInt(TEXT("ColorVisionSeverity"), 8); } // 1 .. 10 correction strength
	inline float TextScale()          { return GetFloat(TEXT("TextScale"), 1.f); }       // 0.9 .. 1.3, UI text only (on top of UIScale)

	// Audio (volumes live in UDCAudioSubsystem; these are global multipliers applied through FApp by the UI)
	inline bool MuteAll()             { return GetBool(TEXT("MuteAll"), false); }
	inline bool MuteWhenUnfocused()   { return GetBool(TEXT("MuteWhenUnfocused"), true); }

	// Display
	inline bool ShowFPS()             { return GetBool(TEXT("ShowFPS"), false); }        // small fps readout on the HUD
	inline bool CelShading()          { return GetBool(TEXT("ToonOutlines"), true); }    // ink outlines + stepped lighting post pass

	// Developer
	inline bool DebugView()           { return GetBool(TEXT("DebugView"), false); }      // overlay: build grid, rooms, doorways, nav tile map, crew routes

	// Gameplay / camera (read by World: ADCCameraRig / ADCPlayerController)
	inline bool EdgeScroll()          { return GetBool(TEXT("EdgeScroll"), false); }     // pan when the mouse touches the screen edge
	inline float CameraPanSpeed()     { return GetFloat(TEXT("CameraPanSpeed"), 1.f); }  // 0.5 .. 2 multiplier on keyboard/edge pan
	inline float CameraZoomSpeed()    { return GetFloat(TEXT("CameraZoomSpeed"), 1.f); } // 0.5 .. 2 multiplier on scroll zoom
}

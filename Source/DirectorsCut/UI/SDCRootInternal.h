// Private helpers shared by the SDCRoot*.cpp files (Agent C).
#pragma once

#include "UI/SDCRoot.h"
#include "UI/DCUIStyle.h"
#include "Core/DCRunSubsystem.h"
#include "Core/DCUISubsystem.h"
#include "Core/DCAudioSubsystem.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Layout/SWrapBox.h"
#include "HAL/PlatformTime.h"

using namespace DCUI;

struct SDCRoot::FPlanCache
{
	FDCSuccessBreakdown B;
};

struct SDCRoot::FDailiesState
{
	FDCNightReport Report;
	int32 Index = 0;
	double T0 = 0.0;
	bool bSummary = false;
	bool bLive = false; // phase was Dailies when shown (Continue -> FinishDailies)
	// sfx/timeline flags for the current card
	bool bClap = false, bDice = false, bDice2 = false, bStamp = false, bMembers = false, bLuckySfx = false;
};

namespace DCUIInternal
{
	inline double Now() { return FPlatformTime::Seconds(); }
	inline float EaseOut(float X) { X = FMath::Clamp(X, 0.f, 1.f); return 1.f - FMath::Pow(1.f - X, 3.f); }
	inline FLinearColor OutcomeColor(EDCHeistOutcome O)
	{
		switch (O)
		{
		case EDCHeistOutcome::Clean: return DCUI::Green;
		case EDCHeistOutcome::Messy: return DCUI::Rgb(0xb8, 0x7a, 0x1c);
		case EDCHeistOutcome::Failed: return DCUI::Red;
		case EDCHeistOutcome::Busted: return DCUI::Rgb(0x7a, 0x0e, 0x0e);
		default: return DCUI::InkSoft;
		}
	}
	inline FString StatusText(const FDCCriminal& C)
	{
		switch (C.Status)
		{
		case EDCCriminalStatus::Available: return TEXT("Available");
		case EDCCriminalStatus::OnJob: return TEXT("Cast tonight");
		case EDCCriminalStatus::Injured: return FString::Printf(TEXT("Injured (%d night%s)"), C.InjuredNights, C.InjuredNights == 1 ? TEXT("") : TEXT("s"));
		case EDCCriminalStatus::Arrested: return TEXT("Arrested");
		default: return TEXT("");
		}
	}
	inline FLinearColor StatusColor(const FDCCriminal& C)
	{
		switch (C.Status)
		{
		case EDCCriminalStatus::Available: return DCUI::Green;
		case EDCCriminalStatus::OnJob: return DCUI::Rgb(0x9a, 0x6a, 0x10);
		default: return DCUI::Red;
		}
	}
}
using namespace DCUIInternal;

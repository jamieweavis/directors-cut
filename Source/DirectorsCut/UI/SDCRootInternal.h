// Private helpers shared by the SDCRoot / SDCPanel*.cpp files (Agent C).
#pragma once

#include "UI/SDCRoot.h"
#include "UI/SDCRing.h"
#include "UI/DCUIStyle.h"
#include "Core/DCRunSubsystem.h"
#include "Core/DCUISubsystem.h"
#include "Core/DCAudioSubsystem.h"
#include "Core/DCSettings.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SToolTip.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"

using namespace DCUI;

struct SDCRoot::FPlanCache
{
	FDCSuccessBreakdown B;
	bool bHas = false;
};

namespace DCUIInternal
{
	inline TSharedRef<SWidget> Gap(float W, float H = 0.f) { return SNew(SSpacer).Size(FVector2D(W, H)); }

	inline FString StatusTip(const FDCCriminal& C, double RunTime)
	{
		switch (C.Status)
		{
		case EDCCriminalStatus::OnMission: return TEXT("On a mission");
		case EDCCriminalStatus::Injured:   return FString::Printf(TEXT("Injured · back in %s"), *DC::Clock(C.InjuredUntil - RunTime));
		case EDCCriminalStatus::Arrested:  return TEXT("Arrested");
		default:                           return TEXT("Available");
		}
	}
	inline FName StatusIcon(const FDCCriminal& C)
	{
		switch (C.Status)
		{
		case EDCCriminalStatus::OnMission: return TEXT("car-front");
		case EDCCriminalStatus::Injured:   return TEXT("bandage");
		case EDCCriminalStatus::Arrested:  return TEXT("handcuffs");
		default:                           return TEXT("circle-check");
		}
	}
	inline FLinearColor StatusColor(const FDCCriminal& C)
	{
		switch (C.Status)
		{
		case EDCCriminalStatus::OnMission: return DCUI::Info;
		case EDCCriminalStatus::Injured:   return DCUI::Bad;
		case EDCCriminalStatus::Arrested:  return DCUI::Bad;
		default:                           return DCUI::Good;
		}
	}
	/** Level-vs-mission colouring. */
	inline FLinearColor LevelColor(float EffLevel, int32 MissionLevel)
	{
		const float D = EffLevel - (float)MissionLevel;
		if (D >= 0.f) { return DCUI::Good; }
		if (D > -1.01f) { return DCUI::Warn; }
		return DCUI::Bad;
	}
}
using namespace DCUIInternal;

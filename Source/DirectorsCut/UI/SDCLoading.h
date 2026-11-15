// Director's Cut — boot loading screen: covers the viewport until the world is ready to look at.
//
// The hideout and landscape are built synchronously in StartPlay, but their materials still compile
// (uncooked builds), PSOs precache, textures stream in and Lumen / shadows warm up over the next frames.
// This opaque overlay (styled like the title screen) sits on top of everything while the world keeps
// rendering underneath, and fades out once all of that has been quiet for a moment.
#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

class SDCLoading : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SDCLoading) {}
		/** Loading finished: the overlay starts fading out (the title intro should play now). */
		SLATE_EVENT(FSimpleDelegate, OnReveal)
		/** Fade-out done: the widget can be removed. */
		SLATE_EVENT(FSimpleDelegate, OnFinished)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

	/** True until the reveal starts: world input and hotkeys stay blocked. */
	bool IsBlocking() const { return RevealT0 < 0.0; }

	/** One frame's view of what is still pending (also logged). */
	struct FPending
	{
		int32 AsyncLoading = 0;   // packages still loading (0/1)
		int32 Shaders = 0;        // shader compile jobs (uncooked builds)
		int32 Pipelines = 0;      // PSO precache / precompile requests
		int32 Textures = 0;       // streaming resources still wanting mips
		int32 Total() const { return AsyncLoading + Shaders + Pipelines + Textures; }
	};
	static FPending Poll();

private:
	FSimpleDelegate OnReveal, OnFinished;

	double T0 = 0.0;              // shown at
	double QuietSince = -1.0;     // everything has been idle since (or -1)
	double TextureStallT0 = -1.0; // only textures still pending since (or -1)
	double RevealT0 = -1.0;       // fade-out start (or -1)
	bool bFinished = false;
	bool bHold = false;           // dev (-DCHoldLoading): never lift, for screenshots of the loading screen
	int32 Frames = 0;

	FPending Cur;
	FPending Peak;                // high-water marks, so the bar can show a fraction
	float Shown = 0.f;            // displayed progress (never goes back)
	FString Status;

	float Progress01() const;
	float Alpha() const;          // whole overlay
	float ContentAlpha() const;   // wordmark / bar (leave a little before the backdrop)
	void Reveal(const TCHAR* Why);
};

#include "DirectorsCut.h"
#include "Misc/App.h"
#include "Misc/CoreDelegates.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY(LogDC);

// Global mute switch: DefaultGame.ini [/Script/DirectorsCut.Audio] bMuteAll=True silences every sound in the app
// (FApp volume multiplier, which applies on top of all sound classes). -DCUnmute overrides it for a single run.
class FDirectorsCutModule : public FDefaultGameModuleImpl
{
	virtual void StartupModule() override
	{
		bool bMute = false;
		if (GConfig) { GConfig->GetBool(TEXT("/Script/DirectorsCut.Audio"), TEXT("bMuteAll"), bMute, GGameIni); }
		if (FParse::Param(FCommandLine::Get(), TEXT("DCUnmute"))) { bMute = false; }
		if (bMute)
		{
			auto Apply = []() { FApp::SetVolumeMultiplier(0.f); FApp::SetUnfocusedVolumeMultiplier(0.f); };
			Apply();
			FCoreDelegates::OnPostEngineInit.AddLambda(Apply); // the engine re-reads UnfocusedVolumeMultiplier during init
			UE_LOG(LogDC, Display, TEXT("Audio muted (DefaultGame.ini bMuteAll). Pass -DCUnmute to hear it."));
		}
	}
};

IMPLEMENT_PRIMARY_GAME_MODULE(FDirectorsCutModule, DirectorsCut, "DirectorsCut");

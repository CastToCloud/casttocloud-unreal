// Copyright Cast To Cloud 2024-2026. All Rights Reserved.

#include <Misc/ConfigContext.h>
#include <Misc/CoreDelegates.h>
#include <ProfilingDebugging/TraceAuxiliary.h>

#include "CtcSharedConfigurationSettings.h"
#include "CtcSharedHelpers.h"

namespace
{
	void AdjustTraceTailSizeBytes(int32 TailSizeBytes)
	{
		UE::Trace::FInitializeDesc const* InitDesc = FTraceAuxiliary::GetInitializeDesc();
		UE::Trace::FInitializeDesc* MutableDesc = const_cast<UE::Trace::FInitializeDesc*>(InitDesc);
		MutableDesc->TailSizeBytes = TailSizeBytes;
	}

	void StartTraceToMemory(const FString& Channels)
	{
		FTraceAuxiliary::FOptions Options;
		FTraceAuxiliary::Start(FTraceAuxiliary::EConnectionType::None, nullptr, *Channels, &Options, LogTemp);
	}
}

static FDelayedAutoRegisterHelper MonitoringEnginePreInit(EDelayedRegisterRunPhase::StartOfEnginePreInit,
	[]
	{
		auto IniConfig = CastToCloudSharedHelpers::GetPreInitConfig();
		if (IniConfig.HasError())
		{
			return;
		}

		FConfigFile Ini = IniConfig.GetValue();

		int32 TailSizeBytes = 0;
		const bool bHasTailSize = Ini.GetInt(TEXT("/Script/CastToCloudShared.CtcSharedSettings"), TEXT("GameTailSizeBytes"), TailSizeBytes) && TailSizeBytes > 0;

		FString TraceChannels = TEXT("");
		const bool bHasChannels = Ini.GetString(TEXT("/Script/CastToCloudShared.CtcSharedSettings"), TEXT("TraceChannels"), TraceChannels) && !TraceChannels.IsEmpty();

#if !WITH_EDITOR
		//NOTES:
		// 1. The Tail Size needs to be adjusted before FTraceAuxiliary::Initialize, so EDelayedRegisterRunPhase::StartOfEnginePreInit is the only delegate we can use
		// 2. After FTraceAuxiliary::Initialize, FTraceAuxiliary::TryAutoConnect will be tried, so we need to keep in mind Unreal Insights might connect here
		// 3. The next available delegate for us is FCoreDelegates::OnOutputDevicesInit (even if the name is not ideal), so we are going to use it to start our trace
		if (bHasTailSize)
		{
			AdjustTraceTailSizeBytes(TailSizeBytes);
		}
		if (bHasChannels)
		{
			FCoreDelegates::OnOutputDevicesInit.AddStatic(&StartTraceToMemory, TraceChannels);
		}
#else
		//NOTES: 
		// For editor builds the plugin dll is loaded too late, and we cannot modify the TailSize. So we just start the trace to memory
		if (bHasChannels)
		{
			StartTraceToMemory(TraceChannels);
		}
#endif
	});

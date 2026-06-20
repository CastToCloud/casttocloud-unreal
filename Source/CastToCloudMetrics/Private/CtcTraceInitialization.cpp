// Copyright Cast To Cloud 2024-2026. All Rights Reserved.

#include <Misc/CoreDelegates.h>
#include <ProfilingDebugging/TraceAuxiliary.h>

#include "CtcMetricsLog.h"

static FDelayedAutoRegisterHelper MonitoringEnginePreInit(
	EDelayedRegisterRunPhase::StartOfEnginePreInit,
	[]
	{
// NOTE: The Tail Size needs to be adjusted before FTraceAuxiliary::Initialize,
// so EDelayedRegisterRunPhase::StartOfEnginePreInit is the only delegate we can use (available only in packaged builds - the editor version runs too late)
#if WITH_EDITOR
		UE_LOG(LogCtcMetrics, Display, TEXT("Skipping TailSize modification (Editor build)"));
#elif !defined(CTC_EXPORTED_GAME_TAIL_SIZE_BYTES)
		UE_LOG(LogCtcMetrics, Display, TEXT("Skipping TailSize modification (missing define)"));
#else
		const int32 TailSizeBytes = CTC_EXPORTED_GAME_TAIL_SIZE_BYTES;
		UE_LOG(LogCtcMetrics, Display, TEXT("Executing TailSize modification (%d bytes)"), TailSizeBytes);

		UE::Trace::FInitializeDesc const* InitDesc = FTraceAuxiliary::GetInitializeDesc();
		if (!InitDesc)
		{
			UE_LOG(LogCtcMetrics, Error, TEXT("Failed to adjust trace tail size bytes."));
			return;
		}

		UE::Trace::FInitializeDesc* MutableDesc = const_cast<UE::Trace::FInitializeDesc*>(InitDesc);
		MutableDesc->TailSizeBytes = TailSizeBytes;
#endif
	}
);

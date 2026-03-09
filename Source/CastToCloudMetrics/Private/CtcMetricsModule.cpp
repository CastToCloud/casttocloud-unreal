// Copyright Cast To Cloud 2024-2026. All Rights Reserved.

#include "CtcMetricsModule.h"

#include <HAL/IConsoleManager.h>
#include <Misc/DateTime.h>
#include <Templates/UniquePtr.h>

#include "CtcMetricsBackendOutputDevice.h"
#include "CtcMetricsLog.h"

namespace
{
	TUniquePtr<FCtcMetricsBackendOutputDevice> GBackendOutputDevice;

	FAutoConsoleCommand FlushForwardedLogs(
		TEXT("CastToCloud.LogForwarding.Flush"),
		TEXT("Immediately attempts to flush pending log forwarding entries to the backend."),
		FConsoleCommandDelegate::CreateLambda(
			[]
			{
				if (!GBackendOutputDevice)
				{
					UE_LOG(LogCtcMetrics, Warning, TEXT("Log forwarding output device is not active."));
					return;
				}

				GBackendOutputDevice->Flush();
			}
		)
	);

	FAutoConsoleCommand EmitForwardedTestLog(
		TEXT("CastToCloud.LogForwarding.EmitTest"),
		TEXT("Emits a test log line that should be captured by the backend output device."),
		FConsoleCommandDelegate::CreateLambda(
			[]
			{
				UE_LOG(LogTemp, Display, TEXT("CastToCloud log forwarding test message at %s"), *FDateTime::UtcNow().ToIso8601());
			}
		)
	);
}

void FCtcMetricsModule::StartupModule()
{
	TUniquePtr<FCtcMetricsBackendOutputDevice> OutputDevice = MakeUnique<FCtcMetricsBackendOutputDevice>();
	if (OutputDevice->Setup())
	{
		GBackendOutputDevice = MoveTemp(OutputDevice);
	}
}

void FCtcMetricsModule::ShutdownModule()
{
	if (GBackendOutputDevice)
	{
		GBackendOutputDevice->TearDown();
		GBackendOutputDevice.Reset();
	}
}

IMPLEMENT_MODULE(FCtcMetricsModule, CastToCloudMetrics)

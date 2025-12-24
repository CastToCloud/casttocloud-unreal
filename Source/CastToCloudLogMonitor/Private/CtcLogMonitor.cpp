#include "CtcLogMonitor.h"

#include "CtcLogMonitorLog.h"
#include "CtcLogMonitoringSettings.h"
#include "CtcOutputDevice.h"

void FCtcLogMonitor::StartupModule()
{
	const UCtcLogMonitoringSettings* Settings = GetDefault<UCtcLogMonitoringSettings>();

	if (Settings && Settings->AllowedExecutables.IsCurrentConfigurationAllowed())
	{
		UE_LOG(LogCtcLogMonitoring, Display, TEXT("Output device will be created. Settings allow current configuration."))
		OutputDevice = MakeUnique<FCtcOutputDevice>();
	}
	else
	{
		UE_LOG(LogCtcLogMonitoring, Warning, TEXT("Output device will NOT be created. Settings don't allow current configuration."))
	}
}

void FCtcLogMonitor::ShutdownModule()
{
	OutputDevice.Reset();
}

IMPLEMENT_MODULE(FCtcLogMonitor, CastToCloudLogMonitor)
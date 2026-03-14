#include "CtcLogMonitor.h"

#include "CtcLogMonitorLog.h"
#include "CtcOutputDevice.h"

namespace
{
	// Keep this alive past module shutdown so it can capture late shutdown logs until GLog tears down.
	FCtcOutputDevice* GLeakedOutputDevice = nullptr;
}

void FCtcLogMonitor::StartupModule()
{
	//if (Settings && Settings->AllowedExecutables.IsCurrentConfigurationAllowed())
	{
		UE_LOG(LogCtcLogMonitoring, Display, TEXT("Output device will be created. Settings allow current configuration."))

		if (!GLeakedOutputDevice)
		{
			GLeakedOutputDevice = new FCtcOutputDevice();
		}
	}
	//else
	{
		//UE_LOG(LogCtcLogMonitoring, Warning, TEXT("Output device will NOT be created. Settings don't allow current configuration."))
	}
}

void FCtcLogMonitor::ShutdownModule()
{
	// Intentionally do not destroy GLeakedOutputDevice here.
	// It must remain registered until the global log redirector tears down to capture final shutdown logs.
}

IMPLEMENT_MODULE(FCtcLogMonitor, CastToCloudLogMonitor)

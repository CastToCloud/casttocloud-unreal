#include "CtcPerformanceMonitorUtils.h"

FString CtcPerformanceMonitorUtils::GetTracesFolder()
{
	return FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("CastToCloud") / TEXT("PerformanceMonitor"));
}

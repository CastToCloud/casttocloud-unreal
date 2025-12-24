#include "CtcPerformanceMonitorSubsystem.h"

#include "CtcPerformanceMonitorData.h"
#include "CtcPerformanceMonitorUtils.h"

void UCtcPerformanceMonitorSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// Find all .json files in the folder
	const FString TraceFolder = CtcPerformanceMonitorUtils::GetTracesFolder();
	TArray<FString> Files;
	IFileManager::Get().FindFiles(Files, *TraceFolder,TEXT("*.json"));

	for (const FString& File : Files)
	{
		const FString FilePath = TraceFolder / File;
		TOptional<FCtcPerformanceMonitorSessionData> SessionData = FCtcPerformanceMonitorSessionData::FromFile(FilePath);
		if (SessionData.IsSet())
		{
			//TODO: Upload the file to the backend and delete it only if the upload was successful
			SessionData->DeleteFiles();
		}
	}

	DataConsumer = MakeShared<FCtcPerformanceMonitorDataConsumer>();
	DataConsumer->Start();
}

void UCtcPerformanceMonitorSubsystem::Deinitialize()
{
	Super::Deinitialize();

	//TODO: Investigate if there any later stages we can use to upload the data
	DataConsumer->Stop();
	DataConsumer.Reset();
}

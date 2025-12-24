#include "CtcPerformanceMonitorData.h"

#include <JsonObjectConverter.h>

#include "CtcPerformanceMonitorSettings.h"
#include "CtcPerformanceMonitorUtils.h"
#include "GeneralProjectSettings.h"

extern ENGINE_API float GAverageFPS;

TOptional<FCtcPerformanceMonitorSessionData> FCtcPerformanceMonitorSessionData::FromFile(const FString& FilePath)
{
	FCtcPerformanceMonitorSessionData Result;

	FString DataString;
	const bool bLoadSuccess = FFileHelper::LoadFileToString(DataString, *FilePath);
	if (!bLoadSuccess)
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to load. File: %s"), *FilePath);
		return {};
	}

	const bool bConvertSuccess = FJsonObjectConverter::JsonObjectStringToUStruct(DataString, &Result, 0, 0);
	if (!bConvertSuccess)
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to convert JSON to struct. File: %s Data: %s"), *FilePath, *DataString);
		return {};
	}

	return Result;
}

FCtcPerformanceMonitorSessionData::FCtcPerformanceMonitorSessionData()
{
	ProjectVersion = GetDefault<UGeneralProjectSettings>()->ProjectVersion;
	Configuration = LexToString(FApp::GetBuildConfiguration());
	EngineMode = FGenericPlatformMisc::GetEngineMode();
	bStandalone = FApp::IsStandalone();
	bUnattended = FApp::IsUnattended();
}

FString FCtcPerformanceMonitorSessionData::GetDataFile() const
{
	return TraceFilePath + TEXT(".json");
}

void FCtcPerformanceMonitorSessionData::DumpToJsonFile()
{
	const FString DataFile = GetDataFile();
	FString DataString;
	FJsonObjectConverter::UStructToJsonObjectString(*this, DataString);

	FFileHelper::SaveStringToFile(DataString, *DataFile);
}

void FCtcPerformanceMonitorSessionData::DeleteFiles()
{
	const FString DataFile = GetDataFile();

	IFileManager::Get().Delete(*DataFile);
	IFileManager::Get().Delete(*TraceFilePath);
}

void FCtcPerformanceMonitorDataConsumer::Start()
{
	const UCtcPerformanceMonitorSettings* Settings = GetDefault<UCtcPerformanceMonitorSettings>();

	GEngine->AddPerformanceDataConsumer(AsShared());

	constexpr FTraceAuxiliary::EConnectionType TraceType = FTraceAuxiliary::EConnectionType::File;
	const FString TraceFolder = CtcPerformanceMonitorUtils::GetTracesFolder();
	const FString TraceFile = FPaths::CreateTempFilename(*TraceFolder, TEXT("Trace-"), TEXT(".utrace"));
	FTraceAuxiliary::Start(TraceType, *TraceFile, *Settings->TraceChannels);
	UE_LOG(LogTemp, Warning, TEXT("Trace file: %s"), *TraceFile);

	constexpr float TickInterval = 5.0f;
	FTickerDelegate TickDelegate = FTickerDelegate::CreateRaw(this, &FCtcPerformanceMonitorDataConsumer::OnTick);
	TickDelegateHandle = FTSTicker::GetCoreTicker().AddTicker(TickDelegate, TickInterval);

	SessionData.TraceFilePath = TraceFile;
	SessionData.DumpToJsonFile();
}

void FCtcPerformanceMonitorDataConsumer::Stop()
{
	SessionData.DumpToJsonFile();

	FTSTicker::GetCoreTicker().RemoveTicker(TickDelegateHandle);

	FTraceAuxiliary::Stop();

	GEngine->RemovePerformanceDataConsumer(AsShared());
}

void FCtcPerformanceMonitorDataConsumer::StartCharting()
{
	UE_LOG(LogTemp, Warning, TEXT("Start charting"));
}

void FCtcPerformanceMonitorDataConsumer::ProcessFrame(const FFrameData& FrameData)
{
	UE_LOG(LogTemp, Warning, TEXT("FPS: %s"), *FString::SanitizeFloat(1.0f / FrameData.DeltaSeconds));
	SessionData.AverageFrameRate = GAverageFPS;
}

void FCtcPerformanceMonitorDataConsumer::StopCharting()
{
	UE_LOG(LogTemp, Warning, TEXT("Stop charting"));
}

bool FCtcPerformanceMonitorDataConsumer::OnTick(float DeltaTime)
{
	SessionData.DumpToJsonFile();

	return false;
}

void FCtcPerformanceMonitorDataConsumer::OnHitchDetected(EFrameHitchType HitchType, float HitchDurationInSeconds)
{
	FString HitchTypeString;
	UE_LOG(LogTemp, Warning, TEXT("Hitch detected! Type: %s, Duration: %f"), *HitchTypeString, HitchDurationInSeconds);
}

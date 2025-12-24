#pragma once

#include <ChartCreation.h>

#include "CtcPerformanceMonitorData.generated.h"

USTRUCT()
struct FCtcPerformanceMonitorSessionData
{
	GENERATED_BODY()

	static TOptional<FCtcPerformanceMonitorSessionData> FromFile(const FString& FilePath);

	FCtcPerformanceMonitorSessionData();

	UPROPERTY()
	FString TraceFilePath;

	UPROPERTY()
	int NumHitches = 0;

	UPROPERTY()
	double AverageFrameRate = -1;

	UPROPERTY()
	FString ProjectVersion;

	UPROPERTY()
	FString Configuration;

	UPROPERTY()
	FString EngineMode;

	UPROPERTY()
	bool bStandalone;

	UPROPERTY()
	bool bUnattended;

	FString GetDataFile() const;

	void DumpToJsonFile();
	void DeleteFiles();
};

class FCtcPerformanceMonitorDataConsumer : public IPerformanceDataConsumer, public TSharedFromThis<FCtcPerformanceMonitorDataConsumer>
{
public:
	virtual ~FCtcPerformanceMonitorDataConsumer() override = default;

	void Start();
	void Stop();

private:
	virtual void StartCharting() override;
	virtual void ProcessFrame(const FFrameData& FrameData) override;
	virtual void StopCharting() override;

	bool OnTick(float DeltaTime);
	void OnHitchDetected(EFrameHitchType HitchType, float HitchDurationInSeconds);

	FCtcPerformanceMonitorSessionData SessionData;
	FTSTicker::FDelegateHandle TickDelegateHandle;
};



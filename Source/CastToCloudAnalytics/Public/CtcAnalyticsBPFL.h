// Copyright Cast To Cloud 2024-2026. All Rights Reserved.

#pragma once

#include <AnalyticsBlueprintLibrary.h>
#include <CoreMinimal.h>

#include "CtcAnalyticsBPFL.generated.h"

UCLASS(meta = (DisplayName = "CastToCloud Analytics Blueprint Function Library"))
class CASTTOCLOUDANALYTICS_API UCtcAnalyticsBPFL : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "CastToCloud|Analytics", meta = (AutoCreateRefTerm = "Attributes"))
	static void RecordEventWithCustomLocation(const FString& EventName, const FVector& Location, const TArray<FAnalyticsEventAttr>& Attributes);

	UFUNCTION(BlueprintCallable, Category = "CastToCloud|Analytics", meta = (AutoCreateRefTerm = "Attributes"))
	static void RecordEventWithNoLocation(const FString& EventName, const TArray<FAnalyticsEventAttr>& Attributes);

	static void RecordEventWithPossibleLocation(const FString& EventName, const TOptional<FVector>& Location, const TArray<FAnalyticsEventAttr>& Attributes);
};

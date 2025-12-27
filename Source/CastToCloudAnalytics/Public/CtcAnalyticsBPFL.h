// Copyright Cast To Cloud 2024-2026. All Rights Reserved.

#pragma once

#include <AnalyticsBlueprintLibrary.h>
#include <AnalyticsEventAttribute.h>

#include "CtcAnalyticsBPFL.generated.h"

UCLASS(meta = (DisplayName = "CastToCloud Analytics Blueprint Function Library"))
class CASTTOCLOUDANALYTICS_API UCtcAnalyticsBPFL : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "CastToCloud|Analytics", meta = (DisplayName = "Record Event at Location", AutoCreateRefTerm = "Attributes", AdvancedDisplay = "3"))
	static void RecordEventAtLocationBP(const FString& EventName, const FVector& Location, const FQuat& Rotation, const TArray<FAnalyticsEventAttr>& Attributes);

	static void RecordEventAtLocation(const FString& EventName, const FVector& Location, const FQuat& Rotation = FQuat::Identity, const TArray<FAnalyticsEventAttribute>& Attributes = TArray<FAnalyticsEventAttribute>());

	UFUNCTION(BlueprintCallable, Category = "CastToCloud|Analytics", meta = (DisplayName = "Record Event with Transform", AutoCreateRefTerm = "Attributes", AdvancedDisplay = "2"))
	static void RecordEventWithTransformBP(const FString& EventName, const FTransform& Transform, const TArray<FAnalyticsEventAttr>& Attributes);

	static void RecordEventWithTransform(const FString& EventName, const FTransform& Transform, const TArray<FAnalyticsEventAttribute>& Attributes = TArray<FAnalyticsEventAttribute>());

	static void RecordEventWithOptionalTransform(const FString& EventName, TOptional<FTransform> Transform, const TArray<FAnalyticsEventAttribute>& Attributes = TArray<FAnalyticsEventAttribute>());
};

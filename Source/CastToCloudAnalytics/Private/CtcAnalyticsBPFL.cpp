// Copyright Cast To Cloud 2024-2026. All Rights Reserved.

#include "CtcAnalyticsBPFL.h"

#include "CtcAnalyticsModule.h"
#include "CtcAnalyticsProvider.h"

// Stolen from AnalyticsBlueprintLibrary.cpp
namespace
{
	TArray<FAnalyticsEventAttribute> ConvertAttrs(const TArray<FAnalyticsEventAttr>& Attributes)
	{
		TArray<FAnalyticsEventAttribute> Converted;
		Converted.Reserve(Attributes.Num());
		for (const FAnalyticsEventAttr& Attr : Attributes)
		{
			Converted.Emplace(Attr.Name, Attr.Value);
		}
		return Converted;
	}
} // namespace

void UCtcAnalyticsBPFL::RecordEventAtLocationBP(const FString& EventName, const FVector& Location, const FQuat& Rotation, const TArray<FAnalyticsEventAttr>& Attributes)
{
	RecordEventAtLocation(EventName, Location, Rotation, ConvertAttrs(Attributes));
}

void UCtcAnalyticsBPFL::RecordEventAtLocation(const FString& EventName, const FVector& Location, const FQuat& Rotation, const TArray<FAnalyticsEventAttribute>& Attributes)
{
	if (TSharedPtr<FCtcAnalyticsProvider> CtcProvider = FCtcAnalyticsModule::Get().GetProvider())
	{
		FTransform EventTransform = FTransform(Rotation, Location);
		CtcProvider->RecordEventWithTransform(EventName, EventTransform, Attributes);
	}
}

void UCtcAnalyticsBPFL::RecordEventWithTransformBP(const FString& EventName, const FTransform& Transform, const TArray<FAnalyticsEventAttr>& Attributes)
{
	RecordEventWithTransform(EventName, Transform, ConvertAttrs(Attributes));
}

void UCtcAnalyticsBPFL::RecordEventWithTransform(const FString& EventName, const FTransform& Transform, const TArray<FAnalyticsEventAttribute>& Attributes)
{
	if (TSharedPtr<FCtcAnalyticsProvider> CtcProvider = FCtcAnalyticsModule::Get().GetProvider())
	{
		CtcProvider->RecordEventWithTransform(EventName, Transform, Attributes);
	}
}

void UCtcAnalyticsBPFL::RecordEventWithOptionalTransform(const FString& EventName, TOptional<FTransform> Transform, const TArray<FAnalyticsEventAttribute>& Attributes)
{
	if (TSharedPtr<FCtcAnalyticsProvider> CtcProvider = FCtcAnalyticsModule::Get().GetProvider())
	{
		if (Transform.IsSet())
		{
			CtcProvider->RecordEventWithTransform(EventName, *Transform, Attributes);
		}
		else
		{
			CtcProvider->RecordEvent(EventName, Attributes);
		}
	}
}

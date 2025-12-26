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

void UCtcAnalyticsBPFL::RecordEventWithCustomLocation(const FString& EventName, const FVector& Location, const TArray<FAnalyticsEventAttr>& Attributes)
{
	RecordEventWithPossibleLocation(EventName, Location, Attributes);
}

void UCtcAnalyticsBPFL::RecordEventWithPossibleLocation(const FString& EventName, const TOptional<FVector>& Location, const TArray<FAnalyticsEventAttr>& Attributes)
{
	if (TSharedPtr<FCtcAnalyticsProvider> CtcProvider = FCtcAnalyticsModule::Get().GetProvider())
	{
		if (Location.IsSet())
		{
			CtcProvider->RecordEventWithCustomLocation(EventName, *Location, ConvertAttrs(Attributes));
		}
		else
		{
			CtcProvider->RecordEvent(EventName, ConvertAttrs(Attributes));
		}
	}
}

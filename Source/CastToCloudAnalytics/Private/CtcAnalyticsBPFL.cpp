// Copyright Cast To Cloud 2024-2026. All Rights Reserved.

#include "CtcAnalyticsBPFL.h"

#include <Analytics.h>

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
	TSharedPtr<IAnalyticsProvider> Provider = FAnalytics::Get().GetDefaultConfiguredProvider();
	if (TSharedPtr<FCtcAnalyticsProvider> CtcProvider = StaticCastSharedPtr<FCtcAnalyticsProvider>(Provider))
	{
		CtcProvider->RecordEventWithCustomLocation(EventName, Location, ConvertAttrs(Attributes));
	}
}

void UCtcAnalyticsBPFL::RecordEventWithNoLocation(const FString& EventName, const TArray<FAnalyticsEventAttr>& Attributes)
{
	TSharedPtr<IAnalyticsProvider> Provider = FAnalytics::Get().GetDefaultConfiguredProvider();
	if (TSharedPtr<FCtcAnalyticsProvider> CtcProvider = StaticCastSharedPtr<FCtcAnalyticsProvider>(Provider))
	{
		CtcProvider->RecordEventWithNoLocation(EventName, ConvertAttrs(Attributes));
	}
}

void UCtcAnalyticsBPFL::RecordEventWithPossibleLocation(const FString& EventName, const TOptional<FVector>& Location, const TArray<FAnalyticsEventAttr>& Attributes)
{
	if (Location.IsSet())
	{
		RecordEventWithCustomLocation(EventName, *Location, Attributes);
	}
	else
	{
		RecordEventWithNoLocation(EventName, Attributes);
	}
}

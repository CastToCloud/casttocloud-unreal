// Copyright Cast To Cloud 2024-2026. All Rights Reserved.

#pragma once

#include "CtcAnalyticsEditorData.generated.h"

UENUM()
enum class ECtcAnalyticsEditorSource : uint8
{
	FromApi,
	FromFile,
};

USTRUCT()
struct FCtcAnalyticsEditorHeatmapPoint
{
	GENERATED_BODY()

	UPROPERTY()
	FVector Position;

	UPROPERTY()
	int Count;
};

USTRUCT()
struct FCtcAnalyticsEditorHeatmapPoints
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FCtcAnalyticsEditorHeatmapPoint> Points;
};
